// Example file for defining nanopb compilation options
// in a separate header file. Copy this file to pb_config.h
// in your own project to activate it. On most compilers,
// its existence is automatically detected. Alternatively
// you can define PB_CONFIG_HEADER_NAME in compiler arguments.

#ifndef PB_CONFIG_H_INCLUDED
#define PB_CONFIG_H_INCLUDED

/*****************************************************************
 * By default all features default to enabled.                   *
 * They can be individually disabled by defining PB_NO_xxxx to 1.*
 * Disabling features generally reduces code size and RAM usage, *
 * but benefits vary between platforms.                          *
 *                                                               *
 * Alternatively PB_MINIMAL=1 will disable features by default.  *
 * Features can then be enabled by defining PB_NO_xxxx to 0.     *
 *****************************************************************/


// PB_API_VERSION: API compatibility level for old code.
// This enables macros that ease porting of code written for older
// nanopb versions. Define to e.g. '40' to enable nanopb-0.4.x API.
// Latest API is always supported independent of this setting.

// #define PB_API_VERSION 40


// PB_MINIMAL: When 1, disable all optional features by default.
// Individual features can still be enabled by defining their
// PB_NO_xxxx setting to 0.

// #define PB_MINIMAL 1


// PB_NO_MALLOC: Disable support for dynamically allocated fields.

// #define PB_NO_MALLOC 1


// PB_NO_DEFAULT_ALLOCATOR: Disable support for default allocator.
// Dynamic allocation is enabled only if ctx->allocator is set by user code.

// #define PB_NO_DEFAULT_ALLOCATOR 1


// PB_NO_CONTEXT_ALLOCATOR: Disable support for context-specific allocator.
// Only default allocator (pb_realloc()) is supported,
// the pb_decode_ctx_t allocator field is disabled.

// #define PB_NO_CONTEXT_ALLOCATOR 1


// PB_NO_STREAM_CALLBACK: Only support input/output to memory buffers.
// Disables using callback functions for IO streams.
// This option was called PB_BUFFER_ONLY in nanopb-0.4.x

// #define PB_NO_STREAM_CALLBACK 1


// PB_NO_ERRMSG: Disable support for descriptive error messages.
// Only success/failure status is provided.

// #define PB_NO_ERRMSG 1


// PB_NO_RECURSION: Disable recursive function calls in nanopb core.
// Only up to PB_MESSAGE_NESTING levels of nested messages can be
// processed. User callbacks can still invoke recursion.

// #define PB_NO_RECURSION 1


// PB_NO_LARGEMSG: Disable support for messages over 4 kB.
// Tag numbers are limited to max 4095 and arrays to max 255 items.

// #define PB_NO_LARGEMSG 1


// PB_NO_VALIDATE_UTF8: Do not validate string encoding.
// Protobuf spec requires strings to be valid UTF-8. This setting
// disables string validation in nanopb.

// #define PB_NO_VALIDATE_UTF8 1


// PB_NO_OPT_ASSERT: Disable optional assertions in code.
// These are mainly to more easily catch bugs during development.
// #define PB_NO_OPT_ASSERT 1


// PB_NO_EXTENSIONS: Disable support for proto2 extension fields

// #define PB_NO_EXTENSIONS 1


// PB_NO_CONTEXT_FIELD_CALLBACK: Disable support for field callbacks
// using ctx->field_callback() mechanism.

// #define PB_NO_CONTEXT_FIELD_CALLBACK 1


// PB_NO_NAME_FIELD_CALLBACK: Disable support for name-bound field
// callbacks using generator 'callback_function' option.

// #define PB_NO_NAME_FIELD_CALLBACK 1


// PB_NO_STRUCT_FIELD_CALLBACK: Disable support for field callbacks
// defined using the pb_callback_t mechanism.

// #define PB_NO_STRUCT_FIELD_CALLBACK 1


/*********************************************************************
 * Platform feature options.                                         *
 *                                                                   *
 * These are used to disable features for CPU platforms or compilers *
 * that do not support the full ISO C99 feature set.                 *
 *                                                                   *
 * By default all of these are disabled. Define to 1 to enable.      *
 *********************************************************************/

// PB_WITHOUT_64BIT: Disable usage of 64-bit data types in the code.
// For compilers or CPUs that do not support uint64_t.

// #define PB_WITHOUT_64BIT 1


// PB_NO_PACKED_STRUCTS: Never use 'packed' attribute on structures.
// Note that the attribute is only specified when requested in .proto
// file options. This define allows globally disabling it on platforms
// that do not support unaligned memory access.

// #define PB_NO_PACKED_STRUCTS 1


// PB_C99_STATIC_ASSERT: Force use of older, C99 static assertion mechanism.
// This is for compilers that do not support _Static_assert() keyword that
// was introduced in C11 standard. Many compilers supported it before that.

// #define PB_C99_STATIC_ASSERT 1


// PB_NO_STATIC_ASSERT: Disable compile-time assertions in the code.
// This is for compilers where the PB_STATIC_ASSERT macro does not work.
// It's preferable to either change compiler to C11 standards mode or
// to define PB_C99_STATIC_ASSERT.

// #define PB_NO_STATIC_ASSERT 1


// PB_LITTLE_ENDIAN_8BIT: Specify memory layout compatibility.
// This can be defined if CPU uses 8-bit bytes and has little-endian
// memory layout. If undefined (default), the support is automatically
// detected from compiler type.

// #define PB_LITTLE_ENDIAN_8BIT 1


// PB_PROGMEM: Attribute and access method for storing constants in ROM.
// This is automatically enabled for AVR platform. It can be used
// on platforms where const variables are not automatically stored in ROM.

// #define PB_PROGMEM             ...attribute...
//  #define PB_PROGMEM_READU32(x)  ...access function...


// PB_WALK_STACK_ALIGN_TYPE: Alignment requirement for pb_walk() stack.
// By default this is void*, which should have large enough alignment
// for storage of any pointer or 32-bit integer. Special platforms could
// require e.g. uint32_t here.

// #define PB_WALK_STACK_ALIGN_TYPE void*


// PB_BYTE_T_OVERRIDE: Override type used to access byte buffers.
// On most platforms, pb_byte_t = uint8_t. On platforms without uint8_t,
// by default unsigned char is used. Alternatively this option can
// be used to set a custom type.

// #define PB_BYTE_T_OVERRIDE uint_least8_t


// PB_SIZE_T_OVERRIDE: Override size type used for messages and streams.
// On 8-32 bit platforms, pb_size_t defaults to size_t.
// On 64 bit platforms, pb_size_t defaults to uint32_t.
// This option can be used to override the type.

// #define PB_SIZE_T_OVERRIDE uint16_t

/*******************************************************************
 * Protobuf compatibility options                                  *
 *                                                                 *
 * These aid in compatibility with other protobuf implementations  *
 * in specific situations. By default they are disabled.           *
 *******************************************************************/

// PB_ENCODE_ARRAYS_UNPACKED: Use 'unpacked' array format for all fields.
// Normally the more efficient 'packed' array format is used for field types
// that support it. This option forces 'unpacked' format. In particular,
// it is needed when communicating with protobuf.js versions before 2020.

// #define PB_ENCODE_ARRAYS_UNPACKED 1


// PB_CONVERT_DOUBLE_FLOAT: Convert 64-bit doubles to 32-bit floats.
// AVR platform only supports 32-bit floats. If you need to use a .proto
// that has 'double' fields, this option will convert the encoded binary
// format. The precision of values will be limited to 32-bit.

// #define PB_CONVERT_DOUBLE_FLOAT 1


/*******************************************************************
 * Stack usage and nesting limit options                           *
 *                                                                 *
 * Nanopb uses a hybrid approach to handling recursive messages.   *
 * Instead of C recursion, the core uses a memory buffer to store  *
 * minimal amount of information for each message level. For this  *
 * storage, a constant-sized buffer is allocated on stack. This    *
 * initial reservation is enough for PB_MESSAGE_NESTING levels.    *
 *                                                                 *
 * Once the buffer fills up, more memory is allocated from stack   *
 * using C recursion. This can be disabled with PB_NO_RECURSION.   *
 *                                                                 *
 * In any case, message nesting is limited to maximum of           *
 * PB_MESSAGE_NESTING_MAX levels, after which runtime error is     *
 * returned.                                                       *
 *******************************************************************/

// PB_MESSAGE_NESTING: Expected depth of message hierarchy.
// Encode and decode calls initially reserve enough stack space to
// handle this number of nested message levels.

// #define PB_MESSAGE_NESTING 8


// PB_MESSAGE_NESTING_MAX: Runtime limit of message nesting.
// If recursion is enabled, up to this many nested message levels
// can be processed by dynamically allocating more stack space.

// #define PB_MESSAGE_NESTING_MAX 100


// PB_WALK_STACK_SIZE: Block size of recursive memory reservation.
// Once the initial stack allocation is exhausted, pb_walk() will
// reserve more stack in blocks of this many bytes.

// #define PB_WALK_STACK_SIZE 256


// PB_MAX_REQUIRED_FIELDS: Expected number of required fields per message.
// This is only used for calculating the initial stack allocation.
// At runtime, memory is allocated based on actual number of required
// fields in each message.

// #define PB_MAX_REQUIRED_FIELDS 64


#endif
