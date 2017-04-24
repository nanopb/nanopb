#!/usr/bin/env python

from __future__ import unicode_literals

'''Generate header file for nanopb from a ProtoBuf FileDescriptorSet.'''
nanopb_version = "nanopb-0.3.8-dev"

import sys
import re
from functools import reduce

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

# String types (for python 2 / python 3 compatibility)
try:
    strtypes = (unicode, str)
except NameError:
    strtypes = (str, )

class Names:
    '''Keeps a set of nested names and formats them to C identifier.'''
    def __init__(self, parts = ()):
        if isinstance(parts, Names):
            parts = parts.parts
        self.parts = tuple(parts)

    def __str__(self):
        return '_'.join(self.parts)

    def __add__(self, other):
        if isinstance(other, strtypes):
            return Names(self.parts + (other,))
        elif isinstance(other, tuple):
            return Names(self.parts + other)
        else:
            raise ValueError("Name parts should be of type str")

    def __eq__(self, other):
        return isinstance(other, Names) and self.parts == other.parts

    def __hash__(self):
        '''Hash function required for creating sets of Names'''
        return hash(str(self))

def names_from_type_name(type_name):
    '''Parse Names() from FieldDescriptorProto type_name'''
    if type_name[0] != '.':
        raise NotImplementedError("Lookup of non-absolute type names is not supported")
    return Names(type_name[1:].split('.'))


# ---------------------------------------------------------------------------
#                   Generation of Methods
# ---------------------------------------------------------------------------

class Method:
    def __init__(self, names, desc, method_options):
        self.full_name = names
        self.name = desc.name
        self.input = None
        self.output = None

        self.server_streaming = desc.server_streaming
        self.client_streaming = desc.client_streaming

        if hasattr(desc, 'input_type'):
            self.input = names_from_type_name(desc.input_type)
        else:
            sys.stderr.write('''
                 *************************************************************
                 ***  Method withoud argument not supported yet            ***
                 *************************************************************
            ''' + '\n')
            raise

        if hasattr(desc, 'output_type'):
            self.output = names_from_type_name(desc.output_type)
        else:
            sys.stderr.write('''
                 *************************************************************
                 ***  Method withoud response not supported yet            ***
                 *************************************************************
            ''' + '\n')
            raise

        # print('new method named: {}, input: {}, output: {}'.format(self.name,
        #                                                         self.input,
        #                                                         self.output))

    def get_declaration(self):
        result = 'extern ng_method_t {}_method;'.format(self.full_name)
        return result

    def get_definition(self):
        result = ''
        # result += 'DEFINE_FILL_WITH_ZEROS_FUNCTION({})\n'.format(self.input)
        # result += 'DEFINE_FILL_WITH_ZEROS_FUNCTION({})\n'.format(self.output)
        # result += '\n'
        result += 'ng_method_t {}_method = {{\n'.format(self.full_name)
        result += '    "{}",\n'.format(self.name)           # name
        result += '    0,\n'  # TODO place method id option here # hasg
        # result += '    NULL,\n'                           # handler
        result += '    NULL,\n'                             # callback
        # result += '    NULL,\n'                             # request_holder
        result += '    NULL,\n'                             # context
        result += '    {}_fields,\n'.format(self.input)     # request_fields
        result += '    &FILL_WITH_ZEROS_FUNCTION_NAME({}),\n'.format(self.input) # request_fillWithZeros
        # result += '    NULL,\n'                             # response_holder
        result += '    {}_fields,\n'.format(self.output)    # response_fields
        result += '    &FILL_WITH_ZEROS_FUNCTION_NAME({}),\n'.format(self.output) # response_fillWithZeros

        if self.server_streaming:
            result += '    true,\n'
        else:
            result += '    false,\n'

        if self.client_streaming:
            result += '    true,\n'
        else:
            result += '    false,\n'

        result += '    {NULL, NULL},\n'                     # cleanup
        result += '    NULL,\n'                             # next
        result += '};'
        return result

# ---------------------------------------------------------------------------
#                   Generation of Services
# ---------------------------------------------------------------------------

class Service:
    def __init__(self, names, desc, service_options):
        self.name = names
        self.methods = []

        for name, method in iterate_methods(desc, None):
            self.methods.append(Method('_'.join([self.name, name]), method, method_options=None))

    def get_declaration(self):
        result = 'extern ng_service_t {}_service;'.format(self.name)
        return result

    def get_definition(self):
        result =  'ng_service_t {}_service = {{\n'.format(self.name)
        result += '    "{}",\n'.format(self.name)
        result += '    0,\n'
        result += '    NULL,\n'
        result += '};'
        return result

    def get_methods(self):
        return self.methods


def iterate_servies(desc, names=Names()):
    '''Recursively find all services.
    for each, yield name, ServiceDescriptorProto
    '''
    for service in desc.service:
        sub_name = service.name
        yield sub_name, service

def iterate_methods(desc, names=Names()):
    if hasattr(desc, 'method'):
        for method in desc.method:
            sub_name = method.name
            yield sub_name, method

def toposort2(data):
    '''Topological sort.
    From http://code.activestate.com/recipes/577413-topological-sort/
    This function is under the MIT license.
    '''
    for k, v in list(data.items()):
        v.discard(k) # Ignore self dependencies
    extra_items_in_deps = reduce(set.union, list(data.values()), set()) - set(data.keys())
    data.update(dict([(item, set()) for item in extra_items_in_deps]))
    while True:
        ordered = set(item for item,dep in list(data.items()) if not dep)
        if not ordered:
            break
        for item in sorted(ordered):
            yield item
        data = dict([(item, (dep - ordered)) for item,dep in list(data.items())
                if item not in ordered])
    assert not data, "A cyclic dependency exists amongst %r" % data

# def sort_dependencies(messages):
#     '''Sort a list of Messages based on dependencies.'''
#     dependencies = {}
#     message_by_name = {}
#     for message in messages:
#         dependencies[str(message.name)] = set(message.get_dependencies())
#         message_by_name[str(message.name)] = message
#
#     for msgname in toposort2(dependencies):
#         if msgname in message_by_name:
#             yield message_by_name[msgname]

def make_identifier(headername):
    '''Make #ifndef identifier that contains uppercase A-Z and digits 0-9'''
    result = ""
    for c in headername.upper():
        if c.isalnum():
            result += c
        else:
            result += '_'
    return result

class ProtoFile:
    def __init__(self, fdesc, file_options):
        '''Takes a FileDescriptorProto and parses it.'''
        self.fdesc = fdesc
        self.file_options = file_options
        self.dependencies = {}
        self.parse()

        # Some of types used in this file probably come from the file itself.
        # Thus it has implicit dependency on itself.
        # self.add_dependency(self)

    def parse(self):
        self.enums = []
        self.services = []

        if self.fdesc.package:
            base_name = Names(self.fdesc.package.split('.'))
        else:
            base_name = Names()

        for names, service in iterate_servies(self.fdesc, base_name):
            self.services.append(Service(names, service, service_options=None))


    def add_dependency(self, other):
        for enum in other.enums:
            self.dependencies[str(enum.names)] = enum

        for msg in other.messages:
            self.dependencies[str(msg.name)] = msg

        # Fix field default values where enum short names are used.
        for enum in other.enums:
            if not enum.options.long_names:
                for message in self.messages:
                    for field in message.fields:
                        if field.default in enum.value_longnames:
                            idx = enum.value_longnames.index(field.default)
                            field.default = enum.values[idx][0]

        # Fix field data types where enums have negative values.
        for enum in other.enums:
            if not enum.has_negative():
                for message in self.messages:
                    for field in message.fields:
                        if field.pbtype == 'ENUM' and field.ctype == enum.names:
                            field.pbtype = 'UENUM'

    def get_all_used_messages(self):
        methods = set()
        if self.services:
            for service in self.services:
                if service.methods:
                    for m in service.methods:
                        methods.add(m.input)
                        # print(str(m.input))
                        methods.add(m.output)
                        # print(m.output)
        return methods

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
        yield '#ifndef NG_%s_INCLUDED\n' % symbol
        yield '#define NG_%s_INCLUDED\n' % symbol
        try:
            yield options.libformat % ('pb.h')
            yield options.libformat % ('ng.h')
            yield options.libformat % ('ng_server.h')
        except TypeError:
            # no %s specified - use whatever was passed in as options.libformat
            yield options.libformat
        yield '\n'

        for incfile in includes:
            noext = os.path.splitext(incfile)[0]
            yield options.genformat % (noext + options.extension + '.h')
            yield '\n'
        # yield '#include "ng.h"\n'

        yield '/* @@protoc_insertion_point(includes) */\n'

        yield '#if PB_PROTO_HEADER_VERSION != 30\n'
        yield '#error Regenerate this file with the current version of nanopb generator.\n'
        yield '#endif\n'
        yield '\n'

        yield '#ifdef __cplusplus\n'
        yield 'extern "C" {\n'
        yield '#endif\n\n'

        yield '/* Services definitions */\n'
        for service in self.services:
            yield service.get_declaration()
            yield '\n'
            for method in service.get_methods():
                yield method.get_declaration()
                yield '\n'
            yield 'ng_GrpcStatus_t {}_service_init();\n'.format(service.name)
        yield '\n'
        yield '#ifdef __cplusplus\n'
        yield '} /* extern "C" */\n'
        yield '#endif\n'

        # End of header
        yield '/* @@protoc_insertion_point(eof) */\n'
        yield '\n#endif\n'

    def generate_source(self, headername, options):
        '''Generate content for a source file.'''

        yield '/* Automatically generated nanopb constant definitions */\n'
        if options.notimestamp:
            yield '/* Generated by %s */\n\n' % (nanopb_version)
        else:
            yield '/* Generated by %s at %s. */\n\n' % (nanopb_version, time.asctime())
        yield options.genformat % (headername)
        yield '\n'
        yield '/* @@protoc_insertion_point(includes) */\n'

        yield '#if PB_PROTO_HEADER_VERSION != 30\n'
        yield '#error Regenerate this file with the current version of nanopb generator.\n'
        yield '#endif\n'
        yield '\n'

        yield '#ifdef NG_USE_DEFAULT_CONTEXT\n'
        yield '\n'
        for service in self.services:
            for method in service.methods:
                yield '{} {}_DefaultRequest;\n'.format(method.input, method.full_name)
                yield '{} {}_DefaultResponse;\n'.format(method.output, method.full_name)
                yield 'ng_methodContext_t {}_DefaultContext;\n'.format(method.full_name)
                yield '\n'
        yield '#endif\n'

        yield '\n'
        for msg in self.get_all_used_messages():
            yield 'DEFINE_FILL_WITH_ZEROS_FUNCTION({})\n'.format(msg)
        yield '\n'
        if self.services:
            for service in self.services:
                yield service.get_definition()
                yield '\n\n'
                for method in service.get_methods():
                    yield method.get_definition()
                    yield '\n\n'
                yield '\n'
        for service in self.services:
            yield 'ng_GrpcStatus_t {}_service_init(){{\n'.format(service.name)
            for method in service.methods:
                yield '    ng_addMethodToService(&{}_service, &{}_method);\n'.format(service.name, method.full_name)

            yield '#ifdef NG_USE_DEFAULT_CONTEXT\n'
            for method in service.methods:
                yield '    {}_DefaultContext.request = (void*)&{}_DefaultRequest;\n'.format(method.full_name, method.full_name)
                yield '    {}_DefaultContext.response = (void*)&{}_DefaultResponse;\n'.format(method.full_name, method.full_name)
                yield '    ng_setMethodContext(&{}_method, &{}_DefaultContext);\n'.format(method.full_name, method.full_name)
            yield '#endif\n'
            yield '    return 0;\n'
            yield '}\n'
            yield '\n'
        yield '/* @@protoc_insertion_point(eof) */\n'

# ---------------------------------------------------------------------------
#                    Options parsing for the .proto files
# ---------------------------------------------------------------------------

from fnmatch import fnmatch

def read_options_file(infile):
    '''Parse a separate options file to list:
        [(namemask, options), ...]
    '''
    results = []
    data = infile.read()
    data = re.sub('/\*.*?\*/', '', data, flags = re.MULTILINE)
    data = re.sub('//.*?$', '', data, flags = re.MULTILINE)
    data = re.sub('#.*?$', '', data, flags = re.MULTILINE)
    for i, line in enumerate(data.split('\n')):
        line = line.strip()
        if not line:
            continue

        parts = line.split(None, 1)

        if len(parts) < 2:
            sys.stderr.write("%s:%d: " % (infile.name, i + 1) +
                             "Option lines should have space between field name and options. " +
                             "Skipping line: '%s'\n" % line)
            continue

        opts = nanopb_pb2.NanoPBOptions()

        try:
            text_format.Merge(parts[1], opts)
        except Exception as e:
            sys.stderr.write("%s:%d: " % (infile.name, i + 1) +
                             "Unparseable option line: '%s'. " % line +
                             "Error: %s\n" % str(e))
            continue
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

    if hasattr(subdesc, 'syntax') and subdesc.syntax == "proto3":
        new_options.proto3 = True

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
             "Output will be written to file.ng.h and file.ng.c.")
optparser.add_option("-x", dest="exclude", metavar="FILE", action="append", default=[],
    help="Exclude file from generated #include list.")

optparser.add_option("-e", "--extension", dest="extension", metavar="EXTENSION", default=".pb",
    help="Set extension of coresponding nanopb files to be included. [default: %default]")
optparser.add_option("-g", "--grpc_extension", dest="grpc_extension", metavar="EXTENSION", default=".ng",
    help="Set extension to use instead of '.ng' for generated files. [default: %default]")

optparser.add_option("-f", "--options-file", dest="options_file", metavar="FILE", default="%s.options",
    help="Set name of a separate generator options file.")
optparser.add_option("-I", "--options-path", dest="options_path", metavar="DIR",
    action="append", default = [],
    help="Search for .options files additionally in this path")
optparser.add_option("-D", "--output-dir", dest="output_dir",
                     metavar="OUTPUTDIR", default=None,
                     help="Output directory of .pb.h and .pb.c files")
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

def parse_file(filename, fdesc, options):
    '''Parse a single file. Returns a ProtoFile instance.'''
    toplevel_options = nanopb_pb2.NanoPBOptions()
    for s in options.settings:
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
            Globals.separate_options = read_options_file(open(optfilename, "rU"))
            break
    else:
        # If we are given a full filename and it does not exist, give an error.
        # However, don't give error when we automatically look for .options file
        # with the same name as .proto.
        if options.verbose or had_abspath:
            sys.stderr.write('Options file not found: ' + optfilename + '\n')
        Globals.separate_options = []

    Globals.matched_namemasks = set()

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

    # Provide dependencies if available
    # for dep in f.fdesc.dependency:
    #     if dep in other_files:
    #         f.add_dependency(other_files[dep])

    # Decide the file names
    noext = os.path.splitext(filename)[0]
    headername = noext + options.grpc_extension + '.h'
    sourcename = noext + options.grpc_extension + '.c'
    headerbasename = os.path.basename(headername)

    # List of .proto files that should not be included in the C header file
    # even if they are mentioned in the source .proto.
    excludes = ['nanopb.proto', 'google/protobuf/descriptor.proto'] + options.exclude
    includes = [d for d in f.fdesc.dependency if d not in excludes]
    includes.append(os.path.split(noext)[-1])

    headerdata = ''.join(f.generate_header(includes, headerbasename, options))
    sourcedata = ''.join(f.generate_source(headerbasename, options))

    # Check if there were any lines in .options that did not match a member
    unmatched = [n for n,o in Globals.separate_options if n not in Globals.matched_namemasks]
    if unmatched and not options.quiet:
        sys.stderr.write("Following patterns in " + f.optfilename + " did not match any fields: "
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

    if options.output_dir and not os.path.exists(options.output_dir):
        optparser.print_help()
        sys.stderr.write("\noutput_dir does not exist: %s\n" % options.output_dir)
        sys.exit(1)


    Globals.verbose_options = options.verbose
    for filename in filenames:
        results = process_file(filename, None, options)

        base_dir = options.output_dir or ''
        to_write = [
            (os.path.join(base_dir, results['headername']), results['headerdata']),
            (os.path.join(base_dir, results['sourcename']), results['sourcedata']),
        ]

        if not options.quiet:
            paths = " and ".join([x[0] for x in to_write])
            sys.stderr.write("Writing to %s\n" % paths)

        for path, data in to_write:
            with open(path, 'w') as f:
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

    import shlex
    args = shlex.split(params)
    options, dummy = optparser.parse_args(args)

    Globals.verbose_options = options.verbose

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

    io.open(sys.stdout.fileno(), "wb").write(response.SerializeToString())

if __name__ == '__main__':
    # Check if we are running as a plugin under protoc
    if 'protoc-gen-' in sys.argv[0] or '--protoc-plugin' in sys.argv:
        main_plugin()
    else:
        main_cli()
