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

#if !PB_NO_STREAM_CALLBACK
/* Callback pointer for custom stream types. This can read the
 * bytes directly from your storage, which can be for example
 * a file or a network socket.
 * 
 * The callback must conform to these rules:
 *
 * 1. The callback should read up to 'count' bytes into 'buf'.
 *    Value of 'count' is always smaller than PB_SIZE_MAX and
 *    at most ctx->bytes_left.
 *
 * 2. On EOF, return the actual number of bytes successfully read.
 *    Any value less than 'count' is regarded as EOF condition.
 *
 * 3. On errors, return PB_READ_ERROR. This will cause decoding to abort.
 *
 * 4. You can use stream_callback_state to store your own data.
 *    Alternatively you can wrap pb_decode_ctx_t to extend it with
 *    your own fields.
 *
 * 5. During submessage decoding, bytes_left is set to smaller value
 *    than the main stream. Don't use bytes_left to compute any pointers.
 *    The ctx pointer remains the same even for substreams.
 */
typedef pb_size_t (*pb_decode_ctx_read_callback_t)(pb_decode_ctx_t *ctx, pb_byte_t *buf, pb_size_t count);
#endif

#if !PB_NO_CONTEXT_FIELD_CALLBACK
/* Field callback function for decoding.
 *
 * This lets you implement custom processing for incoming data, such as formatting it into
 * custom data structures or storing it into a filesystem. The callback implementation should:
 *
 * 1. Check field->descriptor and field->tag to see which field is being processed, e.g.:
 *      if (field->descriptor == &MyMessage_msg && field->tag == MyMessage_myfield_tag)
 *
 * 2. Process data from the stream. The ctx->bytes_left is automatically set to the field
 *    payload length. The field tag number and length prefix (if any) has already been read.
 *    For example to read bytes fields payload, call pb_read(ctx, mybuffer, ctx->bytes_left).
 *
 * 3. Return true if you processed the field successfully, or false on e.g. IO errors.
 *    You can use PB_RETURN_ERROR(ctx, "...") to set diagnostics error message.
 *
 * 4. If you don't recognize the field, just return true.
 *    If name-bound or struct-bound callbacks are defined, control is passed to them.
 *    If no callbacks handle the field, the data is discarded.
 */
typedef bool (*pb_decode_ctx_field_callback_t)(pb_decode_ctx_t *ctx, const pb_field_t *field);
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

// PB_ENCODE_CTX_FLAG_NO_VALIDATE_UTF8: Disable UTF8 validation of strings.
// Can also be applied globally with PB_NO_VALIDATE_UTF8 define.
#define PB_DECODE_CTX_FLAG_NO_VALIDATE_UTF8  (pb_decode_ctx_flags_t)(1 << 4)

/* pb_decode_ctx_t contains the state associated with message decoding.
 *
 * Structure should be initialized using either
 *   pb_init_decode_ctx_for_buffer(...);
 * or
 *   pb_init_decode_ctx_for_callback(...);
 *
 * After that, user code can optionally set the following:
 *   - flags                   to enable e.g. DELIMITED decoding
 *   - allocator               for fields with FT_POINTER type
 *   - field_callback          for fields with FT_CALLBACK type
 *   - field_callback_state    for information to field_callback
 */
struct pb_decode_ctx_s
{
    // Flags that affect decoding behavior, combination of PB_DECODE_CTX_FLAG_*
    pb_decode_ctx_flags_t flags;

    // Maximum number of bytes left in this stream.
    // Normally this should equal the total message length, which has
    // to be conveyed by other means. If DELIMITED flag is provided,
    // the message length is taken from prefix at start of the message.
    //
    // For callback streams, message size may be unknown. In that case
    // bytes_left should be a sane maximum length limit, to avoid denial
    // of service through unexpectedly long input messages. The stream
    // callback can report EOF before this limit is reached.
    pb_size_t bytes_left;

    // Memory buffer with the data to decode.
    // The pointer is advanced by pb_read() when data has been read.
    // This may be NULL for callback streams.
    const pb_byte_t *rdpos;

    // Stack-allocated pb_walk() state, internally used for memory usage
    // optimizations during callback handling. This is initialized to NULL
    // and later set by pb_decode().
    pb_walk_state_t *walk_state;

#if !PB_NO_STREAM_CALLBACK
    // Optional stream callback for reading from input directly, instead
    // of the memory buffer. The callback can use stream_callback_state
    // for its own purposes, it is not modified by nanopb.
    pb_decode_ctx_read_callback_t stream_callback;
    void *stream_callback_state;

    // Pointer to the beginning of the optional memory buffer usable
    // as cache. Bytes are stored aligned to the end of the buffer.
    pb_byte_t *buffer;

    // Total size of the cache buffer
    pb_size_t buffer_size;
#endif

#if !PB_NO_ERRMSG
    // Pointer to constant (ROM) string when decoding function returns error
    const char *errmsg;
#endif

#if !PB_NO_CONTEXT_ALLOCATOR
    // User-provided memory allocator to use for any FT_POINTER fields.
    // If NULL, default allocator is used.
    pb_allocator_t *allocator;
#endif

#if !PB_NO_CONTEXT_FIELD_CALLBACK
    // User-provided field callback function, applies to all callback-type fields.
    // The callback can use field_callback_state for its own purposes.
    pb_decode_ctx_field_callback_t field_callback;
    void *field_callback_state;
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
 * If ctx is NULL or ctx->allocator is NULL, default allocator is used.
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
void pb_init_decode_ctx_for_buffer(pb_decode_ctx_t *ctx, const pb_byte_t *buf, pb_size_t msglen);

#if !PB_NO_STREAM_CALLBACK
/* Create encode context for a stream with a callback function.
 * State is a free pointer for use by the callback.
 *
 * max_msglen can be the actual length of the message, or an upper limit.
 * The callback function can indicate EOF before max_msglen is exhausted.
 *
 * Note: if max_msglen is given as PB_SIZE_MAX, this function clamps
 * it to PB_SIZE_MAX-1, as the highest value is used for error indicator
 * in the stream callback.
 */
void pb_init_decode_ctx_for_callback(pb_decode_ctx_t *ctx,
    pb_decode_ctx_read_callback_t stream_callback, void *stream_callback_state,
    pb_size_t max_msglen, pb_byte_t *buf, pb_size_t bufsize);
#endif

/* Function to read from the stream associated with decode context.
 * You can use this if you need to read some custom header data, or to
 * read data in field callbacks. It returns true when 'count' bytes were
 * successfully read, and false on errors or EOF.
 */
bool pb_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, pb_size_t count);

#if !PB_NO_MALLOC
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
bool pb_allocate_field(pb_decode_ctx_t *ctx, void **ptr, pb_size_t data_size, pb_size_t array_size);

/* Release storage previously allocated by pb_allocate_field().
 * Uses either the allocator defined by ctx or the default allocator.
 */
void pb_release_field(pb_decode_ctx_t *ctx, void *ptr);

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
#if !PB_WITHOUT_64BIT
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
#if !PB_WITHOUT_64BIT
bool pb_decode_svarint(pb_decode_ctx_t *ctx, int64_t *dest);
#else
bool pb_decode_svarint(pb_decode_ctx_t *ctx, int32_t *dest);
#endif

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
bool pb_decode_fixed32(pb_decode_ctx_t *ctx, void *dest);

#if !PB_WITHOUT_64BIT
/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
bool pb_decode_fixed64(pb_decode_ctx_t *ctx, void *dest);
#endif

#if PB_CONVERT_DOUBLE_FLOAT
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
bool pb_decode_open_substream(pb_decode_ctx_t *ctx, pb_size_t *old_length);
bool pb_decode_close_substream(pb_decode_ctx_t *ctx, pb_size_t old_length);

/* API compatibility defines for code written before nanopb-1.0.0 */
#if PB_API_VERSION < PB_API_VERSION_v1_0

static inline pb_istream_t pb_istream_from_buffer(const pb_byte_t *buf, size_t msglen)
{
    pb_istream_t ctx;
    PB_OPT_ASSERT(msglen <= PB_SIZE_MAX);
    pb_init_decode_ctx_for_buffer(&ctx, buf, (pb_size_t)msglen);
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
    pb_size_t old_length = stream->bytes_left;
    *stream = *substream;
    return pb_decode_close_substream(stream, old_length);
}

// PB_ISTREAM_EMPTY has been replaced by pb_init_decode_ctx_buffer() function
// called with zero length.
#define PB_ISTREAM_EMPTY {0, 0, NULL, NULL}

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

#endif /* PB_API_VERSION */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
