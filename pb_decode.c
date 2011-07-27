/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_decode.h"
#include <string.h>

const pb_decoder_t PB_DECODERS[16] = {
    &pb_dec_uint32,
    &pb_dec_sint32,
    &pb_dec_fixed32,
    &pb_dec_uint64,
    &pb_dec_sint64,
    &pb_dec_fixed64,
    &pb_dec_bool,
    &pb_dec_enum,
    
    &pb_dec_float,
    &pb_dec_double,
    
    &pb_dec_bytes,
    &pb_dec_string,
    &pb_dec_submessage
};

////////////////////////
// Helper functions
////////////////////////

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
    pb_istream_t stream = {&buf_read, buf, bufsize};
    return stream;
}

bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest)
{
    uint64_t temp;
    if (!pb_decode_varint64(stream, &temp))
        return false;
    *dest = temp;
    return true;
}

bool pb_decode_varint64(pb_istream_t *stream, uint64_t *dest)
{
    uint8_t byte;
    int bitpos = 0;
    *dest = 0;
    
    while (bitpos < 64 && pb_read(stream, &byte, 1))
    {
        *dest |= (byte & 0x7F) << bitpos;
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

enum wire_type_t {
    WT_VARINT = 0,
    WT_64BIT  = 1,
    WT_STRING = 2,
    WT_32BIT  = 5
};

static bool skip(pb_istream_t *stream, int wire_type)
{
    switch (wire_type)
    {
        case WT_VARINT: return pb_skip_varint(stream);
        case WT_64BIT: return pb_read(stream, NULL, 8);
        case WT_STRING: return pb_skip_string(stream);
        case WT_32BIT: return pb_read(stream, NULL, 4);
        default: return false;
    }
}

// Read a raw value to buffer, for the purpose of passing it to callback.
// Size is maximum size on call, and actual size on return.
static bool read_raw_value(pb_istream_t *stream, int wire_type, uint8_t *buf, size_t *size)
{
    size_t max_size = *size;
    switch (wire_type)
    {
        case WT_VARINT:
            *size = 0;
            do
            {
                (*size)++;
                if (*size > max_size) return false;
                if (!pb_read(stream, buf++, 1)) return false;
            } while (*buf & 0x80);
            return true;
            
        case WT_64BIT:
            *size = 8;
            return pb_read(stream, buf, 8);
        
        case WT_32BIT:
            *size = 4;
            return pb_read(stream, buf, 4);
        
        default: return false;
    }
}

// Decode string length from stream and return a substream with limited length
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

bool decode_field(pb_istream_t *stream, int wire_type, const pb_field_t *field, void *dest_struct)
{
    pb_decoder_t func = PB_DECODERS[PB_LTYPE(field->type)];
    void *pData = (char*)dest_struct + field->data_offset;
    void *pSize = (char*)dest_struct + field->size_offset;
    
    switch (PB_HTYPE(field->type))
    {
        case PB_HTYPE_REQUIRED:
            return func(stream, field, pData);
            
        case PB_HTYPE_OPTIONAL:
            *(bool*)pSize = true;
            return func(stream, field, pData);
    
        case PB_HTYPE_ARRAY:
            if (wire_type == WT_STRING
                && PB_LTYPE(field->type) != PB_LTYPE_BYTES
                && PB_LTYPE(field->type) != PB_LTYPE_STRING
                && PB_LTYPE(field->type) != PB_LTYPE_SUBMESSAGE)
            {
                // Packed array
                size_t *size = (size_t*)pSize;
                pb_istream_t substream;
                if (!make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left && *size < field->array_size)
                {
                    void *pItem = pData + field->data_size * (*size);
                    if (!func(stream, field, pItem))
                        return false;
                    (*size)++;
                }
                return (substream.bytes_left == 0);
            }
            else
            {
                // Repeated field
                size_t *size = (size_t*)pSize;
                if (*size >= field->array_size)
                    return false;
                
                void *pItem = pData + field->data_size * (*size);
                (*size)++;
                return func(stream, field, pItem);
            }
        
        case PB_HTYPE_CALLBACK:
            if (wire_type == WT_STRING)
            {
                pb_callback_t *pCallback = (pb_callback_t*)pData;
                pb_istream_t substream;
                
                if (!make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if (!pCallback->funcs.decode(&substream, field, pCallback->arg))
                        return false;
                }
            }
            else
            {
                // Copy the single scalar value to stack.
                // This is required so that we can limit the stream length,
                // which in turn allows to use same callback for packed and
                // not-packed fields.
                uint8_t buffer[10];
                size_t size = sizeof(buffer);
                if (!read_raw_value(stream, wire_type, buffer, &size))
                    return false;
                pb_istream_t substream = pb_istream_from_buffer(buffer, size);
                
                pb_callback_t *pCallback = (pb_callback_t*)pData;
                return pCallback->funcs.decode(&substream, field, pCallback->arg);
            }
            
        default:
            return false;
    }
}

bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    // Used to check for required fields
    uint32_t fields_seen = 0;
    int i;
    
    // Initialize size/has fields and apply default values
    for (i = 0; fields[i].tag != 0; i++)
    {
        void *pData = (char*)dest_struct + fields[i].data_offset;
        void *pSize = (char*)dest_struct + fields[i].size_offset;
        if (PB_HTYPE(fields[i].type) == PB_HTYPE_OPTIONAL)
        {
            *(bool*)pSize = false;
        }
        else if (PB_HTYPE(fields[i].type) == PB_HTYPE_ARRAY)
        {
            *(size_t*)pSize = 0;
        }
        
        if (PB_HTYPE(fields[i].type) != PB_HTYPE_ARRAY &&
            PB_HTYPE(fields[i].type) != PB_HTYPE_CALLBACK)
        {
            if (fields[i].ptr != NULL)
            {
                memcpy(pData, fields[i].ptr, fields[i].data_size);
            }
            else
            {
                memset(pData, 0, fields[i].data_size);
            }
        }
    }
    
    while (stream->bytes_left)
    {
        uint32_t temp;
        if (!pb_decode_varint32(stream, &temp))
            return false;
        
        int tag = temp >> 3;
        int wire_type = temp & 7;
        
        i = 0;
        while (fields[i].tag != 0 && fields[i].tag != tag)
        {
            i++;
        }
        
        if (fields[i].tag == 0) // No match found, skip data
        {
            skip(stream, wire_type);
            continue;
        }
        
        fields_seen |= 1 << (i & 31);
            
        if (!decode_field(stream, wire_type, &fields[i], dest_struct))
            return false;
    }
    
    // Check that all required fields (mod 31) were present.
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

bool pb_dec_uint32(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_decode_varint32(stream, (uint32_t*)dest);
}

bool pb_dec_sint32(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t *x = (uint32_t*)dest;
    bool status = pb_decode_varint32(stream, x);
    *x = (*x >> 1) ^ -(int32_t)(*x & 1);
    return status;
}

bool pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint8_t bytes[4] = {0};
    bool status = pb_read(stream, bytes, 4);
    
#ifdef __BIG_ENDIAN__
    uint8_t lebytes[4] = {bytes[3], bytes[2], bytes[1], bytes[0]};
    memcpy(dest, lebytes, 4);
#else
    memcpy(dest, bytes, 4);
#endif
    return status;
}

bool pb_dec_uint64(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_decode_varint64(stream, (uint64_t*)dest);
}

bool pb_dec_sint64(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t *x = (uint64_t*)dest;
    bool status = pb_decode_varint64(stream, x);
    *x = (*x >> 1) ^ -(int64_t)(*x & 1);
    return status;
}

bool pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint8_t bytes[8] = {0};
    bool status = pb_read(stream, bytes, 8);
    
#ifdef __BIG_ENDIAN__
    uint8_t lebytes[8] = {bytes[7], bytes[6], bytes[5], bytes[4], 
                          bytes[3], bytes[2], bytes[1], bytes[0]};
    memcpy(dest, lebytes, 4);
#else
    memcpy(dest, bytes, 4);
#endif
    return status;
}

bool pb_dec_bool(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t temp = 0;
    bool status = pb_decode_varint32(stream, &temp);
    *(bool*)dest = !!temp;
    return status;
}

bool pb_dec_enum(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    // Enum sizes can vary, copy only data_size amount of bytes.
    uint32_t temp = 0;
    bool status = pb_decode_varint32(stream, &temp);
    memcpy(dest, &temp, field->data_size);
    return status;
}

bool pb_dec_float(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_read(stream, (uint8_t*)dest, sizeof(float));
}

bool pb_dec_double(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_read(stream, (uint8_t*)dest, sizeof(double));
}

bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_bytes_array_t *x = (pb_bytes_array_t*)dest;
    
    uint32_t temp;
    if (!pb_decode_varint32(stream, &temp))
        return false;
    x->size = temp;
    
    // Note: data_size includes the size of the x.size field, too.
    // Calculate actual size starting from offset.
    if (x->size > field->data_size - offsetof(pb_bytes_array_t, bytes))
        return false;
    
    return pb_read(stream, x->bytes, x->size);
}

bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    if (size > field->data_size - 1)
        return false;
    
    bool status = pb_read(stream, (uint8_t*)dest, size);
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
