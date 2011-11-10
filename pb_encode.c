/* pb_encode.c -- encode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_encode.h"
#include <string.h>

#ifdef __GNUC__
/* Verify that we remember to check all return values for proper error propagation */
#define checkreturn __attribute__((warn_unused_result))
#else
#define checkreturn
#endif


typedef bool (*pb_encoder_t)(pb_ostream_t *stream, const pb_field_t *field, const void *src) checkreturn;

/* --- Function pointers to field encoders ---
 * Order in the array must match pb_action_t LTYPE numbering.
 */
static const pb_encoder_t PB_ENCODERS[PB_LTYPES_COUNT] = {
    &pb_enc_varint,
    &pb_enc_svarint,
    &pb_enc_fixed32,
    &pb_enc_fixed64,
    
    &pb_enc_bytes,
    &pb_enc_string,
    &pb_enc_submessage
};

/* pb_ostream_t implementation */

static bool checkreturn buf_write(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    uint8_t *dest = (uint8_t*)stream->state;
    memcpy(dest, buf, count);
    stream->state = dest + count;
    return true;
}

pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t bufsize)
{
    pb_ostream_t stream;
    stream.callback = &buf_write;
    stream.state = buf;
    stream.max_size = bufsize;
    stream.bytes_written = 0;
    return stream;
}

bool checkreturn pb_write(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    if (stream->callback != NULL)
    {
        if (stream->bytes_written + count > stream->max_size)
            return false;
        
        if (!stream->callback(stream, buf, count))
            return false;
    }
    
    stream->bytes_written += count;
    return true;
}

/* Main encoding stuff */

/* Callbacks don't need this function because they usually know the data type
 * without examining the field structure.
 * Therefore it is static for now.
 */
static bool checkreturn encode_array(pb_ostream_t *stream, const pb_field_t *field,
                         const void *pData, size_t count, pb_encoder_t func)
{
    int i;
    const void *p;
    size_t size;
    
    if (count == 0)
        return true;
    
    if (PB_LTYPE(field->type) <= PB_LTYPE_LAST_PACKABLE)
    {
        if (!pb_encode_tag(stream, PB_WT_STRING, field->tag))
            return false;
        
        /* Determine the total size of packed array. */
        if (PB_LTYPE(field->type) == PB_LTYPE_FIXED32)
        {
            size = 4 * count;
        }
        else if (PB_LTYPE(field->type) == PB_LTYPE_FIXED64)
        {
            size = 8 * count;
        }
        else
        {
            pb_ostream_t sizestream = {0};
            p = pData;
            for (i = 0; i < count; i++)
            {
                if (!func(&sizestream, field, p))
                    return false;
                p = (const char*)p + field->data_size;
            }
            size = sizestream.bytes_written;
        }
        
        if (!pb_encode_varint(stream, size))
            return false;
        
        if (stream->callback == NULL)
            return pb_write(stream, NULL, size); /* Just sizing.. */
        
        /* Write the data */
        p = pData;
        for (i = 0; i < count; i++)
        {
            if (!func(stream, field, p))
                return false;
            p = (const char*)p + field->data_size;
        }
    }
    else
    {
        p = pData;
        for (i = 0; i < count; i++)
        {
            if (!pb_encode_tag_for_field(stream, field))
                return false;
            if (!func(stream, field, p))
                return false;
            p = (const char*)p + field->data_size;
        }
    }
    
    return true;
}

bool checkreturn pb_encode(pb_ostream_t *stream, const pb_field_t fields[], const void *src_struct)
{
    const pb_field_t *field = fields;
    const void *pData = src_struct;
    const void *pSize;
    size_t prev_size = 0;
    
    while (field->tag != 0)
    {
        pb_encoder_t func = PB_ENCODERS[PB_LTYPE(field->type)];
        pData = (const char*)pData + prev_size + field->data_offset;
        pSize = (const char*)pData + field->size_offset;
        
        prev_size = field->data_size;
        if (PB_HTYPE(field->type) == PB_HTYPE_ARRAY)
            prev_size *= field->array_size;
                
        switch (PB_HTYPE(field->type))
        {
            case PB_HTYPE_REQUIRED:
                if (!pb_encode_tag_for_field(stream, field))
                    return false;
                if (!func(stream, field, pData))
                    return false;
                break;
            
            case PB_HTYPE_OPTIONAL:
                if (*(bool*)pSize)
                {
                    if (!pb_encode_tag_for_field(stream, field))
                        return false;
                
                    if (!func(stream, field, pData))
                        return false;
                }
                break;
            
            case PB_HTYPE_ARRAY:
                if (!encode_array(stream, field, pData, *(size_t*)pSize, func))
                    return false;
                break;
            
            case PB_HTYPE_CALLBACK:
            {
                pb_callback_t *callback = (pb_callback_t*)pData;
                if (callback->funcs.encode != NULL)
                {
                    if (!callback->funcs.encode(stream, field, callback->arg))
                        return false;
                }
                break;
            }
        }
    
        field++;
    }
    
    return true;
}

/* Helper functions */
bool checkreturn pb_encode_varint(pb_ostream_t *stream, uint64_t value)
{
    uint8_t buffer[10];
    int i = 0;
    
    if (value == 0)
        return pb_write(stream, (uint8_t*)&value, 1);
    
    while (value)
    {
        buffer[i] = (value & 0x7F) | 0x80;
        value >>= 7;
        i++;
    }
    buffer[i-1] &= 0x7F; /* Unset top bit on last byte */
    
    return pb_write(stream, buffer, i);
}

bool checkreturn pb_encode_tag(pb_ostream_t *stream, pb_wire_type_t wiretype, int field_number)
{
    int tag = wiretype | (field_number << 3);
    return pb_encode_varint(stream, tag);
}

bool checkreturn pb_encode_tag_for_field(pb_ostream_t *stream, const pb_field_t *field)
{
    pb_wire_type_t wiretype;
    switch (PB_LTYPE(field->type))
    {
        case PB_LTYPE_VARINT:
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
            wiretype = PB_WT_STRING;
            break;
        
        default:
            return false;
    }
    
    return pb_encode_tag(stream, wiretype, field->tag);
}

bool checkreturn pb_encode_string(pb_ostream_t *stream, const uint8_t *buffer, size_t size)
{
    if (!pb_encode_varint(stream, size))
        return false;
    
    return pb_write(stream, buffer, size);
}

/* Field encoders */

/* Copy srcsize bytes from src so that values are casted properly.
 * On little endian machine, copy to start of dest
 * On big endian machine, copy to end of dest
 * destsize must always be larger than srcsize
 * 
 * Note: This is the reverse of the endian_copy in pb_decode.c.
 */
static void endian_copy(void *dest, const void *src, size_t destsize, size_t srcsize)
{
#ifdef __BIG_ENDIAN__
    memcpy((char*)dest + (destsize - srcsize), src, srcsize);
#else
    memcpy(dest, src, srcsize);
#endif
}

bool checkreturn pb_enc_varint(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    uint64_t value = 0;
    endian_copy(&value, src, sizeof(value), field->data_size);
    return pb_encode_varint(stream, value);
}

bool checkreturn pb_enc_svarint(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    uint64_t value = 0;
    uint64_t zigzagged;
    uint64_t signbitmask, xormask;
    endian_copy(&value, src, sizeof(value), field->data_size);
    
    signbitmask = (uint64_t)0x80 << (field->data_size * 8 - 8);
    xormask = ((uint64_t)-1) >> (64 - field->data_size * 8);
    if (value & signbitmask)
        zigzagged = ((value ^ xormask) << 1) | 1;
    else
        zigzagged = value << 1;
    
    return pb_encode_varint(stream, zigzagged);
}

bool checkreturn pb_enc_fixed64(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    #ifdef __BIG_ENDIAN__
    uint8_t bytes[8] = {0};
    memcpy(bytes, src, 8);
    uint8_t lebytes[8] = {bytes[7], bytes[6], bytes[5], bytes[4], 
                          bytes[3], bytes[2], bytes[1], bytes[0]};
    return pb_write(stream, lebytes, 8);
    #else
    return pb_write(stream, (uint8_t*)src, 8);
    #endif
}

bool checkreturn pb_enc_fixed32(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    #ifdef __BIG_ENDIAN__
    uint8_t bytes[4] = {0};
    memcpy(bytes, src, 4);
    uint8_t lebytes[4] = {bytes[3], bytes[2], bytes[1], bytes[0]};
    return pb_write(stream, lebytes, 4);
    #else
    return pb_write(stream, (uint8_t*)src, 4);
    #endif
}

bool checkreturn pb_enc_bytes(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    pb_bytes_array_t *bytes = (pb_bytes_array_t*)src;
    return pb_encode_string(stream, bytes->bytes, bytes->size);
}

bool checkreturn pb_enc_string(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    return pb_encode_string(stream, (uint8_t*)src, strlen((char*)src));
}

bool checkreturn pb_enc_submessage(pb_ostream_t *stream, const pb_field_t *field, const void *src)
{
    pb_ostream_t substream = {0};
    size_t size;
    bool status;
    
    if (field->ptr == NULL)
        return false;
    
    if (!pb_encode(&substream, (pb_field_t*)field->ptr, src))
        return false;
    
    size = substream.bytes_written;
    
    if (!pb_encode_varint(stream, size))
        return false;
    
    if (stream->callback == NULL)
        return pb_write(stream, NULL, size); /* Just sizing */
    
    if (stream->bytes_written + size > stream->max_size)
        return false;
        
    /* Use a substream to verify that a callback doesn't write more than
     * what it did the first time. */
    substream.callback = stream->callback;
    substream.state = stream->state;
    substream.max_size = size;
    substream.bytes_written = 0;
    
    status = pb_encode(&substream, (pb_field_t*)field->ptr, src);
    
    stream->bytes_written += substream.bytes_written;
    
    if (substream.bytes_written != size)
        return false;
    
    return status;
}

