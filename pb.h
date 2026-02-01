/* Type and macro definitions for the nanopb library.
 * This file contains configuration options and internal definitions.
 *
 * For encoding and decoding API, see pb_encode.h and pb_decode.h
 */

#ifndef PB_H_INCLUDED
#define PB_H_INCLUDED

#define PB_HDRNAME2(x) #x
#define PB_HDRNAME(x) PB_HDRNAME2(x)

/* Include all the system headers needed by nanopb. You will need the
 * definitions of the following:
 * - strlen, memcpy, memmove, memset functions
 * - [u]int_least8_t, uint_fast8_t, [u]int_least16_t, [u]int32_t, [u]int64_t
 * - size_t
 * - bool
 * - realloc() and free() unless PB_NO_DEFAULT_ALLOCATOR is defined
 *
 * If you don't have the standard header files, you can instead provide
 * a custom header that defines or includes all this.
 * Note that for legacy reasons, PB_SYSTEM_HEADER value has to include
 * either quotes or brackets, such as:
 * '-DPB_SYSTEM_HEADER="myhdr.h"'
 *
 * Alternatively PB_SYSTEM_HEADER_NAME can be used without quotes:
 * -DPB_SYSTEM_HEADER_NAME=myhdr.h
 */
#if defined(PB_SYSTEM_HEADER)
#include PB_SYSTEM_HEADER
#elif defined(PB_SYSTEM_HEADER_NAME)
#include PB_HDRNAME(PB_SYSTEM_HEADER_NAME)
#else

#ifdef __cplusplus
#ifndef __STDC_LIMIT_MACROS
// In older C++ versions, this is required to UINT32_MAX etc. defined
#define __STDC_LIMIT_MACROS 1
#endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#endif

/* Configuration options can be defined in a pb_config.h file.
 * On most compilers its existence can be automatically detected,
 * but otherwise you can declare PB_CONFIG_HEADER_NAME.
 */
#if defined(PB_CONFIG_HEADER_NAME)
#include PB_HDRNAME(PB_CONFIG_HEADER_NAME)
#elif defined(__has_include)
# if __has_include("pb_config.h")
#  include "pb_config.h"
# endif
#endif

/* Extra include file that can be used to provide e.g. default
 * allocator definition. */
#ifdef PB_EXTRA_HEADER_NAME
#include PB_HDRNAME(PB_EXTRA_HEADER_NAME)
#endif

/*****************************************************************
 * Nanopb compilation time options. You can change these here by *
 * uncommenting the lines, on the compiler command line or in    *
 * a pb_config.h file (see pb_config_example.h).                 *
 *                                                               *
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
#ifndef PB_API_VERSION
#define PB_API_VERSION PB_API_VERSION_LATEST
#endif

// PB_MINIMAL: When 1, disable all optional features by default.
// Individual features can still be enabled by defining their
// PB_NO_xxxx setting to 0.
#ifndef PB_MINIMAL
#define PB_MINIMAL 0
#endif

// PB_NO_MALLOC: Disable support for dynamically allocated fields.
#ifndef PB_NO_MALLOC
#define PB_NO_MALLOC PB_MINIMAL
#endif

// PB_NO_DEFAULT_ALLOCATOR: Disable support for default allocator.
// Dynamic allocation is enabled only if ctx->allocator is set by user code.
#ifndef PB_NO_DEFAULT_ALLOCATOR
#define PB_NO_DEFAULT_ALLOCATOR PB_NO_MALLOC
#endif

// PB_NO_CONTEXT_ALLOCATOR: Disable support for context-specific allocator.
// Only default allocator (pb_realloc()) is supported,
// the pb_decode_ctx_t allocator field is disabled.
#ifndef PB_NO_CONTEXT_ALLOCATOR
#define PB_NO_CONTEXT_ALLOCATOR PB_NO_MALLOC
#endif

// PB_NO_STREAM_CALLBACK: Only support input/output to memory buffers.
// Disables using callback functions for IO streams.
// This option was called PB_BUFFER_ONLY in nanopb-0.4.x
#ifndef PB_NO_STREAM_CALLBACK
#ifdef PB_BUFFER_ONLY
#define PB_NO_STREAM_CALLBACK 1
#else
#define PB_NO_STREAM_CALLBACK PB_MINIMAL
#endif
#endif

// PB_NO_ERRMSG: Disable support for descriptive error messages.
// Only success/failure status is provided.
#ifndef PB_NO_ERRMSG
#define PB_NO_ERRMSG PB_MINIMAL
#endif

// PB_NO_RECURSION: Disable recursive function calls in nanopb core.
// Only up to PB_MESSAGE_NESTING levels of nested messages can be
// processed. User callbacks can still invoke recursion.
#ifndef PB_NO_RECURSION
#define PB_NO_RECURSION PB_MINIMAL
#endif

// PB_NO_LARGEMSG: Disable support for messages over 4 kB.
// Tag numbers are limited to max 4095 and arrays to max 255 items.
#ifndef PB_NO_LARGEMSG
#define PB_NO_LARGEMSG PB_MINIMAL
#endif

// PB_NO_VALIDATE_UTF8: Do not validate string encoding.
// Protobuf spec requires strings to be valid UTF-8. This setting
// disables string validation in nanopb.
#ifndef PB_NO_VALIDATE_UTF8
#define PB_NO_VALIDATE_UTF8 PB_MINIMAL
#endif

// PB_NO_OPT_ASSERT: Disable optional assertions in code.
// These are mainly to more easily catch bugs during development.
#ifndef PB_NO_OPT_ASSERT
#define PB_NO_OPT_ASSERT PB_MINIMAL
#endif

// PB_NO_EXTENSIONS: Disable support for proto2 extension fields
#ifndef PB_NO_EXTENSIONS
#define PB_NO_EXTENSIONS PB_MINIMAL
#endif

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
#ifndef PB_WITHOUT_64BIT
#define PB_WITHOUT_64BIT 0
#endif

// PB_NO_PACKED_STRUCTS: Never use 'packed' attribute on structures.
// Note that the attribute is only specified when requested in .proto
// file options. This define allows globally disabling it on platforms
// that do not support unaligned memory access.
#ifndef PB_NO_PACKED_STRUCTS
#define PB_NO_PACKED_STRUCTS 0
#endif

// PB_C99_STATIC_ASSERT: Force use of older, C99 static assertion mechanism.
// This is for compilers that do not support _Static_assert() keyword that
// was introduced in C11 standard. Many compilers supported it before that.
#ifndef PB_C99_STATIC_ASSERT
#define PB_C99_STATIC_ASSERT 0
#endif

// PB_NO_STATIC_ASSERT: Disable compile-time assertions in the code.
// This is for compilers where the PB_STATIC_ASSERT macro does not work.
// It's preferable to either change compiler to C11 standards mode or
// to define PB_C99_STATIC_ASSERT.
#ifndef PB_NO_STATIC_ASSERT
#define PB_NO_STATIC_ASSERT 0
#endif

// PB_LITTLE_ENDIAN_8BIT: Specify memory layout compatibility.
// This can be defined if CPU uses 8-bit bytes and has little-endian
// memory layout. If undefined (default), the support is automatically
// detected from compiler type.
/* #define PB_LITTLE_ENDIAN_8BIT 1 */

// PB_PROGMEM: Attribute and access method for storing constants in ROM.
// This is automatically enabled for AVR platform. It can be used
// on platforms where const variables are not automatically stored in ROM.
/* #define PB_PROGMEM             ...attribute... */
/* #define PB_PROGMEM_READU32(x)  ...access function... */

// PB_WALK_STACK_ALIGN_TYPE: Alignment requirement for pb_walk() stack.
// By default this is void*, which should have large enough alignment
// for storage of any pointer or 32-bit integer. Special platforms could
// require e.g. uint32_t here.
#ifndef PB_WALK_STACK_ALIGN_TYPE
#define PB_WALK_STACK_ALIGN_TYPE void*
#endif

// PB_BYTE_T_OVERRIDE: Override type used to access byte buffers.
// On most platforms, pb_byte_t = uint8_t. On platforms without uint8_t,
// by default unsigned char is used. Alternatively this option can
// be used to set a custom type.
/* #define PB_BYTE_T_OVERRIDE uint_least8_t */

// PB_SIZE_T_OVERRIDE: Override size type used for messages and streams.
// On 8-32 bit platforms, pb_size_t defaults to size_t.
// On 64 bit platforms, pb_size_t defaults to uint32_t.
// This option can be used to override the type.
/* #define PB_SIZE_T_OVERRIDE uint16_t */

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
#ifndef PB_ENCODE_ARRAYS_UNPACKED
#define PB_ENCODE_ARRAYS_UNPACKED 0
#endif

// PB_CONVERT_DOUBLE_FLOAT: Convert 64-bit doubles to 32-bit floats.
// AVR platform only supports 32-bit floats. If you need to use a .proto
// that has 'double' fields, this option will convert the encoded binary
// format. The precision of values will be limited to 32-bit.
#ifndef PB_CONVERT_DOUBLE_FLOAT
#define PB_CONVERT_DOUBLE_FLOAT 0
#endif

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
#ifndef PB_MESSAGE_NESTING
#if PB_MINIMAL
#define PB_MESSAGE_NESTING 4
#else
#define PB_MESSAGE_NESTING 8
#endif
#endif

// PB_MESSAGE_NESTING_MAX: Runtime limit of message nesting.
// If recursion is enabled, up to this many nested message levels
// can be processed by dynamically allocating more stack space.
#ifndef PB_MESSAGE_NESTING_MAX
#define PB_MESSAGE_NESTING_MAX 100
#endif

// PB_WALK_STACK_SIZE: Block size of recursive memory reservation.
// Once the initial stack allocation is exhausted, pb_walk() will
// reserve more stack in blocks of this many bytes.
#ifndef PB_WALK_STACK_SIZE
#define PB_WALK_STACK_SIZE 256
#endif

// PB_MAX_REQUIRED_FIELDS: Expected number of required fields per message.
// This is only used for calculating the initial stack allocation.
// At runtime, memory is allocated based on actual number of required
// fields in each message.
#ifndef PB_MAX_REQUIRED_FIELDS
#define PB_MAX_REQUIRED_FIELDS 64
#endif

/******************************************************************
 * You usually don't need to change anything below this line.     *
 * Feel free to look around and use the defined macros, though.   *
 ******************************************************************/

/* Version of the nanopb library. Just in case you want to check it in
 * your own program. */
#define NANOPB_VERSION "nanopb-1.0.0-dev"

/* API compatibility version.
 * If this is set to lower than PB_API_VERSION_LATEST, compatibility
 * defines at the end of this file get enabled. This allows easier
 * porting from older nanopb versions.
 *
 * For example, set -DPB_API_VERSION=40 in compiler arguments.
 */
#define PB_API_VERSION_v1_0 100
#define PB_API_VERSION_v0_4  40
#define PB_API_VERSION_LATEST PB_API_VERSION_v1_0

/* Validate configuration options */
#if PB_NO_MALLOC && !PB_NO_DEFAULT_ALLOCATOR
#undef PB_NO_DEFAULT_ALLOCATOR
#define PB_NO_DEFAULT_ALLOCATOR 1
#endif

#if PB_NO_MALLOC && !PB_NO_CONTEXT_ALLOCATOR
#undef PB_NO_CONTEXT_ALLOCATOR
#define PB_NO_CONTEXT_ALLOCATOR 1
#endif

#if PB_WITHOUT_64BIT && PB_CONVERT_DOUBLE_FLOAT
#undef PB_CONVERT_DOUBLE_FLOAT
#define PB_CONVERT_DOUBLE_FLOAT 0
#endif

#if PB_MESSAGE_NESTING < 1
#error PB_MESSAGE_NESTING must be >= 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Macro for defining packed structures (compiler dependent).
 * This just reduces memory requirements, but is not required.
 * It is only used if 'packed_struct = true' is defined in .proto
 */
#if PB_NO_PACKED_STRUCTS
    /* Disable struct packing */
#   define PB_PACKED_STRUCT_START
#   define PB_PACKED_STRUCT_END
#   define pb_packed
#elif defined(__GNUC__) || defined(__clang__)
    /* For GCC and clang */
#   define PB_PACKED_STRUCT_START
#   define PB_PACKED_STRUCT_END
#   define pb_packed __attribute__((packed))
#elif defined(__ICCARM__) || defined(__CC_ARM)
    /* For IAR ARM and Keil MDK-ARM compilers */
#   define PB_PACKED_STRUCT_START _Pragma("pack(push, 1)")
#   define PB_PACKED_STRUCT_END _Pragma("pack(pop)")
#   define pb_packed
#elif defined(_MSC_VER) && (_MSC_VER >= 1500)
    /* For Microsoft Visual C++ */
#   define PB_PACKED_STRUCT_START __pragma(pack(push, 1))
#   define PB_PACKED_STRUCT_END __pragma(pack(pop))
#   define pb_packed
#else
    /* Unknown compiler */
#   define PB_PACKED_STRUCT_START
#   define PB_PACKED_STRUCT_END
#   define pb_packed
#endif

/* Detect endianness and size of char type */
#if !defined(CHAR_BIT) && defined(__CHAR_BIT__)
#define CHAR_BIT __CHAR_BIT__
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef PB_LITTLE_ENDIAN_8BIT
#if ((defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) || \
     (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
      defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || \
      defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || \
      defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM)) \
     && defined(CHAR_BIT) && CHAR_BIT == 8
#define PB_LITTLE_ENDIAN_8BIT 1
#else
#define PB_LITTLE_ENDIAN_8BIT 0
#endif
#endif

/* Handly macro for suppressing unreferenced-parameter compiler warnings. */
#ifndef PB_UNUSED
#define PB_UNUSED(x) (void)(x)
#endif

/* Harvard-architecture processors may need special attributes for storing
 * field information in program memory. */
#ifndef PB_PROGMEM
#ifdef __AVR__
#include <avr/pgmspace.h>
#define PB_PROGMEM             PROGMEM
#define PB_PROGMEM_READU32(x)  pgm_read_dword(&x)
#else
#define PB_PROGMEM
#define PB_PROGMEM_READU32(x)  (x)
#endif
#endif

/* Optional run-time assertions.
 * These are mainly to more easily catch bugs during development.
 * They can be disabled for small amount of code size saving.
 */
#ifndef PB_OPT_ASSERT
# if PB_NO_OPT_ASSERT
#  define PB_OPT_ASSERT(ignore) ((void)(ignore))
# else
#  include <assert.h>
#  define PB_OPT_ASSERT(cond) assert(cond)
# endif
#endif

/* Compile-time assertion, used for checking compatible compilation options.
 * If this does not work properly on your compiler, use
 * #define PB_NO_STATIC_ASSERT to disable it.
 *
 * But before doing that, check carefully the error message / place where it
 * comes from to see if the error has a real cause. Unfortunately the error
 * message is not always very clear to read, but you can see the reason better
 * in the place where the PB_STATIC_ASSERT macro was called.
 */
#if !PB_NO_STATIC_ASSERT
#  ifndef PB_STATIC_ASSERT
#    if defined(__ICCARM__)
       /* IAR has static_assert keyword but no _Static_assert */
#      define PB_STATIC_ASSERT(COND,MSG) static_assert(COND,#MSG);
#    elif defined(_MSC_VER) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112)
       /* MSVC in C89 mode supports static_assert() keyword anyway */
#      define PB_STATIC_ASSERT(COND,MSG) static_assert(COND,#MSG);
#    elif defined(PB_C99_STATIC_ASSERT) && PB_C99_STATIC_ASSERT
       /* Classic negative-size-array static assert mechanism */
#      define PB_STATIC_ASSERT(COND,MSG) typedef char PB_STATIC_ASSERT_MSG(MSG, __LINE__, __COUNTER__)[(COND)?1:-1];
#      define PB_STATIC_ASSERT_MSG(MSG, LINE, COUNTER) PB_STATIC_ASSERT_MSG_(MSG, LINE, COUNTER)
#      define PB_STATIC_ASSERT_MSG_(MSG, LINE, COUNTER) pb_static_assertion_##MSG##_##LINE##_##COUNTER
#    elif defined(__cplusplus)
       /* C++11 standard static_assert mechanism */
#      define PB_STATIC_ASSERT(COND,MSG) static_assert(COND,#MSG);
#    else
       /* C11 standard _Static_assert mechanism */
#      define PB_STATIC_ASSERT(COND,MSG) _Static_assert(COND,#MSG);
#    endif
#  endif
#else
   /* Static asserts disabled by PB_NO_STATIC_ASSERT */
#  define PB_STATIC_ASSERT(COND,MSG)
#endif

/* Test that PB_STATIC_ASSERT works
 * If you get errors here, you may need to do one of these:
 * - Enable C11 standard support in your compiler
 * - Define PB_C99_STATIC_ASSERT to enable C99 standard support
 * - Define PB_NO_STATIC_ASSERT to disable static asserts altogether
 */
PB_STATIC_ASSERT(1, STATIC_ASSERT_IS_NOT_WORKING)

// Stack alignment used by pb_walk().
typedef PB_WALK_STACK_ALIGN_TYPE pb_walk_stack_align_t;

// Round byte count upwards to a multiple of sizeof(void*) to retain alignment
#define PB_WALK_ALIGNSIZE sizeof(pb_walk_stack_align_t)
#define PB_WALK_ALIGN(size) (pb_walk_stacksize_t)(((size) + PB_WALK_ALIGNSIZE - 1) / PB_WALK_ALIGNSIZE * PB_WALK_ALIGNSIZE)

// Size of internal stack frame in pb_walk().
// This is the overhead per message recursion level.
#define PB_WALK_STACKFRAME_SIZE PB_WALK_ALIGN(sizeof(pb_walk_stackframe_t))

// Declare stack buffer storage for pb_walk() function
#define PB_WALK_DECLARE_STACKBUF(size_) \
    struct {pb_walk_stack_align_t buf[PB_WALK_ALIGN(size_) / sizeof(pb_walk_stack_align_t)];}

// Apply previously declared stack buffer to pb_walk_state_t
#define PB_WALK_SET_STACKBUF(state_, stackbuf_) \
    ((state_)->stack = (char*)&stackbuf_ + sizeof(stackbuf_), \
    (state_)->stack_remain = sizeof(stackbuf_))

/* Data type for storing encoded data and other byte streams.
 * This typedef exists to support platforms where uint8_t does not exist.
 * You can regard it as equivalent on uint8_t on other platforms.
 */
#if defined(PB_BYTE_T_OVERRIDE)
typedef PB_BYTE_T_OVERRIDE pb_byte_t;
#elif defined(UINT8_MAX)
typedef uint8_t pb_byte_t;
#else
typedef unsigned char pb_byte_t;
#endif

/* List of possible field types.
 * These are used in field descriptors to identify the data type
 *
 * Basic information is contained in lowest 8 bits:
 *    Bits 0-3: Scalar type (PB_LTYPE_*)
 *    Bits 4-5: Required/optional/etc. rules (PB_HTYPE_*)
 *    Bits 6-7: Allocation type (PB_ATYPE_*)
 *    Bits 8-10: Reserved
 */
typedef uint16_t pb_type_t;

/**** Field data types ****/

/* Numeric types */
#define PB_LTYPE_BOOL    0x00U /* bool */
#define PB_LTYPE_VARINT  0x01U /* int32, int64, enum */
#define PB_LTYPE_UVARINT 0x02U /* uint32, uint64 */
#define PB_LTYPE_SVARINT 0x03U /* sint32, sint64 */
#define PB_LTYPE_FIXED32 0x04U /* fixed32, sfixed32, float */
#define PB_LTYPE_FIXED64 0x05U /* fixed64, sfixed64, double */

/* Marker for last packable field type. */
#define PB_LTYPE_LAST_PACKABLE 0x05U

/* Byte array with pre-allocated buffer.
 * data_size is the length of the allocated PB_BYTES_ARRAY structure. */
#define PB_LTYPE_BYTES 0x06U

/* String with pre-allocated buffer.
 * data_size is the maximum length. */
#define PB_LTYPE_STRING 0x07U

/* Submessage
 * submsg_fields is pointer to field descriptions */
#define PB_LTYPE_SUBMESSAGE 0x08U

/* Submessage with pre-decoding callback
 * The pre-decoding callback is stored as pb_callback_t right before pSize.
 * submsg_fields is pointer to field descriptions */
#define PB_LTYPE_SUBMSG_W_CB 0x09U

/* Extension pseudo-field
 * The field contains a pointer to pb_extension_t */
#define PB_LTYPE_EXTENSION 0x0AU

/* Byte array with inline, pre-allocated byffer.
 * data_size is the length of the inline, allocated buffer.
 * This differs from PB_LTYPE_BYTES by defining the element as
 * pb_byte_t[data_size] rather than pb_bytes_array_t. */
#define PB_LTYPE_FIXED_LENGTH_BYTES 0x0BU

/* Number of declared LTYPES */
#define PB_LTYPES_COUNT 0x0CU
#define PB_LTYPE_MASK 0x0FU

/**** Field repetition rules ****/

#define PB_HTYPE_REQUIRED 0x00U
#define PB_HTYPE_OPTIONAL 0x10U
#define PB_HTYPE_SINGULAR 0x10U
#define PB_HTYPE_REPEATED 0x20U
#define PB_HTYPE_FIXARRAY 0x20U
#define PB_HTYPE_ONEOF    0x30U
#define PB_HTYPE_MASK     0x30U

/**** Field allocation types ****/

#define PB_ATYPE_STATIC   0x00U
#define PB_ATYPE_POINTER  0x80U
#define PB_ATYPE_CALLBACK 0x40U
#define PB_ATYPE_MASK     0xC0U

#define PB_ATYPE(x) ((x) & PB_ATYPE_MASK)
#define PB_HTYPE(x) ((x) & PB_HTYPE_MASK)
#define PB_LTYPE(x) ((x) & PB_LTYPE_MASK)
#define PB_LTYPE_IS_SUBMSG(x) (PB_LTYPE(x) == PB_LTYPE_SUBMESSAGE || \
                               PB_LTYPE(x) == PB_LTYPE_SUBMSG_W_CB)

/* Data type used for storing field tags. */
#if PB_NO_LARGEMSG
    typedef uint_least16_t pb_tag_t;
#else
    typedef uint32_t pb_tag_t;
#endif

/* Data type used for storing field sizes and array counts.
 * If large descriptor is disabled, all messages are limited to max 4 kB
 * and we can use 16-bit size type.
 *
 * Even when running on 64-bit platforms, we have pb_size_t limited to
 * 32 bits by default. It's not typical to have messages over 4 GB.
 */
#if defined(PB_SIZE_T_OVERRIDE)
    typedef PB_SIZE_T_OVERRIDE pb_size_t;
#elif PB_NO_LARGEMSG
    typedef uint_least16_t pb_size_t;
#elif SIZE_MAX > UINT32_MAX
    typedef uint32_t pb_size_t;
#else
    typedef size_t pb_size_t;
#endif

// Maximum value that can be stored in pb_size_t
#define PB_SIZE_MAX ((pb_size_t)-1)

// The code assumes that any value that fits in pb_size_t
// will also fit in platform size_t.
PB_STATIC_ASSERT(PB_SIZE_MAX <= (size_t)-1, PB_SIZE_T_TOO_BIG)

/* Data type used for storing field indexes.
 * According to Google "Proto Limits", protobuf implementations
 * are typically limited to about 4000 fields per message.
 * Using uint16_t for index works for up to 13000 fields,
 * and saves space in nested message stack frames vs. uint32_t.
 */
typedef uint_least16_t pb_fieldidx_t;

/* Forward declaration of struct types */
typedef struct pb_decode_ctx_s pb_decode_ctx_t;
typedef struct pb_encode_ctx_s pb_encode_ctx_t;
typedef struct pb_field_iter_s pb_field_iter_t;
typedef struct pb_msgdesc_s pb_msgdesc_t;
typedef struct pb_bytes_array_s pb_bytes_array_t;
typedef struct pb_bytes_s pb_bytes_t;
typedef struct pb_callback_s pb_callback_t;
typedef struct pb_extension_s pb_extension_t;
typedef struct pb_walk_state_s pb_walk_state_t;

/* Binary flags related to message descriptor
 * These flags are set by generator and control the
 * encoder/decoder behavior.
 *
 * Flags marked _R_ are recursive: if submessage has the flag,
 * parent message will have it also.
 */
typedef uint16_t pb_msgflag_t;
#define PB_MSGFLAG_LARGEDESC      ((pb_msgflag_t)1 << 0)
#define PB_MSGFLAG_EXTENSIBLE     ((pb_msgflag_t)1 << 1)
#define PB_MSGFLAG_R_HAS_PTRS     ((pb_msgflag_t)1 << 8)
#define PB_MSGFLAG_R_HAS_DEFVAL   ((pb_msgflag_t)1 << 9)
#define PB_MSGFLAG_R_HAS_CBS      ((pb_msgflag_t)1 << 10)
#define PB_MSGFLAG_R_HAS_EXTS     ((pb_msgflag_t)1 << 11)

/* Collect recursive flags from submessages */
#define PB_MSGFLAG_COLLECT(x)     ((x) & 0xFF00)

/* Structure that defines the mapping between protobuf fields and C struct. */
struct pb_msgdesc_s {
    /* Basic information about the message */
    pb_size_t struct_size;
    pb_fieldidx_t field_count;
    pb_fieldidx_t required_field_count;
    pb_tag_t largest_tag;
    pb_msgflag_t msg_flags;

    /* Field_info can be in either long or short format,
     * depending on message flag PB_MSGFLAG_SHORT
     */
    const uint32_t *field_info;

    /* If the message contains submessages, this is a pointer
     * to an array of pointers to their descriptors.
     */
    const pb_msgdesc_t * const * submsg_info;

    /* If the message has proto2 default values, this is a pointer
     * to a serialized protobuf message with them.
     */
    const pb_byte_t *default_value;

    /* If the message uses name-bound callbacks, this is the function
     * pointer to the callback function.
     */
    bool (*field_callback)(pb_decode_ctx_t *decctx, pb_encode_ctx_t *encctx, const pb_field_iter_t *field);
};

/* Iterator for message descriptor */
struct pb_field_iter_s {
    const pb_msgdesc_t *descriptor;  /* Pointer to message descriptor constant */
    void *message;                   /* Pointer to start of the structure */

    pb_fieldidx_t index;                 /* Index of the field */
    pb_fieldidx_t required_field_index;  /* Index that counts only the required fields */
    pb_fieldidx_t submessage_index;      /* Index that counts only submessages */
    pb_fieldidx_t field_info_index;      /* Index to descriptor->field_info array */

    pb_tag_t tag;                    /* Tag of current field */
    pb_size_t data_size;             /* sizeof() of a single item */
    pb_size_t array_size;            /* Number of array entries */
    pb_type_t type;                  /* Type of current field */

    void *pField;                    /* Pointer to current field in struct */
    void *pData;                     /* Pointer to current data contents. Different than pField for arrays and pointers. */
    void *pSize;                     /* Pointer to count/has field */

    const pb_msgdesc_t *submsg_desc; /* For submessage fields, pointer to field descriptor for the submessage. */
};

/* For compatibility with legacy code */
typedef pb_field_iter_t pb_field_t;

/* Make sure that the standard integer types are of the expected sizes.
 * Otherwise fixed32/fixed64 fields can break.
 *
 * If you get errors here, it probably means that your stdint.h is not
 * correct for your platform.
 */
#if !PB_WITHOUT_64BIT
PB_STATIC_ASSERT(sizeof(int64_t) == 2 * sizeof(int32_t), INT64_T_WRONG_SIZE)
PB_STATIC_ASSERT(sizeof(uint64_t) == 2 * sizeof(uint32_t), UINT64_T_WRONG_SIZE)
#endif

/* Make sure CHAR_BIT is set correctly */
PB_STATIC_ASSERT(CHAR_BIT >= 8 &&
    (((unsigned char)-1) >> (CHAR_BIT - 1)) == 1,
    CHAR_BIT_IS_WRONG)

/* This structure is used for statically allocated 'bytes' arrays.
 * It has the number of bytes in the beginning, and after that an array.
 * Note that actual structs used will have a different length of bytes array.
 */
#define PB_BYTES_ARRAY_T(n) struct { pb_size_t size; pb_byte_t bytes[n]; }
#define PB_BYTES_ARRAY_T_ALLOCSIZE(n) ((pb_size_t)((n) + offsetof(pb_bytes_array_t, bytes)))

struct pb_bytes_array_s {
    pb_size_t size;
    pb_byte_t bytes[1];
};

/* This structure is used for pointer-type bytes fields.
 * The length of the bytes field is stored statically in the structure,
 * and the payload pointer points to the raw bytes.
 */
struct pb_bytes_s {
    pb_size_t size;
    pb_byte_t *bytes;
};

/* This structure is used for giving the callback function.
 * It is stored in the message structure and filled in by the method that
 * calls pb_decode.
 *
 * The decoding callback will be given a limited-length stream
 * If the wire type was string, the length is the length of the string.
 * If the wire type was a varint/fixed32/fixed64, the length is the length
 * of the actual value.
 * The function may be called multiple times (especially for repeated types,
 * but also otherwise if the message happens to contain the field multiple
 * times.)
 *
 * The encoding callback will receive the actual output stream.
 * It should write all the data in one call, including the field tag and
 * wire type. It can write multiple fields.
 *
 * The callback can be null if you want to skip a field.
 */
struct pb_callback_s {
    /* Callback functions receive a pointer to the arg field.
     * You can access the value of the field as *arg, and modify it if needed.
     */
    union {
        bool (*decode)(pb_decode_ctx_t *ctx, const pb_field_t *field, void **arg);
        bool (*encode)(pb_encode_ctx_t *ctx, const pb_field_t *field, void * const *arg);
    } funcs;

    /* Free arg for use by callback */
    void *arg;
};

extern bool pb_default_field_callback(pb_decode_ctx_t *decctx, pb_encode_ctx_t *encctx, const pb_field_t *field);

/* Wire types. Library user needs these only in encoder callbacks. */
typedef enum {
    PB_WT_VARINT = 0,
    PB_WT_64BIT  = 1,
    PB_WT_STRING = 2,
    PB_WT_32BIT  = 5,
    PB_WT_PACKED = 255 /* PB_WT_PACKED is internal marker for packed arrays. */
} pb_wire_type_t;

/* Structure for defining the handling of extension fields.
 * The extensions are stored as a linked list, which is searched until
 * matching field tag is found.
 */
struct pb_extension_s {
    /* Message descriptor for the extension field */
    const pb_msgdesc_t *type;

    /* Destination for the decoded data. This must match the datatype
     * of the extension field. */
    void *dest;

    /* Pointer to the next extension handler, or NULL.
     * If this extension does not match a field, the next handler is
     * automatically called. */
    pb_extension_t *next;

    /* The decoder sets this to true if the extension was found.
     * Ignored for encoding. */
    bool found;
};

#define pb_extension_init_zero {NULL,NULL,NULL,false}

#if !PB_NO_CONTEXT_ALLOCATOR
/* Memory allocator can be defined individually per each decoding
 * context using this structure. User implementation can extend
 * the state structure by wrapping it in another struct.
 */
typedef struct pb_allocator_s pb_allocator_t;
struct pb_allocator_s {
    // Allocate or reallocate memory.
    // If ptr is NULL, make a new allocation of 'size' bytes.
    // If ptr is not NULL, adjust the size of the old allocation,
    // or make a new one and copy the old data.
    // On failed allocation, return NULL.
    void* (*realloc)(pb_allocator_t *actx, void *ptr, size_t size);

    // Release previously allocated memory
    void (*free)(pb_allocator_t *actx, void *ptr);

    // Free pointer that can be used by realloc/free implementation
    void *ctx;
};
#endif /* !PB_NO_CONTEXT_ALLOCATOR */

#if !PB_NO_DEFAULT_ALLOCATOR
/* Memory allocation functions to use if context-based allocator
 * is not given. You can define pb_realloc and pb_free to custom
 * functions if you want. */
#  ifndef pb_realloc
#      define pb_realloc(ptr, size) realloc(ptr, size)
#  endif
#  ifndef pb_free
#      define pb_free(ptr) free(ptr)
#  endif
#endif /* !PB_NO_DEFAULT_ALLOCATOR */

// Type used to store stack sizes internally in pb_walk().
// By default up to 64 kB can be allocated per recursion level,
// which is enough for about 1000 nested messages. Protobuf standard
// generally recommends a maximum of 100 nested messages.
#ifndef PB_WALK_STACKSIZE_TYPE_OVERRIDE
typedef uint_least16_t pb_walk_stacksize_t;
#else
typedef PB_WALK_STACKSIZE_TYPE_OVERRIDE pb_walk_stacksize_t;
#endif

/* This structure is used by pb_walk() for storing information
 * for each submessage level. It shouldn't be directly accessed
 * by user code, but the definition is provided so that sizeof()
 * can by used in PB_WALK_STACKFRAME_SIZE definition.
 */
typedef struct {
    /* Message descriptor and structure */
    const pb_msgdesc_t *descriptor;
    void *message;

    /* pb_field_iter_t state */
    pb_fieldidx_t index;
    pb_fieldidx_t required_field_index;
    pb_fieldidx_t submessage_index;

    /* Size of the previous user stackframe */
    pb_walk_stacksize_t prev_stacksize;
} pb_walk_stackframe_t;

/* This is used to inform about need to regenerate .pb.h/.pb.c files. */
#define PB_PROTO_HEADER_VERSION 93

/* These macros are used to declare pb_field_t's in the constant array. */
/* Size of a structure member, in bytes. */
#define pb_membersize(st, m) (sizeof ((st*)0)->m)
/* Number of entries in an array. */
#define pb_arraysize(st, m) (pb_membersize(st, m) / pb_membersize(st, m[0]))
/* Delta from start of one member to the start of another member. */
#define pb_delta(st, m1, m2) ((int)offsetof(st, m1) - (int)offsetof(st, m2))

/* Force expansion of macro value */
#define PB_EXPAND(x) x

/* Cast away const from pointer type.
 * This is used so that the same iterator logic can be used for both encoding
 * and decoding functions.
 */
#define PB_CONST_CAST(x) ((void*)(uintptr_t)(x))

/* Binding of a message field set into a specific structure
 * This calls the other macros to generate a pb_msgdesc_t.
 */
#define PB_BIND(msgname, structname, width) \
    const uint32_t structname ## _field_info[] PB_PROGMEM = \
    { \
        msgname ## _FIELDLIST(PB_GEN_FIELD_INFO_ ## width, structname) \
        0 \
    }; \
    const pb_msgdesc_t* const structname ## _submsg_info[] = \
    { \
        msgname ## _FIELDLIST(PB_GEN_SUBMSG_INFO, structname) \
        NULL \
    }; \
    const pb_msgdesc_t structname ## _msg = \
    { \
       sizeof(structname), \
       0 msgname ## _FIELDLIST(PB_GEN_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(PB_GEN_REQ_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(PB_GEN_LARGEST_TAG, structname), \
       (msgname ## _FLAGS) | (PB_MSGFLAG_DESCWIDTH_ ## width), \
       structname ## _field_info, \
       structname ## _submsg_info, \
       msgname ## _DEFAULT, \
       msgname ## _CALLBACK, \
    }; \
    msgname ## _FIELDLIST(PB_GEN_FIELD_INFO_ASSERT_ ## width, structname)

#if !PB_NO_LARGEMSG
#define PB_MSGFLAG_DESCWIDTH_S 0
#define PB_MSGFLAG_DESCWIDTH_L PB_MSGFLAG_LARGEDESC
#else
#define PB_MSGFLAG_DESCWIDTH_S 0
#define PB_MSGFLAG_DESCWIDTH_L 0
#endif

#define PB_GEN_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) +1
#define PB_GEN_REQ_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) \
    + (PB_HTYPE_ ## htype == PB_HTYPE_REQUIRED)
#define PB_GEN_LARGEST_TAG(structname, atype, htype, ltype, fieldname, tag) \
    * 0 + tag

/* X-macro for generating the entries in struct_field_info[] array. */
#define PB_GEN_FIELD_INFO_S(structname, atype, htype, ltype, fieldname, tag) \
    PB_FIELDINFO_S( \
                   tag, \
                   PB_ATYPE_ ## atype | PB_HTYPE_ ## htype | PB_LTYPE_MAP_ ## ltype, \
                   PB_DATA_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_DATA_SIZE_ ## atype(_PB_HTYPE_ ## htype, _PB_LTYPE_ ## ltype, structname, fieldname), \
                   PB_SIZE_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_ARRAY_SIZE_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname))

#if !PB_NO_LARGEMSG
#define PB_GEN_FIELD_INFO_L(structname, atype, htype, ltype, fieldname, tag) \
    PB_FIELDINFO_L( \
                   tag, \
                   PB_ATYPE_ ## atype | PB_HTYPE_ ## htype | PB_LTYPE_MAP_ ## ltype, \
                   PB_DATA_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_DATA_SIZE_ ## atype(_PB_HTYPE_ ## htype, _PB_LTYPE_ ## ltype, structname, fieldname), \
                   PB_SIZE_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_ARRAY_SIZE_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname))
#else
#define PB_GEN_FIELD_INFO_L PB_GEN_FIELD_INFO_S
#endif

/* X-macro for generating asserts that entries fit in struct_field_info[] array.
 * This must match the GEN_FIELD_INFO macros above.
 *
 * If you get static assertion failure here, it probably means you have defined
 * PB_NO_LARGEMSG, which limits fields to max. 4096 tag/size.
 *
 * Otherwise errors here indicate a bug in the generator. It may be possible to
 * work around it by setting '(nanopb).descriptorsize = DS_LARGE' on the field.
 */
#define PB_GEN_FIELD_INFO_ASSERT_S(structname, atype, htype, ltype, fieldname, tag) \
    PB_FIELDINFO_ASSERT_S( \
                   tag, \
                   PB_ATYPE_ ## atype | PB_HTYPE_ ## htype | PB_LTYPE_MAP_ ## ltype, \
                   PB_DATA_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_DATA_SIZE_ ## atype(_PB_HTYPE_ ## htype, _PB_LTYPE_ ## ltype, structname, fieldname), \
                   PB_SIZE_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_ARRAY_SIZE_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname))

#if !PB_NO_LARGEMSG
#define PB_GEN_FIELD_INFO_ASSERT_L(structname, atype, htype, ltype, fieldname, tag) \
    PB_FIELDINFO_ASSERT_L( \
                   tag, \
                   PB_ATYPE_ ## atype | PB_HTYPE_ ## htype | PB_LTYPE_MAP_ ## ltype, \
                   PB_DATA_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_DATA_SIZE_ ## atype(_PB_HTYPE_ ## htype, _PB_LTYPE_ ## ltype, structname, fieldname), \
                   PB_SIZE_OFFSET_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname), \
                   PB_ARRAY_SIZE_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname))
#else
#define PB_GEN_FIELD_INFO_ASSERT_L PB_GEN_FIELD_INFO_ASSERT_S
#endif

/* The macros below handle different data storage formats (static/callback/ptr) and
 * handle the collecting of struct field properties for them.
 */

/* Offset to data field from beginning of message. */
#define PB_DATA_OFFSET_STATIC(htype, structname, fieldname) PB_DO ## htype(structname, fieldname)
#define PB_DATA_OFFSET_POINTER(htype, structname, fieldname) PB_DO ## htype(structname, fieldname)
#define PB_DATA_OFFSET_CALLBACK(htype, structname, fieldname) PB_DO ## htype(structname, fieldname)
#define PB_DO_PB_HTYPE_REQUIRED(structname, fieldname) offsetof(structname, fieldname)
#define PB_DO_PB_HTYPE_SINGULAR(structname, fieldname) offsetof(structname, fieldname)
#define PB_DO_PB_HTYPE_ONEOF(structname, fieldname) offsetof(structname, PB_ONEOF_NAME(FULL, fieldname))
#define PB_DO_PB_HTYPE_OPTIONAL(structname, fieldname) offsetof(structname, fieldname)
#define PB_DO_PB_HTYPE_REPEATED(structname, fieldname) offsetof(structname, fieldname)
#define PB_DO_PB_HTYPE_FIXARRAY(structname, fieldname) offsetof(structname, fieldname)

/* Offset to size field from beginning of message.
 * If there is no size field, this is same as data_offset. */
#define PB_SIZE_OFFSET_STATIC(htype, structname, fieldname) PB_SO ## htype(structname, fieldname)
#define PB_SIZE_OFFSET_POINTER(htype, structname, fieldname) PB_SO_PTR ## htype(structname, fieldname)
#define PB_SIZE_OFFSET_CALLBACK(htype, structname, fieldname) PB_SO_CB ## htype(structname, fieldname)
#define PB_SO_PB_HTYPE_REQUIRED(structname, fieldname)      offsetof(structname, fieldname)
#define PB_SO_PB_HTYPE_SINGULAR(structname, fieldname)      offsetof(structname, fieldname)
#define PB_SO_PB_HTYPE_ONEOF(structname, fieldname)         PB_SO_PB_HTYPE_ONEOF2(structname, PB_ONEOF_NAME(UNION, fieldname))
#define PB_SO_PB_HTYPE_ONEOF2(structname, unionname)        PB_SO_PB_HTYPE_ONEOF3(structname, unionname)
#define PB_SO_PB_HTYPE_ONEOF3(structname, unionname)        offsetof(structname, which_ ## unionname)
#define PB_SO_PB_HTYPE_OPTIONAL(structname, fieldname)      offsetof(structname, has_ ## fieldname)
#define PB_SO_PB_HTYPE_REPEATED(structname, fieldname)      offsetof(structname, fieldname ## _count)
#define PB_SO_PB_HTYPE_FIXARRAY(structname, fieldname)      offsetof(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_REQUIRED(structname, fieldname)  offsetof(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_SINGULAR(structname, fieldname)  offsetof(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_ONEOF(structname, fieldname)     PB_SO_PB_HTYPE_ONEOF(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_OPTIONAL(structname, fieldname)  offsetof(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_REPEATED(structname, fieldname)  PB_SO_PB_HTYPE_REPEATED(structname, fieldname)
#define PB_SO_PTR_PB_HTYPE_FIXARRAY(structname, fieldname)  offsetof(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_REQUIRED(structname, fieldname)   offsetof(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_SINGULAR(structname, fieldname)   offsetof(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_ONEOF(structname, fieldname)      PB_SO_PB_HTYPE_ONEOF(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_OPTIONAL(structname, fieldname)   offsetof(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_REPEATED(structname, fieldname)   offsetof(structname, fieldname)
#define PB_SO_CB_PB_HTYPE_FIXARRAY(structname, fieldname)   offsetof(structname, fieldname)

/* Array size of a repeated field */
#define PB_ARRAY_SIZE_STATIC(htype, structname, fieldname) PB_AS ## htype(structname, fieldname)
#define PB_ARRAY_SIZE_POINTER(htype, structname, fieldname) PB_AS_PTR ## htype(structname, fieldname)
#define PB_ARRAY_SIZE_CALLBACK(htype, structname, fieldname) 1
#define PB_AS_PB_HTYPE_REQUIRED(structname, fieldname) 1
#define PB_AS_PB_HTYPE_SINGULAR(structname, fieldname) 1
#define PB_AS_PB_HTYPE_OPTIONAL(structname, fieldname) 1
#define PB_AS_PB_HTYPE_ONEOF(structname, fieldname) 1
#define PB_AS_PB_HTYPE_REPEATED(structname, fieldname) pb_arraysize(structname, fieldname)
#define PB_AS_PB_HTYPE_FIXARRAY(structname, fieldname) pb_arraysize(structname, fieldname)
#define PB_AS_PTR_PB_HTYPE_REQUIRED(structname, fieldname) 1
#define PB_AS_PTR_PB_HTYPE_SINGULAR(structname, fieldname) 1
#define PB_AS_PTR_PB_HTYPE_OPTIONAL(structname, fieldname) 1
#define PB_AS_PTR_PB_HTYPE_ONEOF(structname, fieldname) 1
#define PB_AS_PTR_PB_HTYPE_REPEATED(structname, fieldname) 1
#define PB_AS_PTR_PB_HTYPE_FIXARRAY(structname, fieldname) pb_arraysize(structname, fieldname[0])

/* Data size of the field */
#define PB_DATA_SIZE_STATIC(htype, ltype, structname, fieldname) PB_DS ## htype(structname, fieldname)
#define PB_DATA_SIZE_POINTER(htype, ltype, structname, fieldname) PB_DS_PTR ## htype(ltype, structname, fieldname)
#define PB_DATA_SIZE_CALLBACK(htype, ltype, structname, fieldname) PB_DS_CB ## htype(structname, fieldname)
#define PB_DS_PB_HTYPE_REQUIRED(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_PB_HTYPE_SINGULAR(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_PB_HTYPE_OPTIONAL(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_PB_HTYPE_ONEOF(structname, fieldname) pb_membersize(structname, PB_ONEOF_NAME(FULL, fieldname))
#define PB_DS_PB_HTYPE_REPEATED(structname, fieldname) pb_membersize(structname, fieldname[0])
#define PB_DS_PB_HTYPE_FIXARRAY(structname, fieldname) pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_HTYPE_REQUIRED(ltype, structname, fieldname) PB_DS_PTR ## ltype(structname, fieldname)
#define PB_DS_PTR_PB_HTYPE_SINGULAR(ltype, structname, fieldname) PB_DS_PTR ## ltype(structname, fieldname)
#define PB_DS_PTR_PB_HTYPE_OPTIONAL(ltype, structname, fieldname) PB_DS_PTR ## ltype(structname, fieldname)
#define PB_DS_PTR_PB_HTYPE_ONEOF(ltype, structname, fieldname) PB_DS_PTR ## ltype(structname, PB_ONEOF_NAME(FULL, fieldname))
#define PB_DS_PTR_PB_HTYPE_REPEATED(ltype, structname, fieldname) pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_HTYPE_FIXARRAY(ltype, structname, fieldname) pb_membersize(structname, fieldname[0][0])
#define PB_DS_CB_PB_HTYPE_REQUIRED(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_CB_PB_HTYPE_SINGULAR(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_CB_PB_HTYPE_OPTIONAL(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_CB_PB_HTYPE_ONEOF(structname, fieldname) pb_membersize(structname, PB_ONEOF_NAME(FULL, fieldname))
#define PB_DS_CB_PB_HTYPE_REPEATED(structname, fieldname) pb_membersize(structname, fieldname)
#define PB_DS_CB_PB_HTYPE_FIXARRAY(structname, fieldname) pb_membersize(structname, fieldname)

/* Because PB_LTYPE_BYTES with pointer allocation uses a special wrapper struct,
 * we need to specialize the data size based on LTYPE */
#define PB_DS_PTR_PB_LTYPE_BOOL(structname, fieldname)               pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_BYTES(structname, fieldname)              pb_membersize(structname, fieldname)
#define PB_DS_PTR_PB_LTYPE_DOUBLE(structname, fieldname)             pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_ENUM(structname, fieldname)               pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_UENUM(structname, fieldname)              pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_FIXED32(structname, fieldname)            pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_FIXED64(structname, fieldname)            pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_FLOAT(structname, fieldname)              pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_INT32(structname, fieldname)              pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_INT64(structname, fieldname)              pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_MESSAGE(structname, fieldname)            pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_MSG_W_CB(structname, fieldname)           pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_SFIXED32(structname, fieldname)           pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_SFIXED64(structname, fieldname)           pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_SINT32(structname, fieldname)             pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_SINT64(structname, fieldname)             pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_STRING(structname, fieldname)             pb_membersize(structname, fieldname)
#define PB_DS_PTR_PB_LTYPE_UINT32(structname, fieldname)             pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_UINT64(structname, fieldname)             pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_EXTENSION(structname, fieldname)          pb_membersize(structname, fieldname[0])
#define PB_DS_PTR_PB_LTYPE_FIXED_LENGTH_BYTES(structname, fieldname) pb_membersize(structname, fieldname[0])

/* Oneof names are given as a tuple of (unionname, membername, fullname)
 * These macros allow to access the members.
 */
#define PB_ONEOF_NAME(type, tuple) PB_EXPAND(PB_ONEOF_NAME_ ## type tuple)
#define PB_ONEOF_NAME_UNION(unionname,membername,fullname) unionname
#define PB_ONEOF_NAME_MEMBER(unionname,membername,fullname) membername
#define PB_ONEOF_NAME_FULL(unionname,membername,fullname) fullname

/* Generate array of pointers to the submessage descriptors */
#define PB_GEN_SUBMSG_INFO(structname, atype, htype, ltype, fieldname, tag) \
    PB_SUBMSG_INFO_ ## htype(_PB_LTYPE_ ## ltype, structname, fieldname)
#define PB_SUBMSG_INFO_REQUIRED(ltype, structname, fieldname) PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PB_SUBMSG_INFO_SINGULAR(ltype, structname, fieldname) PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PB_SUBMSG_INFO_OPTIONAL(ltype, structname, fieldname) PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PB_SUBMSG_INFO_ONEOF(ltype, structname, fieldname) PB_SUBMSG_INFO_ONEOF2(ltype, structname, PB_ONEOF_NAME(UNION, fieldname), PB_ONEOF_NAME(MEMBER, fieldname))
#define PB_SUBMSG_INFO_ONEOF2(ltype, structname, unionname, membername) PB_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername)
#define PB_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername) PB_SI ## ltype(structname ## _ ## unionname ## _ ## membername ## _MSGTYPE)
#define PB_SUBMSG_INFO_REPEATED(ltype, structname, fieldname) PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PB_SUBMSG_INFO_FIXARRAY(ltype, structname, fieldname) PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define PB_SI_PB_LTYPE_BOOL(t)
#define PB_SI_PB_LTYPE_BYTES(t)
#define PB_SI_PB_LTYPE_DOUBLE(t)
#define PB_SI_PB_LTYPE_ENUM(t)
#define PB_SI_PB_LTYPE_UENUM(t)
#define PB_SI_PB_LTYPE_FIXED32(t)
#define PB_SI_PB_LTYPE_FIXED64(t)
#define PB_SI_PB_LTYPE_FLOAT(t)
#define PB_SI_PB_LTYPE_INT32(t)
#define PB_SI_PB_LTYPE_INT64(t)
#define PB_SI_PB_LTYPE_MESSAGE(t)  PB_SUBMSG_DESCRIPTOR(t)
#define PB_SI_PB_LTYPE_MSG_W_CB(t) PB_SUBMSG_DESCRIPTOR(t)
#define PB_SI_PB_LTYPE_SFIXED32(t)
#define PB_SI_PB_LTYPE_SFIXED64(t)
#define PB_SI_PB_LTYPE_SINT32(t)
#define PB_SI_PB_LTYPE_SINT64(t)
#define PB_SI_PB_LTYPE_STRING(t)
#define PB_SI_PB_LTYPE_UINT32(t)
#define PB_SI_PB_LTYPE_UINT64(t)
#define PB_SI_PB_LTYPE_EXTENSION(t)
#define PB_SI_PB_LTYPE_FIXED_LENGTH_BYTES(t)
#define PB_SUBMSG_DESCRIPTOR(t)    &(t ## _msg),

/* Depending on the message type, the field descriptors can be stored in
 * either a narrow (2 words) or wide (5 words) format. The format is
 * selected by PB_MSGFLAG_LARGEDESC in pb_msgdesc_t.
 *
 * To simplify iterating, type and tag are at same location in both
 * formats.
 *
 * Formats are shown starting with the LSB of the first word.
 *
 * 2 words: [12-bit tag] [12-bit data_offset] [8-bit array_size]
 *          [12-bit data_size] [12-bit size_offset] [8-bit type]
 *
 * 5 words: [29-bit tag] [3 bits type upper]
 *          [24-bit array_size] [8-bit type]
 *          [32-bit data_size]
 *          [32-bit size_offset]
 *          [32-bit data_offset]
 */

#define PB_FIELDINFO_S(tag, type, data_offset, data_size, size_offset, array_size) \
    (((uint32_t)(tag)) | ((uint32_t)(data_offset) << 12) | ((uint32_t)(array_size) << 24)), \
    (((uint32_t)(data_size)) | ((uint32_t)(size_offset) << 12) | ((uint32_t)(type) << 24)),

#define PB_FIELDINFO_L(tag, type, data_offset, data_size, size_offset, array_size) \
    (((uint32_t)(tag)) | ((uint32_t)((type) >> 8) << 29)), \
    (((uint32_t)(array_size)) | ((uint32_t)((type) & 0xFF) << 24)), \
    ((uint32_t)(data_size)), \
    ((uint32_t)(size_offset)), \
    ((uint32_t)(data_offset)),

/* These assertions verify that the field information fits in the allocated space.
 * The generator tries to automatically determine the correct width that can fit all
 * data associated with a message. These asserts will fail only if there has been a
 * problem in the automatic logic - this may be worth reporting as a bug. As a workaround,
 * you can increase the descriptor width by setting descriptorsize option in .options file.
 */
#define PB_FITS(value,bits) (((uint32_t)(value) >> bits) == 0)

#define PB_FIELDINFO_ASSERT_S(tag, type, data_offset, data_size, size_offset, array_size) \
    PB_STATIC_ASSERT( \
        PB_FITS(tag, 12) && \
        PB_FITS(type, 8) && \
        PB_FITS(data_offset, 12) && \
        PB_FITS(data_size, 12) && \
        PB_FITS(array_size, 8) && \
        PB_FITS(size_offset, 12), \
        FIELDINFO_DOES_NOT_FIT_width2_field ## tag \
    )

#define PB_FIELDINFO_ASSERT_L(tag, type, data_offset, data_size, size_offset, array_size) \
    PB_STATIC_ASSERT( \
        PB_FITS(tag, 29) && \
        PB_FITS(type, 11) && \
        PB_FITS(array_size, 24), \
        FIELDINFO_DOES_NOT_FIT_width5_field ## tag \
    )

/* The mapping from protobuf types to LTYPEs is done using these macros. */
#define PB_LTYPE_MAP_BOOL               PB_LTYPE_BOOL
#define PB_LTYPE_MAP_BYTES              PB_LTYPE_BYTES
#define PB_LTYPE_MAP_DOUBLE             PB_LTYPE_FIXED64
#define PB_LTYPE_MAP_ENUM               PB_LTYPE_VARINT
#define PB_LTYPE_MAP_UENUM              PB_LTYPE_UVARINT
#define PB_LTYPE_MAP_FIXED32            PB_LTYPE_FIXED32
#define PB_LTYPE_MAP_FIXED64            PB_LTYPE_FIXED64
#define PB_LTYPE_MAP_FLOAT              PB_LTYPE_FIXED32
#define PB_LTYPE_MAP_INT32              PB_LTYPE_VARINT
#define PB_LTYPE_MAP_INT64              PB_LTYPE_VARINT
#define PB_LTYPE_MAP_MESSAGE            PB_LTYPE_SUBMESSAGE
#define PB_LTYPE_MAP_MSG_W_CB           PB_LTYPE_SUBMSG_W_CB
#define PB_LTYPE_MAP_SFIXED32           PB_LTYPE_FIXED32
#define PB_LTYPE_MAP_SFIXED64           PB_LTYPE_FIXED64
#define PB_LTYPE_MAP_SINT32             PB_LTYPE_SVARINT
#define PB_LTYPE_MAP_SINT64             PB_LTYPE_SVARINT
#define PB_LTYPE_MAP_STRING             PB_LTYPE_STRING
#define PB_LTYPE_MAP_UINT32             PB_LTYPE_UVARINT
#define PB_LTYPE_MAP_UINT64             PB_LTYPE_UVARINT
#define PB_LTYPE_MAP_EXTENSION          PB_LTYPE_EXTENSION
#define PB_LTYPE_MAP_FIXED_LENGTH_BYTES PB_LTYPE_FIXED_LENGTH_BYTES

/* These macros are used for giving out error messages.
 * They are mostly a debugging aid; the main error information
 * is the true/false return value from functions.
 * Some code space can be saved by disabling the error
 * messages if not used.
 *
 * PB_SET_ERROR() sets the error message if none has been set yet.
 *                msg must be a constant string literal.
 * PB_GET_ERROR() always returns a pointer to a string.
 * PB_RETURN_ERROR() sets the error and returns false from current
 *                   function.
 */
#if PB_NO_ERRMSG
#define PB_SET_ERROR(ctx, msg) PB_UNUSED(ctx)
#define PB_GET_ERROR(ctx) "(errmsg disabled)"
#else
#define PB_SET_ERROR(ctx, msg) ((ctx)->errmsg = (ctx)->errmsg ? (ctx)->errmsg : (msg))
#define PB_GET_ERROR(ctx) ((ctx)->errmsg ? (ctx)->errmsg : "(none)")
#endif

#define PB_RETURN_ERROR(ctx, msg) return PB_SET_ERROR(ctx, msg), false

/* API compatibility defines for code written before nanopb-1.0.0 */
#if PB_API_VERSION < PB_API_VERSION_v1_0

/* Streams are similar to new context concept */
typedef pb_decode_ctx_t pb_istream_t;
typedef pb_encode_ctx_t pb_ostream_t;

/* Arguments for PB_BIND() can be remapped */
#define PB_GEN_FIELD_INFO_AUTO PB_GEN_FIELD_INFO_S
#define PB_GEN_FIELD_INFO_1    PB_GEN_FIELD_INFO_S
#define PB_GEN_FIELD_INFO_2    PB_GEN_FIELD_INFO_S
#define PB_GEN_FIELD_INFO_4    PB_GEN_FIELD_INFO_L
#define PB_GEN_FIELD_INFO_8    PB_GEN_FIELD_INFO_L
#define PB_GEN_FIELD_INFO_ASSERT_AUTO PB_GEN_FIELD_INFO_ASSERT_S
#define PB_GEN_FIELD_INFO_ASSERT_1    PB_GEN_FIELD_INFO_ASSERT_S
#define PB_GEN_FIELD_INFO_ASSERT_2    PB_GEN_FIELD_INFO_ASSERT_S
#define PB_GEN_FIELD_INFO_ASSERT_4    PB_GEN_FIELD_INFO_ASSERT_L
#define PB_GEN_FIELD_INFO_ASSERT_8    PB_GEN_FIELD_INFO_ASSERT_L
#define PB_MSGFLAG_DESCWIDTH_AUTO 0
#define PB_MSGFLAG_DESCWIDTH_1 0
#define PB_MSGFLAG_DESCWIDTH_2 0
#define PB_MSGFLAG_DESCWIDTH_4 PB_MSGFLAG_LARGEDESC
#define PB_MSGFLAG_DESCWIDTH_8 PB_MSGFLAG_LARGEDESC

#endif /* PB_API_VERSION */

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
#if __cplusplus >= 201103L
#define PB_CONSTEXPR constexpr
#else  // __cplusplus >= 201103L
#define PB_CONSTEXPR
#endif  // __cplusplus >= 201103L

#if __cplusplus >= 201703L
#define PB_INLINE_CONSTEXPR inline constexpr
#else  // __cplusplus >= 201703L
#define PB_INLINE_CONSTEXPR PB_CONSTEXPR
#endif  // __cplusplus >= 201703L

extern "C++"
{
namespace nanopb {
// Each type will be partially specialized by the generator.
template <typename GenMessageT> struct MessageDescriptor;
}  // namespace nanopb
}
#endif  /* __cplusplus */

#endif
