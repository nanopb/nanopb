#ifndef PB_DECODE_FAST_H_INCLUDED
#define PB_DECODE_FAST_H_INCLUDED

#ifdef PB_ENABLE_DECODE_FAST

#include <pb.h>
#include <pb_decode.h>

/* -- RELEASE CALLBACK FIELDS --------------------------------------------------------------------------------------- */

#define PB_RELEASE_CALLBACK(structname, fieldname, atype, htype, ltype)

/* -- RELEASE POINTER FIELDS ---------------------------------------------------------------------------------------- */

#define PB_RELEASE_POINTER_FIXARRAY(structname, fieldname, atype, htype, ltype)                                        \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_FIXARRAY")

#define PB_RELEASE_POINTER_REPEATED_FIXED_LENGTH_BYTES(structname, fieldname, atype, htype, ltype)                     \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_REPEATED_FIXED_LENGTH_BYTES")

#define PB_RELEASE_POINTER_REPEATED_EXTENSION(structname, fieldname, atype, htype, ltype)                              \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_REPEATED_EXTENSION")

#define PB_RELEASE_POINTER_REPEATED_SUBMSG_W_CB(structname, fieldname, atype, htype, ltype)                            \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_REPEATED_SUBMSG_W_CB")

#define PB_RELEASE_POINTER_REPEATED_SUBMESSAGE(structname, fieldname, atype, htype, ltype)                             \
    do {                                                                                                               \
      for (pb_size_t i = 0; i < msg->fieldname ## _count; i++) {                                                       \
        PB_CONCAT_EXPAND(pb_release_, structname ## _ ## fieldname ## _MSGTYPE)(&msg->fieldname[i]);                   \
      }                                                                                                                \
    } while (0)

#define PB_RELEASE_POINTER_REPEATED_STRING(structname, fieldname, atype, htype, ltype)                                 \
    do {                                                                                                               \
      for (pb_size_t i = 0; i < msg->fieldname ## _count; i++) {                                                       \
        pb_free(msg->fieldname[i]);                                                                                    \
        msg->fieldname[i] = NULL;                                                                                      \
      }                                                                                                                \
    } while (0)

#define PB_RELEASE_POINTER_REPEATED_BYTES(structname, fieldname, atype, htype, ltype)                                  \
    do {                                                                                                               \
      for (pb_size_t i = 0; i < msg->fieldname ## _count; i++) {                                                       \
        pb_free(msg->fieldname[i]);                                                                                    \
        msg->fieldname[i] = NULL;                                                                                      \
      }                                                                                                                \
    } while (0)

#define PB_RELEASE_POINTER_REPEATED_FIXED64(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED_FIXED32(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED_SVARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED_UVARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED_VARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED_BOOL(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_REPEATED(structname, fieldname, atype, htype, ltype)                                        \
    PB_CONCAT_EXPAND(PB_RELEASE_POINTER_REPEATED_, PB_LTYPE_MAP_ ## ltype)(                                            \
        structname, fieldname, atype, htype, ltype);                                                                   \
    msg->fieldname ## _count = 0;

#define PB_RELEASE_POINTER_OPTIONAL_FIXED_LENGTH_BYTES(structname, fieldname, atype, htype, ltype)                     \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_OPTIONAL_FIXED_LENGTH_BYTES")

#define PB_RELEASE_POINTER_OPTIONAL_EXTENSION(structname, fieldname, atype, htype, ltype)                              \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_OPTIONAL_EXTENSION")

#define PB_RELEASE_POINTER_OPTIONAL_SUBMSG_W_CB(structname, fieldname, atype, htype, ltype)                            \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_OPTIONAL_SUBMSG_W_CB")

#define PB_RELEASE_POINTER_OPTIONAL_SUBMESSAGE(structname, fieldname, atype, htype, ltype)                             \
    PB_CONCAT_EXPAND(pb_release_, structname ## _ ## fieldname ## _MSGTYPE)(msg->fieldname)

#define PB_RELEASE_POINTER_OPTIONAL_STRING(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_BYTES(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_FIXED64(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_FIXED32(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_SVARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_UVARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_VARINT(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL_BOOL(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_POINTER_OPTIONAL(structname, fieldname, atype, htype, ltype)                                        \
    PB_CONCAT_EXPAND(PB_RELEASE_POINTER_OPTIONAL_, PB_LTYPE_MAP_ ## ltype)(                                            \
        structname, fieldname, atype, htype, ltype);                                                                   \

#define PB_RELEASE_POINTER_ONEOF(structname, fieldname, atype, htype, ltype)                                           \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_ONEOF")

#define PB_RELEASE_POINTER_SINGULAR(structname, fieldname, atype, htype, ltype)                                        \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_SINGULAR")

#define PB_RELEASE_POINTER_REQUIRED(structname, fieldname, atype, htype, ltype)                                        \
    PB_RETURN_ERROR(stream, "FIXME PB_RELEASE_POINTER_REQUIRED")

#define PB_RELEASE_POINTER(structname, fieldname, atype, htype, ltype)                                                 \
    PB_RELEASE_POINTER_ ## htype(structname, fieldname, atype, htype, ltype);                                          \
    pb_free(msg->fieldname);                                                                                           \
    msg->fieldname = NULL;

/* -- RELEASE STATIC FIELDS ----------------------------------------------------------------------------------------- */

#define PB_RELEASE_STATIC_FIXARRAY(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_STATIC_REPEATED(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_STATIC_OPTIONAL(structname, fieldname, atype, htype, ltype)                                         \
    msg->has_ ## fieldname = true;

#define PB_RELEASE_STATIC_ONEOF(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_STATIC_SINGULAR(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_STATIC_REQUIRED(structname, fieldname, atype, htype, ltype)

#define PB_RELEASE_STATIC(structname, fieldname, atype, htype, ltype)                                                  \
    PB_RELEASE_STATIC_ ## htype(structname, fieldname, atype, htype, ltype);

/* -- RELEASE FUNCTIONS --------------------------------------------------------------------------------------------- */

#define PB_GENERATE_RELEASE_FIELD(structname, atype, htype, ltype, fieldname, tag)                                     \
    PB_RELEASE_ ## atype(structname, fieldname, atype, htype, ltype)

#define PB_GENERATE_RELEASE(structname)                                                                                \
    void PB_RELEASE(structname)(structname *msg)                                                                       \
    {                                                                                                                  \
        structname ## _FIELDLIST(PB_GENERATE_RELEASE_FIELD, structname)                                                \
    }

#define PB_RELEASE(structname) pb_release_##structname

/* -- DECODE VALUES TO POINTER -------------------------------------------------------------------------------------- */

#define PB_DECODE_SUBMESSAGE(structname, fieldname, ptr)                                                               \
    do {                                                                                                               \
        pb_istream_t substream;                                                                                        \
        bool res;                                                                                                      \
                                                                                                                       \
        if (!pb_make_string_substream(stream, &substream))                                                             \
            return false;                                                                                              \
                                                                                                                       \
        res = PB_CONCAT_EXPAND(pb_decode_, structname ## _ ## fieldname ## _MSGTYPE)(&substream, ptr);                 \
                                                                                                                       \
        if (!pb_close_string_substream(stream, &substream))                                                            \
            return false;                                                                                              \
                                                                                                                       \
        if (!res)                                                                                                      \
            return false;                                                                                              \
    } while (0)

#define PB_DECODE_FIXED64(ptr)                                                                                         \
    if (!pb_decode_fixed64(stream, ptr)) return false

#define PB_DECODE_FIXED32(ptr)                                                                                         \
    if (!pb_decode_fixed32(stream, ptr)) return false

#define PB_DECODE_SVARINT(ptr)                                                                                         \
    if (!pb_decode_svarint(stream, ptr)) return false

#define PB_DECODE_VARINT(ptr)                                                                                          \
    if (!pb_decode_varint(stream, ptr)) return false

#define PB_DECODE_BOOL(ptr)                                                                                            \
    if (!pb_decode_bool(stream, ptr)) return false

#define PB_DECODE_VARINT32(ptr)                                                                                        \
    if (!pb_decode_varint32(stream, ptr)) return false

#define PB_DECODE_SKIP()                                                                                               \
    if (!pb_skip_field(stream, wire_type)) return false

/* -- DECODE VALUES TO MESSAGE FIELD -------------------------------------------------------------------------------- */

#define PB_DECODE_CHECK_OVERFLOW(a, b)                                                                                 \
    if (a != b) PB_RETURN_ERROR(stream, "integer too large")

#define PB_DECODE_CHECK_SIZE(size)                                                                                     \
    if (size > PB_SIZE_MAX) PB_RETURN_ERROR(stream, "bytes overflow")

#define PB_DECODE_CHECK_WIRETYPE(expected)                                                                             \
    if (wire_type != expected) PB_RETURN_ERROR(stream, "wrong wire type")

#define PB_DECODE_BASIC_FIXED_LENGTH_BYTES(structname, fieldname, field, length)                                       \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_BASIC_FIXED_LENGTH_BYTES")

#define PB_DECODE_BASIC_EXTENSION(structname, fieldname, field, length)                                                \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_BASIC_EXTENSION")

#define PB_DECODE_BASIC_SUBMSG_W_CB(structname, fieldname, field, length)                                              \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_BASIC_SUBMSG_W_CB")

#define PB_DECODE_BASIC_SUBMESSAGE(structname, fieldname, field, length)                                               \
    do {                                                                                                               \
        PB_DECODE_SUBMESSAGE(structname, fieldname, &field);                                                           \
    } while (0)

#define PB_DECODE_BASIC_STRING(structname, fieldname, field, length)                                                   \
    do {                                                                                                               \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_STRING);                                                                        \
        PB_DECODE_CHECK_SIZE(length);                                                                                  \
        field[length] = 0;                                                                                             \
        if (!pb_read(stream, (pb_byte_t *)field, length))                                                              \
            return false;                                                                                              \
    } while (0)

#define PB_DECODE_BASIC_BYTES(structname, fieldname, field, length)                                                    \
    do {                                                                                                               \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_STRING);                                                                        \
        PB_DECODE_CHECK_SIZE(length);                                                                                  \
        field.si##ze = length;                                                                                         \
        if (!pb_read(stream, field.bytes, length))                                                                     \
            return false;                                                                                              \
    } while (0)

#define PB_DECODE_BASIC_FIXED64(structname, fieldname, field, length)                                                  \
    do {                                                                                                               \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_64BIT);                                                                         \
        PB_DECODE_FIXED64(&field);                                                                                     \
    } while (0)

#define PB_DECODE_BASIC_FIXED32(structname, fieldname, field, length)                                                  \
    do {                                                                                                               \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_32BIT);                                                                         \
        PB_DECODE_FIXED32(&field);                                                                                     \
    } while (0)

#define PB_DECODE_BASIC_SVARINT(structname, fieldname, field, length)                                                  \
    do {                                                                                                               \
        int64_t value;                                                                                                 \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_VARINT);                                                                        \
        PB_DECODE_SVARINT(&value);                                                                                     \
        field = value;                                                                                                 \
        PB_DECODE_CHECK_OVERFLOW(field, value);                                                                        \
    } while (0)

#define PB_DECODE_BASIC_UVARINT(structname, fieldname, field, length)                                                  \
    do {                                                                                                               \
        uint64_t value;                                                                                                \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_VARINT);                                                                        \
        PB_DECODE_VARINT(&value);                                                                                      \
        field = value;                                                                                                 \
        PB_DECODE_CHECK_OVERFLOW(field, value);                                                                        \
    } while (0)

#define PB_DECODE_BASIC_VARINT(structname, fieldname, field, length)                                                   \
    do {                                                                                                               \
        union { uint64_t u; int64_t s; } value;                                                                        \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_VARINT);                                                                        \
        PB_DECODE_VARINT(&value.u);                                                                                    \
        field = value.s;                                                                                               \
        PB_DECODE_CHECK_OVERFLOW(field, value.s);                                                                      \
    } while (0)

#define PB_DECODE_BASIC_BOOL(structname, fieldname, field, length)                                                     \
    do {                                                                                                               \
        PB_DECODE_CHECK_WIRETYPE(PB_WT_VARINT);                                                                        \
        PB_DECODE_BOOL(&field);                                                                                        \
    } while (0)

#define PB_DECODE_BASIC(structname, fieldname, ltype, field, length)                                                   \
    PB_CONCAT_EXPAND(PB_DECODE_BASIC_, PB_LTYPE_MAP_ ## ltype)(                                                        \
        structname, fieldname, field, length)

/* -- DECODE CALLBACK FIELDS ---------------------------------------------------------------------------------------- */

#define PB_DECODE_CALLBACK(structname, fieldname, atype, htype, ltype)                                                 \
    PB_DECODE_SKIP();

/* -- DECODE POINTER FIELDS ----------------------------------------------------------------------------------------- */

#define PB_DECODE_POINTER_FIXARRAY(structname, fieldname, atype, htype, ltype)                                         \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_POINTER_FIXARRAY")

#define PB_DECODE_POINTER_REPEATED_FIXED_LENGTH_BYTES(structname, fieldname, subscript, atype, htype, ltype)           \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_EXTENSION(structname, fieldname, subscript, atype, htype, ltype)                    \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_SUBMSG_W_CB(structname, fieldname, subscript, atype, htype, ltype)                  \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_SUBMESSAGE(structname, fieldname, subscript, atype, htype, ltype)                   \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_STRING(structname, fieldname, subscript, atype, htype, ltype)                       \
    PB_DECODE_POINTER_OPTIONAL_STRING(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_BYTES(structname, fieldname, subscript, atype, htype, ltype)                        \
    PB_DECODE_POINTER_OPTIONAL_BYTES(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_FIXED64(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_FIXED32(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_SVARINT(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_UVARINT(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_VARINT(structname, fieldname, subscript, atype, htype, ltype)                       \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_BOOL(structname, fieldname, subscript, atype, htype, ltype)                         \
    PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_REPEATED_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_BASIC(structname, fieldname, ltype, msg->fieldname subscript, 0)

#define PB_DECODE_POINTER_REPEATED(structname, fieldname, atype, htype, ltype)                                         \
    do {                                                                                                               \
      size_t data_size = PB_DATA_SIZE_ ## atype(_PB_HTYPE_ ## htype, structname, fieldname);                           \
      pb_size_t old_count = msg->fieldname ## _count;                                                                  \
      pb_size_t new_count = old_count + 1;                                                                             \
                                                                                                                       \
      msg->fieldname ## _count = new_count;                                                                            \
      msg->fieldname = pb_realloc(msg->fieldname, data_size * new_count);                                              \
      memset(&msg->fieldname[old_count], 0, data_size);                                                                \
                                                                                                                       \
      PB_CONCAT_EXPAND(PB_DECODE_POINTER_REPEATED_, PB_LTYPE_MAP_ ## ltype)(                                           \
          structname, fieldname, [old_count], atype, htype, ltype);                                                    \
    } while (0)

#define PB_DECODE_POINTER_OPTIONAL_FIXED_LENGTH_BYTES(structname, fieldname, subscript, atype, htype, ltype)           \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_EXTENSION(structname, fieldname, subscript, atype, htype, ltype)                    \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_SUBMSG_W_CB(structname, fieldname, subscript, atype, htype, ltype)                  \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_SUBMESSAGE(structname, fieldname, subscript, atype, htype, ltype)                   \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_STRING(structname, fieldname, subscript, atype, htype, ltype)                       \
    do {                                                                                                               \
      uint32_t tmp;                                                                                                    \
                                                                                                                       \
      if (!pb_decode_varint32(stream, &tmp))                                                                           \
          return false;                                                                                                \
                                                                                                                       \
      size_t length = (size_t)tmp;                                                                                     \
      size_t size = length + 1;                                                                                        \
                                                                                                                       \
      if (size < length)                                                                                               \
          PB_RETURN_ERROR(stream, "size too large");                                                                   \
                                                                                                                       \
      msg->fieldname subscript = pb_realloc(msg->fieldname subscript, size);                                           \
      memset(msg->fieldname subscript, 0, size);                                                                       \
      PB_DECODE_BASIC(structname, fieldname, ltype, msg->fieldname subscript, length);                                 \
    } while (0)

#define PB_DECODE_POINTER_OPTIONAL_BYTES(structname, fieldname, subscript, atype, htype, ltype)                        \
    do {                                                                                                               \
      uint32_t tmp;                                                                                                    \
                                                                                                                       \
      if (!pb_decode_varint32(stream, &tmp))                                                                           \
          return false;                                                                                                \
                                                                                                                       \
      size_t length = (size_t)tmp;                                                                                     \
      size_t size = PB_BYTES_ARRAY_T_ALLOCSIZE(length);                                                                \
                                                                                                                       \
      if (size < length)                                                                                               \
          PB_RETURN_ERROR(stream, "size too large");                                                                   \
                                                                                                                       \
      msg->fieldname subscript = pb_realloc(msg->fieldname subscript, size);                                           \
      memset(msg->fieldname subscript, 0, size);                                                                       \
      PB_DECODE_BASIC(structname, fieldname, ltype, (*(msg->fieldname subscript)), length);                            \
    } while (0)

#define PB_DECODE_POINTER_OPTIONAL_FIXED64(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_FIXED32(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_SVARINT(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_UVARINT(structname, fieldname, subscript, atype, htype, ltype)                      \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_VARINT(structname, fieldname, subscript, atype, htype, ltype)                       \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_BOOL(structname, fieldname, subscript, atype, htype, ltype)                         \
    PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)

#define PB_DECODE_POINTER_OPTIONAL_DEFAULT(structname, fieldname, subscript, atype, htype, ltype)                      \
    do {                                                                                                               \
        pb_size_t data_size = sizeof(*(msg->fieldname subscript));                                                     \
        msg->fieldname subscript = pb_realloc(msg->fieldname subscript, data_size);                                    \
        memset(msg->fieldname subscript, 0, data_size);                                                                \
        PB_DECODE_BASIC(structname, fieldname, ltype, (*(msg->fieldname subscript)), 0);                               \
    } while (0)

#define PB_DECODE_POINTER_OPTIONAL(structname, fieldname, atype, htype, ltype)                                         \
    PB_CONCAT_EXPAND(PB_DECODE_POINTER_OPTIONAL_, PB_LTYPE_MAP_ ## ltype)(                                             \
      structname, fieldname, , atype, htype, ltype)

#define PB_DECODE_POINTER_ONEOF(structname, fieldname, atype, htype, ltype)                                            \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_POINTER_ONEOF")

#define PB_DECODE_POINTER_SINGULAR(structname, fieldname, atype, htype, ltype)                                         \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_POINTER_SINGULAR")

#define PB_DECODE_POINTER_REQUIRED(structname, fieldname, atype, htype, ltype)                                         \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_POINTER_REQUIRED")

#define PB_DECODE_POINTER(structname, fieldname, atype, htype, ltype)                                                  \
    PB_DECODE_POINTER_ ## htype(structname, fieldname, atype, htype, ltype)

/* -- DECODE STATIC FIELDS ------------------------------------------------------------------------------------------ */

#define PB_DECODE_STATIC_FIXARRAY(structname, fieldname, atype, htype, ltype)                                          \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_STATIC_FIXARRAY")

#define PB_DECODE_STATIC_REPEATED(structname, fieldname, atype, htype, ltype)                                          \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_STATIC_REPEATED")

#define PB_DECODE_STATIC_OPTIONAL(structname, fieldname, atype, htype, ltype)                                          \
    do {                                                                                                               \
      PB_DECODE_BASIC(structname, fieldname, ltype, msg->fieldname, 0);                                                \
      msg->has_ ## fieldname = true;                                                                                   \
    } while (0)

#define PB_DECODE_STATIC_ONEOF(structname, fieldname, atype, htype, ltype)                                             \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_STATIC_ONEOF")

#define PB_DECODE_STATIC_SINGULAR(structname, fieldname, atype, htype, ltype)                                          \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_STATIC_SINGULAR")

#define PB_DECODE_STATIC_REQUIRED(structname, fieldname, atype, htype, ltype)                                          \
    PB_RETURN_ERROR(stream, "FIXME PB_DECODE_STATIC_REQUIRED")

#define PB_DECODE_STATIC(structname, fieldname, atype, htype, ltype)                                                   \
    PB_DECODE_STATIC_ ## htype(structname, fieldname, atype, htype, ltype)

/* -- DECODE FUNCTIONS ---------------------------------------------------------------------------------------------- */

#define PB_GENERATE_DECODE_FIELD(structname, atype, htype, ltype, fieldname, tag)                                      \
    case tag: {                                                                                                        \
        PB_DECODE_ ## atype(structname, fieldname, atype, htype, ltype);                                               \
    } break;

#define PB_GENERATE_DECODE(structname)                                                                                 \
    bool PB_DECODE(structname)(pb_istream_t *stream, structname *msg)                                                  \
    {                                                                                                                  \
        pb_wire_type_t wire_type;                                                                                      \
        uint32_t tag;                                                                                                  \
        bool eof;                                                                                                      \
                                                                                                                       \
        while (pb_decode_tag(stream, &wire_type, &tag, &eof))                                                          \
        {                                                                                                              \
            switch (tag)                                                                                               \
            {                                                                                                          \
                structname ## _FIELDLIST(PB_GENERATE_DECODE_FIELD, structname)                                         \
                                                                                                                       \
                default:                                                                                               \
                    PB_DECODE_SKIP();                                                                                  \
                    break;                                                                                             \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        return eof;                                                                                                    \
    }

#define PB_DECODE(structname) pb_decode_##structname

#endif

#endif
