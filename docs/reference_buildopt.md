# Build options

Compilation options affect the functionality included in the nanopb core C code.
The options can be specified in one of two ways:

1.  Using the `-D` switch on the C compiler command line.
2.  Using a `#define` in a `pb_config.h` file.

You can find a list of all the flags in the beginning of `pb.h`, or in the `pb_config_example.h` file.

> **NOTE:** You must have the same compilation options for the nanopb library and all code that
includes nanopb headers. [ABI](https: en.wikipedia.org/wiki/Application_binary_interface) compatibility is only tested for the fully-featured default configuration.

## API version compatibility flag

Backwards compatibility functions and macros have been included to ease porting user code from older nanopb versions to the 1.0 version.

By default these compatibility features are disabled.
To enable them, define `PB_API_VERSION` to a value corresponding to the nanopb version the code has been written against.
For example, for nanopb-0.4.x compatibility set `PB_API_VERSION = 4`

The latest API remains fully usable even if compatibility features are enabled.
This permits piece-by-piece migration of old code.

## Feature disable flags

These flags can be used to disable individual nanopb features.
Disabling features generally reduces code size and RAM usage, but benefits vary between platforms.

To disable a feature, define its `PB_NO_xxxxx` flag to `1`.

Alternatively, define `PB_MINIMAL` to `1`, which disables all optional features by default. Individual features can then be enabled by setting their `PB_NO_xxxxx` flags to `0`.

### Memory allocation

* `PB_NO_MALLOC`: Disable support for dynamically allocated fields.

* `PB_NO_DEFAULT_ALLOCATOR`: Disable support for default allocator.
Dynamic allocation is enabled only if ctx->allocator is set by user code.

* `PB_NO_CONTEXT_ALLOCATOR`: Disable support for context-specific allocator.
Only default allocator (pb_realloc()) is supported,
the pb_decode_ctx_t allocator field is disabled.

### Context and stream management

* `PB_NO_STREAM_CALLBACK`: Only support input/output to memory buffers.
Disables using callback functions for IO streams.
This option was called PB_BUFFER_ONLY in nanopb-0.4.x

* `PB_NO_ERRMSG`: Disable support for descriptive error messages.
Only success/failure status is provided.

* `PB_NO_RECURSION`: Disable recursive function calls in nanopb core.
Only up to `PB_MESSAGE_NESTING` levels of nested messages can be
processed. User callbacks can still invoke recursion.

* `PB_NO_OPT_ASSERT`: Disable optional assertions in code.
These are mainly to more easily catch bugs during development.

### Protobuf feature support

* `PB_NO_LARGEMSG`: Disable support for messages over 4 kB.
Tag numbers are limited to max 4095 and arrays to max 255 items.

* `PB_NO_VALIDATE_UTF8`: Do not validate string encoding.
Protobuf spec requires strings to be valid UTF-8. This setting
disables string validation in nanopb.

* `PB_NO_EXTENSIONS`: Disable support for proto2 extension fields

* `PB_NO_CONTEXT_FIELD_CALLBACK`: Disable support for field callbacks
using ctx->field_callback() mechanism.

* `PB_NO_NAME_FIELD_CALLBACK`: Disable support for name-bound field
callbacks using generator 'callback_function' option.

* `PB_NO_STRUCT_FIELD_CALLBACK`: Disable support for field callbacks
defined using the pb_callback_t mechanism.

* `PB_NO_DEFAULT_VALUES`: Disable support for runtime-initialization
of field default values. MyMessage_init_default macro is still
available.

## Platform-specific options

Some embedded platforms have special limitations, which can require
special compilation options. For most platforms there is no need
to define these, as they are automatically detected from C compiler
features.

**Data types**

* `PB_WITHOUT_64BIT`: Disable usage of 64-bit data types in the code.
  For compilers or CPUs that do not support uint64_t.

* `PB_LITTLE_ENDIAN_8BIT`: Specify memory layout compatibility.
  This can be defined if CPU uses 8-bit bytes and has little-endian
  memory layout. If undefined (default), the support is automatically
  detected from compiler type.

* `PB_NO_PACKED_STRUCTS`: Never use 'packed' attribute on structures.
  Note that the attribute is only specified when requested in .proto
  file options. This define allows globally disabling it on platforms
  that do not support unaligned memory access.

* `PB_WALK_STACK_ALIGN_TYPE`: Alignment requirement for pb_walk() stack.
  By default this is void*, which should have large enough alignment
  for storage of any pointer or 32-bit integer. Special platforms could
  require e.g. uint32_t here.

* `PB_BYTE_T_OVERRIDE`: Override type used to access byte buffers.
  On most platforms, pb_byte_t = uint8_t. On platforms without uint8_t,
  by default unsigned char is used. Alternatively this option can
  be used to set a custom type.

* `PB_SIZE_T_OVERRIDE`: Override size type used for messages and streams.
  On 8-32 bit platforms, pb_size_t defaults to size_t.
  On 64 bit platforms, pb_size_t defaults to uint32_t.
  This option can be used to override the type.

**Static assertions**

* `PB_C99_STATIC_ASSERT`: Force use of older, C99 static assertion mechanism.
  This is for compilers that do not support _Static_assert() keyword that
  was introduced in C11 standard. Many compilers supported it before that.

* `PB_NO_STATIC_ASSERT`: Disable compile-time assertions in the code.
  This is for compilers where the PB_STATIC_ASSERT macro does not work.
  It's preferable to either change compiler to C11 standards mode or
  to define PB_C99_STATIC_ASSERT.

**Memory and code attributes**

* `PB_NO_FUNCTION_POINTERS`: Disable usage of function pointers in the library.
  Useful for platforms where function pointers are either expensive, or for
  compatibility with code safety standards such as MISRA-C.
  Removes support for any callback-based features.

* `PB_PROGMEM`: Attribute and access method for storing constants in ROM.
  This is automatically enabled for AVR platform. It can be used
  on platforms where const variables are not automatically stored in ROM.

* `PB_WEAK_FUNCTION`: Attribute for weak function declarations.
  Used only with PB_NO_FUNCTION_POINTERS and other special features.
  By default autodetected by compiler type.

## Protobuf compatibility options

The options below enable interoperability features that can be useful
for communicating with externally defined protobuf schema.

* `PB_ENCODE_ARRAYS_UNPACKED`: Use 'unpacked' array format for all fields.
  Normally the more efficient 'packed' array format is used for field types
  that support it. This option forces 'unpacked' format. In particular,
  it is needed when communicating with protobuf.js versions before 2020.

* `PB_CONVERT_DOUBLE_FLOAT`: Convert 64-bit doubles to 32-bit floats.
  AVR platform only supports 32-bit floats. If you need to use a .proto
  that has 'double' fields, this option will convert the encoded binary
  format. The precision of values will be limited to 32-bit.

## Stack usage options

Nanopb uses a hybrid approach to handling recursive messages.
Instead of C recursion, the core uses a memory buffer to store
minimal amount of information for each message level. For this
storage, a constant-sized buffer is allocated on stack. This
initial reservation is enough for `PB_MESSAGE_NESTING` levels.

Once the buffer fills up, more memory is allocated from stack
using C recursion. This can be disabled with `PB_NO_RECURSION`.

In any case, message nesting is limited to maximum of
`PB_MESSAGE_NESTING_MAX` levels, after which runtime error is
returned.

* `PB_MESSAGE_NESTING`: Expected depth of message hierarchy.
  Encode and decode calls initially reserve enough stack space to
  handle this number of nested message levels.

* `PB_MESSAGE_NESTING_MAX`: Runtime limit of message nesting.
  If recursion is enabled, up to this many nested message levels
  can be processed by dynamically allocating more stack space.

* `PB_WALK_STACK_SIZE`: Block size of recursive memory reservation.
  Once the initial stack allocation is exhausted, [pb_walk()](reference_pb_common_h.md) will
  reserve more stack in blocks of this many bytes.

* `PB_MAX_REQUIRED_FIELDS`: Expected number of required fields per message.
  This is only used for calculating the initial stack allocation.
  At runtime, memory is allocated based on actual number of required
  fields in each message.

