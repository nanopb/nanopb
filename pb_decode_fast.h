#ifndef PB_DECODE_FAST_H_INCLUDED
#define PB_DECODE_FAST_H_INCLUDED

#ifdef PB_DECODE_FAST

#define PB_DECODE_INTERNAL

#include "pb_common.h"
#include "pb_decode.h"

/* internal flags that extend the ones supplied to pb_decode */
#define PB_DECODE_ISINIT      0x10U
#define PB_DECODE_ISDECODE    0x20U
#define PB_DECODE_ISEXTENSION 0x40U

/* -- DECODE INTERFACE ------------------------------------------------------------------------------------------------------------------ */

#ifdef PB_DYNAMIC_DECODE_FAST

extern struct pb_decode_interface_s pb_dec_if;

#define PB_ISTREAM_FROM_BUFFER(_1, _2)           pb_dec_if.istream_from_buffer(_1, _2)
#define PB_MAKE_STRING_SUBSTREAM(_1, _2)         pb_dec_if.make_string_substream(_1, _2)
#define PB_CLOSE_STRING_SUBSTREAM(_1, _2)        pb_dec_if.close_string_substream(_1, _2)
#define PB_DECODE_CALLBACK_FIELD(_1, _2, _3)     pb_dec_if.decode_callback_field(_1, _2, _3)
#define PB_DECODE_TAG(_1, _2, _3, _4)            pb_dec_if.decode_tag(_1, _2, _3, _4)
#define PB_DECODE_VARINT32(_1, _2)               pb_dec_if.decode_varint32(_1, _2)
#define PB_DECODE_BOOL(_1, _2)                   pb_dec_if.dec_bool(_1, _2)
#define PB_DECODE_VARINT(_1, _2, _3)             pb_dec_if.dec_varint(_1, _2, _3)
#define PB_DECODE_UVARINT(_1, _2, _3)            pb_dec_if.dec_uvarint(_1, _2, _3)
#define PB_DECODE_SVARINT(_1, _2, _3)            pb_dec_if.dec_svarint(_1, _2, _3)
#define PB_DECODE_FIXED32(_1, _2)                pb_dec_if.dec_fixed32(_1, _2)
#define PB_DECODE_FIXED64(_1, _2, _3)            pb_dec_if.dec_fixed64(_1, _2, _3)
#define PB_DECODE_BYTES(_1, _2, _3)              pb_dec_if.dec_bytes(_1, _2, _3)
#define PB_DECODE_STRING(_1, _2, _3)             pb_dec_if.dec_string(_1, _2, _3)
#define PB_DECODE_SUBMESSAGE(_1, _2, _3)         pb_dec_if.dec_submessage(_1, _2, _3)
#define PB_DECODE_FIXED_LENGTH_BYTES(_1, _2, _3) pb_dec_if.dec_fixed_length_bytes(_1, _2, _3)
#define PB_SKIP_FIELD(_1, _2)                    pb_dec_if.skip_field(_1, _2)
#define PB_ALLOC_FIELD(_1, _2, _3, _4)           pb_dec_if.alloc_field(_1, _2, _3, _4)
#define PB_FREE_FIELD(_1)                        pb_dec_if.free_field(_1)

#else

#define PB_ISTREAM_FROM_BUFFER(_1, _2)           pb_istream_from_buffer(_1, _2)
#define PB_MAKE_STRING_SUBSTREAM(_1, _2)         pb_make_string_substream(_1, _2)
#define PB_CLOSE_STRING_SUBSTREAM(_1, _2)        pb_close_string_substream(_1, _2)
#define PB_DECODE_CALLBACK_FIELD(_1, _2, _3)     decode_callback_field(_1, _2, _3)
#define PB_DECODE_TAG(_1, _2, _3, _4)            pb_decode_tag(_1, _2, _3, _4)
#define PB_DECODE_VARINT32(_1, _2)               pb_decode_varint32(_1, _2)
#define PB_DECODE_BOOL(_1, _2)                   pb_dec_bool(_1, _2)
#define PB_DECODE_VARINT(_1, _2, _3)             pb_dec_varint(_1, _2, _3)
#define PB_DECODE_UVARINT(_1, _2, _3)            pb_dec_uvarint(_1, _2, _3)
#define PB_DECODE_SVARINT(_1, _2, _3)            pb_dec_svarint(_1, _2, _3)
#define PB_DECODE_FIXED32(_1, _2)                pb_dec_fixed32(_1, _2)
#define PB_DECODE_FIXED64(_1, _2, _3)            pb_dec_fixed64(_1, _2, _3)
#define PB_DECODE_BYTES(_1, _2, _3)              pb_dec_bytes(_1, _2, _3)
#define PB_DECODE_STRING(_1, _2, _3)             pb_dec_string(_1, _2, _3)
#define PB_DECODE_SUBMESSAGE(_1, _2, _3)         pb_dec_submessage(_1, _2, _3)
#define PB_DECODE_FIXED_LENGTH_BYTES(_1, _2, _3) pb_dec_fixed_length_bytes(_1, _2, _3)
#define PB_SKIP_FIELD(_1, _2)                    pb_skip_field(_1, _2)
#define PB_ALLOC_FIELD(_1, _2, _3, _4)           pb_alloc_field(_1, _2, _3, _4)
#define PB_FREE_FIELD(_1)                        pb_free_field(_1)

#endif

/* -- FIELD HELPERS --------------------------------------------------------------------------------------------------------------------- */

#define PB_MSGDESC(structname, fieldname, htype)                             (&PB_CONCAT(PB_MSGTYPE(structname, fieldname, htype), _msg))

#define PB_MSGTYPE_HTYPE_ONEOF1(unionname, membername, fullname)             unionname ## _ ## membername ## _MSGTYPE
#define PB_MSGTYPE_HTYPE_ONEOF(structname, fieldname)                        PB_CONCAT(structname ## _, PB_MSGTYPE_HTYPE_ONEOF1 fieldname)
#define PB_MSGTYPE_HTYPE_FIXARRAY(structname, fieldname)                     structname ## _ ## fieldname ## _MSGTYPE
#define PB_MSGTYPE_HTYPE_REPEATED(structname, fieldname)                     structname ## _ ## fieldname ## _MSGTYPE
#define PB_MSGTYPE_HTYPE_SINGULAR(structname, fieldname)                     structname ## _ ## fieldname ## _MSGTYPE
#define PB_MSGTYPE_HTYPE_OPTIONAL(structname, fieldname)                     structname ## _ ## fieldname ## _MSGTYPE
#define PB_MSGTYPE_HTYPE_REQUIRED(structname, fieldname)                     structname ## _ ## fieldname ## _MSGTYPE
#define PB_MSGTYPE(structname, fieldname, htype)                             PB_MSGTYPE_ ## htype(structname, fieldname)

#define PB_ARRAYSIZE_ATYPE_CALLBACK(structname, fieldname, htype)            0
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_ONEOF(structname, fieldname)        0
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_FIXARRAY(structname, fieldname)     pb_arraysize(structname, fieldname[0])
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_REPEATED(structname, fieldname)     0
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_SINGULAR(structname, fieldname)     0
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_OPTIONAL(structname, fieldname)     0
#define PB_ARRAYSIZE_ATYPE_POINTER_HTYPE_REQUIRED(structname, fieldname)     0
#define PB_ARRAYSIZE_ATYPE_POINTER(structname, fieldname, htype)             PB_ARRAYSIZE_ATYPE_POINTER_ ## htype(structname, fieldname)
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_ONEOF(structname, fieldname)         0
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_FIXARRAY(structname, fieldname)      pb_arraysize(structname, fieldname)
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_REPEATED(structname, fieldname)      pb_arraysize(structname, fieldname)
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_SINGULAR(structname, fieldname)      0
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_OPTIONAL(structname, fieldname)      0
#define PB_ARRAYSIZE_ATYPE_STATIC_HTYPE_REQUIRED(structname, fieldname)      0
#define PB_ARRAYSIZE_ATYPE_STATIC(structname, fieldname, htype)              PB_ARRAYSIZE_ATYPE_STATIC_ ## htype(structname, fieldname)
#define PB_ARRAYSIZE(structname, fieldname, atype, htype)                    PB_ARRAYSIZE_ ## atype(structname, fieldname, htype)

#define PB_FIELDSIZE_HTYPE_ONEOF1(unionname, membername, fullnbame)          which_ ## unionname
#define PB_FIELDSIZE_HTYPE_ONEOF(fieldname, atype)                           PB_FIELDSIZE_HTYPE_ONEOF1 fieldname
#define PB_FIELDSIZE_HTYPE_FIXARRAY(fieldname, atype)                        0
#define PB_FIELDSIZE_HTYPE_REPEATED(fieldname, atype)                        fieldname ## _count
#define PB_FIELDSIZE_HTYPE_SINGULAR(fieldname, atype)                        0
#define PB_FIELDSIZE_HTYPE_OPTIONAL_ATYPE_CALLBACK(fieldname)                0
#define PB_FIELDSIZE_HTYPE_OPTIONAL_ATYPE_POINTER(fieldname)                 0
#define PB_FIELDSIZE_HTYPE_OPTIONAL_ATYPE_STATIC(fieldname)                  has_ ## fieldname
#define PB_FIELDSIZE_HTYPE_OPTIONAL(fieldname, atype)                        PB_FIELDSIZE_HTYPE_OPTIONAL_ ## atype(fieldname)
#define PB_FIELDSIZE_HTYPE_REQUIRED(fieldname, atype)                        0
#define PB_FIELDSIZE(structname, fieldname, atype, htype)                    msg->PB_FIELDSIZE_ ## htype(fieldname, atype)

#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_ONEOF(field, ltype)                &field
#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_FIXARRAY(field, ltype)             &field
#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_REPEATED(field, ltype)             &field
#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_SINGULAR(field, ltype)             &field
#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_OPTIONAL(field, ltype)             &field
#define PB_FIELDDATA_ATYPE_CALLBACK_HTYPE_REQUIRED(field, ltype)             &field
#define PB_FIELDDATA_ATYPE_CALLBACK(field, htype, ltype)                     PB_FIELDDATA_ATYPE_CALLBACK_ ## htype(field, ltype)
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_FIXED_LENGTH_BYTES(field)     field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_UINT64(field)                 &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_UINT32(field)                 &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_STRING(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_SINT64(field)                 &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_SINT32(field)                 &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_SFIXED64(field)               &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_SFIXED32(field)               &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_MSG_W_CB(field)               &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_MESSAGE(field)                &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_INT64(field)                  &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_INT32(field)                  &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_FLOAT(field)                  &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_FIXED64(field)                &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_FIXED32(field)                &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_UENUM(field)                  &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_ENUM(field)                   &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_DOUBLE(field)                 &field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_BYTES(field)                  field
#define PB_FIELDDATA_ATYPE_POINTER_ARRAY_LTYPE_BOOL(field)                   &field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_FIXED_LENGTH_BYTES(field)    field[0]
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_UINT64(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_UINT32(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_STRING(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_SINT64(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_SINT32(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_SFIXED64(field)              field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_SFIXED32(field)              field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_MSG_W_CB(field)              field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_MESSAGE(field)               field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_INT64(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_INT32(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_FLOAT(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_FIXED64(field)               field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_FIXED32(field)               field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_UENUM(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_ENUM(field)                  field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_DOUBLE(field)                field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_BYTES(field)                 field
#define PB_FIELDDATA_ATYPE_POINTER_SCALAR_LTYPE_BOOL(field)                  field
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_ONEOF(field, ltype)                 PB_FIELDDATA_ATYPE_POINTER_SCALAR_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_FIXARRAY(field, ltype)              PB_FIELDDATA_ATYPE_POINTER_ARRAY_ ## ltype(field[0])
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_REPEATED(field, ltype)              PB_FIELDDATA_ATYPE_POINTER_ARRAY_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_SINGULAR(field, ltype)              PB_FIELDDATA_ATYPE_POINTER_SCALAR_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_OPTIONAL(field, ltype)              PB_FIELDDATA_ATYPE_POINTER_SCALAR_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_POINTER_HTYPE_REQUIRED(field, ltype)              PB_FIELDDATA_ATYPE_POINTER_SCALAR_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_POINTER(field, htype, ltype)                      PB_FIELDDATA_ATYPE_POINTER_ ## htype(field, ltype)
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_FIXED_LENGTH_BYTES(field)            field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_UINT64(field)                        &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_UINT32(field)                        &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_STRING(field)                        field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_SINT64(field)                        &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_SINT32(field)                        &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_SFIXED64(field)                      &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_SFIXED32(field)                      &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_MSG_W_CB(field)                      &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_MESSAGE(field)                       &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_INT64(field)                         &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_INT32(field)                         &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_FLOAT(field)                         &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_FIXED64(field)                       &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_FIXED32(field)                       &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_UENUM(field)                         &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_ENUM(field)                          &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_DOUBLE(field)                        &field
#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_BYTES(field)                         &field

#define PB_FIELDDATA_ATYPE_STATIC_LTYPE_BOOL(field)                          &field
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_ONEOF(field, ltype)                  PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_FIXARRAY(field, ltype)               PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_REPEATED(field, ltype)               PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_SINGULAR(field, ltype)               PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_OPTIONAL(field, ltype)               PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC_HTYPE_REQUIRED(field, ltype)               PB_FIELDDATA_ATYPE_STATIC_ ## ltype(field)
#define PB_FIELDDATA_ATYPE_STATIC(field, htype, ltype)                       PB_FIELDDATA_ATYPE_STATIC_ ## htype(field, ltype)
#define PB_FIELDDATA(structname, fieldname, atype, htype, ltype)             PB_FIELDDATA_ ## atype (PB_FIELDNAME(structname, fieldname, htype), htype, ltype)

#define PB_FIELDNAME_HTYPE_ONEOF1(unionname, membername, fullname)           fullname
#define PB_FIELDNAME_HTYPE_ONEOF(fieldname)                                  PB_FIELDNAME_HTYPE_ONEOF1 fieldname
#define PB_FIELDNAME_HTYPE_FIXARRAY(fieldname)                               fieldname
#define PB_FIELDNAME_HTYPE_REPEATED(fieldname)                               fieldname
#define PB_FIELDNAME_HTYPE_SINGULAR(fieldname)                               fieldname
#define PB_FIELDNAME_HTYPE_OPTIONAL(fieldname)                               fieldname
#define PB_FIELDNAME_HTYPE_REQUIRED(fieldname)                               fieldname
#define PB_FIELDNAME(structname, fieldname, htype)                           msg->PB_FIELDNAME_ ## htype(fieldname)

#define PB_IS_VARLEN_LTYPE_FIXED_LENGTH_BYTES                                0
#define PB_IS_VARLEN_LTYPE_EXTENSION                                         0
#define PB_IS_VARLEN_LTYPE_UINT64                                            0
#define PB_IS_VARLEN_LTYPE_UINT32                                            0
#define PB_IS_VARLEN_LTYPE_STRING                                            1
#define PB_IS_VARLEN_LTYPE_SINT64                                            0
#define PB_IS_VARLEN_LTYPE_SINT32                                            0
#define PB_IS_VARLEN_LTYPE_SFIXED64                                          0
#define PB_IS_VARLEN_LTYPE_SFIXED32                                          0
#define PB_IS_VARLEN_LTYPE_MSG_W_CB                                          0
#define PB_IS_VARLEN_LTYPE_MESSAGE                                           0
#define PB_IS_VARLEN_LTYPE_INT64                                             0
#define PB_IS_VARLEN_LTYPE_INT32                                             0
#define PB_IS_VARLEN_LTYPE_FLOAT                                             0
#define PB_IS_VARLEN_LTYPE_FIXED64                                           0
#define PB_IS_VARLEN_LTYPE_FIXED32                                           0
#define PB_IS_VARLEN_LTYPE_UENUM                                             0
#define PB_IS_VARLEN_LTYPE_ENUM                                              0
#define PB_IS_VARLEN_LTYPE_DOUBLE                                            0
#define PB_IS_VARLEN_LTYPE_BYTES                                             1
#define PB_IS_VARLEN_LTYPE_BOOL                                              0

#define PB_IS_EXTENSION_LTYPE_FIXED_LENGTH_BYTES                             0
#define PB_IS_EXTENSION_LTYPE_EXTENSION                                      1
#define PB_IS_EXTENSION_LTYPE_UINT64                                         0
#define PB_IS_EXTENSION_LTYPE_UINT32                                         0
#define PB_IS_EXTENSION_LTYPE_STRING                                         0
#define PB_IS_EXTENSION_LTYPE_SINT64                                         0
#define PB_IS_EXTENSION_LTYPE_SINT32                                         0
#define PB_IS_EXTENSION_LTYPE_SFIXED64                                       0
#define PB_IS_EXTENSION_LTYPE_SFIXED32                                       0
#define PB_IS_EXTENSION_LTYPE_MSG_W_CB                                       0
#define PB_IS_EXTENSION_LTYPE_MESSAGE                                        0
#define PB_IS_EXTENSION_LTYPE_INT64                                          0
#define PB_IS_EXTENSION_LTYPE_INT32                                          0
#define PB_IS_EXTENSION_LTYPE_FLOAT                                          0
#define PB_IS_EXTENSION_LTYPE_FIXED64                                        0
#define PB_IS_EXTENSION_LTYPE_FIXED32                                        0
#define PB_IS_EXTENSION_LTYPE_UENUM                                          0
#define PB_IS_EXTENSION_LTYPE_ENUM                                           0
#define PB_IS_EXTENSION_LTYPE_DOUBLE                                         0
#define PB_IS_EXTENSION_LTYPE_BYTES                                          0
#define PB_IS_EXTENSION_LTYPE_BOOL                                           0

#define PB_IS_MESSAGE_LTYPE_FIXED_LENGTH_BYTES                               0
#define PB_IS_MESSAGE_LTYPE_EXTENSION                                        0
#define PB_IS_MESSAGE_LTYPE_UINT64                                           0
#define PB_IS_MESSAGE_LTYPE_UINT32                                           0
#define PB_IS_MESSAGE_LTYPE_STRING                                           0
#define PB_IS_MESSAGE_LTYPE_SINT64                                           0
#define PB_IS_MESSAGE_LTYPE_SINT32                                           0
#define PB_IS_MESSAGE_LTYPE_SFIXED64                                         0
#define PB_IS_MESSAGE_LTYPE_SFIXED32                                         0
#define PB_IS_MESSAGE_LTYPE_MSG_W_CB                                         1
#define PB_IS_MESSAGE_LTYPE_MESSAGE                                          1
#define PB_IS_MESSAGE_LTYPE_INT64                                            0
#define PB_IS_MESSAGE_LTYPE_INT32                                            0
#define PB_IS_MESSAGE_LTYPE_FLOAT                                            0
#define PB_IS_MESSAGE_LTYPE_FIXED64                                          0
#define PB_IS_MESSAGE_LTYPE_FIXED32                                          0
#define PB_IS_MESSAGE_LTYPE_UENUM                                            0
#define PB_IS_MESSAGE_LTYPE_ENUM                                             0
#define PB_IS_MESSAGE_LTYPE_DOUBLE                                           0
#define PB_IS_MESSAGE_LTYPE_BYTES                                            0
#define PB_IS_MESSAGE_LTYPE_BOOL                                             0

#define PB_IS_PACKABLE_LTYPE_FIXED_LENGTH_BYTES                              0
#define PB_IS_PACKABLE_LTYPE_EXTENSION                                       0
#define PB_IS_PACKABLE_LTYPE_UINT64                                          1
#define PB_IS_PACKABLE_LTYPE_UINT32                                          1
#define PB_IS_PACKABLE_LTYPE_STRING                                          0
#define PB_IS_PACKABLE_LTYPE_SINT64                                          1
#define PB_IS_PACKABLE_LTYPE_SINT32                                          1
#define PB_IS_PACKABLE_LTYPE_SFIXED64                                        1
#define PB_IS_PACKABLE_LTYPE_SFIXED32                                        1
#define PB_IS_PACKABLE_LTYPE_MSG_W_CB                                        0
#define PB_IS_PACKABLE_LTYPE_MESSAGE                                         0
#define PB_IS_PACKABLE_LTYPE_INT64                                           1
#define PB_IS_PACKABLE_LTYPE_INT32                                           1
#define PB_IS_PACKABLE_LTYPE_FLOAT                                           1
#define PB_IS_PACKABLE_LTYPE_FIXED64                                         1
#define PB_IS_PACKABLE_LTYPE_FIXED32                                         1
#define PB_IS_PACKABLE_LTYPE_UENUM                                           1
#define PB_IS_PACKABLE_LTYPE_ENUM                                            1
#define PB_IS_PACKABLE_LTYPE_DOUBLE                                          1
#define PB_IS_PACKABLE_LTYPE_BYTES                                           0
#define PB_IS_PACKABLE_LTYPE_BOOL                                            1

#define PB_IS_SCALAR_HTYPE_ONEOF                                             1
#define PB_IS_SCALAR_HTYPE_FIXARRAY                                          0
#define PB_IS_SCALAR_HTYPE_REPEATED                                          0
#define PB_IS_SCALAR_HTYPE_SINGULAR                                          1
#define PB_IS_SCALAR_HTYPE_OPTIONAL                                          1
#define PB_IS_SCALAR_HTYPE_REQUIRED                                          1

#define PB_EXTENSION_FIELD_ATYPE(_0, atype, htype, ltype, fieldname, tag)    | PB_ATYPE_ ## atype
#define PB_EXTENSION_ATYPE(msgname, structname)                              (0 msgname ## _FIELDLIST(PB_EXTENSION_FIELD_ATYPE, structname))

#define PB_FREE(ptr)                                                                                                                       \
    do {                                                                                                                                   \
        PB_FREE_FIELD(ptr);                                                                                                                \
        ptr = NULL;                                                                                                                        \
    } while (0)

#define PB_REALLOC(ptr, num, size)                                                                                                         \
    do {                                                                                                                                   \
        if (!PB_ALLOC_FIELD(stream, &(ptr), size, num))                                                                                    \
            return false;                                                                                                                  \
    } while (0)

/* -- RELEASE VALUES -------------------------------------------------------------------------------------------------------------------- */

#define PB_RELEASE_VALUE_LTYPE_FIXED_LENGTH_BYTES(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_UINT64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_UINT32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_STRING(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_SINT64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_SINT32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_SFIXED64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_SFIXED32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_MSG_W_CB(structname, atype, htype, ltype, fieldname, tag, data)                                             \
    if (data)                                                                                                                              \
    {                                                                                                                                      \
        const pb_msgdesc_t *field_desc = PB_MSGDESC(structname, fieldname, htype);                                                         \
                                                                                                                                           \
        field_desc->release(data, 0, 0);                                                                                                   \
    }

#define PB_RELEASE_VALUE_LTYPE_MESSAGE(structname, atype, htype, ltype, fieldname, tag, data)                                              \
    if (data)                                                                                                                              \
    {                                                                                                                                      \
        const pb_msgdesc_t *field_desc = PB_MSGDESC(structname, fieldname, htype);                                                         \
                                                                                                                                           \
        field_desc->release(data, 0, 0);                                                                                                   \
    }

#define PB_RELEASE_VALUE_LTYPE_INT64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_INT32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_FLOAT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_UENUM(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_ENUM(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_DOUBLE(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_BYTES(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE_LTYPE_BOOL(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data)                                                            \
    PB_RELEASE_VALUE_ ## ltype(structname, atype, htype, ltype, fieldname, tag, data)

/* -- RELEASE CALLBACK FIELDS ----------------------------------------------------------------------------------------------------------- */

#define PB_RELEASE_ATYPE_CALLBACK(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- RELEASE POINTER FIELDS ------------------------------------------------------------------------------------------------------------ */

#define PB_RELEASE_ATYPE_POINTER_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    do {                                                                                                                                   \
        if (size == tag)                                                                                                                   \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                       \
            PB_FREE(field);                                                                                                                \
            size = 0;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        for (i = 0; i < capacity; i++)                                                                                                     \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                    \
            PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_FREE(data[i]), );                                                                         \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_FREE(field);                                                                                                                    \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    do {                                                                                                                                   \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        for (i = 0; i < size; i++)                                                                                                         \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                    \
            PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_FREE(data[i]), );                                                                         \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_FREE(field);                                                                                                                    \
        size = 0;                                                                                                                          \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
        PB_FREE(field);                                                                                                                    \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
        PB_FREE(field);                                                                                                                    \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
        PB_FREE(field);                                                                                                                    \
    } while (0)

#define PB_RELEASE_ATYPE_POINTER(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                       \
    PB_RELEASE_ATYPE_POINTER_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- RELEASE STATIC FIELDS ------------------------------------------------------------------------------------------------------------- */

#define PB_RELEASE_ATYPE_STATIC_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    do {                                                                                                                                   \
        if (size == tag)                                                                                                                   \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                       \
            size = 0;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        for (i = 0; i < capacity; i++)                                                                                                     \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                    \
        }                                                                                                                                  \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        /* protect against corrupt _count fields */                                                                                        \
        if (size > capacity)                                                                                                               \
            size = capacity;                                                                                                               \
                                                                                                                                           \
        for (i = 0; i < size; i++)                                                                                                         \
        {                                                                                                                                  \
            PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                    \
        }                                                                                                                                  \
                                                                                                                                           \
        size = 0;                                                                                                                          \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
        size = 0;                                                                                                                          \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_RELEASE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                           \
    } while (0)

#define PB_RELEASE_ATYPE_STATIC(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                        \
    PB_RELEASE_ATYPE_STATIC_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- RELEASE FUNCTIONS ----------------------------------------------------------------------------------------------------------------- */

#define PB_RELEASE_FIELD_EXTENSION(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                     \
    do {                                                                                                                                   \
        pb_extension_t *ext = *data;                                                                                                       \
                                                                                                                                           \
        while (ext)                                                                                                                        \
        {                                                                                                                                  \
            const pb_msgdesc_t *ext_desc = (const pb_msgdesc_t *)ext->type->arg;                                                           \
                                                                                                                                           \
            if (t##ag == 0 || t##ag == ext_desc->largest_tag)                                                                              \
                ext_desc->release(&ext->dest, PB_DECODE_ISEXTENSION, ext_desc->largest_tag);                                               \
                                                                                                                                           \
            ext = ext->next;                                                                                                               \
        }                                                                                                                                  \
    } while (0)

#define PB_RELEASE_FIELD_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                       \
    do {                                                                                                                                   \
        PB_RELEASE_ ## atype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                                          \
    } while (0)

#define PB_RELEASE_FIELD_EXPAND(structname, atype, htype, ltype, fieldname, tag)                                                           \
    PB_SELECT(PB_IS_EXTENSION_ ## ltype, PB_RELEASE_FIELD_EXTENSION, PB_RELEASE_FIELD_DEFAULT)(                                            \
        structname, atype, htype, ltype, fieldname, tag,                                                                                   \
        PB_FIELDNAME(structname, fieldname, htype),                                                                                        \
        PB_FIELDDATA(structname, fieldname, atype, htype, ltype),                                                                          \
        PB_FIELDSIZE(structname, fieldname, atype, htype))

#define PB_RELEASE_FIELD(structname, atype, htype, ltype, fieldname, tag)                                                                  \
    if (t##ag == 0 || t##ag == tag)                                                                                                        \
    {                                                                                                                                      \
        PB_RELEASE_FIELD_EXPAND(structname, ATYPE_ ## atype, HTYPE_ ## htype, LTYPE_ ## ltype, fieldname, tag);                            \
    }

#define PB_RELEASE_GENERATE(msgname, structname)                                                                                           \
    static void pb_release_ ## structname(void *data, unsigned flags, uint32_t tag)                                                        \
    {                                                                                                                                      \
        structname *msg = (structname *)data;                                                                                              \
                                                                                                                                           \
        /* see notes from PB_DECODE_GENERATE */                                                                                            \
        if ((flags & PB_DECODE_ISEXTENSION) && PB_EXTENSION_ATYPE(msgname, structname) != PB_ATYPE_POINTER)                                \
            msg = (structname *)*(void **)data;                                                                                            \
                                                                                                                                           \
        /* mark unused in case FIELDLIST is empty */                                                                                       \
        PB_UNUSED(data);                                                                                                                   \
        PB_UNUSED(flags);                                                                                                                  \
        PB_UNUSED(tag);                                                                                                                    \
        PB_UNUSED(msg);                                                                                                                    \
                                                                                                                                           \
        msgname ## _FIELDLIST(PB_RELEASE_FIELD, structname)                                                                                \
    }

/* -- DECODE VALUES --------------------------------------------------------------------------------------------------------------------- */

#define PB_WT_MAP_LTYPE_FIXED_LENGTH_BYTES PB_WT_STRING
#define PB_WT_MAP_LTYPE_UINT64             PB_WT_VARINT
#define PB_WT_MAP_LTYPE_UINT32             PB_WT_VARINT
#define PB_WT_MAP_LTYPE_STRING             PB_WT_STRING
#define PB_WT_MAP_LTYPE_SINT64             PB_WT_VARINT
#define PB_WT_MAP_LTYPE_SINT32             PB_WT_VARINT
#define PB_WT_MAP_LTYPE_SFIXED64           PB_WT_64BIT
#define PB_WT_MAP_LTYPE_SFIXED32           PB_WT_32BIT
#define PB_WT_MAP_LTYPE_MSG_W_CB           PB_WT_STRING
#define PB_WT_MAP_LTYPE_MESSAGE            PB_WT_STRING
#define PB_WT_MAP_LTYPE_INT64              PB_WT_VARINT
#define PB_WT_MAP_LTYPE_INT32              PB_WT_VARINT
#define PB_WT_MAP_LTYPE_FLOAT              PB_WT_32BIT
#define PB_WT_MAP_LTYPE_FIXED64            PB_WT_64BIT
#define PB_WT_MAP_LTYPE_FIXED32            PB_WT_32BIT
#define PB_WT_MAP_LTYPE_UENUM              PB_WT_VARINT
#define PB_WT_MAP_LTYPE_ENUM               PB_WT_VARINT
#define PB_WT_MAP_LTYPE_DOUBLE             PB_WT_64BIT
#define PB_WT_MAP_LTYPE_BYTES              PB_WT_STRING
#define PB_WT_MAP_LTYPE_BOOL               PB_WT_VARINT

#define PB_DECODE_VALUE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)                                                     \
    do {                                                                                                                                   \
        if (!PB_DECODE_FIXED64(stream, data, sizeof(*data)))                                                                               \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)                                                     \
    do {                                                                                                                                   \
        if (!PB_DECODE_FIXED32(stream, data))                                                                                              \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_SVARINT(structname, atype, htype, ltype, fieldname, tag, data)                                                     \
    do {                                                                                                                                   \
        if (!PB_DECODE_SVARINT(stream, data, sizeof(*data)))                                                                               \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_UVARINT(structname, atype, htype, ltype, fieldname, tag, data)                                                     \
    do {                                                                                                                                   \
        if (!PB_DECODE_UVARINT(stream, data, sizeof(*data)))                                                                               \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)                                                      \
    do {                                                                                                                                   \
        if (!PB_DECODE_VARINT(stream, data, sizeof(*data)))                                                                                \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_FIXED_LENGTH_BYTES(structname, atype, htype, ltype, fieldname, tag, data)                                    \
    do {                                                                                                                                   \
        if (!PB_DECODE_FIXED_LENGTH_BYTES(stream, data, sizeof(data)))                                                                     \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_UINT64(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_UVARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_UINT32(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_UVARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_STRING_ATYPE_POINTER(structname, atype, htype, ltype, fieldname, tag, data)                                  \
    do {                                                                                                                                   \
        if (!PB_DECODE_STRING(stream, &data, 0))                                                                                           \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_STRING_ATYPE_STATIC(structname, atype, htype, ltype, fieldname, tag, data)                                   \
    do {                                                                                                                                   \
        if (!PB_DECODE_STRING(stream, data, sizeof(data)))                                                                                 \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_STRING(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_LTYPE_STRING_ ## atype(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_SINT64(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_SVARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_SINT32(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_SVARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_SFIXED64(structname, atype, htype, ltype, fieldname, tag, data)                                              \
    PB_DECODE_VALUE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_SFIXED32(structname, atype, htype, ltype, fieldname, tag, data)                                              \
    PB_DECODE_VALUE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_MSG_W_CB(structname, atype, htype, ltype, fieldname, tag, data)                                              \
    do {                                                                                                                                   \
        pb_field_iter_t iter;                                                                                                              \
                                                                                                                                           \
        if (!pb_field_iter_begin(&iter, desc, msg) || !pb_field_iter_find(&iter, tag))                                                     \
            PB_RETURN_ERROR(stream, "failed to resolve field tag");                                                                        \
                                                                                                                                           \
        if (!PB_DECODE_SUBMESSAGE(stream, &iter, subflags))                                                                                \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_MESSAGE(structname, atype, htype, ltype, fieldname, tag, data)                                               \
    do {                                                                                                                                   \
        const pb_msgdesc_t *field_desc = PB_MSGDESC(structname, fieldname, htype);                                                         \
                                                                                                                                           \
        if (!field_desc->decode(stream, data, subflags | PB_DECODE_DELIMITED, 0, (pb_wire_type_t)0))                                       \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_INT64(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_INT32(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_FLOAT(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)                                               \
    PB_DECODE_VALUE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)                                               \
    PB_DECODE_VALUE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_UENUM(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_UVARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_ENUM(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    PB_DECODE_VALUE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_DOUBLE(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    PB_DECODE_VALUE_VARINT(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_BYTES_ATYPE_POINTER(structname, atype, htype, ltype, fieldname, tag, data)                                   \
    do {                                                                                                                                   \
        if (!PB_DECODE_BYTES(stream, &data, 0))                                                                                            \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_BYTES_ATYPE_STATIC(structname, atype, htype, ltype, fieldname, tag, data)                                    \
    do {                                                                                                                                   \
        if (!PB_DECODE_BYTES(stream, data, sizeof(*data)))                                                                                 \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_LTYPE_BYTES(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_LTYPE_BYTES_ ## atype(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_DECODE_VALUE_LTYPE_BOOL(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    do {                                                                                                                                   \
        if (!PB_DECODE_BOOL(stream, data))                                                                                                 \
            return false;                                                                                                                  \
    } while (0)

#define PB_DECODE_VALUE_NO_WT_CHECK(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    PB_DECODE_VALUE_ ## ltype(structname, atype, htype, ltype, fieldname, tag, (data))

#define PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data)                                                             \
    do {                                                                                                                                   \
        const pb_wire_type_t expected = PB_WT_MAP_ ## ltype;                                                                               \
                                                                                                                                           \
        if (wire_type != expected)                                                                                                         \
            PB_RETURN_ERROR(stream, "wrong wire type");                                                                                    \
                                                                                                                                           \
        PB_DECODE_VALUE_NO_WT_CHECK(structname, atype, htype, ltype, fieldname, tag, data);                                                \
    } while (0)

/* -- DECODE CALLBACK FIELDS ------------------------------------------------------------------------------------------------------------ */

#define PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)                               \
    do {                                                                                                                                   \
        if (desc->field_callback)                                                                                                          \
        {                                                                                                                                  \
            pb_field_iter_t iter;                                                                                                          \
                                                                                                                                           \
            if (!pb_field_iter_begin(&iter, desc, msg) || !pb_field_iter_find(&iter, tag))                                                 \
                PB_RETURN_ERROR(stream, "failed to resolve field tag");                                                                    \
                                                                                                                                           \
            if (!PB_DECODE_CALLBACK_FIELD(stream, wire_type, &iter))                                                                       \
                return false;                                                                                                              \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            if (!PB_SKIP_FIELD(stream, wire_type))                                                                                         \
                return false;                                                                                                              \
        }                                                                                                                                  \
    } while (0)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    do {                                                                                                                                   \
        if (size && size != tag)                                                                                                           \
            pb_release_ ## structname(msg, 0, size);                                                                                       \
                                                                                                                                           \
        size = tag;                                                                                                                        \
                                                                                                                                           \
        PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size);                              \
    } while (0)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_ATYPE_CALLBACK_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                        \
    PB_DECODE_ATYPE_CALLBACK_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_ATYPE_CALLBACK(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                       \
    PB_DECODE_ATYPE_CALLBACK_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- DECODE POINTER FIELDS ------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_NESTED_POINTERS(structname, atype, htype, ltype, fieldname, tag, data)                                                     \
    PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_INIT_VALUE, PB_INIT_IGNORE)(structname, atype, htype, ltype, fieldname, tag, data);               \
    PB_SELECT(PB_IS_MESSAGE_ ## ltype, PB_INIT_VALUE, PB_INIT_IGNORE)(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_GROW_ARRAY_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)                         \
    do {                                                                                                                                   \
        if (size >= capacity)                                                                                                              \
            PB_RETURN_ERROR(stream, "array overflow");                                                                                     \
    } while (0)

#define PB_GROW_ARRAY_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)                         \
    do {                                                                                                                                   \
        if (size >= PB_SIZE_MAX)                                                                                                           \
            PB_RETURN_ERROR(stream, "too many array entries");                                                                             \
                                                                                                                                           \
        PB_REALLOC(field, size + 1, sizeof(*field));                                                                                       \
        PB_INIT_NESTED_POINTERS(structname, atype, htype, ltype, fieldname, tag, data[size]);                                              \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_ARRAY_PACKABLE(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)               \
    do {                                                                                                                                   \
        if (wire_type == PB_WT_STRING)                                                                                                     \
        {                                                                                                                                  \
            size_t end_bytes_left;                                                                                                         \
            uint32_t len;                                                                                                                  \
                                                                                                                                           \
            if (!PB_DECODE_VARINT32(stream, &len))                                                                                         \
                return false;                                                                                                              \
                                                                                                                                           \
            end_bytes_left = stream->bytes_left - (size_t)len;                                                                             \
                                                                                                                                           \
            while (stream->bytes_left > end_bytes_left)                                                                                    \
            {                                                                                                                              \
                PB_GROW_ARRAY_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity);                     \
                PB_DECODE_VALUE_NO_WT_CHECK(structname, atype, htype, ltype, fieldname, tag, data[size]);                                  \
                size += 1;                                                                                                                 \
            }                                                                                                                              \
                                                                                                                                           \
            if (stream->bytes_left != end_bytes_left)                                                                                      \
                PB_RETURN_ERROR(stream, "incorrect packed array size");                                                                    \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            PB_DECODE_ATYPE_POINTER_ARRAY_NORMAL(                                                                                          \
                structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity);                                             \
        }                                                                                                                                  \
      } while (0)

#define PB_DECODE_ATYPE_POINTER_ARRAY_NORMAL(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)                 \
    do {                                                                                                                                   \
        PB_GROW_ARRAY_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity);                             \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[size]);                                                      \
        size += 1;                                                                                                                         \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_SCALAR_VARLEN(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        /* nothing to initialize, already NULL'd when initializing parent message */                                                       \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_SCALAR_FIXLEN(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        if (!field)                                                                                                                        \
        {                                                                                                                                  \
            PB_REALLOC(field, 1, sizeof(*field));                                                                                          \
            PB_INIT_NESTED_POINTERS(structname, atype, htype, ltype, fieldname, tag, data);                                                \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    do {                                                                                                                                   \
        if (size && size != tag)                                                                                                           \
        {                                                                                                                                  \
            pb_release_ ## structname(msg, 0, size);                                                                                       \
                                                                                                                                           \
            /* make sure there's no static data leftover that will produce a garbage pointer */                                            \
            data = NULL;                                                                                                                   \
        }                                                                                                                                  \
                                                                                                                                           \
        size = tag;                                                                                                                        \
                                                                                                                                           \
        PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_DECODE_ATYPE_POINTER_SCALAR_VARLEN,                                                           \
            PB_DECODE_ATYPE_POINTER_SCALAR_FIXLEN)(structname, atype, htype, ltype, fieldname, tag, field, data, size);                    \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        if (!field)                                                                                                                        \
        {                                                                                                                                  \
            size_t i;                                                                                                                      \
                                                                                                                                           \
            PB_REALLOC(field, 1, sizeof(*field));                                                                                          \
                                                                                                                                           \
            for (i = 0; i < fixarray_capacity; i++)                                                                                        \
            {                                                                                                                              \
                PB_INIT_NESTED_POINTERS(structname, atype, htype, ltype, fieldname, tag, data[i]);                                         \
            }                                                                                                                              \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_SELECT(PB_IS_PACKABLE_ ## ltype, PB_DECODE_ATYPE_POINTER_ARRAY_PACKABLE,                                                        \
            PB_DECODE_ATYPE_POINTER_ARRAY_NORMAL)(                                                                                         \
                structname, atype, htype, ltype, fieldname, tag, field, data, fixarray_size, fixarray_capacity);                           \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_SELECT(PB_IS_PACKABLE_ ## ltype, PB_DECODE_ATYPE_POINTER_ARRAY_PACKABLE,                                                        \
            PB_DECODE_ATYPE_POINTER_ARRAY_NORMAL)(                                                                                         \
                structname, atype, htype, ltype, fieldname, tag, field, data, size, 0);                                                    \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_DECODE_ATYPE_POINTER_SCALAR_VARLEN,                                                           \
            PB_DECODE_ATYPE_POINTER_SCALAR_FIXLEN)(structname, atype, htype, ltype, fieldname, tag, field, data, size);                    \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_DECODE_ATYPE_POINTER_SCALAR_VARLEN,                                                           \
            PB_DECODE_ATYPE_POINTER_SCALAR_FIXLEN)(structname, atype, htype, ltype, fieldname, tag, field, data, size);                    \
    } while (0)

#define PB_DECODE_ATYPE_POINTER_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                         \
    do {                                                                                                                                   \
        PB_SELECT(PB_IS_VARLEN_ ## ltype, PB_DECODE_ATYPE_POINTER_SCALAR_VARLEN,                                                           \
            PB_DECODE_ATYPE_POINTER_SCALAR_FIXLEN)(structname, atype, htype, ltype, fieldname, tag, field, data, size);                    \
    } while (0)

#define PB_DECODE_ATYPE_POINTER(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                        \
    PB_DECODE_ATYPE_POINTER_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- DECODE STATIC FIELDS -------------------------------------------------------------------------------------------------------------- */

#define PB_DECODE_ATYPE_STATIC_ARRAY_PACKABLE(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)                \
    do {                                                                                                                                   \
        if (wire_type == PB_WT_STRING)                                                                                                     \
        {                                                                                                                                  \
            size_t end_bytes_left;                                                                                                         \
            uint32_t len;                                                                                                                  \
                                                                                                                                           \
            if (!PB_DECODE_VARINT32(stream, &len))                                                                                         \
                return false;                                                                                                              \
                                                                                                                                           \
            end_bytes_left = stream->bytes_left - (size_t)len;                                                                             \
                                                                                                                                           \
            while (stream->bytes_left > end_bytes_left)                                                                                    \
            {                                                                                                                              \
                if (size >= capacity)                                                                                                      \
                    PB_RETURN_ERROR(stream, "array overflow");                                                                             \
                                                                                                                                           \
                PB_DECODE_VALUE_NO_WT_CHECK(structname, atype, htype, ltype, fieldname, tag, data[size]);                                  \
                size += 1;                                                                                                                 \
            }                                                                                                                              \
                                                                                                                                           \
            if (stream->bytes_left != end_bytes_left)                                                                                      \
                PB_RETURN_ERROR(stream, "incorrect packed array size");                                                                    \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            PB_DECODE_ATYPE_STATIC_ARRAY_NORMAL(                                                                                           \
                structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity);                                             \
        }                                                                                                                                  \
      } while (0)

#define PB_DECODE_ATYPE_STATIC_ARRAY_NORMAL(structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity)                  \
    do {                                                                                                                                   \
        if (size >= capacity)                                                                                                              \
            PB_RETURN_ERROR(stream, "array overflow");                                                                                     \
                                                                                                                                           \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data[size]);                                                      \
        size += 1;                                                                                                                         \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                             \
    do {                                                                                                                                   \
        if (size && size != tag)                                                                                                           \
            pb_release_ ## structname(msg, 0, size);                                                                                       \
                                                                                                                                           \
        if (!size)                                                                                                                         \
        {                                                                                                                                  \
            PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                          \
            size = tag;                                                                                                                    \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        PB_SELECT(PB_IS_PACKABLE_ ## ltype, PB_DECODE_ATYPE_STATIC_ARRAY_PACKABLE,                                                         \
            PB_DECODE_ATYPE_STATIC_ARRAY_NORMAL)(                                                                                          \
                structname, atype, htype, ltype, fieldname, tag, field, data, fixarray_size, fixarray_capacity);                           \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
                                                                                                                                           \
        PB_SELECT(PB_IS_PACKABLE_ ## ltype, PB_DECODE_ATYPE_STATIC_ARRAY_PACKABLE,                                                         \
            PB_DECODE_ATYPE_STATIC_ARRAY_NORMAL)(                                                                                          \
                structname, atype, htype, ltype, fieldname, tag, field, data, size, capacity);                                             \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        /* don't set when initializing fields to user-defined values */                                                                    \
        size = !(flags & PB_DECODE_ISINIT);                                                                                                \
                                                                                                                                           \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_STATIC_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                          \
    do {                                                                                                                                   \
        PB_DECODE_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                            \
    } while (0)

#define PB_DECODE_ATYPE_STATIC(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                         \
    do {                                                                                                                                   \
        PB_DECODE_ATYPE_STATIC_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                              \
    } while (0)

/* -- DECODE FUNCTIONS ------------------------------------------------------------------------------------------------------------------ */

#define PB_CHECK_REQUIRED_STATE()                                                                                                          \
    do {                                                                                                                                   \
        /* not an error when decoding user-provided defaults */                                                                            \
        if ((flags & PB_DECODE_ISINIT) == 0)                                                                                               \
            if (num_required_fields != NUM_REQUIRED_FIELDS)                                                                                \
                PB_RETURN_ERROR(stream, "missing required fields");                                                                        \
    } while (0)

#define PB_CHECK_FIXARRAY_STATE()                                                                                                          \
    do {                                                                                                                                   \
        if (fixarray_size != fixarray_capacity)                                                                                            \
            PB_RETURN_ERROR(stream, "wrong size for fixed count field");                                                                   \
    } while (0)

#define PB_POST_DECODE_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_POST_DECODE_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_POST_DECODE_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_POST_DECODE_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_POST_DECODE_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_POST_DECODE_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                  \
    do {                                                                                                                                   \
        uint32_t word = REQUIRED_FIELD_##tag / 32;                                                                                         \
        uint32_t bit = REQUIRED_FIELD_##tag & 31;                                                                                          \
        num_required_fields += ((required_fields[word] >> bit) & 0x1) ^ 0x1;                                                               \
        required_fields[word] |= UINT32_C(1) << bit;                                                                                       \
    } while (0)

#define PB_PRE_DECODE_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_PRE_DECODE_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                   \
    do {                                                                                                                                   \
        if (tag != fixarray_tag)                                                                                                           \
        {                                                                                                                                  \
            PB_CHECK_FIXARRAY_STATE();                                                                                                     \
                                                                                                                                           \
            fixarray_capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                         \
            fixarray_size = 0;                                                                                                             \
            fixarray_tag = tag;                                                                                                            \
        }                                                                                                                                  \
    } while (0)

#define PB_PRE_DECODE_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_PRE_DECODE_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_PRE_DECODE_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_PRE_DECODE_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)

#define PB_DECODE_FIELD_EXTENSION(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                      \
    if (t##ag >= tag)                                                                                                                      \
    {                                                                                                                                      \
        ext = *data;                                                                                                                       \
                                                                                                                                           \
        while (ext)                                                                                                                        \
        {                                                                                                                                  \
            const pb_msgdesc_t *ext_desc = (const pb_msgdesc_t *)ext->type->arg;                                                           \
                                                                                                                                           \
            if (ext_desc->largest_tag == t##ag)                                                                                            \
            {                                                                                                                              \
                bool status;                                                                                                               \
                                                                                                                                           \
                if (ext->type->decode)                                                                                                     \
                    status = ext->type->decode(stream, ext, t##ag, wire_type);                                                             \
                else                                                                                                                       \
                    status = ext_desc->decode(stream, &ext->dest, subflags | PB_DECODE_ISEXTENSION, t##ag, wire_type);                     \
                                                                                                                                           \
                ext->found = true;                                                                                                         \
                                                                                                                                           \
                if (!status)                                                                                                               \
                    return false;                                                                                                          \
                                                                                                                                           \
                break;                                                                                                                     \
            }                                                                                                                              \
                                                                                                                                           \
            ext = ext->next;                                                                                                               \
        }                                                                                                                                  \
    }

#define PB_DECODE_FIELD_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                        \
    case tag: {                                                                                                                            \
        PB_PRE_DECODE_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                                       \
        PB_DECODE_ ## atype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                                           \
        PB_POST_DECODE_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                                      \
    } break;

#define PB_DECODE_FIELD_EXPAND(structname, atype, htype, ltype, fieldname, tag)                                                            \
    PB_SELECT(PB_IS_EXTENSION_ ## ltype, PB_DECODE_FIELD_EXTENSION, PB_DECODE_FIELD_DEFAULT)(                                              \
        structname, atype, htype, ltype, fieldname, tag,                                                                                   \
        PB_FIELDNAME(structname, fieldname, htype),                                                                                        \
        PB_FIELDDATA(structname, fieldname, atype, htype, ltype),                                                                          \
        PB_FIELDSIZE(structname, fieldname, atype, htype))

#define PB_DECODE_FIELD_IGNORE(structname, atype, htype, ltype, fieldname, tag)

#define PB_DECODE_FIELD_ONLY_EXTENSIONS(structname, atype, htype, ltype, fieldname, tag)                                                   \
    PB_SELECT(PB_IS_EXTENSION_LTYPE_ ## ltype, PB_DECODE_FIELD_EXPAND, PB_DECODE_FIELD_IGNORE)(                                            \
        structname, ATYPE_ ## atype, HTYPE_ ## htype, LTYPE_ ## ltype, fieldname, tag)

#define PB_DECODE_FIELD_NO_EXTENSIONS(structname, atype, htype, ltype, fieldname, tag)                                                     \
    PB_SELECT(PB_IS_EXTENSION_LTYPE_ ## ltype, PB_DECODE_FIELD_IGNORE, PB_DECODE_FIELD_EXPAND)(                                            \
        structname, ATYPE_ ## atype, HTYPE_ ## htype, LTYPE_ ## ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_ONEOF(structname, atype, htype, ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_FIXARRAY(structname, atype, htype, ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_REPEATED(structname, atype, htype, ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_SINGULAR(structname, atype, htype, ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_OPTIONAL(structname, atype, htype, ltype, fieldname, tag)

#define PB_EMIT_REQUIRED_FIELD_ENUM_REQUIRED(structname, atype, htype, ltype, fieldname, tag)                                              \
    REQUIRED_FIELD_ ## tag,

#define PB_EMIT_REQUIRED_FIELD_ENUM(structname, atype, htype, ltype, fieldname, tag)                                                       \
    PB_EMIT_REQUIRED_FIELD_ENUM_ ## htype(structname, atype, htype, ltype, fieldname, tag)

#define PB_DECODE_GENERATE(msgname, structname)                                                                                            \
    static bool pb_decode_inner_ ## structname(pb_istream_t *stream, void *data, unsigned flags, uint32_t tag, pb_wire_type_t wire_type)   \
    {                                                                                                                                      \
        const unsigned subflags = PB_DECODE_ISDECODE | PB_DECODE_NOINIT;                                                                   \
        const pb_msgdesc_t *desc = &(structname ## _msg);                                                                                  \
        structname *msg = (structname *)data;                                                                                              \
                                                                                                                                           \
        /* for pointer extension fields, the pointer is stored directly in the extension structure to avoid an                             \
           additional indirection. PB_DECODE_FIELD_EXTENSION doesn't know the allocation type of the extension                             \
           field, so it always passes a void **, which needs to be dereferenced when not a pointer */                                      \
        if ((flags & PB_DECODE_ISEXTENSION) && PB_EXTENSION_ATYPE(msgname, structname) != PB_ATYPE_POINTER)                                \
            msg = (structname *)*(void **)data;                                                                                            \
                                                                                                                                           \
        PB_UNUSED(subflags);                                                                                                               \
        PB_UNUSED(desc);                                                                                                                   \
        PB_UNUSED(msg);                                                                                                                    \
                                                                                                                                           \
        /* REQUIRED field state */                                                                                                         \
        enum                                                                                                                               \
        {                                                                                                                                  \
            msgname ## _FIELDLIST(PB_EMIT_REQUIRED_FIELD_ENUM, structname)                                                                 \
            NUM_REQUIRED_FIELDS,                                                                                                           \
        };                                                                                                                                 \
        uint32_t required_fields[(NUM_REQUIRED_FIELDS + 31) / 32] = {};                                                                    \
        int num_required_fields = 0;                                                                                                       \
                                                                                                                                           \
        PB_UNUSED(required_fields);                                                                                                        \
                                                                                                                                           \
        /* FIXARRAY field state */                                                                                                         \
        size_t fixarray_capacity = 0;                                                                                                      \
        size_t fixarray_size = 0;                                                                                                          \
        uint32_t fixarray_tag = 0;                                                                                                         \
                                                                                                                                           \
        PB_UNUSED(fixarray_tag);                                                                                                           \
                                                                                                                                           \
        /* current field state */                                                                                                          \
        bool decode_single = tag != 0;                                                                                                     \
        bool eof = false;                                                                                                                  \
                                                                                                                                           \
        if ((flags & PB_DECODE_NOINIT) == 0)                                                                                               \
            if (!pb_init_ ## structname(data, flags))                                                                                      \
                return false;                                                                                                              \
                                                                                                                                           \
        while (!eof)                                                                                                                       \
        {                                                                                                                                  \
            if (decode_single)                                                                                                             \
            {                                                                                                                              \
                eof = true;                                                                                                                \
            }                                                                                                                              \
            else                                                                                                                           \
            {                                                                                                                              \
                if (!PB_DECODE_TAG(stream, &wire_type, &tag, &eof))                                                                        \
                    break;                                                                                                                 \
            }                                                                                                                              \
                                                                                                                                           \
            switch (tag)                                                                                                                   \
            {                                                                                                                              \
                case 0: {                                                                                                                  \
                    if (!(flags & PB_DECODE_NULLTERMINATED))                                                                               \
                        PB_RETURN_ERROR(stream, "zero tag");                                                                               \
                                                                                                                                           \
                    eof = true;                                                                                                            \
                } break;                                                                                                                   \
                                                                                                                                           \
                msgname ## _FIELDLIST(PB_DECODE_FIELD_NO_EXTENSIONS, structname)                                                           \
                                                                                                                                           \
                default: {                                                                                                                 \
                    pb_extension_t *ext = NULL;                                                                                            \
                                                                                                                                           \
                    msgname ## _FIELDLIST(PB_DECODE_FIELD_ONLY_EXTENSIONS, structname)                                                     \
                                                                                                                                           \
                    if (!ext && !PB_SKIP_FIELD(stream, wire_type))                                                                         \
                        return false;                                                                                                      \
                } break;                                                                                                                   \
            }                                                                                                                              \
        }                                                                                                                                  \
                                                                                                                                           \
        PB_CHECK_REQUIRED_STATE();                                                                                                         \
        PB_CHECK_FIXARRAY_STATE();                                                                                                         \
                                                                                                                                           \
        return eof;                                                                                                                        \
    }                                                                                                                                      \
                                                                                                                                           \
    static bool pb_decode_ ## structname(pb_istream_t *stream, void *data,                                                                 \
        unsigned flags, uint32_t tag, pb_wire_type_t wire_type)                                                                            \
    {                                                                                                                                      \
        pb_istream_t *decstream = stream;                                                                                                  \
        pb_istream_t substream;                                                                                                            \
        bool status;                                                                                                                       \
                                                                                                                                           \
        if (flags & PB_DECODE_DELIMITED)                                                                                                   \
        {                                                                                                                                  \
            if (!PB_MAKE_STRING_SUBSTREAM(stream, &substream))                                                                             \
                return false;                                                                                                              \
                                                                                                                                           \
            decstream = &substream;                                                                                                        \
        }                                                                                                                                  \
                                                                                                                                           \
        status = pb_decode_inner_ ## structname(decstream, data, flags, tag, wire_type);                                                   \
                                                                                                                                           \
        if (flags & PB_DECODE_DELIMITED)                                                                                                   \
        {                                                                                                                                  \
            if (!PB_CLOSE_STRING_SUBSTREAM(stream, &substream))                                                                            \
                status = false;                                                                                                            \
        }                                                                                                                                  \
                                                                                                                                           \
        if (!status)                                                                                                                       \
            pb_release_ ## structname(data, flags, 0);                                                                                     \
                                                                                                                                           \
        return status;                                                                                                                     \
    }

/* -- INIT VALUES ----------------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_BYTES_ATYPE_POINTER(data)                                                                                                  \
    data = NULL

#define PB_INIT_BYTES_ATYPE_STATIC(data)

#define PB_INIT_VALUE_LTYPE_FIXED_LENGTH_BYTES(structname, atype, htype, ltype, fieldname, tag, data)                                      \
    memset(data, 0, sizeof(data))

#define PB_INIT_VALUE_LTYPE_UINT64(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    *data = 0

#define PB_INIT_VALUE_LTYPE_UINT32(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    *data = 0

#define PB_INIT_VALUE_LTYPE_STRING(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    do {                                                                                                                                   \
        PB_INIT_BYTES_ ## atype(data);                                                                                                     \
                                                                                                                                           \
        if (data)                                                                                                                          \
            data[0] = 0;                                                                                                                   \
    } while (0)

#define PB_INIT_VALUE_LTYPE_SINT64(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    *data = 0

#define PB_INIT_VALUE_LTYPE_SINT32(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    *data = 0

#define PB_INIT_VALUE_LTYPE_SFIXED64(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    *data = 0

#define PB_INIT_VALUE_LTYPE_SFIXED32(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    *data = 0

#define PB_INIT_VALUE_LTYPE_MSG_W_CB(structname, atype, htype, ltype, fieldname, tag, data)                                                \
    do {                                                                                                                                   \
        const pb_msgdesc_t *field_desc = PB_MSGDESC(structname, fieldname, htype);                                                         \
                                                                                                                                           \
        if (!field_desc->init(data, subflags))                                                                                             \
            return false;                                                                                                                  \
    } while (0)

#define PB_INIT_VALUE_LTYPE_MESSAGE(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    do {                                                                                                                                   \
        const pb_msgdesc_t *field_desc = PB_MSGDESC(structname, fieldname, htype);                                                         \
                                                                                                                                           \
        if (!field_desc->init(data, subflags))                                                                                             \
            return false;                                                                                                                  \
    } while (0)

#define PB_INIT_VALUE_LTYPE_INT64(structname, atype, htype, ltype, fieldname, tag, data)                                                   \
    *data = 0

#define PB_INIT_VALUE_LTYPE_INT32(structname, atype, htype, ltype, fieldname, tag, data)                                                   \
    *data = 0

#define PB_INIT_VALUE_LTYPE_FLOAT(structname, atype, htype, ltype, fieldname, tag, data)                                                   \
    *data = 0.0f

#define PB_INIT_VALUE_LTYPE_FIXED64(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    *data = 0

#define PB_INIT_VALUE_LTYPE_FIXED32(structname, atype, htype, ltype, fieldname, tag, data)                                                 \
    *data = 0

#define PB_INIT_VALUE_LTYPE_UENUM(structname, atype, htype, ltype, fieldname, tag, data)                                                   \
    memset(data, 0, sizeof(*data))

#define PB_INIT_VALUE_LTYPE_ENUM(structname, atype, htype, ltype, fieldname, tag, data)                                                    \
    memset(data, 0, sizeof(*data))

#define PB_INIT_VALUE_LTYPE_DOUBLE(structname, atype, htype, ltype, fieldname, tag, data)                                                  \
    *data = 0.0

#define PB_INIT_VALUE_LTYPE_BYTES(structname, atype, htype, ltype, fieldname, tag, data)                                                   \
    do {                                                                                                                                   \
        PB_INIT_BYTES_ ## atype(data);                                                                                                     \
                                                                                                                                           \
        if (data)                                                                                                                          \
            memset(data, 0, sizeof(*data));                                                                                                \
    } while (0)

#define PB_INIT_VALUE_LTYPE_BOOL(structname, atype, htype, ltype, fieldname, tag, data)                                                    \
    *data = false

#define PB_INIT_IGNORE(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data)                                                               \
    PB_INIT_VALUE_ ## ltype(structname, atype, htype, ltype, fieldname, tag, (data))

/* -- INIT CALLBACK FIELDS -------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_ATYPE_CALLBACK(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                         \
    do {                                                                                                                                   \
        /* be careful not to overwrite user-defined callbacks */                                                                           \
        if (flags & PB_DECODE_ISDECODE)                                                                                                    \
            memset((void *)data, 0, sizeof(field));                                                                                        \
    } while (0)

/* -- INIT POINTER FIELDS --------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_ATYPE_POINTER_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                              \
    field = NULL;                                                                                                                          \
    size = 0

#define PB_INIT_ATYPE_POINTER_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    field = NULL

#define PB_INIT_ATYPE_POINTER_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    field = NULL;                                                                                                                          \
    size = 0

#define PB_INIT_ATYPE_POINTER_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    field = NULL

#define PB_INIT_ATYPE_POINTER_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    field = NULL

#define PB_INIT_ATYPE_POINTER_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                           \
    field = NULL

#define PB_INIT_ATYPE_POINTER(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                          \
    PB_INIT_ATYPE_POINTER_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- INIT SCALAR FIELDS ---------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_ATYPE_STATIC_HTYPE_ONEOF(structname, atype, htype, ltype, fieldname, tag, field, data, size)                               \
    size = 0

#define PB_INIT_ATYPE_STATIC_HTYPE_FIXARRAY(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        for (i = 0; i < capacity; i++)                                                                                                     \
        {                                                                                                                                  \
            PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                       \
        }                                                                                                                                  \
    } while (0)

#define PB_INIT_ATYPE_STATIC_HTYPE_REPEATED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    do {                                                                                                                                   \
        size_t capacity = PB_ARRAYSIZE(structname, fieldname, atype, htype);                                                               \
        size_t i;                                                                                                                          \
                                                                                                                                           \
        for (i = 0; i < capacity; i++)                                                                                                     \
        {                                                                                                                                  \
            PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data[i]);                                                       \
        }                                                                                                                                  \
                                                                                                                                           \
        size = 0;                                                                                                                          \
    } while (0)

#define PB_INIT_ATYPE_STATIC_HTYPE_SINGULAR(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_INIT_ATYPE_STATIC_HTYPE_OPTIONAL(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data);                                                                  \
    size = 0

#define PB_INIT_ATYPE_STATIC_HTYPE_REQUIRED(structname, atype, htype, ltype, fieldname, tag, field, data, size)                            \
    PB_INIT_VALUE(structname, atype, htype, ltype, fieldname, tag, data)

#define PB_INIT_ATYPE_STATIC(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                           \
    PB_INIT_ATYPE_STATIC_ ## htype(structname, atype, htype, ltype, fieldname, tag, field, data, size)

/* -- INIT FUNCTIONS -------------------------------------------------------------------------------------------------------------------- */

#define PB_INIT_FIELD_EXTENSION(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                        \
    do {                                                                                                                                   \
        pb_extension_t *ext = *data;                                                                                                       \
                                                                                                                                           \
        while (ext)                                                                                                                        \
        {                                                                                                                                  \
            const pb_msgdesc_t *ext_desc = (const pb_msgdesc_t *)ext->type->arg;                                                           \
                                                                                                                                           \
            if (!ext_desc->init(&ext->dest, subflags | PB_DECODE_ISEXTENSION))                                                             \
                return false;                                                                                                              \
                                                                                                                                           \
            ext->found = false;                                                                                                            \
                                                                                                                                           \
            ext = ext->next;                                                                                                               \
        }                                                                                                                                  \
    } while (0)

#define PB_INIT_FIELD_DEFAULT(structname, atype, htype, ltype, fieldname, tag, field, data, size)                                          \
    do {                                                                                                                                   \
        PB_INIT_ ## atype(structname, atype, htype, ltype, fieldname, tag, field, data, size);                                             \
    } while (0)

#define PB_INIT_FIELD_EXPAND(structname, atype, htype, ltype, fieldname, tag)                                                              \
    PB_SELECT(PB_IS_EXTENSION_ ## ltype, PB_INIT_FIELD_EXTENSION, PB_INIT_FIELD_DEFAULT)(                                                  \
        structname, atype, htype, ltype, fieldname, tag,                                                                                   \
        PB_FIELDNAME(structname, fieldname, htype),                                                                                        \
        PB_FIELDDATA(structname, fieldname, atype, htype, ltype),                                                                          \
        PB_FIELDSIZE(structname, fieldname, atype, htype))

#define PB_INIT_FIELD(structname, atype, htype, ltype, fieldname, tag)                                                                     \
    PB_INIT_FIELD_EXPAND(structname, ATYPE_ ## atype, HTYPE_ ## htype, LTYPE_ ## ltype, fieldname, tag);

#define PB_INIT_GENERATE(msgname, structname)                                                                                              \
    static bool pb_init_ ## structname(void *data, unsigned flags)                                                                         \
    {                                                                                                                                      \
        const unsigned subflags = PB_DECODE_ISINIT | PB_DECODE_NOINIT;                                                                     \
        structname *msg = (structname *)data;                                                                                              \
                                                                                                                                           \
        /* see notes from PB_DECODE_GENERATE */                                                                                            \
        if ((flags & PB_DECODE_ISEXTENSION) && PB_EXTENSION_ATYPE(msgname, structname) != PB_ATYPE_POINTER)                                \
            msg = (structname *)*(void **)data;                                                                                            \
                                                                                                                                           \
        PB_UNUSED(msg);                                                                                                                    \
                                                                                                                                           \
        /* initialize fields to type-specific default values */                                                                            \
        msgname ## _FIELDLIST(PB_INIT_FIELD, structname)                                                                                   \
                                                                                                                                           \
        if (msgname ## _DEFAULT == NULL)                                                                                                   \
            return true;                                                                                                                   \
                                                                                                                                           \
        /* initialize fields to user-defined default values */                                                                             \
        pb_istream_t stream = PB_ISTREAM_FROM_BUFFER(msgname ## _DEFAULT, (size_t)-1);                                                     \
        return pb_decode_ ## structname(&stream, msg, subflags | PB_DECODE_NULLTERMINATED, 0, (pb_wire_type_t)0);                          \
    }

/* -- ENTRYPOINT ------------------------------------------------------------------------------------------------------------------------ */

#define PB_BIND_DECODE_FAST(msgname, structname)                                                                                           \
    static bool pb_init_ ## structname(void *, unsigned);                                                                                  \
    static bool pb_decode_ ## structname(pb_istream_t *, void *, unsigned, uint32_t, pb_wire_type_t);                                      \
    static void pb_release_ ## structname(void *, unsigned, uint32_t);                                                                     \
                                                                                                                                           \
    PB_INIT_GENERATE(msgname, structname)                                                                                                  \
    PB_DECODE_GENERATE(msgname, structname)                                                                                                \
    PB_RELEASE_GENERATE(msgname, structname)

#endif

#endif
