/* pb_decode.c -- decode a protobuf using callback functions
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb_decode.h"

const pb_decoder_t PB_DECODERS[PB_LAST_ACT] = {
    NULL,
    &pb_dec_uint32,
    &pb_dec_sint32,
    &pb_dec_uint32, // Cast to int32
    &pb_dec_fixed32,
    &pb_dec_fixed32, // Cast to int32
    &pb_dec_uint64,
    &pb_dec_sint64,
    &pb_dec_uint64, // Cast to int64
    &pb_dec_fixed64,
    &pb_dec_fixed64, // Cast to int64
    &pb_dec_bool,
    &pb_dec_float,
    &pb_dec_double,
    &pb_dec_bytes,
    &pb_dec_string,
    &pb_dec_submessage
};

enum wire_type {
    WT_VARINT = 0,
    WT_64BIT  = 1,
    WT_STRING = 2,
    WT_32BIT  = 5
};

// Note: pb_decode_varint32 is a bit un-orthodox:
// it will refuse to decode values that exceed uint32 range.
// The Google implementation would simply cast to 32 bits.
bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest)
{
    char byte;
    int bitpos = 0;
    *dest = 0;
    
    while (bitpos < 32 && pb_read(stream, &byte, 1))
    {
        *dest |= (byte & 0x7F) << bitpos;
        bitpos += 7;
        
        if (!(byte & 0x80))
            return true;
    }
    
    return false;
}

bool pb_decode_varint64(pb_istream_t *stream, uint64_t *dest)
{
    char byte;
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
    char byte;
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

bool pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest)
{
    while (stream->bytes_left)
    {
        uint32_t temp;
        if (!pb_decode_varint32(stream, &temp))
            return false;
        
        int field_number = temp >> 3;
        int wire_type = temp & 7;
        
        const pb_field_t *field = fields;
        while (field->field_number != 0)
        {
            if (field->field_number != field_number)
            {
                field++;
                continue;
            }
            
            void *destfield = dest + field->offset; 
            
            if (field->action == PB_ACT_HAS)
            {
                *(bool*)destfield = true;
                field++;
                continue;
            }
            
            pb_decoder_t func = PB_DECODERS[field->action];
            if (!func(stream, field, destfield))
                return false;
            
            break;
        }
        
        if (field->field_number == 0) // No match found, skip data
        {
            bool status = false;
            switch (wire_type)
            {
                case WT_VARINT:
                    status = pb_skip_varint(stream);
                    break;
                
                case WT_64BIT:
                    status = pb_read(stream, NULL, 8);
                    break;
                
                case WT_STRING:
                    status = pb_skip_string(stream);
                    break;
                
                case WT_32BIT:
                    status = pb_read(stream, NULL, 4);
                    break;
            }
            
            if (!status)
                return false;
        }
    }
    
    return true;
}

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
    char bytes[4] = {0};
    bool status = pb_read(stream, bytes, 4);
    *(uint32_t*)dest = 
        bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
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
    char bytes[8] = {0};
    bool status = pb_read(stream, bytes, 8);
    *(uint64_t*)dest =
        (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) |
        ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |
        ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
        ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
    return status;
}

bool pb_dec_bool(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t temp = 0;
    bool status = pb_decode_varint32(stream, &temp);
    *(bool*)dest = !!temp;
    return status;
}

bool pb_dec_float(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_read(stream, (char*)dest, sizeof(float));
}

bool pb_dec_double(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    return pb_read(stream, (char*)dest, sizeof(double));
}

bool pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_bytearray_t *x = (pb_bytearray_t*)dest;
    uint32_t temp;
    if (!pb_decode_varint32(stream, &temp))
        return false;
    x->size = temp;
    
    if (x->size > field->fieldsize)
        return false;
    
    return pb_read(stream, x->bytes, x->size);
}

bool pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    if (size > field->fieldsize - 1)
        return false;
    
    bool status = pb_read(stream, (char*)dest, size);
    *((char*)dest + size) = 0;
    return status;
}

bool pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_callback_t *x = (pb_callback_t*)dest;
    
    if (x->funcs.decode == NULL)
        return pb_skip_string(stream);
    
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    if (stream->bytes_left < size)
        return false;
    
    // Make a limited-length istream for decoding submessage
    pb_istream_t shortstream = *stream;
    shortstream.bytes_left = size;
    bool status = x->funcs.decode(&shortstream, field, x->arg);
    stream->bytes_left -= size - shortstream.bytes_left;
    return status;
}
