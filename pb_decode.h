/* pb_decode.h: Functions to decode protocol buffers. Depends on pb_decode.c.
 * The main function is pb_decode. You also need an input stream, and the
 * field descriptions created by nanopb_generator.py.
 */

#ifndef PB_DECODE_H_INCLUDED
#define PB_DECODE_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PB_NO_STREAM_CALLBACK
/* Callback pointer for custom stream types. This can read the
 * bytes directly from your storage, which can be for example
 * a file or a network socket.
 * 
 * The callback must conform to these rules:
 *
 * 1) Return false on IO errors. This will cause decoding to abort.
 * 2) You can use state to store your own data (e.g. buffer pointer),
 *    and rely on pb_read to verify that no-body reads past bytes_left.
 * 3) Your callback may be used with substreams, in which case bytes_left
 *    is different than from the main stream. Don't use bytes_left to compute
 *    any pointers. The ctx pointer remains the same even for substreams.
 */
typedef bool (*pb_decode_ctx_read_callback_t)(pb_decode_ctx_t *ctx, pb_byte_t *buf, size_t count);
#endif

/* Flags for decode context state */
typedef uint16_t pb_decode_ctx_flags_t;

// PB_DECODE_CTX_FLAG_NOINIT: Do not initialize fields before decoding.
// This is slightly faster if you do not need the default values and instead
// initialize the structure to 0 using e.g. memset(). This can also be used
// for merging two messages, i.e. combine already existing data with new
// values.
#define PB_DECODE_CTX_FLAG_NOINIT            (pb_decode_ctx_flags_t)(1 << 0)

// PB_DECODE_CTX_FLAG_DELIMITED: Read varint length prefix before message.
// Corresponds to parseDelimitedFrom() in Google's protobuf API.
#define PB_DECODE_CTX_FLAG_DELIMITED         (pb_decode_ctx_flags_t)(1 << 1)

// PB_DECODE_CTX_FLAG_NULLTERMINATED: Terminate decoding on zero tag number.
// NOTE: This behavior is not supported in most other protobuf implementations.
#define PB_DECODE_CTX_FLAG_NULLTERMINATED    (pb_decode_ctx_flags_t)(1 << 2)

/* Structure containing the state associated with message decoding.
 * For the common case of message coming from a memory buffer, this
 * is initialized with pb_init_decode_ctx_for_buffer().
 */
struct pb_decode_ctx_s
{
#ifndef PB_NO_STREAM_CALLBACK
    // Optional callback for reading from input directly, instead
    // of the memory buffer. State is a free field for use by the callback.
    // It's also allowed to extend the pb_encode_ctx_t struct with your own
    // fields by wrapping it.
    pb_decode_ctx_read_callback_t callback;
    void *state;
#endif

    // Maximum number of bytes left in this stream. Callback can report
    // EOF before this limit is reached. Setting a limit is recommended
    // when decoding directly from file or network streams to avoid
    // denial-of-service by excessively long messages.
    size_t bytes_left;
    
#ifndef PB_NO_ERRMSG
    // Pointer to constant (ROM) string when decoding function returns error
    const char *errmsg;
#endif

    // Flags that affect decoding behavior, combination of PB_DECODE_CTX_FLAG_*
    pb_decode_ctx_flags_t flags;

    // Memory buffer with the data to decode.
    // The pointer is advanced when data has been read
    const pb_byte_t *rdpos;

#ifndef PB_NO_STREAM_CALLBACK
    // Pointer to the beginning of the memory buffer usable as cache
    // Bytes are stored aligned to the end of the buffer.
    pb_byte_t *buffer;

    // Total size of the cache buffer
    size_t buffer_size;
#endif

    // Outer pb_walk() stackframe, used for memory usage optimizations during
    // callback handling. This is initialized to NULL and later set by pb_encode().
    pb_walk_state_t *walk_state;

#if defined(PB_ENABLE_MALLOC) && !defined(PB_NO_CONTEXT_ALLOCATOR)
    pb_allocator_t *allocator;
#endif
};

/***************************
 * Main decoding functions *
 ***************************/
 
/* Prefer using pb_decode() macro instead of these functions.
 * The macro passes struct_size automatically, giving some amount
 * of type safety of pb_msgdesc_t pointer vs. wrong struct type.
 *
 * If you don't know the structure size, you can pass 0 as the struct_size
 * and the check will be skipped. The actual object at dest_struct must
 * still match the field descriptor.
 */
bool pb_decode_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields,
                 void *dest_struct, size_t struct_size);
bool pb_release_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields,
                  void *dest_struct, size_t struct_size);

/* Decode a single protocol buffers message from input stream into a C structure.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by dest must match the description in fields.
 * Callback fields of the destination structure must be initialized by caller.
 * All other fields will be initialized by this function.
 *
 * Example usage:
 *    MyMessage msg = {};
 *    uint8_t buffer[64];
 *    pb_decode_ctx_t ctx;
 *    
 *    // ... read some data into buffer ...
 *
 *    pb_init_decode_ctx_for_buffer(&ctx, buffer, count);
 *    pb_decode(&ctx, MyMessage_fields, &msg);
 */
#define pb_decode(ctx, fields, dest_struct) pb_decode_s(ctx, fields, dest_struct, sizeof(*dest_struct))

/* Release any allocated pointer fields. If you use dynamic allocation, you should
 * call this for any successfully decoded message when you are done with it. If
 * pb_decode() returns with an error, the message is automatically released.
 *
 * If ctx is not NULL, releasing uses the allocator defined in the context.
 */
#define pb_release(ctx, fields, dest_struct) pb_release_s(ctx, fields, dest_struct, sizeof(*dest_struct))

/**************************************
 * Functions for manipulating streams *
 **************************************/

/* Create a decode context for reading from a memory buffer.
 *
 * msglen should be the actual length of the message, not the full size of
 * allocated buffer.
 *
 * Alternatively, you can use a custom stream that reads directly from e.g.
 * a file or a network socket.
 */
void pb_init_decode_ctx_for_buffer(pb_decode_ctx_t *ctx, const pb_byte_t *buf, size_t msglen);

#ifndef PB_NO_STREAM_CALLBACK
/* Create encode context for a stream with a callback function.
 * State is a free pointer for use by the callback.
 * A memory buffer can optionally be provided for caching.
 */
void pb_init_decode_ctx_for_callback(pb_decode_ctx_t *ctx,
    pb_decode_ctx_read_callback_t callback, void *state,
    size_t msglen, pb_byte_t *buf, size_t bufsize);
#endif

/* Function to read from the stream associated with decode context. You can use this if you need to
 * read some custom header data, or to read data in field callbacks.
 */
bool pb_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, size_t count);

#ifdef PB_ENABLE_MALLOC
/******************************************
 * Helper functions for memory allocation *
 ******************************************/

/* Allocate storage for 'array_size' entries each of 'data_size' bytes.
 * Uses either the allocator defined by ctx or the default allocator.
 * Pointer to allocate memory is stored in '*ptr'.
 *
 * If old value of '*ptr' is not NULL, realloc is done to expand the allocation.
 * During realloc, the value of '*ptr' may change (old data is copied to new storage).
 *
 * On failure, false is returned and '*ptr' remains unchanged.
 */
bool pb_allocate_field(pb_decode_ctx_t *ctx, void **ptr, size_t data_size, size_t array_size);

/* Release storage previously allocated by pb_allocate_field().
 * Uses either the allocator defined by ctx or the default allocator.
 *
 * Pointer to the memory is taken from '*ptr', and NULL is written to it.
 */
void pb_release_field(pb_decode_ctx_t *ctx, void **ptr);

#endif

/************************************************
 * Helper functions for writing field callbacks *
 ************************************************/

/* Decode the tag for the next field in the stream. Gives the wire type and
 * field tag. At end of the message, returns false and sets eof to true. */
bool pb_decode_tag(pb_decode_ctx_t *ctx, pb_wire_type_t *wire_type, pb_tag_t *tag, bool *eof);

/* Skip the field payload data, given the wire type. */
bool pb_skip_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type);

/* Decode an integer in the varint format. This works for enum, int32,
 * int64, uint32 and uint64 field types. */
#ifndef PB_WITHOUT_64BIT
bool pb_decode_varint(pb_decode_ctx_t *ctx, uint64_t *dest);
#else
#define pb_decode_varint pb_decode_varint32
#endif

/* Decode an integer in the varint format. This works for enum, int32,
 * and uint32 field types. */
bool pb_decode_varint32(pb_decode_ctx_t *ctx, uint32_t *dest);

/* Decode a bool value in varint format. */
bool pb_decode_bool(pb_decode_ctx_t *ctx, bool *dest);

/* Decode an integer in the zig-zagged svarint format. This works for sint32
 * and sint64. */
#ifndef PB_WITHOUT_64BIT
bool pb_decode_svarint(pb_decode_ctx_t *ctx, int64_t *dest);
#else
bool pb_decode_svarint(pb_decode_ctx_t *ctx, int32_t *dest);
#endif

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
bool pb_decode_fixed32(pb_decode_ctx_t *ctx, void *dest);

#ifndef PB_WITHOUT_64BIT
/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
bool pb_decode_fixed64(pb_decode_ctx_t *ctx, void *dest);
#endif

#ifdef PB_CONVERT_DOUBLE_FLOAT
/* Decode a double value into float variable. */
bool pb_decode_double_as_float(pb_decode_ctx_t *ctx, float *dest);
#endif

/* Start reading a PB_WT_STRING field.
 * This function first reads a varint32 for the field length and then
 * modifies the ctx->bytes_left to the length of the field data.
 *
 * Calling code can then proceed to decode the field contents and the
 * end of the stream indicates the end of the field.
 *
 * Remember to close the substream using pb_decode_close_substream().
 */
bool pb_decode_open_substream(pb_decode_ctx_t *ctx, size_t *old_length);
bool pb_decode_close_substream(pb_decode_ctx_t *ctx, size_t old_length);

/* API compatibility defines for code written before nanopb-1.0.0 */
#if PB_API_VERSION < PB_API_VERSION_v1_0

static inline pb_istream_t pb_istream_from_buffer(const pb_byte_t *buf, size_t msglen)
{
    pb_istream_t ctx;
    pb_init_decode_ctx_for_buffer(&ctx, buf, msglen);
    return ctx;
}

static inline bool pb_make_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    *substream = *stream;
    if (!pb_decode_open_substream(substream, &stream->bytes_left))
    {
        stream->bytes_left = substream->bytes_left;
        PB_RETURN_ERROR(stream, PB_GET_ERROR(substream));
    }
    return true;
}

static inline bool pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    size_t old_length = stream->bytes_left;
    *stream = *substream;
    return pb_decode_close_substream(stream, old_length);
}

// PB_ISTREAM_EMPTY has been replaced by pb_init_decode_ctx_buffer() function
// called with zero length.
#define PB_ISTREAM_EMPTY {0}

/* Extended version of pb_decode, with several options to control
 * the decoding process:
 *
 * PB_DECODE_NOINIT:         Do not initialize the fields to default values.
 *                           This is slightly faster if you do not need the default
 *                           values and instead initialize the structure to 0 using
 *                           e.g. memset(). This can also be used for merging two
 *                           messages, i.e. combine already existing data with new
 *                           values.
 *
 * PB_DECODE_DELIMITED:      Input message starts with the message size as varint.
 *                           Corresponds to parseDelimitedFrom() in Google's
 *                           protobuf API.
 *
 * PB_DECODE_NULLTERMINATED: Stop reading when field tag is read as 0. This allows
 *                           reading null terminated messages.
 *                           NOTE: Until nanopb-0.4.0, pb_decode() also allows
 *                           null-termination. This behaviour is not supported in
 *                           most other protobuf implementations, so PB_DECODE_DELIMITED
 *                           is a better option for compatibility.
 *
 * Multiple flags can be combined with bitwise or (| operator)
 */
static inline bool pb_decode_ex(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields,
                                void *dest_struct, pb_decode_ctx_flags_t flags)
{
    pb_decode_ctx_flags_t old_flags = ctx->flags;
    ctx->flags |= flags;
    bool status = pb_decode_s(ctx, fields, dest_struct, 0);
    ctx->flags = old_flags;
    return status;
}
#define PB_DECODE_NOINIT          PB_DECODE_CTX_FLAG_NOINIT
#define PB_DECODE_DELIMITED       PB_DECODE_CTX_FLAG_DELIMITED
#define PB_DECODE_NULLTERMINATED  PB_DECODE_CTX_FLAG_NULLTERMINATED

#undef pb_release
/* Compatibility macro for old prototype of pb_release() that didn't take context argument.
 * This can be called as either
 *        pb_release(fields, dest_struct);       (old API, uses default allocator)
 * or
 *        pb_release(ctx, fields, dest_struct);  (new API, ctx can be NULL)
 */
#define pb_release(...) PB_EXPAND(pb_release_varmacro(__VA_ARGS__, pb_release3, pb_release2)(__VA_ARGS__))
#define pb_release_varmacro(PB_ARG1, PB_ARG2, PB_ARG3, PB_MACRONAME, ...) PB_MACRONAME
#define pb_release2(fields, dest_struct) pb_release_s(NULL, fields, dest_struct, sizeof(*dest_struct))
#define pb_release3(ctx, fields, dest_struct) pb_release_s(ctx, fields, dest_struct, sizeof(*dest_struct))

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define pb_decode_noinit(s,f,d) pb_decode_ex(s,f,d, PB_DECODE_NOINIT)
#define pb_decode_delimited(s,f,d) pb_decode_ex(s,f,d, PB_DECODE_DELIMITED)
#define pb_decode_delimited_noinit(s,f,d) pb_decode_ex(s,f,d, PB_DECODE_DELIMITED | PB_DECODE_NOINIT)
#define pb_decode_nullterminated(s,f,d) pb_decode_ex(s,f,d, PB_DECODE_NULLTERMINATED)

#endif /* PB_API_VERSION */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
