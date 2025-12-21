/* pb_encode.h: Functions to encode protocol buffers. Depends on pb_encode.c.
 * The main function is pb_encode. You also need an output stream, and the
 * field descriptions created by nanopb_generator.py.
 */

#ifndef PB_ENCODE_H_INCLUDED
#define PB_ENCODE_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PB_NO_STREAM_CALLBACK
/* Callback pointer for custom stream types. This can write the
 * bytes directly to your storage, which can be for example
 * a file or a network socket.
 *
 * The callback must conform to these rules:
 *
 * 1) Return false on IO errors. This will cause encoding to abort.
 * 2) You can use state to store your own data (e.g. buffer pointer).
 * 3) pb_write will update bytes_written after your callback runs.
 * 4) Substreams will modify max_size and bytes_written. Don't use them
 *    to calculate any pointers. The ctx pointer remains the same even
 *    for substreams.
 */
typedef bool (*pb_encode_ctx_write_callback_t)(pb_encode_ctx_t *ctx, const pb_byte_t *buf, size_t count);
#endif

/* Flags for encode context state */
typedef uint16_t pb_encode_ctx_flags_t;

// PB_ENCODE_CTX_FLAG_SIZING: Don't write output data, just compute size
// When this flag is set, only bytes_written is modified.
#define PB_ENCODE_CTX_FLAG_SIZING            (pb_encode_ctx_flags_t)(1 << 0)

// PB_ENCODE_CTX_FLAG_DELIMITED: Write data length as varint before the main message.
// Corresponds to writeDelimitedTo() in Google's protobuf API.
#define PB_ENCODE_CTX_FLAG_DELIMITED         (pb_encode_ctx_flags_t)(1 << 1)

// PB_ENCODE_CTX_FLAG_NULLTERMINATED: Append a null byte after the message for termination.
// NOTE: This behavior is not supported in most other protobuf implementations.
#define PB_ENCODE_CTX_FLAG_NULLTERMINATED    (pb_encode_ctx_flags_t)(1 << 2)

// PB_ENCODE_CTX_FLAG_ONEPASS_SIZING: Write output data normally, but if
// callback would get invoked, switch to SIZING mode instead. Has no effect
// for memory buffer streams. This flag is used internally to optimize encoding
// performance.
#define PB_ENCODE_CTX_FLAG_ONEPASS_SIZING    (pb_encode_ctx_flags_t)(1 << 3)

/* Structure containing the state associated with message encoding.
 * For the common case of writing a message to a memory buffer, this
 * is initialized with pb_init_encode_ctx_for_buffer().
 */
struct pb_encode_ctx_s
{
#ifndef PB_NO_STREAM_CALLBACK
    // Optional callback function for writing to output directly, instead
    // of the memory buffer. State is a free field for use by the callback.
    // It's also allowed to extend the pb_encode_ctx_t struct with your own
    // fields by wrapping it.
    pb_encode_ctx_write_callback_t callback;
    void *state;
#endif

    // Limit number of output bytes written. Can be set to SIZE_MAX.
    size_t max_size;

    // Number of bytes written so far.
    size_t bytes_written;
    
#ifndef PB_NO_ERRMSG
    /* Pointer to constant (ROM) string when decoding function returns error */
    const char *errmsg;
#endif

    // Flags that affect encoding behavior, combination of PB_ENCODE_CTX_FLAG_*
    pb_encode_ctx_flags_t flags;

    // Memory buffer used to store encoded data.
    // If callback is provided, this is optional and used as cache.
    pb_byte_t *buffer;

#ifndef PB_NO_STREAM_CALLBACK
    // Total size of memory buffer
    // If callback is not used, this is equal to max_size.
    size_t buffer_size;

    // Number of bytes currently in memory buffer
    // If callback is not used, this is equal to bytes_written.
    size_t buffer_count;
#endif
};

/***************************
 * Main encoding functions *
 ***************************/

/* Prefer using pb_encode() macro instead of these functions.
 * The macro passes struct_size automatically, giving some amount
 * of type safety of pb_msgdesc_t pointer vs. wrong struct type.
 * If you need to handle void pointers without knowing the size,
 * you can call pb_encode_s() directly with struct_size = 0.
 */
bool pb_encode_s(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields,
                 const void *src_struct, size_t struct_size);
bool pb_get_encoded_size_s(size_t *size, const pb_msgdesc_t *fields,
                           const void *src_struct, size_t struct_size);

/* Encode a single protocol buffers message from C structure into a stream.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by src_struct must match the description in fields.
 * All required fields in the struct are assumed to have been filled in.
 *
 * Example usage:
 *    MyMessage msg = {};
 *    uint8_t buffer[64];
 *    pb_ostream_t stream;
 *
 *    msg.field1 = 42;
 *    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
 *    pb_encode(&stream, MyMessage_fields, &msg);
 */
#define pb_encode(ctx, fields, src_struct) pb_encode_s(ctx, fields, src_struct, sizeof(*src_struct))

/* Encode the message to get the size of the encoded data, but do not store
 * the data. */
#define pb_get_encoded_size(size, fields, src_struct) pb_get_encoded_size_s(size, fields, src_struct, sizeof(*src_struct))

/**************************************
 * Functions for manipulating streams *
 **************************************/

/* Create an encode context for reading from a memory buffer.
 *
 * The number of bytes written can be found in ctx.bytes_written after
 * encoding the message.
 *
 * Alternatively, you can use a custom stream that writes directly to e.g.
 * a file or a network socket.
 */
void pb_init_encode_ctx_for_buffer(pb_encode_ctx_t *ctx, pb_byte_t *buf, size_t bufsize);

#ifndef PB_NO_STREAM_CALLBACK
/* Create encode context for a stream with a callback function.
 * State is a free pointer for use by the callback.
 * A memory buffer can optionally be provided for caching.
 */
void pb_init_encode_ctx_for_callback(pb_encode_ctx_t *ctx,
    pb_encode_ctx_write_callback_t callback, void *state,
    size_t max_size, pb_byte_t *buf, size_t bufsize);
#endif


/* Create encode context that writes nothing, just computes the size */
void pb_init_encode_ctx_sizing(pb_encode_ctx_t *ctx);

/* When using callback stream with a memory buffer, the writes might not be
 * immediately flushed to the callback. pb_encode() automatically flushes
 * the buffer at the end of encoding. If you use pb_write() manually, call
 * pb_flush_write_buffer() afterwards.
 */
bool pb_flush_write_buffer(pb_encode_ctx_t *ctx);

/* Function to write into a pb_ostream_t stream. You can use this if you need
 * to append or prepend some custom headers to the message.
 */
bool pb_write(pb_encode_ctx_t *ctx, const pb_byte_t *buf, size_t count);

/************************************************
 * Helper functions for writing field callbacks *
 ************************************************/

/* Encode field header based on type and field number defined in the field
 * structure. Call this from the callback before writing out field contents. */
bool pb_encode_tag_for_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);

/* Encode field header by manually specifying wire type. You need to use this
 * if you want to write out packed arrays from a callback field. */
bool pb_encode_tag(pb_encode_ctx_t *ctx, pb_wire_type_t wiretype, pb_tag_t field_number);

/* Encode an integer in the varint format.
 * This works for bool, enum, int32, int64, uint32 and uint64 field types. */
#ifndef PB_WITHOUT_64BIT
bool pb_encode_varint(pb_encode_ctx_t *ctx, uint64_t value);
#else
bool pb_encode_varint(pb_encode_ctx_t *ctx, uint32_t value);
#endif

/* Encode an integer in the zig-zagged svarint format.
 * This works for sint32 and sint64. */
#ifndef PB_WITHOUT_64BIT
bool pb_encode_svarint(pb_encode_ctx_t *ctx, int64_t value);
#else
bool pb_encode_svarint(pb_encode_ctx_t *ctx, int32_t value);
#endif

/* Encode a string or bytes type field. For strings, pass strlen(s) as size. */
bool pb_encode_string(pb_encode_ctx_t *ctx, const pb_byte_t *buffer, size_t size);

/* Encode a fixed32, sfixed32 or float value.
 * You need to pass a pointer to a 4-byte wide C variable. */
bool pb_encode_fixed32(pb_encode_ctx_t *ctx, const void *value);

#ifndef PB_WITHOUT_64BIT
/* Encode a fixed64, sfixed64 or double value.
 * You need to pass a pointer to a 8-byte wide C variable. */
bool pb_encode_fixed64(pb_encode_ctx_t *ctx, const void *value);
#endif

#ifdef PB_CONVERT_DOUBLE_FLOAT
/* Encode a float value so that it appears like a double in the encoded
 * message. */
bool pb_encode_float_as_double(pb_encode_ctx_t *ctx, float value);
#endif

/* Encode a submessage field.
 * You need to pass the pb_field_t array and pointer to struct, just like
 * with pb_encode(). This internally encodes the submessage twice, first to
 * calculate message size and then to actually write it out.
 */
bool pb_encode_submessage(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields, const void *src_struct);

/* API compatibility defines for code written before nanopb-1.0.0 */
#if PB_API_VERSION < PB_API_VERSION_v1_0

static inline pb_ostream_t pb_ostream_from_buffer(pb_byte_t *buf, size_t bufsize)
{
    pb_ostream_t ctx;
    pb_init_encode_ctx_for_buffer(&ctx, buf, bufsize);
    return ctx;
}

/* PB_OSTREAM_SIZING has been replaced by pb_init_encode_ctx_sizing() */
#ifndef PB_NO_STREAM_CALLBACK
# ifndef PB_NO_ERRMSG
#  define PB_OSTREAM_SIZING {NULL, NULL, 0, 0, NULL, PB_ENCODE_CTX_FLAG_SIZING, NULL, 0, 0}
# else
#  define PB_OSTREAM_SIZING {NULL, NULL, 0, 0, PB_ENCODE_CTX_FLAG_SIZING, NULL, 0, 0}
# endif
#else
# ifndef PB_NO_ERRMSG
#  define PB_OSTREAM_SIZING {0, 0, NULL, PB_ENCODE_CTX_FLAG_SIZING, NULL}
# else
#  define PB_OSTREAM_SIZING {0, 0, PB_ENCODE_CTX_FLAG_SIZING, NULL}
# endif
#endif

/* Extended version of pb_encode, with several options to control the
 * encoding process:
 *
 * PB_ENCODE_DELIMITED:      Prepend the length of message as a varint.
 *                           Corresponds to writeDelimitedTo() in Google's
 *                           protobuf API.
 *
 * PB_ENCODE_NULLTERMINATED: Append a null byte to the message for termination.
 *                           NOTE: This behaviour is not supported in most other
 *                           protobuf implementations, so PB_ENCODE_DELIMITED
 *                           is a better option for compatibility.
 */
#define PB_ENCODE_DELIMITED       PB_ENCODE_CTX_FLAG_DELIMITED
#define PB_ENCODE_NULLTERMINATED  PB_ENCODE_CTX_FLAG_NULLTERMINATED
static inline bool pb_encode_ex(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields,
    const void *src_struct, pb_encode_ctx_flags_t flags)
{
    pb_encode_ctx_flags_t old_flags = ctx->flags;
    ctx->flags |= flags;
    bool status = pb_encode_s(ctx, fields, src_struct, 0);
    ctx->flags = old_flags;
    return status;
}

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define pb_encode_delimited(s,f,d) pb_encode_ex(s,f,d, PB_ENCODE_DELIMITED)
#define pb_encode_nullterminated(s,f,d) pb_encode_ex(s,f,d, PB_ENCODE_NULLTERMINATED)

#endif /* PB_API_VERSION */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
