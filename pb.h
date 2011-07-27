#ifndef _PB_H_
#define _PB_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __GNUC__
/* This just reduces memory requirements, but is not required. */
#define pb_packed __attribute__((packed))
#else
#define pb_packed
#endif

/* Lightweight output stream. */
typedef struct _pb_ostream_t pb_ostream_t;
struct _pb_ostream_t
{
    bool (*callback)(pb_ostream_t *stream, const uint8_t *buf, size_t count);
    void *state; /* Free field for use by callback implementation */
    size_t bytes_written;
};

/*static inline bool pb_write(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    bool status = stream->callback(stream, buf, count);
    stream->bytes_written += count;
    return status;
}*/

/* List of possible field types
 * Least-significant 4 bits tell the scalar type
 * Most-significant 4 bits specify repeated/required/packed etc.
 * 
 * INT32 and UINT32 are treated the same, as are (U)INT64 and (S)FIXED*
 * These types are simply casted to correct field type when they are
 * assigned to the memory pointer.
 * SINT* is different, though, because it is zig-zag coded.
 */

typedef enum {
    /************************
     * Field contents types *
     ************************/
    
    /* Standard integer types */
    PB_LTYPE_UINT32 = 0x00,
    PB_LTYPE_INT32 = 0x00,
    PB_LTYPE_SINT32 = 0x01,
    PB_LTYPE_FIXED32 = 0x02,
    PB_LTYPE_SFIXED32 = 0x02,
    PB_LTYPE_UINT64 = 0x03,
    PB_LTYPE_INT64 = 0x03,
    PB_LTYPE_SINT64 = 0x04,
    PB_LTYPE_FIXED64 = 0x05,
    PB_LTYPE_SFIXED64 = 0x05,
    PB_LTYPE_BOOL = 0x06,
    PB_LTYPE_ENUM = 0x07,
    
    /* Standard float types */
    PB_LTYPE_FLOAT = 0x08,
    PB_LTYPE_DOUBLE = 0x09,
    
    /* Byte array with pre-allocated buffer.
     * data_size is the length of the allocated PB_BYTES_ARRAY structure. */
    PB_LTYPE_BYTES = 0x0A,
    
    /* String with pre-allocated buffer.
     * data_size is the maximum length. */
    PB_LTYPE_STRING = 0x0B,
    
    /* Submessage
     * submsg_fields is pointer to field descriptions */
    PB_LTYPE_SUBMESSAGE = 0x0C,
    
    /******************
     * Modifier flags *
     ******************/
    
    /* Just the basic, write data at data_offset */
    PB_HTYPE_REQUIRED = 0x00,
    
    /* Write true at size_offset */
    PB_HTYPE_OPTIONAL = 0x10,
    
    /* Read to pre-allocated array
     * Maximum number of entries is array_size,
     * actual number is stored at size_offset */
    PB_HTYPE_ARRAY = 0x20,
    
    /* Works for all required/optional/repeated fields.
     * data_offset points to pb_callback_t structure.
     * LTYPE is ignored. */
    PB_HTYPE_CALLBACK = 0x30
} pb_packed pb_type_t;

#define PB_HTYPE(x) ((x) & 0xF0)
#define PB_LTYPE(x) ((x) & 0x0F)

/* This structure is used in auto-generated constants
 * to specify struct fields.
 * You can change field sizes here if you need structures
 * larger than 256 bytes or field tags larger than 256.
 * The compiler should complain if your .proto has such
 * structures ("initializer too large for type").
 */
typedef struct _pb_field_t pb_field_t;
struct _pb_field_t {
    uint8_t tag;
    pb_type_t type;
    uint8_t data_offset; /* Offset of actual data or array start */
    uint8_t size_offset; /* Offset of array size or has-boolean */
    uint8_t data_size; /* Data size in bytes for a single item */
    uint8_t array_size; /* Maximum number of entries in array */
    
    /* Field definitions for submessage
     * OR default value for all other non-array, non-callback types
     * If null, then field will zeroed. */
    const void *ptr;
} pb_packed;

#define PB_LAST_FIELD {0,0,0,0}

/* This structure is used for 'bytes' arrays.
 * It has the number of bytes in the beginning, and after that an array. */
#define PB_BYTES_ARRAY(buffersize) \
struct { \
    size_t size; \
    uint8_t bytes[buffersize]; \
}

typedef PB_BYTES_ARRAY(1) pb_bytes_array_t;

/* This structure is used for giving the callback function.
 * It is stored in the message structure and filled in by the method that
 * calls pb_decode.
 *
 * The decoding callback will be given a limited-length stream
 * If the wire type was string, the length is the length of the string.
 * If the wire type was a varint/fixed32/fixed64, the length is the length
 * of the actual value.
 * The function may be called multiple times (especially for repeated types,
 * but also otherwise if the message happens to contain the field multiple
 * times.)
 *
 * The encoding callback will receive the actual output stream.
 * It should write all the data in one call, including the field tag and
 * wire type. It can write multiple fields.
 */
typedef struct _pb_istream_t pb_istream_t;
typedef struct _pb_callback_t pb_callback_t;
struct _pb_callback_t {
    union {
        bool (*decode)(pb_istream_t *stream, const pb_field_t *field, void *arg);
        bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, void *arg);
    } funcs;
    
    /* Free arg for use by callback */
    void *arg;
};

#endif