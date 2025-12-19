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

class RERule:
    '''Code modification rule using Python regex.
    Less robust than TSRules but sometimes easier to use.
    '''

    def __init__(self, pattern, repl, flags = re.MULTILINE):
        self.pattern = pattern
        self.repl = repl
        self.flags = flags
    
    def __str__(self):
        return 'RERule(%s,%s)' % (repr(self.pattern), repr(self.repl))

    def run(self, source):
        try:
            return re.sub(self.pattern, self.repl, source.decode(), flags = self.flags).encode()
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

# Regexp rules are not currently used as tree-sitter is more robust.
# They are retained here as inspiration ;)
# RERule(r"(?<!\w)pb_istream_t(?!\w)", "pb_decode_ctx_t"),
# RERule(r"^(?P<tab>\s*)pb_decode_ctx_t\s+(?P<var>\w+)\s*=\s*pb_istream_from_buffer\((?P<buf>[^,]+),(?P<len>.+)\s*\)\s*;",
#        r"\g<tab>pb_decode_ctx_t \g<var>;\n\g<tab>pb_init_decode_ctx_for_buffer(&\g<var>, \g<buf>,\g<len>);"),
# RERule(r"(?P<tab>\s*)(?<!\w)(?P<var>\w+)\s*=\s*pb_istream_from_buffer\((?P<buf>[^,]+),(?P<len>.+)\s*\)\s*;",
#        r"\g<tab>pb_init_decode_ctx_for_buffer(&\g<var>, \g<buf>,\g<len>);"),

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

# stream = pb_istream_from_buffer(...)
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

# pb_decode_delimited() -> pb_decode() and set flags
RERule(r'^(?P<tab>\s*)(?P<prefix>.*)pb_decode_delimited\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_DECODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>\s*)(?P<prefix>.*)pb_decode_delimited\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_DECODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_decode(\g<ctx>,\g<args>)'),

# Same for pb_encode_delimited()
RERule(r'^(?P<tab>\s*)(?P<prefix>.*)pb_encode_delimited\(&(?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>\g<ctx>.flags |= PB_ENCODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_encode(\g<ctx>,\g<args>)'),

RERule(r'^(?P<tab>\s*)(?P<prefix>.*)pb_encode_delimited\((?P<ctx>[^,]+),(?P<args>.+)\)',
       r'\g<tab>(\g<ctx>)->flags |= PB_ENCODE_CTX_FLAG_DELIMITED;\n' +
       r'\g<tab>\g<prefix>pb_encode(\g<ctx>,\g<args>)'),

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

