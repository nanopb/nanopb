/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

/* The warn_unused_result attribute appeared first in gcc-3.4.0 */
#if !defined(__GNUC__) || ( __GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
    #define checkreturn
#else
    /* Verify that we remember to check all return values for proper error propagation */
    #define checkreturn __attribute__((warn_unused_result))
#endif

#define NANOPB_INTERNALS
#include "pb.h"
#include "pb_decode.h"
#include <string.h>

typedef bool (*pb_decoder_t)(pb_istream_t *stream, const pb_field_t *field, void *dest) checkreturn;

/* --- Function pointers to field decoders ---
 * Order in the array must match pb_action_t LTYPE numbering.
 */
static const pb_decoder_t PB_DECODERS[PB_LTYPES_COUNT] = {
    &pb_dec_varint,
    &pb_dec_svarint,
    &pb_dec_fixed32,
    &pb_dec_fixed64,
    
    &pb_dec_bytes,
    &pb_dec_string,
    &pb_dec_submessage
};

/**************
 * pb_istream *
 **************/

bool checkreturn pb_read(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    if (stream->bytes_left < count)
        PB_RETURN_ERROR(stream, "end-of-stream");
    
    if (!stream->callback(stream, buf, count))
        PB_RETURN_ERROR(stream, "io error");
    
    stream->bytes_left -= count;
    return true;
}

static bool checkreturn buf_read(pb_istream_t *stream, uint8_t *buf, size_t count)
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

static bool checkreturn pb_decode_varint32(pb_istream_t *stream, uint32_t *dest)
{
    uint64_t temp;
    bool status = pb_decode_varint(stream, &temp);
    *dest = (uint32_t)temp;
    return status;
}

bool checkreturn pb_decode_varint(pb_istream_t *stream, uint64_t *dest)
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
    
    PB_RETURN_ERROR(stream, "varint overflow");
}

bool checkreturn pb_skip_varint(pb_istream_t *stream)
{
    uint8_t byte;
    do
    {
        if (!pb_read(stream, &byte, 1))
            return false;
    } while (byte & 0x80);
    return true;
}

bool checkreturn pb_skip_string(pb_istream_t *stream)
{
    uint32_t length;
    if (!pb_decode_varint32(stream, &length))
        return false;
    
    return pb_read(stream, NULL, length);
}

bool checkreturn pb_decode_tag(pb_istream_t *stream, pb_wire_type_t *wire_type, uint32_t *tag, bool *eof)
{
    uint32_t temp;
    *eof = false;
    *wire_type = (pb_wire_type_t) 0;
    *tag = 0;
    
    if (!pb_decode_varint32(stream, &temp))
    {
        if (stream->bytes_left == 0)
            *eof = true;

        return false;
    }
    
    if (temp == 0)
    {
        *eof = true; /* Special feature: allow 0-terminated messages. */
        return false;
    }
    
    *tag = temp >> 3;
    *wire_type = (pb_wire_type_t)(temp & 7);
    return true;
}

bool checkreturn pb_skip_field(pb_istream_t *stream, pb_wire_type_t wire_type)
{
    switch (wire_type)
    {
        case PB_WT_VARINT: return pb_skip_varint(stream);
        case PB_WT_64BIT: return pb_read(stream, NULL, 8);
        case PB_WT_STRING: return pb_skip_string(stream);
        case PB_WT_32BIT: return pb_read(stream, NULL, 4);
        default: PB_RETURN_ERROR(stream, "invalid wire_type");
    }
}

/* Read a raw value to buffer, for the purpose of passing it to callback as
 * a substream. Size is maximum size on call, and actual size on return.
 */
static bool checkreturn read_raw_value(pb_istream_t *stream, pb_wire_type_t wire_type, uint8_t *buf, size_t *size)
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
        
        default: PB_RETURN_ERROR(stream, "invalid wire_type");
    }
}

/* Decode string length from stream and return a substream with limited length.
 * Remember to close the substream using pb_close_string_substream().
 */
bool checkreturn pb_make_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    uint32_t size;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    *substream = *stream;
    if (substream->bytes_left < size)
        PB_RETURN_ERROR(stream, "parent stream too short");
    
    substream->bytes_left = size;
    stream->bytes_left -= size;
    return true;
}

void pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream)
{
    stream->state = substream->state;
}

/* Iterator for pb_field_t list */
typedef struct {
    const pb_field_t *start; /* Start of the pb_field_t array */
    const pb_field_t *current; /* Current position of the iterator */
    int field_index; /* Zero-based index of the field. */
    int required_field_index; /* Zero-based index that counts only the required fields */
    void *dest_struct; /* Pointer to the destination structure to decode to */
    void *pData; /* Pointer where to store current field value */
    void *pSize; /* Pointer where to store the size of current array field */
} pb_field_iterator_t;

static void pb_field_init(pb_field_iterator_t *iter, const pb_field_t *fields, void *dest_struct)
{
    iter->start = iter->current = fields;
    iter->field_index = 0;
    iter->required_field_index = 0;
    iter->pData = (char*)dest_struct + iter->current->data_offset;
    iter->pSize = (char*)iter->pData + iter->current->size_offset;
    iter->dest_struct = dest_struct;
}

static bool pb_field_next(pb_field_iterator_t *iter)
{
    bool notwrapped = true;
    size_t prev_size = iter->current->data_size;
    
    if (PB_HTYPE(iter->current->type) == PB_HTYPE_ARRAY)
        prev_size *= iter->current->array_size;
    
    if (PB_HTYPE(iter->current->type) == PB_HTYPE_REQUIRED)
        iter->required_field_index++;
    
    iter->current++;
    iter->field_index++;
    if (iter->current->tag == 0)
    {
        iter->current = iter->start;
        iter->field_index = 0;
        iter->required_field_index = 0;
        iter->pData = iter->dest_struct;
        prev_size = 0;
        notwrapped = false;
    }
    
    iter->pData = (char*)iter->pData + prev_size + iter->current->data_offset;
    iter->pSize = (char*)iter->pData + iter->current->size_offset;
    return notwrapped;
}

static bool checkreturn pb_field_find(pb_field_iterator_t *iter, uint32_t tag)
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

static bool checkreturn decode_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iterator_t *iter)
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
                bool status;
                size_t *size = (size_t*)iter->pSize;
                pb_istream_t substream;
                if (!pb_make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left && *size < iter->current->array_size)
                {
                    void *pItem = (uint8_t*)iter->pData + iter->current->data_size * (*size);
                    if (!func(&substream, iter->current, pItem))
                        return false;
                    (*size)++;
                }
                status = (substream.bytes_left == 0);
                pb_close_string_substream(stream, &substream);
                return status;
            }
            else
            {
                /* Repeated field */
                size_t *size = (size_t*)iter->pSize;
                void *pItem = (uint8_t*)iter->pData + iter->current->data_size * (*size);
                if (*size >= iter->current->array_size)
                    PB_RETURN_ERROR(stream, "array overflow");
                
                (*size)++;
                return func(stream, iter->current, pItem);
            }
        
        case PB_HTYPE_CALLBACK:
        {
            pb_callback_t *pCallback = (pb_callback_t*)iter->pData;
            
            if (pCallback->funcs.decode == NULL)
                return pb_skip_field(stream, wire_type);
            
            if (wire_type == PB_WT_STRING)
            {
                pb_istream_t substream;
                
                if (!pb_make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if (!pCallback->funcs.decode(&substream, iter->current, pCallback->arg))
                        PB_RETURN_ERROR(stream, "callback failed");
                }
                
                pb_close_string_substream(stream, &substream);
                return true;
            }
            else
            {
                /* Copy the single scalar value to stack.
                 * This is required so that we can limit the stream length,
                 * which in turn allows to use same callback for packed and
                 * not-packed fields. */
                pb_istream_t substream;
                uint8_t buffer[10];
                size_t size = sizeof(buffer);
                
                if (!read_raw_value(stream, wire_type, buffer, &size))
                    return false;
                substream = pb_istream_from_buffer(buffer, size);
                
                return pCallback->funcs.decode(&substream, iter->current, pCallback->arg);
            }
        }
        
        default:
            PB_RETURN_ERROR(stream, "invalid field type");
    }
}

/* Initialize message fields to default values, recursively */
static void pb_message_set_to_defaults(const pb_field_t fields[], void *dest_struct)
{
    pb_field_iterator_t iter;
    pb_field_init(&iter, fields, dest_struct);
    
    /* Initialize size/has fields and apply default values */
    do
    {
        if (iter.current->tag == 0)
            continue;
        
        /* Initialize the size field for optional/repeated fields to 0. */
        if (PB_HTYPE(iter.current->type) == PB_HTYPE_OPTIONAL)
        {
            *(bool*)iter.pSize = false;
        }
        else if (PB_HTYPE(iter.current->type) == PB_HTYPE_ARRAY)
        {
            *(size_t*)iter.pSize = 0;
            continue; /* Array is empty, no need to initialize contents */
        }
        
        /* Initialize field contents to default value */
        if (PB_HTYPE(iter.current->type) == PB_HTYPE_CALLBACK)
        {
            continue; /* Don't overwrite callback */
        }
        else if (PB_LTYPE(iter.current->type) == PB_LTYPE_SUBMESSAGE)
        {
            pb_message_set_to_defaults((const pb_field_t *) iter.current->ptr, iter.pData);
        }
        else if (iter.current->ptr != NULL)
        {
            memcpy(iter.pData, iter.current->ptr, iter.current->data_size);
        }
        else
        {
            memset(iter.pData, 0, iter.current->data_size);
        }
    } while (pb_field_next(&iter));
}

/*********************
 * Decode all fields *
 *********************/

bool checkreturn pb_decode_noinit(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    uint8_t fields_seen[(PB_MAX_REQUIRED_FIELDS + 7) / 8] = {0}; /* Used to check for required fields */
    pb_field_iterator_t iter;
    
    pb_field_init(&iter, fields, dest_struct);
    
    while (stream->bytes_left)
    {
        uint32_t tag;
        pb_wire_type_t wire_type;
        bool eof;
        
        if (!pb_decode_tag(stream, &wire_type, &tag, &eof))
        {
            if (eof)
                break;
            else
                return false;
        }
        
        if (!pb_field_find(&iter, tag))
        {
            /* No match found, skip data */
            if (!pb_skip_field(stream, wire_type))
                return false;
            continue;
        }
        
        if (PB_HTYPE(iter.current->type) == PB_HTYPE_REQUIRED
            && iter.required_field_index < PB_MAX_REQUIRED_FIELDS)
        {
            fields_seen[iter.required_field_index >> 3] |= (uint8_t)(1 << (iter.required_field_index & 7));
        }
            
        if (!decode_field(stream, wire_type, &iter))
            return false;
    }
    
    /* Check that all required fields were present. */
    pb_field_init(&iter, fields, dest_struct);
    do {
        if (PB_HTYPE(iter.current->type) == PB_HTYPE_REQUIRED &&
            iter.required_field_index < PB_MAX_REQUIRED_FIELDS &&
            !(fields_seen[iter.required_field_index >> 3] & (1 << (iter.required_field_index & 7))))
        {
            PB_RETURN_ERROR(stream, "missing required field");
        }
    } while (pb_field_next(&iter));
    
    return true;
}

bool checkreturn pb_decode(pb_istream_t *stream, const pb_field_t fields[], void *dest_struct)
{
    pb_message_set_to_defaults(fields, dest_struct);
    return pb_decode_noinit(stream, fields, dest_struct);
}

/* Field decoders */

bool pb_decode_svarint(pb_istream_t *stream, int64_t *dest)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;
    
    if (value & 1)
        *dest = (int64_t)(~(value >> 1));
    else
        *dest = (int64_t)(value >> 1);
    
    return true;
}

bool pb_decode_fixed32(pb_istream_t *stream, void *dest)
{
    #ifdef __BIG_ENDIAN__
    uint8_t *bytes = (uint8_t*)dest;
    uint8_t lebytes[4];
    
    if (!pb_read(stream, lebytes, 4))
        return false;
    
    bytes[0] = lebytes[3];
    bytes[1] = lebytes[2];
    bytes[2] = lebytes[1];
    bytes[3] = lebytes[0];
    return true;
    #else
    return pb_read(stream, (uint8_t*)dest, 4);
    #endif   
}

bool pb_decode_fixed64(pb_istream_t *stream, void *dest)
{
    #ifdef __BIG_ENDIAN__
    uint8_t *bytes = (uint8_t*)dest;
    uint8_t lebytes[8];
    
    if (!pb_read(stream, lebytes, 8))
        return false;
    
    bytes[0] = lebytes[7];
    bytes[1] = lebytes[6];
    bytes[2] = lebytes[5];
    bytes[3] = lebytes[4];
    bytes[4] = lebytes[3];
    bytes[5] = lebytes[2];
    bytes[6] = lebytes[1];
    bytes[7] = lebytes[0];
    return true;
    #else
    return pb_read(stream, (uint8_t*)dest, 8);
    #endif   
}

bool checkreturn pb_dec_varint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint64_t value;
    bool status = pb_decode_varint(stream, &value);
    
    switch (field->data_size)
    {
        case 1: *(uint8_t*)dest = (uint8_t)value; break;
        case 2: *(uint16_t*)dest = (uint16_t)value; break;
        case 4: *(uint32_t*)dest = (uint32_t)value; break;
        case 8: *(uint64_t*)dest = value; break;
        default: PB_RETURN_ERROR(stream, "invalid data_size");
    }
    
    return status;
}

bool checkreturn pb_dec_svarint(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    int64_t value;
    bool status = pb_decode_svarint(stream, &value);
    
    switch (field->data_size)
    {
        case 4: *(int32_t*)dest = (int32_t)value; break;
        case 8: *(int64_t*)dest = value; break;
        default: PB_RETURN_ERROR(stream, "invalid data_size");
    }
    
    return status;
}

bool checkreturn pb_dec_fixed32(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    UNUSED(field);
    return pb_decode_fixed32(stream, dest);
}

bool checkreturn pb_dec_fixed64(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    UNUSED(field);
    return pb_decode_fixed64(stream, dest);
}

bool checkreturn pb_dec_bytes(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    pb_bytes_array_t *x = (pb_bytes_array_t*)dest;
    
    uint32_t temp;
    if (!pb_decode_varint32(stream, &temp))
        return false;
    x->size = temp;
    
    /* Check length, noting the space taken by the size_t header. */
    if (x->size > field->data_size - offsetof(pb_bytes_array_t, bytes))
        PB_RETURN_ERROR(stream, "bytes overflow");
    
    return pb_read(stream, x->bytes, x->size);
}

bool checkreturn pb_dec_string(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    uint32_t size;
    bool status;
    if (!pb_decode_varint32(stream, &size))
        return false;
    
    /* Check length, noting the null terminator */
    if (size + 1 > field->data_size)
        PB_RETURN_ERROR(stream, "string overflow");
    
    status = pb_read(stream, (uint8_t*)dest, size);
    *((uint8_t*)dest + size) = 0;
    return status;
}

bool checkreturn pb_dec_submessage(pb_istream_t *stream, const pb_field_t *field, void *dest)
{
    bool status;
    pb_istream_t substream;
    const pb_field_t* submsg_fields = (const pb_field_t*)field->ptr;
    
    if (!pb_make_string_substream(stream, &substream))
        return false;
    
    if (field->ptr == NULL)
        PB_RETURN_ERROR(stream, "invalid field descriptor");
    
    /* New array entries need to be initialized, while required and optional
     * submessages have already been initialized in the top-level pb_decode. */
    if (PB_HTYPE(field->type) == PB_HTYPE_ARRAY)
        status = pb_decode(&substream, submsg_fields, dest);
    else
        status = pb_decode_noinit(&substream, submsg_fields, dest);
    
    pb_close_string_substream(stream, &substream);
    return status;
}
