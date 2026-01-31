/* pb_encode.c -- encode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_encode.h"
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
#if PB_WITHOUT_64BIT
#define pb_int64_t int32_t
#define pb_uint64_t uint32_t
#else
#define pb_int64_t int64_t
#define pb_uint64_t uint64_t
#endif

// It doesn't make sense to try onepass encoding if the buffer is very small.
#ifndef PB_ENCODE_MIN_ONEPASS_BUFFER_SIZE
#define PB_ENCODE_MIN_ONEPASS_BUFFER_SIZE 16
#endif

// Information that is needed per each message level during encoding
typedef struct {
    pb_size_t msg_start_pos;
    uint_least16_t flags;
} pb_encode_walk_stackframe_t;

// Total stackframe size, including PB_WALK internal stackframe
#define PB_ENCODE_STACKFRAME_SIZE (PB_WALK_STACKFRAME_SIZE + PB_WALK_ALIGN(sizeof(pb_encode_walk_stackframe_t)))

// Amount of stack initially allocated for encoding.
// pb_walk() will allocate more automatically unless recursion is disabled.
#ifndef PB_ENCODE_INITIAL_STACKSIZE
#define PB_ENCODE_INITIAL_STACKSIZE (PB_MESSAGE_NESTING * PB_ENCODE_STACKFRAME_SIZE)
#endif

// Make sure each recursion level can fit at least one frame
PB_STATIC_ASSERT(PB_WALK_STACK_SIZE > PB_ENCODE_STACKFRAME_SIZE, PB_WALK_STACK_SIZE_is_too_small)

static bool checkreturn pb_check_proto3_default_value(const pb_field_iter_t *field);
static bool checkreturn encode_basic_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static uint_fast8_t pb_encode_buffer_varint32(pb_byte_t *buffer, uint32_t value);
static inline bool checkreturn pb_encode_varint32(pb_encode_ctx_t *ctx, uint32_t value);
static inline bool safe_read_bool(const void *pSize);
static bool checkreturn pb_enc_varint(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_string(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);

#if !PB_ENCODE_ARRAYS_UNPACKED
static bool checkreturn encode_packed_array(pb_encode_ctx_t *ctx, pb_field_iter_t *field, pb_size_t count);
#endif

/*******************************
 * pb_ostream_t implementation *
 *******************************/

void pb_init_encode_ctx_for_buffer(pb_encode_ctx_t *ctx, pb_byte_t *buf, pb_size_t bufsize)
{
    ctx->flags = 0;
    ctx->buffer = buf;
    ctx->max_size = bufsize;
    ctx->bytes_written = 0;

#if !PB_NO_STREAM_CALLBACK
    ctx->callback = NULL;
    ctx->state = NULL;
    ctx->buffer_size = bufsize;
    ctx->buffer_count = 0;
#endif

#if !PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif

    ctx->walk_state = NULL;
}

void pb_init_encode_ctx_sizing(pb_encode_ctx_t *ctx)
{
    ctx->flags = PB_ENCODE_CTX_FLAG_SIZING;
    ctx->buffer = NULL;
    ctx->max_size = PB_SIZE_MAX;
    ctx->bytes_written = 0;

#if !PB_NO_STREAM_CALLBACK
    ctx->callback = NULL;
    ctx->state = NULL;
    ctx->buffer_size = 0;
    ctx->buffer_count = 0;
#endif

#if !PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif

    ctx->walk_state = NULL;
}

#if !PB_NO_STREAM_CALLBACK
void pb_init_encode_ctx_for_callback(pb_encode_ctx_t *ctx,
    pb_encode_ctx_write_callback_t callback, void *state,
    pb_size_t max_size, pb_byte_t *buf, pb_size_t bufsize)
{
    ctx->flags = 0;
    ctx->callback = callback;
    ctx->state = state;

    ctx->max_size = max_size;
    ctx->bytes_written = 0;

    ctx->buffer = buf;

    ctx->buffer_size = bufsize;
    ctx->buffer_count = 0;

#if !PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif

    ctx->walk_state = NULL;
}
#endif

#if !PB_NO_STREAM_CALLBACK
// Flush any buffered data to callback
inline bool pb_flush_write_buffer(pb_encode_ctx_t *ctx)
{
    if (ctx->callback != NULL && ctx->buffer_count > 0)
    {
        if (ctx->flags & PB_ENCODE_CTX_FLAG_ONEPASS_SIZING)
        {
            // We can't flush yet because sizing pass is not finished
            PB_RETURN_ERROR(ctx, "onepass flush");
        }

        if (!ctx->callback(ctx, ctx->buffer, (size_t)ctx->buffer_count))
            PB_RETURN_ERROR(ctx, "io error");

        ctx->buffer_count = 0;
    }

    return true;
}
#else
inline bool pb_flush_write_buffer(pb_encode_ctx_t *ctx)
{
    PB_UNUSED(ctx);
    return true;
}
#endif

// Get pointer for directly writing up to max_bytes data.
// Returns NULL if not enough space.
static inline pb_byte_t *pb_bufwrite_start(pb_encode_ctx_t *ctx, pb_size_t max_bytes)
{
    if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
        return NULL;

#if !PB_NO_STREAM_CALLBACK
    if (ctx->bytes_written + max_bytes <= ctx->max_size)
    {
        if (ctx->buffer_count + max_bytes > ctx->buffer_size)
        {
            if (max_bytes > ctx->buffer_size)
            {
                return NULL; // Buffer is just too small
            }
            else if (ctx->flags & PB_ENCODE_CTX_FLAG_ONEPASS_SIZING)
            {
                return NULL; // pb_write() will convert this to sizing stream
            }
            else
            {
                // Flush buffer to release space
                if (!pb_flush_write_buffer(ctx))
                    return NULL; // Error will be returned when caller resorts to pb_write().
            }
        }

        return ctx->buffer + ctx->buffer_count;
    }
    else
    {
        return NULL;
    }
#else
    if (ctx->bytes_written + max_bytes <= ctx->max_size)
        return ctx->buffer + ctx->bytes_written;
    else
        return NULL;
#endif
}

// Finish write to buffer previously obtained from pb_get_wrptr().
static inline void pb_bufwrite_done(pb_encode_ctx_t *ctx, pb_size_t bytes_written)
{
#if !PB_NO_STREAM_CALLBACK
    ctx->bytes_written += bytes_written;
    ctx->buffer_count += bytes_written;
#else
    ctx->bytes_written += bytes_written;
#endif
}

// Write bytes to stream by copying them from source buffer.
bool checkreturn pb_write(pb_encode_ctx_t *ctx, const pb_byte_t *buf, pb_size_t count)
{
    if (count == 0)
        return true;

    if ((pb_size_t)(ctx->bytes_written + count) < ctx->bytes_written)
    {
        PB_RETURN_ERROR(ctx, "stream full"); // pb_size_t overflow
    }

    if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
    {
        ctx->bytes_written += count;
        return true;
    }

    if (ctx->bytes_written + count > ctx->max_size)
    {
        PB_RETURN_ERROR(ctx, "stream full");
    }

#if !PB_NO_STREAM_CALLBACK
    pb_byte_t *target = pb_bufwrite_start(ctx, count);
    if (target)
    {
        memcpy(target, buf, count * sizeof(pb_byte_t));
        pb_bufwrite_done(ctx, count);
        return true;
    }
    else if (ctx->flags & PB_ENCODE_CTX_FLAG_ONEPASS_SIZING)
    {
        // Memory buffer is full, convert to sizing stream
        ctx->flags |= PB_ENCODE_CTX_FLAG_SIZING;
        ctx->bytes_written += count;
        return true;
    }

    if (ctx->callback)
    {
        // Flush any preceding data and write to output callback directly
        if (pb_flush_write_buffer(ctx) &&
            ctx->callback(ctx, buf, (size_t)count))
        {
            ctx->bytes_written += count;
            return true;
        }
    }

    // If we get this far, the callback failed.
    // pb_bufwrite_start() should return non-NULL for memory streams.
    PB_RETURN_ERROR(ctx, "io error");
#else
    memcpy(ctx->buffer + ctx->bytes_written, buf, count);
    ctx->bytes_written += count;
    return true;
#endif
}

/*************************
 * Encode a single field *
 *************************/

/* Read a bool value without causing undefined behavior even if the value
 * is invalid. The goal of this is to gracefully handle conditions where the
 * structure is uninitialized or otherwise corrupted. See issue #434 and
 * https://stackoverflow.com/questions/27661768/weird-results-for-conditional
 */
static inline bool safe_read_bool(const void *pSize)
{
    switch (sizeof(bool))
    {
        case 1: return *(const char*)pSize != 0;
        case 2: return *(const uint16_t*)pSize != 0;
        case 4: return *(const uint32_t*)pSize != 0;
        case 8: return *(const pb_uint64_t*)pSize != 0;
        default: return *(const bool*)pSize;
    }
}

/* Encode a packed array in WT_STRING data. */
#if !PB_ENCODE_ARRAYS_UNPACKED
static bool checkreturn encode_packed_array(pb_encode_ctx_t *ctx, pb_field_iter_t *field, pb_size_t count)
{
    pb_size_t i;
    pb_size_t size;

    if (!pb_encode_tag(ctx, PB_WT_STRING, field->tag))
        return false;
    
    /* Determine the total size of packed array. */
    if (PB_LTYPE(field->type) == PB_LTYPE_BOOL)
    {
        size = count;
    }
    else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32)
    {
        size = 4 * count;
    }
    else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
    {
        size = 8 * count;
    }
    else
    {
        pb_encode_ctx_flags_t flags_orig = ctx->flags;
        pb_size_t size_orig = ctx->bytes_written;
        ctx->flags |= PB_ENCODE_CTX_FLAG_SIZING;

        void *pData_orig = field->pData;
        for (i = 0; i < count; i++)
        {
            if (!pb_enc_varint(ctx, field))
                return false;
            field->pData = (char*)field->pData + field->data_size;
        }
        field->pData = pData_orig;
        size = (pb_size_t)(ctx->bytes_written - size_orig);
        ctx->bytes_written = size_orig;
        ctx->flags = flags_orig;
    }

    /* Write the size prefix */
    if (!pb_encode_varint32(ctx, (uint32_t)size))
        return false;

    if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
        return pb_write(ctx, NULL, size); /* Just sizing.. */

    /* Write the data */
    if (PB_LTYPE(field->type) == PB_LTYPE_BOOL)
    {
        for (i = 0; i < count; i++)
        {
            pb_byte_t byte = safe_read_bool(field->pData) ? 1 : 0;
            if (!pb_write(ctx, &byte, 1))
                return false;

            field->pData = (char*)field->pData + field->data_size;
        }

        return true;
    }

    if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32)
    {
        PB_OPT_ASSERT(field->data_size == sizeof(uint32_t));
#if PB_LITTLE_ENDIAN_8BIT
        // Data can be written directly
        return pb_write(ctx, (pb_byte_t*)field->pData, size);
#else
        // Convert endianess
        for (i = 0; i < count; i++)
        {
            if (!pb_encode_fixed32(ctx, field->pData))
                return false;

            field->pData = (char*)field->pData + field->data_size;
        }
        return true;
#endif
    }

    if (PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
    {
#if !PB_WITHOUT_64BIT
        if (field->data_size == sizeof(uint64_t))
        {
#if PB_LITTLE_ENDIAN_8BIT
            // Data can be written directly
            return pb_write(ctx, (pb_byte_t*)field->pData, size);
#else
            // Convert endianess
            for (i = 0; i < count; i++)
            {
                if (!pb_encode_fixed64(ctx, field->pData))
                    return false;

                field->pData = (char*)field->pData + field->data_size;
            }

            return true;
#endif /* PB_LITTLE_ENDIAN_8BIT */
        }
#endif /* PB_WITHOUT_64BIT */

#if PB_CONVERT_DOUBLE_FLOAT
        /* On AVR, there is no 64-bit double, but we can convert float */
        if (field->data_size == sizeof(float))
        {
            for (i = 0; i < count; i++)
            {
                if (!pb_encode_float_as_double(ctx, *(float*)field->pData))
                    return false;

                field->pData = (char*)field->pData + field->data_size;
            }

            return true;
        }
#endif /* PB_CONVERT_DOUBLE_FLOAT */

        PB_RETURN_ERROR(ctx, "invalid data_size");
    } /* PB_LTYPE(field->type) == PB_LTYPE_FIXED64 */

    // Remaining types are VARINT/UVARINT/SVARINT
    for (i = 0; i < count; i++)
    {
        if (!pb_enc_varint(ctx, field))
            return false;

        field->pData = (char*)field->pData + field->data_size;
    }

    return true;
}
#endif /* PB_ENCODE_ARRAYS_UNPACKED */

/* In proto3, all fields are optional and are only encoded if their value is "non-zero".
 * This function implements the check for the zero value. */
static bool checkreturn pb_check_proto3_default_value(const pb_field_iter_t *field)
{
    pb_type_t type = field->type;

    if (PB_ATYPE(type) == PB_ATYPE_STATIC)
    {
        if (PB_HTYPE(type) == PB_HTYPE_REQUIRED)
        {
            /* Required proto2 fields inside proto3 submessage, pretty rare case */
            return false;
        }
        else if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
        {
            /* Repeated fields inside proto3 submessage: present if count != 0 */
            return *(const pb_size_t*)field->pSize == 0;
        }
        else if (PB_HTYPE(type) == PB_HTYPE_ONEOF)
        {
            /* Oneof fields */
            return *(const pb_tag_t*)field->pSize == 0;
        }
        else if (PB_HTYPE(type) == PB_HTYPE_OPTIONAL && field->pSize != NULL)
        {
            /* Proto2 optional fields inside proto3 message, or proto3
             * submessage fields. */
            return safe_read_bool(field->pSize) == false;
        }
        else if (field->descriptor->default_value)
        {
            /* Proto3 messages do not have default values, but proto2 messages
             * can contain optional fields without has_fields (generator option 'proto3').
             * In this case they must always be encoded, to make sure that the
             * non-zero default value is overwritten.
             */
            return false;
        }

        /* Rest is proto3 singular fields */
        if (PB_LTYPE(type) <= PB_LTYPE_LAST_PACKABLE)
        {
            /* Simple integer / float fields */
            pb_size_t i;
            const char *p = (const char*)field->pData;
            for (i = 0; i < field->data_size; i++)
            {
                if (p[i] != 0)
                {
                    return false;
                }
            }

            return true;
        }
        else if (PB_LTYPE(type) == PB_LTYPE_BYTES)
        {
            const pb_bytes_array_t *bytes = (const pb_bytes_array_t*)field->pData;
            return bytes->size == 0;
        }
        else if (PB_LTYPE(type) == PB_LTYPE_STRING)
        {
            return *(const char*)field->pData == '\0';
        }
        else if (PB_LTYPE(type) == PB_LTYPE_FIXED_LENGTH_BYTES)
        {
            /* Fixed length bytes is only empty if its length is fixed
             * as 0. Which would be pretty strange, but we can check
             * it anyway. */
            return field->data_size == 0;
        }
        else if (PB_LTYPE_IS_SUBMSG(type))
        {
            /* Check all fields in the submessage to find if any of them
             * are non-zero. The comparison cannot be done byte-per-byte
             * because the C struct may contain padding bytes that must
             * be skipped. Note that usually proto3 submessages have
             * a separate has_field that is checked earlier in this if.
             *
             * FIXME: this still has recursion
             */
            pb_field_iter_t iter;
            if (pb_field_iter_begin(&iter, field->submsg_desc, field->pData))
            {
                do
                {
                    if (!pb_check_proto3_default_value(&iter))
                    {
                        return false;
                    }
                } while (pb_field_iter_next(&iter));
            }
            return true;
        }
    }
    else if (PB_ATYPE(type) == PB_ATYPE_POINTER &&
             PB_LTYPE(type) == PB_LTYPE_BYTES &&
             PB_HTYPE(type) != PB_HTYPE_REPEATED)
    {
        return ((pb_bytes_t*)field->pData)->size != 0;
    }
    else if (PB_ATYPE(type) == PB_ATYPE_POINTER)
    {
        return field->pData == NULL;
    }
    else if (PB_ATYPE(type) == PB_ATYPE_CALLBACK)
    {
        if (PB_LTYPE(type) == PB_LTYPE_EXTENSION)
        {
#if !PB_NO_EXTENSIONS
            const pb_extension_t *extension = *(const pb_extension_t* const *)field->pData;
            return extension == NULL;
#else
            return true;
#endif
        }
        else if (field->descriptor->field_callback == pb_default_field_callback)
        {
            pb_callback_t *pCallback = (pb_callback_t*)field->pData;
            return pCallback->funcs.encode == NULL;
        }
        else
        {
            return field->descriptor->field_callback == NULL;
        }
    }

    return false; /* Not typically reached, safe default for weird special cases. */
}

/* Encode a field with static or pointer allocation, i.e. one whose data
 * is available to the encoder directly. */
static bool checkreturn encode_basic_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    if (!field->pData)
    {
        /* Missing pointer field */
        return true;
    }

    if (!pb_encode_tag_for_field(ctx, field))
        return false;

    switch (PB_LTYPE(field->type))
    {
        case PB_LTYPE_BOOL:
            {
                pb_byte_t byte = safe_read_bool(field->pData) ? 1 : 0;
                return pb_write(ctx, &byte, 1);
            }

        case PB_LTYPE_VARINT:
        case PB_LTYPE_UVARINT:
        case PB_LTYPE_SVARINT:
            return pb_enc_varint(ctx, field);

        case PB_LTYPE_FIXED32:
            PB_OPT_ASSERT(field->data_size == sizeof(uint32_t));
            return pb_encode_fixed32(ctx, field->pData);

        case PB_LTYPE_FIXED64:
#if !PB_WITHOUT_64BIT
            if (field->data_size == sizeof(uint64_t))
            {
                return pb_encode_fixed64(ctx, field->pData);
            }
#endif
#if PB_CONVERT_DOUBLE_FLOAT
            if (field->data_size == sizeof(float) && PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
            {
                return pb_encode_float_as_double(ctx, *(float*)field->pData);
            }
#endif
            PB_RETURN_ERROR(ctx, "invalid data_size");

        case PB_LTYPE_BYTES:
            return pb_enc_bytes(ctx, field);

        case PB_LTYPE_STRING:
            return pb_enc_string(ctx, field);

        case PB_LTYPE_FIXED_LENGTH_BYTES:
            return pb_encode_string(ctx, (const pb_byte_t*)field->pData, field->data_size);

        case PB_LTYPE_SUBMESSAGE:
        case PB_LTYPE_SUBMSG_W_CB:
        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
}

/* Check if field is present */
static bool field_present(const pb_field_iter_t *field)
{
    if (PB_HTYPE(field->type) == PB_HTYPE_ONEOF)
    {
        if (*(const pb_tag_t*)field->pSize != field->tag)
        {
            /* Different type oneof field */
            return false;
        }
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_OPTIONAL)
    {
        if (field->pSize)
        {
            if (safe_read_bool(field->pSize) == false)
            {
                /* Missing optional field */
                return false;
            }
        }
        else if (PB_ATYPE(field->type) == PB_ATYPE_STATIC)
        {
            /* Proto3 singular field */
            if (pb_check_proto3_default_value(field))
                return false;
        }
        else if (PB_ATYPE(field->type) == PB_ATYPE_POINTER &&
                 PB_LTYPE(field->type) == PB_LTYPE_BYTES)
        {
            const pb_bytes_t *bytes = (const pb_bytes_t*)field->pData;
            return bytes->size != 0;
        }
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
    {
        if (field->pSize)
        {
            if (*(pb_size_t*)field->pSize == 0)
            {
                /* Empty array */
                return false;
            }
        }
    }

    if (!field->pData)
    {
        /* Pointer field set to NULL */
        return false;
    }

    return true;
}

/*********************
 * Encode all fields *
 *********************/

// State flags are stored in pb_walk_state_t and persist across
// message tree levels.
#define PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG      (uint_least16_t)1

// Frame flags are stored in pb_encode_walk_stackframe_t and
// apply only to a single level.
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1      (uint_least16_t)1
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_NESTED     (uint_least16_t)2
#define PB_ENCODE_WALK_FRAME_FLAG_BYTE_RESERVED     (uint_least16_t)4
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS2      (uint_least16_t)8

/* Loop through all fields in the message and encode them.
 * If a submessage is encoutered, return to pb_walk().
 */
static pb_walk_retval_t encode_all_fields(pb_encode_ctx_t *ctx, pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;

    if (iter->tag == 0 || iter->message == NULL)
    {
        // End of message or empty message
        return PB_WALK_OUT;
    }

    do {
        if (PB_LTYPE(iter->type) == PB_LTYPE_EXTENSION)
        {
#if !PB_NO_EXTENSIONS
            pb_extension_t* extension = *(pb_extension_t**)iter->pData;
            if (extension)
            {
                // Descend into extension
                return pb_walk_into(state, extension->type, pb_get_extension_data_ptr(extension));
            }
#endif
            continue;
        }

        if (!field_present(iter))
        {
            if (PB_HTYPE(iter->type) == PB_HTYPE_REQUIRED)
            {
                PB_SET_ERROR(ctx, "missing required field");
                return PB_WALK_EXIT_ERR;
            }

            continue;
        }

        if (PB_ATYPE(iter->type) == PB_ATYPE_CALLBACK)
        {
            // Pass control to a callback function provided by user
            if (iter->descriptor->field_callback != NULL)
            {
                if (!iter->descriptor->field_callback(NULL, ctx, iter))
                {
                    PB_SET_ERROR(ctx, "callback error");
                    return PB_WALK_EXIT_ERR;
                }
            }
        }
        else if (PB_LTYPE_IS_SUBMSG(iter->type))
        {
            // Encode submessage prefix tag
            if (!pb_encode_tag_for_field(ctx, iter))
                return PB_WALK_EXIT_ERR;

            // Go into a submessage
            state->flags = PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG;
            return PB_WALK_IN;
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
        {
            pb_size_t count = *(pb_size_t*)iter->pSize;

            if (PB_ATYPE(iter->type) != PB_ATYPE_POINTER && count > iter->array_size)
            {
                PB_SET_ERROR(ctx, "array max size exceeded");
                return PB_WALK_EXIT_ERR;
            }

#if !PB_ENCODE_ARRAYS_UNPACKED
            if (PB_LTYPE(iter->type) <= PB_LTYPE_LAST_PACKABLE)
            {
                // Packed arrays are encoded inside single PB_WT_STRING
                if (!encode_packed_array(ctx, iter, count))
                    return PB_WALK_EXIT_ERR;
            }
            else
#endif
            {
                // Unpacked arrays are encoded as a sequence of fields
                pb_size_t i = 0;
                for (i = 0; i < count; i++)
                {
                    if (!encode_basic_field(ctx, iter))
                        return PB_WALK_EXIT_ERR;

                    iter->pData = (char*)iter->pData + iter->data_size;
                }
            }
        }
        else
        {
            if (!encode_basic_field(ctx, iter))
                return PB_WALK_EXIT_ERR;
        }
    } while (pb_field_iter_next(iter));

    return PB_WALK_OUT;
}

// After a submessage has been encoded on ONEPASS_SIZING mode, we need to patch
// the message size to the memory buffer. One byte is already reserved for it,
// but more can be added using memmove().
static bool update_message_size(pb_encode_ctx_t *ctx, pb_byte_t *msgstart, pb_size_t submsgsize)
{
#if PB_NO_STREAM_CALLBACK
    PB_OPT_ASSERT(msgstart >= ctx->buffer && msgstart + submsgsize <= ctx->buffer + ctx->max_size);
#else
    PB_OPT_ASSERT(msgstart >= ctx->buffer && msgstart + submsgsize <= ctx->buffer + ctx->buffer_size);
#endif

    if (submsgsize < 0x7F)
    {
        // Length fits in the one byte we reserved
        *(msgstart - 1) = (pb_byte_t)submsgsize;
        return true;
    }
    else
    {
        // Message data has to be moved, more space is needed
        pb_byte_t tmpbuf[5];
        uint_fast8_t prefixlen = pb_encode_buffer_varint32(tmpbuf, (uint32_t)submsgsize);

        // First allocate dummy space at end of the stream
        if (!pb_write(ctx, tmpbuf, (pb_size_t)(prefixlen - 1)))
            return false;

        // If memory buffer filled up, it may convert back to sizing, in which case we are done.
        if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
            return true;

        // Move the data around
        memmove(msgstart + prefixlen - 1, msgstart, submsgsize);
        memcpy(msgstart - 1, tmpbuf, prefixlen);
        return true;
    }
}

static pb_walk_retval_t pb_encode_walk_cb(pb_walk_state_t *state)
{
    pb_field_iter_t *iter = &state->iter;
    pb_encode_ctx_t *ctx = (pb_encode_ctx_t *)state->ctx;
    pb_encode_walk_stackframe_t *frame = (pb_encode_walk_stackframe_t*)state->stack;

    // Check the previous action
    if (state->flags & PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG)
    {
        // This is the beginning of a submessage
        // We need to start by calculating the submessage size
        state->flags = 0;

        if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
        {
            // We are inside another submessage which is being sized
            frame->flags |= PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1 |
                            PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_NESTED;
        }
        else
        {
            if (ctx->flags & PB_ENCODE_CTX_FLAG_ONEPASS_SIZING)
            {
                // We are inside another submessage that is being written
                // out and sized in one pass.
                frame->flags |= PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1 |
                                PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_NESTED |
                                PB_ENCODE_WALK_FRAME_FLAG_BYTE_RESERVED;

                pb_byte_t dummy = 0;
                if (!pb_write(ctx, &dummy, 1))
                    return PB_WALK_EXIT_ERR;
            }
#if !PB_NO_STREAM_CALLBACK
            else if (ctx->callback != NULL && ctx->buffer_size < PB_ENCODE_MIN_ONEPASS_BUFFER_SIZE)
            {
                // Buffer is too small for one-pass sizing, do it with two passes.
                // Message data will be encoded after the size is known.
                ctx->flags |= PB_ENCODE_CTX_FLAG_SIZING;
                frame->flags = PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1;
            }
#endif
            else
            {
                // Try to do encoding and size calculation in one pass.
                // One byte is reserved for the message size, and if more
                // is needed, memmove() is used to patch it.
                // For callback streams, if the memory buffer fills up,
                // the stream reverts to normal sizing mode.
                pb_byte_t dummy = 0;
                if (!pb_flush_write_buffer(ctx) ||
                    !pb_write(ctx, &dummy, 1))
                    return PB_WALK_EXIT_ERR;

                ctx->flags |= PB_ENCODE_CTX_FLAG_ONEPASS_SIZING;
                frame->flags |= PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1 |
                                PB_ENCODE_WALK_FRAME_FLAG_BYTE_RESERVED;
            }
        }

        frame->msg_start_pos = ctx->bytes_written;
    }
    else if (state->retval == PB_WALK_OUT)
    {
        // Submessage is done, go to next array item or field
        return PB_WALK_NEXT_ITEM;
    }

    pb_walk_retval_t retval = encode_all_fields(ctx, state);

    if (retval == PB_WALK_OUT &&
        (frame->flags & PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1) != 0)
    {
        // End of sizing pass.
        pb_size_t submsgsize = ctx->bytes_written - frame->msg_start_pos;

        bool nested = (frame->flags & PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_NESTED);
        bool byte_reserved = (frame->flags & PB_ENCODE_WALK_FRAME_FLAG_BYTE_RESERVED);
        bool size_only = (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING);
        bool onepass_size = (ctx->flags & PB_ENCODE_CTX_FLAG_ONEPASS_SIZING);

        if (nested && size_only)
        {
            // This was part of upper level sizing pass,
            // we can just add the size of the length prefix now.
            frame->flags = 0;
            if (!pb_encode_varint32(ctx, (uint32_t)submsgsize))
                return PB_WALK_EXIT_ERR;

            if (byte_reserved)
                ctx->bytes_written--;
        }
        else if (onepass_size && !size_only)
        {
            // Onepass sizing was successful, message data is now in buffer.
#if !PB_NO_STREAM_CALLBACK
            pb_byte_t *msgstart = ctx->buffer + ctx->buffer_count - submsgsize;
#else
            pb_byte_t *msgstart = ctx->buffer + ctx->bytes_written - submsgsize;
#endif

            if (!nested)
            {
                // Onepass sizing ends at this level
                ctx->flags = (pb_encode_ctx_flags_t)(ctx->flags & ~PB_ENCODE_CTX_FLAG_ONEPASS_SIZING);
            }

#if !PB_NO_STREAM_CALLBACK
            if (!nested && ctx->callback != NULL)
            {
                // We can write the size to callback directly.
                pb_byte_t tmpbuf[5];
                uint_fast8_t prefixlen = pb_encode_buffer_varint32(tmpbuf, (uint32_t)submsgsize);

                ctx->buffer_count = 0;

                if (!ctx->callback(ctx, tmpbuf, (size_t)prefixlen) ||
                    !ctx->callback(ctx, msgstart, (size_t)submsgsize))
                {
                    return PB_WALK_EXIT_ERR;
                }
            }
            else
#endif
            {
                // Patch the size to memory buffer
                if (!update_message_size(ctx, msgstart, submsgsize))
                {
                    return PB_WALK_EXIT_ERR;
                }
            }
        }
        else
        {
            // Submessage size is now known.
            // Encode again to write out the message data.
            ctx->flags &= (pb_encode_ctx_flags_t)~(PB_ENCODE_CTX_FLAG_SIZING | PB_ENCODE_CTX_FLAG_ONEPASS_SIZING);
            frame->flags = PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS2;

            // Return the stream to the state it was before the sizing pass started.
            if (byte_reserved)
            {
                ctx->bytes_written = frame->msg_start_pos - 1;

#if !PB_NO_STREAM_CALLBACK
                if (ctx->callback != NULL)
                    ctx->buffer_count = 0; // Buffer was flushed before sizing
                else
                    ctx->buffer_count = ctx->bytes_written;
#endif
            }
            else
            {
                ctx->bytes_written = frame->msg_start_pos;
            }

            if (!pb_encode_varint32(ctx, (uint32_t)submsgsize))
                return PB_WALK_EXIT_ERR;

            // Store expected end position of message
            frame->msg_start_pos = ctx->bytes_written + submsgsize;

            // Do the encoding and write output data
            (void)pb_field_iter_reset(iter);
            retval = encode_all_fields(ctx, state);
        }
    }

    if (retval == PB_WALK_OUT &&
        (frame->flags & PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS2) != 0)
    {
        // End of writeout pass, make sure size matched
        frame->flags = 0;
        if (ctx->bytes_written != frame->msg_start_pos)
        {
            PB_SET_ERROR(ctx, "submsg size changed");
            return PB_WALK_EXIT_ERR;
        }
    }
    
    return retval;
}

bool checkreturn pb_encode_s(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields,
                             const void *src_struct, size_t struct_size)
{
    // Error in struct_size is typically caused by forgetting to rebuild .pb.c file
    // or by it having different compilation options.
    // NOTE: On GCC, sizeof(*(void*)) == 1
    if (fields->struct_size != struct_size && struct_size > 1)
        PB_RETURN_ERROR(ctx, "struct_size mismatch");

    pb_walk_state_t state;
    (void)pb_walk_init(&state, fields, src_struct, pb_encode_walk_cb);

    PB_WALK_DECLARE_STACKBUF(PB_ENCODE_INITIAL_STACKSIZE) stackbuf;
    PB_WALK_SET_STACKBUF(&state, stackbuf);

    if (ctx->flags & PB_ENCODE_CTX_FLAG_DELIMITED)
    {
        // Act as if the top level message is a submessage
        state.flags |= PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG;
    }

    state.ctx = ctx;
    state.next_stacksize = sizeof(pb_encode_walk_stackframe_t);

    // Top level flags shouldn't be applied to any recursive pb_encode() calls from
    // user callbacks, so unset them.
    pb_encode_ctx_flags_t old_flags = ctx->flags;
    ctx->flags &= (pb_encode_ctx_flags_t)~(PB_ENCODE_CTX_FLAG_DELIMITED | PB_ENCODE_CTX_FLAG_NULLTERMINATED);

    // Store pointer to the walk state so that it can be reused by pb_encode_submessage()
    // when called from user callbacks.
    pb_walk_state_t *old_walkstate = ctx->walk_state;
    ctx->walk_state = &state;

    bool status = pb_walk(&state);

    ctx->walk_state = old_walkstate;
    ctx->flags = old_flags;

    if (!status)
    {
        PB_RETURN_ERROR(ctx, state.errmsg);
    }

    if (ctx->flags & PB_ENCODE_CTX_FLAG_NULLTERMINATED)
    {
        const pb_byte_t zero = 0;
        if (!pb_write(ctx, &zero, 1))
            return false;
    }

    return pb_flush_write_buffer(ctx);
}

bool pb_get_encoded_size_s(size_t *size, const pb_msgdesc_t *fields,
                const void *src_struct, size_t struct_size)
{
    pb_encode_ctx_t ctx;
    pb_init_encode_ctx_sizing(&ctx);
    
    if (!pb_encode_s(&ctx, fields, src_struct, struct_size))
        return false;
    
    *size = (size_t)ctx.bytes_written;
    return true;
}

/********************
 * Helper functions *
 ********************/

// Encode unsigned varint of max. 32 bits to buffer that has at least 5 bytes space.
// Returns number of bytes written.
static uint_fast8_t pb_encode_buffer_varint32(pb_byte_t *buffer, uint32_t value)
{
    uint_fast8_t len = 0;

    if (value <= 0x7F)
    {
        buffer[0] = (pb_byte_t)value;
        return 1;
    }

    do
    {
        buffer[len++] = (pb_byte_t)((value & 0x7F) | 0x80);
        value >>= 7;
    } while (value > 0x7F);

    buffer[len++] = (pb_byte_t)value;

    return len;
}

static inline bool checkreturn pb_encode_varint32(pb_encode_ctx_t *ctx, uint32_t value)
{
    pb_byte_t tmpbuf[5];
    pb_byte_t *buffer = pb_bufwrite_start(ctx, 5);
    if (!buffer) buffer = tmpbuf;

    uint_fast8_t len = pb_encode_buffer_varint32(buffer, value);

    if (buffer == tmpbuf)
    {
        return pb_write(ctx, buffer, len);
    }
    else
    {
        pb_bufwrite_done(ctx, len);
        return true;
    }
}

bool checkreturn pb_encode_varint(pb_encode_ctx_t *ctx, pb_uint64_t value)
{
    // Write either directly to stream or to tmpbuf
    pb_byte_t tmpbuf[10];
    pb_byte_t *buffer = pb_bufwrite_start(ctx, 10);
    if (!buffer) buffer = tmpbuf;

    uint_fast8_t len = 0;

    do
    {
        uint32_t lowval = (uint32_t)value & 0xFFFFFFF; // 28 lowest bits
        value >>= 28;

        if (value != 0)
        {
            // Encode 28 bits and force the last byte to be a continuation
            lowval |= ((uint32_t)1 << 29);
            len = (uint_fast8_t)(len + pb_encode_buffer_varint32(buffer + len, lowval) - 1);
        }
        else
        {
            // Encode last max. 28 bits of the varint
            len = (uint_fast8_t)(len + pb_encode_buffer_varint32(buffer + len, lowval));
        }
    } while (value != 0);

    if (buffer == tmpbuf)
    {
        return pb_write(ctx, buffer, len);
    }
    else
    {
        pb_bufwrite_done(ctx, len);
        return true;
    }
}

#if PB_WITHOUT_64BIT
// When 64-bit datatypes are not available, negative int32_t values still
// need to be encoded as-if they were 64-bit.
static bool checkreturn pb_encode_negative_varint(pb_encode_ctx_t *ctx, int32_t value)
{
    pb_byte_t tmpbuf[10];
    pb_byte_t *buffer = pb_bufwrite_start(ctx, 10);
    if (!buffer) buffer = tmpbuf;

    PB_OPT_ASSERT(value < 0);
    pb_encode_buffer_varint32(buffer, (uint32_t)value);
    buffer[4] |= 0xF8;
    buffer[5] = 0xFF;
    buffer[6] = 0xFF;
    buffer[7] = 0xFF;
    buffer[8] = 0xFF;
    buffer[9] = 0x01;

    if (buffer == tmpbuf)
    {
        return pb_write(ctx, buffer, 10);
    }
    else
    {
        pb_bufwrite_done(ctx, 10);
        return true;
    }
}
#endif

bool checkreturn pb_encode_svarint(pb_encode_ctx_t *ctx, pb_int64_t value)
{
    pb_uint64_t zigzagged;
    pb_uint64_t mask = ((pb_uint64_t)-1) >> 1; /* Satisfy clang -fsanitize=integer */
    if (value < 0)
        zigzagged = ~(((pb_uint64_t)value & mask) << 1);
    else
        zigzagged = (pb_uint64_t)value << 1;
    
    return pb_encode_varint(ctx, zigzagged);
}

bool checkreturn pb_encode_fixed32(pb_encode_ctx_t *ctx, const void *value)
{
#if PB_LITTLE_ENDIAN_8BIT
    /* Fast path if we know that we're on little endian */
    return pb_write(ctx, (const pb_byte_t*)value, 4);
#else
    uint32_t val = *(const uint32_t*)value;
    pb_byte_t bytes[4];
    bytes[0] = (pb_byte_t)(val & 0xFF);
    bytes[1] = (pb_byte_t)((val >> 8) & 0xFF);
    bytes[2] = (pb_byte_t)((val >> 16) & 0xFF);
    bytes[3] = (pb_byte_t)((val >> 24) & 0xFF);
    return pb_write(ctx, bytes, 4);
#endif
}

#if !PB_WITHOUT_64BIT
bool checkreturn pb_encode_fixed64(pb_encode_ctx_t *ctx, const void *value)
{
#if PB_LITTLE_ENDIAN_8BIT
    /* Fast path if we know that we're on little endian */
    return pb_write(ctx, (const pb_byte_t*)value, 8);
#else
    uint64_t val = *(const uint64_t*)value;
    pb_byte_t bytes[8];
    bytes[0] = (pb_byte_t)(val & 0xFF);
    bytes[1] = (pb_byte_t)((val >> 8) & 0xFF);
    bytes[2] = (pb_byte_t)((val >> 16) & 0xFF);
    bytes[3] = (pb_byte_t)((val >> 24) & 0xFF);
    bytes[4] = (pb_byte_t)((val >> 32) & 0xFF);
    bytes[5] = (pb_byte_t)((val >> 40) & 0xFF);
    bytes[6] = (pb_byte_t)((val >> 48) & 0xFF);
    bytes[7] = (pb_byte_t)((val >> 56) & 0xFF);
    return pb_write(ctx, bytes, 8);
#endif
}
#endif

bool checkreturn pb_encode_tag(pb_encode_ctx_t *ctx, pb_wire_type_t wiretype, pb_tag_t field_number)
{
    // pb_tag_t is either 16- or 32-bit
    // Field number is either 29-bit or 12-bit, depending on PB_NO_LARGEMSG.
    return pb_encode_varint32(ctx, ((pb_tag_t)(field_number << 3) | wiretype));
}

bool pb_encode_tag_for_field(pb_encode_ctx_t* ctx, const pb_field_iter_t* field)
{
    pb_tag_t tag = (pb_tag_t)(field->tag << 3);

    switch (PB_LTYPE(field->type))
    {
        case PB_LTYPE_BOOL:
        case PB_LTYPE_VARINT:
        case PB_LTYPE_UVARINT:
        case PB_LTYPE_SVARINT:
            tag |= (pb_tag_t)PB_WT_VARINT;
            break;

        case PB_LTYPE_FIXED32:
            tag |= (pb_tag_t)PB_WT_32BIT;
            break;

        case PB_LTYPE_FIXED64:
            tag |= (pb_tag_t)PB_WT_64BIT;
            break;

        case PB_LTYPE_BYTES:
        case PB_LTYPE_STRING:
        case PB_LTYPE_SUBMESSAGE:
        case PB_LTYPE_SUBMSG_W_CB:
        case PB_LTYPE_FIXED_LENGTH_BYTES:
            tag |= (pb_tag_t)PB_WT_STRING;
            break;

        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }

    return pb_encode_varint32(ctx, tag);
}

bool checkreturn pb_encode_string(pb_encode_ctx_t *ctx, const pb_byte_t *buffer, pb_size_t size)
{
    if (!pb_encode_varint32(ctx, (uint32_t)size))
        return false;
    
    return pb_write(ctx, buffer, size);
}

bool checkreturn pb_encode_submessage(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields, const void *src_struct)
{
    bool status;

    if (ctx->walk_state != NULL &&
        ctx->walk_state->callback == pb_encode_walk_cb &&
        ctx->walk_state->ctx == ctx)
    {
        // Reuse existing walk state variable to conserve stack space
        pb_walk_state_t *state = ctx->walk_state;
        uint32_t old_flags = state->flags;
        state->flags = PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG;
        state->iter.pData = PB_CONST_CAST(src_struct);
        state->iter.submsg_desc = fields;
        state->retval = PB_WALK_IN;
        state->depth += 1;
        state->next_stacksize = sizeof(pb_encode_walk_stackframe_t);

        status = pb_walk(state);

        state->flags = old_flags;
        state->depth -= 1;

        if (!status)
        {
            PB_SET_ERROR(ctx, state->errmsg);
        }
    }
    else
    {
        // Go through normal pb_encode() to allocate a new walk state
        pb_encode_ctx_flags_t old_flags = ctx->flags;
        ctx->flags |= PB_ENCODE_CTX_FLAG_DELIMITED;
        status = pb_encode_s(ctx, fields, src_struct, 0);
        ctx->flags = old_flags;
    }

    return status;
}

/* Field encoders */

static bool checkreturn pb_enc_varint(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    if (PB_LTYPE(field->type) == PB_LTYPE_UVARINT)
    {
        /* Perform unsigned integer extension */
        pb_uint64_t value = 0;

        if (field->data_size == sizeof(uint_least8_t))
            value = *(const uint_least8_t*)field->pData;
        else if (field->data_size == sizeof(uint_least16_t))
            value = *(const uint_least16_t*)field->pData;
        else if (field->data_size == sizeof(uint32_t))
            value = *(const uint32_t*)field->pData;
        else if (field->data_size == sizeof(pb_uint64_t))
            value = *(const pb_uint64_t*)field->pData;
        else
            PB_RETURN_ERROR(ctx, "invalid data_size");

        return pb_encode_varint(ctx, value);
    }
    else
    {
        /* Perform signed integer extension */
        pb_int64_t value = 0;

        if (field->data_size == sizeof(int_least8_t))
            value = *(const int_least8_t*)field->pData;
        else if (field->data_size == sizeof(int_least16_t))
            value = *(const int_least16_t*)field->pData;
        else if (field->data_size == sizeof(int32_t))
            value = *(const int32_t*)field->pData;
        else if (field->data_size == sizeof(pb_int64_t))
            value = *(const pb_int64_t*)field->pData;
        else
            PB_RETURN_ERROR(ctx, "invalid data_size");

        if (PB_LTYPE(field->type) == PB_LTYPE_SVARINT)
            return pb_encode_svarint(ctx, value);
#if PB_WITHOUT_64BIT
        else if (value < 0)
            return pb_encode_negative_varint(ctx, value);
#endif
        else
            return pb_encode_varint(ctx, (pb_uint64_t)value);

    }
}

static bool checkreturn pb_enc_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        PB_OPT_ASSERT(field->data_size == sizeof(pb_bytes_t));
        const pb_bytes_t *bytes = (const pb_bytes_t*)field->pData;

        if (bytes->bytes == NULL)
        {
            /* Treat null pointer as an empty bytes field */
            return pb_encode_string(ctx, NULL, 0);
        }

        return pb_encode_string(ctx, bytes->bytes, bytes->size);
    }
    else
    {
        const pb_bytes_array_t *bytes = (const pb_bytes_array_t*)field->pData;

        if (bytes->size > field->data_size - offsetof(pb_bytes_array_t, bytes))
        {
            PB_RETURN_ERROR(ctx, "bytes size exceeded");
        }
    
        return pb_encode_string(ctx, bytes->bytes, bytes->size);
    }
}

static bool checkreturn pb_enc_string(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    pb_size_t size = 0;
    pb_size_t max_size = (pb_size_t)field->data_size;
    const char *str = NULL;
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        max_size = PB_SIZE_MAX;

        if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
        {
            // In pointer-type string arrays, the array entries are themselves
            // pointers. Therefore we have to dereference twice.
            str = *(const char**)field->pData;
        }
        else
        {
            str = (const char*)field->pData;
        }
    }
    else
    {
        /* pb_dec_string() assumes string fields end with a null
         * terminator when the type isn't PB_ATYPE_POINTER, so we
         * shouldn't allow more than max-1 bytes to be written to
         * allow space for the null terminator.
         */
        if (max_size == 0)
            PB_RETURN_ERROR(ctx, "zero-length string");

        max_size -= 1;
        str = (const char*)field->pData;
    }

    if (str == NULL)
    {
        size = 0; /* Treat null pointer as an empty string */
    }
    else
    {
        const char *p = str;

        /* strnlen() is not always available, so just use a loop */
        while (size < max_size && *p != '\0')
        {
            size++;
            p++;
        }

        if (*p != '\0')
        {
            PB_RETURN_ERROR(ctx, "unterminated string");
        }
    }

#if !PB_NO_VALIDATE_UTF8
    if (str != NULL &&
        (ctx->flags & PB_ENCODE_CTX_FLAG_NO_VALIDATE_UTF8) == 0 &&
        !pb_validate_utf8(str))
    {
        PB_RETURN_ERROR(ctx, "invalid utf8");
    }
#endif

    return pb_encode_string(ctx, (const pb_byte_t*)str, size);
}

#if PB_CONVERT_DOUBLE_FLOAT
bool pb_encode_float_as_double(pb_encode_ctx_t *ctx, float value)
{
    union { float f; uint32_t i; } in;
    uint_least8_t sign;
    int exponent;
    uint64_t mantissa;

    in.f = value;

    /* Decompose input value */
    sign = (uint_least8_t)((in.i >> 31) & 1);
    exponent = (int)((in.i >> 23) & 0xFF) - 127;
    mantissa = in.i & 0x7FFFFF;

    if (exponent == 128)
    {
        /* Special value (NaN etc.) */
        exponent = 1024;
    }
    else if (exponent == -127)
    {
        if (!mantissa)
        {
            /* Zero */
            exponent = -1023;
        }
        else
        {
            /* Denormalized */
            mantissa <<= 1;
            while (!(mantissa & 0x800000))
            {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x7FFFFF;
        }
    }

    /* Combine fields */
    mantissa <<= 29;
    mantissa |= (uint64_t)(exponent + 1023) << 52;
    mantissa |= (uint64_t)sign << 63;

    return pb_encode_fixed64(ctx, &mantissa);
}
#endif
