# Nanopb generator API reference

## Generator options

Generator options affect how the `.proto` files get converted to `.pb.c` and `.pb.h.` files.

Most options are related to specific message or field in `.proto` file.
The full set of available options is defined in [nanopb.proto](https://github.com/nanopb/nanopb/blob/master/generator/proto/nanopb.proto). Here is a list of the most common options, but see the file for a full list:

* `max_size`: Allocated maximum size for `bytes` and `string` fields. For strings, this includes the terminating zero.
* `max_length`: Maximum length for `string` fields. Setting this is equivalent to setting `max_size` to a value of length + 1.
* `max_count`: Allocated maximum number of entries in arrays (`repeated` fields).
* `type`: Select how memory is allocated for the generated field. Default value is `FT_DEFAULT`, which defaults to `FT_STATIC` when possible and `FT_CALLBACK` if not possible. You can use `FT_CALLBACK`, `FT_POINTER`, `FT_STATIC` or `FT_IGNORE` to select a callback field, a dynamically allocate dfield, a statically allocated field or to completely ignore the field.
* `long_names`: Prefix the enum name to the enum value in definitions, i.e. `EnumName_EnumValue`. Enabled by default.
* `packed_struct`: Make the generated structures packed, which saves some RAM space but slows down execution. This can only be used if the CPU supports unaligned access to variables.
* `skip_message`: Skip a whole message from generation. Can be used to remove message types that are not needed in an application.
* `no_unions`: Generate `oneof` fields as multiple optional fields instead of a C `union {}`.
* `anonymous_oneof`: Generate `oneof` fields as an anonymous union.
* `msgid`: Specifies a unique id for this message type. Can be used by user code as an identifier.
* `fixed_length`: Generate `bytes` fields with a constant length defined by `max_size`. A separate `.size` field will then not be generated.
* `fixed_count`: Generate arrays with constant length defined by `max_count`.
* `package`: Package name that applies only for nanopb generator. Defaults to name defined by `package` keyword in .proto file, which applies for all languages.
* `int_size`: Override the integer type of a field. For example, specify `int_size = IS_8` to convert `int32` from protocol definition into `int8_t` in the structure. When used with enum types, the size of the generated enum can be specified (C++ only)

These options can be defined for the .proto files before they are
converted using the nanopb-generator.py. There are three ways to define
the options:

1.  Using a separate .options file. This allows using wildcards for
    applying same options to multiple fields.
2.  Defining the options on the command line of nanopb_generator.py.
    This only makes sense for settings that apply to a whole file.
3.  Defining the options in the .proto file using the nanopb extensions.
    This keeps the options close to the fields they apply to, but can be
    problematic if the same .proto file is shared with many projects.

The effect of the options is the same no matter how they are given. The
most common purpose is to define maximum size for string fields in order
to statically allocate them.

### Defining the options in a .options file

The preferred way to define options is to have a separate file
'myproto.options' in the same directory as the 'myproto.proto'. :

    # myproto.proto
    message MyMessage {
        required string name = 1;
        repeated int32 ids = 4;
    }

    # myproto.options
    MyMessage.name         max_size:40
    MyMessage.ids          max_count:5

The generator will automatically search for this file and read the
options from it. The file format is as follows:

-   Lines starting with `#` or `//` are regarded as comments.
-   Blank lines are ignored.
-   All other lines should start with a field name pattern, followed by
    one or more options. For example: `MyMessage.myfield max_size:5 max_count:10`.
-   The field name pattern is matched against a string of form
    `Message.field`. For nested messages, the string is
    `Message.SubMessage.field`. A whole file can be matched by its
    filename `dir/file.proto`.
-   The field name pattern may use the notation recognized by Python
    fnmatch():
    -   `*` matches any part of string, like `Message.*` for all
        fields
    -   `?` matches any single character
    -   `[seq]` matches any of characters `s`, `e` and `q`
    -   `[!seq]` matches any other character
-   The options are written as `option_name:option_value` and
    several options can be defined on same line, separated by
    whitespace.
-   Options defined later in the file override the ones specified
    earlier, so it makes sense to define wildcard options first in the
    file and more specific ones later.

To debug problems in applying the options, you can use the `-v` option
for the nanopb generator. With protoc, plugin options are specified with
`--nanopb_opt`:

    nanopb_generator -v message.proto           # When invoked directly
    protoc ... --nanopb_opt=-v --nanopb_out=. message.proto  # When invoked through protoc

Protoc doesn't currently pass include path into plugins. Therefore if
your `.proto` is in a subdirectory, nanopb may have trouble finding the
associated `.options` file. A workaround is to specify include path
separately to the nanopb plugin, like:

    protoc -Isubdir --nanopb_opt=-Isubdir --nanopb_out=. message.proto

If preferred, the name of the options file can be set using generator
argument `-f`.

### Defining the options in the .proto file

The .proto file format allows defining custom options for the fields.
The nanopb library comes with *nanopb.proto* which does exactly that,
allowing you do define the options directly in the .proto file:

~~~~ protobuf
import "nanopb.proto";

message MyMessage {
    required string name = 1 [(nanopb).max_size = 40];
    repeated int32 ids = 4   [(nanopb).max_count = 5];
}
~~~~

A small complication is that you have to set the include path of protoc
so that nanopb.proto can be found. Therefore, to compile a .proto file
which uses options, use a protoc command similar to:

    protoc -Inanopb/generator/proto -I. --nanopb_out=. message.proto

The options can be defined in file, message and field scopes:

~~~~ protobuf
option (nanopb_fileopt).max_size = 20; // File scope
message Message
{
    option (nanopb_msgopt).max_size = 30; // Message scope
    required string fieldsize = 1 [(nanopb).max_size = 40]; // Field scope
}
~~~~

### Defining the options on command line

The nanopb_generator.py has a simple command line option `-s OPTION:VALUE`.
The setting applies to the whole file that is being processed.

There are also a few command line options that cannot be applied using the
other mechanisms, as they affect the whole generation:

* `--c-style`: Modify symbol names to better match C naming conventions.
* `--custom-style`: Modify symbol names by providing your own styler implementation.
* `--no-timestamp`: Do not add timestamp to generated files.
* `--strip-path`: Remove relative path from generated `#include` directives.
* `--cpp-descriptors`: Generate extra convenience definitions for use from C++

For a full list of generator command line options, use `nanopb_generator.py --help`:

    Usage: nanopb_generator.py [options] file.pb ...

    Options:
    -h, --help            show this help message and exit
    -V, --version         Show version info and exit (add -v for protoc version
                            info)
    -x FILE               Exclude file from generated #include list.
    -e EXTENSION, --extension=EXTENSION
                            Set extension to use instead of '.pb' for generated
                            files. [default: .pb]
    -H EXTENSION, --header-extension=EXTENSION
                            Set extension to use for generated header files.
                            [default: .h]
    -S EXTENSION, --source-extension=EXTENSION
                            Set extension to use for generated source files.
                            [default: .c]
    -f FILE, --options-file=FILE
                            Set name of a separate generator options file.
    -I DIR, --options-path=DIR, --proto-path=DIR
                            Search path for .options and .proto files. Also
                            determines relative paths for output directory
                            structure.
    --error-on-unmatched  Stop generation if there are unmatched fields in
                            options file
    --no-error-on-unmatched
                            Continue generation if there are unmatched fields in
                            options file (default)
    -D OUTPUTDIR, --output-dir=OUTPUTDIR
                            Output directory of .pb.h and .pb.c files
    -Q FORMAT, --generated-include-format=FORMAT
                            Set format string to use for including other .pb.h
                            files. Value can be 'quote', 'bracket' or a format
                            string. [default: #include "%s"]
    -L FORMAT, --library-include-format=FORMAT
                            Set format string to use for including the nanopb pb.h
                            header. Value can be 'quote', 'bracket' or a format
                            string. [default: #include <%s>]
    --strip-path          Strip directory path from #included .pb.h file name
    --no-strip-path       Opposite of --strip-path (default since 0.4.0)
    --cpp-descriptors     Generate C++ descriptors to lookup by type (e.g.
                            pb_field_t for a message)
    -T, --no-timestamp    Don't add timestamp to .pb.h and .pb.c preambles
                            (default since 0.4.0)
    -t, --timestamp       Add timestamp to .pb.h and .pb.c preambles
    -q, --quiet           Don't print anything except errors.
    -v, --verbose         Print more information.
    -s OPTION:VALUE       Set generator option (max_size, max_count etc.).
    --protoc-opt=OPTION   Pass an option to protoc when compiling .proto files
    --protoc-insertion-points
                            Include insertion point comments in output for use by
                            custom protoc plugins
    -C, --c-style         Use C naming convention.
    --custom-style=MODULE.CLASS
                          Use a custom naming convention from a module/class
                          that defines the methods from the NamingStyle class to
                          be overridden. When paired with the -C/--c-style
                          option, the NamingStyleC class is the fallback,
                          otherwise it's the NamingStyle class.

    Compile file.pb from file.proto by: 'protoc -ofile.pb file.proto'. Output will
    be written to file.pb.h and file.pb.c.
