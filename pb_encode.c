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
static bool checkreturn buf_write(pb_encode_ctx_t *ctx, const pb_byte_t *buf, size_t count);
static bool checkreturn encode_array(pb_encode_ctx_t *ctx, pb_field_iter_t *field);
static bool checkreturn pb_check_proto3_default_value(const pb_field_iter_t *field);
static bool checkreturn encode_basic_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn encode_callback_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
// static pb_noinline bool checkreturn encode_extension_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
// static bool checkreturn default_extension_encoder(pb_encode_ctx_t *ctx, const pb_extension_t *extension);
static bool checkreturn pb_encode_varint_32(pb_encode_ctx_t *ctx, uint32_t low, uint32_t high);
static bool checkreturn pb_enc_bool(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_varint(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_fixed(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_string(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_enc_fixed_length_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field);

#ifdef PB_WITHOUT_64BIT
#define pb_int64_t int32_t
#define pb_uint64_t uint32_t
#else
#define pb_int64_t int64_t
#define pb_uint64_t uint64_t
#endif

/*******************************
 * pb_ostream_t implementation *
 *******************************/

static bool checkreturn buf_write(pb_encode_ctx_t *ctx, const pb_byte_t *buf, size_t count)
{
    pb_byte_t *dest = (pb_byte_t*)ctx->state;
    ctx->state = dest + count;
    
    memcpy(dest, buf, count * sizeof(pb_byte_t));
    
    return true;
}

void pb_init_encode_ctx_for_buffer(pb_encode_ctx_t *ctx, pb_byte_t *buf, size_t bufsize)
{
    ctx->flags = 0;
#ifdef PB_BUFFER_ONLY
    /* In PB_BUFFER_ONLY configuration the callback pointer is just int*.
     * NULL pointer marks a sizing field, so put a non-NULL value to mark a buffer stream.
     */
    static const int marker = 0;
    ctx->callback = &marker;
#else
    ctx->callback = &buf_write;
#endif
    ctx->state = buf;
    ctx->max_size = bufsize;
    ctx->bytes_written = 0;
#ifndef PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif
}

void pb_init_encode_ctx_sizing(pb_encode_ctx_t *ctx)
{
    ctx->flags = PB_ENCODE_CTX_FLAG_SIZING;
    ctx->callback = 0;
    ctx->max_size = PB_SIZE_MAX;
    ctx->bytes_written = 0;
#ifndef PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif
}

bool checkreturn pb_write(pb_encode_ctx_t *ctx, const pb_byte_t *buf, size_t count)
{
    if (count > 0 && ctx->callback != NULL &&
        (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING) == 0)
    {
        if (ctx->bytes_written + count < ctx->bytes_written ||
            ctx->bytes_written + count > ctx->max_size)
        {
            PB_RETURN_ERROR(ctx, "stream full");
        }

#ifdef PB_BUFFER_ONLY
        if (!buf_write(ctx, buf, count))
            PB_RETURN_ERROR(ctx, "io error");
#else        
        if (!ctx->callback(ctx, buf, count))
            PB_RETURN_ERROR(ctx, "io error");
#endif
    }
    
    ctx->bytes_written += count;
    return true;
}

/*************************
 * Encode a single field *
 *************************/

/* Read a bool value without causing undefined behavior even if the value
 * is invalid. See issue #434 and
 * https://stackoverflow.com/questions/27661768/weird-results-for-conditional
 */
static bool safe_read_bool(const void *pSize)
{
    const char *p = (const char *)pSize;
    size_t i;
    for (i = 0; i < sizeof(bool); i++)
    {
        if (p[i] != 0)
            return true;
    }
    return false;
}

/* Encode a static array. Handles the size calculations and possible packing. */
static bool checkreturn encode_array(pb_encode_ctx_t *ctx, pb_field_iter_t *field)
{
    pb_size_t i;
    pb_size_t count;
#ifndef PB_ENCODE_ARRAYS_UNPACKED
    size_t size;
#endif

    count = *(pb_size_t*)field->pSize;

    if (count == 0)
        return true;

    if (PB_ATYPE(field->type) != PB_ATYPE_POINTER && count > field->array_size)
        PB_RETURN_ERROR(ctx, "array max size exceeded");
    
#ifndef PB_ENCODE_ARRAYS_UNPACKED
    /* We always pack arrays if the datatype allows it. */
    if (PB_LTYPE(field->type) <= PB_LTYPE_LAST_PACKABLE)
    {
        if (!pb_encode_tag(ctx, PB_WT_STRING, field->tag))
            return false;
        
        /* Determine the total size of packed array. */
        if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32)
        {
            size = 4 * (size_t)count;
        }
        else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
        {
            size = 8 * (size_t)count;
        }
        else
        {
            pb_encode_ctx_flags_t flags_orig = ctx->flags;
            size_t size_orig = ctx->bytes_written;
            ctx->flags |= PB_ENCODE_CTX_FLAG_SIZING;

            void *pData_orig = field->pData;
            for (i = 0; i < count; i++)
            {
                if (!pb_enc_varint(ctx, field))
                    return false;
                field->pData = (char*)field->pData + field->data_size;
            }
            field->pData = pData_orig;
            size = ctx->bytes_written - size_orig;
            ctx->bytes_written = size_orig;
            ctx->flags = flags_orig;
        }
        
        if (!pb_encode_varint(ctx, (pb_uint64_t)size))
            return false;
        
        if (ctx->callback == NULL)
            return pb_write(ctx, NULL, size); /* Just sizing.. */
        
        /* Write the data */
        for (i = 0; i < count; i++)
        {
            if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32 || PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
            {
                if (!pb_enc_fixed(ctx, field))
                    return false;
            }
            else
            {
                if (!pb_enc_varint(ctx, field))
                    return false;
            }

            field->pData = (char*)field->pData + field->data_size;
        }
    }
    else /* Unpacked fields */
#endif
    {
        for (i = 0; i < count; i++)
        {
            /* Normally the data is stored directly in the array entries, but
             * for pointer-type string and bytes fields, the array entries are
             * actually pointers themselves also. So we have to dereference once
             * more to get to the actual data. */
            if (PB_ATYPE(field->type) == PB_ATYPE_POINTER &&
                (PB_LTYPE(field->type) == PB_LTYPE_STRING ||
                 PB_LTYPE(field->type) == PB_LTYPE_BYTES))
            {
                bool status;
                void *pData_orig = field->pData;
                field->pData = *(void* const*)field->pData;

                if (!field->pData)
                {
                    /* Null pointer in array is treated as empty string / bytes */
                    status = pb_encode_tag_for_field(ctx, field) &&
                             pb_encode_varint(ctx, 0);
                }
                else
                {
                    status = encode_basic_field(ctx, field);
                }

                field->pData = pData_orig;

                if (!status)
                    return false;
            }
            else
            {
                if (!encode_basic_field(ctx, field))
                    return false;
            }
            field->pData = (char*)field->pData + field->data_size;
        }
    }
    
    return true;
}

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
    else if (PB_ATYPE(type) == PB_ATYPE_POINTER)
    {
        return field->pData == NULL;
    }
    else if (PB_ATYPE(type) == PB_ATYPE_CALLBACK)
    {
        if (PB_LTYPE(type) == PB_LTYPE_EXTENSION)
        {
            const pb_extension_t *extension = *(const pb_extension_t* const *)field->pData;
            return extension == NULL;
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
            return pb_enc_bool(ctx, field);

        case PB_LTYPE_VARINT:
        case PB_LTYPE_UVARINT:
        case PB_LTYPE_SVARINT:
            return pb_enc_varint(ctx, field);

        case PB_LTYPE_FIXED32:
        case PB_LTYPE_FIXED64:
            return pb_enc_fixed(ctx, field);

        case PB_LTYPE_BYTES:
            return pb_enc_bytes(ctx, field);

        case PB_LTYPE_STRING:
            return pb_enc_string(ctx, field);

        case PB_LTYPE_FIXED_LENGTH_BYTES:
            return pb_enc_fixed_length_bytes(ctx, field);

        case PB_LTYPE_SUBMESSAGE:
        case PB_LTYPE_SUBMSG_W_CB:
        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
}

/* Encode a field with callback semantics. This means that a user function is
 * called to provide and encode the actual data. */
static bool checkreturn encode_callback_field(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    if (field->descriptor->field_callback != NULL)
    {
        if (!field->descriptor->field_callback(NULL, ctx, field))
            PB_RETURN_ERROR(ctx, "callback error");
    }
    return true;
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

typedef struct {
    size_t msg_start_pos;
    uint_least16_t flags;
} pb_encode_walk_stackframe_t;

#define PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG      (uint_least16_t)1
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1         (uint_least16_t)1
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_SIZE_ONLY  (uint_least16_t)2
#define PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS2      (uint_least16_t)4

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
            pb_extension_t* extension = *(pb_extension_t**)iter->pData;
            if (extension)
            {
                // Descend into extension
                iter->submsg_desc = extension->type;
                iter->pData = pb_get_extension_data_ptr(extension);
                return PB_WALK_IN;
            }
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

        if (PB_LTYPE_IS_SUBMSG(iter->type) && PB_ATYPE(iter->type) != PB_ATYPE_CALLBACK)
        {
            // Encode submessage prefix tag
            if (!pb_encode_tag_for_field(ctx, iter))
                return PB_WALK_EXIT_ERR;

            // Go into a submessage
            state->flags = PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG;
            return PB_WALK_IN;
        }

        if (PB_ATYPE(iter->type) == PB_ATYPE_CALLBACK)
        {
            if (!encode_callback_field(ctx, iter))
                return PB_WALK_EXIT_ERR;
        }
        else if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
        {
            if (!encode_array(ctx, iter))
                return PB_WALK_EXIT_ERR;
        }
        else
        {
            if (!encode_basic_field(ctx, iter))
                return PB_WALK_EXIT_ERR;
        }
    } while (pb_field_iter_next(iter));

    return PB_WALK_OUT;
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
        frame->msg_start_pos = ctx->bytes_written;

        if (ctx->flags & PB_ENCODE_CTX_FLAG_SIZING)
        {
            // We are inside another submessage which is being sized
            frame->flags |= PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1 |
                            PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_SIZE_ONLY;
        }
        else
        {
            // Start sizing the submessage
            frame->flags = PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS1;
            ctx->flags |= PB_ENCODE_CTX_FLAG_SIZING;
        }
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
        size_t submsgsize = ctx->bytes_written - frame->msg_start_pos;

        if (frame->flags & PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_SIZE_ONLY)
        {
            // This was part of upper level sizing pass,
            // we can just add the size of the length prefix now.
            frame->flags = 0;
            if (!pb_encode_varint(ctx, (pb_uint64_t)submsgsize))
                return PB_WALK_EXIT_ERR;
        }
        else
        {
            // We started this sizing pass, we can now write out
            // the actual message data.
            ctx->flags = (pb_encode_ctx_flags_t)(ctx->flags & ~PB_ENCODE_CTX_FLAG_SIZING);
            ctx->bytes_written = frame->msg_start_pos;
            frame->flags = PB_ENCODE_WALK_FRAME_FLAG_SUBMSG_PASS2;

            if (!pb_encode_varint(ctx, (pb_uint64_t)submsgsize))
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

    state.ctx = ctx;
    state.next_stacksize = sizeof(pb_encode_walk_stackframe_t);

    if (ctx->flags & PB_ENCODE_CTX_FLAG_DELIMITED)
    {
        // Act as if the top level message is a submessage
        state.flags |= PB_ENCODE_WALK_STATE_FLAG_START_SUBMSG;
    }

    if (!pb_walk(&state))
    {
        PB_RETURN_ERROR(ctx, state.errmsg);
    }

    if (ctx->flags & PB_ENCODE_CTX_FLAG_NULLTERMINATED)
    {
        const pb_byte_t zero = 0;
        if (!pb_write(ctx, &zero, 1))
            return false;
    }

    return true;
}

bool pb_get_encoded_size_s(size_t *size, const pb_msgdesc_t *fields,
                const void *src_struct, size_t struct_size)
{
    pb_encode_ctx_t ctx;
    pb_init_encode_ctx_sizing(&ctx);
    
    if (!pb_encode_s(&ctx, fields, src_struct, struct_size))
        return false;
    
    *size = ctx.bytes_written;
    return true;
}

/********************
 * Helper functions *
 ********************/

/* This function avoids 64-bit shifts as they are quite slow on many platforms. */
static bool checkreturn pb_encode_varint_32(pb_encode_ctx_t *ctx, uint32_t low, uint32_t high)
{
    size_t i = 0;
    pb_byte_t buffer[10];
    pb_byte_t byte = (pb_byte_t)(low & 0x7F);
    low >>= 7;

    while (i < 4 && (low != 0 || high != 0))
    {
        byte |= 0x80;
        buffer[i++] = byte;
        byte = (pb_byte_t)(low & 0x7F);
        low >>= 7;
    }

    if (high)
    {
        byte = (pb_byte_t)(byte | ((high & 0x07) << 4));
        high >>= 3;

        while (high)
        {
            byte |= 0x80;
            buffer[i++] = byte;
            byte = (pb_byte_t)(high & 0x7F);
            high >>= 7;
        }
    }

    buffer[i++] = byte;

    return pb_write(ctx, buffer, i);
}

bool checkreturn pb_encode_varint(pb_encode_ctx_t *ctx, pb_uint64_t value)
{
    if (value <= 0x7F)
    {
        /* Fast path: single byte */
        pb_byte_t byte = (pb_byte_t)value;
        return pb_write(ctx, &byte, 1);
    }
    else
    {
#ifdef PB_WITHOUT_64BIT
        return pb_encode_varint_32(ctx, value, 0);
#else
        return pb_encode_varint_32(ctx, (uint32_t)value, (uint32_t)(value >> 32));
#endif
    }
}

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
#if defined(PB_LITTLE_ENDIAN_8BIT) && PB_LITTLE_ENDIAN_8BIT == 1
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

#ifndef PB_WITHOUT_64BIT
bool checkreturn pb_encode_fixed64(pb_encode_ctx_t *ctx, const void *value)
{
#if defined(PB_LITTLE_ENDIAN_8BIT) && PB_LITTLE_ENDIAN_8BIT == 1
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
    pb_uint64_t tag = ((pb_uint64_t)field_number << 3) | wiretype;
    return pb_encode_varint(ctx, tag);
}

bool pb_encode_tag_for_field(pb_encode_ctx_t* ctx, const pb_field_iter_t* field)
{
    pb_wire_type_t wiretype;
    switch (PB_LTYPE(field->type))
    {
        case PB_LTYPE_BOOL:
        case PB_LTYPE_VARINT:
        case PB_LTYPE_UVARINT:
        case PB_LTYPE_SVARINT:
            wiretype = PB_WT_VARINT;
            break;
        
        case PB_LTYPE_FIXED32:
            wiretype = PB_WT_32BIT;
            break;
        
        case PB_LTYPE_FIXED64:
            wiretype = PB_WT_64BIT;
            break;
        
        case PB_LTYPE_BYTES:
        case PB_LTYPE_STRING:
        case PB_LTYPE_SUBMESSAGE:
        case PB_LTYPE_SUBMSG_W_CB:
        case PB_LTYPE_FIXED_LENGTH_BYTES:
            wiretype = PB_WT_STRING;
            break;
        
        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
    
    return pb_encode_tag(ctx, wiretype, field->tag);
}

bool checkreturn pb_encode_string(pb_encode_ctx_t *ctx, const pb_byte_t *buffer, size_t size)
{
    if (!pb_encode_varint(ctx, (pb_uint64_t)size))
        return false;
    
    return pb_write(ctx, buffer, size);
}

bool checkreturn pb_encode_submessage(pb_encode_ctx_t *ctx, const pb_msgdesc_t *fields, const void *src_struct)
{
    pb_encode_ctx_flags_t old_flags = ctx->flags;
    ctx->flags |= PB_ENCODE_CTX_FLAG_DELIMITED;
    bool status = pb_encode_s(ctx, fields, src_struct, 0);
    ctx->flags = old_flags;
    return status;
}

/* Field encoders */

static bool checkreturn pb_enc_bool(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t value = safe_read_bool(field->pData) ? 1 : 0;
    PB_UNUSED(field);
    return pb_encode_varint(ctx, value);
}

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
#ifdef PB_WITHOUT_64BIT
        else if (value < 0)
            return pb_encode_varint_32(ctx, (uint32_t)value, (uint32_t)-1);
#endif
        else
            return pb_encode_varint(ctx, (pb_uint64_t)value);

    }
}

static bool checkreturn pb_enc_fixed(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
#ifdef PB_CONVERT_DOUBLE_FLOAT
    if (field->data_size == sizeof(float) && PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
    {
        return pb_encode_float_as_double(ctx, *(float*)field->pData);
    }
#endif

    if (field->data_size == sizeof(uint32_t))
    {
        return pb_encode_fixed32(ctx, field->pData);
    }
#ifndef PB_WITHOUT_64BIT
    else if (field->data_size == sizeof(uint64_t))
    {
        return pb_encode_fixed64(ctx, field->pData);
    }
#endif
    else
    {
        PB_RETURN_ERROR(ctx, "invalid data_size");
    }
}

static bool checkreturn pb_enc_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    const pb_bytes_array_t *bytes = NULL;

    bytes = (const pb_bytes_array_t*)field->pData;
    
    if (bytes == NULL)
    {
        /* Treat null pointer as an empty bytes field */
        return pb_encode_string(ctx, NULL, 0);
    }
    
    if (PB_ATYPE(field->type) == PB_ATYPE_STATIC &&
        bytes->size > field->data_size - offsetof(pb_bytes_array_t, bytes))
    {
        PB_RETURN_ERROR(ctx, "bytes size exceeded");
    }
    
    return pb_encode_string(ctx, bytes->bytes, (size_t)bytes->size);
}

static bool checkreturn pb_enc_string(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    size_t size = 0;
    size_t max_size = (size_t)field->data_size;
    const char *str = (const char*)field->pData;
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        max_size = (size_t)-1;
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

#ifdef PB_VALIDATE_UTF8
    if (!pb_validate_utf8(str))
        PB_RETURN_ERROR(ctx, "invalid utf8");
#endif

    return pb_encode_string(ctx, (const pb_byte_t*)str, size);
}

static bool checkreturn pb_enc_fixed_length_bytes(pb_encode_ctx_t *ctx, const pb_field_iter_t *field)
{
    return pb_encode_string(ctx, (const pb_byte_t*)field->pData, (size_t)field->data_size);
}

#ifdef PB_CONVERT_DOUBLE_FLOAT
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
