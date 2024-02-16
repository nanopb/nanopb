/* Attempts to test all the datatypes supported by ProtoBuf.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "alltypes.pb.h"
#include "test_helpers.h"

#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char **argv)
{
    /* Initialize the structure with constants */
    AllTypes alltypes = AllTypes_init_zero;

    alltypes.req_int32         = -1001;
    alltypes.req_int64         = -1002;
    alltypes.req_uint32        = 1003;
    alltypes.req_uint64        = 1004;
    alltypes.req_sint32        = -1005;
    alltypes.req_sint64        = -1006;
    alltypes.req_bool          = true;

    alltypes.req_fixed32       = 1008;
    alltypes.req_sfixed32      = -1009;
    alltypes.req_float         = 1010.0f;

    alltypes.req_fixed64       = 1011;
    alltypes.req_sfixed64      = -1012;
    alltypes.req_double        = 1013.0;

    strcpy(alltypes.req_string, "1014");
    alltypes.req_bytes.size = 4;
    memcpy(alltypes.req_bytes.bytes, "1015", 4);
    strcpy(alltypes.req_submsg.substuff1, "1016");
    alltypes.req_submsg.substuff2 = 1016;
    alltypes.req_enum = MyEnum_Truth;
    memcpy(alltypes.req_fbytes, "1019", 4);

    alltypes.rep_int32_count = 5; alltypes.rep_int32[4] = -2001;
    alltypes.rep_int64_count = 5; alltypes.rep_int64[4] = -2002;
    alltypes.rep_uint32_count = 5; alltypes.rep_uint32[4] = 2003;
    alltypes.rep_uint64_count = 5; alltypes.rep_uint64[4] = 2004;
    alltypes.rep_sint32_count = 5; alltypes.rep_sint32[4] = -2005;
    alltypes.rep_sint64_count = 5; alltypes.rep_sint64[4] = -2006;
    alltypes.rep_bool_count = 5; alltypes.rep_bool[4] = true;

    alltypes.rep_fixed32_count = 5; alltypes.rep_fixed32[4] = 2008;
    alltypes.rep_sfixed32_count = 5; alltypes.rep_sfixed32[4] = -2009;
    alltypes.rep_float_count = 5; alltypes.rep_float[4] = 2010.0f;

    alltypes.rep_fixed64_count = 5; alltypes.rep_fixed64[4] = 2011;
    alltypes.rep_sfixed64_count = 5; alltypes.rep_sfixed64[4] = -2012;
    alltypes.rep_double_count = 5; alltypes.rep_double[4] = 2013.0;

    alltypes.rep_string_count = 5; strcpy(alltypes.rep_string[4], "2014");
    alltypes.rep_bytes_count = 5; alltypes.rep_bytes[4].size = 4;
    memcpy(alltypes.rep_bytes[4].bytes, "2015", 4);

    alltypes.rep_submsg_count = 5;
    strcpy(alltypes.rep_submsg[4].substuff1, "2016");
    alltypes.rep_submsg[4].substuff2 = 2016;
    alltypes.rep_submsg[4].has_substuff3 = true;
    alltypes.rep_submsg[4].substuff3 = 2016;

    alltypes.rep_enum_count = 5; alltypes.rep_enum[4] = MyEnum_Truth;
    alltypes.rep_emptymsg_count = 5;

    alltypes.rep_fbytes_count = 5;
    memcpy(alltypes.rep_fbytes[4], "2019", 4);

    alltypes.rep_farray[4] = 2040;
    alltypes.rep_farray2[2] = 2095;

    alltypes.req_limits.int32_min  = INT32_MIN;
    alltypes.req_limits.int32_max  = INT32_MAX;
    alltypes.req_limits.uint32_min = 0;
    alltypes.req_limits.uint32_max = UINT32_MAX;
    alltypes.req_limits.int64_min  = INT64_MIN;
    alltypes.req_limits.int64_max  = INT64_MAX;
    alltypes.req_limits.uint64_min = 0;
    alltypes.req_limits.uint64_max = UINT64_MAX;
    alltypes.req_limits.enum_min   = HugeEnum_Negative;
    alltypes.req_limits.enum_max   = HugeEnum_Positive;
    alltypes.req_limits.largetag   = 1001;

    alltypes.req_ds8.first = 9991;
    alltypes.req_ds8.second = 9992;

    alltypes.req_intsizes.req_int8 = -128;
    alltypes.req_intsizes.req_uint8 = 255;
    alltypes.req_intsizes.req_sint8 = -128;
    alltypes.req_intsizes.req_int16 = -32768;
    alltypes.req_intsizes.req_uint16 = 65535;
    alltypes.req_intsizes.req_sint16 = -32768;

    alltypes.end = 1099;

    uint8_t buffer[AllTypes_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    stream << alltypes;

    uint8_t buffer2[AllTypes_size];
    pb_ostream_t stream2 = pb_ostream_from_buffer(buffer2, sizeof(buffer2));

    /* Now encode it and check if we succeeded. */
    if (AllTypes::encode_req_int32(stream2, alltypes.req_int32) &&
        AllTypes::encode_req_int64(stream2, alltypes.req_int64) &&
        AllTypes::encode_req_uint32(stream2, alltypes.req_uint32) &&
        AllTypes::encode_req_uint64(stream2, alltypes.req_uint64) &&
        AllTypes::encode_req_sint32(stream2, alltypes.req_sint32) &&
        AllTypes::encode_req_sint64(stream2, alltypes.req_sint64) &&
        AllTypes::encode_req_bool(stream2, alltypes.req_bool) &&
        AllTypes::encode_req_fixed32(stream2, alltypes.req_fixed32) &&
        AllTypes::encode_req_sfixed32(stream2, alltypes.req_sfixed32) &&
        AllTypes::encode_req_float(stream2, alltypes.req_float) &&
        AllTypes::encode_req_fixed64(stream2, alltypes.req_fixed64) &&
        AllTypes::encode_req_sfixed64(stream2, alltypes.req_sfixed64) &&
        AllTypes::encode_req_double(stream2, alltypes.req_double) &&
        AllTypes::encode_req_string(stream2, alltypes.req_string) &&
        AllTypes::encode_req_bytes(stream2, alltypes.req_bytes) &&
        AllTypes::encode_req_submsg(stream2, alltypes.req_submsg) &&
        AllTypes::encode_req_enum(stream2, alltypes.req_enum) &&
        AllTypes::encode_req_emptymsg(stream2, alltypes.req_emptymsg) &&
        AllTypes::encode_req_fbytes(stream2, alltypes.req_fbytes) &&
        AllTypes::encode_rep_int32_pack(stream2, alltypes.rep_int32, alltypes.rep_int32_count) &&
        AllTypes::encode_rep_int64_pack(stream2, alltypes.rep_int64, alltypes.rep_int64_count) &&
        AllTypes::encode_rep_uint32_pack(stream2, alltypes.rep_uint32, alltypes.rep_uint32_count) &&
        AllTypes::encode_rep_uint64_pack(stream2, alltypes.rep_uint64, alltypes.rep_uint64_count) &&
        AllTypes::encode_rep_sint32_pack(stream2, alltypes.rep_sint32, alltypes.rep_sint32_count) &&
        AllTypes::encode_rep_sint64_pack(stream2, alltypes.rep_sint64, alltypes.rep_sint64_count) &&
        AllTypes::encode_rep_bool_pack(stream2, alltypes.rep_bool, alltypes.rep_bool_count) &&
        AllTypes::encode_rep_fixed32_pack(stream2, alltypes.rep_fixed32, alltypes.rep_fixed32_count) &&
        AllTypes::encode_rep_sfixed32_pack(stream2, alltypes.rep_sfixed32, alltypes.rep_sfixed32_count) &&
        AllTypes::encode_rep_float_pack(stream2, alltypes.rep_float, alltypes.rep_float_count) &&
        AllTypes::encode_rep_fixed64_pack(stream2, alltypes.rep_fixed64, alltypes.rep_fixed64_count) &&
        AllTypes::encode_rep_sfixed64_pack(stream2, alltypes.rep_sfixed64, alltypes.rep_sfixed64_count) &&
        AllTypes::encode_rep_double_pack(stream2, alltypes.rep_double, alltypes.rep_double_count) &&
        AllTypes::encode_rep_string(stream2, alltypes.rep_string[0]) &&
        AllTypes::encode_rep_string(stream2, alltypes.rep_string[1]) &&
        AllTypes::encode_rep_string(stream2, alltypes.rep_string[2]) &&
        AllTypes::encode_rep_string(stream2, alltypes.rep_string[3]) &&
        AllTypes::encode_rep_string(stream2, alltypes.rep_string[4]) &&
        AllTypes::encode_rep_bytes(stream2, alltypes.rep_bytes[0]) &&
        AllTypes::encode_rep_bytes(stream2, alltypes.rep_bytes[1]) &&
        AllTypes::encode_rep_bytes(stream2, alltypes.rep_bytes[2]) &&
        AllTypes::encode_rep_bytes(stream2, alltypes.rep_bytes[3]) &&
        AllTypes::encode_rep_bytes(stream2, alltypes.rep_bytes[4]) &&
        AllTypes::encode_rep_submsg(stream2, alltypes.rep_submsg[0]) &&
        AllTypes::encode_rep_submsg(stream2, alltypes.rep_submsg[1]) &&
        AllTypes::encode_rep_submsg(stream2, alltypes.rep_submsg[2]) &&
        AllTypes::encode_rep_submsg(stream2, alltypes.rep_submsg[3]) &&
        AllTypes::encode_rep_submsg(stream2, alltypes.rep_submsg[4]) &&
        AllTypes::encode_rep_enum_pack(stream2, alltypes.rep_enum, alltypes.rep_enum_count) &&
        AllTypes::encode_rep_emptymsg(stream2, alltypes.rep_emptymsg[0]) &&
        AllTypes::encode_rep_emptymsg(stream2, alltypes.rep_emptymsg[1]) &&
        AllTypes::encode_rep_emptymsg(stream2, alltypes.rep_emptymsg[2]) &&
        AllTypes::encode_rep_emptymsg(stream2, alltypes.rep_emptymsg[3]) &&
        AllTypes::encode_rep_emptymsg(stream2, alltypes.rep_emptymsg[4]) &&
        AllTypes::encode_rep_fbytes(stream2, alltypes.rep_fbytes[0]) &&
        AllTypes::encode_rep_fbytes(stream2, alltypes.rep_fbytes[1]) &&
        AllTypes::encode_rep_fbytes(stream2, alltypes.rep_fbytes[2]) &&
        AllTypes::encode_rep_fbytes(stream2, alltypes.rep_fbytes[3]) &&
        AllTypes::encode_rep_fbytes(stream2, alltypes.rep_fbytes[4]) &&
        AllTypes::encode_rep_farray_pack(stream2, alltypes.rep_farray, 5) &&
        AllTypes::encode_rep_farray2_pack(stream2, alltypes.rep_farray2, 3) &&
        AllTypes::encode_req_intsizes(stream2, alltypes.req_intsizes) &&
        AllTypes::encode_req_ds8(stream2, alltypes.req_ds8) &&
        AllTypes::encode_req_limits(stream2, alltypes.req_limits) &&
        AllTypes::encode_end(stream2, alltypes.end))
    {
        if (stream.bytes_written == stream2.bytes_written && memcmp(buffer, buffer2, stream.bytes_written) == 0)
        {
            SET_BINARY_MODE(stdout);
            fwrite(buffer, 1, stream2.bytes_written, stdout);
            return 0; /* Success */
        }
        else
        {
            fprintf(stderr, "Encoding failed: Manual encoding mismatch\n");
            return 1; /* Failure */
        }
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream2));
        return 1; /* Failure */
    }
}
