/* Automatically generated nanopb constant definitions */
#include "bc_alltypes.pb.h"

const char SubMessage_substuff1_default[17] = "1";
const int32_t SubMessage_substuff2_default = 2;
const uint32_t SubMessage_substuff3_default = 3;
const int32_t AllTypes_opt_int32_default = 4041;
const int64_t AllTypes_opt_int64_default = 4042;
const uint32_t AllTypes_opt_uint32_default = 4043;
const uint64_t AllTypes_opt_uint64_default = 4044;
const int32_t AllTypes_opt_sint32_default = 4045;
const int64_t AllTypes_opt_sint64_default = 4046;
const bool AllTypes_opt_bool_default = false;
const uint32_t AllTypes_opt_fixed32_default = 4048;
const int32_t AllTypes_opt_sfixed32_default = 4049;
const float AllTypes_opt_float_default = 4050;
const uint64_t AllTypes_opt_fixed64_default = 4051;
const int64_t AllTypes_opt_sfixed64_default = 4052;
const double AllTypes_opt_double_default = 4053;
const char AllTypes_opt_string_default[17] = "4054";
const AllTypes_opt_bytes_t AllTypes_opt_bytes_default = {4, {0x34,0x30,0x35,0x35}};
const MyEnum AllTypes_opt_enum_default = MyEnum_Second;


const pb_field_t SubMessage_fields[4] = {
    {1, PB_HTYPE_REQUIRED | PB_LTYPE_STRING,
    offsetof(SubMessage, substuff1), 0,
    pb_membersize(SubMessage, substuff1), 0,
    &SubMessage_substuff1_default},

    {2, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(SubMessage, substuff2, substuff1), 0,
    pb_membersize(SubMessage, substuff2), 0,
    &SubMessage_substuff2_default},

    {3, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED32,
    pb_delta_end(SubMessage, substuff3, substuff2),
    pb_delta(SubMessage, has_substuff3, substuff3),
    pb_membersize(SubMessage, substuff3), 0,
    &SubMessage_substuff3_default},

    PB_LAST_FIELD
};

const pb_field_t AllTypes_fields[53] = {
    {1, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    offsetof(AllTypes, req_int32), 0,
    pb_membersize(AllTypes, req_int32), 0, 0},

    {2, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, req_int64, req_int32), 0,
    pb_membersize(AllTypes, req_int64), 0, 0},

    {3, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, req_uint32, req_int64), 0,
    pb_membersize(AllTypes, req_uint32), 0, 0},

    {4, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, req_uint64, req_uint32), 0,
    pb_membersize(AllTypes, req_uint64), 0, 0},

    {5, PB_HTYPE_REQUIRED | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, req_sint32, req_uint64), 0,
    pb_membersize(AllTypes, req_sint32), 0, 0},

    {6, PB_HTYPE_REQUIRED | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, req_sint64, req_sint32), 0,
    pb_membersize(AllTypes, req_sint64), 0, 0},

    {7, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, req_bool, req_sint64), 0,
    pb_membersize(AllTypes, req_bool), 0, 0},

    {8, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, req_fixed32, req_bool), 0,
    pb_membersize(AllTypes, req_fixed32), 0, 0},

    {9, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, req_sfixed32, req_fixed32), 0,
    pb_membersize(AllTypes, req_sfixed32), 0, 0},

    {10, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, req_float, req_sfixed32), 0,
    pb_membersize(AllTypes, req_float), 0, 0},

    {11, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, req_fixed64, req_float), 0,
    pb_membersize(AllTypes, req_fixed64), 0, 0},

    {12, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, req_sfixed64, req_fixed64), 0,
    pb_membersize(AllTypes, req_sfixed64), 0, 0},

    {13, PB_HTYPE_REQUIRED | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, req_double, req_sfixed64), 0,
    pb_membersize(AllTypes, req_double), 0, 0},

    {14, PB_HTYPE_REQUIRED | PB_LTYPE_STRING,
    pb_delta_end(AllTypes, req_string, req_double), 0,
    pb_membersize(AllTypes, req_string), 0, 0},

    {15, PB_HTYPE_REQUIRED | PB_LTYPE_BYTES,
    pb_delta_end(AllTypes, req_bytes, req_string), 0,
    pb_membersize(AllTypes, req_bytes), 0, 0},

    {16, PB_HTYPE_REQUIRED | PB_LTYPE_SUBMESSAGE,
    pb_delta_end(AllTypes, req_submsg, req_bytes), 0,
    pb_membersize(AllTypes, req_submsg), 0,
    &SubMessage_fields},

    {17, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, req_enum, req_submsg), 0,
    pb_membersize(AllTypes, req_enum), 0, 0},

    {21, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_int32, req_enum),
    pb_delta(AllTypes, rep_int32_count, rep_int32),
    pb_membersize(AllTypes, rep_int32[0]),
    pb_membersize(AllTypes, rep_int32) / pb_membersize(AllTypes, rep_int32[0]), 0},

    {22, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_int64, rep_int32),
    pb_delta(AllTypes, rep_int64_count, rep_int64),
    pb_membersize(AllTypes, rep_int64[0]),
    pb_membersize(AllTypes, rep_int64) / pb_membersize(AllTypes, rep_int64[0]), 0},

    {23, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_uint32, rep_int64),
    pb_delta(AllTypes, rep_uint32_count, rep_uint32),
    pb_membersize(AllTypes, rep_uint32[0]),
    pb_membersize(AllTypes, rep_uint32) / pb_membersize(AllTypes, rep_uint32[0]), 0},

    {24, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_uint64, rep_uint32),
    pb_delta(AllTypes, rep_uint64_count, rep_uint64),
    pb_membersize(AllTypes, rep_uint64[0]),
    pb_membersize(AllTypes, rep_uint64) / pb_membersize(AllTypes, rep_uint64[0]), 0},

    {25, PB_HTYPE_ARRAY | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, rep_sint32, rep_uint64),
    pb_delta(AllTypes, rep_sint32_count, rep_sint32),
    pb_membersize(AllTypes, rep_sint32[0]),
    pb_membersize(AllTypes, rep_sint32) / pb_membersize(AllTypes, rep_sint32[0]), 0},

    {26, PB_HTYPE_ARRAY | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, rep_sint64, rep_sint32),
    pb_delta(AllTypes, rep_sint64_count, rep_sint64),
    pb_membersize(AllTypes, rep_sint64[0]),
    pb_membersize(AllTypes, rep_sint64) / pb_membersize(AllTypes, rep_sint64[0]), 0},

    {27, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_bool, rep_sint64),
    pb_delta(AllTypes, rep_bool_count, rep_bool),
    pb_membersize(AllTypes, rep_bool[0]),
    pb_membersize(AllTypes, rep_bool) / pb_membersize(AllTypes, rep_bool[0]), 0},

    {28, PB_HTYPE_ARRAY | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, rep_fixed32, rep_bool),
    pb_delta(AllTypes, rep_fixed32_count, rep_fixed32),
    pb_membersize(AllTypes, rep_fixed32[0]),
    pb_membersize(AllTypes, rep_fixed32) / pb_membersize(AllTypes, rep_fixed32[0]), 0},

    {29, PB_HTYPE_ARRAY | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, rep_sfixed32, rep_fixed32),
    pb_delta(AllTypes, rep_sfixed32_count, rep_sfixed32),
    pb_membersize(AllTypes, rep_sfixed32[0]),
    pb_membersize(AllTypes, rep_sfixed32) / pb_membersize(AllTypes, rep_sfixed32[0]), 0},

    {30, PB_HTYPE_ARRAY | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, rep_float, rep_sfixed32),
    pb_delta(AllTypes, rep_float_count, rep_float),
    pb_membersize(AllTypes, rep_float[0]),
    pb_membersize(AllTypes, rep_float) / pb_membersize(AllTypes, rep_float[0]), 0},

    {31, PB_HTYPE_ARRAY | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, rep_fixed64, rep_float),
    pb_delta(AllTypes, rep_fixed64_count, rep_fixed64),
    pb_membersize(AllTypes, rep_fixed64[0]),
    pb_membersize(AllTypes, rep_fixed64) / pb_membersize(AllTypes, rep_fixed64[0]), 0},

    {32, PB_HTYPE_ARRAY | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, rep_sfixed64, rep_fixed64),
    pb_delta(AllTypes, rep_sfixed64_count, rep_sfixed64),
    pb_membersize(AllTypes, rep_sfixed64[0]),
    pb_membersize(AllTypes, rep_sfixed64) / pb_membersize(AllTypes, rep_sfixed64[0]), 0},

    {33, PB_HTYPE_ARRAY | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, rep_double, rep_sfixed64),
    pb_delta(AllTypes, rep_double_count, rep_double),
    pb_membersize(AllTypes, rep_double[0]),
    pb_membersize(AllTypes, rep_double) / pb_membersize(AllTypes, rep_double[0]), 0},

    {34, PB_HTYPE_ARRAY | PB_LTYPE_STRING,
    pb_delta_end(AllTypes, rep_string, rep_double),
    pb_delta(AllTypes, rep_string_count, rep_string),
    pb_membersize(AllTypes, rep_string[0]),
    pb_membersize(AllTypes, rep_string) / pb_membersize(AllTypes, rep_string[0]), 0},

    {35, PB_HTYPE_ARRAY | PB_LTYPE_BYTES,
    pb_delta_end(AllTypes, rep_bytes, rep_string),
    pb_delta(AllTypes, rep_bytes_count, rep_bytes),
    pb_membersize(AllTypes, rep_bytes[0]),
    pb_membersize(AllTypes, rep_bytes) / pb_membersize(AllTypes, rep_bytes[0]), 0},

    {36, PB_HTYPE_ARRAY | PB_LTYPE_SUBMESSAGE,
    pb_delta_end(AllTypes, rep_submsg, rep_bytes),
    pb_delta(AllTypes, rep_submsg_count, rep_submsg),
    pb_membersize(AllTypes, rep_submsg[0]),
    pb_membersize(AllTypes, rep_submsg) / pb_membersize(AllTypes, rep_submsg[0]),
    &SubMessage_fields},

    {37, PB_HTYPE_ARRAY | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, rep_enum, rep_submsg),
    pb_delta(AllTypes, rep_enum_count, rep_enum),
    pb_membersize(AllTypes, rep_enum[0]),
    pb_membersize(AllTypes, rep_enum) / pb_membersize(AllTypes, rep_enum[0]), 0},

    {41, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_int32, rep_enum),
    pb_delta(AllTypes, has_opt_int32, opt_int32),
    pb_membersize(AllTypes, opt_int32), 0,
    &AllTypes_opt_int32_default},

    {42, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_int64, opt_int32),
    pb_delta(AllTypes, has_opt_int64, opt_int64),
    pb_membersize(AllTypes, opt_int64), 0,
    &AllTypes_opt_int64_default},

    {43, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_uint32, opt_int64),
    pb_delta(AllTypes, has_opt_uint32, opt_uint32),
    pb_membersize(AllTypes, opt_uint32), 0,
    &AllTypes_opt_uint32_default},

    {44, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_uint64, opt_uint32),
    pb_delta(AllTypes, has_opt_uint64, opt_uint64),
    pb_membersize(AllTypes, opt_uint64), 0,
    &AllTypes_opt_uint64_default},

    {45, PB_HTYPE_OPTIONAL | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, opt_sint32, opt_uint64),
    pb_delta(AllTypes, has_opt_sint32, opt_sint32),
    pb_membersize(AllTypes, opt_sint32), 0,
    &AllTypes_opt_sint32_default},

    {46, PB_HTYPE_OPTIONAL | PB_LTYPE_SVARINT,
    pb_delta_end(AllTypes, opt_sint64, opt_sint32),
    pb_delta(AllTypes, has_opt_sint64, opt_sint64),
    pb_membersize(AllTypes, opt_sint64), 0,
    &AllTypes_opt_sint64_default},

    {47, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_bool, opt_sint64),
    pb_delta(AllTypes, has_opt_bool, opt_bool),
    pb_membersize(AllTypes, opt_bool), 0,
    &AllTypes_opt_bool_default},

    {48, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, opt_fixed32, opt_bool),
    pb_delta(AllTypes, has_opt_fixed32, opt_fixed32),
    pb_membersize(AllTypes, opt_fixed32), 0,
    &AllTypes_opt_fixed32_default},

    {49, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, opt_sfixed32, opt_fixed32),
    pb_delta(AllTypes, has_opt_sfixed32, opt_sfixed32),
    pb_membersize(AllTypes, opt_sfixed32), 0,
    &AllTypes_opt_sfixed32_default},

    {50, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED32,
    pb_delta_end(AllTypes, opt_float, opt_sfixed32),
    pb_delta(AllTypes, has_opt_float, opt_float),
    pb_membersize(AllTypes, opt_float), 0,
    &AllTypes_opt_float_default},

    {51, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, opt_fixed64, opt_float),
    pb_delta(AllTypes, has_opt_fixed64, opt_fixed64),
    pb_membersize(AllTypes, opt_fixed64), 0,
    &AllTypes_opt_fixed64_default},

    {52, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, opt_sfixed64, opt_fixed64),
    pb_delta(AllTypes, has_opt_sfixed64, opt_sfixed64),
    pb_membersize(AllTypes, opt_sfixed64), 0,
    &AllTypes_opt_sfixed64_default},

    {53, PB_HTYPE_OPTIONAL | PB_LTYPE_FIXED64,
    pb_delta_end(AllTypes, opt_double, opt_sfixed64),
    pb_delta(AllTypes, has_opt_double, opt_double),
    pb_membersize(AllTypes, opt_double), 0,
    &AllTypes_opt_double_default},

    {54, PB_HTYPE_OPTIONAL | PB_LTYPE_STRING,
    pb_delta_end(AllTypes, opt_string, opt_double),
    pb_delta(AllTypes, has_opt_string, opt_string),
    pb_membersize(AllTypes, opt_string), 0,
    &AllTypes_opt_string_default},

    {55, PB_HTYPE_OPTIONAL | PB_LTYPE_BYTES,
    pb_delta_end(AllTypes, opt_bytes, opt_string),
    pb_delta(AllTypes, has_opt_bytes, opt_bytes),
    pb_membersize(AllTypes, opt_bytes), 0,
    &AllTypes_opt_bytes_default},

    {56, PB_HTYPE_OPTIONAL | PB_LTYPE_SUBMESSAGE,
    pb_delta_end(AllTypes, opt_submsg, opt_bytes),
    pb_delta(AllTypes, has_opt_submsg, opt_submsg),
    pb_membersize(AllTypes, opt_submsg), 0,
    &SubMessage_fields},

    {57, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, opt_enum, opt_submsg),
    pb_delta(AllTypes, has_opt_enum, opt_enum),
    pb_membersize(AllTypes, opt_enum), 0,
    &AllTypes_opt_enum_default},

    {99, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
    pb_delta_end(AllTypes, end, opt_enum), 0,
    pb_membersize(AllTypes, end), 0, 0},

    PB_LAST_FIELD
};

