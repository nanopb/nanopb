#!/bin/env python3
'''This script converts user code from older nanopb versions
to the latest API. Old code can usually be compiled without
changes by defining PB_API_VERSION, but this script modifies
the code to match latest conventions.

Following pip packages are required:
tree-sitter, tree-sitter-c
'''

from typing import Callable, Generator
from tree_sitter import Language, Node, Parser, Range, QueryCursor, Query
import re
import tree_sitter_c

C_LANGUAGE = Language(tree_sitter_c.language())

# Number of overlapping matches to search for.
# This is needed when same variable name is referenced multiple times
# but the replacement pattern only matches one at a time.
max_overlap = 10

class RERule:
    '''Code modification rule using Python regex.
    Less robust than TSRules but sometimes easier to use.
    Use e.g. https://regex101.com/ to debug.
    '''

    def __init__(self, pattern, repl, flags = re.MULTILINE, repeat = max_overlap):
        self.pattern = pattern
        self.repl = repl
        self.flags = flags
        self.repeat = repeat
    
    def __str__(self):
        return 'RERule(%s,%s)' % (repr(self.pattern), repr(self.repl))

    def run(self, source):
        try:
            text = source.decode()

            for i in range(self.repeat):
                prev_text = text
                text = re.sub(self.pattern, self.repl, text, flags = self.flags)
                if text == prev_text:
                    break

            return text.encode()
        except:
            sys.stderr.write("In " + str(self) + ":\n")
            raise

class TSRule:
    '''Code modification rule using tree-sitter C language parser.
    Takes an S-expression to search for in the parsed source code,
    and a replacement text for it.

    The replacement text goes in place of a node identified by "@node"
    in the expression. It can refer to variables assigned by expression.

    Alternatively multiple nodes can be replaced if a dictionary is
    given as the replacement.

    To find out node names, use tree-sitter command line tool:
    tree-sitter parse --cst --no-ranges file.c

    or the online tool:
    https://tree-sitter.github.io/tree-sitter/7-playground.html
    '''
    
    def __init__(self, query, replacement):
        self.query = Query(C_LANGUAGE, query)

        if isinstance(replacement, dict):
            self.replacements = replacement
        else:
            self.replacements = {'node': replacement}

    def run(self, source):
        c_parser = Parser(C_LANGUAGE)
        tree = c_parser.parse(source)
        cursor = QueryCursor(self.query)
        mods = []

        # Collect all source code modifications from this rule
        for pid, match in cursor.matches(tree.root_node):
            args = {}
            for key, val in match.items():
                args[key] = val[0].text.decode()
            
            for nodename, replacement in self.replacements.items():
                to_replace = match[nodename][0]
                    
                if '\n' in replacement:
                    # Figure out indentation
                    node_start = to_replace.range.start_byte
                    prev_nl = source.rfind(b'\n', 0, node_start)
                    if prev_nl > 0:
                        line = source[prev_nl:node_start].decode()
                        indent = line[:(len(line)-len(line.lstrip()))].strip('\n')
                        replacement = replacement.replace('\n', '\n' + indent)
                
                new_text = replacement.format(**args)
                mods.append((to_replace.range, new_text.encode()))
        
        # Apply the modifications in order
        mods.sort(key = lambda m: m[0].start_byte)
        offset = 0
        last_pos = 0
        for range, new_text in mods:
            if range.start_byte < last_pos:
                sys.stderr.write("Warning: overlapping replacement %s\n" % repr(new_text))
                continue

            source = (source[:range.start_byte + offset] +
                    new_text +
                    source[range.end_byte + offset:])
            offset += len(new_text) - (range.end_byte - range.start_byte)
            last_pos = range.end_byte
        
        return source

# Rule definitions for API updates

RULES = [

# pb_istream_t -> pb_decode_ctx_t
TSRule('((type_identifier) @node (#eq? @node "pb_istream_t"))', 'pb_decode_ctx_t'),

# pb_decode_ctx_t stream = pb_istream_from_buffer(...)
# -> pb_init_decode_ctx(&stream, ...)
TSRule('''(
    (declaration
        type: (type_identifier) @type
        declarator: (init_declarator
            declarator: (identifier) @var
            value: (call_expression
                function: (identifier) @func
                arguments: (argument_list . (_) @buf . (_) @len .)
            )
        )
    ) @node
    (#eq? @type "pb_decode_ctx_t")
    (#eq? @func "pb_istream_from_buffer")
    )''',
    '{type} {var};\n' +
    'pb_init_decode_ctx_for_buffer(&{var}, {buf}, {len});'),

# stream = pb_istream_from_buffer(...)
# -> pb_init_decode_ctx(&stream, ...)
TSRule('''(
     (assignment_expression
        left: (identifier) @var
        right: (call_expression
                function: (identifier) @func
                arguments: (argument_list . (_) @buf . (_) @len .)
            )
    ) @node
    (#eq? @func "pb_istream_from_buffer")
    )''',
    'pb_init_decode_ctx_for_buffer(&{var}, {buf}, {len})'),

# pb_ostream_t -> pb_encode_ctx_t
TSRule('((type_identifier) @node (#eq? @node "pb_ostream_t"))', 'pb_encode_ctx_t'),

# pb_encode_ctx_t stream = pb_ostream_from_buffer(...)
# -> pb_init_encode_ctx(&stream, ...)
TSRule('''(
    (declaration
        type: (type_identifier) @type
        declarator: (init_declarator
            declarator: (identifier) @var
            value: (call_expression
                function: (identifier) @func
                arguments: (argument_list . (_) @buf . (_) @len .)
            )
        )
    ) @node
    (#eq? @type "pb_encode_ctx_t")
    (#eq? @func "pb_ostream_from_buffer")
    )''',
    '{type} {var};\n' +
    'pb_init_encode_ctx_for_buffer(&{var}, {buf}, {len});'),

# stream = pb_ostream_from_buffer(...)
# -> pb_init_encode_ctx(&stream, ...)
TSRule('''(
     (assignment_expression
        left: (identifier) @var
        right: (call_expression
                function: (identifier) @func
                arguments: (argument_list . (_) @buf . (_) @len .)
            )
    ) @node
    (#eq? @func "pb_ostream_from_buffer")
    )''',
    'pb_init_encode_ctx_for_buffer(&{var}, {buf}, {len})'),

# pb_istream_t stream = {func, state, size}
# -> pb_init_decode_ctx_for_callback(...)
TSRule('''(
    (declaration
        type: (type_identifier) @type
        declarator: (init_declarator
                     declarator: (identifier) @var
                     value: (initializer_list _ . (_) @func . (_) @state . (_) @size )
        )
    ) @node
    (#eq? @type "pb_decode_ctx_t")
    )''',
    'pb_decode_ctx_t {var};\n' +
    'pb_init_decode_ctx_for_callback(&{var}, {func}, {state}, {size}, NULL, 0);'
),

# pb_ostream_t stream = {func, state, size}
# -> pb_init_encode_ctx_for_callback(...)
TSRule('''(
    (declaration
        type: (type_identifier) @type
        declarator: (init_declarator
                     declarator: (identifier) @var
                     value: (initializer_list _ . (_) @func . (_) @state . (_) @size )
        )
    ) @node
    (#eq? @type "pb_encode_ctx_t")
    )''',
    'pb_encode_ctx_t {var};\n' +
    'pb_init_encode_ctx_for_callback(&{var}, {func}, {state}, {size}, NULL, 0);'
),

# SIZE_MAX to PB_SIZE_MAX in pb_init_decode_ctx_for_callback()
TSRule('''(
        (call_expression
            function: (identifier) @func
            arguments: (argument_list _ . (_) . (_) . (_) . (identifier) @size)
        ) @node
        (#eq? @func "pb_init_decode_ctx_for_callback")
        (#eq? @size "SIZE_MAX")
        )''',
        {'size': 'PB_SIZE_MAX'}
),

# SIZE_MAX to PB_SIZE_MAX in pb_init_encode_ctx_for_callback()
TSRule('''(
        (call_expression
            function: (identifier) @func
            arguments: (argument_list _ . (_) . (_) . (_) . (identifier) @size)
        ) @node
        (#eq? @func "pb_init_encode_ctx_for_callback")
        (#eq? @size "SIZE_MAX")
        )''',
        {'size': 'PB_SIZE_MAX'}
),

# pb_decode_delimited() -> pb_decode() and set flags
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_delimited\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_DECODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_decode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_delimited\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_DECODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

# Same for pb_encode_delimited()
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_encode_delimited\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_ENCODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_encode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_encode_delimited\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_ENCODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_encode(\g<ctx>,\g<args>)'),

# pb_decode_nullterminated() -> pb_decode() and set flags
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_nullterminated\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_DECODE_CTX_FLAG_NULLTERMINATED;\n' +
       r'\g<tab>\g<prefix>pb_decode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_nullterminated\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_DECODE_CTX_FLAG_NULLTERMINATED;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

# Same for pb_encode_nullterminated()
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_encode_nullterminated\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_ENCODE_CTX_FLAG_NULLTERMINATED;\n' +
       r'\g<tab>\g<prefix>pb_encode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_encode_nullterminated\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_ENCODE_CTX_FLAG_NULLTERMINATED;\n' +
       r'\g<tab>\g<prefix>pb_encode(\g<ctx>,\g<args>)'),

# pb_decode_noinit() -> pb_decode() and set flags
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_noinit\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_DECODE_CTX_FLAG_NOINIT;\n' +
       r'\g<tab>\g<prefix>pb_decode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_noinit\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_DECODE_CTX_FLAG_NOINIT;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

# PB_DECODE flags to PB_DECODE_CTX flags
TSRule('((identifier) @node (#eq? @node "PB_DECODE_DELIMITED"))',       'PB_DECODE_CTX_FLAG_DELIMITED'),
TSRule('((identifier) @node (#eq? @node "PB_DECODE_NULLTERMINATED"))',  'PB_DECODE_CTX_FLAG_NULLTERMINATED'),
TSRule('((identifier) @node (#eq? @node "PB_DECODE_NOINIT"))',          'PB_DECODE_CTX_FLAG_NOINIT'),

# PB_ENCODE flags to PB_ENCODE_CTX flags
TSRule('((identifier) @node (#eq? @node "PB_ENCODE_DELIMITED"))',       'PB_ENCODE_CTX_FLAG_DELIMITED'),
TSRule('((identifier) @node (#eq? @node "PB_ENCODE_NULLTERMINATED"))',  'PB_ENCODE_CTX_FLAG_NULLTERMINATED'),

# pb_decode_ex() -> pb_decode() and set flags
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_ex\(&(?P<ctx>[^,]+),(?P<args>.+),\s*(?P<flags>[^\)]+)\s*\)',
       r'\g<tab>\g<ctx>.flags |= \g<flags>;\n' +
       r'\g<tab>\g<prefix>pb_decode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_delimited\((?P<ctx>[^,]+),(?P<args>.+),\s*(?P<flags>[^\)]+)\s*\)',
       r'\g<tab>(\g<ctx>)->flags |= \g<flags>;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

# pb_encode_ex() -> pb_encode() and set flags
RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_encode_ex\(&(?P<ctx>[^,]+),(?P<args>.+),\s*(?P<flags>[^\)]+)\s*\)',
       r'\g<tab>\g<ctx>.flags |= \g<flags>;\n' +
       r'\g<tab>\g<prefix>pb_encode(&\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>[ \t]*)(?P<prefix>.*)pb_decode_delimited\((?P<ctx>[^,]+),(?P<args>.+),\s*(?P<flags>[^\)]+)\s*\)',
       r'\g<tab>(\g<ctx>)->flags |= \g<flags>;\n' +
       r'\g<tab>\g<prefix>pb_encode(\g<ctx>,\g<args>)'),

# pb_make_string_substream(stream, substream)
# -> pb_decode_open_substream(stream, &old_length)
RERule(r'pb_decode_ctx_t\s*(?P<substream>[^\s;]+)\b[^;]*;' +
       r'(?P<ctx1>.*)' +
       r'pb_make_string_substream\(\s*(?P<stream>[^\s,]+)\s*,\s*&(?P=substream)\s*\)' +
       r'(?P<ctx2>.*)pb_close_string_substream\(\s*(?P=stream)\s*,\s*&(?P=substream)\s*\)',

       r'pb_size_t old_length; /* API_UPDATER_TODO: replace &\g<substream> -> \g<stream> */' +
       r'\g<ctx1>' +
       r'pb_decode_open_substream(\g<stream>, &old_length)' +
       r'\g<ctx2>pb_decode_close_substream(\g<stream>, old_length)',
       re.MULTILINE | re.DOTALL),

# Replace dangling references from previous rule
RERule(r'(?P<ctx>/\* API_UPDATER_TODO: replace (?P<substream>[^\s]*) -> (?P<stream>[^\s]*) \*/.*)' +
       r'(?P=substream)',
       r'\g<ctx>\g<stream>', re.MULTILINE | re.DOTALL),

# Same but for struct member access
RERule(r'(?P<ctx>/\* API_UPDATER_TODO: replace &(?P<substream>[^\s]*) -> (?P<stream>[^\s]*) \*/.*)' +
       r'(?P=substream)\.',
       r'\g<ctx>\g<stream>->', re.MULTILINE | re.DOTALL),

# Remove placeholder
RERule(r'\s*/\* API_UPDATER_TODO: replace &(?P<substream>[^\s]*) -> (?P<stream>[^\s]*) \*/', ''),

# Provide context as argument to pb_release()
RERule(r'(?P<head>pb_decode\((?P<ctx>[^,\s]*)\s*,\s*(?P<msgtype>[^,\s]*)\s*,\s*(?P<msg>[^\)\s]*)\s*\).*)' +
       r'pb_release\((?P=msgtype)\s*,\s*(?P=msg)\s*\)',
       r'\g<head>pb_release(\g<ctx>, \g<msgtype>, \g<msg>)', re.MULTILINE | re.DOTALL),

# If context was not detected, give NULL to pb_release().
# This is safe enough, as previously context-based allocators
# did not exist so old code won't be using it.
TSRule('''(
    (call_expression
        function: (identifier) @func
        arguments: (argument_list _ . (_) @msgtype . (_) @msg .)
    ) @node
    (#eq? @func "pb_release")
    )''',
    'pb_release(NULL /* Uses default allocator */, {msgtype}, {msg})'),

# AUTO argument to PB_BIND -> S
RERule(r'PB_BIND\((?P<args>.*),\s*AUTO\s*\)',
       r'PB_BIND(\g<args>, S)'),

# #ifdef PB_NO_xxxx -> #if PB_NO_xxxx
RERule(r'(?P<prefix>\s*#\s*)ifdef\s+(?P<name>PB_NO_[A-Z_]+)',
       r'\g<prefix>if \g<name>'),

# #ifndef PB_NO_xxxx -> #if !PB_NO_xxxx
# but only if no #define PB_NO_xxxx inside it
RERule(r'^(?P<prefix>[ \t]*#[ \t]*)ifndef\s+(?P<name>PB_NO_[A-Z_]+)' +
       r'(?P<rest>\s*(?:(?!#define\s(?P=name)).)*#endif)',
       r'\g<prefix>if !\g<name>\g<rest>', re.MULTILINE | re.DOTALL),

# #define PB_NO_xxxx without value -> 1
RERule(r'(?P<prefix>\s*#\s*)define\s+(?P<name>PB_NO_[A-Z_]+)\s*$',
       r'\g<prefix>define \g<name> 1'),

# #ifndef PB_ENABLE_MALLOC -> #if PB_NO_MALLOC
RERule(r'(?P<prefix>\s*#\s*)ifndef\s+PB_ENABLE_MALLOC',
       r'\g<prefix>if PB_NO_MALLOC'),

# #ifdef PB_ENABLE_MALLOC -> #if !PB_NO_MALLOC
RERule(r'(?P<prefix>\s*#\s*)ifdef\s+PB_ENABLE_MALLOC',
       r'\g<prefix>if !PB_NO_MALLOC'),

# #ifndef PB_CONVERT_DOUBLE_FLOAT -> #if !PB_CONVERT_DOUBLE_FLOAT
RERule(r'(?P<prefix>\s*#\s*)ifndef\s+PB_CONVERT_DOUBLE_FLOAT',
       r'\g<prefix>if !PB_CONVERT_DOUBLE_FLOAT'),

# #ifdef PB_CONVERT_DOUBLE_FLOAT -> #if PB_CONVERT_DOUBLE_FLOAT
RERule(r'(?P<prefix>\s*#\s*)ifdef\s+PB_CONVERT_DOUBLE_FLOAT',
       r'\g<prefix>if PB_CONVERT_DOUBLE_FLOAT'),

# defined(PB_NO_xxxx) -> PB_NO_xxxxx
RERule(r'defined\(\s*(?P<name>PB_NO_[A-Z_]+)\s*\)',
       r'\g<name>'),

]

def main(args, rules = RULES):
    import glob
    from optparse import OptionParser

    optparser = OptionParser(
        usage = "Usage: nanopb_api_updater.py [options] file.c ...",
        epilog = "This script updates user C code to latest nanopb API")
    
    optparser.add_option("--stdout", dest="stdout", action="store_true", default=False,
                         help="Write output to stdout instead of modifying the file")
    optparser.add_option("--glob", dest="glob", action="store_true", default=False,
                         help="Interpret paths as Python globs, for example '**/*.c' matches recursively.")

    options, filenames = optparser.parse_args(args)
    c_parser = Parser(C_LANGUAGE)

    if options.glob:
        globs = filenames
        filenames = []
        for pattern in globs:
            filenames += glob.glob(pattern, recursive = True)

    for file in filenames:
        source = open(file, 'rb').read()
        
        for rule in rules:
            source = rule.run(source)
        
        if options.stdout:
            print(source.decode())
        else:
            open(file, 'wb').write(source)

if __name__ == '__main__':
    import sys
    main(sys.argv[1:])

