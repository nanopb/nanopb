/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_decode.h"
#include "pb_common.h"

/* Use the GCC warn_unused_result attribute to check that all return values
 * are propagated correctly. On other compilers, gcc before 3.4.0 and iar
 * before 9.40.1 just ignore the annotation.
 */
#if (defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) || \
    (defined(__IAR_SYSTEMS_ICC__) && (__VER__ >= 9040001))
    #define checkreturn __attribute__((warn_unused_result))
#else
    #define checkreturn
#endif

/**************************************
 * Declarations internal to this file *
 **************************************/

static bool checkreturn decode_basic_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_static_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_pointer_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn pb_dec_varint(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_string(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_fixed_length_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_skip_varint(pb_decode_ctx_t *ctx);
static bool checkreturn pb_skip_string(pb_decode_ctx_t *ctx);

#if !PB_NO_MALLOC
static pb_size_t get_packed_array_size(const pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static pb_size_t get_array_size(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, const pb_field_iter_t *field);

static void pb_release_single_field(pb_decode_ctx_t *ctx, pb_field_iter_t *field);
static pb_walk_retval_t pb_release_walk_cb(pb_walk_state_t *state);
#endif

#if PB_WITHOUT_64BIT
#define pb_int64_t int32_t
#define pb_uint64_t uint32_t
#else
#define pb_int64_t int64_t
#define pb_uint64_t uint64_t
#endif

#if !PB_NO_ERRMSG
#if !PB_NO_STREAM_CALLBACK
// This is used in pb_decode_tag() to workaround corner case
// in EOF detection with input stream callbacks.
static const char c_errmsg_io_error[] = "io error";
#endif
#endif

// Structure used to store information per each message level
// during the decoding operation.
typedef struct {
    // Parent stream length that gets stored when pb_decode_open_substream() is called.
    // It's also used to store wire type when oneof release is done.
    pb_size_t old_length;

    // Flags for this recursion level
    uint_least16_t flags;

    // Tracking for fixed count repeated fields.
    // This can handle at most one fixarray that is unpacked and
    // unordered among other (non-fixarray) fields.
    // During field processing fixarray_count contains the number
    // of items decoded so far. Otherwise it contains the number
    // of items remaining.
    pb_tag_t fixarray_tag;
    pb_size_t fixarray_count;

    // Array of (descriptor->required_field_count / 8) bytes is stored
    // after the end of this structure. It is equivalent to a flexible
    // array member, but because of lack of C++ standardization, manual
    // pointer arithmetic is used to access it.
} pb_decode_walk_stackframe_t;

// Total stackframe size, including PB_WALK internal stackframe
#define PB_DECODE_STACKFRAME_SIZE (PB_WALK_STACKFRAME_SIZE + PB_WALK_ALIGN(sizeof(pb_decode_walk_stackframe_t) + PB_MAX_REQUIRED_FIELDS / 8))

// Amount of stack initially allocated for decoding.
// pb_walk() will allocate more automatically unless recursion is disabled.
#ifndef PB_DECODE_INITIAL_STACKSIZE
#define PB_DECODE_INITIAL_STACKSIZE (PB_MESSAGE_NESTING * PB_DECODE_STACKFRAME_SIZE)
#endif

// pb_release() only has the default pb_walk frame
#ifndef PB_RELEASE_INITIAL_STACKSIZE
#define PB_RELEASE_INITIAL_STACKSIZE (PB_MESSAGE_NESTING * PB_WALK_STACKFRAME_SIZE)
#endif

// Make sure each recursion level can fit at least one frame
PB_STATIC_ASSERT(PB_WALK_STACK_SIZE > PB_DECODE_STACKFRAME_SIZE, PB_WALK_STACK_SIZE_is_too_small)

// Temporary storage for restoring stream state after a substream has
// been used for callback invocation.
typedef struct {
    pb_size_t bytes_left;

#if !PB_NO_STREAM_CALLBACK
    // We may need a temporary buffer when varint field comes from a stream source
    const pb_byte_t *rdpos;
    pb_byte_t *buffer;
    pb_size_t buffer_size;
    pb_byte_t varintbuf[10];
#endif
} callback_substream_tmp_t;

static bool checkreturn open_callback_substream(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, callback_substream_tmp_t *tmp);
static bool close_callback_substream(pb_decode_ctx_t *ctx, const callback_substream_tmp_t *tmp);

/*******************************
 * pb_istream_t implementation *
 *******************************/

#if !PB_NO_STREAM_CALLBACK

// Return how many bytes are available at rdpos
static inline pb_size_t pb_bytes_available(const pb_decode_ctx_t *ctx)
{
    if (ctx->rdpos != NULL)
    {
        PB_OPT_ASSERT(ctx->rdpos >= ctx->buffer &&
                      ctx->rdpos <= ctx->buffer + ctx->buffer_size);

        return ctx->buffer_size - (pb_size_t)(ctx->rdpos - ctx->buffer);
    }
    else
    {
        return 0;
    }
}

// Fill up cache buffer that is used for accessing callback streams.
// This should only be called when the remaining length of message is known.
static bool checkreturn pb_fill_stream_cache(pb_decode_ctx_t *ctx)
{
    if (ctx->callback != NULL && ctx->buffer_size > 0)
    {
        // Check how many bytes we can buffer in total
        pb_size_t total = ctx->buffer_size;
        if (total > ctx->bytes_left)
        {
            total = ctx->bytes_left;
        }

        // Don't refill if already over half.
        // There is a memcpy overhead in refilling too often.
        pb_size_t bytes_in_buf = pb_bytes_available(ctx);
        if (total > 2 * bytes_in_buf)
        {
            // Buffer is always end-aligned
            pb_byte_t *buf = ctx->buffer + ctx->buffer_size - total;

            // If there is old data, move it into place
            if (bytes_in_buf > 0)
            {
                PB_OPT_ASSERT(total < bytes_in_buf);
                memmove(buf, ctx->rdpos, bytes_in_buf);
            }

            ctx->rdpos = buf;

            // Fill buffer with new data
            if (!ctx->callback(ctx, buf + bytes_in_buf, (size_t)(total - bytes_in_buf)))
            {
                PB_RETURN_ERROR(ctx, c_errmsg_io_error);
            }
        }
    }

    return true;
}
#endif

bool checkreturn pb_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, pb_size_t count)
{
    if (count == 0)
        return true;

    if (ctx->bytes_left < count)
        PB_RETURN_ERROR(ctx, "end-of-stream");

    pb_size_t bufcount = 0;
    
    if (ctx->rdpos)
    {
        bufcount = count;

#if !PB_NO_STREAM_CALLBACK
        // Check how much data is available from cache
        if (ctx->buffer_size > 0)
        {
            pb_size_t buf_remain = pb_bytes_available(ctx);
            if (bufcount > buf_remain)
            {
                bufcount = buf_remain;
            }
        }
#endif
    
        if (buf != NULL)
        {
            memcpy(buf, ctx->rdpos, bufcount);
        }

        ctx->rdpos += bufcount;
        ctx->bytes_left -= bufcount;
    }

#if !PB_NO_STREAM_CALLBACK
    if (ctx->callback != NULL && bufcount < count)
    {
        // Read rest of the data directly from callback
        pb_size_t cbcount = count - bufcount;
        ctx->rdpos = NULL;

        if (buf != NULL)
        {
            if (!ctx->callback(ctx, buf + bufcount, (size_t)cbcount))
                PB_RETURN_ERROR(ctx, c_errmsg_io_error);
        }
        else
        {
            // Discard bytes coming from the callback
            pb_byte_t tmp[16];
            pb_size_t remain = cbcount;
            while (remain > 0)
            {
                pb_size_t block = (remain > sizeof(tmp)) ? sizeof(tmp) : remain;
                if (!ctx->callback(ctx, tmp, (size_t)block))
                    PB_RETURN_ERROR(ctx, c_errmsg_io_error);

                remain -= block;
            }
        }

        if (ctx->bytes_left < cbcount)
            ctx->bytes_left = 0;
        else
            ctx->bytes_left -= cbcount;
    }
#endif

    return true;
}

/* Read a single byte from input stream. buf may not be NULL.
 * This is an optimization for the varint decoding. */
static bool checkreturn pb_readbyte(pb_decode_ctx_t *ctx, pb_byte_t *buf)
{
    if (ctx->bytes_left == 0)
        PB_RETURN_ERROR(ctx, "end-of-stream");

#if !PB_NO_STREAM_CALLBACK
    if (!ctx->rdpos ||
        (ctx->buffer_size > 0 && ctx->rdpos >= ctx->buffer + ctx->buffer_size))
    {
        // No more data in cache, read directly from callback
        ctx->rdpos = NULL;
        ctx->bytes_left--;
        if (!ctx->callback(ctx, buf, 1))
            PB_RETURN_ERROR(ctx, c_errmsg_io_error);
        return true;
    }
#endif

    *buf = *ctx->rdpos++;
    ctx->bytes_left--;
    
    return true;    
}

void pb_init_decode_ctx_for_buffer(pb_decode_ctx_t *ctx, const pb_byte_t *buf, pb_size_t msglen)
{
    ctx->flags = 0;
    ctx->bytes_left = msglen;
    ctx->rdpos = buf;

#if !PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif

#if !PB_NO_STREAM_CALLBACK
    ctx->callback = NULL;
    ctx->state = NULL;
    ctx->buffer = NULL;
    ctx->buffer_size = 0;
#endif

    ctx->walk_state = NULL;

#if !PB_NO_CONTEXT_ALLOCATOR
    ctx->allocator = NULL;
#endif
}

#if !PB_NO_STREAM_CALLBACK
void pb_init_decode_ctx_for_callback(pb_decode_ctx_t *ctx,
    pb_decode_ctx_read_callback_t callback, void *state,
    pb_size_t msglen, pb_byte_t *buf, pb_size_t bufsize)
{
    ctx->flags = 0;
    ctx->bytes_left = msglen;
    ctx->rdpos = NULL;

    ctx->callback = callback;
    ctx->state = state;

    if (bufsize >= 10)
    {
        // We need at least one 64-bit varint worth of buffer
        // for it to be useful.
        ctx->buffer = buf;
        ctx->buffer_size = bufsize;
    }
    else
    {
        ctx->buffer = NULL;
        ctx->buffer_size = 0;
    }

#if !PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif

    ctx->walk_state = NULL;

#if !PB_NO_CONTEXT_ALLOCATOR
    ctx->allocator = NULL;
#endif
}
#endif

/********************
 * Helper functions *
 ********************/

bool checkreturn pb_decode_varint32(pb_decode_ctx_t *ctx, uint32_t *dest)
{
    pb_byte_t byte;
    uint32_t result;
    
    if (!pb_readbyte(ctx, &byte))
    {
        return false;
    }
    
    if ((byte & 0x80) == 0)
    {
        /* Quick case, 1 byte value */
        result = byte;
    }
    else
    {
        /* Multibyte case */
        uint_fast8_t bitpos = 7;
        result = byte & 0x7F;
        
        do
        {
            if (!pb_readbyte(ctx, &byte))
                return false;
            
            if (bitpos >= 32)
            {
                /* Note: The varint could have trailing 0x80 bytes, or 0xFF for negative. */
                pb_byte_t sign_extension = (bitpos < 63) ? 0xFF : 0x01;
                bool valid_extension = ((byte & 0x7F) == 0x00 ||
                         ((result >> 31) != 0 && byte == sign_extension));

                if (bitpos >= 64 || !valid_extension)
                {
                    PB_RETURN_ERROR(ctx, "varint overflow");
                }
            }
            else if (bitpos == 28)
            {
                if ((byte & 0x70) != 0 && (byte & 0x78) != 0x78)
                {
                    PB_RETURN_ERROR(ctx, "varint overflow");
                }
                result |= (uint32_t)(byte & 0x0F) << bitpos;
            }
            else
            {
                result |= (uint32_t)(byte & 0x7F) << bitpos;
            }
            bitpos = (uint_fast8_t)(bitpos + 7);
        } while ((byte & 0x80) != 0);
   }
   
   *dest = result;
   return true;
}

#if !PB_WITHOUT_64BIT
bool checkreturn pb_decode_varint(pb_decode_ctx_t *ctx, uint64_t *dest)
{
    pb_byte_t byte;
    uint_fast8_t bitpos = 0;
    uint64_t result = 0;
    
    do
    {
        if (!pb_readbyte(ctx, &byte))
            return false;

        if (bitpos >= 63 && (byte & 0xFE) != 0)
            PB_RETURN_ERROR(ctx, "varint overflow");

        result |= (uint64_t)(byte & 0x7F) << bitpos;
        bitpos = (uint_fast8_t)(bitpos + 7);
    } while ((byte & 0x80) != 0);
    
    *dest = result;
    return true;
}
#endif

bool checkreturn pb_skip_varint(pb_decode_ctx_t *ctx)
{
    pb_byte_t byte;
    do
    {
        if (!pb_read(ctx, &byte, 1))
            return false;
    } while ((byte & 0x80) != 0);
    return true;
}

bool checkreturn pb_skip_string(pb_decode_ctx_t *ctx)
{
    uint32_t size_u32;
    if (!pb_decode_varint32(ctx, &size_u32))
        return false;
    
    if ((pb_size_t)size_u32 != size_u32)
    {
        PB_RETURN_ERROR(ctx, "size too large");
    }

    return pb_read(ctx, NULL, (pb_size_t)size_u32);
}

bool checkreturn pb_decode_tag(pb_decode_ctx_t *ctx, pb_wire_type_t *wire_type, pb_tag_t *tag, bool *eof)
{
    uint32_t temp;
    *eof = false;
    *wire_type = (pb_wire_type_t) 0;
    *tag = 0;

    if (ctx->bytes_left == 0)
    {
        *eof = true;
        return false;
    }

    if (!pb_decode_varint32(ctx, &temp))
    {
#if !PB_NO_STREAM_CALLBACK
        // Callbacks might detect eof only after first unsuccessful read
        if (ctx->callback != NULL && ctx->bytes_left == 0)
        {
#if !PB_NO_ERRMSG
            // Workaround issue #1017 where the "io error" message set by pb_readbyte()
            // will cause other error messages to be overridden later.
            if (ctx->errmsg == c_errmsg_io_error)
                ctx->errmsg = NULL;
#endif
            *eof = true;
        }
#endif
        return false;
    }
    
#if PB_NO_LARGEMSG
    // Range of tags in messages is limited to 4096,
    // make sure large tag numbers are not confused
    // after truncation to pb_tag_t.
    pb_tag_t tag_val = (pb_tag_t)(temp >> 3);
    if (tag_val != (temp >> 3))
        tag_val = 65535;
    *tag = tag_val;
#else
    *tag = temp >> 3;
#endif

    *wire_type = (pb_wire_type_t)(temp & 7);
    return true;
}

bool checkreturn pb_skip_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type)
{
    switch (wire_type)
    {
        case PB_WT_VARINT: return pb_skip_varint(ctx);
        case PB_WT_64BIT: return pb_read(ctx, NULL, 8);
        case PB_WT_STRING: return pb_skip_string(ctx);
        case PB_WT_32BIT: return pb_read(ctx, NULL, 4);
        case PB_WT_PACKED:
            /* Calling pb_skip_field with a PB_WT_PACKED is an error.
             * Explicitly handle this case and fallthrough to default to avoid
             * compiler warnings.
             */
        default: PB_RETURN_ERROR(ctx, "invalid wire_type");
    }
}

/* Start reading a PB_WT_STRING field.
 * This modifies the ctx->bytes_left to the length of the field data.
 * Remember to close the substream using pb_decode_close_substream().
 */
bool pb_decode_open_substream(pb_decode_ctx_t *ctx, pb_size_t *old_length)
{
    uint32_t size;
    if (!pb_decode_varint32(ctx, &size))
        return false;
    
    if (ctx->bytes_left < size)
        PB_RETURN_ERROR(ctx, "parent stream too short");

    *old_length = (pb_size_t)(ctx->bytes_left - size);
    ctx->bytes_left = (pb_size_t)size;

#if !PB_NO_STREAM_CALLBACK
    // We can prefill stream cache now that we know substream length
    if (!pb_fill_stream_cache(ctx))
        return false;
#endif

    return true;
}

bool pb_decode_close_substream(pb_decode_ctx_t *ctx, pb_size_t old_length)
{
    if (ctx->bytes_left > 0)
    {
        /* Read any left-over bytes from the field data */
        if (!pb_read(ctx, NULL, ctx->bytes_left))
            return false;
    }

    ctx->bytes_left = old_length;
    return true;
}

// Prepare ctx as a substream for invoking a callback.
// tmp is a buffer for storing the state.
static bool checkreturn open_callback_substream(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, callback_substream_tmp_t *tmp)
{
    uint32_t length = 0;

#if !PB_NO_STREAM_CALLBACK
    pb_byte_t *buf = NULL;
#endif

    if (wire_type == PB_WT_STRING)
    {
        if (!pb_decode_varint32(ctx, &length))
            return false;
    }
    else if (wire_type == PB_WT_32BIT)
    {
        length = 4;
    }
    else if (wire_type == PB_WT_64BIT)
    {
        length = 8;
    }
#if !PB_NO_STREAM_CALLBACK
    else if (wire_type == PB_WT_VARINT && ctx->callback != NULL &&
             pb_bytes_available(ctx) < 10)
    {
        // Varint size is not known in advance, we need to read it into a temporary buffer.
        unsigned int i;
        for (i = 0; i < 10; i++)
        {
            if (!pb_read(ctx, &tmp->varintbuf[i], 1))
                return false;

            if ((tmp->varintbuf[i] & 0x80) == 0)
            {
                length = i + 1;
                break;
            }
        }

        if (length == 0)
        {
            PB_RETURN_ERROR(ctx, "varint overflow");
        }

        buf = tmp->varintbuf;
    }
#endif
    else if (wire_type == PB_WT_VARINT)
    {
        // Read ahead in the stream buffer to find varint length
        unsigned int i;
        for (i = 0; i < 10; i++)
        {
            if (i + 1 > ctx->bytes_left)
            {
                PB_RETURN_ERROR(ctx, "end-of-stream");
            }

            if ((ctx->rdpos[i] & 0x80) == 0)
            {
                length = i + 1;
                break;
            }
        }

        if (length == 0)
        {
            PB_RETURN_ERROR(ctx, "varint overflow");
        }
    }
    else
    {
        PB_RETURN_ERROR(ctx, "invalid wire_type");
    }

    // Note: this comparison also ensures that length fits in pb_size_t
    if (length > ctx->bytes_left)
    {
        PB_RETURN_ERROR(ctx, "parent stream too short");
    }

    // Store stream state for restoring
    tmp->bytes_left = (pb_size_t)(ctx->bytes_left - length);
    ctx->bytes_left = (pb_size_t)length;

#if !PB_NO_STREAM_CALLBACK
    tmp->rdpos = ctx->rdpos;
    tmp->buffer = ctx->buffer;
    tmp->buffer_size = ctx->buffer_size;

    if (buf)
    {
        // Read from the temp buffer
        ctx->rdpos = buf;
        ctx->buffer = buf;
        ctx->buffer_size = (pb_size_t)length;
    }
#endif

    return true;
}

// Restore state after callback invocation
static bool close_callback_substream(pb_decode_ctx_t *ctx, const callback_substream_tmp_t *tmp)
{
    if (ctx->bytes_left > 0)
    {
        /* Read any left-over bytes from the field data */
        if (!pb_read(ctx, NULL, ctx->bytes_left))
            return false;
    }

    ctx->bytes_left = tmp->bytes_left;

#if !PB_NO_STREAM_CALLBACK
    // Check if temporary buffer was used
    if (ctx->buffer != tmp->buffer)
    {
        ctx->rdpos = tmp->rdpos;
        ctx->buffer = tmp->buffer;
        ctx->buffer_size = tmp->buffer_size;
    }
#endif

    return true;
}

#if !PB_NO_MALLOC
// Estimate the array allocation size of packed array.
static pb_size_t get_packed_array_size(const pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    pb_size_t result = 0;

    if (PB_LTYPE(field->type) == PB_LTYPE_BOOL)
    {
        result = ctx->bytes_left;
    }
    else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32)
    {
        result = ctx->bytes_left / 4;
    }
    else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
    {
        result = ctx->bytes_left / 8;
    }
    else
    {
#if !PB_NO_STREAM_CALLBACK
        if (ctx->callback != NULL && pb_bytes_available(ctx) < ctx->bytes_left)
        {
            // Stream contents not available, make a guess.
            result = (pb_size_t)((ctx->bytes_left - 1) / field->data_size + 1);
        }
        else
#endif
        {
            // Varint, count bytes that have top bit unset.
            pb_size_t length = ctx->bytes_left;
            const pb_byte_t *p = ctx->rdpos;
            const pb_byte_t *end = p + length;

            while (p < end)
            {
                if ((*p++ & 0x80) == 0)
                {
                    result++;
                }
            }
        }
    }

    if (result == 0 && ctx->bytes_left > 0)
    {
        result = 1;
    }

    return result;
}

// Count how many times the non-packed array field occurs
static pb_size_t get_array_size(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, const pb_field_iter_t *field)
{
    const pb_byte_t *old_rdpos = ctx->rdpos;
    pb_size_t old_length = ctx->bytes_left;
    pb_size_t count = 0;

#if !PB_NO_STREAM_CALLBACK
    if (ctx->callback)
    {
        // Don't read more than what is available in memory buffer
        pb_size_t available = pb_bytes_available(ctx);
        if (available < ctx->bytes_left)
        {
            ctx->bytes_left = available;
        }
    }
#endif

    while (ctx->bytes_left > 0 && count < PB_SIZE_MAX)
    {
        pb_tag_t tag;
        pb_wire_type_t wt;
        bool eof;

        count++;

        if (!pb_skip_field(ctx, wire_type) ||
            !pb_decode_tag(ctx, &wt, &tag, &eof))
        {
#if !PB_NO_ERRMSG
            ctx->errmsg = NULL;
#endif
            break;
        }

        if (tag != field->tag || wt != wire_type)
        {
            break;
        }
    }

    ctx->rdpos = old_rdpos;
    ctx->bytes_left = old_length;

    if (count == 0 && ctx->bytes_left > 0)
    {
        // If we don't know, assume 1 item remains.
        count = 1;
    }

    return count;
}
#endif

/*************************
 * Field initialization  *
 *************************/

// Initialize statically allocated fields, possibly descending
// into submessages.
static pb_walk_retval_t pb_init_static_field(pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;

    bool init_data = true;
    if (PB_HTYPE(iter->type) == PB_HTYPE_OPTIONAL && iter->pSize != NULL)
    {
        /* Set has_field to false. Still initialize the optional field itself also. */
        *(bool*)iter->pSize = false;
    }
    else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED && iter->pSize != &iter->array_size)
    {
        /* REPEATED: Set array count to 0, no need to initialize contents. */
        *(pb_size_t*)iter->pSize = 0;
        init_data = false;
    }
    else if (PB_HTYPE(iter->type) == PB_HTYPE_ONEOF)
    {
        /* ONEOF: Set which_field to 0. */
        *(pb_tag_t*)iter->pSize = 0;
        init_data = false;
    }

    if (init_data)
    {
        // Submessage needs recursive initialization if it has callbacks
        // that must be skipped or fields with default values.
        // Otherwise it can be memset to 0, which is faster.
        pb_msgflag_t init_flags = (PB_MSGFLAG_R_HAS_DEFVAL |
                                   PB_MSGFLAG_R_HAS_CBS |
                                   PB_MSGFLAG_R_HAS_EXTS);
        if (PB_LTYPE_IS_SUBMSG(iter->type) &&
            (iter->submsg_desc->msg_flags & init_flags) != 0)
        {
            PB_OPT_ASSERT(iter->submsg_desc->struct_size == iter->data_size);
            return PB_WALK_IN;
        }
        else
        {
            /* Initialize to zeros */
            memset(iter->pData, 0, (size_t)iter->data_size);
        }
    }

    return PB_WALK_NEXT_FIELD;
}

//  Initialize message fields to default values, recursively
static pb_walk_retval_t pb_defaults_walk_cb(pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;

    if (iter->tag == 0 || iter->message == NULL)
    {
        // End of message or empty message
        return PB_WALK_OUT;
    }

    if (state->retval == PB_WALK_OUT)
    {
        // End of submessage or extension
        return PB_WALK_NEXT_ITEM;
    }

    pb_decode_ctx_t defctx;
    pb_tag_t tag = 0;
    pb_wire_type_t wire_type = PB_WT_VARINT;
    bool eof;

    if (iter->descriptor->default_value)
    {
        // Field default values are stored as a protobuf stream
        pb_init_decode_ctx_for_buffer(&defctx, iter->descriptor->default_value, PB_SIZE_MAX);
        if (!pb_decode_tag(&defctx, &wire_type, &tag, &eof))
        {
            return PB_WALK_EXIT_ERR;
        }

        // Seek to current field
        while (tag != 0 && iter->tag > tag)
        {
            if (!pb_skip_field(&defctx, wire_type) ||
                !pb_decode_tag(&defctx, &wire_type, &tag, &eof))
            {
                return PB_WALK_EXIT_ERR;
            }
        }
    }
    else
    {
        pb_init_decode_ctx_for_buffer(&defctx, NULL, 0);
    }

    do
    {
        if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
        {
            pb_extension_t* extension = *(pb_extension_t**)iter->pData;
            if (extension)
            {
                // Descend into extension
                return pb_walk_into(state, extension->type, pb_get_extension_data_ptr(extension));
            }
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            /* Initialize array count to 0. */
            if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
            {
                *(pb_size_t*)iter->pSize = 0;
            }
            else if(PB_HTYPE(iter->type) == PB_HTYPE_ONEOF)
            {
                *(pb_tag_t*)iter->pSize = 0;
            }

            if (PB_LTYPE(iter->type) == PB_LTYPE_BYTES &&
                PB_HTYPE(iter->type) != PB_HTYPE_REPEATED)
            {
                /* Initialize a singular pointer-type bytes field to zero length */
                PB_OPT_ASSERT(iter->data_size == sizeof(pb_bytes_t));
                pb_bytes_t *bytes = (pb_bytes_t*)iter->pData;
                bytes->size = 0;
                bytes->bytes = NULL;
            }
            else
            {
                /* Initialize the pointer to NULL. */
                *(void**)iter->pField = NULL;
            }
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_CALLBACK)
        {
            /* Don't overwrite callback */
            continue;
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_STATIC)
        {
            pb_walk_retval_t retval = pb_init_static_field(state);
            if (retval == PB_WALK_IN)
                return retval;

            /* Only static fields can have default values */
            if (iter->tag == tag)
            {
                /* We have a default value for this field, read it out and read the next tag. */
                if (!decode_static_field(&defctx, wire_type, iter))
                    return PB_WALK_EXIT_ERR;

                if (!pb_decode_tag(&defctx, &wire_type, &tag, &eof))
                    return PB_WALK_EXIT_ERR;

                /* The has_field should still be set to false */
                if (PB_HTYPE(iter->type) == PB_HTYPE_OPTIONAL && iter->pSize != NULL)
                    *(bool*)iter->pSize = false;
            }
        }
    } while (pb_field_iter_next(iter));

    return PB_WALK_OUT;
}

/*************************
 * Decode a single field *
 *************************/

static bool checkreturn decode_basic_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
    switch (PB_LTYPE(field->type))
    {
        case PB_LTYPE_BOOL:
            if (wire_type != PB_WT_VARINT && wire_type != PB_WT_PACKED)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_decode_bool(ctx, (bool*)field->pData);

        case PB_LTYPE_VARINT:
        case PB_LTYPE_UVARINT:
        case PB_LTYPE_SVARINT:
            if (wire_type != PB_WT_VARINT && wire_type != PB_WT_PACKED)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_dec_varint(ctx, field);

        case PB_LTYPE_FIXED32:
            if (wire_type != PB_WT_32BIT && wire_type != PB_WT_PACKED)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_decode_fixed32(ctx, field->pData);

        case PB_LTYPE_FIXED64:
            if (wire_type != PB_WT_64BIT && wire_type != PB_WT_PACKED)
                PB_RETURN_ERROR(ctx, "wrong wire type");

#if PB_CONVERT_DOUBLE_FLOAT
            if (field->data_size == sizeof(float))
            {
                return pb_decode_double_as_float(ctx, (float*)field->pData);
            }
#endif

#if PB_WITHOUT_64BIT
            PB_RETURN_ERROR(ctx, "invalid data_size");
#else
            return pb_decode_fixed64(ctx, field->pData);
#endif

        case PB_LTYPE_BYTES:
            if (wire_type != PB_WT_STRING)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_dec_bytes(ctx, field);

        case PB_LTYPE_STRING:
            if (wire_type != PB_WT_STRING)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_dec_string(ctx, field);

        case PB_LTYPE_FIXED_LENGTH_BYTES:
            if (wire_type != PB_WT_STRING)
                PB_RETURN_ERROR(ctx, "wrong wire type");

            return pb_dec_fixed_length_bytes(ctx, field);

        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
}

static bool checkreturn decode_static_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
    switch (PB_HTYPE(field->type))
    {
        case PB_HTYPE_REQUIRED:
            return decode_basic_field(ctx, wire_type, field);
            
        case PB_HTYPE_OPTIONAL:
            if (field->pSize != NULL)
                *(bool*)field->pSize = true;
            return decode_basic_field(ctx, wire_type, field);
    
        case PB_HTYPE_REPEATED:
            if (wire_type == PB_WT_STRING
                && PB_LTYPE(field->type) <= PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array */
                bool status = true;
                pb_size_t old_length;
                pb_size_t *size = (pb_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if (!pb_decode_open_substream(ctx, &old_length))
                    return false;

                while (ctx->bytes_left > 0 && *size < field->array_size)
                {
                    if (!decode_basic_field(ctx, PB_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    (*size)++;
                    field->pData = (char*)field->pData + field->data_size;
                }

                if (ctx->bytes_left != 0)
                    PB_RETURN_ERROR(ctx, "array overflow");
                if (!pb_decode_close_substream(ctx, old_length))
                    return false;

                return status;
            }
            else
            {
                /* Repeated field */
                pb_size_t *size = (pb_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if ((*size)++ >= field->array_size)
                    PB_RETURN_ERROR(ctx, "array overflow");

                return decode_basic_field(ctx, wire_type, field);
            }

        case PB_HTYPE_ONEOF:
            *(pb_tag_t*)field->pSize = field->tag;
            return decode_basic_field(ctx, wire_type, field);

        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
}

static bool checkreturn decode_pointer_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
#if PB_NO_MALLOC
    PB_UNUSED(wire_type);
    PB_UNUSED(field);
    PB_RETURN_ERROR(ctx, "no malloc support");
#else
    switch (PB_HTYPE(field->type))
    {
        case PB_HTYPE_REQUIRED:
        case PB_HTYPE_OPTIONAL:
        case PB_HTYPE_ONEOF:
            if (PB_LTYPE(field->type) == PB_LTYPE_STRING ||
                PB_LTYPE(field->type) == PB_LTYPE_BYTES)
            {
                /* pb_dec_string and pb_dec_bytes handle allocation themselves */
                field->pData = field->pField;
                return decode_basic_field(ctx, wire_type, field);
            }
            else
            {
                if (!pb_allocate_field(ctx, (void**)field->pField, field->data_size, 1))
                    return false;
                
                field->pData = *(void**)field->pField;
                if (PB_HTYPE(field->type) == PB_HTYPE_ONEOF)
                {
                    *(pb_tag_t*)field->pSize = field->tag;
                }

                return decode_basic_field(ctx, wire_type, field);
            }
    
        case PB_HTYPE_REPEATED:
            if (wire_type == PB_WT_STRING
                && PB_LTYPE(field->type) <= PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array, multiple items come in at once. */
                bool status = true;
                pb_size_t *size = (pb_size_t*)field->pSize;
                pb_size_t old_length;
                pb_size_t allocated_size = *size;
                
                if (!pb_decode_open_substream(ctx, &old_length))
                    return false;
                
                while (ctx->bytes_left)
                {
                    if (*size == PB_SIZE_MAX)
                    {
                        PB_SET_ERROR(ctx, "too many array entries");
                        status = false;
                        break;
                    }

                    if (*size + 1 > allocated_size)
                    {
                        // Grow the array size.
                        // This tries to estimate the number of remaining entries to minimize
                        // the number of reallocs needed.
                        pb_size_t remain = get_packed_array_size(ctx, field);

                        if (remain > PB_SIZE_MAX - allocated_size)
                        {
                            PB_SET_ERROR(ctx, "too many array entries");
                            status = false;
                            break;
                        }
                        else
                        {
                            allocated_size += remain;
                        }
                        
                        if (!pb_allocate_field(ctx, (void**)field->pField, field->data_size, allocated_size))
                        {
                            status = false;
                            break;
                        }
                    }

                    /* Decode the array entry */
                    PB_OPT_ASSERT(*(char**)field->pField != NULL);
                    field->pData = *(char**)field->pField + field->data_size * (*size);

                    memset(field->pData, 0, field->data_size);
                    if (!decode_basic_field(ctx, PB_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    
                    (*size)++;
                }

                if (!pb_decode_close_substream(ctx, old_length))
                    return false;
                
                return status;
            }
            else
            {
                /* Normal repeated field, i.e. each item has its own tag.
                 * We decode as many consecutive items as there are, to
                 * reduce the number of separate allocations.
                 */
                pb_size_t *size = (pb_size_t*)field->pSize;
                pb_size_t remain = get_array_size(ctx, wire_type, field);

                if (*size > PB_SIZE_MAX - remain)
                    PB_RETURN_ERROR(ctx, "too many array entries");
                
                if (!pb_allocate_field(ctx, (void**)field->pField, field->data_size, (pb_size_t)(*size + remain)))
                    return false;
            
                field->pData = *(char**)field->pField + field->data_size * (*size);
                memset(field->pData, 0, field->data_size * remain);

                while (remain > 0)
                {
                    // Decode field data
                    (*size)++;
                    remain--;
                    if (!decode_basic_field(ctx, wire_type, field))
                    {
                        return false;
                    }

                    // Decode tag for the following field
                    if (remain > 0)
                    {
                        pb_tag_t new_tag;
                        pb_wire_type_t new_wt;
                        bool eof;
                        if (!pb_decode_tag(ctx, &new_wt, &new_tag, &eof))
                        {
                            return false;
                        }

                        if (new_tag != field->tag || new_wt != wire_type)
                        {
                            PB_RETURN_ERROR(ctx, "wrong tag");
                        }

                        field->pData = (char*)field->pData + field->data_size;
                    }
                }

                return true;
            }

        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
#endif
}


/*********************
 * Decode all fields *
 *********************/

// State flags apply to all recursion levels.
#define PB_DECODE_WALK_STATE_FLAG_SET_DEFAULTS      (uint_least16_t)1
#define PB_DECODE_WALK_STATE_FLAG_RELEASE_FIELD     (uint_least16_t)2

// Frame flags apply to single level, and are used for unsetting state flags.
#define PB_DECODE_WALK_FRAME_FLAG_END_DEFAULTS      (uint_least16_t)1
#define PB_DECODE_WALK_FRAME_FLAG_END_RELEASE       (uint_least16_t)2

static inline pb_walk_stacksize_t stacksize_for_msg(const pb_msgdesc_t *msgdesc)
{
    // Round upwards the needed number of bytes to store required_field_count bits
    pb_walk_stacksize_t stacksize = sizeof(pb_decode_walk_stackframe_t);
    stacksize += (pb_walk_stacksize_t)((msgdesc->required_field_count + CHAR_BIT - 1) / CHAR_BIT);
    return stacksize;
}

static void set_required_field_present(pb_walk_state_t *state)
{
    unsigned char *req_array = (unsigned char*)state->stack + sizeof(pb_decode_walk_stackframe_t);
    pb_fieldidx_t idx = state->iter.required_field_index;
    req_array[idx / CHAR_BIT] = (unsigned char)(req_array[idx / CHAR_BIT] | (1 << (idx % CHAR_BIT)));
}

static bool check_all_required_fields_present(pb_walk_state_t *state)
{
    unsigned char *req_array = (unsigned char*)state->stack + sizeof(pb_decode_walk_stackframe_t);
    pb_fieldidx_t count = state->iter.descriptor->required_field_count;

    while (count >= CHAR_BIT)
    {
        unsigned char inverted_bits = (unsigned char)(~(*req_array++));
        count -= 8;
        if (inverted_bits != 0)
        {
            // At least one bit is zero
            return false;
        }
    }

    if (count > 0)
    {
        unsigned char inverted_bits = (unsigned char)(~(*req_array++));
        unsigned char mask = (unsigned char)((1 << count) - 1);
        if ((inverted_bits & mask) != 0)
        {
            // There is zero bit among the lowest 'count' bits
            return false;
        }
    }

    return true;
}

#if !PB_NO_MALLOC

// Check if there is old data in the oneof union that needs to be released
// prior to replacing it with a new value.
// Return value:
//   PB_WALK_NEXT_ITEM => ok, released or no release needed
//   PB_WALK_IN => release requires recursion, field->pData has been adjusted to the old field
//   PB_WALK_EXIT_ERR => release failed
static pb_walk_retval_t release_oneof(pb_walk_state_t *state, pb_field_iter_t *field, bool submsg_released)
{
    pb_decode_ctx_t *ctx = (pb_decode_ctx_t*)state->ctx;
    pb_field_iter_t old_field = *field;
    pb_tag_t old_tag = *(pb_tag_t*)field->pSize; /* Previous which_ value */
    pb_tag_t new_tag = field->tag; /* New which_ value */

    if (old_tag == 0)
        return PB_WALK_NEXT_ITEM; /* Ok, no old data in union */

    if (old_tag == new_tag)
        return PB_WALK_NEXT_ITEM; /* Ok, old data is of same type => merge */

    /* Release old data. The find can fail if the message struct contains
     * invalid data. */
    if (!pb_field_iter_find(&old_field, old_tag, NULL))
    {
        PB_SET_ERROR(ctx, "invalid union tag");
        return PB_WALK_EXIT_ERR;
    }

    pb_type_t type = old_field.type;
    if (PB_LTYPE_IS_SUBMSG(type) && PB_ATYPE(type) != PB_ATYPE_CALLBACK && !submsg_released)
    {
        if (old_field.submsg_desc->msg_flags & PB_MSGFLAG_R_HAS_PTRS)
        {
            // Recurse into the *old* submessage.
            // After it's done, pb_walk() restores iterator to point to the new field type
            return pb_walk_into(state, old_field.submsg_desc, old_field.pData);
        }
    }

    pb_release_single_field(ctx, &old_field);

    *(pb_tag_t*)field->pSize = 0;

    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        field->pData = NULL;
    }

    return PB_WALK_NEXT_ITEM;
}
#endif

static pb_walk_retval_t decode_submsg(pb_decode_ctx_t *ctx, pb_walk_state_t *state)
{
    pb_field_iter_t *field = &state->iter;
    pb_decode_walk_stackframe_t *frame = (pb_decode_walk_stackframe_t*)state->stack;

    if (field->submsg_desc == NULL)
    {
        PB_SET_ERROR(ctx, "invalid field descriptor");
        return PB_WALK_EXIT_ERR;
    }

    if (!pb_decode_open_substream(ctx, &frame->old_length))
        return PB_WALK_EXIT_ERR;

    bool need_init = false;

    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
#if !PB_NO_MALLOC
        // Allocate memory for a pointer field
        if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
        {
            pb_size_t *size = (pb_size_t*)field->pSize;

            if (*size > PB_SIZE_MAX - 1)
            {
                PB_SET_ERROR(ctx, "too many array entries");
                return PB_WALK_EXIT_ERR;
            }

            if (!pb_allocate_field(ctx, (void**)field->pField, field->data_size, (pb_size_t)(*size + 1)))
                return PB_WALK_EXIT_ERR;

            field->pData = *(char**)field->pField + field->data_size * (*size);
            *size += 1;
            need_init = true;
        }
        else if (!field->pData)
        {
            if (!pb_allocate_field(ctx, (void**)field->pField, field->data_size, 1))
                return PB_WALK_EXIT_ERR;

            field->pData = *(void**)field->pField;
            need_init = true;
        }
#else
        PB_SET_ERROR(ctx, "no malloc support");
        return PB_WALK_EXIT_ERR;
#endif
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
    {
        // Increment count of a repeated field
        pb_size_t *size = (pb_size_t*)field->pSize;
        if (*size >= field->array_size)
        {
            PB_SET_ERROR(ctx, "array overflow");
            return PB_WALK_EXIT_ERR;
        }

        field->pData = (char*)field->pField + field->data_size * (*size);
        *size += 1;
        need_init = true;
    }

    if (PB_HTYPE(field->type) == PB_HTYPE_OPTIONAL && field->pSize != NULL)
    {
        *(bool*)field->pSize = true;
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_ONEOF)
    {
        pb_tag_t *which_field = (pb_tag_t*)field->pSize;
        if (*which_field != field->tag)
        {
            need_init = true;
            *which_field = field->tag;
        }
    }

    if (need_init)
    {
        // First clear the submessage to zeros
        memset(field->pData, 0, field->data_size);

        // Check if it has default values to apply
        // The default values will get applied recursively, and normal
        // decoding resumes when WALK_OUT is returned to this frame.
        if (field->submsg_desc->msg_flags & PB_MSGFLAG_R_HAS_DEFVAL)
        {
            state->flags |= PB_DECODE_WALK_STATE_FLAG_SET_DEFAULTS;
            frame->flags |= PB_DECODE_WALK_FRAME_FLAG_END_DEFAULTS;
        }
    }

    /* Submessages can have a separate message-level callback that is called
     * before decoding the message. Typically it is used to set callback fields
     * inside oneofs. */
    if (PB_LTYPE(field->type) == PB_LTYPE_SUBMSG_W_CB && field->pSize != NULL)
    {
        /* Message callback is stored right before pSize. */
        pb_callback_t *callback = (pb_callback_t*)field->pSize - 1;
        bool status = true;
        if (callback->funcs.decode)
        {
            status = callback->funcs.decode(ctx, field, &callback->arg);
        }

        if (!status || ctx->bytes_left == 0)
        {
            status &= pb_decode_close_substream(ctx, frame->old_length);

            if (status)
                return PB_WALK_NEXT_ITEM;
            else
                return PB_WALK_EXIT_ERR;
        }
    }

    /* Descend to iterate through submessage fields */
    PB_OPT_ASSERT(field->submsg_desc->struct_size == field->data_size);
    state->next_stacksize = stacksize_for_msg(field->submsg_desc);
    return PB_WALK_IN;
}

static pb_walk_retval_t pb_decode_walk_cb(pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;
    pb_decode_ctx_t *ctx = (pb_decode_ctx_t*)state->ctx;
    pb_decode_walk_stackframe_t *frame = (pb_decode_walk_stackframe_t*)state->stack;

    // Tag and wire type of next field from the input stream
    pb_tag_t tag = 0;
    pb_wire_type_t wire_type = PB_WT_VARINT;
    bool eof = false;
    bool skip_decode_tag = false;

    if (state->flags & PB_DECODE_WALK_STATE_FLAG_SET_DEFAULTS)
    {
        if (frame->flags & PB_DECODE_WALK_FRAME_FLAG_END_DEFAULTS)
        {
            // Setting defaults finished, now proceed to decoding
            frame->flags = 0;
            state->flags = 0;
            return PB_WALK_IN;
        }
        else
        {
            // We are setting default values for a new submessage
            return pb_defaults_walk_cb(state);
        }
    }
#if !PB_NO_MALLOC
    else if (state->flags & PB_DECODE_WALK_STATE_FLAG_RELEASE_FIELD)
    {
        if (frame->flags & PB_DECODE_WALK_FRAME_FLAG_END_RELEASE)
        {
            // Old oneof field has been released, proceed to decoding
            // the new one. This will happen through the loop below.
            // Tag of the new field has already been read.
            frame->flags = 0;
            state->flags = 0;
            tag = iter->tag;
            wire_type = (pb_wire_type_t)frame->old_length;
            skip_decode_tag = true;
            frame->old_length = 0;

            // Still need to release the top-level pointer
            pb_walk_retval_t r = release_oneof(state, iter, true);
            PB_OPT_ASSERT(r == PB_WALK_NEXT_ITEM);
        }
        else
        {
            return pb_release_walk_cb(state);
        }
    }
#endif
    else if (state->retval == PB_WALK_OUT)
    {
        // Close the substream opened for the submessage
        if (!pb_decode_close_substream(ctx, frame->old_length))
            return PB_WALK_EXIT_ERR;
    }

    PB_OPT_ASSERT(state->stacksize >= stacksize_for_msg(iter->descriptor));

    // Each loop iteration reads one field tag from the input stream
    while (skip_decode_tag || pb_decode_tag(ctx, &wire_type, &tag, &eof))
    {
        skip_decode_tag = false;

        // Check for the optional zero tag termination
        if (tag == 0)
        {
            if (ctx->flags & PB_DECODE_CTX_FLAG_NULLTERMINATED)
            {
                eof = true;
                break;
            }
            else
            {
                PB_SET_ERROR(ctx, "zero tag");
                return PB_WALK_EXIT_ERR;
            }
        }

        // Find the descriptor for this field
        // Usually fields are encoded in numeric order so the search only
        // needs to advance by one field.
        pb_extension_t *extension = NULL;
        if (!pb_field_iter_find(iter, tag, &extension))
        {
            // Discard the input data for this field
            if (!pb_skip_field(ctx, wire_type))
            {
                return PB_WALK_EXIT_ERR;
            }

            continue;
        }

        if (extension)
        {
            // Load iterator information for the extension field contents.
            // Iterator will resume in the parent message after the extension
            // is done.
            if (!pb_field_iter_load_extension(iter, extension))
            {
                PB_SET_ERROR(ctx, "invalid extension");
                return PB_WALK_EXIT_ERR;
            }

            extension->found = true;
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_REQUIRED)
        {
            set_required_field_present(state);
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED &&
                 (iter->pSize == &iter->array_size || iter->pSize == &frame->fixarray_count))
        {
            // This is a fixed count array
            // There is no count field in the message structure, so the count is tracked
            // as part of our stackframe.
            if (iter->tag != frame->fixarray_tag)
            {
                if (frame->fixarray_count > 0)
                {
                    PB_SET_ERROR(ctx, "wrong size for fixed count field");
                    return PB_WALK_EXIT_ERR;
                }

                frame->fixarray_tag = iter->tag;
                frame->fixarray_count = 0;
            }
            else
            {
                // fixarray_count is switched between "items decoded" and "items remaining"
                // This permits verifying the total count without reseeking the iterator.
                frame->fixarray_count = iter->array_size - frame->fixarray_count;
            }

            iter->pSize = &frame->fixarray_count;
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_ONEOF)
        {
            // Release old value of the oneof field
#if !PB_NO_MALLOC
            pb_walk_retval_t retval = release_oneof(state, iter, false);
            if (retval != PB_WALK_NEXT_ITEM)
            {
                if (retval == PB_WALK_IN)
                {
                    state->flags |= PB_DECODE_WALK_STATE_FLAG_RELEASE_FIELD;
                    frame->flags |= PB_DECODE_WALK_FRAME_FLAG_END_RELEASE;
                    frame->old_length = (pb_size_t)wire_type;
                }
                return retval;
            }
#endif
        }

        pb_walk_retval_t retval = PB_WALK_NEXT_ITEM;

        if (PB_LTYPE_IS_SUBMSG(iter->type) && PB_ATYPE(iter->type) != PB_ATYPE_CALLBACK)
        {
            // Descend into submessage decoding
            if (wire_type != PB_WT_STRING)
            {
                PB_SET_ERROR(ctx, "wrong wire type");
                return PB_WALK_EXIT_ERR;
            }

            retval = decode_submsg(ctx, state);
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_STATIC)
        {
            if (!decode_static_field(ctx, wire_type, iter))
                return PB_WALK_EXIT_ERR;
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            if (!decode_pointer_field(ctx, wire_type, iter))
                return PB_WALK_EXIT_ERR;
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_CALLBACK)
        {
            // Open a substream with limited length.
            // This informs the callback how much it can read.
            callback_substream_tmp_t tmp;
            if (!open_callback_substream(ctx, wire_type, &tmp))
                return PB_WALK_EXIT_ERR;

#if !PB_NO_STREAM_CALLBACK
            if (PB_LTYPE_IS_SUBMSG(iter->type) && ctx->callback != NULL)
            {
                // We now know the submessage data length, so preload it into cache.
                if (!pb_fill_stream_cache(ctx))
                    return PB_WALK_EXIT_ERR;
            }
#endif

            /* SUBMSG_W_CB is meant to be used with oneof fields, where it permits
             * configuring fields inside a oneof.
             * TODO: consider replacing with some different approach
             */
            if (PB_LTYPE(iter->type) == PB_LTYPE_SUBMSG_W_CB && iter->pSize != NULL) {
                pb_callback_t* callback;
                *(pb_tag_t*)iter->pSize = iter->tag;
                callback = (pb_callback_t*)iter->pSize - 1;

                if (callback->funcs.decode)
                {
                    if (!callback->funcs.decode(ctx, iter, &callback->arg)) {
                        (void)close_callback_substream(ctx, &tmp);
                        PB_SET_ERROR(ctx, "submsg callback failed");
                        return PB_WALK_EXIT_ERR;
                    }
                }
            }

            // Call callback until all data has been processed
            bool status = true;
            pb_size_t prev_bytes_left;
            do
            {
                prev_bytes_left = ctx->bytes_left;
                if (!iter->descriptor->field_callback(ctx, NULL, iter))
                {
                    status = false;
                }
            } while (status && ctx->bytes_left > 0 && ctx->bytes_left < prev_bytes_left);

            if (!close_callback_substream(ctx, &tmp) || !status)
            {
                PB_SET_ERROR(ctx, "callback failed");
                return PB_WALK_EXIT_ERR;
            }
        }
        else
        {
            PB_SET_ERROR(ctx, "invalid field type");
            return PB_WALK_EXIT_ERR;
        }

        if (iter->pSize == &frame->fixarray_count)
        {
            if (frame->fixarray_count > iter->array_size)
            {
                // This can occur for dynamically allocated fixarrays.
                // It would be better to detect this before the unnecessary memory allocation.
                PB_SET_ERROR(ctx, "wrong size for fixed count field");
                return PB_WALK_EXIT_ERR;
            }

            frame->fixarray_count = iter->array_size - frame->fixarray_count;
        }

        if (retval != PB_WALK_NEXT_ITEM)
        {
            return retval;
        }
    }

    if (!eof)
    {
        // pb_decode_tag() returned error before end of stream
        return PB_WALK_EXIT_ERR;
    }

    if (!check_all_required_fields_present(state))
    {
        PB_SET_ERROR(ctx, "missing required field");
        return PB_WALK_EXIT_ERR;
    }

    if (frame->fixarray_count > 0)
    {
        PB_SET_ERROR(ctx, "wrong size for fixed count field");
        return PB_WALK_EXIT_ERR;
    }

    return PB_WALK_OUT;
}

// Allocate stack buffer and run pb_walk() for decoding the message
static bool checkreturn pb_decode_walk_begin(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields, void *dest_struct)
{
    pb_walk_state_t state;
    bool status;
    PB_WALK_DECLARE_STACKBUF(PB_DECODE_INITIAL_STACKSIZE) stackbuf;

    /* Set default values, if needed */
    if ((ctx->flags & PB_DECODE_CTX_FLAG_NOINIT) == 0)
    {
        (void)pb_walk_init(&state, fields, dest_struct, pb_defaults_walk_cb);
        PB_WALK_SET_STACKBUF(&state, stackbuf);
        if (!pb_walk(&state))
        {
            PB_RETURN_ERROR(ctx, "failed to set defaults");
        }
    }

    /* Decode the message */
    (void)pb_walk_init(&state, fields, dest_struct, pb_decode_walk_cb);
    PB_WALK_SET_STACKBUF(&state, stackbuf);
    state.ctx = ctx;
    state.next_stacksize = stacksize_for_msg(fields);

    pb_walk_state_t *old_walkstate = ctx->walk_state;
    ctx->walk_state = &state;
    status = pb_walk(&state);
    ctx->walk_state = old_walkstate;

    if (!status)
    {
        PB_SET_ERROR(ctx, state.errmsg);

#if !PB_NO_MALLOC
        (void)pb_walk_init(&state, fields, dest_struct, pb_release_walk_cb);
        PB_WALK_SET_STACKBUF(&state, stackbuf);
        state.ctx = ctx;
        (void)pb_walk(&state);
#endif

        return false;
    }

    return true;
}

// Reuse already allocated pb_walk_state_t and decode the message
static bool checkreturn pb_decode_walk_reuse(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields, void *dest_struct)
{
    bool status = true;
    pb_walk_state_t *state = ctx->walk_state;
    uint32_t old_flags = state->flags;
    state->depth += 1;

    /* Set default values, if needed */
    if ((ctx->flags & PB_DECODE_CTX_FLAG_NOINIT) == 0)
    {
        state->flags = 0;
        state->iter.pData = dest_struct;
        state->iter.submsg_desc = fields;
        state->retval = PB_WALK_IN;
        state->callback = pb_defaults_walk_cb;
        state->next_stacksize = 0;
        if (!pb_walk(state))
        {
            PB_SET_ERROR(ctx, "failed to set defaults");
            status = false;
        }
        state->callback = pb_decode_walk_cb;
    }

    /* Decode message contents */
    if (status)
    {
        state->flags = 0;
        state->iter.pData = dest_struct;
        state->iter.submsg_desc = fields;
        state->retval = PB_WALK_IN;
        state->next_stacksize = stacksize_for_msg(fields);
        if (!pb_walk(state))
        {
            PB_SET_ERROR(ctx, state->errmsg);
            status = false;
        }
    }

    /* Release allocations on failure*/
#if !PB_NO_MALLOC
    if (!status)
    {
        state->flags = 0;
        state->iter.pData = dest_struct;
        state->iter.submsg_desc = fields;
        state->retval = PB_WALK_IN;
        state->callback = pb_release_walk_cb;
        state->next_stacksize = 0;
        (void)pb_walk(state);
        state->callback = pb_decode_walk_cb;
    }
#endif

    state->flags = old_flags;
    state->depth -= 1;

    return status;
}


bool checkreturn pb_decode_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields,
                             void *dest_struct, size_t struct_size)
{
    // Error in struct_size is typically caused by forgetting to rebuild .pb.c file
    // or by it having different compilation options.
    // NOTE: On GCC, sizeof(*(void*)) == 1
    if (fields->struct_size != struct_size && struct_size > 1)
        PB_RETURN_ERROR(ctx, "struct_size mismatch");

    bool status = true;
    pb_decode_ctx_flags_t orig_flags = ctx->flags;
    pb_size_t old_length = 0;

    if (ctx->flags & PB_DECODE_CTX_FLAG_DELIMITED)
    {
        if (!pb_decode_open_substream(ctx, &old_length))
            return false;

        // Make sure the flag doesn't accidentally apply to recursive calls
        // from user callbacks.
        ctx->flags &= (pb_decode_ctx_flags_t)~PB_DECODE_CTX_FLAG_DELIMITED;
    }

    if (ctx->walk_state != NULL &&
        ctx->walk_state->callback == pb_decode_walk_cb &&
        ctx->walk_state->ctx == ctx)
    {
        // Reuse existing walk state variable to conserve stack space
        status = pb_decode_walk_reuse(ctx, fields, dest_struct);
    }
    else
    {
        // Allocate new walk state on stack
        status = pb_decode_walk_begin(ctx, fields, dest_struct);
    }

    ctx->flags = orig_flags;
    if (ctx->flags & PB_DECODE_CTX_FLAG_DELIMITED)
    {
        if (!pb_decode_close_substream(ctx, old_length))
            status = false;
    }
    
    return status;
}

/*********************************
 * Memory allocation and release *
 *********************************/

#if !PB_NO_MALLOC

/* Allocate storage for 'array_size' entries each of 'data_size' bytes.
 * Uses either the allocator defined by ctx or the default allocator.
 * Pointer to allocate memory is stored in '*ptr'.
 *
 * If old value of '*ptr' is not NULL, realloc is done to expand the allocation.
 * During realloc, the value of '*ptr' may change (old data is copied to new storage).
 *
 * On failure, false is returned and '*ptr' remains unchanged.
 */
bool pb_allocate_field(pb_decode_ctx_t *ctx, void **ptr, pb_size_t data_size, pb_size_t array_size)
{
    PB_OPT_ASSERT(ptr != NULL && ctx != NULL);

    if (data_size == 0 || array_size == 0)
        PB_RETURN_ERROR(ctx, "invalid size");

#ifdef __AVR__
    /* Workaround for AVR libc bug 53284: http://savannah.nongnu.org/bugs/?53284
     * Realloc to size of 1 byte can cause corruption of the malloc structures.
     */
    if (data_size == 1 && array_size == 1)
    {
        data_size = 2;
    }
#endif

    /* Check for multiplication overflows.
     * This code avoids the costly division if the sizes are small enough.
     * Multiplication is safe as long as only half of bits are set
     * in either multiplicand.
     */
    {
        const pb_size_t check_limit = (pb_size_t)1 << (pb_size_t)(sizeof(pb_size_t) * 4);
        if (data_size >= check_limit || array_size >= check_limit)
        {
            if (PB_SIZE_MAX / array_size < data_size)
            {
                PB_RETURN_ERROR(ctx, "size too large");
            }
        }
    }

    pb_size_t bytes = array_size * data_size;
    void *new_ptr = NULL;

#if !PB_NO_CONTEXT_ALLOCATOR
    if (ctx != NULL && ctx->allocator != NULL)
    {
        new_ptr = ctx->allocator->realloc(ctx->allocator, *ptr, bytes);
    }
    else
#endif
#if !PB_NO_DEFAULT_ALLOCATOR
    {
        new_ptr = pb_realloc(*ptr, bytes);
    }
#else
    {
        PB_RETURN_ERROR(ctx, "no allocator");
    }
#endif

    if (new_ptr == NULL)
        PB_RETURN_ERROR(ctx, "realloc failed");

    *ptr = new_ptr;
    return true;
}

/* Release storage previously allocated by pb_allocate_field().
 * Uses either the allocator defined by ctx or the default allocator.
 */
void pb_release_field(pb_decode_ctx_t *ctx, void *ptr)
{
#if PB_NO_DEFAULT_ALLOCATOR
    PB_OPT_ASSERT(ctx != NULL);
#endif

    if (ptr == NULL)
        return; // Nothing to do

#if !PB_NO_CONTEXT_ALLOCATOR
    if (ctx != NULL && ctx->allocator != NULL)
    {
        ctx->allocator->free(ctx->allocator, ptr);
    }
    else
#endif
    {
#if !PB_NO_DEFAULT_ALLOCATOR
        pb_free(ptr);
#endif
    }
}

/* Release a single structure field based on its field_iter information. */
static void pb_release_single_field(pb_decode_ctx_t *ctx, pb_field_iter_t *field)
{
    pb_type_t type;
    type = field->type;

    if (PB_ATYPE(type) != PB_ATYPE_POINTER)
        return;
    
    if (PB_HTYPE(type) == PB_HTYPE_ONEOF)
    {
        /* Set oneof which_field to 0 */
        PB_OPT_ASSERT(*(pb_tag_t*)field->pSize == field->tag);
        *(pb_tag_t*)field->pSize = 0;
    }

    if (PB_LTYPE(type) == PB_LTYPE_BYTES)
    {
        // Pointer-type bytes fields store the pointer inside pb_bytes_t
        if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
        {
            /* Release entries in repeated bytes array */
            pb_bytes_t *pItem = *(pb_bytes_t**)field->pField;
            pb_size_t count = *(pb_size_t*)field->pSize;
            pb_size_t i;
            for (i = 0; i < count; i++)
            {
                pb_release_field(ctx, pItem[i].bytes);
                pItem[i].size = 0;
                pItem[i].bytes = NULL;
            }

            /* Release the array itself */
            *(pb_size_t*)field->pSize = 0;
            pb_release_field(ctx, *(void**)field->pField);
            *(void**)field->pField = field->pData = NULL;
        }
        else
        {
            /* Release pointer stored inside the struct */
            PB_OPT_ASSERT(field->data_size == sizeof(pb_bytes_t));
            pb_bytes_t *bytes = (pb_bytes_t*)field->pData;
            pb_release_field(ctx, bytes->bytes);
            bytes->size = 0;
            bytes->bytes = NULL;
        }
    }
    else if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
    {
        if (PB_LTYPE(type) == PB_LTYPE_STRING)
        {
            /* Release entries in repeated string array */
            char **pItem = *(char***)field->pField;
            pb_size_t count = *(pb_size_t*)field->pSize;
            pb_size_t i;
            for (i = 0; i < count; i++)
            {
                pb_release_field(ctx, pItem[i]);
                pItem[i] = NULL;
            }
        }

        /* Release the array itself */
        *(pb_size_t*)field->pSize = 0;
        pb_release_field(ctx, *(void**)field->pField);
        *(void**)field->pField = field->pData = NULL;
    }
    else
    {
        /* Just a single item to release */
        pb_release_field(ctx, *(void**)field->pField);
        *(void**)field->pField = field->pData = NULL;
    }
}

static pb_walk_retval_t pb_release_walk_cb(pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;
    pb_decode_ctx_t *ctx = (pb_decode_ctx_t *)state->ctx;

    // Check the previous action
    if (state->retval == PB_WALK_OUT)
    {
        // Submessage is done, release it (if pointer) and go to
        // next array item or field.

        if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
            {
                char *array_start = *(char**)iter->pField;
                char *array_end = array_start + iter->data_size * *(pb_size_t*)iter->pSize;
                char *position = (char*)iter->pData;
                if (position >= array_start && position + iter->data_size < array_end)
                {
                    // More array items remain before we can release the array itself
                    return PB_WALK_NEXT_ITEM;
                }

                // All of the repeated submessages have been released
                *(pb_size_t*)iter->pSize = 0;
            }

            pb_release_single_field(ctx, iter);
        }

        return PB_WALK_NEXT_ITEM;
    }

    do {
        if (iter->pData == NULL)
        {
            // Pointer is already null
            continue;
        }

        if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
        {
            pb_extension_t* extension = *(pb_extension_t**)iter->pData;
            if (extension)
            {
                // Descend into extension
                return pb_walk_into(state, extension->type, pb_get_extension_data_ptr(extension));
            }
            continue;
        }

        if (PB_ATYPE(iter->type) == PB_ATYPE_CALLBACK)
        {
            // Nothing to do for callback fields
            continue;
        }

        if (PB_HTYPE(iter->type) == PB_HTYPE_ONEOF &&
            *(pb_tag_t*)iter->pSize != iter->tag)
        {
            // This is not the currently present field in the union
            continue;
        }

        if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED &&
            *(pb_size_t*)iter->pSize == 0)
        {
            // Empty array
            if (iter->pData != NULL && PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
            {
                pb_release_field(ctx, *(void**)iter->pField);
                *(void**)iter->pField = iter->pData = NULL;
            }
            continue;
        }

        if (PB_LTYPE_IS_SUBMSG(iter->type) &&
            (iter->submsg_desc->msg_flags & PB_MSGFLAG_R_HAS_PTRS) != 0)
        {
            // Descend into submessage
            PB_OPT_ASSERT(iter->submsg_desc->struct_size == iter->data_size);
            return PB_WALK_IN;
        }

        if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            pb_release_single_field(ctx, iter);
        }
    } while (pb_field_iter_next(iter));

    return PB_WALK_OUT;
}

bool pb_release_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields, void *dest_struct, size_t struct_size)
{
    if (fields->struct_size != struct_size && struct_size > 1)
    {
        if (ctx) PB_SET_ERROR(ctx, "struct_size mismatch");
        return false;
    }

    if (!dest_struct)
        return true; /* Ignore NULL pointers, similar to free() */

    pb_walk_state_t state;
    PB_WALK_DECLARE_STACKBUF(PB_RELEASE_INITIAL_STACKSIZE) stackbuf;

    (void)pb_walk_init(&state, fields, dest_struct, pb_release_walk_cb);
    PB_WALK_SET_STACKBUF(&state, stackbuf);
    state.ctx = ctx;

    return pb_walk(&state);
}
#else
bool pb_release_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields, void *dest_struct, size_t struct_size)
{
    /* Nothing to release without PB_ENABLE_MALLOC. */
    PB_UNUSED(ctx);
    PB_UNUSED(dest_struct);

    if (fields->struct_size != struct_size && struct_size > 1)
    {
        if (ctx) PB_SET_ERROR(ctx, "struct_size mismatch");
        return false;
    }

    return true;
}
#endif

/* Field decoders */

bool pb_decode_bool(pb_decode_ctx_t *ctx, bool *dest)
{
    uint32_t value;
    if (!pb_decode_varint32(ctx, &value))
        return false;

    *(bool*)dest = (value != 0);
    return true;
}

bool pb_decode_svarint(pb_decode_ctx_t *ctx, pb_int64_t *dest)
{
    pb_uint64_t value;
    if (!pb_decode_varint(ctx, &value))
        return false;
    
    if (value & 1)
        *dest = (pb_int64_t)(~(value >> 1));
    else
        *dest = (pb_int64_t)(value >> 1);
    
    return true;
}

bool pb_decode_fixed32(pb_decode_ctx_t *ctx, void *dest)
{
    union {
        uint32_t fixed32;
        pb_byte_t bytes[4];
    } u = {0};

    if (!pb_read(ctx, u.bytes, 4))
        return false;

#if PB_LITTLE_ENDIAN_8BIT
    /* fast path - if we know that we're on little endian, assign directly */
    *(uint32_t*)dest = u.fixed32;
#else
    *(uint32_t*)dest = ((uint32_t)u.bytes[0] << 0) |
                       ((uint32_t)u.bytes[1] << 8) |
                       ((uint32_t)u.bytes[2] << 16) |
                       ((uint32_t)u.bytes[3] << 24);
#endif
    return true;
}

#if !PB_WITHOUT_64BIT
bool pb_decode_fixed64(pb_decode_ctx_t *ctx, void *dest)
{
    union {
        uint64_t fixed64;
        pb_byte_t bytes[8];
    } u = {0};

    if (!pb_read(ctx, u.bytes, 8))
        return false;

#if PB_LITTLE_ENDIAN_8BIT
    /* fast path - if we know that we're on little endian, assign directly */
    *(uint64_t*)dest = u.fixed64;
#else
    *(uint64_t*)dest = ((uint64_t)u.bytes[0] << 0) |
                       ((uint64_t)u.bytes[1] << 8) |
                       ((uint64_t)u.bytes[2] << 16) |
                       ((uint64_t)u.bytes[3] << 24) |
                       ((uint64_t)u.bytes[4] << 32) |
                       ((uint64_t)u.bytes[5] << 40) |
                       ((uint64_t)u.bytes[6] << 48) |
                       ((uint64_t)u.bytes[7] << 56);
#endif
    return true;
}
#endif

static bool checkreturn pb_dec_varint(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    if (PB_LTYPE(field->type) == PB_LTYPE_UVARINT)
    {
        pb_uint64_t value, clamped;
        if (!pb_decode_varint(ctx, &value))
            return false;

        /* Cast to the proper field size, while checking for overflows */
        if (field->data_size == sizeof(pb_uint64_t))
            clamped = *(pb_uint64_t*)field->pData = value;
        else if (field->data_size == sizeof(uint32_t))
            clamped = *(uint32_t*)field->pData = (uint32_t)value;
        else if (field->data_size == sizeof(uint_least16_t))
            clamped = *(uint_least16_t*)field->pData = (uint_least16_t)value;
        else if (field->data_size == sizeof(uint_least8_t))
            clamped = *(uint_least8_t*)field->pData = (uint_least8_t)value;
        else
            PB_RETURN_ERROR(ctx, "invalid data_size");

        if (clamped != value)
            PB_RETURN_ERROR(ctx, "integer too large");

        return true;
    }
    else
    {
        pb_uint64_t value;
        pb_int64_t svalue;
        pb_int64_t clamped;

        if (PB_LTYPE(field->type) == PB_LTYPE_SVARINT)
        {
            if (!pb_decode_svarint(ctx, &svalue))
                return false;
        }
        else
        {
            if (!pb_decode_varint(ctx, &value))
                return false;

            /* See issue 97: Google's C++ protobuf allows negative varint values to
            * be cast as int32_t, instead of the int64_t that should be used when
            * encoding. Nanopb versions before 0.2.5 had a bug in encoding. In order to
            * not break decoding of such messages, we cast <=32 bit fields to
            * int32_t first to get the sign correct.
            */
            if (field->data_size == sizeof(pb_int64_t))
                svalue = (pb_int64_t)value;
            else
                svalue = (int32_t)value;
        }

        /* Cast to the proper field size, while checking for overflows */
        if (field->data_size == sizeof(pb_int64_t))
            clamped = *(pb_int64_t*)field->pData = svalue;
        else if (field->data_size == sizeof(int32_t))
            clamped = *(int32_t*)field->pData = (int32_t)svalue;
        else if (field->data_size == sizeof(int_least16_t))
            clamped = *(int_least16_t*)field->pData = (int_least16_t)svalue;
        else if (field->data_size == sizeof(int_least8_t))
            clamped = *(int_least8_t*)field->pData = (int_least8_t)svalue;
        else
            PB_RETURN_ERROR(ctx, "invalid data_size");

        if (clamped != svalue)
            PB_RETURN_ERROR(ctx, "integer too large");

        return true;
    }
}

static bool checkreturn pb_dec_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t size_u32;
    pb_size_t size;
    pb_byte_t *dest;
    
    if (!pb_decode_varint32(ctx, &size_u32))
        return false;
    
    if (size_u32 > PB_SIZE_MAX - offsetof(pb_bytes_array_t, bytes))
        PB_RETURN_ERROR(ctx, "bytes overflow");
    
    size = (pb_size_t)size_u32;

    if (size > ctx->bytes_left)
        PB_RETURN_ERROR(ctx, "end-of-stream");
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
#if PB_NO_MALLOC
        PB_RETURN_ERROR(ctx, "no malloc support");
#else
        PB_OPT_ASSERT(field->data_size == sizeof(pb_bytes_t));
        pb_bytes_t *bytes = (pb_bytes_t*)field->pData;
        bytes->size = size;

        void *alloc = bytes->bytes;
        if (size > 0 && !pb_allocate_field(ctx, &alloc, size, 1))
            return false;
        dest = bytes->bytes = (pb_byte_t*)alloc;
#endif
    }
    else
    {
        pb_size_t alloc_size = PB_BYTES_ARRAY_T_ALLOCSIZE(size);
        if (alloc_size > field->data_size)
            PB_RETURN_ERROR(ctx, "bytes overflow");

        pb_bytes_array_t *bytes = (pb_bytes_array_t*)field->pData;
        bytes->size = size;
        dest = bytes->bytes;
    }

    return pb_read(ctx, dest, size);
}

static bool checkreturn pb_dec_string(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t size_u32;
    pb_size_t size;
    pb_size_t alloc_size;
    pb_byte_t *dest = (pb_byte_t*)field->pData;

    if (!pb_decode_varint32(ctx, &size_u32))
        return false;

    // Note: this comparison also ensures that
    // size_u32 < PB_SIZE_MAX - 1
    // because ctx->bytes_left was decremented by at least
    // one when the varint was read.
    if ((pb_size_t)size_u32 > ctx->bytes_left)
        PB_RETURN_ERROR(ctx, "end-of-stream");

    /* Space for null terminator */
    size = (pb_size_t)size_u32;
    alloc_size = (pb_size_t)(size + 1);

    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
#if PB_NO_MALLOC
        PB_RETURN_ERROR(ctx, "no malloc support");
#else
        if (!pb_allocate_field(ctx, (void**)field->pData, alloc_size, 1))
            return false;
        dest = *(pb_byte_t**)field->pData;
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            PB_RETURN_ERROR(ctx, "string overflow");
    }
    
    dest[size] = 0;

    if (!pb_read(ctx, dest, size))
        return false;

#if !PB_NO_VALIDATE_UTF8
    if ((ctx->flags & PB_DECODE_CTX_FLAG_NO_VALIDATE_UTF8) == 0 &&
        !pb_validate_utf8((const char*)dest))
    {
        PB_RETURN_ERROR(ctx, "invalid utf8");
    }
#endif

    return true;
}

static bool checkreturn pb_dec_fixed_length_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t size_u32;

    if (!pb_decode_varint32(ctx, &size_u32))
        return false;

    if (size_u32 == 0)
    {
        /* As a special case, treat empty bytes string as all zeros for fixed_length_bytes. */
        memset(field->pData, 0, (size_t)field->data_size);
        return true;
    }

    if (size_u32 != field->data_size)
        PB_RETURN_ERROR(ctx, "incorrect fixed length bytes size");

    return pb_read(ctx, (pb_byte_t*)field->pData, field->data_size);
}

#if PB_CONVERT_DOUBLE_FLOAT
bool pb_decode_double_as_float(pb_decode_ctx_t *ctx, float *dest)
{
    uint_least8_t sign;
    int exponent;
    uint32_t mantissa;
    uint64_t value;
    union { float f; uint32_t i; } out;

    if (!pb_decode_fixed64(ctx, &value))
        return false;

    /* Decompose input value */
    sign = (uint_least8_t)((value >> 63) & 1);
    exponent = (int)((value >> 52) & 0x7FF) - 1023;
    mantissa = (value >> 28) & 0xFFFFFF; /* Highest 24 bits */

    /* Figure if value is in range representable by floats. */
    if (exponent == 1024)
    {
        /* Special value */
        exponent = 128;
        mantissa >>= 1;
    }
    else
    {
        if (exponent > 127)
        {
            /* Too large, convert to infinity */
            exponent = 128;
            mantissa = 0;
        }
        else if (exponent < -150)
        {
            /* Too small, convert to zero */
            exponent = -127;
            mantissa = 0;
        }
        else if (exponent < -126)
        {
            /* Denormalized */
            mantissa |= 0x1000000;
            mantissa >>= (-126 - exponent);
            exponent = -127;
        }

        /* Round off mantissa */
        mantissa = (mantissa + 1) >> 1;

        /* Check if mantissa went over 2.0 */
        if (mantissa & 0x800000)
        {
            exponent += 1;
            mantissa &= 0x7FFFFF;
            mantissa >>= 1;
        }
    }

    /* Combine fields */
    out.i = mantissa;
    out.i |= (uint32_t)(exponent + 127) << 23;
    out.i |= (uint32_t)sign << 31;

    *dest = out.f;
    return true;
}
#endif
