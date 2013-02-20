/* Automatically generated nanopb header */
/* This is a file generated using nanopb-0.2.0-dev.
 * It is used as a part of test suite in order to detect any
 * incompatible changes made to the generator in future versions.
 */

#ifndef _PB_ALLTYPES_PB_H_
#define _PB_ALLTYPES_PB_H_
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _MyEnum {
    MyEnum_Zero = 0,
    MyEnum_First = 1,
    MyEnum_Second = 2,
    MyEnum_Truth = 42
} MyEnum;

/* Struct definitions */
typedef struct _SubMessage {
    char substuff1[16];
    int32_t substuff2;
    bool has_substuff3;
    uint32_t substuff3;
} SubMessage;

typedef struct {
    size_t size;
    uint8_t bytes[16];
} AllTypes_req_bytes_t;

typedef struct {
    size_t size;
    uint8_t bytes[16];
} AllTypes_rep_bytes_t;

typedef struct {
    size_t size;
    uint8_t bytes[16];
} AllTypes_opt_bytes_t;

typedef struct _AllTypes {
    int32_t req_int32;
    int64_t req_int64;
    uint32_t req_uint32;
    uint64_t req_uint64;
    int32_t req_sint32;
    int64_t req_sint64;
    bool req_bool;
    uint32_t req_fixed32;
    int32_t req_sfixed32;
    float req_float;
    uint64_t req_fixed64;
    int64_t req_sfixed64;
    double req_double;
    char req_string[16];
    AllTypes_req_bytes_t req_bytes;
    SubMessage req_submsg;
    MyEnum req_enum;
    size_t rep_int32_count;
    int32_t rep_int32[5];
    size_t rep_int64_count;
    int64_t rep_int64[5];
    size_t rep_uint32_count;
    uint32_t rep_uint32[5];
    size_t rep_uint64_count;
    uint64_t rep_uint64[5];
    size_t rep_sint32_count;
    int32_t rep_sint32[5];
    size_t rep_sint64_count;
    int64_t rep_sint64[5];
    size_t rep_bool_count;
    bool rep_bool[5];
    size_t rep_fixed32_count;
    uint32_t rep_fixed32[5];
    size_t rep_sfixed32_count;
    int32_t rep_sfixed32[5];
    size_t rep_float_count;
    float rep_float[5];
    size_t rep_fixed64_count;
    uint64_t rep_fixed64[5];
    size_t rep_sfixed64_count;
    int64_t rep_sfixed64[5];
    size_t rep_double_count;
    double rep_double[5];
    size_t rep_string_count;
    char rep_string[5][16];
    size_t rep_bytes_count;
    AllTypes_rep_bytes_t rep_bytes[5];
    size_t rep_submsg_count;
    SubMessage rep_submsg[5];
    size_t rep_enum_count;
    MyEnum rep_enum[5];
    bool has_opt_int32;
    int32_t opt_int32;
    bool has_opt_int64;
    int64_t opt_int64;
    bool has_opt_uint32;
    uint32_t opt_uint32;
    bool has_opt_uint64;
    uint64_t opt_uint64;
    bool has_opt_sint32;
    int32_t opt_sint32;
    bool has_opt_sint64;
    int64_t opt_sint64;
    bool has_opt_bool;
    bool opt_bool;
    bool has_opt_fixed32;
    uint32_t opt_fixed32;
    bool has_opt_sfixed32;
    int32_t opt_sfixed32;
    bool has_opt_float;
    float opt_float;
    bool has_opt_fixed64;
    uint64_t opt_fixed64;
    bool has_opt_sfixed64;
    int64_t opt_sfixed64;
    bool has_opt_double;
    double opt_double;
    bool has_opt_string;
    char opt_string[16];
    bool has_opt_bytes;
    AllTypes_opt_bytes_t opt_bytes;
    bool has_opt_submsg;
    SubMessage opt_submsg;
    bool has_opt_enum;
    MyEnum opt_enum;
    int32_t end;
} AllTypes;

/* Default values for struct fields */
extern const char SubMessage_substuff1_default[16];
extern const int32_t SubMessage_substuff2_default;
extern const uint32_t SubMessage_substuff3_default;
extern const int32_t AllTypes_opt_int32_default;
extern const int64_t AllTypes_opt_int64_default;
extern const uint32_t AllTypes_opt_uint32_default;
extern const uint64_t AllTypes_opt_uint64_default;
extern const int32_t AllTypes_opt_sint32_default;
extern const int64_t AllTypes_opt_sint64_default;
extern const bool AllTypes_opt_bool_default;
extern const uint32_t AllTypes_opt_fixed32_default;
extern const int32_t AllTypes_opt_sfixed32_default;
extern const float AllTypes_opt_float_default;
extern const uint64_t AllTypes_opt_fixed64_default;
extern const int64_t AllTypes_opt_sfixed64_default;
extern const double AllTypes_opt_double_default;
extern const char AllTypes_opt_string_default[16];
extern const AllTypes_opt_bytes_t AllTypes_opt_bytes_default;
extern const MyEnum AllTypes_opt_enum_default;

/* Struct field encoding specification for nanopb */
extern const pb_field_t SubMessage_fields[4];
extern const pb_field_t AllTypes_fields[53];

/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
STATIC_ASSERT((pb_membersize(AllTypes, req_submsg) < 256 && pb_membersize(AllTypes, rep_submsg[0]) < 256 && pb_membersize(AllTypes, opt_submsg) < 256), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_SubMessage_AllTypes)
#endif

#if !defined(PB_FIELD_32BIT)
STATIC_ASSERT((pb_membersize(AllTypes, req_submsg) < 65536 && pb_membersize(AllTypes, rep_submsg[0]) < 65536 && pb_membersize(AllTypes, opt_submsg) < 65536), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_SubMessage_AllTypes)
#endif

/* On some platforms (such as AVR), double is really float.
 * These are not directly supported by nanopb, but see example_avr_double.
 */
STATIC_ASSERT(sizeof(double) == 8, DOUBLE_MUST_BE_8_BYTES)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
