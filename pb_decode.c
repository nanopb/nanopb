/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

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

#include "pb.h"
#include "pb_decode.h"
#include "pb_common.h"

/**************************************
 * Declarations internal to this file *
 **************************************/

static bool checkreturn buf_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, size_t count);
static bool checkreturn read_raw_value(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_byte_t *buf, size_t *size);
static bool checkreturn decode_basic_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_static_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_pointer_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_callback_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn decode_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field);
static bool checkreturn pb_dec_bool(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_varint(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_string(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_dec_fixed_length_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field);
static bool checkreturn pb_skip_varint(pb_decode_ctx_t *ctx);
static bool checkreturn pb_skip_string(pb_decode_ctx_t *ctx);

#ifdef PB_ENABLE_MALLOC
static bool checkreturn allocate_field(pb_decode_ctx_t *ctx, void *pData, size_t data_size, size_t array_size);
static void initialize_pointer_field(void *pItem, pb_field_iter_t *field);
static void pb_release_single_field(pb_decode_ctx_t *ctx, pb_field_iter_t *field);
static pb_walk_retval_t pb_release_walk_cb(pb_walk_state_t *state);
#endif

#ifdef PB_WITHOUT_64BIT
#define pb_int64_t int32_t
#define pb_uint64_t uint32_t
#else
#define pb_int64_t int64_t
#define pb_uint64_t uint64_t
#endif

typedef struct {
    uint32_t bitfield[(PB_MAX_REQUIRED_FIELDS + 31) / 32];
} pb_fields_seen_t;

/*******************************
 * pb_istream_t implementation *
 *******************************/

static bool checkreturn buf_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, size_t count)
{
    const pb_byte_t *source = (const pb_byte_t*)ctx->state;
    ctx->state = (pb_byte_t*)ctx->state + count;
    
    if (buf != NULL)
    {
        memcpy(buf, source, count * sizeof(pb_byte_t));
    }
    
    return true;
}

bool checkreturn pb_read(pb_decode_ctx_t *ctx, pb_byte_t *buf, size_t count)
{
    if (count == 0)
        return true;

#ifndef PB_BUFFER_ONLY
	if (buf == NULL && ctx->callback != buf_read)
	{
		/* Skip input bytes */
		pb_byte_t tmp[16];
		while (count > 16)
		{
			if (!pb_read(ctx, tmp, 16))
				return false;
			
			count -= 16;
		}
		
		return pb_read(ctx, tmp, count);
	}
#endif

    if (ctx->bytes_left < count)
        PB_RETURN_ERROR(ctx, "end-of-stream");
    
#ifndef PB_BUFFER_ONLY
    if (!ctx->callback(ctx, buf, count))
        PB_RETURN_ERROR(ctx, "io error");
#else
    if (!buf_read(ctx, buf, count))
        return false;
#endif
    
    if (ctx->bytes_left < count)
        ctx->bytes_left = 0;
    else
        ctx->bytes_left -= count;

    return true;
}

/* Read a single byte from input stream. buf may not be NULL.
 * This is an optimization for the varint decoding. */
static bool checkreturn pb_readbyte(pb_decode_ctx_t *ctx, pb_byte_t *buf)
{
    if (ctx->bytes_left == 0)
        PB_RETURN_ERROR(ctx, "end-of-stream");

#ifndef PB_BUFFER_ONLY
    if (!ctx->callback(ctx, buf, 1))
        PB_RETURN_ERROR(ctx, "io error");
#else
    *buf = *(const pb_byte_t*)ctx->state;
    ctx->state = (pb_byte_t*)ctx->state + 1;
#endif

    ctx->bytes_left--;
    
    return true;    
}

bool pb_init_decode_ctx_for_buffer(pb_decode_ctx_t *ctx, const pb_byte_t *buf, size_t msglen)
{
    /* Cast away the const from buf without a compiler error.  We are
     * careful to use it only in a const manner in the callbacks.
     */
    union {
        void *state;
        const void *c_state;
    } state;
#ifdef PB_BUFFER_ONLY
    ctx->callback = NULL;
#else
    ctx->callback = &buf_read;
#endif
    state.c_state = buf;
    ctx->state = state.state;
    ctx->bytes_left = msglen;
#ifndef PB_NO_ERRMSG
    ctx->errmsg = NULL;
#endif
    ctx->flags = 0;
    return true;
}


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
        } while (byte & 0x80);
   }
   
   *dest = result;
   return true;
}

#ifndef PB_WITHOUT_64BIT
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
    } while (byte & 0x80);
    
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
    } while (byte & 0x80);
    return true;
}

bool checkreturn pb_skip_string(pb_decode_ctx_t *ctx)
{
    uint32_t length;
    if (!pb_decode_varint32(ctx, &length))
        return false;
    
    if ((size_t)length != length)
    {
        PB_RETURN_ERROR(ctx, "size too large");
    }

    return pb_read(ctx, NULL, (size_t)length);
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
#ifndef PB_BUFFER_ONLY
        /* Workaround for issue #1017
         *
         * Callback streams don't set bytes_left to 0 on eof until after being called by pb_decode_varint32,
         * which results in "io error" being raised. This contrasts the behavior of buffer streams who raise
         * no error on eof as bytes_left is already 0 on entry. This causes legitimate errors (e.g. missing
         * required fields) to be incorrectly reported by callback streams.
         */
        if (ctx->callback != buf_read && ctx->bytes_left == 0)
        {
#ifndef PB_NO_ERRMSG
            if (strcmp(ctx->errmsg, "io error") == 0)
                ctx->errmsg = NULL;
#endif
            *eof = true;
        }
#endif
        return false;
    }
    
#ifdef PB_NO_LARGEMSG
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

/* Read a raw value to buffer, for the purpose of passing it to callback as
 * a substream. Size is maximum size on call, and actual size on return.
 */
static bool checkreturn read_raw_value(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_byte_t *buf, size_t *size)
{
    size_t max_size = *size;
    switch (wire_type)
    {
        case PB_WT_VARINT:
            *size = 0;
            do
            {
                (*size)++;
                if (*size > max_size)
                    PB_RETURN_ERROR(ctx, "varint overflow");

                if (!pb_read(ctx, buf, 1))
                    return false;
            } while (*buf++ & 0x80);
            return true;
            
        case PB_WT_64BIT:
            *size = 8;
            return pb_read(ctx, buf, 8);
        
        case PB_WT_32BIT:
            *size = 4;
            return pb_read(ctx, buf, 4);
        
        case PB_WT_STRING:
            /* Calling read_raw_value with a PB_WT_STRING is an error.
             * Explicitly handle this case and fallthrough to default to avoid
             * compiler warnings.
             */

	case PB_WT_PACKED: 
            /* Calling read_raw_value with a PB_WT_PACKED is an error.
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
bool pb_decode_open_substream(pb_decode_ctx_t *ctx, size_t *old_length)
{
    uint32_t size;
    if (!pb_decode_varint32(ctx, &size))
        return false;
    
    if (ctx->bytes_left < size)
        PB_RETURN_ERROR(ctx, "parent stream too short");

    *old_length = (size_t)(ctx->bytes_left - size);
    ctx->bytes_left = (size_t)size;
    return true;
}

bool pb_decode_close_substream(pb_decode_ctx_t *ctx, size_t old_length)
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

    pb_decode_ctx_t defctx = PB_ISTREAM_EMPTY;
    pb_tag_t tag = 0;
    pb_wire_type_t wire_type = PB_WT_VARINT;
    bool eof;

    if (iter->descriptor->default_value)
    {
        // Field default values are stored as a protobuf stream
        if (!pb_init_decode_ctx_for_buffer(&defctx, iter->descriptor->default_value, (size_t)-1) ||
            !pb_decode_tag(&defctx, &wire_type, &tag, &eof))
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

    do
    {
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
        }
        else if (PB_ATYPE(iter->type) == PB_ATYPE_POINTER)
        {
            /* Initialize the pointer to NULL. */
            *(void**)iter->pField = NULL;

            /* Initialize array count to 0. */
            if (PB_HTYPE(iter->type) == PB_HTYPE_REPEATED)
            {
                *(pb_size_t*)iter->pSize = 0;
            }
            else if(PB_HTYPE(iter->type) == PB_HTYPE_ONEOF)
            {
                *(pb_tag_t*)iter->pSize = 0;
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
        }

        if (iter->tag == tag)
        {
            /* We have a default value for this field, read it out and read the next tag. */
            if (!decode_field(&defctx, wire_type, iter))
                return PB_WALK_EXIT_ERR;

            if (!pb_decode_tag(&defctx, &wire_type, &tag, &eof))
                return PB_WALK_EXIT_ERR;

            if (PB_HTYPE(iter->type) == PB_HTYPE_OPTIONAL && iter->pSize != NULL)
                *(bool*)iter->pSize = false;
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

            return pb_dec_bool(ctx, field);

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

#ifdef PB_CONVERT_DOUBLE_FLOAT
            if (field->data_size == sizeof(float))
            {
                return pb_decode_double_as_float(ctx, (float*)field->pData);
            }
#endif

#ifdef PB_WITHOUT_64BIT
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
                size_t old_length;
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

#ifdef PB_ENABLE_MALLOC
/* Allocate storage for the field and store the pointer at iter->pData.
 * array_size is the number of entries to reserve in an array.
 * Zero size is not allowed, use pb_free() for releasing.
 */
static bool checkreturn allocate_field(pb_decode_ctx_t *ctx, void *pData, size_t data_size, size_t array_size)
{    
    void *ptr = *(void**)pData;
    
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
        const size_t check_limit = (size_t)1 << (sizeof(size_t) * 4);
        if (data_size >= check_limit || array_size >= check_limit)
        {
            const size_t size_max = (size_t)-1;
            if (size_max / array_size < data_size)
            {
                PB_RETURN_ERROR(ctx, "size too large");
            }
        }
    }
    
    /* Allocate new or expand previous allocation */
    /* Note: on failure the old pointer will remain in the structure,
     * the message must be freed by caller also on error return. */
    ptr = pb_realloc(ptr, array_size * data_size);
    if (ptr == NULL)
        PB_RETURN_ERROR(ctx, "realloc failed");
    
    *(void**)pData = ptr;
    return true;
}

/* Clear a newly allocated item in case it contains a pointer. */
static void initialize_pointer_field(void *pItem, pb_field_iter_t *field)
{
    if (PB_LTYPE(field->type) == PB_LTYPE_STRING ||
        PB_LTYPE(field->type) == PB_LTYPE_BYTES)
    {
        *(void**)pItem = NULL;
    }
}
#endif

static bool checkreturn decode_pointer_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
#ifndef PB_ENABLE_MALLOC
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
                if (!allocate_field(ctx, field->pField, field->data_size, 1))
                    return false;
                
                field->pData = *(void**)field->pField;
                initialize_pointer_field(field->pData, field);

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
                size_t allocated_size = *size;
                pb_istream_t substream;
                
                if (!pb_make_string_substream(ctx, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if (*size == PB_SIZE_MAX)
                    {
#ifndef PB_NO_ERRMSG
                        ctx->errmsg = "too many array entries";
#endif
                        status = false;
                        break;
                    }

                    if ((size_t)*size + 1 > allocated_size)
                    {
                        /* Allocate more storage. This tries to guess the
                         * number of remaining entries. Round the division
                         * upwards. */
                        size_t remain = (substream.bytes_left - 1) / field->data_size + 1;
                        if (remain < PB_SIZE_MAX - allocated_size)
                            allocated_size += remain;
                        else
                            allocated_size += 1;
                        
                        if (!allocate_field(&substream, field->pField, field->data_size, allocated_size))
                        {
                            status = false;
                            break;
                        }
                    }

                    /* Decode the array entry */
                    field->pData = *(char**)field->pField + field->data_size * (*size);
                    if (field->pData == NULL)
                    {
                        /* Shouldn't happen, but satisfies static analyzers */
                        status = false;
                        break;
                    }
                    initialize_pointer_field(field->pData, field);
                    if (!decode_basic_field(&substream, PB_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    
                    (*size)++;
                }
                if (!pb_close_string_substream(ctx, &substream))
                    return false;
                
                return status;
            }
            else
            {
                /* Normal repeated field, i.e. only one item at a time. */
                pb_size_t *size = (pb_size_t*)field->pSize;

                if (*size == PB_SIZE_MAX)
                    PB_RETURN_ERROR(ctx, "too many array entries");
                
                if (!allocate_field(ctx, field->pField, field->data_size, (size_t)(*size + 1)))
                    return false;
            
                field->pData = *(char**)field->pField + field->data_size * (*size);
                (*size)++;
                initialize_pointer_field(field->pData, field);
                return decode_basic_field(ctx, wire_type, field);
            }

        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
#endif
}

static bool checkreturn decode_callback_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
    if (!field->descriptor->field_callback)
        return pb_skip_field(ctx, wire_type);

    if (wire_type == PB_WT_STRING)
    {
        size_t old_length;
        size_t prev_bytes_left;
        
        if (!pb_decode_open_substream(ctx, &old_length))
            return false;

        /* If the callback field is inside a submsg, first call the submsg_callback which
         * should set the decoder for the callback field. */
        if (PB_LTYPE(field->type) == PB_LTYPE_SUBMSG_W_CB && field->pSize != NULL) {
            pb_callback_t* callback;
            *(pb_tag_t*)field->pSize = field->tag;
            callback = (pb_callback_t*)field->pSize - 1;

            if (callback->funcs.decode)
            {
                if (!callback->funcs.decode(ctx, field, &callback->arg)) {
                    (void)pb_decode_close_substream(ctx, old_length);
                    PB_RETURN_ERROR(ctx, "submsg callback failed");
                }
            }
        }
        
        do
        {
            prev_bytes_left = ctx->bytes_left;
            if (!field->descriptor->field_callback(ctx, NULL, field))
            {
                (void)pb_decode_close_substream(ctx, old_length);
                PB_RETURN_ERROR(ctx, "submsg callback failed");
            }
        } while (ctx->bytes_left > 0 && ctx->bytes_left < prev_bytes_left);
        
        if (!pb_decode_close_substream(ctx, old_length))
            return false;

        return true;
    }
    else
    {
        /* Copy the single scalar value to stack.
         * This is required so that we can limit the stream length,
         * which in turn allows to use same callback for packed and
         * not-packed fields. */
        pb_byte_t buffer[10];
        size_t size = sizeof(buffer);
        
        if (!read_raw_value(ctx, wire_type, buffer, &size))
            return false;

        // Call the field callback with a length-limited buffer
        // substream context.
        // This is a manual version of pb_decode_open_substream().
        size_t old_length = ctx->bytes_left;
        void *old_state = ctx->state;
        pb_decode_ctx_read_callback_t old_callback = ctx->callback;

        if (!pb_init_decode_ctx_for_buffer(ctx, buffer, size))
            return false;
        
        bool status = field->descriptor->field_callback(ctx, NULL, field);

        ctx->bytes_left = old_length;
        ctx->state = old_state;
        ctx->callback = old_callback;

        return status;
    }
}

static bool checkreturn decode_field(pb_decode_ctx_t *ctx, pb_wire_type_t wire_type, pb_field_iter_t *field)
{
    switch (PB_ATYPE(field->type))
    {
        case PB_ATYPE_STATIC:
            return decode_static_field(ctx, wire_type, field);
        
        case PB_ATYPE_POINTER:
            return decode_pointer_field(ctx, wire_type, field);
        
        case PB_ATYPE_CALLBACK:
            return decode_callback_field(ctx, wire_type, field);
        
        default:
            PB_RETURN_ERROR(ctx, "invalid field type");
    }
}

/*********************
 * Decode all fields *
 *********************/

typedef struct {
    // Parent stream length that gets stored when pb_decode_open_substream() is called.
    // It's also used to store wire type when oneof release is done.
    size_t old_length;

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

// State flags apply to all recursion levels.
// Frame flags apply to single level, and are used for unsetting state flags.
#define PB_DECODE_WALK_STATE_FLAG_SET_DEFAULTS      (uint_least16_t)1
#define PB_DECODE_WALK_STATE_FLAG_RELEASE_FIELD     (uint_least16_t)2
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

#ifdef PB_ENABLE_MALLOC

// Check if there is old data in the oneof union that needs to be released
// prior to replacing it with a new value.
// Return value:
//   PB_WALK_NEXT_ITEM => ok, released or no release needed
//   PB_WALK_IN => release requires recursion, field->pData has been adjusted to the old field
//   PB_WALK_EXIT_ERR => release failed
static pb_walk_retval_t release_oneof(pb_decode_ctx_t *ctx, pb_field_iter_t *field, bool submsg_released)
{
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
            field->pData = old_field.pData;
            field->submsg_desc = old_field.submsg_desc;
            return PB_WALK_IN;
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
#ifdef PB_ENABLE_MALLOC
        // Allocate memory for a pointer field
        if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
        {
            pb_size_t *size = (pb_size_t*)field->pSize;

            if (!allocate_field(ctx, field->pField, field->data_size, (size_t)(*size + 1)))
                return PB_WALK_EXIT_ERR;

            field->pData = *(char**)field->pField + field->data_size * (*size);
            *size += 1;
            need_init = true;
        }
        else if (!field->pData)
        {
            if (!allocate_field(ctx, field->pField, field->data_size, 1))
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
#ifdef PB_ENABLE_MALLOC
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
            release_oneof(ctx, iter, true);
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
#ifdef PB_ENABLE_MALLOC
            pb_walk_retval_t retval = release_oneof(ctx, iter, false);
            if (retval != PB_WALK_NEXT_ITEM)
            {
                if (retval == PB_WALK_IN)
                {
                    state->flags |= PB_DECODE_WALK_STATE_FLAG_RELEASE_FIELD;
                    frame->flags |= PB_DECODE_WALK_FRAME_FLAG_END_RELEASE;
                    frame->old_length = (size_t)wire_type;
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
        else
        {
            if (!decode_field(ctx, wire_type, iter))
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

bool checkreturn pb_decode_s(pb_decode_ctx_t *ctx, const pb_msgdesc_t *fields,
                             void *dest_struct, size_t struct_size)
{
    // Error in struct_size is typically caused by forgetting to rebuild .pb.c file
    // or by it having different compilation options.
    // NOTE: On GCC, sizeof(*(void*)) == 1
    if (fields->struct_size != struct_size && struct_size > 1)
        PB_RETURN_ERROR(ctx, "struct_size mismatch");

    bool status = true;
    size_t old_length = 0;

    pb_walk_state_t state;

    /* Set default values, if needed */
    if ((ctx->flags & PB_DECODE_CTX_FLAG_NOINIT) == 0)
    {
        (void)pb_walk_init(&state, fields, dest_struct, pb_defaults_walk_cb);
        if (!pb_walk(&state))
        {
            PB_RETURN_ERROR(ctx, "failed to set defaults");
        }
    }

    if (ctx->flags & PB_DECODE_CTX_FLAG_DELIMITED)
    {
        if (!pb_decode_open_substream(ctx, &old_length))
            return false;
    }

    /* Decode the message */
    (void)pb_walk_init(&state, fields, dest_struct, pb_decode_walk_cb);
    state.ctx = ctx;
    state.next_stacksize = stacksize_for_msg(fields);

    if (!pb_walk(&state))
    {
        PB_SET_ERROR(ctx, state.errmsg);
        status = false;
    }

    if (ctx->flags & PB_DECODE_CTX_FLAG_DELIMITED)
    {
        if (!pb_decode_close_substream(ctx, old_length))
            status = false;
    }
    
#ifdef PB_ENABLE_MALLOC
    if (!status)
        pb_release_s(ctx, fields, dest_struct, struct_size);
#endif
    
    return status;
}

#ifdef PB_ENABLE_MALLOC

static void pb_release_single_field(pb_decode_ctx_t *ctx, pb_field_iter_t *field)
{
    pb_type_t type;
    type = field->type;

    PB_UNUSED(ctx);
    
    if (PB_ATYPE(type) == PB_ATYPE_POINTER)
    {
        if (PB_HTYPE(type) == PB_HTYPE_REPEATED &&
            (PB_LTYPE(type) == PB_LTYPE_STRING ||
             PB_LTYPE(type) == PB_LTYPE_BYTES))
        {
            /* Release entries in repeated string or bytes array */
            void **pItem = *(void***)field->pField;
            pb_size_t count = *(pb_size_t*)field->pSize;
            for (; count > 0; count--)
            {
                pb_free(*pItem);
                *pItem++ = NULL;
            }
        }
        
        if (PB_HTYPE(type) == PB_HTYPE_REPEATED)
        {
            /* We are going to release the array, so set the size to 0 */
            *(pb_size_t*)field->pSize = 0;
        }
        else if (PB_HTYPE(type) == PB_HTYPE_ONEOF)
        {
            /* Set oneof which_field to 0 */
            *(pb_tag_t*)field->pSize = 0;
        }

        /* Release main pointer */
        pb_free(*(void**)field->pField);
        *(void**)field->pField = NULL;
        field->pData = NULL;
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
                iter->submsg_desc = extension->type;
                iter->pData = pb_get_extension_data_ptr(extension);
                return PB_WALK_IN;
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
                pb_free(iter->pData);
                *(void**)iter->pField = NULL;
            }
            continue;
        }

        if (PB_LTYPE_IS_SUBMSG(iter->type) &&
            (iter->submsg_desc->msg_flags & PB_MSGFLAG_R_HAS_PTRS) != 0)
        {
            // Descend into submessage
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
    (void)pb_walk_init(&state, fields, dest_struct, pb_release_walk_cb);

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
    } u;

    if (!pb_read(ctx, u.bytes, 4))
        return false;

#if defined(PB_LITTLE_ENDIAN_8BIT) && PB_LITTLE_ENDIAN_8BIT == 1
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

#ifndef PB_WITHOUT_64BIT
bool pb_decode_fixed64(pb_decode_ctx_t *ctx, void *dest)
{
    union {
        uint64_t fixed64;
        pb_byte_t bytes[8];
    } u;

    if (!pb_read(ctx, u.bytes, 8))
        return false;

#if defined(PB_LITTLE_ENDIAN_8BIT) && PB_LITTLE_ENDIAN_8BIT == 1
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

static bool checkreturn pb_dec_bool(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    return pb_decode_bool(ctx, (bool*)field->pData);
}

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
    uint32_t size;
    size_t alloc_size;
    pb_bytes_array_t *dest;
    
    if (!pb_decode_varint32(ctx, &size))
        return false;
    
    if ((pb_size_t)size != size)
        PB_RETURN_ERROR(ctx, "bytes overflow");
    
    alloc_size = PB_BYTES_ARRAY_T_ALLOCSIZE(size);
    if (size > alloc_size)
        PB_RETURN_ERROR(ctx, "size too large");
    
    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
#ifndef PB_ENABLE_MALLOC
        PB_RETURN_ERROR(ctx, "no malloc support");
#else
        if (ctx->bytes_left < size)
            PB_RETURN_ERROR(ctx, "end-of-stream");

        if (!allocate_field(ctx, field->pData, alloc_size, 1))
            return false;
        dest = *(pb_bytes_array_t**)field->pData;
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            PB_RETURN_ERROR(ctx, "bytes overflow");
        dest = (pb_bytes_array_t*)field->pData;
    }

    dest->size = (pb_size_t)size;
    return pb_read(ctx, dest->bytes, (size_t)size);
}

static bool checkreturn pb_dec_string(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t size;
    size_t alloc_size;
    pb_byte_t *dest = (pb_byte_t*)field->pData;

    if (!pb_decode_varint32(ctx, &size))
        return false;

    if (size == (uint32_t)-1)
        PB_RETURN_ERROR(ctx, "size too large");

    /* Space for null terminator */
    alloc_size = (size_t)(size + 1);

    if (alloc_size < size)
        PB_RETURN_ERROR(ctx, "size too large");

    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
#ifndef PB_ENABLE_MALLOC
        PB_RETURN_ERROR(ctx, "no malloc support");
#else
        if (ctx->bytes_left < size)
            PB_RETURN_ERROR(ctx, "end-of-stream");

        if (!allocate_field(ctx, field->pData, alloc_size, 1))
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

    if (!pb_read(ctx, dest, (size_t)size))
        return false;

#ifdef PB_VALIDATE_UTF8
    if (!pb_validate_utf8((const char*)dest))
        PB_RETURN_ERROR(ctx, "invalid utf8");
#endif

    return true;
}

static bool checkreturn pb_dec_fixed_length_bytes(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    uint32_t size;

    if (!pb_decode_varint32(ctx, &size))
        return false;

    if ((pb_size_t)size != size)
        PB_RETURN_ERROR(ctx, "bytes overflow");

    if (size == 0)
    {
        /* As a special case, treat empty bytes string as all zeros for fixed_length_bytes. */
        memset(field->pData, 0, (size_t)field->data_size);
        return true;
    }

    if (size != field->data_size)
        PB_RETURN_ERROR(ctx, "incorrect fixed length bytes size");

    return pb_read(ctx, (pb_byte_t*)field->pData, (size_t)field->data_size);
}

#ifdef PB_CONVERT_DOUBLE_FLOAT
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
