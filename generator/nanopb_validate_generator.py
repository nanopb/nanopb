#!/usr/bin/env python3
# kate: replace-tabs on; indent-width 4;

"""
nanopb_validate_generator.py - project-specific validation and packet-filter codegen
===================================================================================

This is a *separate* protoc plugin that layers our project-specific code on top
of stock nanopb output.  It exists so that `nanopb_generator.py` can stay
upstream nanopb: none of the logic here is patched into the upstream generator
any more.

Responsibilities
----------------
1. Emit ``<base>_validate.h`` / ``<base>_validate.c``.  The actual validator
   code is produced by :mod:`nanopb_validator`; this module only drives it.
2. Inject our packet-filter code (``filter_udp`` / ``filter_tcp``) into the
   ``.pb.h`` and ``.pb.c`` that nanopb already generated, using protoc
   *insertion points* rather than by modifying the generator.

How the injection works
-----------------------
nanopb emits ``/* @@protoc_insertion_point(...) */`` markers when it is run
with ``--protoc-insertion-points``.  A protoc plugin can then return a
``CodeGeneratorResponse.File`` whose ``name`` is an already-generated file and
whose ``insertion_point`` names one of those markers; protoc splices the
``content`` in at that spot.

Only three markers exist in nanopb, and ``struct:<Msg>`` sits *inside* the
struct body, so it is unusable for us.  That leaves:

    ===============  ===============  =========================================
    Target           Insertion point  What we inject
    ===============  ===============  =========================================
    ``<base>.pb.h``  ``eof``          opcode enum alias + filter declarations
    ``<base>.pb.c``  ``includes``     pb_encode.h / pb_decode.h / _validate.h
    ``<base>.pb.c``  ``eof``          validate_message() + filter bodies
    ===============  ===============  =========================================

Ordering requirement
--------------------
protoc runs ``--*_out`` generators in command-line order, and a plugin may only
insert into a file produced *earlier in the same invocation*.  So nanopb must
run first, and with insertion points enabled::

    protoc \\
      --nanopb_out=--protoc-insertion-points,-x,validate.proto:. \\
      --nanopb-validate_out=--root-message=pkg.Packet,-x,validate.proto:. \\
      myfile.proto

Options that affect C naming (``-C``, ``--custom-style``, ``-s``, ``-f``,
``-I``, ``-x``) must be given to *both* plugins, because this plugin rebuilds
nanopb's intermediate representation via :func:`nanopb_generator.parse_file` in
order to see exactly the same mangled type names that nanopb emitted.

Scope of validation
-------------------
Validation covers statically allocated fields, including ``POINTER``
allocation.  ``pb_callback_t`` fields are deliberately *not* validated and no
decode-callback plumbing is generated for them.
"""

from __future__ import unicode_literals

import os
import shlex
import sys

# The heavy lifting - descriptor parsing, option handling, naming styles - is
# all reused from nanopb_generator.  Importing it also builds nanopb_pb2 and
# validate_pb2 as a side effect, which is what makes nanopb_validator usable.
if not __package__:
    import nanopb_generator as nanopb
else:
    from . import nanopb_generator as nanopb

Globals = nanopb.Globals
OneOf = nanopb.OneOf
plugin_pb2 = nanopb.plugin_pb2
descriptor = nanopb.descriptor

# validate.proto and the validator itself are ours, not nanopb's, so we import
# them here rather than leaning on nanopb_generator to have done it.  Importing
# nanopb_generator above already built the generated _pb2 modules.
try:
    from proto import validate_pb2  # Generated from validate.proto
except ImportError:
    try:
        import validate_pb2  # fallback if PYTHONPATH already contains it
    except ImportError:
        validate_pb2 = None

try:
    from proto import nanopb_validator
except ImportError:
    try:
        import nanopb_validator
    except ImportError:
        nanopb_validator = None


class GeneratorError(Exception):
    """Raised for user-facing errors that should surface as a protoc failure."""


# ---------------------------------------------------------------------------
#                        Validation rule IR enrichment
# ---------------------------------------------------------------------------


def attach_validate_rules(f):
    """Attach `validate_rules` to every field of every message in a ProtoFile.

    nanopb's IR knows nothing about validate.proto, so after
    :func:`nanopb_generator.parse_file` has built the IR we walk it once and
    hang the parsed ``(validate.rules)`` extension off each Field.  That is the
    only thing :mod:`nanopb_validator` needs from us.

    Fields are matched to their descriptors by field *number* rather than by
    position, because nanopb folds oneof members into a single OneOf entry and
    reorders `msg.fields` relative to `msg.desc.field`.
    """
    for msg in f.messages:
        rules_by_number = {}
        if validate_pb2 is not None:
            for fdesc in getattr(msg.desc, 'field', []):
                try:
                    if fdesc.options.HasExtension(validate_pb2.rules):
                        rules_by_number[fdesc.number] = fdesc.options.Extensions[validate_pb2.rules]
                except (KeyError, AttributeError):
                    # Extension not available or not properly registered
                    pass

        for field in msg.fields:
            # OneOf is a container; the rules belong on its members.
            members = field.fields if isinstance(field, OneOf) else [field]
            for member in members:
                member.validate_rules = rules_by_number.get(
                    getattr(member, 'tag', None), None)


# ---------------------------------------------------------------------------
#                        Envelope / root-message discovery
# ---------------------------------------------------------------------------
#
# The filters need to know which message to decode a packet as.  There are
# three supported ways to determine that, checked in this order:
#
#   1. --root-message=NAME    explicit; decode every packet as that message
#   2. --envelope-mode=any    a message carrying a google.protobuf.Any payload
#   3. --envelope-mode=oneof  a message with an opcode enum + oneof payload,
#                             or just a oneof payload
#
# All three work purely off the nanopb IR, so the names they produce match the
# names nanopb emitted into the .pb.h.


def find_message_by_name(f, message_name):
    """Find a message by its fully qualified name or simple name.

    Args:
        f: The nanopb ProtoFile.
        message_name: Name like "mypkg.Packet", "mypkg.sub.Packet", or "Packet".

    Returns:
        The Message object if found, None otherwise.
    """
    if not message_name:
        return None

    # Normalize the message name: remove leading dots
    normalized_name = message_name.lstrip('.')

    # Build package prefix
    pkg_prefix = f.fdesc.package + '.' if f.fdesc.package else ''

    for msg in f.messages:
        # msg.name is like "chat_ClientMessage" or "mypackage_sub_Packet"
        # We need to match against various name forms
        msg_name_str = str(msg.name)

        # Extract the simple message name (last part after underscore)
        msg_name_parts = msg_name_str.split('_')
        simple_name = msg_name_parts[-1] if len(msg_name_parts) > 1 else msg_name_str

        # Try to reconstruct the fully qualified name.
        # If package is "mypkg" and msg.name is "mypkg_sub_Packet", then the
        # fully qualified name is "mypkg.sub.Packet".
        if len(msg_name_parts) > 1 and f.fdesc.package:
            pkg_parts = f.fdesc.package.split('.')
            # Check that msg_name_parts has enough elements to match pkg_parts
            if len(msg_name_parts) >= len(pkg_parts) and msg_name_parts[:len(pkg_parts)] == pkg_parts:
                remaining_parts = msg_name_parts[len(pkg_parts):]
            else:
                remaining_parts = msg_name_parts
            full_qualified_name = pkg_prefix + '.'.join(remaining_parts)
        else:
            full_qualified_name = pkg_prefix + simple_name

        # Match against:
        # 1. Fully qualified name: "mypkg.Packet"
        # 2. Simple name: "Packet"
        # 3. Partial qualified name: "sub.Packet" (for nested messages)
        if normalized_name == full_qualified_name:
            return msg
        if normalized_name == simple_name:
            return msg
        if full_qualified_name.endswith('.' + normalized_name):
            return msg
        # Also try matching against the raw msg.name (underscore-separated)
        if normalized_name.replace('.', '_') == msg_name_str:
            return msg

    return None


def detect_envelope_pattern(f, envelope_name=None):
    """Detect an Envelope message with an enum + oneof payload, or just a oneof.

    Returns (envelope_msg, opcode_field, opcode_enum, oneof_field,
    opcode_to_msg_map) or None.  For oneof-only patterns, opcode_field,
    opcode_enum and opcode_to_msg_map are None.

    Args:
        f: The nanopb ProtoFile.
        envelope_name: Optional envelope message name.  If given, only that
            message is considered.
    """
    messages_to_check = f.messages

    # If envelope_name is specified, filter to just that message
    if envelope_name:
        messages_to_check = [msg for msg in f.messages
                             if str(msg.name).split('_')[-1].lower() == envelope_name.lower()]

    for msg in messages_to_check:
        # Look for a message with both an enum field and a oneof
        enum_field = None
        oneof_field = None

        for field in msg.fields:
            # Check if this is an enum field (potential opcode) - ENUM or UENUM
            if hasattr(field, 'pbtype') and field.pbtype in ('ENUM', 'UENUM'):
                enum_field = field
            # Check if this is a oneof
            elif isinstance(field, OneOf):
                oneof_field = field

        # If we found both an enum and a oneof, this is likely an envelope with opcode
        if enum_field and oneof_field:
            # Try to find the enum definition
            opcode_enum = None
            for enum in f.enums:
                if str(enum.names) == str(enum_field.ctype):
                    opcode_enum = enum
                    break

            if not opcode_enum:
                continue

            # Build mapping from enum values to message types in the oneof.
            # This requires matching enum value names to oneof field names.
            opcode_to_msg_map = {}

            for enum_name, enum_value in opcode_enum.values:
                # Get the last part of the enum name (e.g. "OP_LOGIN" -> "LOGIN")
                enum_suffix = str(enum_name).split('_')[-1].lower()

                # Try to match with oneof field names
                for oneof_subfield in oneof_field.fields:
                    field_name_lower = oneof_subfield.name.lower()
                    if enum_suffix == field_name_lower or enum_suffix in field_name_lower:
                        opcode_to_msg_map[enum_value] = oneof_subfield
                        break

            # If we have at least one mapping, consider this a valid envelope pattern
            if opcode_to_msg_map:
                return (msg, enum_field, opcode_enum, oneof_field, opcode_to_msg_map)

        # If we found just a oneof (no enum), this is a simpler envelope pattern
        elif oneof_field:
            return (msg, None, None, oneof_field, None)

    return None


def detect_any_envelope_pattern(f, envelope_name=None):
    """Detect an Envelope message carrying a google.protobuf.Any payload.

    Returns (envelope_msg, any_field, all_msg_types) or None.

    Args:
        f: The nanopb ProtoFile.
        envelope_name: Optional envelope message name.  If given, only that
            message is considered.
    """
    messages_to_check = f.messages

    # If envelope_name is specified, filter to just that message
    if envelope_name:
        messages_to_check = [msg for msg in f.messages
                             if str(msg.name).split('_')[-1].lower() == envelope_name.lower()]

    for msg in messages_to_check:
        # Look for a message with a google.protobuf.Any field.
        # The ctype for Any fields will be 'google_protobuf_Any' or similar.
        any_field = None

        for field in msg.fields:
            if hasattr(field, 'ctype'):
                ctype_str = str(field.ctype).lower()
                if 'any' in ctype_str and 'google' in ctype_str:
                    any_field = field
                    break

        if any_field:
            # Collect all message types that could be payloads
            # (excluding the envelope itself)
            all_msg_types = [other for other in f.messages if other != msg]
            return (msg, any_field, all_msg_types)

    return None


def resolve_filter_target(f, options):
    """Work out what the filters should decode, based on the command line.

    Returns a (root_message, any_envelope_info, envelope_info) triple in which
    at most one entry is non-None, or (None, None, None) when this file has
    nothing filterable.

    Raises:
        GeneratorError: if --root-message names a message that does not exist.
    """
    root_message_name = getattr(options, 'root_message', None)
    envelope_mode = getattr(options, 'envelope_mode', 'oneof')
    envelope_name = getattr(options, 'envelope_name', None)

    if root_message_name:
        root_message = find_message_by_name(f, root_message_name)
        if not root_message:
            available = '\n'.join('  - %s' % str(msg.name) for msg in f.messages)
            raise GeneratorError(
                "--root-message '%s' does not match any message in the loaded "
                "descriptors.\nAvailable messages:\n%s" % (root_message_name, available))
        return (root_message, None, None)

    if envelope_mode == 'any':
        return (None, detect_any_envelope_pattern(f, envelope_name), None)

    return (None, None, detect_envelope_pattern(f, envelope_name))


# ---------------------------------------------------------------------------
#                     .pb.h injection (insertion point: eof)
# ---------------------------------------------------------------------------


def opcode_alias_type(envelope_msg):
    """Name of the ALL-CAPS opcode enum alias emitted for an envelope message."""
    return (str(envelope_msg.name) + '_OPCODE').replace('.', '_').replace('-', '_').upper()


def generate_filter_declarations(f, options):
    """Declarations for the UDP and TCP packet filters, plus an optional
    Envelope opcode enum alias.

    Injected into <base>.pb.h at the `eof` insertion point, which sits inside
    the include guard and after every struct definition.
    """
    # If an Envelope pattern is detected, generate a CAPS enum alias that maps
    # to the original opcode enum.
    envelope_info = detect_envelope_pattern(f, getattr(options, 'envelope_name', None))
    if envelope_info:
        envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info

        # Only generate the enum alias if we have an opcode enum
        # (not for oneof-only patterns)
        if opcode_enum:
            # Type name in ALL CAPS: <ENVELOPE_NAME>_OPCODE
            alias_type = opcode_alias_type(envelope_msg)
            yield 'typedef enum %s {\n' % alias_type

            # Map numeric values back to enumerator names and emit alias entries
            # Format: <ALIAS_TYPE>_<ENUM_ENTRY> = <ENUM_ENTRY>,
            for (enumname, enumvalue) in opcode_enum.values:
                original_entry = Globals.naming_style.enum_entry(enumname)
                alias_entry = original_entry.replace('.', '_').replace('-', '_').upper()
                yield '    %s_%s = %s,\n' % (alias_type, alias_entry, original_entry)

            yield '} %s;\n\n' % alias_type

    # Doxygen for filter functions
    yield "/**\n"
    yield " * @brief Decode and validate a UDP packet.\n"
    yield " *\n"
    yield " * Decodes an incoming datagram and validates the contained message(s).\n"
    yield " *\n"
    yield " * @param ctx           Optional user context (implementation-defined).\n"
    yield " * @param packet        Pointer to packet buffer.\n"
    yield " * @param packet_size   Length of packet buffer in bytes.\n"
    yield " * @return 0 on success, -1 on failure.\n"
    yield " */\n"
    yield 'int filter_udp(void *ctx, uint8_t *packet, size_t packet_size);\n\n'

    yield "/**\n"
    yield " * @brief Decode and validate a TCP packet.\n"
    yield " *\n"
    yield " * Decodes an incoming stream message and validates the contained message(s).\n"
    yield " *\n"
    yield " * @param ctx           Optional user context (implementation-defined).\n"
    yield " * @param packet        Pointer to packet buffer.\n"
    yield " * @param packet_size   Length of packet buffer in bytes.\n"
    yield " * @param is_to_server  True if packet direction is client -> server.\n"
    yield " * @return 0 on success, -1 on failure.\n"
    yield " */\n"
    yield 'int filter_tcp(void *ctx, uint8_t *packet, size_t packet_size, bool is_to_server);\n'
    yield '\n'


def generate_header_injection(f, options):
    """Full <base>.pb.h `eof` payload, wrapped for C++ compatibility."""
    yield '\n'
    yield '#ifdef __cplusplus\n'
    yield 'extern "C" {\n'
    yield '#endif\n\n'

    for line in generate_filter_declarations(f, options):
        yield line

    yield '\n#ifdef __cplusplus\n'
    yield '} /* extern "C" */\n'
    yield '#endif\n'


# ---------------------------------------------------------------------------
#                 .pb.c injection (insertion points: includes, eof)
# ---------------------------------------------------------------------------

# Return codes used by both filters.
RET_OK = '0'
RET_ERR = '-1'


def generate_source_includes(f, options):
    """Includes the filter code needs, injected at the `includes` point.

    These used to be emitted at the very end of the .pb.c.  Putting them at the
    `includes` marker is both idiomatic and keeps the file compilable if
    anything is ever injected between them and the filter bodies.
    """
    # options.libformat carries no trailing newline (it defaults to
    # '#include <%s>'), so each include has to be terminated explicitly.
    for header in ('pb_encode.h', 'pb_decode.h'):
        try:
            yield options.libformat % (header)
        except TypeError:
            # no %s specified - use whatever was passed in as options.libformat
            yield options.libformat
        yield '\n'

    # The generated validators live in a sibling header.
    basename = f.fdesc.name.rsplit('.', 1)[0]
    yield '#include "%s_validate.h"\n' % basename


def generate_validate_message_helper(f):
    """Emit the static validate_message() dispatcher.

    Maps a nanopb message descriptor to the matching pb_validate_*() function,
    so the filter bodies can validate a decoded message generically.
    """
    yield 'static int validate_message(const pb_msgdesc_t *fields, const void *msg_struct) {\n'
    yield '    pb_violations_t violations = {0};\n'
    for msg in f.messages:
        msg_type_name = Globals.naming_style.type_name(msg.name)
        validate_func_name = 'pb_validate_' + msg_type_name
        yield '    if (fields == &%s_msg) {\n' % msg_type_name
        yield '        return %s((const %s *)msg_struct, &violations) ? 1 : 0;\n' % (
            validate_func_name, msg_type_name)
        yield '    }\n'
    yield '    return 1; /* Default: message is valid */\n'
    yield '}\n\n'


def _generate_root_message_body(f, root_message, is_tcp):
    """Filter body for --root-message mode: decode and validate one fixed type."""
    msg_type = Globals.naming_style.type_name(root_message.name)
    init_zero = Globals.naming_style.define_name(str(root_message.name) + '_init_zero')

    if is_tcp:
        yield '    (void)is_to_server; /* Direction unused in single-root-message mode */\n'
    yield '    /* Single-root-message mode: decode as %s */\n' % msg_type
    yield '    %s msg = %s;\n' % (msg_type, init_zero)
    yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
    yield '    status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
    yield '    \n'
    yield '    if (!status) {\n'
    yield '        return %s;\n' % RET_ERR
    yield '    }\n'
    yield '    \n'
    yield '    /* Validate the root message */\n'
    yield '    if (validate_message(&%s_msg, &msg)) {\n' % msg_type
    yield '        return %s;\n' % RET_OK
    yield '    }\n'
    yield '    \n'
    yield '    return %s;\n' % RET_ERR


def _generate_any_envelope_body(f, any_envelope_info):
    """Filter body for an envelope carrying a google.protobuf.Any payload.

    The payload type is identified by its type_url.  Rather than strcmp-ing
    against every candidate, we switch on a cheap rolling hash of the type_url
    and only strcmp inside the matching case.
    """
    envelope_msg, any_field, all_msg_types = any_envelope_info
    envelope_type = Globals.naming_style.type_name(envelope_msg.name)
    any_field_name = Globals.naming_style.var_name(any_field.name)

    yield '    %s envelope = %s;\n' % (
        envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
    yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
    yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
    yield '    \n'
    yield '    if (!status) {\n'
    yield '        return %s;\n' % RET_ERR
    yield '    }\n'
    yield '    \n'
    yield '    /* Validate the envelope message first (checks any.in/any.not_in rules) */\n'
    yield '    if (!validate_message(&%s_msg, &envelope)) {\n' % envelope_type
    yield '        return %s;\n' % RET_ERR
    yield '    }\n'
    yield '    \n'
    yield '    /* Extract type_url from Any field */\n'
    yield '    const char *type_url = (const char *)envelope.%s.type_url;\n' % any_field_name
    yield '    if (!type_url) {\n'
    yield '        return %s;\n' % RET_ERR
    yield '    }\n'
    yield '    \n'
    yield '    /* Compute hash of type_url for efficient switching */\n'
    yield '    uint32_t type_hash = 0;\n'
    yield '    for (const char *p = type_url; *p; p++) {\n'
    yield '        type_hash = type_hash * 31 + (uint8_t)*p;\n'
    yield '    }\n'
    yield '    \n'
    yield '    /* Switch on type_url hash to determine payload type */\n'
    yield '    switch (type_hash) {\n'

    # Generate switch cases using hash values
    for msg in all_msg_types:
        msg_type = Globals.naming_style.type_name(msg.name)
        # Extract the simple message name for type URL
        msg_simple_name = str(msg.name).split('_')[-1]

        # Build the expected type_url
        # (typically "type.googleapis.com/package.MessageName")
        if f.fdesc.package:
            expected_type_url = 'type.googleapis.com/%s.%s' % (f.fdesc.package, msg_simple_name)
        else:
            expected_type_url = 'type.googleapis.com/%s' % msg_simple_name

        # Calculate hash for the case label - must match the C loop emitted above
        type_hash = 0
        for c in expected_type_url:
            type_hash = (type_hash * 31 + ord(c)) & 0xFFFFFFFF

        yield '        case 0x%08XU: /* %s */\n' % (type_hash, expected_type_url)
        yield '            if (strcmp(type_url, "%s") == 0) {\n' % expected_type_url
        yield '                %s payload_msg = %s;\n' % (
            msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
        yield '                pb_istream_t payload_stream = pb_istream_from_buffer(envelope.%s.value.bytes, envelope.%s.value.size);\n' % (
            any_field_name, any_field_name)
        yield '                if (pb_decode(&payload_stream, &%s_msg, &payload_msg)) {\n' % msg_type
        yield '                    if (validate_message(&%s_msg, &payload_msg)) {\n' % msg_type
        yield '                        return %s;\n' % RET_OK
        yield '                    }\n'
        yield '                }\n'
        yield '            }\n'
        yield '            break;\n'

    yield '        default:\n'
    yield '            break;\n'
    yield '    }\n'
    yield '    \n'
    yield '    return %s;\n' % RET_ERR


def _generate_oneof_envelope_body(f, envelope_info):
    """Filter body for an envelope with a oneof payload.

    Two shapes are supported: an explicit opcode enum paired with the oneof
    (switch on the opcode, then confirm the oneof tag agrees), or a bare oneof
    (switch directly on the which_ tag).
    """
    envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info
    envelope_type = Globals.naming_style.type_name(envelope_msg.name)
    oneof_name = Globals.naming_style.var_name(oneof_field.name)

    yield '    %s envelope = %s;\n' % (
        envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
    yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
    yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
    yield '    \n'
    yield '    if (!status) {\n'
    yield '        return %s;\n' % RET_ERR
    yield '    }\n'
    yield '    \n'

    def validate_payload(oneof_subfield, indent):
        """Emit the validate_message() call opening one oneof arm.

        MESSAGE arms validate the nested submessage.  Scalar arms have no
        descriptor of their own, so the whole envelope is validated instead.
        """
        if oneof_subfield.pbtype == 'MESSAGE':
            submsg_type = Globals.naming_style.type_name(oneof_subfield.ctype)
            oneof_member_name = Globals.naming_style.var_name(oneof_subfield.name)
            yield '%sif (validate_message(&%s_msg, &envelope.%s.%s)) {\n' % (
                indent, submsg_type, oneof_name, oneof_member_name)
        else:
            yield '%sif (validate_message(&%s_msg, &envelope)) {\n' % (indent, envelope_type)

    if opcode_field and opcode_enum and opcode_to_msg_map:
        # Opcode + oneof pattern
        opcode_field_name = Globals.naming_style.var_name(opcode_field.name)
        # CAPS alias type produced in the header
        alias_type = opcode_alias_type(envelope_msg)
        # Map numeric opcode values to original enumerator names
        val_to_name = {
            v: Globals.naming_style.enum_entry(n).replace('.', '_').replace('-', '_').upper()
            for (n, v) in opcode_enum.values}

        yield '    switch (envelope.%s) {\n' % opcode_field_name

        for opcode_val, oneof_subfield in sorted(opcode_to_msg_map.items(), key=lambda x: x[0]):
            enum_suffix = val_to_name.get(opcode_val, None)
            if enum_suffix is not None:
                yield '        case %s_%s:\n' % (alias_type, enum_suffix)
            else:
                # Fallback to numeric value if mapping fails
                yield '        case %d:\n' % (opcode_val)
            # Use the field tag constant for the which_field comparison
            tag_constant = Globals.naming_style.define_name(
                '%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))
            yield '            if (envelope.which_%s == %s) {\n' % (oneof_name, tag_constant)
            for line in validate_payload(oneof_subfield, '                '):
                yield line
            yield '                    return %s;\n' % RET_OK
            yield '                }\n'
            yield '            }\n'
            yield '            break;\n'

        yield '        default:\n'
        yield '            return %s;\n' % RET_ERR
        yield '    }\n'
        yield '    \n'
        yield '    return %s;\n' % RET_ERR
    else:
        # Oneof-only pattern - switch on which_field
        yield '    switch (envelope.which_%s) {\n' % oneof_name

        for oneof_subfield in oneof_field.fields:
            tag_constant = Globals.naming_style.define_name(
                '%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))

            yield '        case %s:\n' % tag_constant
            for line in validate_payload(oneof_subfield, '            '):
                yield line
            yield '                return %s;\n' % RET_OK
            yield '            }\n'
            yield '            break;\n'

        yield '        default:\n'
        yield '            break;\n'
        yield '    }\n'
        yield '    \n'
        yield '    return %s;\n' % RET_ERR


def generate_filter_function(f, signature, is_tcp, root_message,
                             any_envelope_info, envelope_info):
    """Emit one complete filter function.

    filter_udp and filter_tcp share their entire body apart from the unused
    `is_to_server` parameter, so both are produced from this one generator.
    """
    yield 'int %s {\n' % signature
    yield '    pb_istream_t stream;\n'
    yield '    bool status;\n'
    yield '    (void)ctx; /* Context may be unused */\n\n'

    if root_message:
        body = _generate_root_message_body(f, root_message, is_tcp)
    elif any_envelope_info:
        body = _generate_any_envelope_body(f, any_envelope_info)
    elif envelope_info:
        body = _generate_oneof_envelope_body(f, envelope_info)
    else:
        # Nothing to decode against - reject everything.
        body = iter(['    return %s;\n' % RET_ERR])

    for line in body:
        yield line

    yield '}\n'


def generate_source_injection(f, options):
    """Full <base>.pb.c `eof` payload: the dispatcher plus both filters."""
    root_message, any_envelope_info, envelope_info = resolve_filter_target(f, options)

    for line in generate_validate_message_helper(f):
        yield line

    for line in generate_filter_function(
            f, 'filter_udp(void *ctx, uint8_t *packet, size_t packet_size)',
            False, root_message, any_envelope_info, envelope_info):
        yield line
    yield '\n'

    for line in generate_filter_function(
            f, 'filter_tcp(void *ctx, uint8_t *packet, size_t packet_size, bool is_to_server)',
            True, root_message, any_envelope_info, envelope_info):
        yield line


# ---------------------------------------------------------------------------
#                              Validator driver
# ---------------------------------------------------------------------------


def build_validator_generator(f):
    """Create and populate a ValidatorGenerator for one ProtoFile."""
    validator_gen = nanopb_validator.ValidatorGenerator(f)

    # Add validators for all messages that carry rules.  Message-level rules
    # are not supported: every rule lives on a field.
    for msg in f.messages:
        if hasattr(msg, 'fields'):
            validator_gen.add_message_validator(msg)

    # Always emit validation functions, even when no message declares a rule,
    # so that validate_message() has something to dispatch to for every
    # descriptor it knows about.
    if not validator_gen.validators:
        for msg in f.messages:
            if hasattr(msg, 'fields'):
                validator_gen.force_add_message_validator(msg)

    return validator_gen


# ---------------------------------------------------------------------------
#                            Command line handling
# ---------------------------------------------------------------------------

# Options owned by this plugin.  Everything else in the argument list is handed
# straight to nanopb's own parser, so that -I/-x/-s/-C/--custom-style behave
# identically here and in nanopb_generator.
OWN_OPTIONS = {
    '--root-message': 'root_message',
    '--envelope-mode': 'envelope_mode',
    '--envelope-name': 'envelope_name',
}

OWN_DEFAULTS = {
    'root_message': None,
    'envelope_mode': 'oneof',
    'envelope_name': None,
}


def split_own_options(args):
    """Separate this plugin's options from the ones nanopb should parse.

    Both "--opt=value" and "--opt value" spellings are accepted, matching how
    nanopb itself tolerates protoc's comma separation and shell-style splitting.
    """
    own = dict(OWN_DEFAULTS)
    rest = []

    i = 0
    while i < len(args):
        arg = args[i]
        name, sep, value = arg.partition('=')
        if name in OWN_OPTIONS:
            if not sep:
                # "--opt value" form; consume the following argument
                if i + 1 >= len(args):
                    raise GeneratorError('%s requires a value' % name)
                value = args[i + 1]
                i += 1
            own[OWN_OPTIONS[name]] = value
        else:
            rest.append(arg)
        i += 1

    if own['envelope_mode'] not in ('oneof', 'any'):
        raise GeneratorError(
            "--envelope-mode must be either 'oneof' or 'any', got: '%s'"
            % own['envelope_mode'])

    return own, rest


def parse_plugin_parameter(parameter):
    """Split a protoc plugin parameter string into an argument list.

    Mirrors nanopb_generator.main_plugin so that both plugins accept exactly
    the same spellings.
    """
    try:
        # Versions of Python prior to 2.7.3 do not support unicode
        # input to shlex.split(). Try to convert to str if possible.
        params = str(parameter)
    except UnicodeEncodeError:
        params = parameter

    if ',' not in params and ' -' in params:
        # Nanopb has traditionally supported space as separator in options
        return shlex.split(params)

    # Protoc separates options passed to plugins by comma.
    # This allows also giving --nanopb-validate_opt option multiple times.
    lex = shlex.shlex(params)
    lex.whitespace_split = True
    lex.whitespace = ','
    lex.commenters = ''
    return list(lex)


# ---------------------------------------------------------------------------
#                              Plugin entry point
# ---------------------------------------------------------------------------


def process_file(filename, fdesc, options, other_files):
    """Produce every output this plugin owns, for one .proto file.

    Returns a list of (name, insertion_point_or_None, content) tuples ready to
    be turned into CodeGeneratorResponse entries.
    """
    f = nanopb.parse_file(filename, fdesc, options)
    attach_validate_rules(f)

    # Check the list of dependencies, and if they are available in other_files,
    # add them to be considered for import resolving. Recursively add any files
    # imported by the dependencies.  This mirrors nanopb_generator.process_file,
    # and the validator needs it in order to emit `#include "<dep>_validate.h"`
    # for messages that reference types from another .proto.
    deps = list(f.fdesc.dependency)
    while deps:
        dep = deps.pop(0)
        if dep in other_files:
            f.add_dependency(other_files[dep])
            deps += list(other_files[dep].fdesc.dependency)

    # Match nanopb's own output naming exactly, so our insertion targets line up.
    noext = os.path.splitext(filename)[0]
    headername = noext + options.extension + options.header_extension
    sourcename = noext + options.extension + options.source_extension

    outputs = []

    # 1. The standalone validator files.
    validator_gen = build_validator_generator(f)
    validate_headerdata = ''.join(validator_gen.generate_header())
    validate_sourcedata = ''.join(validator_gen.generate_source())
    if validate_headerdata or validate_sourcedata:
        outputs.append((noext + '_validate' + options.header_extension,
                        None, validate_headerdata))
        outputs.append((noext + '_validate' + options.source_extension,
                        None, validate_sourcedata))

    # 2. The filter code, spliced into the files nanopb already produced.
    root_message, any_envelope_info, envelope_info = resolve_filter_target(f, options)
    if root_message or any_envelope_info or envelope_info:
        outputs.append((headername, 'eof',
                        ''.join(generate_header_injection(f, options))))
        outputs.append((sourcename, 'includes',
                        ''.join(generate_source_includes(f, options))))
        outputs.append((sourcename, 'eof',
                        ''.join(generate_source_injection(f, options))))

    return outputs


def main_plugin():
    """Main function when invoked as a protoc plugin."""
    import io

    if sys.platform == "win32":
        import msvcrt
        # Set stdin and stdout to binary mode
        msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
        msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)

    data = io.open(sys.stdin.fileno(), "rb").read()
    request = plugin_pb2.CodeGeneratorRequest.FromString(data)
    response = plugin_pb2.CodeGeneratorResponse()

    try:
        args = parse_plugin_parameter(request.parameter)
        own, nanopb_args = split_own_options(args)

        if nanopb_validator is None:
            raise GeneratorError("nanopb_validator module is not available; "
                                 "cannot generate validation code.")

        nanopb.optparser.usage = ("protoc --nanopb-validate_out=outdir "
                                  "[--nanopb-validate_opt=option] file.proto")
        options, _ = nanopb.process_cmdline(nanopb_args, is_plugin=True)

        # Carry our own options alongside nanopb's, so the generators below can
        # read everything off a single object.
        for key, value in own.items():
            setattr(options, key, value)

        # Google's protoc does not currently indicate the full path of proto
        # files.  Instead always add the main file path to the search dirs,
        # that works for the common case.
        options.options_path.append(os.path.dirname(request.file_to_generate[0]))

        # Process any include files first, in order to have them available as
        # dependencies when resolving cross-file message references.
        other_files = {}
        for fdesc in request.proto_file:
            dep = nanopb.parse_file(fdesc.name, fdesc, options)
            attach_validate_rules(dep)
            other_files[fdesc.name] = dep

        for filename in request.file_to_generate:
            for fdesc in request.proto_file:
                if fdesc.name == filename:
                    for name, insertion_point, content in process_file(
                            filename, fdesc, options, other_files):
                        entry = response.file.add()
                        entry.name = name
                        if insertion_point:
                            entry.insertion_point = insertion_point
                        entry.content = content
    except GeneratorError as e:
        # Reported by protoc as a plugin failure, without a Python traceback.
        response.error = str(e)

    if hasattr(plugin_pb2.CodeGeneratorResponse, "FEATURE_PROTO3_OPTIONAL"):
        response.supported_features = plugin_pb2.CodeGeneratorResponse.FEATURE_PROTO3_OPTIONAL

    if hasattr(plugin_pb2.CodeGeneratorResponse, "FEATURE_SUPPORTS_EDITIONS"):
        response.supported_features |= plugin_pb2.CodeGeneratorResponse.FEATURE_SUPPORTS_EDITIONS
        response.minimum_edition = descriptor.EDITION_PROTO2
        response.maximum_edition = descriptor.EDITION_2024

    io.open(sys.stdout.fileno(), "wb").write(response.SerializeToString())


if __name__ == '__main__':
    main_plugin()
