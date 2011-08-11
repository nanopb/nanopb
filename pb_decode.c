/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_decode.h"
#include <string.h>

typedef bool (*pb_decoder_t)(pb_istream_t *stream, const pb_field_t *field, void *dest);

/* --- Function pointers to field decoders ---
 * Order in the array must match pb_action_t LTYPE numbering.
 */
static const pb_decoder_t PB_DECODERS[PB_LTYPES_COUNT] = {
    &pb_dec_varint,
    &pb_dec_svarint,
    &pb_dec_fixed,
    
    &pb_dec_bytes,
    &pb_dec_string,
    &pb_dec_submessage
};

/**************
 * pb_istream *
 **************/

bool pb_read(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    if (stream->bytes_left < count)
        return false;
    
    if (!stream->callback(stream, buf, count))
        return false;
    
    stream->bytes_left -= count;
    return true;
}

static bool buf_read(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    uint8_t *source = (uint8_t*)stream->state;
    
    if (buf != NULL)
        memcpy(buf, source, count);
    
    stream->state = source + count;
    return true;
}

pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize)
{
    pb_istream_t stream;
    stream.callback = &buf_read;
    stream.state = buf;
    stream.bytes_left = bufsize;
    return stream;
}

/********************
 * Helper functions *
 ********************/

bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest)
{
    uint64_t temp;
    bool status = pb_decode_varint64(stream, &temp);
    *dest = temp;
    return status;
}

bool pb_decode_varint64(pb_istream_t *stream, uint64_t *dest)
{
    uint8_t byte;
    int bitpos = 0;
    *dest = 0;
    
    while (bitpos < 64 && pb_read(stream, &byte, 1))
    {
        *dest |= (uint64_t)(byte & 0x7F) << bitpos;
        bitpos += 7;
        
        if (!(byte & 0x80))
            return true;
    }
    
    return false;
}

bool pb_skip_varint(pb_istream_t *stream)
{
    uint8_t byte;
    do
    {
        if (!pb_read(stream, &byte, 1))
            return false;
    } while (byte & 0x80);
    return true;
}

bool pb_skip_string(pb_istream_t *stream)
{
    uint32_t length;
    if (!pb_decode_varint32(stream, &length))
        return false;
    
    return pb_read(stream, NULL, length);
}

/* Currently all wire type related stuff is kept hidden from
 * callbacks. They shouldn't need it. It's better for performance
 * to just assume the correct type and fail safely on corrupt message.
 */

static bool skip(pb_istream_t *stream, int wire_type)
{
    switch (wire_type)
    {
        case PB_WT_VARINT: return pb_skip_varint(stream);
        case PB_WT_64BIT: return pb_read(stream, NULL, 8);
        case PB_WT_STRING: return pb_skip_string(stream);
        case PB_WT_32BIT: return pb_read(stream, NULL, 4);
        default: return false;
    }
}

/* Read a raw value to buffer, for the purpose of passing it to callback as
 * a substream. Size is maximum size on call, and actual size on return.
 */
static bool read_raw_value(pb_istream_t *stream, pb_wire_type_t wire_type, uint8_t *buf, size_t *size)
{
    size_t max_size = *size;
    switch (wire_type)
    {
        case PB_WT_VARINT:
            *size = 0;
            do
            {
                (*size)++;
                if (*size > max_size) return false;
                if (!pb_read(stream, buf, 1)) return false;
            } while (*buf++ & 0x80);
            return true;
            
        case PB_WT_64BIT:
            *size = 8;
            return pb_read(stream, buf, 8);
        
        case PB_WT_32BIT:
            *size = 4;
            return pb_read(stream, buf, 4);
        
        default: return false;
    }
}

/* Decode string length from stream and return a substream with limited length */
static bool make_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    *substream = *stream;
    if (substream->bytes_left < size)
        return false;
    
    substream->bytes_left = size;
    stream->bytes_left -= size;
    return true;
}

/* Iterator for pb_field_t list */
typedef struct {
    const pb_field_t *start;
    const pb_field_t *current;
    int field_index;
    void *dest_struct;
    void *pData;
    void *pSize;
} pb_field_iterator_t;

static void pb_field_init(pb_field_iterator_t *iter, const pb_field_t *fields, void *dest_struct)
{
    iter->start = iter->current = fields;
    iter->field_index = 0;
    iter->pData = dest_struct;
    iter->dest_struct = dest_struct;
}

static bool pb_field_next(pb_field_iterator_t *iter)
{
    bool notwrapped = true;
    iter->current++;
    iter->field_index++;
    if (iter->current->tag == 0)
    {
        iter->current = iter->start;
        iter->field_index = 0;
        iter->pData = iter->dest_struct;
        notwrapped = false;
    }
    
    iter->pData = (char*)iter->pData + iter->current->data_offset;
    iter->pSize = (char*)iter->pData + iter->current->size_offset;
    return notwrapped;
}

static bool pb_field_find(pb_field_iterator_t *iter, int tag)
{
    int start = iter->field_index;
    
    do {
        if (iter->current->tag == tag)
            return true;
        pb_field_next(iter);
    } while (iter->field_index != start);
    
    return false;
}

/*************************
 * Decode a single field *
 *************************/

bool decode_field(pb_istream_t *stream, int wire_type, pb_field_iterator_t *iter)
{
    pb_decoder_t func = PB_DECODERS[PB_LTYPE(iter->current->type)];
    
    switch (PB_HTYPE(iter->current->type))
    {
        case PB_HTYPE_REQUIRED:
            return func(stream, iter->current, iter->pData);
            
        case PB_HTYPE_OPTIONAL:
            *(bool*)iter->pSize = true;
            return func(stream, iter->current, iter->pData);
    
        case PB_HTYPE_ARRAY:
            if (wire_type == PB_WT_STRING
                && PB_LTYPE(iter->current->type) <= PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array */
                size_t *size = (size_t*)iter->pSize;
                pb_istream_t substream;
                if (!make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left && *size < iter->current->array_size)
                {
                    void *pItem = (uint8_t*)iter->pData + iter->current->data_size * (*size);
                    if (!func(stream, iter->current, pItem))
                        return false;
                    (*size)++;
                }
                return (substream.bytes_left == 0);
            }
            else
            {
                /* Repeated field */
                size_t *size = (size_t*)iter->pSize;
                void *pItem = (uint8_t*)iter->pData + iter->current->data_size * (*size);
                if (*size >= iter->current->array_size)
                    return false;
                
                (*size)++;
                return func(stream, iter->current, pItem);
            }
        
        case PB_HTYPE_CALLBACK:
            if (wire_type == PB_WT_STRING)
            {
                pb_callback_t *pCallback = (pb_callback_t*)iter->pData;
                pb_istream_t substream;
                
                if (!make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if (!pCallback->funcs.decode(&substream, iter->current, pCallback->arg))
                        return false;
                }
                return true;
            }
            else
            {
                /* Copy the single scalar value to stack.
                 * This is required so that we can limit the stream length,
                 * which in turn allows to use same callback for packed and
                 * not-packed fields. */
                pb_istream_t substream;
                pb_callback_t *pCallback = (pb_callback_t*)iter->pData;
                uint8_t buffer[10];
                size_t size = sizeof(buffer);
                
                if (!read_raw_value(stream, wire_type, buffer, &size))
                    return false;
                substream = pb_istream_from_buffer(buffer, size);
                
                return pCallback->funcs.decode(&substream, iter->current, pCallback->arg);
            }
            
        default:
            return false;
    }
}

/*********************
 * Decode all fields *
 *********************/

bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    uint32_t fields_seen = 0; /* Used to check for required fields */
    pb_field_iterator_t iter;
    int i;
    
    if (fields[0].tag == 0)
    {
        /* No fields -> nothing to do */
        return pb_read(stream, NULL, stream->bytes_left);
    }
    
    pb_field_init(&iter, fields, dest_struct);
    
    /* Initialize size/has fields and apply default values */
    do
    {
        if (PB_HTYPE(iter.current->type) == PB_HTYPE_OPTIONAL)
        {
            *(bool*)iter.pSize = false;
            
            /* Initialize to default value */
            if (iter.current->ptr != NULL)
                memcpy(iter.pData, iter.current->ptr, iter.current->data_size);
            else
                memset(iter.pData, 0, iter.current->data_size);
        }
        else if (PB_HTYPE(iter.current->type) == PB_HTYPE_ARRAY)
        {
            *(size_t*)iter.pSize = 0;
        }
    } while (pb_field_next(&iter));
    
    while (stream->bytes_left)
    {
        uint32_t temp;
        int tag, wire_type;
        if (!pb_decode_varint32(stream, &temp))
            return stream->bytes_left == 0; /* Was it EOF? */
        
        tag = temp >> 3;
        wire_type = temp & 7;
        
        if (!pb_field_find(&iter, tag))
        {
            /* No match found, skip data */
            skip(stream, wire_type);
            continue;
        }
        
        fields_seen |= 1 << (iter.field_index & 31);
            
        if (!decode_field(stream, wire_type, &iter))
            return false;
    }
    
    /* Check that all required fields (mod 31) were present. */
    for (i = 0; fields[i].tag != 0; i++)
    {
        if (PB_HTYPE(fields[i].type) == PB_HTYPE_REQUIRED &&
            !(fields_seen & (1 << (i & 31))))
        {
            return false;
        }
    }
    
    return true;
}

/* Field decoders */

/* Copy destsize bytes from src so that values are casted properly.
 * On little endian machine, copy first n bytes of src
 * On big endian machine, copy last n bytes of src
 * srcsize must always be larger than destsize
 */
static void endian_copy(void *dest, void *src, size_t destsize, size_t srcsize)
{
#ifdef __BIG_ENDIAN__
    memcpy(dest, (char*)src + (srcsize - destsize), destsize);
#else
    memcpy(dest, src, destsize);
#endif
}

bool pb_dec_varint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t temp;
    bool status = pb_decode_varint64(stream, &temp);
    endian_copy(dest, &temp, field->data_size, sizeof(temp));
    return status;
}

bool pb_dec_svarint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t temp;
    bool status = pb_decode_varint64(stream, &temp);
    temp = (temp >> 1) ^ -(int64_t)(temp & 1);
    endian_copy(dest, &temp, field->data_size, sizeof(temp));
    return status;
}

bool pb_dec_fixed(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
#ifdef __BIG_ENDIAN__
    uint8_t bytes[8] = {0};
    bool status = pb_read(stream, bytes, field->data_size);
    uint8_t bebytes[8] = {bytes[7], bytes[6], bytes[5], bytes[4], 
                          bytes[3], bytes[2], bytes[1], bytes[0]};
    endian_copy(dest, lebytes, field->data_size, 8);
    return status;
#else
    return pb_read(stream, (uint8_t*)dest, field->data_size);
#endif
}

bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_bytes_array_t *x = (pb_bytes_array_t*)dest;
    
    uint32_t temp;
    if (!pb_decode_varint32(stream, &temp))
        return false;
    x->size = temp;
    
    /* Note: data_size includes the size of the x.size field, too.
     * Calculate actual size starting from offset. */
    if (x->size > field->data_size - offsetof(pb_bytes_array_t, bytes))
        return false;
    
    return pb_read(stream, x->bytes, x->size);
}

bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    bool status;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    if (size > field->data_size - 1)
        return false;
    
    status = pb_read(stream, (uint8_t*)dest, size);
    *((uint8_t*)dest + size) = 0;
    return status;
}

bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_istream_t substream;
    
    if (!make_string_substream(stream, &substream))
        return false;
    
    if (field->ptr == NULL)
        return false;
    
    return pb_decode(&substream, (pb_field_t*)field->ptr, dest);
}
