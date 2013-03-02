'''Generate header file for nanopb from a ProtoBuf FileDescriptorSet.'''
nanopb_version = "nanopb-0.2.0"

try:
    import google.protobuf.descriptor_pb2 as descriptor
except:
    print
    print "*************************************************************"
    print "*** Could not import the Google protobuf Python libraries ***"
    print "*** Try installing package 'python-protobuf' or similar.  ***"
    print "*************************************************************"
    print
    raise

try:
    import nanopb_pb2
except:
    print
    print "***************************************************************"
    print "*** Could not import the precompiled nanopb_pb2.py.         ***"
    print "*** Run 'make' in the 'generator' folder to update the file.***"
    print "***************************************************************"
    print
    raise






# ---------------------------------------------------------------------------
#                     Generation of single fields
# ---------------------------------------------------------------------------

import time
import os.path

# Values are tuple (c type, pb type)
FieldD = descriptor.FieldDescriptorProto
datatypes = {
    FieldD.TYPE_BOOL:       ('bool',     'BOOL'),
    FieldD.TYPE_DOUBLE:     ('double',   'DOUBLE'),
    FieldD.TYPE_FIXED32:    ('uint32_t', 'FIXED32'),
    FieldD.TYPE_FIXED64:    ('uint64_t', 'FIXED64'),
    FieldD.TYPE_FLOAT:      ('float',    'FLOAT'),
    FieldD.TYPE_INT32:      ('int32_t',  'INT32'),
    FieldD.TYPE_INT64:      ('int64_t',  'INT64'),
    FieldD.TYPE_SFIXED32:   ('int32_t',  'SFIXED32'),
    FieldD.TYPE_SFIXED64:   ('int64_t',  'SFIXED64'),
    FieldD.TYPE_SINT32:     ('int32_t',  'SINT32'),
    FieldD.TYPE_SINT64:     ('int64_t',  'SINT64'),
    FieldD.TYPE_UINT32:     ('uint32_t', 'UINT32'),
    FieldD.TYPE_UINT64:     ('uint64_t', 'UINT64')
}

class Names:
    '''Keeps a set of nested names and formats them to C identifier.
    You can subclass this with your own implementation.
    '''
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
        
        # Decide the C data type to use in the struct.
        if datatypes.has_key(desc.type):
            self.ctype, self.pbtype = datatypes[desc.type]
        elif desc.type == FieldD.TYPE_ENUM:
            self.pbtype = 'ENUM'
            self.ctype = names_from_type_name(desc.type_name)
            if self.default is not None:
                self.default = self.ctype + self.default
        elif desc.type == FieldD.TYPE_STRING:
            self.pbtype = 'STRING'
            if self.max_size is None:
                can_be_static = False
            else:
                self.ctype = 'char'
                self.array_decl += '[%d]' % self.max_size
        elif desc.type == FieldD.TYPE_BYTES:
            self.pbtype = 'BYTES'
            if self.max_size is None:
                can_be_static = False
            else:
                self.ctype = self.struct_name + self.name + 't'
        elif desc.type == FieldD.TYPE_MESSAGE:
            self.pbtype = 'MESSAGE'
            self.ctype = self.submsgname = names_from_type_name(desc.type_name)
        else:
            raise NotImplementedError(desc.type)
        
        if field_options.type == nanopb_pb2.FT_DEFAULT:
            if can_be_static:
                field_options.type = nanopb_pb2.FT_STATIC
            else:
                field_options.type = nanopb_pb2.FT_CALLBACK
        
        if field_options.type == nanopb_pb2.FT_STATIC and not can_be_static:
            raise Exception("Field %s is defined as static, but max_size or max_count is not given." % self.name)
        
        if field_options.type == nanopb_pb2.FT_STATIC:
            self.allocation = 'STATIC'
        elif field_options.type == nanopb_pb2.FT_CALLBACK:
            self.allocation = 'CALLBACK'
            self.ctype = 'pb_callback_t'
            self.array_decl = ''
        else:
            raise NotImplementedError(field_options.type)
    
    def __cmp__(self, other):
        return cmp(self.tag, other.tag)
    
    def __str__(self):
        if self.rules == 'OPTIONAL':
            result = '    bool has_' + self.name + ';\n'
        elif self.rules == 'REPEATED' and self.allocation == 'STATIC':
            result = '    size_t ' + self.name + '_count;\n'
        else:
            result = ''
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
        
        if declaration_only:
            return 'extern const %s %s_default%s;' % (ctype, self.struct_name + self.name, array_decl)
        else:
            return 'const %s %s_default%s = %s;' % (ctype, self.struct_name + self.name, array_decl, default)
    
    def pb_field_t(self, prev_field_name):
        '''Return the pb_field_t initializer to use in the constant array.
        prev_field_name is the name of the previous field or None.
        '''
        result  = '    PB_FIELD(%3d, ' % self.tag
        result += '%-8s, ' % self.pbtype
        result += '%s, ' % self.rules
        result += '%s, ' % self.allocation
        result += '%s, ' % self.struct_name
        result += '%s, ' % self.name
        result += '%s, ' % (prev_field_name or self.name)
        
        if self.pbtype == 'MESSAGE':
            result += '&%s_fields)' % self.submsgname
        elif self.default is None:
            result += '0)'
        elif self.pbtype in ['BYTES', 'STRING'] and self.allocation != 'STATIC':
            result += '0)' # Arbitrary size default values not implemented
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






# ---------------------------------------------------------------------------
#                   Generation of messages (structures)
# ---------------------------------------------------------------------------


class Message:
    def __init__(self, names, desc, message_options):
        self.name = names
        self.fields = []
        
        for f in desc.field:
            field_options = get_nanopb_suboptions(f, message_options)
            if field_options.type != nanopb_pb2.FT_IGNORE:
                self.fields.append(Field(self.name, f, field_options))
        
        self.packed = message_options.packed_struct
        self.ordered_fields = self.fields[:]
        self.ordered_fields.sort()

    def get_dependencies(self):
        '''Get list of type names that this structure refers to.'''
        return [str(field.ctype) for field in self.fields]
    
    def __str__(self):
        result = 'typedef struct _%s {\n' % self.name
        result += '\n'.join([str(f) for f in self.ordered_fields])
        result += '\n}'
        
        if self.packed:
            result += ' pb_packed'
        
        result += ' %s;' % self.name
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

def parse_file(fdesc, file_options):
    '''Takes a FileDescriptorProto and returns tuple (enum, messages).'''
    
    enums = []
    messages = []
    
    if fdesc.package:
        base_name = Names(fdesc.package.split('.'))
    else:
        base_name = Names()
    
    for enum in fdesc.enum_type:
        enum_options = get_nanopb_suboptions(enum, file_options)
        enums.append(Enum(base_name, enum, enum_options))
    
    for names, message in iterate_messages(fdesc, base_name):
        message_options = get_nanopb_suboptions(message, file_options)
        messages.append(Message(names, message, message_options))
        for enum in message.enum_type:
            enum_options = get_nanopb_suboptions(enum, message_options)
            enums.append(Enum(names, enum, enum_options))
    
    # Fix field default values where enum short names are used.
    for enum in enums:
        if not enum.options.long_names:
            for message in messages:
                for field in message.fields:
                    if field.default in enum.value_longnames:
                        idx = enum.value_longnames.index(field.default)
                        field.default = enum.values[idx][0]
    
    return enums, messages

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

def generate_header(dependencies, headername, enums, messages, options):
    '''Generate content for a header file.
    Generates strings, which should be concatenated and stored to file.
    '''
    
    yield '/* Automatically generated nanopb header */\n'
    yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
    
    symbol = make_identifier(headername)
    yield '#ifndef _PB_%s_\n' % symbol
    yield '#define _PB_%s_\n' % symbol
    yield '#include <pb.h>\n\n'
    
    for dependency in dependencies:
        noext = os.path.splitext(dependency)[0]
        yield '#include "%s.%s.h"\n' % (noext,options.extension)
    
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
        
    yield '/* Default values for struct fields */\n'
    for msg in messages:
        yield msg.default_decl(True)
    yield '\n'
    
    yield '/* Struct field encoding specification for nanopb */\n'
    for msg in messages:
        yield msg.fields_declaration() + '\n'
    
    yield '\n#ifdef __cplusplus\n'
    yield '} /* extern "C" */\n'
    yield '#endif\n'
    
    # End of header
    yield '\n#endif\n'

def generate_source(headername, enums, messages):
    '''Generate content for a source file.'''
    
    yield '/* Automatically generated nanopb constant definitions */\n'
    yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
    yield '#include "%s"\n\n' % headername
    
    for msg in messages:
        yield msg.default_decl(False)
    
    yield '\n\n'
    
    for msg in messages:
        yield msg.fields_definition() + '\n\n'
        
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
    
    # Add checks for numeric limits
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
        
        if worst < 65536:
            yield '#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)\n'
            if worst > 255:
                yield '#error Field descriptor for %s is too large. Define PB_FIELD_16BIT to fix this.\n' % worst_field
            else:
                assertion = ' && '.join(str(c) + ' < 256' for c in checks)
                msgs = '_'.join(str(n) for n in checks_msgnames)
                yield 'STATIC_ASSERT((%s), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_%s)\n'%(assertion,msgs)
            yield '#endif\n\n'
        
        if worst > 65535 or checks:
            yield '#if !defined(PB_FIELD_32BIT)\n'
            if worst > 65535:
                yield '#error Field descriptor for %s is too large. Define PB_FIELD_32BIT to fix this.\n' % worst_field
            else:
                assertion = ' && '.join(str(c) + ' < 65536' for c in checks)
                msgs = '_'.join(str(n) for n in checks_msgnames)
                yield 'STATIC_ASSERT((%s), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_%s)\n'%(assertion,msgs)
            yield '#endif\n'
    
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
#                         Command line interface
# ---------------------------------------------------------------------------

import sys
import os.path    
from optparse import OptionParser
import google.protobuf.text_format as text_format

optparser = OptionParser(
    usage = "Usage: nanopb_generator.py [options] file.pb ...",
    epilog = "Compile file.pb from file.proto by: 'protoc -ofile.pb file.proto'. " +
             "Output will be written to file.pb.h and file.pb.c.")
optparser.add_option("-x", dest="exclude", metavar="FILE", action="append", default=[],
    help="Exclude file from generated #include list.")
optparser.add_option("-e", "--extension", dest="extension", metavar="EXTENSION", default="pb",
    help="use extension instead of 'pb' for generated files.")
optparser.add_option("-q", "--quiet", dest="quiet", action="store_true", default=False,
    help="Don't print anything except errors.")
optparser.add_option("-v", "--verbose", dest="verbose", action="store_true", default=False,
    help="Print more information.")
optparser.add_option("-s", dest="settings", metavar="OPTION:VALUE", action="append", default=[],
    help="Set generator option (max_size, max_count etc.).")

def get_nanopb_suboptions(subdesc, options):
    '''Get copy of options, and merge information from subdesc.'''
    new_options = nanopb_pb2.NanoPBOptions()
    new_options.CopyFrom(options)
    
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
    
    return new_options

def process(filenames, options):
    '''Process the files given on the command line.'''
    
    if not filenames:
        optparser.print_help()
        return False
    
    if options.quiet:
        options.verbose = False
    
    toplevel_options = nanopb_pb2.NanoPBOptions()
    for s in options.settings:
        text_format.Merge(s, toplevel_options)
    
    for filename in filenames:
        data = open(filename, 'rb').read()
        fdesc = descriptor.FileDescriptorSet.FromString(data)
        
        file_options = get_nanopb_suboptions(fdesc.file[0], toplevel_options)
        
        if options.verbose:
            print "Options for " + filename + ":"
            print text_format.MessageToString(file_options)
        
        enums, messages = parse_file(fdesc.file[0], file_options)
        
        noext = os.path.splitext(filename)[0]
        headername = noext + '.' + options.extension + '.h'
        sourcename = noext + '.' + options.extension + '.c'
        headerbasename = os.path.basename(headername)
        
        if not options.quiet:
            print "Writing to " + headername + " and " + sourcename
        
        # List of .proto files that should not be included in the C header file
        # even if they are mentioned in the source .proto.
        excludes = ['nanopb.proto', 'google/protobuf/descriptor.proto'] + options.exclude
        dependencies = [d for d in fdesc.file[0].dependency if d not in excludes]
        
        header = open(headername, 'w')
        for part in generate_header(dependencies, headerbasename, enums, messages, options):
            header.write(part)

        source = open(sourcename, 'w')
        for part in generate_source(headerbasename, enums, messages):
            source.write(part)

    return True

if __name__ == '__main__':
    options, filenames = optparser.parse_args()
    status = process(filenames, options)
    
    if not status:
        sys.exit(1)
    
