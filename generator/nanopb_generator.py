#!/usr/bin/python

'''Generate header file for nanopb from a ProtoBuf FileDescriptorSet.'''
nanopb_version = "nanopb-0.2.8-dev"

import sys

try:
    # Add some dummy imports to keep packaging tools happy.
    import google, distutils.util # bbfreeze seems to need these
    import pkg_resources # pyinstaller / protobuf 2.5 seem to need these
except:
    # Don't care, we will error out later if it is actually important.
    pass

try:
    import google.protobuf.text_format as text_format
    import google.protobuf.descriptor_pb2 as descriptor
except:
    sys.stderr.write('''
         *************************************************************
         *** Could not import the Google protobuf Python libraries ***
         *** Try installing package 'python-protobuf' or similar.  ***
         *************************************************************
    ''' + '\n')
    raise

try:
    import proto.nanopb_pb2 as nanopb_pb2
    import proto.plugin_pb2 as plugin_pb2
except:
    sys.stderr.write('''
         ********************************************************************
         *** Failed to import the protocol definitions for generator.     ***
         *** You have to run 'make' in the nanopb/generator/proto folder. ***
         ********************************************************************
    ''' + '\n')
    raise

# ---------------------------------------------------------------------------
#                     Generation of single fields
# ---------------------------------------------------------------------------

import time
import os.path

# Values are tuple (c type, pb type, encoded size)
FieldD = descriptor.FieldDescriptorProto
datatypes = {
    FieldD.TYPE_BOOL:       ('bool',     'BOOL',        1),
    FieldD.TYPE_DOUBLE:     ('double',   'DOUBLE',      8),
    FieldD.TYPE_FIXED32:    ('uint32_t', 'FIXED32',     4),
    FieldD.TYPE_FIXED64:    ('uint64_t', 'FIXED64',     8),
    FieldD.TYPE_FLOAT:      ('float',    'FLOAT',       4),
    FieldD.TYPE_INT32:      ('int32_t',  'INT32',      10),
    FieldD.TYPE_INT64:      ('int64_t',  'INT64',      10),
    FieldD.TYPE_SFIXED32:   ('int32_t',  'SFIXED32',    4),
    FieldD.TYPE_SFIXED64:   ('int64_t',  'SFIXED64',    8),
    FieldD.TYPE_SINT32:     ('int32_t',  'SINT32',      5),
    FieldD.TYPE_SINT64:     ('int64_t',  'SINT64',     10),
    FieldD.TYPE_UINT32:     ('uint32_t', 'UINT32',      5),
    FieldD.TYPE_UINT64:     ('uint64_t', 'UINT64',     10)
}

class Names:
    '''Keeps a set of nested names and formats them to C identifier.'''
    def __init__(self, parts = ()):
        if isinstance(parts, Names):
            parts = parts.parts
        self.parts = tuple(parts)
    
    def __str__(self):
        return '_'.join(self.parts)

    def __add__(self, other):
        if isinstance(other, (str, unicode)):
            return Names(self.parts + (other,))
        elif isinstance(other, tuple):
            return Names(self.parts + other)
        else:
            raise ValueError("Name parts should be of type str")
    
    def __eq__(self, other):
        return isinstance(other, Names) and self.parts == other.parts
    
def names_from_type_name(type_name):
    '''Parse Names() from FieldDescriptorProto type_name'''
    if type_name[0] != '.':
        raise NotImplementedError("Lookup of non-absolute type names is not supported")
    return Names(type_name[1:].split('.'))

def varint_max_size(max_value):
    '''Returns the maximum number of bytes a varint can take when encoded.'''
    for i in range(1, 11):
        if (max_value >> (i * 7)) == 0:
            return i
    raise ValueError("Value too large for varint: " + str(max_value))

assert varint_max_size(0) == 1
assert varint_max_size(127) == 1
assert varint_max_size(128) == 2

class EncodedSize:
    '''Class used to represent the encoded size of a field or a message.
    Consists of a combination of symbolic sizes and integer sizes.'''
    def __init__(self, value = 0, symbols = []):
        if isinstance(value, (str, Names)):
            symbols = [str(value)]
            value = 0
        self.value = value
        self.symbols = symbols
    
    def __add__(self, other):
        if isinstance(other, (int, long)):
            return EncodedSize(self.value + other, self.symbols)
        elif isinstance(other, (str, Names)):
            return EncodedSize(self.value, self.symbols + [str(other)])
        elif isinstance(other, EncodedSize):
            return EncodedSize(self.value + other.value, self.symbols + other.symbols)
        else:
            raise ValueError("Cannot add size: " + repr(other))

    def __mul__(self, other):
        if isinstance(other, (int, long)):
            return EncodedSize(self.value * other, [str(other) + '*' + s for s in self.symbols])
        else:
            raise ValueError("Cannot multiply size: " + repr(other))

    def __str__(self):
        if not self.symbols:
            return str(self.value)
        else:
            return '(' + str(self.value) + ' + ' + ' + '.join(self.symbols) + ')'

    def upperlimit(self):
        if not self.symbols:
            return self.value
        else:
            return 2**32 - 1

class Enum:
    def __init__(self, names, desc, enum_options):
        '''desc is EnumDescriptorProto'''
        
        self.options = enum_options
        self.names = names + desc.name
        
        if enum_options.long_names:
            self.values = [(self.names + x.name, x.number) for x in desc.value]            
        else:
            self.values = [(names + x.name, x.number) for x in desc.value] 
        
        self.value_longnames = [self.names + x.name for x in desc.value]
    
    def __str__(self):
        result = 'typedef enum _%s {\n' % self.names
        result += ',\n'.join(["    %s = %d" % x for x in self.values])
        result += '\n} %s;' % self.names
        return result

class Field:
    def __init__(self, struct_name, desc, field_options):
        '''desc is FieldDescriptorProto'''
        self.tag = desc.number
        self.struct_name = struct_name
        self.name = desc.name
        self.default = None
        self.max_size = None
        self.max_count = None
        self.array_decl = ""
        self.enc_size = None
        self.ctype = None
        
        # Parse field options
        if field_options.HasField("max_size"):
            self.max_size = field_options.max_size
        
        if field_options.HasField("max_count"):
            self.max_count = field_options.max_count
        
        if desc.HasField('default_value'):
            self.default = desc.default_value
           
        # Check field rules, i.e. required/optional/repeated.
        can_be_static = True
        if desc.label == FieldD.LABEL_REQUIRED:
            self.rules = 'REQUIRED'
        elif desc.label == FieldD.LABEL_OPTIONAL:
            self.rules = 'OPTIONAL'
        elif desc.label == FieldD.LABEL_REPEATED:
            self.rules = 'REPEATED'
            if self.max_count is None:
                can_be_static = False
            else:
                self.array_decl = '[%d]' % self.max_count
        else:
            raise NotImplementedError(desc.label)
        
        # Check if the field can be implemented with static allocation
        # i.e. whether the data size is known.
        if desc.type == FieldD.TYPE_STRING and self.max_size is None:
            can_be_static = False
        
        if desc.type == FieldD.TYPE_BYTES and self.max_size is None:
            can_be_static = False
        
        # Decide how the field data will be allocated
        if field_options.type == nanopb_pb2.FT_DEFAULT:
            if can_be_static:
                field_options.type = nanopb_pb2.FT_STATIC
            else:
                field_options.type = nanopb_pb2.FT_CALLBACK
        
        if field_options.type == nanopb_pb2.FT_STATIC and not can_be_static:
            raise Exception("Field %s is defined as static, but max_size or "
                            "max_count is not given." % self.name)
        
        if field_options.type == nanopb_pb2.FT_STATIC:
            self.allocation = 'STATIC'
        elif field_options.type == nanopb_pb2.FT_POINTER:
            self.allocation = 'POINTER'
        elif field_options.type == nanopb_pb2.FT_CALLBACK:
            self.allocation = 'CALLBACK'
        else:
            raise NotImplementedError(field_options.type)
        
        # Decide the C data type to use in the struct.
        if datatypes.has_key(desc.type):
            self.ctype, self.pbtype, self.enc_size = datatypes[desc.type]
        elif desc.type == FieldD.TYPE_ENUM:
            self.pbtype = 'ENUM'
            self.ctype = names_from_type_name(desc.type_name)
            if self.default is not None:
                self.default = self.ctype + self.default
            self.enc_size = 5 # protoc rejects enum values > 32 bits
        elif desc.type == FieldD.TYPE_STRING:
            self.pbtype = 'STRING'
            self.ctype = 'char'
            if self.allocation == 'STATIC':
                self.ctype = 'char'
                self.array_decl += '[%d]' % self.max_size
                self.enc_size = varint_max_size(self.max_size) + self.max_size
        elif desc.type == FieldD.TYPE_BYTES:
            self.pbtype = 'BYTES'
            if self.allocation == 'STATIC':
                self.ctype = self.struct_name + self.name + 't'
                self.enc_size = varint_max_size(self.max_size) + self.max_size
            elif self.allocation == 'POINTER':
                self.ctype = 'pb_bytes_array_t'
        elif desc.type == FieldD.TYPE_MESSAGE:
            self.pbtype = 'MESSAGE'
            self.ctype = self.submsgname = names_from_type_name(desc.type_name)
            self.enc_size = None # Needs to be filled in after the message type is available
        else:
            raise NotImplementedError(desc.type)
        
    def __cmp__(self, other):
        return cmp(self.tag, other.tag)
    
    def __str__(self):
        result = ''
        if self.allocation == 'POINTER':
            if self.rules == 'REPEATED':
                result += '    size_t ' + self.name + '_count;\n'
            
            if self.pbtype == 'MESSAGE':
                # Use struct definition, so recursive submessages are possible
                result += '    struct _%s *%s;' % (self.ctype, self.name)
            elif self.rules == 'REPEATED' and self.pbtype in ['STRING', 'BYTES']:
                # String/bytes arrays need to be defined as pointers to pointers
                result += '    %s **%s;' % (self.ctype, self.name)
            else:
                result += '    %s *%s;' % (self.ctype, self.name)
        elif self.allocation == 'CALLBACK':
            result += '    pb_callback_t %s;' % self.name
        else:
            if self.rules == 'OPTIONAL' and self.allocation == 'STATIC':
                result += '    bool has_' + self.name + ';\n'
            elif self.rules == 'REPEATED' and self.allocation == 'STATIC':
                result += '    size_t ' + self.name + '_count;\n'
            result += '    %s %s%s;' % (self.ctype, self.name, self.array_decl)
        return result
    
    def types(self):
        '''Return definitions for any special types this field might need.'''
        if self.pbtype == 'BYTES' and self.allocation == 'STATIC':
            result = 'typedef struct {\n'
            result += '    size_t size;\n'
            result += '    uint8_t bytes[%d];\n' % self.max_size
            result += '} %s;\n' % self.ctype
        else:
            result = None
        return result
    
    def default_decl(self, declaration_only = False):
        '''Return definition for this field's default value.'''
        if self.default is None:
            return None

        ctype, default = self.ctype, self.default
        array_decl = ''
        
        if self.pbtype == 'STRING':
            if self.allocation != 'STATIC':
                return None # Not implemented
        
            array_decl = '[%d]' % self.max_size
            default = str(self.default).encode('string_escape')
            default = default.replace('"', '\\"')
            default = '"' + default + '"'
        elif self.pbtype == 'BYTES':
            if self.allocation != 'STATIC':
                return None # Not implemented

            data = self.default.decode('string_escape')
            data = ['0x%02x' % ord(c) for c in data]
            default = '{%d, {%s}}' % (len(data), ','.join(data))
        elif self.pbtype in ['FIXED32', 'UINT32']:
            default += 'u'
        elif self.pbtype in ['FIXED64', 'UINT64']:
            default += 'ull'
        elif self.pbtype in ['SFIXED64', 'INT64']:
            default += 'll'
        
        if declaration_only:
            return 'extern const %s %s_default%s;' % (ctype, self.struct_name + self.name, array_decl)
        else:
            return 'const %s %s_default%s = %s;' % (ctype, self.struct_name + self.name, array_decl, default)
    
    def tags(self):
        '''Return the #define for the tag number of this field.'''
        identifier = '%s_%s_tag' % (self.struct_name, self.name)
        return '#define %-40s %d\n' % (identifier, self.tag)
    
    def pb_field_t(self, prev_field_name):
        '''Return the pb_field_t initializer to use in the constant array.
        prev_field_name is the name of the previous field or None.
        '''
        result  = '    PB_FIELD2(%3d, ' % self.tag
        result += '%-8s, ' % self.pbtype
        result += '%s, ' % self.rules
        result += '%-8s, ' % self.allocation
        result += '%s, ' % ("FIRST" if not prev_field_name else "OTHER")
        result += '%s, ' % self.struct_name
        result += '%s, ' % self.name
        result += '%s, ' % (prev_field_name or self.name)
        
        if self.pbtype == 'MESSAGE':
            result += '&%s_fields)' % self.submsgname
        elif self.default is None:
            result += '0)'
        elif self.pbtype in ['BYTES', 'STRING'] and self.allocation != 'STATIC':
            result += '0)' # Arbitrary size default values not implemented
        elif self.rules == 'OPTEXT':
            result += '0)' # Default value for extensions is not implemented
        else:
            result += '&%s_default)' % (self.struct_name + self.name)
        
        return result
    
    def largest_field_value(self):
        '''Determine if this field needs 16bit or 32bit pb_field_t structure to compile properly.
        Returns numeric value or a C-expression for assert.'''
        if self.pbtype == 'MESSAGE':
            if self.rules == 'REPEATED' and self.allocation == 'STATIC':
                return 'pb_membersize(%s, %s[0])' % (self.struct_name, self.name)
            else:
                return 'pb_membersize(%s, %s)' % (self.struct_name, self.name)

        return max(self.tag, self.max_size, self.max_count)        

    def encoded_size(self, allmsgs):
        '''Return the maximum size that this field can take when encoded,
        including the field tag. If the size cannot be determined, returns
        None.'''
        
        if self.allocation != 'STATIC':
            return None
        
        if self.pbtype == 'MESSAGE':
            for msg in allmsgs:
                if msg.name == self.submsgname:
                    encsize = msg.encoded_size(allmsgs)
                    if encsize is None:
                        return None # Submessage size is indeterminate
                        
                    # Include submessage length prefix
                    encsize += varint_max_size(encsize.upperlimit())
                    break
            else:
                # Submessage cannot be found, this currently occurs when
                # the submessage type is defined in a different file.
                # Instead of direct numeric value, reference the size that
                # has been #defined in the other file.
                encsize = EncodedSize(self.submsgname + 'size')

                # We will have to make a conservative assumption on the length
                # prefix size, though.
                encsize += 5

        elif self.enc_size is None:
            raise RuntimeError("Could not determine encoded size for %s.%s"
                               % (self.struct_name, self.name))
        else:
            encsize = EncodedSize(self.enc_size)
        
        encsize += varint_max_size(self.tag << 3) # Tag + wire type

        if self.rules == 'REPEATED':
            # Decoders must be always able to handle unpacked arrays.
            # Therefore we have to reserve space for it, even though
            # we emit packed arrays ourselves.
            encsize *= self.max_count
        
        return encsize


class ExtensionRange(Field):
    def __init__(self, struct_name, range_start, field_options):
        '''Implements a special pb_extension_t* field in an extensible message
        structure. The range_start signifies the index at which the extensions
        start. Not necessarily all tags above this are extensions, it is merely
        a speed optimization.
        '''
        self.tag = range_start
        self.struct_name = struct_name
        self.name = 'extensions'
        self.pbtype = 'EXTENSION'
        self.rules = 'OPTIONAL'
        self.allocation = 'CALLBACK'
        self.ctype = 'pb_extension_t'
        self.array_decl = ''
        self.default = None
        self.max_size = 0
        self.max_count = 0
        
    def __str__(self):
        return '    pb_extension_t *extensions;'
    
    def types(self):
        return None
    
    def tags(self):
        return ''
        
    def encoded_size(self, allmsgs):
        # We exclude extensions from the count, because they cannot be known
        # until runtime. Other option would be to return None here, but this
        # way the value remains useful if extensions are not used.
        return EncodedSize(0)

class ExtensionField(Field):
    def __init__(self, struct_name, desc, field_options):
        self.fullname = struct_name + desc.name
        self.extendee_name = names_from_type_name(desc.extendee)
        Field.__init__(self, self.fullname + 'struct', desc, field_options)
        
        if self.rules != 'OPTIONAL':
            self.skip = True
        else:
            self.skip = False
            self.rules = 'OPTEXT'

    def tags(self):
        '''Return the #define for the tag number of this field.'''
        identifier = '%s_tag' % self.fullname
        return '#define %-40s %d\n' % (identifier, self.tag)

    def extension_decl(self):
        '''Declaration of the extension type in the .pb.h file'''
        if self.skip:
            msg = '/* Extension field %s was skipped because only "optional"\n' % self.fullname
            msg +='   type of extension fields is currently supported. */\n'
            return msg
        
        return 'extern const pb_extension_type_t %s;\n' % self.fullname

    def extension_def(self):
        '''Definition of the extension type in the .pb.c file'''

        if self.skip:
            return ''

        result  = 'typedef struct {\n'
        result += str(self)
        result += '\n} %s;\n\n' % self.struct_name
        result += ('static const pb_field_t %s_field = \n  %s;\n\n' %
                    (self.fullname, self.pb_field_t(None)))
        result += 'const pb_extension_type_t %s = {\n' % self.fullname
        result += '    NULL,\n'
        result += '    NULL,\n'
        result += '    &%s_field\n' % self.fullname
        result += '};\n'
        return result


# ---------------------------------------------------------------------------
#                   Generation of messages (structures)
# ---------------------------------------------------------------------------


class Message:
    def __init__(self, names, desc, message_options):
        self.name = names
        self.fields = []
        
        for f in desc.field:
            field_options = get_nanopb_suboptions(f, message_options, self.name + f.name)
            if field_options.type != nanopb_pb2.FT_IGNORE:
                self.fields.append(Field(self.name, f, field_options))
        
        if len(desc.extension_range) > 0:
            field_options = get_nanopb_suboptions(desc, message_options, self.name + 'extensions')
            range_start = min([r.start for r in desc.extension_range])
            if field_options.type != nanopb_pb2.FT_IGNORE:
                self.fields.append(ExtensionRange(self.name, range_start, field_options))
        
        self.packed = message_options.packed_struct
        self.ordered_fields = self.fields[:]
        self.ordered_fields.sort()

    def get_dependencies(self):
        '''Get list of type names that this structure refers to.'''
        return [str(field.ctype) for field in self.fields]
    
    def __str__(self):
        result = 'typedef struct _%s {\n' % self.name

        if not self.ordered_fields:
            # Empty structs are not allowed in C standard.
            # Therefore add a dummy field if an empty message occurs.
            result += '    uint8_t dummy_field;'

        result += '\n'.join([str(f) for f in self.ordered_fields])
        result += '\n}'
        
        if self.packed:
            result += ' pb_packed'
        
        result += ' %s;' % self.name
        
        if self.packed:
            result = 'PB_PACKED_STRUCT_START\n' + result
            result += '\nPB_PACKED_STRUCT_END'
        
        return result
    
    def types(self):
        result = ""
        for field in self.fields:
            types = field.types()
            if types is not None:
                result += types + '\n'
        return result
    
    def default_decl(self, declaration_only = False):
        result = ""
        for field in self.fields:
            default = field.default_decl(declaration_only)
            if default is not None:
                result += default + '\n'
        return result

    def fields_declaration(self):
        result = 'extern const pb_field_t %s_fields[%d];' % (self.name, len(self.fields) + 1)
        return result

    def fields_definition(self):
        result = 'const pb_field_t %s_fields[%d] = {\n' % (self.name, len(self.fields) + 1)
        
        prev = None
        for field in self.ordered_fields:
            result += field.pb_field_t(prev)
            result += ',\n'
            prev = field.name
        
        result += '    PB_LAST_FIELD\n};'
        return result

    def encoded_size(self, allmsgs):
        '''Return the maximum size that this message can take when encoded.
        If the size cannot be determined, returns None.
        '''
        size = EncodedSize(0)
        for field in self.fields:
            fsize = field.encoded_size(allmsgs)
            if fsize is None:
                return None
            size += fsize
        
        return size


# ---------------------------------------------------------------------------
#                    Processing of entire .proto files
# ---------------------------------------------------------------------------


def iterate_messages(desc, names = Names()):
    '''Recursively find all messages. For each, yield name, DescriptorProto.'''
    if hasattr(desc, 'message_type'):
        submsgs = desc.message_type
    else:
        submsgs = desc.nested_type
    
    for submsg in submsgs:
        sub_names = names + submsg.name
        yield sub_names, submsg
        
        for x in iterate_messages(submsg, sub_names):
            yield x

def iterate_extensions(desc, names = Names()):
    '''Recursively find all extensions.
    For each, yield name, FieldDescriptorProto.
    '''
    for extension in desc.extension:
        yield names, extension

    for subname, subdesc in iterate_messages(desc, names):
        for extension in subdesc.extension:
            yield subname, extension

def parse_file(fdesc, file_options):
    '''Takes a FileDescriptorProto and returns tuple (enums, messages, extensions).'''
    
    enums = []
    messages = []
    extensions = []
    
    if fdesc.package:
        base_name = Names(fdesc.package.split('.'))
    else:
        base_name = Names()
    
    for enum in fdesc.enum_type:
        enum_options = get_nanopb_suboptions(enum, file_options, base_name + enum.name)
        enums.append(Enum(base_name, enum, enum_options))
    
    for names, message in iterate_messages(fdesc, base_name):
        message_options = get_nanopb_suboptions(message, file_options, names)
        messages.append(Message(names, message, message_options))
        for enum in message.enum_type:
            enum_options = get_nanopb_suboptions(enum, message_options, names + enum.name)
            enums.append(Enum(names, enum, enum_options))
    
    for names, extension in iterate_extensions(fdesc, base_name):
        field_options = get_nanopb_suboptions(extension, file_options, names)
        if field_options.type != nanopb_pb2.FT_IGNORE:
            extensions.append(ExtensionField(names, extension, field_options))
    
    # Fix field default values where enum short names are used.
    for enum in enums:
        if not enum.options.long_names:
            for message in messages:
                for field in message.fields:
                    if field.default in enum.value_longnames:
                        idx = enum.value_longnames.index(field.default)
                        field.default = enum.values[idx][0]
    
    return enums, messages, extensions

def toposort2(data):
    '''Topological sort.
    From http://code.activestate.com/recipes/577413-topological-sort/
    This function is under the MIT license.
    '''
    for k, v in data.items():
        v.discard(k) # Ignore self dependencies
    extra_items_in_deps = reduce(set.union, data.values(), set()) - set(data.keys())
    data.update(dict([(item, set()) for item in extra_items_in_deps]))
    while True:
        ordered = set(item for item,dep in data.items() if not dep)
        if not ordered:
            break
        for item in sorted(ordered):
            yield item
        data = dict([(item, (dep - ordered)) for item,dep in data.items()
                if item not in ordered])
    assert not data, "A cyclic dependency exists amongst %r" % data

def sort_dependencies(messages):
    '''Sort a list of Messages based on dependencies.'''
    dependencies = {}
    message_by_name = {}
    for message in messages:
        dependencies[str(message.name)] = set(message.get_dependencies())
        message_by_name[str(message.name)] = message
    
    for msgname in toposort2(dependencies):
        if msgname in message_by_name:
            yield message_by_name[msgname]

def make_identifier(headername):
    '''Make #ifndef identifier that contains uppercase A-Z and digits 0-9'''
    result = ""
    for c in headername.upper():
        if c.isalnum():
            result += c
        else:
            result += '_'
    return result

def generate_header(dependencies, headername, enums, messages, extensions, options):
    '''Generate content for a header file.
    Generates strings, which should be concatenated and stored to file.
    '''
    
    yield '/* Automatically generated nanopb header */\n'
    if options.notimestamp:
        yield '/* Generated by %s */\n\n' % (nanopb_version)
    else:
        yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
    
    symbol = make_identifier(headername)
    yield '#ifndef _PB_%s_\n' % symbol
    yield '#define _PB_%s_\n' % symbol
    try:
        yield options.libformat % ('pb.h')
    except TypeError:
        # no %s specified - use whatever was passed in as options.libformat
        yield options.libformat
    yield '\n'
    
    for dependency in dependencies:
        noext = os.path.splitext(dependency)[0]
        yield options.genformat % (noext + '.' + options.extension + '.h')
        yield '\n'

    yield '#ifdef __cplusplus\n'
    yield 'extern "C" {\n'
    yield '#endif\n\n'
    
    yield '/* Enum definitions */\n'
    for enum in enums:
        yield str(enum) + '\n\n'
    
    yield '/* Struct definitions */\n'
    for msg in sort_dependencies(messages):
        yield msg.types()
        yield str(msg) + '\n\n'
    
    if extensions:
        yield '/* Extensions */\n'
        for extension in extensions:
            yield extension.extension_decl()
        yield '\n'
        
    yield '/* Default values for struct fields */\n'
    for msg in messages:
        yield msg.default_decl(True)
    yield '\n'
    
    yield '/* Field tags (for use in manual encoding/decoding) */\n'
    for msg in sort_dependencies(messages):
        for field in msg.fields:
            yield field.tags()
    for extension in extensions:
        yield extension.tags()
    yield '\n'
    
    yield '/* Struct field encoding specification for nanopb */\n'
    for msg in messages:
        yield msg.fields_declaration() + '\n'
    yield '\n'
    
    yield '/* Maximum encoded size of messages (where known) */\n'
    for msg in messages:
        msize = msg.encoded_size(messages)
        if msize is not None:
            identifier = '%s_size' % msg.name
            yield '#define %-40s %s\n' % (identifier, msize)
    yield '\n'
    
    yield '#ifdef __cplusplus\n'
    yield '} /* extern "C" */\n'
    yield '#endif\n'
    
    # End of header
    yield '\n#endif\n'

def generate_source(headername, enums, messages, extensions, options):
    '''Generate content for a source file.'''
    
    yield '/* Automatically generated nanopb constant definitions */\n'
    if options.notimestamp:
        yield '/* Generated by %s */\n\n' % (nanopb_version)
    else:
        yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
    yield options.genformat % (headername)
    yield '\n'
    
    for msg in messages:
        yield msg.default_decl(False)
    
    yield '\n\n'
    
    for msg in messages:
        yield msg.fields_definition() + '\n\n'
    
    for ext in extensions:
        yield ext.extension_def() + '\n'
        
    # Add checks for numeric limits
    if messages:
        count_required_fields = lambda m: len([f for f in msg.fields if f.rules == 'REQUIRED'])
        largest_msg = max(messages, key = count_required_fields)
        largest_count = count_required_fields(largest_msg)
        if largest_count > 64:
            yield '\n/* Check that missing required fields will be properly detected */\n'
            yield '#if PB_MAX_REQUIRED_FIELDS < %d\n' % largest_count
            yield '#error Properly detecting missing required fields in %s requires \\\n' % largest_msg.name
            yield '       setting PB_MAX_REQUIRED_FIELDS to %d or more.\n' % largest_count
            yield '#endif\n'
    
    worst = 0
    worst_field = ''
    checks = []
    checks_msgnames = []
    for msg in messages:
        checks_msgnames.append(msg.name)
        for field in msg.fields:
            status = field.largest_field_value()
            if isinstance(status, (str, unicode)):
                checks.append(status)
            elif status > worst:
                worst = status
                worst_field = str(field.struct_name) + '.' + str(field.name)

    if worst > 255 or checks:
        yield '\n/* Check that field information fits in pb_field_t */\n'
        
        if worst > 65535 or checks:
            yield '#if !defined(PB_FIELD_32BIT)\n'
            if worst > 65535:
                yield '#error Field descriptor for %s is too large. Define PB_FIELD_32BIT to fix this.\n' % worst_field
            else:
                assertion = ' && '.join(str(c) + ' < 65536' for c in checks)
                msgs = '_'.join(str(n) for n in checks_msgnames)
                yield '/* If you get an error here, it means that you need to define PB_FIELD_32BIT\n'
                yield ' * compile-time option. You can do that in pb.h or on compiler command line.\n'
                yield ' * \n'
                yield ' * The reason you need to do this is that some of your messages contain tag\n'
                yield ' * numbers or field sizes that are larger than what can fit in 8 or 16 bit\n'
                yield ' * field descriptors.\n'
                yield ' */\n'
                yield 'STATIC_ASSERT((%s), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_%s)\n'%(assertion,msgs)
            yield '#endif\n\n'
        
        if worst < 65536:
            yield '#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)\n'
            if worst > 255:
                yield '#error Field descriptor for %s is too large. Define PB_FIELD_16BIT to fix this.\n' % worst_field
            else:
                assertion = ' && '.join(str(c) + ' < 256' for c in checks)
                msgs = '_'.join(str(n) for n in checks_msgnames)
                yield '/* If you get an error here, it means that you need to define PB_FIELD_16BIT\n'
                yield ' * compile-time option. You can do that in pb.h or on compiler command line.\n'
                yield ' * \n'
                yield ' * The reason you need to do this is that some of your messages contain tag\n'
                yield ' * numbers or field sizes that are larger than what can fit in the default\n'
                yield ' * 8 bit descriptors.\n'
                yield ' */\n'
                yield 'STATIC_ASSERT((%s), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_%s)\n'%(assertion,msgs)
            yield '#endif\n\n'
    
    # Add check for sizeof(double)
    has_double = False
    for msg in messages:
        for field in msg.fields:
            if field.ctype == 'double':
                has_double = True
    
    if has_double:
        yield '\n'
        yield '/* On some platforms (such as AVR), double is really float.\n'
        yield ' * These are not directly supported by nanopb, but see example_avr_double.\n'
        yield ' * To get rid of this error, remove any double fields from your .proto.\n'
        yield ' */\n'
        yield 'STATIC_ASSERT(sizeof(double) == 8, DOUBLE_MUST_BE_8_BYTES)\n'
    
    yield '\n'

# ---------------------------------------------------------------------------
#                    Options parsing for the .proto files
# ---------------------------------------------------------------------------

from fnmatch import fnmatch

def read_options_file(infile):
    '''Parse a separate options file to list:
        [(namemask, options), ...]
    '''
    results = []
    for line in infile:
        line = line.strip()
        if not line or line.startswith('//') or line.startswith('#'):
            continue
        
        parts = line.split(None, 1)
        opts = nanopb_pb2.NanoPBOptions()
        text_format.Merge(parts[1], opts)
        results.append((parts[0], opts))

    return results

class Globals:
    '''Ugly global variables, should find a good way to pass these.'''
    verbose_options = False
    separate_options = []
    matched_namemasks = set()

def get_nanopb_suboptions(subdesc, options, name):
    '''Get copy of options, and merge information from subdesc.'''
    new_options = nanopb_pb2.NanoPBOptions()
    new_options.CopyFrom(options)
    
    # Handle options defined in a separate file
    dotname = '.'.join(name.parts)
    for namemask, options in Globals.separate_options:
        if fnmatch(dotname, namemask):
            Globals.matched_namemasks.add(namemask)
            new_options.MergeFrom(options)
    
    # Handle options defined in .proto
    if isinstance(subdesc.options, descriptor.FieldOptions):
        ext_type = nanopb_pb2.nanopb
    elif isinstance(subdesc.options, descriptor.FileOptions):
        ext_type = nanopb_pb2.nanopb_fileopt
    elif isinstance(subdesc.options, descriptor.MessageOptions):
        ext_type = nanopb_pb2.nanopb_msgopt
    elif isinstance(subdesc.options, descriptor.EnumOptions):
        ext_type = nanopb_pb2.nanopb_enumopt
    else:
        raise Exception("Unknown options type")
    
    if subdesc.options.HasExtension(ext_type):
        ext = subdesc.options.Extensions[ext_type]
        new_options.MergeFrom(ext)
    
    if Globals.verbose_options:
        sys.stderr.write("Options for " + dotname + ": ")
        sys.stderr.write(text_format.MessageToString(new_options) + "\n")
    
    return new_options


# ---------------------------------------------------------------------------
#                         Command line interface
# ---------------------------------------------------------------------------

import sys
import os.path    
from optparse import OptionParser

optparser = OptionParser(
    usage = "Usage: nanopb_generator.py [options] file.pb ...",
    epilog = "Compile file.pb from file.proto by: 'protoc -ofile.pb file.proto'. " +
             "Output will be written to file.pb.h and file.pb.c.")
optparser.add_option("-x", dest="exclude", metavar="FILE", action="append", default=[],
    help="Exclude file from generated #include list.")
optparser.add_option("-e", "--extension", dest="extension", metavar="EXTENSION", default="pb",
    help="Set extension to use instead of 'pb' for generated files. [default: %default]")
optparser.add_option("-f", "--options-file", dest="options_file", metavar="FILE", default="%s.options",
    help="Set name of a separate generator options file.")
optparser.add_option("-Q", "--generated-include-format", dest="genformat",
    metavar="FORMAT", default='#include "%s"\n',
    help="Set format string to use for including other .pb.h files. [default: %default]")
optparser.add_option("-L", "--library-include-format", dest="libformat",
    metavar="FORMAT", default='#include <%s>\n',
    help="Set format string to use for including the nanopb pb.h header. [default: %default]")
optparser.add_option("-T", "--no-timestamp", dest="notimestamp", action="store_true", default=False,
    help="Don't add timestamp to .pb.h and .pb.c preambles")
optparser.add_option("-q", "--quiet", dest="quiet", action="store_true", default=False,
    help="Don't print anything except errors.")
optparser.add_option("-v", "--verbose", dest="verbose", action="store_true", default=False,
    help="Print more information.")
optparser.add_option("-s", dest="settings", metavar="OPTION:VALUE", action="append", default=[],
    help="Set generator option (max_size, max_count etc.).")

def process_file(filename, fdesc, options):
    '''Process a single file.
    filename: The full path to the .proto or .pb source file, as string.
    fdesc: The loaded FileDescriptorSet, or None to read from the input file.
    options: Command line options as they come from OptionsParser.
    
    Returns a dict:
        {'headername': Name of header file,
         'headerdata': Data for the .h header file,
         'sourcename': Name of the source code file,
         'sourcedata': Data for the .c source code file
        }
    '''
    toplevel_options = nanopb_pb2.NanoPBOptions()
    for s in options.settings:
        text_format.Merge(s, toplevel_options)
    
    if not fdesc:
        data = open(filename, 'rb').read()
        fdesc = descriptor.FileDescriptorSet.FromString(data).file[0]
    
    # Check if there is a separate .options file
    try:
        optfilename = options.options_file % os.path.splitext(filename)[0]
    except TypeError:
        # No %s specified, use the filename as-is
        optfilename = options.options_file
    
    if os.path.isfile(optfilename):
        if options.verbose:
            sys.stderr.write('Reading options from ' + optfilename + '\n')

        Globals.separate_options = read_options_file(open(optfilename, "rU"))
    else:
        Globals.separate_options = []
    Globals.matched_namemasks = set()
    
    # Parse the file
    file_options = get_nanopb_suboptions(fdesc, toplevel_options, Names([filename]))
    enums, messages, extensions = parse_file(fdesc, file_options)
    
    # Decide the file names
    noext = os.path.splitext(filename)[0]
    headername = noext + '.' + options.extension + '.h'
    sourcename = noext + '.' + options.extension + '.c'
    headerbasename = os.path.basename(headername)
    
    # List of .proto files that should not be included in the C header file
    # even if they are mentioned in the source .proto.
    excludes = ['nanopb.proto', 'google/protobuf/descriptor.proto'] + options.exclude
    dependencies = [d for d in fdesc.dependency if d not in excludes]
    
    headerdata = ''.join(generate_header(dependencies, headerbasename, enums,
                                         messages, extensions, options))

    sourcedata = ''.join(generate_source(headerbasename, enums,
                                         messages, extensions, options))

    # Check if there were any lines in .options that did not match a member
    unmatched = [n for n,o in Globals.separate_options if n not in Globals.matched_namemasks]
    if unmatched and not options.quiet:
        sys.stderr.write("Following patterns in " + optfilename + " did not match any fields: "
                         + ', '.join(unmatched) + "\n")
        if not Globals.verbose_options:
            sys.stderr.write("Use  protoc --nanopb-out=-v:.   to see a list of the field names.\n")

    return {'headername': headername, 'headerdata': headerdata,
            'sourcename': sourcename, 'sourcedata': sourcedata}
    
def main_cli():
    '''Main function when invoked directly from the command line.'''
    
    options, filenames = optparser.parse_args()
    
    if not filenames:
        optparser.print_help()
        sys.exit(1)
    
    if options.quiet:
        options.verbose = False

    Globals.verbose_options = options.verbose
    
    for filename in filenames:
        results = process_file(filename, None, options)
        
        if not options.quiet:
            sys.stderr.write("Writing to " + results['headername'] + " and "
                             + results['sourcename'] + "\n")
    
        open(results['headername'], 'w').write(results['headerdata'])
        open(results['sourcename'], 'w').write(results['sourcedata'])        

def main_plugin():
    '''Main function when invoked as a protoc plugin.'''

    import sys
    if sys.platform == "win32":
        import os, msvcrt
        # Set stdin and stdout to binary mode
        msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
        msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
    
    data = sys.stdin.read()
    request = plugin_pb2.CodeGeneratorRequest.FromString(data)
    
    import shlex
    args = shlex.split(request.parameter)
    options, dummy = optparser.parse_args(args)
    
    Globals.verbose_options = options.verbose
    
    response = plugin_pb2.CodeGeneratorResponse()
    
    for filename in request.file_to_generate:
        for fdesc in request.proto_file:
            if fdesc.name == filename:
                results = process_file(filename, fdesc, options)
                
                f = response.file.add()
                f.name = results['headername']
                f.content = results['headerdata']

                f = response.file.add()
                f.name = results['sourcename']
                f.content = results['sourcedata']    
    
    sys.stdout.write(response.SerializeToString())

if __name__ == '__main__':
    # Check if we are running as a plugin under protoc
    if 'protoc-gen-' in sys.argv[0] or '--protoc-plugin' in sys.argv:
        main_plugin()
    else:
        main_cli()

