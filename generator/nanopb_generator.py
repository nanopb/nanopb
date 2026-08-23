#!/usr/bin/env python3
# kate: replace-tabs on; indent-width 4;

"""
nanopb_generator.py - Generate C header and source files from Protocol Buffers
================================================================================

This module implements the nanopb code generator, which converts Protocol Buffer
definitions (.proto files or compiled .pb descriptors) into C header (.pb.h) and
source (.pb.c) files suitable for embedded systems with limited resources.

Architecture Overview
---------------------
The generator follows a multi-phase approach:

1. **Parsing Phase**: Read FileDescriptorProto from .pb binary or compile .proto
   files using protoc. Extract messages, enums, fields, and their options.

2. **IR Building Phase**: Construct an intermediate representation using:
   - `ProtoFile`: Top-level file representation
   - `Message`: Message structure with fields and nested types
   - `Field`: Individual field with type, encoding, and options
   - `Enum`: Enumeration types
   - `OneOf`: Oneof groups

3. **Code Generation Phase**: Render C code using the IR:
   - Generate struct definitions for messages
   - Generate field descriptor tables (pb_field_t arrays)
   - Generate initializer macros and size definitions

Key Classes
-----------
- `Names`: Manages hierarchical C identifier construction
- `EncodedSize`: Tracks field/message encoded sizes (symbolic + numeric)
- `ProtoElement`: Base class providing comment handling for all proto elements
- `Field`: Handles all field type variations and encoding rules
- `Message`: Manages message structure and nested content
- `ProtoFile`: Orchestrates the complete file generation process
- `Globals`: Configuration state (naming style, options, etc.)

CLI Usage
---------
As standalone tool:
    python nanopb_generator.py [options] file.pb ...

As protoc plugin:
    protoc --plugin=protoc-gen-nanopb=nanopb_generator.py --nanopb_out=. file.proto

Module Constants
----------------
- `nanopb_version`: Version string for generated file headers
- `datatypes`: Mapping from protobuf types to C types and encoding info
- `reserved_keywords`: C/C++ keywords that need underscore suffix

See Also
--------
- nanopb_validator.py: Validation code generation module
- pb.h: Core nanopb runtime header
"""

from __future__ import unicode_literals

nanopb_version = "nanopb-1.0.0-dev"

import sys
import re
import codecs
import copy
import itertools
import tempfile
import shutil
import shlex
import os
from functools import reduce

# Python-protobuf breaks easily with protoc version differences if
# using the cpp or upb implementation. Force it to use pure Python
# implementation. Performance is not very important in the generator.
if not os.getenv("PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION"):
    os.putenv("PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION", "python")
    os.environ["PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION"] = "python"

try:
    import google.protobuf.text_format as text_format
    import google.protobuf.descriptor_pb2 as descriptor
    import google.protobuf.compiler.plugin_pb2 as plugin_pb2
    import google.protobuf.descriptor
    import google.protobuf.message_factory as message_factory
except:
    sys.stderr.write('''
         **********************************************************************
         *** Could not import the Google protobuf Python libraries          ***
         ***                                                                ***
         *** Easiest solution is often to install the dependencies via pip: ***
         ***    pip install protobuf grpcio-tools                           ***
         **********************************************************************
    ''' + '\n')
    raise

# (Moved) nanopb_validator import will be performed after proto/nanopb_pb2 import
# to ensure validate.proto has been generated when needed.
nanopb_validator = None

# (Moved) validate_pb2 import will occur after nanopb_pb2 has been built.
validate_pb2 = None

# GetMessageClass() is used by modern python-protobuf (around 5.x onwards)
# Retain compatibility with older python-protobuf versions.
try:
    import google.protobuf.message_factory as message_factory
    GetMessageClass = message_factory.GetMessageClass
except AttributeError:
    import google.protobuf.reflection as reflection
    GetMessageClass = reflection.MakeClass

# Depending on how this script is run, we may or may not have PEP366 package name
# available for relative imports.
if not __package__:
    import proto
    from proto._utils import invoke_protoc
    from proto import TemporaryDirectory
else:
    from . import proto
    from .proto._utils import invoke_protoc
    from .proto import TemporaryDirectory

if getattr(sys, 'frozen', False):
    # Binary package, just import the file
    from proto import nanopb_pb2
else:
    # Import nanopb_pb2.py, rebuilds if necessary and not disabled
    # by env variable NANOPB_PB2_NO_REBUILD
    nanopb_pb2 = proto.load_nanopb_pb2()

# Import validation support now that proto generation helper ran
try:
    # Prefer relative import through package layout
    from proto import nanopb_validator as nanopb_validator
except ImportError:
    try:
        import nanopb_validator  # fallback to global module
    except ImportError:
        nanopb_validator = None

# Import validate_pb2 now (generated alongside nanopb.proto)
try:
    from proto import validate_pb2  # Generated from validate.proto
except ImportError:
    try:
        import validate_pb2  # fallback if PYTHONPATH already contains it
    except ImportError:
        validate_pb2 = None  # Leave as None; validation features will be disabled gracefully

# ---------------------------------------------------------------------------
#                     Generation of single fields
# ---------------------------------------------------------------------------

import time
import os.path

# Values are tuple (c type, pb type, encoded size, data_size)
FieldD = descriptor.FieldDescriptorProto
datatypes = {
    FieldD.TYPE_BOOL:       ('bool',     'BOOL',        1,  4),
    FieldD.TYPE_DOUBLE:     ('double',   'DOUBLE',      8,  8),
    FieldD.TYPE_FIXED32:    ('uint32_t', 'FIXED32',     4,  4),
    FieldD.TYPE_FIXED64:    ('uint64_t', 'FIXED64',     8,  8),
    FieldD.TYPE_FLOAT:      ('float',    'FLOAT',       4,  4),
    FieldD.TYPE_INT32:      ('int32_t',  'INT32',      10,  4),
    FieldD.TYPE_INT64:      ('int64_t',  'INT64',      10,  8),
    FieldD.TYPE_SFIXED32:   ('int32_t',  'SFIXED32',    4,  4),
    FieldD.TYPE_SFIXED64:   ('int64_t',  'SFIXED64',    8,  8),
    FieldD.TYPE_SINT32:     ('int32_t',  'SINT32',      5,  4),
    FieldD.TYPE_SINT64:     ('int64_t',  'SINT64',     10,  8),
    FieldD.TYPE_UINT32:     ('uint32_t', 'UINT32',      5,  4),
    FieldD.TYPE_UINT64:     ('uint64_t', 'UINT64',     10,  8),

    # Integer size override option
    (FieldD.TYPE_ENUM,    nanopb_pb2.IS_8):   ('uint8_t', 'ENUM',  4,  1),
    (FieldD.TYPE_ENUM,   nanopb_pb2.IS_16):   ('uint16_t', 'ENUM',  4,  2),
    (FieldD.TYPE_ENUM,   nanopb_pb2.IS_32):   ('uint32_t', 'ENUM',  4,  4),
    (FieldD.TYPE_ENUM,   nanopb_pb2.IS_64):   ('uint64_t', 'ENUM',  4,  8),
    (FieldD.TYPE_INT32,   nanopb_pb2.IS_8):   ('int8_t',   'INT32', 10,  1),
    (FieldD.TYPE_INT32,  nanopb_pb2.IS_16):   ('int16_t',  'INT32', 10,  2),
    (FieldD.TYPE_INT32,  nanopb_pb2.IS_32):   ('int32_t',  'INT32', 10,  4),
    (FieldD.TYPE_INT32,  nanopb_pb2.IS_64):   ('int64_t',  'INT32', 10,  8),
    (FieldD.TYPE_SINT32,  nanopb_pb2.IS_8):   ('int8_t',  'SINT32',  2,  1),
    (FieldD.TYPE_SINT32, nanopb_pb2.IS_16):   ('int16_t', 'SINT32',  3,  2),
    (FieldD.TYPE_SINT32, nanopb_pb2.IS_32):   ('int32_t', 'SINT32',  5,  4),
    (FieldD.TYPE_SINT32, nanopb_pb2.IS_64):   ('int64_t', 'SINT32', 10,  8),
    (FieldD.TYPE_UINT32,  nanopb_pb2.IS_8):   ('uint8_t', 'UINT32',  2,  1),
    (FieldD.TYPE_UINT32, nanopb_pb2.IS_16):   ('uint16_t','UINT32',  3,  2),
    (FieldD.TYPE_UINT32, nanopb_pb2.IS_32):   ('uint32_t','UINT32',  5,  4),
    (FieldD.TYPE_UINT32, nanopb_pb2.IS_64):   ('uint64_t','UINT32', 10,  8),
    (FieldD.TYPE_INT64,   nanopb_pb2.IS_8):   ('int8_t',   'INT64', 10,  1),
    (FieldD.TYPE_INT64,  nanopb_pb2.IS_16):   ('int16_t',  'INT64', 10,  2),
    (FieldD.TYPE_INT64,  nanopb_pb2.IS_32):   ('int32_t',  'INT64', 10,  4),
    (FieldD.TYPE_INT64,  nanopb_pb2.IS_64):   ('int64_t',  'INT64', 10,  8),
    (FieldD.TYPE_SINT64,  nanopb_pb2.IS_8):   ('int8_t',  'SINT64',  2,  1),
    (FieldD.TYPE_SINT64, nanopb_pb2.IS_16):   ('int16_t', 'SINT64',  3,  2),
    (FieldD.TYPE_SINT64, nanopb_pb2.IS_32):   ('int32_t', 'SINT64',  5,  4),
    (FieldD.TYPE_SINT64, nanopb_pb2.IS_64):   ('int64_t', 'SINT64', 10,  8),
    (FieldD.TYPE_UINT64,  nanopb_pb2.IS_8):   ('uint8_t', 'UINT64',  2,  1),
    (FieldD.TYPE_UINT64, nanopb_pb2.IS_16):   ('uint16_t','UINT64',  3,  2),
    (FieldD.TYPE_UINT64, nanopb_pb2.IS_32):   ('uint32_t','UINT64',  5,  4),
    (FieldD.TYPE_UINT64, nanopb_pb2.IS_64):   ('uint64_t','UINT64', 10,  8),
}

reserved_keywords = [
    "NULL", "alignas", "alignof", "and", "and_eq", "asm", "assert", "auto",
    "bitand", "bitor", "bool", "break", "case", "catch", "char", "class",
    "compl", "const", "constexpr", "const_cast", "continue", "decltype",
    "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
    "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
    "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
    "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
    "protected", "public", "register", "reinterpret_cast", "return", "short",
    "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "thread_local", "throw", "true", "try",
    "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
    "void", "volatile", "wchar_t", "while", "xor", "xor_eq", "char8_t",
    "char16_t", "char32_t", "concept", "consteval", "constinit", "co_await",
    "co_return", "co_yield", "requires",
]

class NamingStyle:
    """
    Base class for C identifier naming conventions.
    
    NamingStyle provides methods to transform protobuf names into C identifiers
    for different contexts (types, variables, enums, etc.). The base class
    provides identity transformations with minimal prefixing.
    
    Subclass this to implement different naming conventions. Methods can be
    overridden individually to customize specific identifier types.
    
    Methods:
        enum_name: Format for enum type tags (e.g., "enum _MyEnum")
        struct_name: Format for struct tags (e.g., "struct _MyMessage")
        union_name: Format for union tags
        type_name: Format for typedef names (e.g., "MyMessage")
        define_name: Format for #define macro names
        var_name: Format for variable names (handles reserved keywords)
        enum_entry: Format for enum value names
        func_name: Format for function names
        bytes_type: Format for bytes field typedefs
    
    The base implementation prefixes enum/struct/union tags with underscore
    and leaves other names unchanged (except adding underscore suffix for
    reserved keywords in var_name).
    """
    def enum_name(self, name):
        return "_%s" % (name)

    def struct_name(self, name):
        return "_%s" % (name)

    def union_name(self, name):
        return "_%s" % (name)

    def type_name(self, name):
        return "%s" % (name)

    def define_name(self, name):
        return "%s" % (name)

    def var_name(self, name):
        val = "%s" % (name)
        if val in reserved_keywords:
            val += '_'
        return val

    def enum_entry(self, name):
        return "%s" % (name)

    def func_name(self, name):
        return "%s" % (name)

    def bytes_type(self, struct_name, name):
        return "%s_%s_t" % (struct_name, name)

class NamingStyleC(NamingStyle):
    """
    C-style naming convention using snake_case identifiers.
    
    This style converts CamelCase names to snake_case and follows
    traditional C naming conventions:
    - Type names end with _t suffix (e.g., my_message_t)
    - Constants and enum entries are UPPER_SNAKE_CASE
    - Variables and functions are lower_snake_case
    
    Enable with -C or --c-style command line option.
    
    Example transformations:
        MyMessage -> my_message_t (type_name)
        MyMessage -> MY_MESSAGE (define_name)
        fieldName -> field_name (var_name)
        MyEnum.VALUE_ONE -> MY_ENUM_VALUE_ONE (enum_entry)
    """
    def enum_name(self, name):
        return self.underscore(name)

    def struct_name(self, name):
        return self.underscore(name)

    def union_name(self, name):
        return self.underscore(name)

    def type_name(self, name):
        return "%s_t" % self.underscore(name)

    def define_name(self, name):
        return self.underscore(name).upper()

    def var_name(self, name):
        val = self.underscore(name)
        if val in reserved_keywords:
            val += '_'
        return val

    def enum_entry(self, name):
        return self.underscore(name).upper()

    def func_name(self, name):
        return self.underscore(name)

    def bytes_type(self, struct_name, name):
        return "%s_%s_t" % (self.underscore(struct_name), self.underscore(name))

    def underscore(self, word):
        word = str(word)
        word = re.sub(r"([A-Z]+)([A-Z][a-z])", r'\1_\2', word)
        word = re.sub(r"([a-z\d])([A-Z])", r'\1_\2', word)
        word = word.replace("-", "_")
        return word.lower()

class Globals:
    """
    Global configuration state for the code generator.
    
    This class holds generator-wide settings that need to be accessible from
    multiple parts of the codebase. While global state is generally discouraged,
    these values are essentially read-only configuration after CLI parsing.
    
    Attributes:
        verbose_options (bool): If True, print detailed option information
            during generation. Useful for debugging .options file matching.
        separate_options (list): List of (namemask, NanoPBOptions) tuples
            from .options files. Used to apply options to matching fields.
        matched_namemasks (set): Tracks which namemasks from separate_options
            were actually matched. Used to warn about unused patterns.
        protoc_insertion_points (bool): If True, emit protoc insertion point
            comments in output for use by custom protoc plugins.
        naming_style (NamingStyle): The active naming convention for generated
            identifiers. Default is identity; NamingStyleC provides snake_case.
    
    Note:
        These are initialized by process_cmdline() and should be treated as
        read-only after that point.
    """
    verbose_options = False
    separate_options = []
    matched_namemasks = set()
    protoc_insertion_points = False
    naming_style = NamingStyle()

class Names:
    """
    Represents a hierarchical identifier name that can be formatted for C.
    
    Protocol Buffer names are hierarchical (e.g., "package.OuterMessage.InnerMessage.field").
    This class maintains the hierarchy as a tuple of parts and provides operations
    for combining names and formatting them as C identifiers using underscore
    separation (e.g., "package_OuterMessage_InnerMessage_field").
    
    Attributes:
        parts (tuple): The individual name components as strings.
    
    Examples:
        >>> n = Names(('MyPackage', 'MyMessage'))
        >>> str(n)
        'MyPackage_MyMessage'
        >>> n + 'field_name'
        Names('MyPackage','MyMessage','field_name')
        >>> Names('SinglePart')
        Names('SinglePart')
    
    Note:
        The actual formatting (e.g., adding '_t' suffix, converting to snake_case)
        is handled by the NamingStyle classes, not by Names itself.
    """
    def __init__(self, parts = ()):
        if isinstance(parts, Names):
            parts = parts.parts
        elif isinstance(parts, str):
            parts = (parts,)
        self.parts = tuple(parts)

        if self.parts == ('',):
            self.parts = ()

    def __str__(self):
        return '_'.join(self.parts)

    def __repr__(self):
        return 'Names(%s)' % ','.join("'%s'" % x for x in self.parts)

    def __add__(self, other):
        if isinstance(other, str):
            return Names(self.parts + (other,))
        elif isinstance(other, Names):
            return Names(self.parts + other.parts)
        elif isinstance(other, tuple):
            return Names(self.parts + other)
        else:
            raise ValueError("Name parts should be of type str")

    def __eq__(self, other):
        return isinstance(other, Names) and self.parts == other.parts

    def __lt__(self, other):
        if not isinstance(other, Names):
            return NotImplemented
        return str(self) < str(other)

def names_from_type_name(type_name):
    '''Parse Names() from FieldDescriptorProto type_name'''
    if type_name[0] != '.':
        raise NotImplementedError("Lookup of non-absolute type names is not supported")
    return Names(type_name[1:].split('.'))

def varint_max_size(max_value):
    '''Returns the maximum number of bytes a varint can take when encoded.'''
    if max_value < 0:
        max_value = 2**64 - max_value
    for i in range(1, 11):
        if (max_value >> (i * 7)) == 0:
            return i
    raise ValueError("Value too large for varint: " + str(max_value))

assert varint_max_size(-1) == 10
assert varint_max_size(0) == 1
assert varint_max_size(127) == 1
assert varint_max_size(128) == 2

class EncodedSize:
    """
    Represents the encoded size of a protobuf field or message.
    
    Encoded sizes can be a combination of:
    - A fixed numeric value (known at generation time)
    - Symbolic expressions (e.g., references to max_size options)
    
    This allows the generator to produce compile-time size expressions like:
        (12 + MyMessage_data_size + 5*MAX_ITEM_COUNT)
    
    Attributes:
        value (int): The fixed numeric portion of the size.
        symbols (list): List of symbolic size expressions (strings).
        declarations (list): C declarations needed for this size (e.g., union types).
        required_defines (list): Preprocessor defines required for this size.
    
    Examples:
        >>> EncodedSize(10)  # Fixed size of 10 bytes
        >>> EncodedSize('MAX_SIZE')  # Symbolic size
        >>> EncodedSize(5) + EncodedSize('DYNAMIC_PART')  # Combined size
    
    The string representation produces valid C expressions for use in macros.
    """
    def __init__(self, value = 0, symbols = [], declarations = [], required_defines = []):
        if isinstance(value, EncodedSize):
            self.value = value.value
            self.symbols = value.symbols
            self.declarations = value.declarations
            self.required_defines = value.required_defines
        elif isinstance(value, (str, Names)):
            self.symbols = [str(value)]
            self.value = 0
            self.declarations = []
            self.required_defines = [str(value)]
        else:
            self.value = value
            self.symbols = symbols
            self.declarations = declarations
            self.required_defines = required_defines

    def __add__(self, other):
        if isinstance(other, int):
            return EncodedSize(self.value + other, self.symbols, self.declarations, self.required_defines)
        elif isinstance(other, (str, Names)):
            return EncodedSize(self.value, self.symbols + [str(other)], self.declarations, self.required_defines + [str(other)])
        elif isinstance(other, EncodedSize):
            return EncodedSize(self.value + other.value, self.symbols + other.symbols,
                               self.declarations + other.declarations, self.required_defines + other.required_defines)
        else:
            raise ValueError("Cannot add size: " + repr(other))

    def __mul__(self, other):
        if isinstance(other, int):
            return EncodedSize(self.value * other, [str(other) + '*' + s for s in self.symbols],
                               self.declarations, self.required_defines)
        else:
            raise ValueError("Cannot multiply size: " + repr(other))

    def __str__(self):
        if not self.symbols:
            return str(self.value)
        else:
            return '(' + str(self.value) + ' + ' + ' + '.join(self.symbols) + ')'

    def __repr__(self):
        return 'EncodedSize(%s, %s, %s, %s)' % (self.value, self.symbols, self.declarations, self.required_defines)

    def get_declarations(self):
        '''Get any declarations that must appear alongside this encoded size definition,
        such as helper union {} types.'''
        return '\n'.join(self.declarations)

    def get_cpp_guard(self, local_defines):
        '''Get an #if preprocessor statement listing all defines that are required for this definition.'''
        needed = [x for x in self.required_defines if x not in local_defines]
        if needed:
            return '#if ' + ' && '.join(['defined(%s)' % x for x in needed]) + "\n"
        else:
            return ''

    def upperlimit(self):
        if not self.symbols:
            return self.value
        else:
            return 2**32 - 1

class ProtoElement(object):
    """
    Base class for all protobuf elements that can have source code comments.
    
    This class provides shared functionality for handling source code comments
    from .proto files. Protobuf's SourceCodeInfo tracks leading and trailing
    comments for each element using a path-based addressing scheme.
    
    The path system uses integers defined in descriptor.proto to identify
    element types within the FileDescriptor hierarchy. Note that some values
    appear duplicated because they represent different fields in different
    descriptor types:
    
    At FileDescriptor level:
    - MESSAGE (4): FileDescriptorProto.message_type
    - ENUM (5): FileDescriptorProto.enum_type
    
    At DescriptorProto (message) level:
    - FIELD (2): DescriptorProto.field
    - NESTED_TYPE (3): DescriptorProto.nested_type
    - NESTED_ENUM (4): DescriptorProto.enum_type
    
    Attributes:
        element_path (tuple): Path to this element in the FileDescriptor.
        comments (dict): Mapping from paths to SourceCodeInfo.Location objects.
    
    Subclasses:
        - Enum: Enumeration type
        - Field: Message field
        - Message: Message type
        - OneOf: Oneof group (also extends Field)
    
    References:
        https://github.com/google/protobuf/blob/master/src/google/protobuf/descriptor.proto
    """
    # Constants regarding path of proto elements in file descriptor.
    # They are used to connect proto elements with source code information (comments)
    # These values come from:
    # https://github.com/google/protobuf/blob/master/src/google/protobuf/descriptor.proto
    FIELD = 2
    MESSAGE = 4
    ENUM = 5
    NESTED_TYPE = 3
    NESTED_ENUM = 4

    def __init__(self, path, comments = None):
        '''
        path is a tuple containing integers (type, index, ...)
        comments is a dictionary mapping between element path & SourceCodeInfo.Location
            (contains information about source comments).
        '''
        assert(isinstance(path, tuple))
        self.element_path = path
        self.comments = comments or {}

    def get_member_comments(self, index):
        '''Get comments for a member of enum or message.'''
        return self.get_comments((ProtoElement.FIELD, index), leading_indent = True)

    def format_comment(self, comment):
        '''Put comment inside /* */ and sanitize comment contents'''
        comment = comment.strip()
        comment = comment.replace('/*', '/ *')
        comment = comment.replace('*/', '* /')
        return "/* %s */" % comment

    def get_comments(self, member_path = (), leading_indent = False):
        '''Get leading & trailing comments for a protobuf element.

        member_path is the proto path of an element or member (ex. [5 0] or [4 1 2 0])
        leading_indent is a flag that indicates if leading comments should be indented
        '''

        # Obtain SourceCodeInfo.Location object containing comment
        # information (based on the member path)
        path = self.element_path + member_path
        comment = self.comments.get(path)

        leading_comment = ""
        trailing_comment = ""

        if not comment:
            return leading_comment, trailing_comment

        if comment.leading_comments:
            leading_comment = "    " if leading_indent else ""
            leading_comment += self.format_comment(comment.leading_comments)

        if comment.trailing_comments:
            trailing_comment = self.format_comment(comment.trailing_comments)

        return leading_comment, trailing_comment


class Enum(ProtoElement):
    """
    Represents a protobuf enum type for C code generation.
    
    Generates a C typedef enum with the appropriate naming convention
    and optional helper functions (enum_to_string, enum_validate).
    
    Attributes:
        names (Names): Fully qualified enum name
        values (list): List of (Names, int) tuples for enum values
        value_longnames (list): Full names for each value (for long_names mode)
        options: NanoPBOptions for this enum
        packed (bool): If True, use pb_packed attribute
    
    Generated C Code Example:
        typedef enum _MyEnum {
            MyEnum_VALUE_A = 0,
            MyEnum_VALUE_B = 1
        } MyEnum;
        
        #define _MyEnum_MIN MyEnum_VALUE_A
        #define _MyEnum_MAX MyEnum_VALUE_B
        #define _MyEnum_ARRAYSIZE ((MyEnum)(MyEnum_VALUE_B+1))
    """
    def __init__(self, names, desc, enum_options, element_path, comments):
        """
        Initialize an Enum from an EnumDescriptorProto.
        
        Args:
            names: Names object for this enum
            desc: EnumDescriptorProto from protobuf descriptor
            enum_options: NanoPBOptions for this enum
            element_path: Tuple path for source comment lookup
            comments: Dict mapping paths to SourceCodeInfo.Location
        """
        super(Enum, self).__init__(element_path, comments)

        self.options = enum_options
        self.names = names

        # by definition, `names` include this enum's name
        base_name = Names(names.parts[:-1])

        if enum_options.long_names:
            self.values = [(names + x.name, x.number) for x in desc.value]
        else:
            self.values = [(base_name + x.name, x.number) for x in desc.value]

        self.value_longnames = [self.names + x.name for x in desc.value]
        self.packed = enum_options.packed_enum

    def has_negative(self):
        for n, v in self.values:
            if v < 0:
                return True
        return False

    def encoded_size(self):
        return max([varint_max_size(v) for n,v in self.values])

    def __repr__(self):
        return 'Enum(%s)' % self.names

    def __str__(self):
        leading_comment, trailing_comment = self.get_comments()

        result = ''
        if leading_comment:
            result = '%s\n' % leading_comment

        result += 'typedef enum %s' % Globals.naming_style.enum_name(self.names)

        # Override the enum size if user wants to use smaller integers
        if (FieldD.TYPE_ENUM, self.options.enum_intsize) in datatypes:
            self.ctype, self.pbtype, self.enc_size, self.data_item_size = datatypes[(FieldD.TYPE_ENUM, self.options.enum_intsize)]
            result += ': ' + self.ctype

        result += ' {'

        if trailing_comment:
            result += " " + trailing_comment

        result += "\n"

        enum_length = len(self.values)
        enum_values = []
        for index, (name, value) in enumerate(self.values):
            leading_comment, trailing_comment = self.get_member_comments(index)

            if leading_comment:
                enum_values.append(leading_comment)

            comma = ","
            if index == enum_length - 1:
                # last enum member should not end with a comma
                comma = ""

            enum_value = "    %s = %d%s" % (Globals.naming_style.enum_entry(name), value, comma)
            if trailing_comment:
                enum_value += " " + trailing_comment

            enum_values.append(enum_value)

        result += '\n'.join(enum_values)
        result += '\n}'

        if self.packed:
            result += ' pb_packed'

        result += ' %s;' % Globals.naming_style.type_name(self.names)
        return result

    def auxiliary_defines(self):
        # sort the enum by value
        sorted_values = sorted(self.values, key = lambda x: (x[1], x[0]))

        unmangledName = self.protofile.manglenames.unmangle(self.names)
        identifier = Globals.naming_style.define_name('_%s_MIN' % self.names)
        result = '#define %s %s\n' % (
            identifier,
            Globals.naming_style.enum_entry(sorted_values[0][0]))
        if unmangledName:
            unmangledIdentifier = Globals.naming_style.define_name('_%s_MIN' % unmangledName)
            self.protofile.manglenames.reverse_name_mapping[identifier] = unmangledIdentifier

        identifier = Globals.naming_style.define_name('_%s_MAX' % self.names)
        result += '#define %s %s\n' % (
            identifier,
            Globals.naming_style.enum_entry(sorted_values[-1][0]))
        if unmangledName:
            unmangledIdentifier = Globals.naming_style.define_name('_%s_MAX' % unmangledName)
            self.protofile.manglenames.reverse_name_mapping[identifier] = unmangledIdentifier

        identifier = Globals.naming_style.define_name('_%s_ARRAYSIZE' % self.names)
        result += '#define %s ((%s)(%s+1))\n' % (
            identifier,
            Globals.naming_style.type_name(self.names),
            Globals.naming_style.enum_entry(sorted_values[-1][0]))
        if unmangledName:
            unmangledIdentifier = Globals.naming_style.define_name('_%s_ARRAYSIZE' % unmangledName)
            self.protofile.manglenames.reverse_name_mapping[identifier] = unmangledIdentifier

        if not self.options.long_names:
            # Define the long names always so that enum value references
            # from other files work properly.
            for i, x in enumerate(self.values):
                result += '#define %s %s\n' % (Globals.naming_style.define_name(self.value_longnames[i]), Globals.naming_style.enum_entry(x[0]))

        if self.options.enum_to_string:
            result += 'const char *%s(%s v);\n' % (
                Globals.naming_style.func_name('%s_name' % self.names),
                Globals.naming_style.type_name(self.names))

        if self.options.enum_validate:
            result += 'bool %s(%s v);\n' % (
                Globals.naming_style.func_name('%s_valid' % self.names),
                Globals.naming_style.type_name(self.names))

        return result

    def enum_to_string_definition(self):
        if not self.options.enum_to_string:
            return ""

        result = 'const char *%s(%s v) {\n' % (
            Globals.naming_style.func_name('%s_name' % self.names),
            Globals.naming_style.type_name(self.names))

        result += '    switch (v) {\n'

        for ((enumname, _), strname) in zip(self.values, self.value_longnames):
            # Just use the last part of the string value.
            result += '        case %s: return "%s";\n' % (
                Globals.naming_style.enum_entry(enumname),
                Globals.naming_style.enum_entry(strname.parts[-1]))

        result += '    }\n'
        result += '    return "unknown";\n'
        result += '}\n'

        return result

    def enum_validate(self):
        if not self.options.enum_validate:
            return ""

        result = 'bool %s(%s v) {\n' % (
            Globals.naming_style.func_name('%s_valid' % self.names),
            Globals.naming_style.type_name(self.names))

        result += '    switch (v) {\n'

        for (enumname, _) in self.values:
            result += '        case %s: return true;\n' % (
                Globals.naming_style.enum_entry(enumname)
                )

        result += '    }\n'
        result += '    return false;\n'
        result += '}\n'

        return result


class FieldMaxSize:
    """
    Tracks the maximum encoded size for a field, used for buffer allocation.
    
    This class helps determine the worst-case buffer size needed to encode
    a message by tracking both the size value and which field contributes
    to it (useful for debugging/optimization).
    
    Attributes:
        worst (int): The maximum size value found so far.
        worst_field (str): Name of the field contributing the worst-case size.
        checks (list): List of size checks to perform at compile time.
    """
    def __init__(self, worst = 0, checks = [], field_name = 'undefined'):
        if isinstance(worst, list):
            self.worst = max(i for i in worst if i is not None)
        else:
            self.worst = worst

        self.worst_field = field_name
        self.checks = list(checks)

    def extend(self, extend, field_name = None):
        self.worst = max(self.worst, extend.worst)

        if self.worst == extend.worst:
            self.worst_field = extend.worst_field

        self.checks.extend(extend.checks)

class Field(ProtoElement):
    """
    Represents a single field within a protobuf message.
    
    This class handles all the complexity of protobuf field types, including:
    - Scalar types (int32, string, bytes, etc.)
    - Nested messages and enums
    - Repeated fields (with and without fixed counts)
    - Optional vs required vs proto3 semantics
    - Pointer, static, and callback allocation modes
    
    The Field class is responsible for:
    1. Parsing field options from NanoPBOptions
    2. Determining C type and encoding information
    3. Generating struct member declarations
    4. Generating field descriptor macro invocations (PB_FIELD, etc.)
    5. Computing encoded sizes for buffer allocation
    
    Attributes:
        tag (int): Protobuf field number
        name (str): Field name from .proto
        struct_name (Names): Parent message name
        rules (str): REQUIRED, OPTIONAL, REPEATED, FIXARRAY, or SINGULAR
        allocation (str): STATIC, POINTER, or CALLBACK
        ctype (str): C type for the field value
        pbtype (str): Protobuf type identifier (INT32, STRING, MESSAGE, etc.)
        validate_rules: Optional validation rules from validate.proto
    
    Class Attributes:
        macro_x_param (str): Parameter name for X-macro expansion ('X')
        macro_a_param (str): Parameter name for additional macro args ('a')
    """
    macro_x_param = 'X'
    macro_a_param = 'a'

    def __init__(self, struct_name, desc, field_options, element_path = (), comments = None):
        """
        Initialize a Field from a FieldDescriptorProto.
        
        Args:
            struct_name: Names object for the parent message
            desc: FieldDescriptorProto from protobuf descriptor
            field_options: NanoPBOptions for this field
            element_path: Tuple path for source comment lookup
            comments: Dict mapping paths to SourceCodeInfo.Location
        """
        ProtoElement.__init__(self, element_path, comments)
        self.tag = desc.number
        self.struct_name = struct_name
        self.union_name = None
        self.name = desc.name
        self.default = None
        self.max_size = None
        self.max_count = None
        self.array_decl = ""
        self.enc_size = None
        self.data_item_size = None
        self.ctype = None
        self.fixed_count = False
        self.callback_datatype = field_options.callback_datatype
        self.math_include_required = False
        self.sort_by_tag = field_options.sort_by_tag
        self.submsg_callback_requested = False
        self.can_be_static = True
        self.validate_rules = None  # Validation rules

        if field_options.type == nanopb_pb2.FT_INLINE:
            # Before nanopb-0.3.8, fixed length bytes arrays were specified
            # by setting type to FT_INLINE. But to handle pointer typed fields,
            # it makes sense to have it as a separate option.
            field_options.type = nanopb_pb2.FT_STATIC
            field_options.fixed_length = True

        # Parse field options
        if field_options.HasField("max_size"):
            self.max_size = field_options.max_size

        if field_options.HasField("initializer"):
            self.initializer = field_options.initializer
        else:
            self.initializer = None

        self.default_has = field_options.default_has

        if desc.type == FieldD.TYPE_STRING and field_options.HasField("max_length"):
            # max_length overrides max_size for strings
            self.max_size = field_options.max_length + 1

        if field_options.HasField("max_count"):
            self.max_count = field_options.max_count

        if desc.HasField('default_value'):
            self.default = desc.default_value

        # Check field rules, i.e. required/optional/repeated.
        if field_options.HasField("label_override"):
            # Process overrides from nanopb options
            desc.label = field_options.label_override
        elif hasattr(desc.options, "features"):
            # For protobuf 'editions', the field presence is set under features
            field_presence = desc.options.features.field_presence
            if field_presence == descriptor.FeatureSet.LEGACY_REQUIRED:
                desc.label = FieldD.LABEL_REQUIRED
            elif field_presence == descriptor.FeatureSet.EXPLICIT:
                desc.label = FieldD.LABEL_OPTIONAL
            elif field_presence == descriptor.FeatureSet.IMPLICIT:
                desc.label = FieldD.LABEL_OPTIONAL
                field_options.proto3 = True

        if desc.label == FieldD.LABEL_REPEATED:
            self.rules = 'REPEATED'
            if self.max_count is None:
                self.can_be_static = False
            else:
                self.array_decl = '[%d]' % self.max_count
                if field_options.fixed_count:
                  self.rules = 'FIXARRAY'

        elif desc.label == FieldD.LABEL_REQUIRED:
            # We allow LABEL_REQUIRED using label_override even for proto3 (see #962)
            self.rules = 'REQUIRED'
        elif field_options.proto3:
            if desc.type == FieldD.TYPE_MESSAGE and not field_options.proto3_singular_msgs:
                # In most other protobuf libraries proto3 submessages have
                # "null" status. For nanopb, that is implemented as has_ field.
                self.rules = 'OPTIONAL'
            elif hasattr(desc, "proto3_optional") and desc.proto3_optional:
                # Protobuf 3.12 introduced optional fields for proto3 syntax
                self.rules = 'OPTIONAL'
            else:
                # Proto3 singular fields (without has_field)
                self.rules = 'SINGULAR'
        elif desc.label == FieldD.LABEL_OPTIONAL:
            self.rules = 'OPTIONAL'
        else:
            raise NotImplementedError(desc.label)

        # Check if the field can be implemented with static allocation
        # i.e. whether the data size is known.
        if desc.type == FieldD.TYPE_STRING and self.max_size is None:
            self.can_be_static = False

        if desc.type == FieldD.TYPE_BYTES and self.max_size is None:
            self.can_be_static = False

        # Decide how the field data will be allocated
        if field_options.type == nanopb_pb2.FT_DEFAULT:
            if self.can_be_static:
                field_options.type = nanopb_pb2.FT_STATIC
            else:
                field_options.type = field_options.fallback_type

        if field_options.type == nanopb_pb2.FT_STATIC and not self.can_be_static:
            raise Exception("Field '%s' is defined as static, but max_size or "
                            "max_count is not given." % self.name)

        if field_options.fixed_count and self.max_count is None:
            raise Exception("Field '%s' is defined as fixed count, "
                            "but max_count is not given." % self.name)

        if field_options.type == nanopb_pb2.FT_STATIC:
            self.allocation = 'STATIC'
        elif field_options.type == nanopb_pb2.FT_POINTER:
            self.allocation = 'POINTER'
        elif field_options.type == nanopb_pb2.FT_CALLBACK:
            self.allocation = 'CALLBACK'
        else:
            raise NotImplementedError(field_options.type)

        if field_options.HasField("type_override"):
            desc.type = field_options.type_override

        # Decide the C data type to use in the struct.
        if desc.type in datatypes:
            self.ctype, self.pbtype, self.enc_size, self.data_item_size = datatypes[desc.type]

            # Override the field size if user wants to use smaller integers
            if (desc.type, field_options.int_size) in datatypes:
                self.ctype, self.pbtype, self.enc_size, self.data_item_size = datatypes[(desc.type, field_options.int_size)]
        elif desc.type == FieldD.TYPE_ENUM:
            self.pbtype = 'ENUM'
            self.data_item_size = 4
            self.ctype = names_from_type_name(desc.type_name)
            if self.default is not None:
                self.default = self.ctype + self.default
            self.enc_size = None # Needs to be filled in when enum values are known
        elif desc.type == FieldD.TYPE_STRING:
            self.pbtype = 'STRING'
            self.ctype = 'char'
            if self.allocation == 'STATIC':
                self.ctype = 'char'
                self.array_decl += '[%d]' % self.max_size
                # -1 because of null terminator. Both pb_encode and pb_decode
                # check the presence of it.
            if self.can_be_static:
                self.enc_size = varint_max_size(self.max_size) + self.max_size - 1
        elif desc.type == FieldD.TYPE_BYTES:
            if field_options.fixed_length:
                self.pbtype = 'FIXED_LENGTH_BYTES'

                if self.max_size is None:
                    raise Exception("Field '%s' is defined as fixed length, "
                                    "but max_size is not given." % self.name)

                self.ctype = 'pb_byte_t'
                self.array_decl += '[%d]' % self.max_size
            else:
                self.pbtype = 'BYTES'
                self.ctype = 'pb_bytes_array_t'
                if self.allocation == 'STATIC':
                    self.ctype = Globals.naming_style.bytes_type(self.struct_name, self.name)
            if self.can_be_static:
                self.enc_size = varint_max_size(self.max_size) + self.max_size
        elif desc.type == FieldD.TYPE_MESSAGE:
            self.pbtype = 'MESSAGE'
            self.ctype = self.submsgname = names_from_type_name(desc.type_name)
            self.enc_size = None # Needs to be filled in after the message type is available
            self.submsg_callback_requested = field_options.submsg_callback

            # Add submessage callback for statically allocated fields inside or
            # outside oneofs. This can be used for repeated fields and oneofs.
            if field_options.submsg_callback and self.allocation == 'STATIC':
                self.pbtype = 'MSG_W_CB'
        else:
            raise NotImplementedError(desc.type)

        if self.default and self.pbtype in ['FLOAT', 'DOUBLE']:
            if 'inf' in self.default or 'nan' in self.default:
                self.math_include_required = True

    def __lt__(self, other):
        return self.tag < other.tag

    def __repr__(self):
        return 'Field(%s)' % self.name

    def __str__(self):
        result = ''

        var_name = Globals.naming_style.var_name(self.name)
        type_name = Globals.naming_style.type_name(self.ctype) if isinstance(self.ctype, Names) else self.ctype

        if self.allocation == 'POINTER':
            if self.rules == 'REPEATED':
                if self.pbtype == 'MSG_W_CB':
                    result += '    pb_callback_t cb_' + var_name + ';\n'
                result += '    pb_size_t ' + var_name + '_count;\n'

            if self.rules == 'FIXARRAY' and self.pbtype in ['STRING', 'BYTES']:
                # Pointer to fixed size array of pointers
                result += '    %s* (*%s)%s;' % (type_name, var_name, self.array_decl)
            elif self.pbtype == 'FIXED_LENGTH_BYTES' or self.rules == 'FIXARRAY':
                # Pointer to fixed size array of items
                result += '    %s (*%s)%s;' % (type_name, var_name, self.array_decl)
            elif self.rules == 'REPEATED' and self.pbtype in ['STRING', 'BYTES']:
                # String/bytes arrays need to be defined as pointers to pointers
                result += '    %s **%s;' % (type_name, var_name)
            elif self.pbtype in ['MESSAGE', 'MSG_W_CB']:
                # Use struct definition, so recursive submessages are possible
                result += '    struct %s *%s;' % (Globals.naming_style.struct_name(self.ctype), var_name)
            else:
                # Normal case, just a pointer to single item
                result += '    %s *%s;' % (type_name, var_name)
        elif self.allocation == 'CALLBACK':
            result += '    %s %s;' % (self.callback_datatype, var_name)
        else:
            if self.pbtype == 'MSG_W_CB' and self.rules in ['OPTIONAL', 'REPEATED']:
                result += '    pb_callback_t cb_' + var_name + ';\n'

            if self.rules == 'OPTIONAL':
                result += '    bool has_' + var_name + ';\n'
            elif self.rules == 'REPEATED':
                result += '    pb_size_t ' + var_name + '_count;\n'

            result += '    %s %s%s;' % (type_name, var_name, self.array_decl)

        leading_comment, trailing_comment = self.get_comments(leading_indent = True)
        if leading_comment: result = leading_comment + "\n" + result
        if trailing_comment: result = result + " " + trailing_comment

        return result

    def types(self):
        '''Return definitions for any special types this field might need.'''
        if self.pbtype == 'BYTES' and self.allocation == 'STATIC':
            result = 'typedef PB_BYTES_ARRAY_T(%d) %s;\n' % (self.max_size, self.ctype)
        else:
            result = ''
        return result

    def get_dependencies(self):
        '''Get list of type names used by this field.'''
        if self.allocation == 'STATIC':
            return [str(self.ctype)]
        elif self.allocation == 'POINTER' and self.rules == 'FIXARRAY':
            return [str(self.ctype)]
        else:
            return []

    def get_initializer(self, null_init, inner_init_only = False):
        '''Return literal expression for this field's default value.
        null_init: If True, initialize to a 0 value instead of default from .proto
        inner_init_only: If True, exclude initialization for any count/has fields
        '''

        inner_init = None
        if self.initializer is not None:
            inner_init = self.initializer
        elif self.pbtype in ['MESSAGE', 'MSG_W_CB']:
            if null_init:
                inner_init = Globals.naming_style.define_name('%s_init_zero' % self.ctype)
            else:
                inner_init =  Globals.naming_style.define_name('%s_init_default' % self.ctype)
        elif self.default is None or null_init:
            if self.pbtype == 'STRING':
                inner_init = '""'
            elif self.pbtype == 'BYTES':
                inner_init = '{0, {0}}'
            elif self.pbtype == 'FIXED_LENGTH_BYTES':
                inner_init = '{0}'
            elif self.pbtype in ('ENUM', 'UENUM'):
                inner_init = '_%s_MIN' % Globals.naming_style.define_name(self.ctype)
            else:
                inner_init = '0'
        else:
            if self.pbtype == 'STRING':
                data = codecs.escape_encode(self.default.encode('utf-8'))[0]
                inner_init = '"' + data.decode('ascii') + '"'
            elif self.pbtype == 'BYTES':
                data = codecs.escape_decode(self.default)[0]
                data = ["0x%02x" % c for c in bytearray(data)]
                if len(data) == 0:
                    inner_init = '{0, {0}}'
                else:
                    inner_init = '{%d, {%s}}' % (len(data), ','.join(data))
            elif self.pbtype == 'FIXED_LENGTH_BYTES':
                data = codecs.escape_decode(self.default)[0]
                data = ["0x%02x" % c for c in bytearray(data)]
                if len(data) == 0:
                    inner_init = '{0}'
                else:
                    inner_init = '{%s}' % ','.join(data)
            elif self.pbtype in ['FIXED32', 'UINT32']:
                inner_init = str(self.default) + 'u'
            elif self.pbtype in ['FIXED64', 'UINT64']:
                inner_init = str(self.default) + 'ull'
            elif self.pbtype in ['SFIXED64', 'INT64']:
                inner_init = str(self.default) + 'll'
            elif self.pbtype in ['FLOAT', 'DOUBLE']:
                inner_init = str(self.default)
                if 'inf' in inner_init:
                    inner_init = inner_init.replace('inf', 'INFINITY')
                elif 'nan' in inner_init:
                    inner_init = inner_init.replace('nan', 'NAN')
                elif (not '.' in inner_init) and self.pbtype == 'FLOAT':
                    inner_init += '.0f'
                elif self.pbtype == 'FLOAT':
                    inner_init += 'f'
            elif self.pbtype in ('ENUM', 'UENUM'):
                inner_init = Globals.naming_style.enum_entry(self.default)
            else:
                inner_init = str(self.default)

        if inner_init_only:
            return inner_init

        outer_init = None
        if self.allocation == 'STATIC':
            if self.rules == 'REPEATED':
                outer_init = '0, {' + ', '.join([inner_init] * self.max_count) + '}'
            elif self.rules == 'FIXARRAY':
                outer_init = '{' + ', '.join([inner_init] * self.max_count) + '}'
            elif self.rules == 'OPTIONAL':
                if null_init or not self.default_has:
                    outer_init = 'false, ' + inner_init
                else:
                    outer_init = 'true, ' + inner_init
            else:
                outer_init = inner_init
        elif self.allocation == 'POINTER':
            if self.rules == 'REPEATED':
                outer_init = '0, NULL'
            else:
                outer_init = 'NULL'
        elif self.allocation == 'CALLBACK':
            if self.pbtype == 'EXTENSION':
                outer_init = 'NULL'
            elif self.callback_datatype == 'pb_callback_t':
                outer_init = '{{NULL}, NULL}'
            elif self.initializer is not None:
                outer_init = inner_init
            elif self.callback_datatype.strip().endswith('*'):
                outer_init = 'NULL'
            else:
                outer_init = '{0}'

        if self.pbtype == 'MSG_W_CB' and self.rules in ['REPEATED', 'OPTIONAL']:
            outer_init = '{{NULL}, NULL}, ' + outer_init

        return outer_init

    def tags(self):
        '''Return the #define for the tag number of this field.'''
        identifier = Globals.naming_style.define_name('%s_%s_tag' % (self.struct_name, self.name))
        return '#define %-40s %d\n' % (identifier, self.tag)

    def fieldlist(self):
        '''Return the FIELDLIST macro entry for this field.
        Format is: X(a, ATYPE, HTYPE, LTYPE, field_name, tag)
        '''
        name = Globals.naming_style.var_name(self.name)

        if self.rules == "ONEOF":
          # For oneofs, make a tuple of the union name, union member name,
          # and the name inside the parent struct.
          if not self.anonymous:
            name = '(%s,%s,%s)' % (
                Globals.naming_style.var_name(self.union_name),
                Globals.naming_style.var_name(self.name),
                Globals.naming_style.var_name(self.union_name) + '.' +
                Globals.naming_style.var_name(self.name))
          else:
            name = '(%s,%s,%s)' % (
                Globals.naming_style.var_name(self.union_name),
                Globals.naming_style.var_name(self.name),
                Globals.naming_style.var_name(self.name))

        return '%s(%s, %-9s %-9s %-9s %-16s %3d)' % (self.macro_x_param,
                                                     self.macro_a_param,
                                                     self.allocation + ',',
                                                     self.rules + ',',
                                                     self.pbtype + ',',
                                                     name + ',',
                                                     self.tag)

    def data_size(self, dependencies):
        '''Return estimated size of this field in the C struct.
        This is used to try to automatically pick right descriptor size.
        If the estimate is wrong, it will result in compile time error and
        user having to specify descriptor_width option.
        '''
        if self.allocation == 'POINTER' or self.pbtype == 'EXTENSION':
            size = 8
            alignment = 8
        elif self.allocation == 'CALLBACK':
            size = 16
            alignment = 8
        elif self.pbtype in ['MESSAGE', 'MSG_W_CB']:
            alignment = 8
            if str(self.submsgname) in dependencies:
                other_dependencies = dict(x for x in dependencies.items() if x[0] != str(self.struct_name))
                size = dependencies[str(self.submsgname)].data_size(other_dependencies)
            else:
                size = 256 # Message is in other file, this is reasonable guess for most cases
                sys.stderr.write('Could not determine size for submessage %s, using default %d\n' % (self.submsgname, size))

            if self.pbtype == 'MSG_W_CB':
                size += 16
        elif self.pbtype in ['STRING', 'FIXED_LENGTH_BYTES']:
            size = self.max_size
            alignment = 4
        elif self.pbtype == 'BYTES':
            size = self.max_size + 4
            alignment = 4
        elif self.data_item_size is not None:
            size = self.data_item_size
            alignment = 4
            if self.data_item_size >= 8:
                alignment = 8
        else:
            raise Exception("Unhandled field type: %s" % self.pbtype)

        if self.rules in ['REPEATED', 'FIXARRAY'] and self.allocation == 'STATIC':
            size *= self.max_count

        if self.rules not in ('REQUIRED', 'SINGULAR'):
            size += 4

        if size % alignment != 0:
            # Estimate how much alignment requirements will increase the size.
            size += alignment - (size % alignment)

        return size

    def encoded_size(self, dependencies):
        '''Return the maximum size that this field can take when encoded,
        including the field tag. If the size cannot be determined, returns
        None.'''

        if not self.can_be_static:
            return None

        if self.pbtype in ['MESSAGE', 'MSG_W_CB']:
            encsize = None
            if str(self.submsgname) in dependencies:
                submsg = dependencies[str(self.submsgname)]
                other_dependencies = dict(x for x in dependencies.items() if x[0] != str(self.struct_name))
                encsize = submsg.encoded_size(other_dependencies)

                my_msg = dependencies.get(str(self.struct_name))
                external = (not my_msg or submsg.protofile != my_msg.protofile)

                if encsize and encsize.symbols and external:
                    # Couldn't fully resolve the size of a dependency from
                    # another file. Instead of including the symbols directly,
                    # just use the #define SubMessage_size from the header.
                    encsize = None

                if encsize is not None:
                    # Include submessage length prefix
                    encsize += varint_max_size(encsize.upperlimit())
                elif not external:
                    # The dependency is from the same file and size cannot be
                    # determined for it, thus we know it will not be possible
                    # in runtime either.
                    return None

            if encsize is None:
                # Submessage or its size cannot be found.
                # This can occur if submessage is defined in different
                # file, and it or its .options could not be found.
                # Instead of direct numeric value, reference the size that
                # has been #defined in the other file.
                encsize = EncodedSize(self.submsgname + 'size')

                # We will have to make a conservative assumption on the length
                # prefix size, though.
                encsize += 5

        elif self.pbtype in ['ENUM', 'UENUM']:
            if str(self.ctype) in dependencies:
                enumtype = dependencies[str(self.ctype)]
                encsize = enumtype.encoded_size()
            else:
                # Conservative assumption
                encsize = 10

        elif self.enc_size is None:
            raise RuntimeError("Could not determine encoded size for %s.%s"
                               % (self.struct_name, self.name))
        else:
            encsize = EncodedSize(self.enc_size)

        encsize += varint_max_size(self.tag << 3) # Tag + wire type

        if self.rules in ['REPEATED', 'FIXARRAY']:
            # Decoders must be always able to handle unpacked arrays.
            # Therefore we have to reserve space for it, even though
            # we emit packed arrays ourselves. For length of 1, packed
            # arrays are larger however so we need to add allowance
            # for the length byte.
            encsize *= self.max_count

            if self.max_count == 1:
                encsize += 1

        return encsize

    def has_callbacks(self):
        return self.allocation == 'CALLBACK'

    def requires_custom_field_callback(self):
        return self.allocation == 'CALLBACK' and self.callback_datatype != 'pb_callback_t'

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
        self.data_item_size = 0
        self.fixed_count = False
        self.callback_datatype = 'pb_extension_t*'
        self.initializer = None

    def requires_custom_field_callback(self):
        return False

    def __str__(self):
        return '    pb_extension_t *extensions;'

    def types(self):
        return ''

    def tags(self):
        return ''

    def encoded_size(self, dependencies):
        # We exclude extensions from the count, because they cannot be known
        # until runtime. Other option would be to return None here, but this
        # way the value remains useful if extensions are not used.
        return EncodedSize(0)

class ExtensionField(Field):
    def __init__(self, fullname, desc, field_options):
        self.fullname = fullname
        self.extendee_name = names_from_type_name(desc.extendee)
        Field.__init__(self, self.fullname + "extmsg", desc, field_options)

        if self.rules != 'OPTIONAL':
            self.skip = True
        else:
            self.skip = False
            self.rules = 'REQUIRED' # We don't really want the has_field for extensions
            # currently no support for comments for extension fields => provide (), {}
            self.msg = Message(self.fullname + "extmsg", None, field_options, (), {})
            self.msg.fields.append(self)

    def tags(self):
        '''Return the #define for the tag number of this field.'''
        identifier = Globals.naming_style.define_name('%s_tag' % (self.fullname))
        return '#define %-40s %d\n' % (identifier, self.tag)

    def extension_decl(self):
        '''Declaration of the extension type in the .pb.h file'''
        if self.skip:
            msg = '/* Extension field %s was skipped because only "optional"\n' % self.fullname
            msg +='   type of extension fields is currently supported. */\n'
            return msg

        return ('extern const pb_extension_type_t %s; /* field type: %s */\n' %
            (Globals.naming_style.var_name(self.fullname), str(self).strip()))

    def extension_def(self, dependencies):
        '''Definition of the extension type in the .pb.c file'''

        if self.skip:
            return ''

        result = "/* Definition for extension field %s */\n" % self.fullname
        result += str(self.msg)
        result += self.msg.fields_declaration(dependencies)
        result += 'pb_byte_t %s_default[] = {0x00};\n' % Globals.naming_style.var_name(self.msg.name)
        result += self.msg.fields_definition(dependencies)
        result += 'const pb_extension_type_t %s = {\n' % Globals.naming_style.var_name(self.fullname)
        result += '    NULL,\n'
        result += '    NULL,\n'
        result += '    &%s_msg\n' % Globals.naming_style.type_name(self.msg.name)
        result += '};\n'
        return result


# ---------------------------------------------------------------------------
#                   Generation of oneofs (unions)
# ---------------------------------------------------------------------------

class OneOf(Field):
    """
    Represents a protobuf oneof group, which becomes a C union.
    
    A oneof is a set of mutually exclusive fields - only one can be set at a time.
    In nanopb, this is implemented as a C union with a tag field indicating
    which member is active.
    
    The OneOf class extends Field because oneofs participate in the field
    list of a message and need to be processed similarly (sorting, encoding, etc.).
    
    Attributes:
        fields (list): List of Field objects that are members of this oneof
        anonymous (bool): If True, generate anonymous union (C11 feature)
        has_msg_cb (bool): True if any member uses message callback
    
    C Output Example:
        pb_size_t which_my_oneof;
        union {
            int32_t option_a;
            char option_b[32];
            SubMessage option_c;
        } my_oneof;
    
    Note:
        The 'which_' field uses the first field's tag value as the enum,
        allowing switch() statements on the oneof selection.
    """
    def __init__(self, struct_name, oneof_desc, oneof_options):
        self.struct_name = struct_name
        self.name = oneof_desc.name
        self.ctype = 'union'
        self.pbtype = 'oneof'
        self.fields = []
        self.allocation = 'ONEOF'
        self.default = None
        self.rules = 'ONEOF'
        self.anonymous = oneof_options.anonymous_oneof
        self.sort_by_tag = oneof_options.sort_by_tag
        self.has_msg_cb = False

    def add_field(self, field):
        field.union_name = self.name
        field.rules = 'ONEOF'
        field.anonymous = self.anonymous
        self.fields.append(field)

        if self.sort_by_tag:
            self.fields.sort()

        # Add submessage callback for callback fields inside oneofs.
        # This is done here because the Field() initializer doesn't know
        # whether the field will end up inside oneof or not.
        if field.submsg_callback_requested and field.allocation == 'CALLBACK':
            field.pbtype = 'MSG_W_CB'

        if field.pbtype == 'MSG_W_CB':
            self.has_msg_cb = True

        # Sort by the lowest tag number inside union
        self.tag = min([f.tag for f in self.fields])

    def __str__(self):
        result = ''
        if self.fields:
            if self.has_msg_cb:
                result += '    pb_callback_t cb_' + Globals.naming_style.var_name(self.name) + ';\n'

            result += '    pb_size_t which_' + Globals.naming_style.var_name(self.name) + ";\n"
            if self.anonymous:
                result += '    union {\n'
            else:
                result += '    union ' + Globals.naming_style.union_name(self.struct_name + self.name) + ' {\n'
            for f in self.fields:
                result += '    ' + str(f).replace('\n', '\n    ') + '\n'
            if self.anonymous:
                result += '    };'
            else:
                result += '    } ' + Globals.naming_style.var_name(self.name) + ';'
        return result

    def types(self):
        return ''.join([f.types() for f in self.fields])

    def get_dependencies(self):
        deps = []
        for f in self.fields:
            deps += f.get_dependencies()
        return deps

    def get_initializer(self, null_init):
        if self.has_msg_cb:
            return '{{NULL}, NULL}, 0, {' + self.fields[0].get_initializer(null_init) + '}'
        else:
            return '0, {' + self.fields[0].get_initializer(null_init) + '}'

    def tags(self):
        return ''.join([f.tags() for f in self.fields])

    def data_size(self, dependencies):
        return max(f.data_size(dependencies) for f in self.fields)

    def encoded_size(self, dependencies):
        '''Returns the size of the largest oneof field.'''
        largest = 0
        dynamic_sizes = {}
        for f in self.fields:
            size = EncodedSize(f.encoded_size(dependencies))
            if size is None or size.value is None:
                return None
            elif size.symbols:
                dynamic_sizes[f.tag] = size
            elif size.value > largest:
                largest = size.value

        if not dynamic_sizes:
            # Simple case, all sizes were known at generator time
            return EncodedSize(largest)

        if largest > 0:
            # Some sizes were known, some were not
            dynamic_sizes[0] = EncodedSize(largest)

        # Couldn't find size for submessage at generation time,
        # have to rely on macro resolution at compile time.
        if len(dynamic_sizes) == 1:
            # Only one symbol was needed
            return list(dynamic_sizes.values())[0]
        else:
            # Use sizeof(union{}) construct to find the maximum size of
            # submessages.
            union_name = "%s_%s_size_union" % (self.struct_name, self.name)
            union_def = 'union %s {%s};\n' % (union_name, ' '.join('char f%d[%s];' % (k, s) for k,s in dynamic_sizes.items()))
            required_defs = list(itertools.chain.from_iterable(s.required_defines for k,s in dynamic_sizes.items()))
            return EncodedSize(0, ['sizeof(union %s)' % union_name], [union_def], required_defs)

    def has_callbacks(self):
        return bool([f for f in self.fields if f.has_callbacks()])

    def requires_custom_field_callback(self):
        return bool([f for f in self.fields if f.requires_custom_field_callback()])

# ---------------------------------------------------------------------------
#                   Generation of messages (structures)
# ---------------------------------------------------------------------------


class Message(ProtoElement):
    """
    Represents a protobuf message, which becomes a C struct.
    
    The Message class is the central element in code generation. It manages:
    - Collection of fields (including nested oneofs)
    - Message-level options (packed struct, descriptor size, etc.)
    - Code generation for struct definition, field tables, and initializers
    
    A Message produces several C artifacts:
    1. **Struct typedef**: The C struct containing all field members
    2. **Field descriptor table**: pb_field_t array describing field encoding
    3. **Initializer macro**: Default value initializer for the struct
    4. **Size macros**: Encoded size calculations for buffer allocation
    
    Attributes:
        name (Names): Fully qualified message name
        fields (list): Field objects (including OneOf groups as single entries)
        oneofs (dict): Mapping from oneof index to OneOf objects
        desc: Original DescriptorProto (or None for synthesized messages)
        packed (bool): If True, use __attribute__((packed)) on struct
        descriptorsize (int): Size of descriptor (DS_AUTO, DS_1, DS_2, DS_4)
        message_validate_rules: Optional message-level validation rules
        callback_function (str): Name of custom callback function if needed
    
    Note:
        Messages can be nested, but the generator flattens them - each message
        gets its own top-level struct definition with a mangled name.
    """
    def __init__(self, names, desc, message_options, element_path, comments):
        """
        Initialize a Message from a DescriptorProto.
        
        Args:
            names: Names object for this message
            desc: DescriptorProto from protobuf descriptor (or None)
            message_options: NanoPBOptions for this message
            element_path: Tuple path for source comment lookup
            comments: Dict mapping paths to SourceCodeInfo.Location
        """
        super(Message, self).__init__(element_path, comments)
        self.name = names
        self.fields = []
        self.oneofs = {}
        self.desc = desc
        self.math_include_required = False
        self.packed = message_options.packed_struct
        self.descriptorsize = message_options.descriptorsize
        self.message_validate_rules = None  # Message-level validation rules

        if message_options.msgid:
            self.msgid = message_options.msgid

        if desc is not None:
            self.load_fields(desc, message_options)

        self.callback_function = message_options.callback_function
        if not message_options.HasField('callback_function'):
            # Automatically assign a per-message callback if any field has
            # a special callback_datatype.
            for field in self.fields:
                if field.requires_custom_field_callback():
                    self.callback_function = "%s_callback" % self.name
                    break

    def load_fields(self, desc, message_options):
        '''Load field list from DescriptorProto'''

        no_unions = []

        if hasattr(desc, 'oneof_decl'):
            for i, f in enumerate(desc.oneof_decl):
                oneof_options = get_nanopb_suboptions(desc, message_options, self.name + f.name)
                if oneof_options.no_unions:
                    no_unions.append(i) # No union, but add fields normally
                elif oneof_options.type == nanopb_pb2.FT_IGNORE:
                    pass # No union and skip fields also
                else:
                    oneof = OneOf(self.name, f, oneof_options)
                    self.oneofs[i] = oneof
        else:
            sys.stderr.write('Note: This Python protobuf library has no OneOf support\n')

        for index, f in enumerate(desc.field):
            field_options = get_nanopb_suboptions(f, message_options, self.name + f.name)

            if field_options.type == nanopb_pb2.FT_IGNORE:
                continue

            if field_options.discard_deprecated and f.options.deprecated:
                continue

            if field_options.descriptorsize > self.descriptorsize:
                self.descriptorsize = field_options.descriptorsize

            field = Field(self.name, f, field_options, self.element_path + (ProtoElement.FIELD, index), self.comments)
            # Parse validation rules if available
            if validate_pb2:
                try:
                    if f.options.HasExtension(validate_pb2.rules):
                        field.validate_rules = f.options.Extensions[validate_pb2.rules]
                    else:
                        field.validate_rules = None
                except (KeyError, AttributeError) as e:
                    # Extension not available or not properly registered
                    field.validate_rules = None
            else:
                field.validate_rules = None
            
            # Store the field descriptor for validation fallback parsing
            field.field_descriptor = f
            
            if hasattr(f, 'oneof_index') and f.HasField('oneof_index'):
                if hasattr(f, 'proto3_optional') and f.proto3_optional:
                    no_unions.append(f.oneof_index)

                if f.oneof_index in no_unions:
                    self.fields.append(field)
                elif f.oneof_index in self.oneofs:
                    self.oneofs[f.oneof_index].add_field(field)

                    if self.oneofs[f.oneof_index] not in self.fields:
                        self.fields.append(self.oneofs[f.oneof_index])
            else:
                self.fields.append(field)

            if field.math_include_required:
                self.math_include_required = True

        if len(desc.extension_range) > 0:
            field_options = get_nanopb_suboptions(desc, message_options, self.name + 'extensions')
            range_start = min([r.start for r in desc.extension_range])
            if field_options.type != nanopb_pb2.FT_IGNORE:
                self.fields.append(ExtensionRange(self.name, range_start, field_options))

        if message_options.sort_by_tag:
            self.fields.sort()

    def get_dependencies(self):
        '''Get list of type names that this structure refers to.'''
        deps = []
        for f in self.fields:
            deps += f.get_dependencies()
        return deps

    def __repr__(self):
        return 'Message(%s)' % self.name

    def __str__(self):
        leading_comment, trailing_comment = self.get_comments()

        result = ''
        if leading_comment:
            result = '%s\n' % leading_comment

        result += 'typedef struct %s {' % Globals.naming_style.struct_name(self.name)
        if trailing_comment:
            result += " " + trailing_comment

        result += '\n'

        if not self.fields:
            # Empty structs are not allowed in C standard.
            # Therefore add a dummy field if an empty message occurs.
            result += '    char dummy_field;'

        result += '\n'.join([str(f) for f in self.fields])

        if Globals.protoc_insertion_points:
            result += '\n/* @@protoc_insertion_point(struct:%s) */' % self.name

        result += '\n}'

        if self.packed:
            result += ' pb_packed'

        result += ' %s;' % Globals.naming_style.type_name(self.name)

        if self.packed:
            result = 'PB_PACKED_STRUCT_START\n' + result
            result += '\nPB_PACKED_STRUCT_END'

        return result + '\n'

    def types(self):
        return ''.join([f.types() for f in self.fields])

    def get_initializer(self, null_init):
        if not self.fields:
            return '{0}'

        parts = []
        for field in self.fields:
            parts.append(field.get_initializer(null_init))
        return '{' + ', '.join(parts) + '}'

    def count_required_fields(self):
        '''Returns number of required fields inside this message'''
        count = 0
        for f in self.fields:
            if not isinstance(f, OneOf):
                if f.rules == 'REQUIRED':
                    count += 1
        return count

    def all_fields(self):
        '''Iterate over all fields in this message, including nested OneOfs.'''
        for f in self.fields:
            if isinstance(f, OneOf):
                for f2 in f.fields:
                    yield f2
            else:
                yield f


    def field_for_tag(self, tag):
        '''Given a tag number, return the Field instance.'''
        for field in self.all_fields():
            if field.tag == tag:
                return field
        return None

    def count_all_fields(self):
        '''Count the total number of fields in this message.'''
        count = 0
        for f in self.fields:
            if isinstance(f, OneOf):
                count += len(f.fields)
            else:
                count += 1
        return count

    def fields_declaration(self, dependencies):
        '''Return X-macro declaration of all fields in this message.'''
        Field.macro_x_param = 'X'
        Field.macro_a_param = 'a'
        while any(field.name == Field.macro_x_param for field in self.all_fields()):
            Field.macro_x_param += '_'
        while any(field.name == Field.macro_a_param for field in self.all_fields()):
            Field.macro_a_param += '_'

        # Field descriptor array must be sorted by tag number, pb_common.c relies on it.
        sorted_fields = list(self.all_fields())
        sorted_fields.sort(key = lambda x: x.tag)

        result = '#define %s_FIELDLIST(%s, %s) \\\n' % (
            Globals.naming_style.define_name(self.name),
            Field.macro_x_param,
            Field.macro_a_param)
        result += ' \\\n'.join(x.fieldlist() for x in sorted_fields)
        result += '\n'

        has_callbacks = bool([f for f in self.fields if f.has_callbacks()])
        if has_callbacks:
            if self.callback_function != 'pb_default_field_callback':
                result += "extern bool %s(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field);\n" % self.callback_function
            result += "#define %s_CALLBACK %s\n" % (
                Globals.naming_style.define_name(self.name),
                self.callback_function)
        else:
            result += "#define %s_CALLBACK NULL\n" % Globals.naming_style.define_name(self.name)

        defval = self.default_value(dependencies)
        if defval:
            hexcoded = ''.join("\\x%02x" % ord(defval[i:i+1]) for i in range(len(defval)))
            result += '#define %s_DEFAULT (const pb_byte_t*)"%s\\x00"\n' % (
                Globals.naming_style.define_name(self.name),
                hexcoded)
        else:
            result += '#define %s_DEFAULT NULL\n' % Globals.naming_style.define_name(self.name)

        for field in sorted_fields:
            if field.pbtype in ['MESSAGE', 'MSG_W_CB']:
                if field.rules == 'ONEOF':
                    result += "#define %s_%s_%s_MSGTYPE %s\n" % (
                        Globals.naming_style.type_name(self.name),
                        Globals.naming_style.var_name(field.union_name),
                        Globals.naming_style.var_name(field.name),
                        Globals.naming_style.type_name(field.ctype)
                    )
                else:
                    result += "#define %s_%s_MSGTYPE %s\n" % (
                        Globals.naming_style.type_name(self.name),
                        Globals.naming_style.var_name(field.name),
                        Globals.naming_style.type_name(field.ctype)
                    )

        return result

    def enumtype_defines(self):
        '''Defines to allow user code to refer to enum type of a specific field'''
        result = ''
        for field in self.all_fields():
            if field.pbtype in ['ENUM', "UENUM"]:
                if field.rules == 'ONEOF':
                    result += "#define %s_%s_%s_ENUMTYPE %s\n" % (
                        Globals.naming_style.type_name(self.name),
                        Globals.naming_style.var_name(field.union_name),
                        Globals.naming_style.var_name(field.name),
                        Globals.naming_style.type_name(field.ctype)
                    )
                else:
                    result += "#define %s_%s_ENUMTYPE %s\n" % (
                        Globals.naming_style.type_name(self.name),
                        Globals.naming_style.var_name(field.name),
                        Globals.naming_style.type_name(field.ctype)
                    )

        return result

    def fields_declaration_cpp_lookup(self, local_defines):
        result = 'template <>\n'
        result += 'struct MessageDescriptor<%s> {\n' % (Globals.naming_style.type_name(self.name))
        result += '    static PB_INLINE_CONSTEXPR const pb_size_t fields_array_length = %d;\n' % (self.count_all_fields())

        size_define = "%s_size" % (Globals.naming_style.type_name(self.name))
        if size_define in local_defines:
            result += '/* The size define we are using may be defined conditionally guarded. */\n'
            result += '#if defined %s\n' % size_define
            result += '    static PB_INLINE_CONSTEXPR const pb_size_t size = %s;\n' % (size_define)
            result += '#endif\n'

        result += '    static PB_INLINE_CONSTEXPR const pb_msgdesc_t* fields() {\n'
        result += '        return &%s_msg;\n' % (Globals.naming_style.type_name(self.name))
        result += '    }\n'
        result += '    static PB_INLINE_CONSTEXPR bool has_msgid() {\n'
        result += '        return %s;\n' % ("true" if hasattr(self, "msgid") else "false", )
        result += '    }\n'
        result += '    static PB_INLINE_CONSTEXPR uint32_t msgid() {\n'
        result += '        return %d;\n' % (getattr(self, "msgid", 0), )
        result += '    }\n'
        result += '};'
        return result

    def fields_definition(self, dependencies):
        '''Return the field descriptor definition that goes in .pb.c file.'''
        width = self.required_descriptor_width(dependencies)
        if width == 1:
          width = 'AUTO'

        result = 'PB_BIND(%s, %s, %s)\n' % (
            Globals.naming_style.define_name(self.name),
            Globals.naming_style.type_name(self.name),
            width)
        return result

    def required_descriptor_width(self, dependencies):
        '''Estimate how many words are necessary for each field descriptor.'''
        if self.descriptorsize != nanopb_pb2.DS_AUTO:
            return int(self.descriptorsize)

        if not self.fields:
          return 1

        max_tag = max(field.tag for field in self.all_fields())
        max_offset = self.data_size(dependencies)
        max_arraysize = max((field.max_count or 0) for field in self.all_fields())
        max_datasize = max(field.data_size(dependencies) for field in self.all_fields())

        if max_arraysize > 0xFFFF:
            return 8
        elif (max_tag > 0x3FF or max_offset > 0xFFFF or
              max_arraysize > 0x0FFF or max_datasize > 0x0FFF):
            return 4
        elif max_tag > 0x3F or max_offset > 0xFF:
            return 2
        else:
            # NOTE: Macro logic in pb.h ensures that width 1 will
            # be raised to 2 automatically for string/submsg fields
            # and repeated fields. Thus only tag and offset need to
            # be checked.
            return 1

    def data_size(self, dependencies):
        '''Return approximate sizeof(struct) in the compiled code.'''
        return sum(f.data_size(dependencies) for f in self.fields)

    def encoded_size(self, dependencies):
        '''Return the maximum size that this message can take when encoded.
        If the size cannot be determined, returns None.
        '''
        size = EncodedSize(0)
        for field in self.fields:
            fsize = field.encoded_size(dependencies)
            if fsize is None:
                return None
            size += fsize

        return size

    def default_value(self, dependencies):
        '''Generate serialized protobuf message that contains the
        default values for optional fields.'''

        if not self.desc:
            return b''

        if self.desc.options.map_entry:
            return b''

        optional_only = copy.deepcopy(self.desc)

        # Remove fields without default values
        # The iteration is done in reverse order to avoid remove() messing up iteration.
        for field in reversed(list(optional_only.field)):
            field.ClearField(str('extendee'))
            parsed_field = self.field_for_tag(field.number)
            if parsed_field is None or parsed_field.allocation != 'STATIC':
                optional_only.field.remove(field)
            elif (field.label == FieldD.LABEL_REPEATED or
                  field.type == FieldD.TYPE_MESSAGE):
                optional_only.field.remove(field)
            elif hasattr(field, 'oneof_index') and field.HasField('oneof_index'):
                optional_only.field.remove(field)
            elif field.type == FieldD.TYPE_ENUM:
                # The partial descriptor doesn't include the enum type
                # so we fake it with int64.
                enumname = names_from_type_name(field.type_name)
                try:
                    enumtype = dependencies[str(enumname)]
                except KeyError:
                    raise Exception("Could not find enum type %s while generating default values for %s.\n" % (enumname, self.name)
                                    + "Try passing all source files to generator at once, or use -I option.")

                if not isinstance(enumtype, Enum):
                    raise Exception("Expected enum type as %s, got %s" % (enumname, repr(enumtype)))

                if field.HasField('default_value'):
                    defvals = [v for n,v in enumtype.values if n.parts[-1] == field.default_value]
                else:
                    # If no default is specified, the default is the first value.
                    defvals = [v for n,v in enumtype.values]
                if defvals and defvals[0] != 0:
                    field.type = FieldD.TYPE_INT64
                    field.default_value = str(defvals[0])
                    field.ClearField(str('type_name'))
                else:
                    optional_only.field.remove(field)
            elif not field.HasField('default_value'):
                optional_only.field.remove(field)

        if len(optional_only.field) == 0:
            return b''

        optional_only.ClearField(str('oneof_decl'))
        optional_only.ClearField(str('nested_type'))
        optional_only.ClearField(str('extension'))
        optional_only.ClearField(str('enum_type'))
        optional_only.name += str(id(self))

        desc = google.protobuf.descriptor.MakeDescriptor(optional_only)
        msg = GetMessageClass(desc)()

        for field in optional_only.field:
            if field.type == FieldD.TYPE_STRING:
                setattr(msg, field.name, field.default_value)
            elif field.type == FieldD.TYPE_BYTES:
                setattr(msg, field.name, codecs.escape_decode(field.default_value)[0])
            elif field.type in [FieldD.TYPE_FLOAT, FieldD.TYPE_DOUBLE]:
                setattr(msg, field.name, float(field.default_value))
            elif field.type == FieldD.TYPE_BOOL:
                setattr(msg, field.name, field.default_value == 'true')
            else:
                setattr(msg, field.name, int(field.default_value))

        return msg.SerializeToString()


# ---------------------------------------------------------------------------
#                   Generation of services (RPC methods)
# ---------------------------------------------------------------------------

class Service(ProtoElement):
    '''Represents a service definition with its RPC methods'''
    
    def __init__(self, names, desc, service_options, element_path, comments, file_proto):
        super(Service, self).__init__(element_path, comments)
        self.name = names
        self.desc = desc
        self.methods = []
        self.file_proto = file_proto
        self.service_options = service_options
        
        # Parse all RPC methods in this service
        for method in desc.method:
            self.methods.append({
                'name': method.name,
                'input_type': method.input_type,
                'output_type': method.output_type,
                'client_streaming': method.client_streaming,
                'server_streaming': method.server_streaming
            })
    
    def get_input_message_types(self):
        '''Get all unique input message types used in this service'''
        types = set()
        for method in self.methods:
            types.add(method['input_type'])
        return types
    
    def get_output_message_types(self):
        '''Get all unique output message types used in this service'''
        types = set()
        for method in self.methods:
            types.add(method['output_type'])
        return types
    
    def get_all_message_types(self):
        '''Get all message types (input and output) used in this service'''
        return self.get_input_message_types() | self.get_output_message_types()


def iterate_services(desc, flatten = False, names = Names()):
    '''Recursively find all services.
    For each, yield name, ServiceDescriptorProto.
    '''
    if hasattr(desc, 'service'):
        for service in desc.service:
            service_names = names + service.name if not flatten else Names(service.name)
            yield service_names, service


# ---------------------------------------------------------------------------
#                    Processing of entire .proto files
# ---------------------------------------------------------------------------

def iterate_messages(desc, flatten = False, names = Names(), comment_path = ()):
    '''Recursively find all messages. For each, yield name, DescriptorProto, comment_path.'''
    if hasattr(desc, 'message_type'):
        submsgs = desc.message_type
        comment_path += (ProtoElement.MESSAGE,)
    else:
        submsgs = desc.nested_type
        comment_path += (ProtoElement.NESTED_TYPE,)

    for idx, submsg in enumerate(submsgs):
        sub_names = names + submsg.name
        sub_path = comment_path + (idx,)
        if flatten:
            yield Names(submsg.name), submsg, sub_path
        else:
            yield sub_names, submsg, sub_path

        for x in iterate_messages(submsg, flatten, sub_names, sub_path):
            yield x

def iterate_extensions(desc, flatten = False, names = Names()):
    '''Recursively find all extensions.
    For each, yield name, FieldDescriptorProto.
    '''
    for extension in desc.extension:
        yield names, extension

    for subname, subdesc, comment_path in iterate_messages(desc, flatten, names):
        for extension in subdesc.extension:
            yield subname, extension

def check_recursive_dependencies(message, all_messages, root = None):
    '''Returns True if message has a recursive dependency on root (or itself if root is None).'''

    if not isinstance(all_messages, dict):
        all_messages = dict((str(m.name), m) for m in all_messages)

    if not root:
        root = message

    for dep in message.get_dependencies():
        if dep == str(root.name):
            return True
        elif dep in all_messages:
            if check_recursive_dependencies(all_messages[dep], all_messages, root):
                return True

    return False

def sort_dependencies(messages):
    '''Sort a list of Messages based on dependencies.'''

    # Construct first level list of dependencies
    dependencies = {}
    for message in messages:
        dependencies[str(message.name)] = set(message.get_dependencies())

    # Emit messages after all their dependencies have been processed
    remaining = list(messages)
    remainset = set(str(m.name) for m in remaining)
    while remaining:
        for candidate in remaining:
            if not remainset.intersection(dependencies[str(candidate.name)]):
                remaining.remove(candidate)
                remainset.remove(str(candidate.name))
                yield candidate
                break
        else:
            sys.stderr.write("Circular dependency in messages: " + ', '.join(remainset) + " (consider changing to FT_POINTER or FT_CALLBACK)\n")
            candidate = remaining.pop(0)
            remainset.remove(str(candidate.name))
            yield candidate

def make_identifier(headername):
    '''Make #ifndef identifier that contains uppercase A-Z and digits 0-9'''
    result = ""
    for c in headername.upper():
        if c.isalnum():
            result += c
        else:
            result += '_'
    return result

class MangleNames:
    """
    Manages type name transformations based on the mangle_names option.
    
    Protobuf uses fully qualified names with package prefixes, which can
    result in long C identifiers. MangleNames provides several strategies
    to shorten or transform these names:
    
    - M_NONE (0): No mangling, use full package.Message_Field names
    - M_STRIP_PACKAGE (1): Remove the package prefix entirely
    - M_FLATTEN (2): Use only the final name component (loses hierarchy)
    - M_PACKAGE_INITIALS (3): Replace package with initials (com.example -> ce)
    
    The class also supports the `package` file option to specify a custom
    replacement for the package prefix.
    
    Attributes:
        mangle_names (int): The mangling mode (M_NONE, M_STRIP_PACKAGE, etc.)
        flatten (bool): True if using M_FLATTEN mode
        strip_prefix (str): Package prefix to remove (e.g., ".com.example")
        replacement_prefix (str): Prefix to add after stripping
        name_mapping (dict): Maps original names to mangled names
        reverse_name_mapping (dict): Maps mangled names back to original
        base_name (Names): Base name for constructing new names
    
    Example:
        With package "com.example" and M_STRIP_PACKAGE:
            com.example.MyMessage -> MyMessage
        With M_PACKAGE_INITIALS:
            com.example.MyMessage -> ce_MyMessage
    """
    def __init__(self, fdesc, file_options):
        self.file_options = file_options
        self.mangle_names = file_options.mangle_names
        self.flatten = (self.mangle_names == nanopb_pb2.M_FLATTEN)
        self.strip_prefix = None
        self.replacement_prefix = None
        self.name_mapping = {}
        self.reverse_name_mapping = {}
        self.canonical_base = Names(fdesc.package.split('.'))

        if self.mangle_names == nanopb_pb2.M_STRIP_PACKAGE:
            self.strip_prefix = "." + fdesc.package
        elif self.mangle_names == nanopb_pb2.M_PACKAGE_INITIALS:
            self.strip_prefix = "." + fdesc.package
            self.replacement_prefix = ""
            for part in fdesc.package.split("."):
                self.replacement_prefix += part[0]
        elif file_options.package:
            self.strip_prefix = "." + fdesc.package
            self.replacement_prefix = file_options.package

        if self.strip_prefix == '.':
            self.strip_prefix = ''

        if self.replacement_prefix is not None:
            self.base_name = Names(self.replacement_prefix.split('.'))
        elif fdesc.package:
            self.base_name = Names(fdesc.package.split('.'))
        else:
            self.base_name = Names()

    def create_name(self, names):
        '''Create name for a new message / enum.
        Argument can be either string or Names instance.
        '''
        if str(names) not in self.name_mapping:
            if self.mangle_names in (nanopb_pb2.M_NONE, nanopb_pb2.M_PACKAGE_INITIALS):
                new_name = self.base_name + names
            elif self.mangle_names == nanopb_pb2.M_STRIP_PACKAGE:
                new_name = Names(names)
            elif isinstance(names, Names):
                new_name = Names(names.parts[-1])
            else:
                new_name = Names(names)

            if str(new_name) in self.reverse_name_mapping:
                sys.stderr.write("Warning: Duplicate name with mangle_names=%s: %s and %s map to %s\n" %
                    (self.mangle_names, self.reverse_name_mapping[str(new_name)], names, new_name))

            self.name_mapping[str(names)] = new_name
            self.reverse_name_mapping[str(new_name)] = self.canonical_base + names

            styled_name = Globals.naming_style.type_name(new_name)
            unmangled_styled_name = Globals.naming_style.type_name(self.canonical_base + names)

            if styled_name != unmangled_styled_name:
                # The styled name is mangled and needs extra mapping from unmangled to mangled. We just need to figure out whether
                # it requires one or two extra mappings to get from the unmangled to the mangled name, depending on how they differ.
                # This is required because enum dependencies are looked up from the reverse_name_mapping using names_from_type_name.

                # The type name (new_name) doesn't match either of the styled names, so we'll have to add an extra mapping to it.
                if str(new_name) != unmangled_styled_name and str(new_name) != styled_name:
                    self.reverse_name_mapping[unmangled_styled_name] = new_name

                # We need to be careful not to redefine the type name (new_name), use unmangled canonical name in this case.
                if styled_name == str(new_name):
                    self.reverse_name_mapping[str(self.canonical_base + names)] = unmangled_styled_name
                else:
                    self.reverse_name_mapping[styled_name] = unmangled_styled_name


        return self.name_mapping[str(names)]

    def mangle_field_typename(self, typename):
        '''Mangle type name for a submessage / enum crossreference.
        Argument is a string.
        '''
        if self.mangle_names == nanopb_pb2.M_FLATTEN:
            return "." + typename.split(".")[-1]

        canonical_mangled_typename = str(Names(typename.strip(".").split(".")))
        if not canonical_mangled_typename.startswith(str(self.canonical_base) + "_") and self.canonical_base != Names():
            return typename

        if self.strip_prefix is not None and typename.startswith(self.strip_prefix):
            if self.replacement_prefix is not None:
                return "." + self.replacement_prefix + typename[len(self.strip_prefix):]
            else:
                return typename[len(self.strip_prefix):]

        if self.file_options.package:
            return "." + self.replacement_prefix + typename

        return typename

    def unmangle(self, names):
        return self.reverse_name_mapping.get(str(names), names)

class ProtoFile:
    """
    Top-level representation of a .proto file for code generation.
    
    ProtoFile is the main orchestrator for code generation. It parses a
    FileDescriptorProto and builds the complete intermediate representation
    (messages, enums, extensions), then provides methods to generate the
    final C header and source files.
    
    The class handles:
    - Parsing and organizing all proto elements
    - Tracking dependencies between files
    - Managing name mangling and package handling
    - Generating the complete .pb.h and .pb.c file contents
    - Generating validation files when requested
    
    Attributes:
        fdesc: The FileDescriptorProto being processed
        file_options: NanoPBOptions for the file
        enums (list): All Enum objects in the file
        messages (list): All Message objects in the file
        extensions (list): All ExtensionField objects in the file
        dependencies (dict): Maps file names to ProtoFile objects
        manglenames (MangleNames): Handles name mangling configuration
        validate_enabled (bool): Whether validation code should be generated
    
    Key Methods:
        generate_header(): Yields lines for the .pb.h file
        generate_source(): Yields lines for the .pb.c file
        generate_validate_header(): Yields lines for the _validate.h file
        generate_validate_source(): Yields lines for the _validate.c file
    """
    def __init__(self, fdesc, file_options):
        """
        Parse a FileDescriptorProto and build the internal representation.
        
        Args:
            fdesc: FileDescriptorProto from protobuf descriptor
            file_options: NanoPBOptions for this file
        """
        self.fdesc = fdesc
        self.file_options = file_options
        self.dependencies = {}
        self.math_include_required = False
        self.validate_enabled = False  # Whether validation is enabled for this file
        
        
        self.parse()
        self.discard_unused_automatic_types()
        for message in self.messages:
            if message.math_include_required:
                self.math_include_required = True
                break

        # Some of types used in this file probably come from the file itself.
        # Thus it has implicit dependency on itself.
        self.add_dependency(self)

    def parse(self):
        self.enums = []
        self.messages = []
        self.extensions = []
        self.manglenames = MangleNames(self.fdesc, self.file_options)

        # process source code comment locations
        # ignores any locations that do not contain any comment information
        self.comment_locations = {
            tuple(location.path): location
            for location in self.fdesc.source_code_info.location
            if location.leading_comments or location.leading_detached_comments or location.trailing_comments
        }

        for index, enum in enumerate(self.fdesc.enum_type):
            name = self.manglenames.create_name(enum.name)
            enum_options = get_nanopb_suboptions(enum, self.file_options, name)
            enum_path = (ProtoElement.ENUM, index)
            self.enums.append(Enum(name, enum, enum_options, enum_path, self.comment_locations))

        for names, message, comment_path in iterate_messages(self.fdesc, self.manglenames.flatten):
            name = self.manglenames.create_name(names)
            message_options = get_nanopb_suboptions(message, self.file_options, name)

            if message_options.skip_message:
                continue

            if message_options.discard_deprecated and message.options.deprecated:
                continue

            # Apply any configured typename mangling options
            message = copy.deepcopy(message)
            for field in message.field:
                if field.type in (FieldD.TYPE_MESSAGE, FieldD.TYPE_ENUM):
                    field.type_name = self.manglenames.mangle_field_typename(field.type_name)

            # Check for circular dependencies
            msgobject = Message(name, message, message_options, comment_path, self.comment_locations)
            if check_recursive_dependencies(msgobject, self.messages):
                message_options.type = message_options.fallback_type
                sys.stderr.write('Breaking circular dependency at message %s by converting to %s\n'
                                 % (msgobject.name, nanopb_pb2.FieldType.Name(message_options.type)))
                msgobject = Message(name, message, message_options, comment_path, self.comment_locations)
            
            
            self.messages.append(msgobject)

            # Process any nested enums
            for index, enum in enumerate(message.enum_type):
                name = self.manglenames.create_name(names + enum.name)
                enum_options = get_nanopb_suboptions(enum, message_options, name)
                enum_path = comment_path + (ProtoElement.NESTED_ENUM, index)
                self.enums.append(Enum(name, enum, enum_options, enum_path, self.comment_locations))

        for names, extension in iterate_extensions(self.fdesc, self.manglenames.flatten):
            name = self.manglenames.create_name(names + extension.name)
            field_options = get_nanopb_suboptions(extension, self.file_options, name)

            extension = copy.deepcopy(extension)
            if extension.type in (FieldD.TYPE_MESSAGE, FieldD.TYPE_ENUM):
                extension.type_name = self.manglenames.mangle_field_typename(extension.type_name)

            if field_options.type != nanopb_pb2.FT_IGNORE:
                self.extensions.append(ExtensionField(name, extension, field_options))
        
        # Parse services
        self.services = []
        for names, service in iterate_services(self.fdesc, self.manglenames.flatten):
            name = self.manglenames.create_name(names)
            # SERVICE constant = 6 (from descriptor.proto)
            service_path = (6, len(self.services))
            # Services don't have nanopb options, so we pass an empty options object
            empty_options = nanopb_pb2.NanoPBOptions()
            self.services.append(Service(name, service, empty_options, service_path, self.comment_locations, self.fdesc))

    def discard_unused_automatic_types(self):
        '''Discard unused types that are automatically generated by protoc if they are not actually
        needed. Currently this applies to map< > types when the field is ignored by options.
        '''

        if not self.file_options.discard_unused_automatic_types:
            return

        map_entries = {}
        types_used = set()
        for msg in self.messages:
            if msg.desc.options.map_entry:
                map_entries[str(msg.name)] = msg

            for field in msg.all_fields():
                if field.pbtype == 'MESSAGE':
                    types_used.add(str(field.submsgname))

        for name, msg in map_entries.items():
            if name not in types_used:
                self.messages.remove(msg)

    def add_dependency(self, other):
        for enum in other.enums:
            self.dependencies[str(enum.names)] = enum
            self.dependencies[str(other.manglenames.unmangle(enum.names))] = enum
            enum.protofile = other

        for msg in other.messages:
            canonical_mangled_typename = str(other.manglenames.unmangle(msg.name))
            self.dependencies[str(msg.name)] = msg
            self.dependencies[canonical_mangled_typename] = msg
            msg.protofile = other

            # Fix references to submessages with different mangling rules
            for message in self.messages:
                for field in message.all_fields():
                    if field.ctype == canonical_mangled_typename:
                        field.ctype = msg.name

        # Fix field default values where enum short names are used.
        for enum in other.enums:
            if not enum.options.long_names:
                for message in self.messages:
                    for field in message.all_fields():
                        if field.default in enum.value_longnames:
                            idx = enum.value_longnames.index(field.default)
                            field.default = enum.values[idx][0]

        # Fix field data types where enums have negative values.
        for enum in other.enums:
            if not enum.has_negative():
                for message in self.messages:
                    for field in message.all_fields():
                        if field.pbtype == 'ENUM' and field.ctype == enum.names:
                            field.pbtype = 'UENUM'

    def generate_header(self, includes, headername, options):
        '''Generate content for a header file.
        Generates strings, which should be concatenated and stored to file.
        '''

        yield '/* Automatically generated nanopb header */\n'
        if options.notimestamp:
            yield '/* Generated by %s */\n\n' % (nanopb_version)
        else:
            yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())

        if self.fdesc.package:
            symbol = make_identifier(self.fdesc.package + '_' + headername)
        else:
            symbol = make_identifier(headername)
        yield '#ifndef PB_%s_INCLUDED\n' % symbol
        yield '#define PB_%s_INCLUDED\n' % symbol
        if self.math_include_required:
            yield '#include <math.h>\n'
        try:
            yield options.libformat % ('pb.h')
        except TypeError:
            # no %s specified - use whatever was passed in as options.libformat
            yield options.libformat
        yield '\n'

        for incfile in self.file_options.include:
            # allow including system headers
            if (incfile.startswith('<')):
                yield '#include %s\n' % incfile
            else:
                yield options.genformat % incfile
                yield '\n'

        for incfile in includes:
            noext = os.path.splitext(incfile)[0]
            yield options.genformat % (noext + options.extension + options.header_extension)
            yield '\n'

        if Globals.protoc_insertion_points:
            yield '/* @@protoc_insertion_point(includes) */\n'

        yield '\n'

        yield '#if PB_PROTO_HEADER_VERSION != 40\n'
        yield '#error Regenerate this file with the current version of nanopb generator.\n'
        yield '#endif\n'
        yield '\n'

        if self.enums:
            yield '/* Enum definitions */\n'
            for enum in self.enums:
                yield str(enum) + '\n\n'

        if self.messages:
            yield '/* Struct definitions */\n'
            for msg in sort_dependencies(self.messages):
                yield msg.types()
                yield str(msg) + '\n'
            yield '\n'

        if self.extensions:
            yield '/* Extensions */\n'
            for extension in self.extensions:
                yield extension.extension_decl()
            yield '\n'

        yield '#ifdef __cplusplus\n'
        yield 'extern "C" {\n'
        yield '#endif\n\n'

        if self.enums:
                yield '/* Helper constants for enums */\n'
                for enum in self.enums:
                    yield enum.auxiliary_defines() + '\n'

                for msg in self.messages:
                    yield msg.enumtype_defines() + '\n'
                yield '\n'

        if self.messages:
            yield '/* Initializer values for message structs */\n'
            for msg in self.messages:
                identifier = Globals.naming_style.define_name('%s_init_default' % msg.name)
                yield '#define %-40s %s\n' % (identifier, msg.get_initializer(False))
                unmangledName = self.manglenames.unmangle(msg.name)
                if unmangledName:
                    unmangledIdentifier = Globals.naming_style.define_name('%s_init_default' % unmangledName)
                    self.manglenames.reverse_name_mapping[identifier] = unmangledIdentifier
            for msg in self.messages:
                identifier = Globals.naming_style.define_name('%s_init_zero' % msg.name)
                yield '#define %-40s %s\n' % (identifier, msg.get_initializer(True))
                unmangledName = self.manglenames.unmangle(msg.name)
                if unmangledName:
                    unmangledIdentifier = Globals.naming_style.define_name('%s_init_zero' % unmangledName)
                    self.manglenames.reverse_name_mapping[identifier] = unmangledIdentifier
            yield '\n'

            yield '/* Field tags (for use in manual encoding/decoding) */\n'
            for msg in sort_dependencies(self.messages):
                for field in msg.fields:
                    yield field.tags()
            for extension in self.extensions:
                yield extension.tags()
            yield '\n'

            yield '/* Struct field encoding specification for nanopb */\n'
            for msg in self.messages:
                yield msg.fields_declaration(self.dependencies) + '\n'
            for msg in self.messages:
                yield 'extern const pb_msgdesc_t %s_msg;\n' % Globals.naming_style.type_name(msg.name)
            yield '\n'

            yield '/* Defines for backwards compatibility with code written before nanopb-0.4.0 */\n'
            for msg in self.messages:
              yield '#define %s &%s_msg\n' % (
                Globals.naming_style.define_name('%s_fields' % msg.name),
                Globals.naming_style.type_name(msg.name))
            yield '\n'

            yield '/* Maximum encoded size of messages (where known) */\n'
            messagesizes = []
            for msg in self.messages:
                identifier = '%s_size' % msg.name
                messagesizes.append((identifier, msg.encoded_size(self.dependencies)))

            # If we require a symbol from another file, put a preprocessor if statement
            # around it to prevent compilation errors if the symbol is not actually available.
            local_defines = [identifier for identifier, msize in messagesizes if msize is not None]

            # emit size_unions, if any
            oneof_sizes = []
            for msg in self.messages:
                for f in msg.fields:
                    if isinstance(f, OneOf):
                        msize = f.encoded_size(self.dependencies)
                        if msize is not None:
                            oneof_sizes.append(msize)
            for msize in oneof_sizes:
                guard = msize.get_cpp_guard(local_defines)
                if guard:
                    yield guard
                yield msize.get_declarations()
                if guard:
                    yield '#endif\n'

            guards = {}
            # Provide a #define of the maximum message size, which faciliates setting the size of static arrays to be the largest possible encoded message size
            max_messagesize = max(messagesizes, key=lambda messagesize: messagesize[1].value if messagesize[1] else 0)
            for identifier, msize in messagesizes:
                if msize is not None:
                    cpp_guard = msize.get_cpp_guard(local_defines)
                    if cpp_guard not in guards:
                        guards[cpp_guard] = set()
                    guards[cpp_guard].add('#define %-40s %s' % (
                        Globals.naming_style.define_name(identifier), msize))

                    if identifier == max_messagesize[0]:
                        guards[cpp_guard].add('#define %-40s %s' % (
                            Globals.naming_style.define_name(symbol + "_MAX_SIZE"), Globals.naming_style.define_name(identifier)))

                else:
                    yield '/* %s depends on runtime parameters */\n' % identifier
            for guard, values in guards.items():
                if guard:
                    yield guard
                for v in sorted(values):
                    yield v
                    yield '\n'
                if guard:
                    yield '#endif\n'
            yield '\n'

            if [msg for msg in self.messages if hasattr(msg,'msgid')]:
              yield '/* Message IDs (where set with "msgid" option) */\n'
              for msg in self.messages:
                  if hasattr(msg,'msgid'):
                      yield '#define PB_MSG_%d %s\n' % (msg.msgid, msg.name)
              yield '\n'

              symbol = make_identifier(headername.split('.')[0])
              yield '#define %s_MESSAGES \\\n' % symbol

              for msg in self.messages:
                  m = "-1"
                  msize = msg.encoded_size(self.dependencies)
                  if msize is not None:
                      m = msize
                  if hasattr(msg,'msgid'):
                      yield '\tPB_MSG(%d,%s,%s) \\\n' % (msg.msgid, m, msg.name)
              yield '\n'

              for msg in self.messages:
                  if hasattr(msg,'msgid'):
                      yield '#define %s_msgid %d\n' % (msg.name, msg.msgid)
              yield '\n'

        # Check if there is any name mangling active
        pairs = [x for x in self.manglenames.reverse_name_mapping.items() if str(x[0]) != str(x[1])]
        if pairs:
            yield '/* Mapping from canonical names (mangle_names or overridden package name) */\n'
            for shortname, longname in pairs:
                yield '#define %s %s\n' % (longname, shortname)
            yield '\n'

        # Generate callback context types if validation is enabled and messages have callback fields
        if (self.validate_enabled or options.validate):
            messages_with_callbacks = []
            for msg in self.messages:
                has_callback_fields = any(f.allocation == 'CALLBACK' for f in msg.fields if not isinstance(f, OneOf))
                if has_callback_fields:
                    messages_with_callbacks.append(msg)
            
            if messages_with_callbacks:
                yield '/* Include validation header for pb_violations_t */\n'
                yield '#include <pb_validate.h>\n'
                yield '\n'
                yield '/* Callback context types for validation */\n'
                for msg in messages_with_callbacks:
                    msg_type_name = Globals.naming_style.type_name(msg.name)
                    yield '/* Callback context structure for %s */\n' % msg_type_name
                    yield '/* Stores decoded callback field data for validation by pb_validate_%s() */\n' % msg_type_name
                    yield 'typedef struct {\n'
                    yield '    pb_violations_t *violations;\n'
                    yield '    const char *field_path;\n'
                    
                    # Add storage for each callback field
                    for field in msg.fields:
                        if isinstance(field, OneOf):
                            continue
                        if field.allocation == 'CALLBACK':
                            field_var_name = Globals.naming_style.var_name(field.name)
                            if field.pbtype in ['STRING', 'BYTES']:
                                # Store data + length for string/bytes content-based validation
                                # Maximum buffer size for callback string storage
                                max_callback_string_size = 256  # Reasonable limit for validation
                                yield '    /* Decoded data for callback field: %s */\n' % field.name
                                yield '    char %s_data[%d];\n' % (field_var_name, max_callback_string_size)
                                yield '    size_t %s_length;\n' % field_var_name
                                yield '    bool %s_decoded;\n' % field_var_name
                            elif field.pbtype == 'MESSAGE':
                                # For submessages, we can validate immediately but set a flag
                                yield '    /* Validation status for callback field: %s */\n' % field.name
                                yield '    bool %s_validated;\n' % field_var_name
                    
                    yield '} %s_callback_ctx_t;\n\n' % msg_type_name
                yield '\n'

        yield '#ifdef __cplusplus\n'
        yield '} /* extern "C" */\n'
        yield '#endif\n'

        if options.cpp_descriptors:
            yield '\n'
            yield '#ifdef __cplusplus\n'
            yield '/* Message descriptors for nanopb */\n'
            yield 'namespace nanopb {\n'
            for msg in self.messages:
                yield msg.fields_declaration_cpp_lookup(local_defines) + '\n'
            yield '}  // namespace nanopb\n'
            yield '\n'
            yield '#endif  /* __cplusplus */\n'
            yield '\n'

        # Generate packet filter functions if services exist or an envelope pattern is detected
        envelope_mode = getattr(options, 'envelope_mode', 'oneof')
        envelope_name = getattr(options, 'envelope_name', None)
        root_message_name = getattr(options, 'root_message', None)
        has_envelope = False
        if envelope_mode == 'any':
            has_envelope = self.detect_any_envelope_pattern(envelope_name) is not None
        else:
            has_envelope = self.detect_envelope_pattern(envelope_name) is not None
        
        # Packet filter declarations are only generated when --validate is explicitly enabled.
        # They are generated when: services exist, envelope pattern is detected, OR root_message is specified.
        if options.validate and (self.services or has_envelope or root_message_name):
            yield '\n'
            yield '#ifdef __cplusplus\n'
            yield 'extern "C" {\n'
            yield '#endif\n\n'

            for line in self.generate_service_filter_declarations(options):
                yield line
            
            yield '\n#ifdef __cplusplus\n'
            yield '} /* extern "C" */\n'
            yield '#endif\n'

        if Globals.protoc_insertion_points:
            yield '/* @@protoc_insertion_point(eof) */\n'

        # End of header
        yield '\n#endif\n'
    
    def generate_validate_header(self, headerbasename, options):
        '''Generate validation header file content.'''
        if not (self.validate_enabled or options.validate):
            return
        
        if not nanopb_validator:
            return
        
        validator_gen = self._build_validator_generator(options)
        
        # Generate header content
        for line in validator_gen.generate_header():
            yield line
    
    def generate_validate_source(self, headerbasename, options):
        '''Generate validation source file content.'''
        if not (self.validate_enabled or options.validate):
            return
        
        if not nanopb_validator:
            return
        
        validator_gen = self._build_validator_generator(options)
        
        # Generate source content
        for line in validator_gen.generate_source():
            yield line

    def _build_validator_generator(self, options):
        """Create and populate a ValidatorGenerator consistently for header/source generation."""
        bypass = getattr(options, 'bypass', False)
        validator_gen = nanopb_validator.ValidatorGenerator(self, bypass=bypass)
        # Add validators for all messages
        for msg in self.messages:
            if hasattr(msg, 'fields'):
                validator_gen.add_message_validator(msg, msg.message_validate_rules)
        # When --validate flag is used, always generate validation files even if no rules are found
        if options.validate and not validator_gen.validators:
            for msg in self.messages:
                if hasattr(msg, 'fields'):
                    validator_gen.force_add_message_validator(msg)
        return validator_gen

    def generate_source(self, headername, options):
        '''Generate content for a source file.'''

        yield '/* Automatically generated nanopb constant definitions */\n'
        if options.notimestamp:
            yield '/* Generated by %s */\n\n' % (nanopb_version)
        else:
            yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
        yield options.genformat % (headername)
        yield '\n'

        if Globals.protoc_insertion_points:
            yield '/* @@protoc_insertion_point(includes) */\n'

        yield '#if PB_PROTO_HEADER_VERSION != 40\n'
        yield '#error Regenerate this file with the current version of nanopb generator.\n'
        yield '#endif\n'
        yield '\n'

        # Check if any messages exceed the 64 kB limit of 16-bit pb_size_t
        exceeds_64kB = []
        for msg in self.messages:
            size = msg.data_size(self.dependencies)
            if size >= 65536:
                exceeds_64kB.append(str(msg.name))

        if exceeds_64kB:
            yield '\n/* The following messages exceed 64kB in size: ' + ', '.join(exceeds_64kB) + ' */\n'
            yield '\n/* The PB_FIELD_32BIT compilation option must be defined to support messages that exceed 64 kB in size. */\n'
            yield '#ifndef PB_FIELD_32BIT\n'
            yield '#error Enable PB_FIELD_32BIT to support messages exceeding 64kB in size: ' + ', '.join(exceeds_64kB) + '\n'
            yield '#endif\n'

        # Generate the message field definitions (PB_BIND() call)
        for msg in self.messages:
            yield msg.fields_definition(self.dependencies) + '\n\n'

        # Generate pb_extension_type_t definitions if extensions are used in proto file
        for ext in self.extensions:
            yield ext.extension_def(self.dependencies) + '\n'

        # Generate enum_name function if enum_to_string option is defined
        for enum in self.enums:
            yield enum.enum_to_string_definition() + '\n'

        # Generate enum_valid function if enum_valid option is defined
        for enum in self.enums:
            yield enum.enum_validate() + '\n'

        # Add checks for numeric limits
        if self.messages:
            largest_msg = max(self.messages, key = lambda m: m.count_required_fields())
            largest_count = largest_msg.count_required_fields()
            if largest_count > 64:
                yield '\n/* Check that missing required fields will be properly detected */\n'
                yield '#if PB_MAX_REQUIRED_FIELDS < %d\n' % largest_count
                yield '#error Properly detecting missing required fields in %s requires \\\n' % largest_msg.name
                yield '       setting PB_MAX_REQUIRED_FIELDS to %d or more.\n' % largest_count
                yield '#endif\n'

        # Add check for sizeof(double)
        has_double = False
        for msg in self.messages:
            for field in msg.all_fields():
                if field.ctype == 'double':
                    has_double = True

        if has_double:
            yield '\n'
            yield '#ifndef PB_CONVERT_DOUBLE_FLOAT\n'
            yield '/* On some platforms (such as AVR), double is really float.\n'
            yield ' * To be able to encode/decode double on these platforms, you need.\n'
            yield ' * to define PB_CONVERT_DOUBLE_FLOAT in pb.h or compiler command line.\n'
            yield ' */\n'
            yield 'PB_STATIC_ASSERT(sizeof(double) == 8, DOUBLE_MUST_BE_8_BYTES)\n'
            yield '#endif\n'

        yield '\n'

        if Globals.protoc_insertion_points:
            yield '/* @@protoc_insertion_point(eof) */\n'
        
        # Generate packet filter function implementations
        envelope_mode = getattr(options, 'envelope_mode', 'oneof')
        envelope_name = getattr(options, 'envelope_name', None)
        root_message_name = getattr(options, 'root_message', None)
        has_envelope = False
        if envelope_mode == 'any':
            has_envelope = self.detect_any_envelope_pattern(envelope_name) is not None
        else:
            has_envelope = self.detect_envelope_pattern(envelope_name) is not None
        
        # Packet filter implementations are only generated when --validate flag is provided.
        # They are generated when: services exist, envelope pattern is detected, OR root_message is specified.
        if options.validate and (self.services or has_envelope or root_message_name):
            yield '\n'
            yield options.libformat % ('pb_encode.h')
            yield '\n'
            yield options.libformat % ('pb_decode.h')
            yield '\n\n'
            for line in self.generate_service_filter_implementations(options):
                yield line
    
    def generate_service_filter_declarations(self, options):
        '''Generate function declarations for UDP and TCP packet filters, and an optional Envelope opcode enum alias.'''
        # If an Envelope pattern is detected, generate a CAPS enum alias that maps to the original opcode enum.
        envelope_info = self.detect_envelope_pattern()
        if envelope_info:
            envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info

            # Only generate the enum alias if we have an opcode enum (not for oneof-only patterns)
            if opcode_enum:
                # Type name in ALL CAPS: <ENVELOPE_NAME>_OPCODE
                alias_type = (str(envelope_msg.name) + '_OPCODE').replace('.', '_').replace('-', '_').upper()
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
    
    def find_message_by_name(self, message_name):
        '''Find a message by its fully qualified name or simple name.
        
        Args:
            message_name: Message name like "mypkg.Packet", "mypkg.sub.Packet", or "Packet"
            
        Returns:
            The Message object if found, None otherwise.
        '''
        if not message_name:
            return None
        
        # Normalize the message name: remove leading dots
        normalized_name = message_name.lstrip('.')
        
        # Build package prefix
        pkg_prefix = self.fdesc.package + '.' if self.fdesc.package else ''
        
        for msg in self.messages:
            # msg.name is like "chat_ClientMessage" or "mypackage_sub_Packet"
            # We need to match against various name forms
            msg_name_str = str(msg.name)
            
            # Extract the simple message name (last part after underscore)
            msg_name_parts = msg_name_str.split('_')
            simple_name = msg_name_parts[-1] if len(msg_name_parts) > 1 else msg_name_str
            
            # Try to reconstruct the fully qualified name
            # Replace underscores with dots for nested messages
            if len(msg_name_parts) > 1 and self.fdesc.package:
                # If package is "mypkg" and msg.name is "mypkg_sub_Packet", 
                # the fully qualified name is "mypkg.sub.Packet"
                # First, try to strip the package prefix from the mangled name
                pkg_parts = self.fdesc.package.split('.')
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
    
    def detect_envelope_pattern(self, envelope_name=None):
        '''Detect if there's an Envelope message with enum + oneof payload pattern, or just oneof.
        Returns (envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map) or None.
        For oneof-only patterns, opcode_field, opcode_enum, and opcode_to_msg_map will be None.
        
        Args:
            envelope_name: Optional name of the envelope message to use. If provided, only that message is checked.
        '''
        messages_to_check = self.messages
        
        # If envelope_name is specified, filter to just that message
        if envelope_name:
            messages_to_check = [msg for msg in self.messages if str(msg.name).split('_')[-1].lower() == envelope_name.lower()]
        
        for msg in messages_to_check:
            # Look for a message with both an enum field and a oneof
            enum_field = None
            oneof_field = None
            
            for field in msg.fields:
                # Check if this is an enum field (potential opcode) - can be ENUM or UENUM
                if hasattr(field, 'pbtype') and field.pbtype in ('ENUM', 'UENUM'):
                    enum_field = field
                # Check if this is a oneof
                elif isinstance(field, OneOf):
                    oneof_field = field
            
            # If we found both an enum and a oneof, this is likely an envelope with opcode
            if enum_field and oneof_field:
                # Try to find the enum definition
                opcode_enum = None
                for enum in self.enums:
                    if str(enum.names) == str(enum_field.ctype):
                        opcode_enum = enum
                        break
                
                if not opcode_enum:
                    continue
                
                # Build mapping from enum values to message types in the oneof
                # This requires matching enum value names to oneof field names
                opcode_to_msg_map = {}
                
                for enum_name, enum_value in opcode_enum.values:
                    # Get the last part of the enum name (e.g., "OP_LOGIN" -> "LOGIN")
                    enum_suffix = str(enum_name).split('_')[-1].lower()
                    
                    # Try to match with oneof field names
                    for oneof_subfield in oneof_field.fields:
                        field_name_lower = oneof_subfield.name.lower()
                        if enum_suffix == field_name_lower or enum_suffix in field_name_lower:
                            # Found a match
                            opcode_to_msg_map[enum_value] = oneof_subfield
                            break
                
                # If we have at least one mapping, consider this a valid envelope pattern
                if opcode_to_msg_map:
                    return (msg, enum_field, opcode_enum, oneof_field, opcode_to_msg_map)
            
            # If we found just a oneof (no enum), this is a simpler envelope pattern
            elif oneof_field:
                # Return with None for opcode-related fields
                return (msg, None, None, oneof_field, None)
        
        return None
    
    def detect_any_envelope_pattern(self, envelope_name=None):
        '''Detect if there's an Envelope message with google.protobuf.Any payload pattern.
        Returns (envelope_msg, any_field, all_msg_types) or None.
        
        Args:
            envelope_name: Optional name of the envelope message to use. If provided, only that message is checked.
        '''
        messages_to_check = self.messages
        
        # If envelope_name is specified, filter to just that message
        if envelope_name:
            messages_to_check = [msg for msg in self.messages if str(msg.name).split('_')[-1].lower() == envelope_name.lower()]
        
        for msg in messages_to_check:
            # Look for a message with a google.protobuf.Any field
            any_field = None
            
            for field in msg.fields:
                # Check if this is an Any field
                # The ctype for Any fields will be 'google_protobuf_Any' or similar
                if hasattr(field, 'ctype'):
                    ctype_str = str(field.ctype).lower()
                    if 'any' in ctype_str and 'google' in ctype_str:
                        any_field = field
                        break
            
            # If we found an Any field, this is an Any-based envelope
            if any_field:
                # Collect all message types that could be payloads
                # (exclude the envelope itself)
                all_msg_types = []
                for other_msg in self.messages:
                    if other_msg != msg:
                        all_msg_types.append(other_msg)
                
                return (msg, any_field, all_msg_types)
        
        return None
    
    def generate_callback_helpers_for_message(self, msg, options, skip_typedef=False):
        '''Generate decode callback helpers and wiring function for a message with callback fields'''
        msg_type_name = Globals.naming_style.type_name(msg.name)
        
        # Structure to pass violations context to callbacks AND store decoded data
        # Skip typedef if it's already been generated in the header
        if not skip_typedef:
            yield '/* Callback context structure for %s */\n' % msg_type_name
            yield '/* Stores decoded callback field data for validation by pb_validate_%s() */\n' % msg_type_name
            yield 'typedef struct {\n'
            yield '    pb_violations_t *violations;\n'
            yield '    const char *field_path;\n'
            
            # Add storage for each callback field
            for field in msg.fields:
                if isinstance(field, OneOf):
                    continue
                if field.allocation == 'CALLBACK':
                    field_var_name = Globals.naming_style.var_name(field.name)
                    if field.pbtype in ['STRING', 'BYTES']:
                        # Store data + length for string/bytes content-based validation
                        # Maximum buffer size for callback string storage
                        max_callback_string_size = 256  # Reasonable limit for validation
                        yield '    /* Decoded data for callback field: %s */\n' % field.name
                        yield '    char %s_data[%d];\n' % (field_var_name, max_callback_string_size)
                        yield '    size_t %s_length;\n' % field_var_name
                        yield '    bool %s_decoded;\n' % field_var_name
                    elif field.pbtype == 'MESSAGE':
                        # For submessages, we can validate immediately but set a flag
                        yield '    /* Validation status for callback field: %s */\n' % field.name
                        yield '    bool %s_validated;\n' % field_var_name
            
            yield '} %s_callback_ctx_t;\n\n' % msg_type_name
        
        # Generate decode callbacks for each callback field
        for field in msg.fields:
            if isinstance(field, OneOf):
                continue  # Skip oneofs for now
            
            if field.allocation == 'CALLBACK':
                # Generate callback function for this field
                if field.pbtype == 'MESSAGE':
                    # Submessage callback
                    for line in self.generate_submessage_decode_callback(msg, field, options):
                        yield line
                elif field.pbtype in ['STRING', 'BYTES']:
                    # String/bytes callback
                    for line in self.generate_string_bytes_decode_callback(msg, field, options):
                        yield line
        
        # Generate pb_wire_callbacks function
        yield '/* Wire decode callbacks for %s */\n' % msg_type_name
        yield 'static void pb_wire_callbacks_%s(%s *msg, %s_callback_ctx_t *ctx) {\n' % (msg_type_name, msg_type_name, msg_type_name)
        yield '\n'
        
        for field in msg.fields:
            if isinstance(field, OneOf):
                continue
            
            if field.allocation == 'CALLBACK':
                field_var_name = Globals.naming_style.var_name(field.name)
                callback_func_name = 'pb_decode_callback_%s_%s' % (msg_type_name, field_var_name)
                
                yield '    msg->%s.funcs.decode = %s;\n' % (field_var_name, callback_func_name)
                yield '    msg->%s.arg = ctx;\n' % field_var_name
        
        yield '}\n\n'
    
    def generate_submessage_decode_callback(self, msg, field, options):
        '''Generate decode callback for a submessage field
        Decodes and validates submessage, sets flag in context.
        Validation happens here because we can't store entire submessage (too large).
        If the submessage has callback fields, we create a local callback_ctx for it.
        
        Note: For callback fields, pb_decode already creates a substream before calling
        the callback, so the stream passed here is already positioned at the submessage
        contents with the correct length.
        '''
        msg_type_name = Globals.naming_style.type_name(msg.name)
        field_var_name = Globals.naming_style.var_name(field.name)
        submsg_type_name = Globals.naming_style.type_name(field.ctype)
        callback_func_name = 'pb_decode_callback_%s_%s' % (msg_type_name, field_var_name)
        
        # Check if submessage type has callback fields
        submsg_has_callbacks = False
        for submsg in self.messages:
            if str(submsg.name) == str(field.ctype):
                for subfield in submsg.fields:
                    if not isinstance(subfield, OneOf) and subfield.allocation == 'CALLBACK':
                        submsg_has_callbacks = True
                        break
                break
        
        yield '/* Decode callback for %s.%s */\n' % (msg_type_name, field_var_name)
        yield '/* Validates submessage and sets flag - validation logic in pb_validate_%s() */\n' % submsg_type_name
        yield 'static bool %s(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {\n' % callback_func_name
        yield '    %s_callback_ctx_t *ctx = (%s_callback_ctx_t *)*arg;\n' % (msg_type_name, msg_type_name)
        yield '    (void)field; /* Unused parameter */\n'
        yield '\n'
        yield '    /* Allocate temporary message on stack */\n'
        yield '    %s tmp = %s;\n' % (submsg_type_name, Globals.naming_style.define_name(str(field.ctype) + '_init_zero'))
        yield '\n'
        
        if submsg_has_callbacks:
            # Submessage has callback fields - need callback context
            yield '    /* Submessage has callback fields - create local callback context */\n'
            yield '    %s_callback_ctx_t submsg_ctx = {0};\n' % submsg_type_name
            yield '    submsg_ctx.violations = ctx->violations;\n'
            yield '    submsg_ctx.field_path = "%s.%s";\n' % (msg_type_name, field_var_name)
            yield '    pb_wire_callbacks_%s(&tmp, &submsg_ctx);\n' % submsg_type_name
            yield '\n'
            yield '    /* Decode the submessage (stream is already a substream for callbacks) */\n'
            yield '    if (!pb_decode(stream, &%s_msg, &tmp)) {\n' % submsg_type_name
            yield '        return false;\n'
            yield '    }\n'
            yield '\n'
            yield '    /* Validate by calling validator function with callback context */\n'
            yield '    if (!pb_validate_%s(&tmp, ctx->violations, &submsg_ctx)) {\n' % submsg_type_name
            yield '        return false;\n'
            yield '    }\n'
        else:
            # Submessage has no callback fields - simple decode and validate
            yield '    /* Decode the submessage (stream is already a substream for callbacks) */\n'
            yield '    if (!pb_decode(stream, &%s_msg, &tmp)) {\n' % submsg_type_name
            yield '        return false;\n'
            yield '    }\n'
            yield '\n'
            yield '    /* Validate by calling validator function */\n'
            yield '    if (!pb_validate_%s(&tmp, ctx->violations)) {\n' % submsg_type_name
            yield '        return false;\n'
            yield '    }\n'
        
        yield '\n'
        yield '    /* Set flag that this field was validated */\n'
        yield '    ctx->%s_validated = true;\n' % field_var_name
        yield '\n'
        yield '    return true;\n'
        yield '}\n\n'
    
    def generate_string_bytes_decode_callback(self, msg, field, options):
        '''Generate decode callback for a string or bytes field  
        Stores decoded field data AND length in context for validation by pb_validate_Msg().
        
        For REPEATED fields: Also validates EACH item during decode, because the
        callback overwrites the buffer on each invocation (only the last item would
        be validated otherwise).
        
        For singular fields: Validation happens later in pb_validate_Msg().
        '''
        msg_type_name = Globals.naming_style.type_name(msg.name)
        field_var_name = Globals.naming_style.var_name(field.name)
        callback_func_name = 'pb_decode_callback_%s_%s' % (msg_type_name, field_var_name)
        max_callback_string_size = 256  # Must match the buffer size in context struct
        
        # Check if field is repeated
        is_repeated = hasattr(field, 'rules') and field.rules == 'REPEATED'
        
        yield '/* Decode callback for %s.%s */\n' % (msg_type_name, field_var_name)
        if is_repeated:
            yield '/* REPEATED field: validates EACH item during decode (buffer is overwritten per item) */\n'
        else:
            yield '/* Stores field data and length in context - validation happens in pb_validate_%s() */\n' % msg_type_name
        yield 'static bool %s(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {\n' % callback_func_name
        yield '    %s_callback_ctx_t *ctx = (%s_callback_ctx_t *)*arg;\n' % (msg_type_name, msg_type_name)
        yield '    (void)field; /* Unused parameter */\n'
        yield '\n'
        yield '    /* Get field length from stream */\n'
        yield '    size_t len = stream->bytes_left;\n'
        yield '    size_t copy_len = (len < %d - 1) ? len : %d - 1;  /* Leave room for null terminator */\n' % (max_callback_string_size, max_callback_string_size)
        yield '\n'
        yield '    /* Read and store the string content for content-based validation */\n'
        yield '    if (!pb_read(stream, (uint8_t *)ctx->%s_data, copy_len)) {\n' % field_var_name
        yield '        return false;\n'
        yield '    }\n'
        yield '    ctx->%s_data[copy_len] = \'\\0\';  /* Null-terminate */\n' % field_var_name
        yield '\n'
        yield '    /* Skip any remaining bytes if string was truncated */\n'
        yield '    while (stream->bytes_left > 0) {\n'
        yield '        uint8_t byte;\n'
        yield '        if (!pb_read(stream, &byte, 1)) {\n'
        yield '            return false;\n'
        yield '        }\n'
        yield '    }\n'
        yield '\n'
        yield '    /* Store length and mark as decoded */\n'
        yield '    ctx->%s_length = len;  /* Original length before truncation */\n' % field_var_name
        yield '    ctx->%s_decoded = true;\n' % field_var_name
        yield '\n'
        
        # For repeated fields, generate inline validation for EACH item during decode
        if is_repeated and hasattr(field, 'validate_rules') and field.validate_rules:
            yield '    /* REPEATED FIELD: Validate THIS item now (before next item overwrites) */\n'
            for line in self._generate_inline_string_validation(field, field_var_name, msg_type_name):
                yield line
            yield '\n'
        
        yield '    return true;\n'
        yield '}\n\n'
    
    def _generate_inline_string_validation(self, field, field_var_name, msg_type_name):
        '''Generate inline validation code for a string field during decode callback.
        This is used for REPEATED callback strings to validate each item as it's decoded.
        
        For repeated string fields, rules are at rules.repeated.items.string
        For singular string fields, rules are at rules.string
        '''
        rules = field.validate_rules
        if not rules:
            return
        
        # Get string rules - handle both direct string and repeated.items.string
        string_rules = None
        
        # Check for direct string rules first
        if rules.HasField('string'):
            string_rules = rules.string
        # For repeated fields, check repeated.items.string
        elif rules.HasField('repeated') and rules.repeated.HasField('items') and rules.repeated.items.HasField('string'):
            string_rules = rules.repeated.items.string
        
        if not string_rules:
            return
        
        # MIN_LEN
        if string_rules.HasField('min_len'):
            min_len = string_rules.min_len
            yield '    if (ctx->%s_length < %d) {\n' % (field_var_name, min_len)
            yield '        if (ctx->violations) {\n'
            yield '            pb_violations_add(ctx->violations, ctx->field_path, "%s.string.min_len", "String too short");\n' % field_var_name
            yield '        }\n'
            yield '    }\n'
        
        # MAX_LEN
        if string_rules.HasField('max_len'):
            max_len = string_rules.max_len
            yield '    if (ctx->%s_length > %d) {\n' % (field_var_name, max_len)
            yield '        if (ctx->violations) {\n'
            yield '            pb_violations_add(ctx->violations, ctx->field_path, "%s.string.max_len", "String too long");\n' % field_var_name
            yield '        }\n'
            yield '    }\n'
        
        # PREFIX
        if string_rules.HasField('prefix'):
            prefix = string_rules.prefix
            # Escape for C string
            prefix_escaped = prefix.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')
            yield '    {\n'
            yield '        const char *__pb_prefix = "%s";\n' % prefix_escaped
            yield '        size_t __pb_prefix_len = %d;\n' % len(prefix)
            yield '        if (ctx->%s_length < __pb_prefix_len || strncmp(ctx->%s_data, __pb_prefix, __pb_prefix_len) != 0) {\n' % (field_var_name, field_var_name)
            yield '            if (ctx->violations) {\n'
            yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.prefix", "String must start with prefix");\n' % field_var_name
            yield '            }\n'
            yield '        }\n'
            yield '    }\n'
        
        # SUFFIX
        if string_rules.HasField('suffix'):
            suffix = string_rules.suffix
            suffix_escaped = suffix.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')
            yield '    {\n'
            yield '        const char *__pb_suffix = "%s";\n' % suffix_escaped
            yield '        size_t __pb_suffix_len = %d;\n' % len(suffix)
            yield '        if (ctx->%s_length >= __pb_suffix_len) {\n' % field_var_name
            yield '            const char *__pb_end = ctx->%s_data + ctx->%s_length - __pb_suffix_len;\n' % (field_var_name, field_var_name)
            yield '            if (strncmp(__pb_end, __pb_suffix, __pb_suffix_len) != 0) {\n'
            yield '                if (ctx->violations) {\n'
            yield '                    pb_violations_add(ctx->violations, ctx->field_path, "%s.string.suffix", "String must end with suffix");\n' % field_var_name
            yield '                }\n'
            yield '            }\n'
            yield '        } else {\n'
            yield '            if (ctx->violations) {\n'
            yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.suffix", "String must end with suffix");\n' % field_var_name
            yield '            }\n'
            yield '        }\n'
            yield '    }\n'
        
        # CONTAINS
        if string_rules.HasField('contains'):
            needle = string_rules.contains
            needle_escaped = needle.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r')
            yield '    {\n'
            yield '        const char *__pb_needle = "%s";\n' % needle_escaped
            yield '        bool __pb_found = (strstr(ctx->%s_data, __pb_needle) != NULL);\n' % field_var_name
            yield '        if (!__pb_found) {\n'
            yield '            if (ctx->violations) {\n'
            yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.contains", "String must contain substring");\n' % field_var_name
            yield '            }\n'
            yield '        }\n'
            yield '    }\n'
        
        # ASCII
        if string_rules.HasField('ascii') and string_rules.ascii:
            yield '    {\n'
            yield '        bool __pb_is_ascii = true;\n'
            yield '        for (size_t __pb_i = 0; __pb_i < ctx->%s_length; __pb_i++) {\n' % field_var_name
            yield '            if ((unsigned char)ctx->%s_data[__pb_i] > 127) {\n' % field_var_name
            yield '                __pb_is_ascii = false; break;\n'
            yield '            }\n'
            yield '        }\n'
            yield '        if (!__pb_is_ascii) {\n'
            yield '            if (ctx->violations) {\n'
            yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.ascii", "String must be ASCII only");\n' % field_var_name
            yield '            }\n'
            yield '        }\n'
            yield '    }\n'
        
        # EMAIL
        if string_rules.HasField('email') and string_rules.email:
            yield '    if (!pb_validate_string(ctx->%s_data, ctx->%s_length, NULL, PB_VALIDATE_RULE_EMAIL)) {\n' % (field_var_name, field_var_name)
            yield '        if (ctx->violations) {\n'
            yield '            pb_violations_add(ctx->violations, ctx->field_path, "%s.string.email", "Invalid email format");\n' % field_var_name
            yield '        }\n'
            yield '    }\n'
        
        # HOSTNAME
        if string_rules.HasField('hostname') and string_rules.hostname:
            yield '    if (!pb_validate_string(ctx->%s_data, ctx->%s_length, NULL, PB_VALIDATE_RULE_HOSTNAME)) {\n' % (field_var_name, field_var_name)
            yield '        if (ctx->violations) {\n'
            yield '            pb_violations_add(ctx->violations, ctx->field_path, "%s.string.hostname", "Invalid hostname format");\n' % field_var_name
            yield '        }\n'
            yield '    }\n'
        
        # IP (general IP check)
        if string_rules.HasField('ip') and string_rules.ip:
            yield '    if (!pb_validate_string(ctx->%s_data, ctx->%s_length, NULL, PB_VALIDATE_RULE_IP)) {\n' % (field_var_name, field_var_name)
            yield '        if (ctx->violations) {\n'
            yield '            pb_violations_add(ctx->violations, ctx->field_path, "%s.string.ip", "Invalid IP address format");\n' % field_var_name
            yield '        }\n'
            yield '    }\n'
        
        # IN (set membership)
        if hasattr(string_rules, 'in') and getattr(string_rules, 'in'):
            values = list(getattr(string_rules, 'in'))
            if values:
                values_escaped = ', '.join('"%s"' % v.replace('\\', '\\\\').replace('"', '\\"') for v in values)
                yield '    {\n'
                yield '        bool __pb_in_set = false;\n'
                yield '        const char *__pb_allowed[] = { %s };\n' % values_escaped
                yield '        for (size_t __pb_k = 0; __pb_k < sizeof(__pb_allowed)/sizeof(__pb_allowed[0]); __pb_k++) {\n'
                yield '            if (strcmp(ctx->%s_data, __pb_allowed[__pb_k]) == 0) {\n' % field_var_name
                yield '                __pb_in_set = true; break;\n'
                yield '            }\n'
                yield '        }\n'
                yield '        if (!__pb_in_set) {\n'
                yield '            if (ctx->violations) {\n'
                yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.in", "Value not in allowed set");\n' % field_var_name
                yield '            }\n'
                yield '        }\n'
                yield '    }\n'
        
        # NOT_IN (set exclusion)
        if string_rules.not_in:
            values = list(string_rules.not_in)
            if values:
                values_escaped = ', '.join('"%s"' % v.replace('\\', '\\\\').replace('"', '\\"') for v in values)
                yield '    {\n'
                yield '        bool __pb_forbidden = false;\n'
                yield '        const char *__pb_blocked[] = { %s };\n' % values_escaped
                yield '        for (size_t __pb_k = 0; __pb_k < sizeof(__pb_blocked)/sizeof(__pb_blocked[0]); __pb_k++) {\n'
                yield '            if (strcmp(ctx->%s_data, __pb_blocked[__pb_k]) == 0) {\n' % field_var_name
                yield '                __pb_forbidden = true; break;\n'
                yield '            }\n'
                yield '        }\n'
                yield '        if (__pb_forbidden) {\n'
                yield '            if (ctx->violations) {\n'
                yield '                pb_violations_add(ctx->violations, ctx->field_path, "%s.string.not_in", "Value in forbidden set");\n' % field_var_name
                yield '            }\n'
                yield '        }\n'
                yield '    }\n'
    
    def generate_service_filter_implementations(self, options):
        '''Generate implementation of packet filter functions (service- or envelope-driven)'''
        # Determine envelope detection mode and name from options
        envelope_mode = getattr(options, 'envelope_mode', 'oneof')
        envelope_name = getattr(options, 'envelope_name', None)
        root_message_name = getattr(options, 'root_message', None)
        
        # Check if single-root-message mode is enabled
        root_message = None
        if root_message_name:
            root_message = self.find_message_by_name(root_message_name)
            if not root_message:
                # Error: unknown message type - raise an error
                sys.stderr.write("Error: --root-message '%s' does not match any message in the loaded descriptors.\n" % root_message_name)
                sys.stderr.write("Available messages:\n")
                for msg in self.messages:
                    sys.stderr.write("  - %s\n" % str(msg.name))
                sys.exit(1)
        
        # Detect envelope pattern based on mode (only if root_message is not set)
        envelope_info = None
        any_envelope_info = None
        
        if not root_message:
            if envelope_mode == 'any':
                any_envelope_info = self.detect_any_envelope_pattern(envelope_name)
            else:  # Default to 'oneof' mode
                envelope_info = self.detect_envelope_pattern(envelope_name)
        
        # Collect all message types used in services (only if services exist)
        all_msg_types = set()
        if self.services:
            for service in self.services:
                all_msg_types.update(service.get_all_message_types())
        
        # Map type names to message objects
        # Service methods reference types like ".chat.ClientMessage"
        # We need to map these to actual message objects
        msg_map = {}
        for msg in self.messages:
            # Build the full protobuf type name
            # msg.name is like "chat_ClientMessage", we need to extract the actual message name
            msg_name_parts = str(msg.name).split('_')
            
            # Try to match with package + message name
            if self.fdesc.package:
                # For messages like chat_ClientMessage, the last part is the actual message name
                actual_msg_name = msg_name_parts[-1] if len(msg_name_parts) > 1 else str(msg.name)
                full_type_name = '.' + self.fdesc.package + '.' + actual_msg_name
                msg_map[full_type_name] = msg
            else:
                # No package, just use the message name
                actual_msg_name = msg_name_parts[-1] if len(msg_name_parts) > 1 else str(msg.name)
                full_type_name = '.' + actual_msg_name
                msg_map[full_type_name] = msg
        
        
        # Stop immediately if --validate not set: no validation helpers or filters.
        if not options.validate:
            return

        # Include validation header only when --validate flag is given.
        basename = self.fdesc.name.rsplit('.', 1)[0]
        yield '#include "%s_validate.h"\n' % basename

        # Generate helper function to validate a message (only when --validate)
        yield 'static int validate_message(const pb_msgdesc_t *fields, const void *msg_struct) {\n'
        yield '    pb_violations_t violations = {0};\n'
        for msg in self.messages:
            msg_type_name = Globals.naming_style.type_name(msg.name)
            validate_func_name = 'pb_validate_' + msg_type_name
            # Check if message has callback fields
            has_callback_fields = any(f.allocation == 'CALLBACK' for f in msg.fields if not isinstance(f, OneOf))
            yield '    if (fields == &%s_msg) {\n' % msg_type_name
            if has_callback_fields:
                # Callback context is not available in this static helper, so pass NULL
                # This function is only used for validating nested messages without callbacks
                yield '        /* Note: %s has callback fields, but callback_ctx not available in this context */\n' % msg_type_name
                yield '        /* This path should not be reached for messages with callback fields */\n'
                yield '        return 0; /* Cannot validate without callback context */\n'
            else:
                yield '        return %s((const %s *)msg_struct, &violations) ? 1 : 0;\n' % (validate_func_name, msg_type_name)
            yield '    }\n'
        yield '    return 1; /* Default: message is valid */\n'
        yield '}\n\n'
        
        # Generate callback decode helpers and wiring functions for messages that need them
        # Only generate for: root_message, envelope messages, or messages used in filters
        # Also need to generate for nested messages that have callback fields
        messages_needing_callbacks = set()
        
        if root_message:
            messages_needing_callbacks.add(root_message)
        elif any_envelope_info:
            envelope_msg, any_field, all_msg_types = any_envelope_info
            messages_needing_callbacks.add(envelope_msg)
            # Also add all payload types that might be in the Any field
            for msg_type in all_msg_types:
                messages_needing_callbacks.add(msg_type)
        elif envelope_info:
            envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info
            messages_needing_callbacks.add(envelope_msg)
        
        # Recursively add nested message types that are referenced by callback fields
        def add_nested_callback_messages(msg):
            for field in msg.fields:
                if isinstance(field, OneOf):
                    continue
                if field.allocation == 'CALLBACK' and field.pbtype == 'MESSAGE':
                    # Find the message type for this field
                    submsg_ctype = field.ctype
                    for candidate_msg in self.messages:
                        if str(candidate_msg.name) == str(submsg_ctype):
                            if candidate_msg not in messages_needing_callbacks:
                                messages_needing_callbacks.add(candidate_msg)
                                # Recursively check this message's nested types
                                add_nested_callback_messages(candidate_msg)
                            break
        
        # Start from the root messages and recursively add nested types
        for msg in list(messages_needing_callbacks):
            add_nested_callback_messages(msg)
        
        # Generate callback helpers only for messages that need them
        # If validation is enabled, typedef was already generated in header, so skip it
        skip_typedef = (self.validate_enabled or options.validate)
        for msg in self.messages:
            if msg not in messages_needing_callbacks:
                continue
            has_callback_fields = any(f.allocation == 'CALLBACK' for f in msg.fields if not isinstance(f, OneOf))
            if has_callback_fields:
                # Generate callback helpers and wiring function for this message
                for line in self.generate_callback_helpers_for_message(msg, options, skip_typedef=skip_typedef):
                    yield line
        
        # Configure return codes (success/failure)
        ret_ok = '0'
        ret_err = '-1'

        # Generate filter_udp function
        yield 'int filter_udp(void *ctx, uint8_t *packet, size_t packet_size) {\n'
        yield '    pb_istream_t stream;\n'
        yield '    bool status;\n'
        yield '    (void)ctx; /* Context may be unused */\n\n'
        
        if root_message:
            # Single-root-message mode: decode and validate directly as the specified message type
            msg_type = Globals.naming_style.type_name(root_message.name)
            validate_func_name = 'pb_validate_' + msg_type
            
            yield '    /* Single-root-message mode: decode as %s */\n' % msg_type
            yield '    %s msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(root_message.name) + '_init_zero'))
            
            # Wire callbacks if the message has callback fields
            has_callback_fields = any(f.allocation == 'CALLBACK' for f in root_message.fields if not isinstance(f, OneOf))
            if has_callback_fields:
                yield '    \n'
                yield '    /* Wire callbacks for automatic decode+validation */\n'
                yield '    pb_violations_t violations = {0};\n'
                yield '    %s_callback_ctx_t callback_ctx = {0};\n' % msg_type
                yield '    callback_ctx.violations = &violations;\n'
                yield '    callback_ctx.field_path = "%s";\n' % msg_type
                yield '    pb_wire_callbacks_%s(&msg, &callback_ctx);\n' % msg_type
            
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            # Validate - pass callback_ctx if we have callback fields
            if has_callback_fields:
                yield '    /* Validate the root message with callback context */\n'
                yield '    if (!%s(&msg, &violations, &callback_ctx)) {\n' % validate_func_name
                yield f'        return {ret_err};\n'
                yield '    }\n'
                yield f'    return {ret_ok};\n'
            else:
                yield '    /* Validate the root message */\n'
                yield '    if (validate_message(&%s_msg, &msg)) {\n' % msg_type
                yield f'        return {ret_ok};\n'
                yield '    }\n'
                yield '    \n'
                yield f'    return {ret_err};\n'
        elif any_envelope_info:
            # Use Any-based envelope decoding
            envelope_msg, any_field, all_msg_types = any_envelope_info
            envelope_type = Globals.naming_style.type_name(envelope_msg.name)
            any_field_name = Globals.naming_style.var_name(any_field.name)
            
            yield '    %s envelope = %s;\n' % (envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            yield '    /* Validate the envelope message first (checks any.in/any.not_in rules) */\n'
            yield '    if (!validate_message(&%s_msg, &envelope)) {\n' % envelope_type
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            yield '    /* Extract type_url from Any field */\n'
            yield '    const char *type_url = (const char *)envelope.%s.type_url;\n' % any_field_name
            yield '    if (!type_url) {\n'
            yield f'        return {ret_err};\n'
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
                
                # Build the expected type_url (typically "type.googleapis.com/package.MessageName")
                if self.fdesc.package:
                    expected_type_url = 'type.googleapis.com/%s.%s' % (self.fdesc.package, msg_simple_name)
                else:
                    expected_type_url = 'type.googleapis.com/%s' % msg_simple_name
                
                # Calculate hash for the case label
                type_hash = 0
                for c in expected_type_url:
                    type_hash = (type_hash * 31 + ord(c)) & 0xFFFFFFFF
                
                yield '        case 0x%08XU: /* %s */\n' % (type_hash, expected_type_url)
                yield '            if (strcmp(type_url, "%s") == 0) {\n' % expected_type_url
                yield '                %s payload_msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
                yield '                pb_istream_t payload_stream = pb_istream_from_buffer(envelope.%s.value.bytes, envelope.%s.value.size);\n' % (any_field_name, any_field_name)
                yield '                if (pb_decode(&payload_stream, &%s_msg, &payload_msg)) {\n' % msg_type
                yield '                    if (validate_message(&%s_msg, &payload_msg)) {\n' % msg_type
                yield f'                        return {ret_ok};\n'
                yield '                    }\n'
                yield '                }\n'
                yield '            }\n'
                yield '            break;\n'
            
            yield '        default:\n'
            yield '            break;\n'
            yield '    }\n'
            yield '    \n'
            yield f'    return {ret_err};\n'
        elif envelope_info:
            # Use efficient envelope-based decoding
            envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info
            envelope_type = Globals.naming_style.type_name(envelope_msg.name)
            
            yield '    %s envelope = %s;\n' % (envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            
            # Check if we have an opcode-based envelope or oneof-only envelope
            if opcode_field and opcode_enum and opcode_to_msg_map:
                # Opcode + oneof pattern
                opcode_field_name = Globals.naming_style.var_name(opcode_field.name)
                # CAPS alias type produced in header
                alias_type = (str(envelope_msg.name) + '_OPCODE').replace('.', '_').replace('-', '_').upper()
                # Map numeric opcode values to original enumerator names
                val_to_name = {v: Globals.naming_style.enum_entry(n).replace('.', '_').replace('-', '_').upper() for (n, v) in opcode_enum.values}
                
                yield '    switch (envelope.%s) {\n' % opcode_field_name
                
                for opcode_val, oneof_subfield in sorted(opcode_to_msg_map.items(), key=lambda x: x[0]):
                    submsg_type = Globals.naming_style.type_name(oneof_subfield.ctype)
                    oneof_member_name = Globals.naming_style.var_name(oneof_subfield.name)
                    oneof_name = Globals.naming_style.var_name(oneof_field.name)
                    enum_suffix = val_to_name.get(opcode_val, None)
                    if enum_suffix is not None:
                        yield '        case %s_%s:\n' % (alias_type, enum_suffix)
                    else:
                        # Fallback to numeric value if mapping fails
                        yield '        case %d:\n' % (opcode_val)
                    # Use the field tag constant for the which_field comparison
                    tag_constant = Globals.naming_style.define_name('%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))
                    yield '            if (envelope.which_%s == %s) {\n' % (
                        oneof_name,
                        tag_constant
                    )
                    # For MESSAGE types, validate with message descriptor; for scalars, validate the whole envelope
                    if oneof_subfield.pbtype == 'MESSAGE':
                        yield '                if (validate_message(&%s_msg, &envelope.%s.%s)) {\n' % (
                            submsg_type, oneof_name, oneof_member_name
                        )
                    else:
                        # For scalar types, validate the whole envelope message
                        yield '                if (validate_message(&%s_msg, &envelope)) {\n' % (
                            Globals.naming_style.type_name(envelope_msg.name)
                        )
                    yield f'                    return {ret_ok};\n'
                    yield '                }\n'
                    yield '            }\n'
                    yield '            break;\n'
                
                yield '        default:\n'
                yield f'            return {ret_err};\n'
                yield '    }\n'
                yield '    \n'
                yield f'    return {ret_err};\n'
            else:
                # Oneof-only pattern - switch on which_field
                oneof_name = Globals.naming_style.var_name(oneof_field.name)
                yield '    switch (envelope.which_%s) {\n' % oneof_name
                
                for oneof_subfield in oneof_field.fields:
                    submsg_type = Globals.naming_style.type_name(oneof_subfield.ctype)
                    oneof_member_name = Globals.naming_style.var_name(oneof_subfield.name)
                    tag_constant = Globals.naming_style.define_name('%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))
                    
                    yield '        case %s:\n' % tag_constant
                    # For MESSAGE types, validate with message descriptor; for scalars, validate the whole envelope
                    if oneof_subfield.pbtype == 'MESSAGE':
                        yield '            if (validate_message(&%s_msg, &envelope.%s.%s)) {\n' % (
                            submsg_type, oneof_name, oneof_member_name
                        )
                    else:
                        # For scalar types, validate the whole envelope message
                        yield '            if (validate_message(&%s_msg, &envelope)) {\n' % (
                            Globals.naming_style.type_name(envelope_msg.name)
                        )
                    yield f'                return {ret_ok};\n'
                    yield '            }\n'
                    yield '            break;\n'
                
                yield '        default:\n'
                yield '            break;\n'
                yield '    }\n'
                yield '    \n'
                yield f'    return {ret_err};\n'
        elif self.services and all_msg_types:
            # Fallback to brute-force decoding
            pass
            for msg_type_name in sorted(all_msg_types):
                if msg_type_name in msg_map:
                    msg = msg_map[msg_type_name]
                    msg_type = Globals.naming_style.type_name(msg.name)
                    
                    pass
                    yield '    {\n'
                    yield '        %s msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
                    yield '        stream = pb_istream_from_buffer(packet, packet_size);\n'
                    yield '        status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
                    yield '        if (status) {\n'
                    pass
                    yield '            if (validate_message(&%s_msg, &msg)) {\n' % msg_type
                    yield f'                return {ret_ok};\n'
                    yield '            }\n'
                    yield '        }\n'
                    yield '    }\n\n'
            
            yield f'    return {ret_err};\n'
        else:
            yield f'    return {ret_err};\n'
        
        yield '}\n\n'
        
    # Generate filter_tcp function
        yield 'int filter_tcp(void *ctx, uint8_t *packet, size_t packet_size, bool is_to_server) {\n'
        yield '    pb_istream_t stream;\n'
        yield '    bool status;\n'
        yield '    (void)ctx; /* Context may be unused */\n\n'
        
        if root_message:
            # Single-root-message mode: decode and validate directly as the specified message type
            msg_type = Globals.naming_style.type_name(root_message.name)
            validate_func_name = 'pb_validate_' + msg_type
            
            yield '    (void)is_to_server; /* Direction unused in single-root-message mode */\n'
            yield '    /* Single-root-message mode: decode as %s */\n' % msg_type
            yield '    %s msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(root_message.name) + '_init_zero'))
            
            # Wire callbacks if the message has callback fields
            has_callback_fields = any(f.allocation == 'CALLBACK' for f in root_message.fields if not isinstance(f, OneOf))
            if has_callback_fields:
                yield '    \n'
                yield '    /* Wire callbacks for automatic decode+validation */\n'
                yield '    pb_violations_t violations = {0};\n'
                yield '    %s_callback_ctx_t callback_ctx;\n' % msg_type
                yield '    callback_ctx.violations = &violations;\n'
                yield '    callback_ctx.field_path = "%s";\n' % msg_type
                yield '    pb_wire_callbacks_%s(&msg, &callback_ctx);\n' % msg_type
            
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            yield '    /* Validate the root message */\n'
            yield '    if (validate_message(&%s_msg, &msg)) {\n' % msg_type
            yield f'        return {ret_ok};\n'
            yield '    }\n'
            yield '    \n'
            yield f'    return {ret_err};\n'
        elif any_envelope_info:
            # Use Any-based envelope decoding for TCP
            envelope_msg, any_field, all_msg_types = any_envelope_info
            envelope_type = Globals.naming_style.type_name(envelope_msg.name)
            any_field_name = Globals.naming_style.var_name(any_field.name)
            
            yield '    %s envelope = %s;\n' % (envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            yield '    /* Validate the envelope message first (checks any.in/any.not_in rules) */\n'
            yield '    if (!validate_message(&%s_msg, &envelope)) {\n' % envelope_type
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            yield '    /* Extract type_url from Any field */\n'
            yield '    const char *type_url = (const char *)envelope.%s.type_url;\n' % any_field_name
            yield '    if (!type_url) {\n'
            yield f'        return {ret_err};\n'
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
                
                # Build the expected type_url (typically "type.googleapis.com/package.MessageName")
                if self.fdesc.package:
                    expected_type_url = 'type.googleapis.com/%s.%s' % (self.fdesc.package, msg_simple_name)
                else:
                    expected_type_url = 'type.googleapis.com/%s' % msg_simple_name
                
                # Calculate hash for the case label
                type_hash = 0
                for c in expected_type_url:
                    type_hash = (type_hash * 31 + ord(c)) & 0xFFFFFFFF
                
                yield '        case 0x%08XU: /* %s */\n' % (type_hash, expected_type_url)
                yield '            if (strcmp(type_url, "%s") == 0) {\n' % expected_type_url
                yield '                %s payload_msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
                yield '                pb_istream_t payload_stream = pb_istream_from_buffer(envelope.%s.value.bytes, envelope.%s.value.size);\n' % (any_field_name, any_field_name)
                yield '                if (pb_decode(&payload_stream, &%s_msg, &payload_msg)) {\n' % msg_type
                yield '                    if (validate_message(&%s_msg, &payload_msg)) {\n' % msg_type
                yield f'                        return {ret_ok};\n'
                yield '                    }\n'
                yield '                }\n'
                yield '            }\n'
                yield '            break;\n'
            
            yield '        default:\n'
            yield '            break;\n'
            yield '    }\n'
            yield '    \n'
            yield f'    return {ret_err};\n'
        elif envelope_info:
            # Use efficient envelope-based decoding for TCP too
            envelope_msg, opcode_field, opcode_enum, oneof_field, opcode_to_msg_map = envelope_info
            envelope_type = Globals.naming_style.type_name(envelope_msg.name)
            
            yield '    %s envelope = %s;\n' % (envelope_type, Globals.naming_style.define_name(str(envelope_msg.name) + '_init_zero'))
            yield '    stream = pb_istream_from_buffer(packet, packet_size);\n'
            yield '    status = pb_decode(&stream, &%s_msg, &envelope);\n' % envelope_type
            yield '    \n'
            yield '    if (!status) {\n'
            yield f'        return {ret_err};\n'
            yield '    }\n'
            yield '    \n'
            
            # Check if we have an opcode-based envelope or oneof-only envelope
            if opcode_field and opcode_enum and opcode_to_msg_map:
                # Opcode + oneof pattern
                opcode_field_name = Globals.naming_style.var_name(opcode_field.name)
                alias_type = (str(envelope_msg.name) + '_OPCODE').replace('.', '_').replace('-', '_').upper()
                val_to_name = {v: Globals.naming_style.enum_entry(n).replace('.', '_').replace('-', '_').upper() for (n, v) in opcode_enum.values}
                
                yield '    switch (envelope.%s) {\n' % opcode_field_name
                
                for opcode_val, oneof_subfield in sorted(opcode_to_msg_map.items(), key=lambda x: x[0]):
                    submsg_type = Globals.naming_style.type_name(oneof_subfield.ctype)
                    oneof_member_name = Globals.naming_style.var_name(oneof_subfield.name)
                    oneof_name = Globals.naming_style.var_name(oneof_field.name)
                    enum_suffix = val_to_name.get(opcode_val, None)
                    if enum_suffix is not None:
                        yield '        case %s_%s:\n' % (alias_type, enum_suffix)
                    else:
                        yield '        case %d:\n' % (opcode_val)
                    # Use the field tag constant for the which_field comparison
                    tag_constant = Globals.naming_style.define_name('%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))
                    yield '            if (envelope.which_%s == %s) {\n' % (
                        oneof_name,
                        tag_constant
                    )
                    # For MESSAGE types, validate with message descriptor; for scalars, validate the whole envelope
                    if oneof_subfield.pbtype == 'MESSAGE':
                        yield '                if (validate_message(&%s_msg, &envelope.%s.%s)) {\n' % (
                            submsg_type, oneof_name, oneof_member_name
                        )
                    else:
                        # For scalar types, validate the whole envelope message
                        yield '                if (validate_message(&%s_msg, &envelope)) {\n' % (
                            Globals.naming_style.type_name(envelope_msg.name)
                        )
                    yield f'                    return {ret_ok};\n'
                    yield '                }\n'
                    yield '            }\n'
                    yield '            break;\n'
                
                yield '        default:\n'
                yield f'            return {ret_err};\n'
                yield '    }\n'
                yield '    \n'
                yield f'    return {ret_err};\n'
            else:
                # Oneof-only pattern - switch on which_field
                oneof_name = Globals.naming_style.var_name(oneof_field.name)
                yield '    switch (envelope.which_%s) {\n' % oneof_name
                
                for oneof_subfield in oneof_field.fields:
                    submsg_type = Globals.naming_style.type_name(oneof_subfield.ctype)
                    oneof_member_name = Globals.naming_style.var_name(oneof_subfield.name)
                    tag_constant = Globals.naming_style.define_name('%s_%s_tag' % (envelope_msg.name, oneof_subfield.name))
                    
                    yield '        case %s:\n' % tag_constant
                    # For MESSAGE types, validate with message descriptor; for scalars, validate the whole envelope
                    if oneof_subfield.pbtype == 'MESSAGE':
                        yield '            if (validate_message(&%s_msg, &envelope.%s.%s)) {\n' % (
                            submsg_type, oneof_name, oneof_member_name
                        )
                    else:
                        # For scalar types, validate the whole envelope message
                        yield '            if (validate_message(&%s_msg, &envelope)) {\n' % (
                            Globals.naming_style.type_name(envelope_msg.name)
                        )
                    yield f'                return {ret_ok};\n'
                    yield '            }\n'
                    yield '            break;\n'
                
                yield '        default:\n'
                yield '            break;\n'
                yield '    }\n'
                yield '    \n'
                yield f'    return {ret_err};\n'
        elif self.services and all_msg_types:
            # Fallback to brute-force decoding with direction awareness
            pass
            yield '    if (is_to_server) {\n'
            pass
            
            # Get input types
            input_types = set()
            for service in self.services:
                input_types.update(service.get_input_message_types())
            
            for msg_type_name in sorted(input_types):
                if msg_type_name in msg_map:
                    msg = msg_map[msg_type_name]
                    msg_type = Globals.naming_style.type_name(msg.name)
                    
                    pass
                    yield '        {\n'
                    yield '            %s msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
                    yield '            stream = pb_istream_from_buffer(packet, packet_size);\n'
                    yield '            status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
                    yield '            if (status && validate_message(&%s_msg, &msg)) {\n' % msg_type
                    if options.minimal_comments:
                        yield f'                return {ret_ok};\n'
                    else:
                        yield f'                return {ret_ok}; /* Valid request packet */\n'
                    yield '            }\n'
                    yield '        }\n\n'
            
            yield '    } else {\n'
            if not options.minimal_comments:
                yield '        /* Try server->client message types (RPC outputs) */\n'
            
            # Get output types
            output_types = set()
            for service in self.services:
                output_types.update(service.get_output_message_types())
            
            for msg_type_name in sorted(output_types):
                if msg_type_name in msg_map:
                    msg = msg_map[msg_type_name]
                    msg_type = Globals.naming_style.type_name(msg.name)
                    
                    pass
                    yield '        {\n'
                    yield '            %s msg = %s;\n' % (msg_type, Globals.naming_style.define_name(str(msg.name) + '_init_zero'))
                    yield '            stream = pb_istream_from_buffer(packet, packet_size);\n'
                    yield '            status = pb_decode(&stream, &%s_msg, &msg);\n' % msg_type
                    yield '            if (status && validate_message(&%s_msg, &msg)) {\n' % msg_type
                    if options.minimal_comments:
                        yield f'                return {ret_ok};\n'
                    else:
                        yield f'                return {ret_ok}; /* Valid response packet */\n'
                    yield '            }\n'
                    yield '        }\n\n'
            
            yield '    }\n\n'
            yield f'    return {ret_err};\n'
        else:
            yield f'    return {ret_err};\n'
        
        yield '}\n'

# ---------------------------------------------------------------------------
#                    Options parsing for the .proto files
# ---------------------------------------------------------------------------

from fnmatch import fnmatchcase

def validate_options_namemask(namemask, filename, line_number):
    '''Verify that an options pattern contains only protobuf path and fnmatch characters.'''
    # File options match .proto paths, which can contain punctuation that is
    # not valid in field names.
    if '/' in namemask or namemask.endswith('.proto'):
        invalid = re.search(r'[\s:]', namemask)
    else:
        invalid = re.search(r'[^A-Za-z0-9_.*?\[\]!]', namemask)
    if invalid:
        hint = ""
        if invalid.group(0) == ':':
            hint = " Did you mean to separate the field pattern from options with whitespace?"

        sys.stderr.write("%s:%d: " % (filename, line_number) +
                         "Invalid character %r in option field pattern %r.%s\n" %
                         (invalid.group(0), namemask, hint))
        sys.exit(1)

def strip_options_comments(data):
    '''Remove comments from .options data without touching quoted strings.'''
    string_or_comment = re.compile(
        r'''(?P<string>"(?:\\.|[^"\\\r\n])*(?:"|$)|'(?:\\.|[^'\\\r\n])*(?:'|$))'''
        r'''|(?P<comment>/\*.*?(?:\*/|\Z)|//[^\r\n]*|#[^\r\n]*)''',
        flags = re.MULTILINE | re.DOTALL)

    def strip_comment(match):
        if match.group('string'):
            return match.group('string')

        return re.sub(r'[^\r\n]', '', match.group('comment'))

    return string_or_comment.sub(strip_comment, data)

def read_options_file(infile):
    """
    Parse a .options file into a list of (namemask, options) tuples.
    
    The .options file format allows specifying nanopb options for fields
    using glob-style name patterns. Each non-empty line should contain:
        <field_pattern> <options_in_text_format>
    
    Example .options file:
        MyMessage.field_name  max_size:100
        MyMessage.*           type:FT_POINTER
        *.password            max_size:64
    
    Comments are supported:
        - C-style: /* comment */
        - C++-style: // comment
        - Shell-style: # comment
    
    Args:
        infile: File object with .name attribute, opened for reading
    
    Returns:
        List of (namemask, NanoPBOptions) tuples where namemask is a
        glob pattern string and NanoPBOptions is the parsed options.
    
    Raises:
        SystemExit: On parse errors (malformed lines or invalid options)
    """
    results = []
    data = strip_options_comments(infile.read())
    for i, line in enumerate(data.split('\n')):
        line = line.strip()
        if not line:
            continue

        parts = line.split(None, 1)

        if len(parts) < 2:
            sys.stderr.write("%s:%d: " % (infile.name, i + 1) +
                             "Option lines should have space between field name and options. " +
                             "Skipping line: '%s'\n" % line)
            sys.exit(1)

        validate_options_namemask(parts[0], infile.name, i + 1)

        opts = nanopb_pb2.NanoPBOptions()

        try:
            text_format.Merge(parts[1], opts)
        except Exception as e:
            sys.stderr.write("%s:%d: " % (infile.name, i + 1) +
                             "Unparsable option line: '%s'. " % line +
                             "Error: %s\n" % str(e))
            sys.exit(1)
        results.append((parts[0], opts))

    return results

def get_nanopb_suboptions(subdesc, options, name):
    """
    Get options for a proto element, merging parent options with element-specific ones.
    
    This function builds the effective options for a field, message, or enum by:
    1. Starting with a copy of the parent options
    2. Checking proto3 syntax and setting the flag
    3. Applying matching patterns from .options files (Globals.separate_options)
    4. Merging options specified directly in the .proto file via extensions
    
    The precedence order (later overrides earlier):
        parent_options < .options_file_patterns < inline_proto_options
    
    Args:
        subdesc: The protobuf descriptor (FieldDescriptor, MessageDescriptor, etc.)
        options: Parent NanoPBOptions to inherit from
        name: Names object for pattern matching in .options files
    
    Returns:
        NanoPBOptions with all applicable options merged
    
    Raises:
        Exception: If subdesc.options is an unknown descriptor type
    """
    new_options = nanopb_pb2.NanoPBOptions()
    new_options.CopyFrom(options)

    if hasattr(subdesc, 'syntax') and subdesc.syntax == "proto3":
        new_options.proto3 = True

    # Handle options defined in a separate file
    dotname = '.'.join(name.parts)
    for namemask, options in Globals.separate_options:
        if fnmatchcase(dotname, namemask):
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
import importlib.util
from optparse import OptionParser, OptionValueError

optparser = OptionParser(
    usage = "Usage: nanopb_generator.py [options] file.pb ...",
    epilog = "Compile file.pb from file.proto by: 'protoc -ofile.pb file.proto'. " +
             "Output will be written to file.pb.h and file.pb.c.")
optparser.add_option("-V", "--version", dest="version", action="store_true",
    help="Show version info and exit (add -v for protoc version info)")
optparser.add_option("-x", dest="exclude", metavar="FILE", action="append", default=[],
    help="Exclude file from generated #include list.")
optparser.add_option("-e", "--extension", dest="extension", metavar="EXTENSION", default=".pb",
    help="Set extension to use instead of '.pb' for generated files. [default: %default]")
optparser.add_option("-H", "--header-extension", dest="header_extension", metavar="EXTENSION", default=".h",
    help="Set extension to use for generated header files. [default: %default]")
optparser.add_option("-S", "--source-extension", dest="source_extension", metavar="EXTENSION", default=".c",
    help="Set extension to use for generated source files. [default: %default]")
optparser.add_option("-f", "--options-file", dest="options_file", metavar="FILE", default="%s.options",
    help="Set name of a separate generator options file.")
optparser.add_option("-I", "--options-path", "--proto-path", dest="options_path", metavar="DIR",
    action="append", default = [],
    help="Search path for .options and .proto files. Also determines relative paths for output directory structure.")
optparser.add_option("--error-on-unmatched", dest="error_on_unmatched", action="store_true", default=False,
                     help ="Stop generation if there are unmatched fields in options file")
optparser.add_option("--no-error-on-unmatched", dest="error_on_unmatched", action="store_false", default=False,
                     help ="Continue generation if there are unmatched fields in options file (default)")
optparser.add_option("-D", "--output-dir", dest="output_dir",
                     metavar="OUTPUTDIR", default=None,
                     help="Output directory of .pb.h and .pb.c files")
optparser.add_option("-Q", "--generated-include-format", dest="genformat",
    metavar="FORMAT", default='#include "%s"',
    help="Set format string to use for including other .pb.h files. Value can be 'quote', 'bracket' or a format string. [default: %default]")
optparser.add_option("-L", "--library-include-format", dest="libformat",
    metavar="FORMAT", default='#include <%s>',
    help="Set format string to use for including the nanopb pb.h header. Value can be 'quote', 'bracket' or a format string. [default: %default]")
optparser.add_option("--strip-path", dest="strip_path", action="store_true", default=False,
    help="Strip directory path from #included .pb.h file name")
optparser.add_option("--no-strip-path", dest="strip_path", action="store_false",
    help="Opposite of --strip-path (default since 0.4.0)")
optparser.add_option("--cpp-descriptors", action="store_true",
    help="Generate C++ descriptors to lookup by type (e.g. pb_field_t for a message)")
optparser.add_option("-T", "--no-timestamp", dest="notimestamp", action="store_true", default=True,
    help="Don't add timestamp to .pb.h and .pb.c preambles (default since 0.4.0)")
optparser.add_option("-t", "--timestamp", dest="notimestamp", action="store_false", default=True,
    help="Add timestamp to .pb.h and .pb.c preambles")
optparser.add_option("-q", "--quiet", dest="quiet", action="store_true", default=False,
    help="Don't print anything except errors.")
optparser.add_option("-v", "--verbose", dest="verbose", action="store_true", default=False,
    help="Print more information.")
optparser.add_option("-s", dest="settings", metavar="OPTION:VALUE", action="append", default=[],
    help="Set generator option (max_size, max_count etc.).")
# Comment/return behavior is now minimal and errno-style by default (no flags)
optparser.add_option("--protoc-opt", dest="protoc_opts", action="append", default = [], metavar="OPTION",
    help="Pass an option to protoc when compiling .proto files")
optparser.add_option("--protoc-insertion-points", dest="protoc_insertion_points", action="store_true", default=False,
    help="Include insertion point comments in output for use by custom protoc plugins")
optparser.add_option("-C", "--c-style", dest="c_style", action="store_true", default=False,
    help="Use C naming convention.")
optparser.add_option("--validate", dest="validate", action="store_true", default=False,
    help="Generate validation code for messages.")
optparser.add_option("--validate-consolidated", dest="validate_consolidated", action="store_true", default=False,
    help="Generate consolidated validation files instead of per-proto files.")
optparser.add_option("--bypass", dest="bypass", action="store_true", default=False,
    help="Generate validation code in bypass mode: collect all violations without early exit.")
optparser.add_option("--envelope-mode", dest="envelope_mode", metavar="MODE", default="oneof",
    help="Envelope detection mode: 'oneof' (enum+oneof pattern) or 'any' (google.protobuf.Any field). [default: %default]")
optparser.add_option("--envelope-name", dest="envelope_name", metavar="NAME", default=None,
    help="Name of the envelope/base message to use for filter generation. If not specified, auto-detection will be used.")
optparser.add_option("--root-message", dest="root_message", metavar="NAME", default=None,
    help="Fully qualified message name for single-root-message mode. When set, filter_tcp/filter_udp decode and validate as this message type directly, bypassing envelope/Any detection. Example: 'mypkg.Packet' or 'MyMessage'.")


def parse_custom_style(option, opt_str, value, parser):
    parts = value.rsplit(".", 1)
    if len(parts) != 2 or not all(len(part) > 0 for part in parts):
        raise OptionValueError("Invalid value for %s, must be in the form %s: %r" % (opt_str, option.metavar, value))
    setattr(parser.values, option.dest, parts)


optparser.add_option("--custom-style", dest="custom_style", type=str, metavar="MODULE.CLASS", action="callback", callback=parse_custom_style,
                     help="Use a custom naming convention from a module/class that defines the methods from the NamingStyle class to be overridden. When paired with the -C/--c-style option, the NamingStyleC class is the fallback, otherwise it's the NamingStyle class.")


def process_cmdline(args, is_plugin):
    """
    Parse and validate command line arguments.
    
    This function processes the command line arguments, sets up global state
    (Globals class), and returns parsed options and input filenames.
    
    Handles both standalone CLI mode and protoc plugin mode, with slight
    differences in error handling and help output destination.
    
    Args:
        args: List of command line arguments (excluding argv[0])
        is_plugin: True if running as protoc plugin, affects output behavior
    
    Returns:
        Tuple of (options, filenames) where options is the parsed options
        namespace and filenames is a list of input files
    
    Side Effects:
        - Sets Globals.verbose_options
        - Sets Globals.protoc_insertion_points
        - Sets Globals.naming_style
        - Loads and parses .options files into Globals.separate_options
        - May call sys.exit() for --version or on errors
    """
    options, filenames = optparser.parse_args(args)

    if options.version:
        if is_plugin:
            sys.stderr.write('%s\n' % (nanopb_version))
        else:
            print(nanopb_version)

        if options.verbose:
            proto.print_versions()

        sys.exit(0)

    if not filenames and not is_plugin:
        optparser.print_help()
        sys.exit(1)

    if options.quiet:
        options.verbose = False

    include_formats = {'quote': '#include "%s"', 'bracket': '#include <%s>'}
    options.libformat = include_formats.get(options.libformat, options.libformat)
    options.genformat = include_formats.get(options.genformat, options.genformat)

    if options.custom_style:
        module_path, class_name = options.custom_style
        module_name = os.path.splitext(os.path.basename(module_path))[0]
        if not module_path.endswith(".py"):
            module_path = module_path + ".py"

        spec = importlib.util.spec_from_file_location(module_name, module_path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)

        custom_class = getattr(module, class_name)

        class InheritNamingStyle(custom_class, NamingStyleC if options.c_style else NamingStyle):
            """Class to inherit from the custom class and then NamingStyle or NamingCStyle, in case it doesn't implement all methods."""
            pass

        Globals.naming_style = InheritNamingStyle()
    elif options.c_style:
        Globals.naming_style = NamingStyleC()

    Globals.verbose_options = options.verbose

    # Validate envelope_mode option
    if options.envelope_mode not in ('oneof', 'any'):
        sys.stderr.write("Error: --envelope-mode must be either 'oneof' or 'any', got: '%s'\n" % options.envelope_mode)
        sys.exit(1)

    if options.verbose:
        sys.stderr.write("Nanopb version %s\n" % nanopb_version)
        sys.stderr.write('Google Python protobuf library imported from %s, version %s\n'
                         % (google.protobuf.__file__, google.protobuf.__version__))

    return options, filenames


def parse_file(filename, fdesc, options):
    '''Parse a single file. Returns a ProtoFile instance.'''
    toplevel_options = nanopb_pb2.NanoPBOptions()
    for s in options.settings:
        if ':' not in s and '=' in s:
            s = s.replace('=', ':')
        text_format.Merge(s, toplevel_options)

    if not fdesc:
        data = open(filename, 'rb').read()
        fdesc = descriptor.FileDescriptorSet.FromString(data).file[0]

    # Check if there is a separate .options file
    had_abspath = False
    try:
        optfilename = options.options_file % os.path.splitext(filename)[0]
    except TypeError:
        # No %s specified, use the filename as-is
        optfilename = options.options_file
        had_abspath = True

    paths = ['.'] + options.options_path
    for p in paths:
        if os.path.isfile(os.path.join(p, optfilename)):
            optfilename = os.path.join(p, optfilename)
            if options.verbose:
                sys.stderr.write('Reading options from ' + optfilename + '\n')
            Globals.separate_options = read_options_file(open(optfilename, 'r', encoding = 'utf-8'))
            break
    else:
        # If we are given a full filename and it does not exist, give an error.
        # However, don't give error when we automatically look for .options file
        # with the same name as .proto.
        if options.verbose or had_abspath:
            sys.stderr.write('Options file not found: ' + optfilename + '\n')
        Globals.separate_options = []

    Globals.matched_namemasks = set()
    Globals.protoc_insertion_points = options.protoc_insertion_points

    # Parse the file
    file_options = get_nanopb_suboptions(fdesc, toplevel_options, Names([filename]))
    f = ProtoFile(fdesc, file_options)
    f.optfilename = optfilename

    return f

def process_file(filename, fdesc, options, other_files = {}):
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
    f = parse_file(filename, fdesc, options)

    # Check the list of dependencies, and if they are available in other_files,
    # add them to be considered for import resolving. Recursively add any files
    # imported by the dependencies.
    deps = list(f.fdesc.dependency)
    while deps:
        dep = deps.pop(0)
        if dep in other_files:
            f.add_dependency(other_files[dep])
            deps += list(other_files[dep].fdesc.dependency)

    # Decide the file names
    noext = os.path.splitext(filename)[0]
    headername = noext + options.extension + options.header_extension
    sourcename = noext + options.extension + options.source_extension

    if options.strip_path:
        headerbasename = os.path.basename(headername)
    else:
        headerbasename = headername

    # List of .proto files that should not be included in the C header file
    # even if they are mentioned in the source .proto.
    excludes = ['nanopb.proto', 'google/protobuf/descriptor.proto'] + options.exclude + list(f.file_options.exclude)
    includes = [d for d in f.fdesc.dependency if d not in excludes]

    headerdata = ''.join(f.generate_header(includes, headerbasename, options))
    sourcedata = ''.join(f.generate_source(headerbasename, options))
    
    # Generate validation files if enabled
    validate_headerdata = None
    validate_sourcedata = None
    validate_headername = None
    validate_sourcename = None
    
    if f.validate_enabled or options.validate:
        # Generate validation files
        validate_headerdata = ''.join(f.generate_validate_header(headerbasename, options))
        validate_sourcedata = ''.join(f.generate_validate_source(headerbasename, options))
        
        if validate_headerdata or validate_sourcedata:
            validate_headername = noext + '_validate' + options.header_extension
            validate_sourcename = noext + '_validate' + options.source_extension

    # Check if there were any lines in .options that did not match a member
    unmatched = [n for n,o in Globals.separate_options if n not in Globals.matched_namemasks]
    if unmatched:
        if options.error_on_unmatched:
            raise Exception("Following patterns in " + f.optfilename + " did not match any fields: "
                            + ', '.join(unmatched));
        elif not options.quiet:
            sys.stderr.write("Following patterns in " + f.optfilename + " did not match any fields: "
                            + ', '.join(unmatched) + "\n")

            if not Globals.verbose_options:
                sys.stderr.write("Use  protoc --nanopb-out=-v:.   to see a list of the field names.\n")

    result = {'headername': headername, 'headerdata': headerdata,
              'sourcename': sourcename, 'sourcedata': sourcedata}
    
    if validate_headername:
        result['validate_headername'] = validate_headername
        result['validate_headerdata'] = validate_headerdata
    if validate_sourcename:
        result['validate_sourcename'] = validate_sourcename
        result['validate_sourcedata'] = validate_sourcedata
    
    return result

def main_cli():
    '''Main function when invoked directly from the command line.'''

    options, filenames = process_cmdline(sys.argv[1:], is_plugin = False)

    if options.output_dir and not os.path.exists(options.output_dir):
        optparser.print_help()
        sys.stderr.write("\noutput_dir does not exist: %s\n" % options.output_dir)
        sys.exit(1)

    # Load .pb files into memory and compile any .proto files.
    include_path = ['-I%s' % p for p in options.options_path]
    all_fdescs = {}
    out_fdescs = {}
    for filename in filenames:
        if filename.endswith(".proto"):
            with TemporaryDirectory() as tmpdir:
                tmpname = os.path.join(tmpdir, os.path.basename(filename) + ".pb")
                args = ["protoc"] + include_path
                args += options.protoc_opts
                args += ['--include_imports', '--include_source_info', '-o' + tmpname, filename]
                status = invoke_protoc(args)
                if status != 0: sys.exit(status)
                data = open(tmpname, 'rb').read()
        else:
            data = open(filename, 'rb').read()

        fdescs = descriptor.FileDescriptorSet.FromString(data).file
        last_fdesc = fdescs[-1]

        for fdesc in fdescs:
          all_fdescs[fdesc.name] = fdesc

        out_fdescs[last_fdesc.name] = last_fdesc

    # Process any include files first, in order to have them
    # available as dependencies
    other_files = {}
    for fdesc in all_fdescs.values():
        other_files[fdesc.name] = parse_file(fdesc.name, fdesc, options)

    # Then generate the headers / sources
    for fdesc in out_fdescs.values():
        results = process_file(fdesc.name, fdesc, options, other_files)

        base_dir = options.output_dir or ''
        to_write = [
            (os.path.join(base_dir, results['headername']), results['headerdata']),
            (os.path.join(base_dir, results['sourcename']), results['sourcedata']),
        ]
        
        # Add validation files if generated
        if 'validate_headername' in results:
            to_write.append((os.path.join(base_dir, results['validate_headername']), results['validate_headerdata']))
        if 'validate_sourcename' in results:
            to_write.append((os.path.join(base_dir, results['validate_sourcename']), results['validate_sourcedata']))

        if not options.quiet:
            paths = " and ".join([x[0] for x in to_write])
            sys.stderr.write("Writing to %s\n" % paths)

        for path, data in to_write:
            dirname = os.path.dirname(path)
            if dirname and not os.path.exists(dirname):
                os.makedirs(dirname)

            with open(path, 'w', encoding='utf-8') as f:
                f.write(data)

def main_plugin():
    '''Main function when invoked as a protoc plugin.'''

    import io, sys
    if sys.platform == "win32":
        import os, msvcrt
        # Set stdin and stdout to binary mode
        msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
        msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)

    data = io.open(sys.stdin.fileno(), "rb").read()

    request = plugin_pb2.CodeGeneratorRequest.FromString(data)

    try:
        # Versions of Python prior to 2.7.3 do not support unicode
        # input to shlex.split(). Try to convert to str if possible.
        params = str(request.parameter)
    except UnicodeEncodeError:
        params = request.parameter

    if ',' not in params and ' -' in params:
        # Nanopb has traditionally supported space as separator in options
        args = shlex.split(params)
    else:
        # Protoc separates options passed to plugins by comma
        # This allows also giving --nanopb_opt option multiple times.
        lex = shlex.shlex(params)
        lex.whitespace_split = True
        lex.whitespace = ','
        lex.commenters = ''
        args = list(lex)

    optparser.usage = "protoc --nanopb_out=outdir [--nanopb_opt=option] ['--nanopb_opt=option with spaces'] file.proto"
    optparser.epilog = "Output will be written to file.pb.h and file.pb.c."

    if '-h' in args or '--help' in args:
        # By default optparser prints help to stdout, which doesn't work for
        # protoc plugins.
        optparser.print_help(sys.stderr)
        sys.exit(1)

    options, dummy = process_cmdline(args, is_plugin = True)

    response = plugin_pb2.CodeGeneratorResponse()

    # Google's protoc does not currently indicate the full path of proto files.
    # Instead always add the main file path to the search dirs, that works for
    # the common case.
    import os.path
    options.options_path.append(os.path.dirname(request.file_to_generate[0]))

    # Process any include files first, in order to have them
    # available as dependencies
    other_files = {}
    for fdesc in request.proto_file:
        other_files[fdesc.name] = parse_file(fdesc.name, fdesc, options)

    for filename in request.file_to_generate:
        for fdesc in request.proto_file:
            if fdesc.name == filename:
                results = process_file(filename, fdesc, options, other_files)

                f = response.file.add()
                f.name = results['headername']
                f.content = results['headerdata']

                f = response.file.add()
                f.name = results['sourcename']
                f.content = results['sourcedata']
                
                # Add validation files if generated
                if 'validate_headername' in results:
                    f = response.file.add()
                    f.name = results['validate_headername']
                    f.content = results['validate_headerdata']
                
                if 'validate_sourcename' in results:
                    f = response.file.add()
                    f.name = results['validate_sourcename']
                    f.content = results['validate_sourcedata']

    if hasattr(plugin_pb2.CodeGeneratorResponse, "FEATURE_PROTO3_OPTIONAL"):
        response.supported_features = plugin_pb2.CodeGeneratorResponse.FEATURE_PROTO3_OPTIONAL

    if hasattr(plugin_pb2.CodeGeneratorResponse, "FEATURE_SUPPORTS_EDITIONS"):
        response.supported_features |= plugin_pb2.CodeGeneratorResponse.FEATURE_SUPPORTS_EDITIONS
        response.minimum_edition = descriptor.EDITION_PROTO2
        response.maximum_edition = descriptor.EDITION_2024

    io.open(sys.stdout.fileno(), "wb").write(response.SerializeToString())

if __name__ == '__main__':
    # Check if we are running as a plugin under protoc
    if 'protoc-gen-' in sys.argv[0] or '--protoc-plugin' in sys.argv:
        main_plugin()
    else:
        main_cli()
