/* Attempts to test all the datatypes supported by ProtoBuf.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "alltypes.pb.h"
#include "test_helpers.h"

int main(int argc, char **argv)
{
    int mode = (argc > 1) ? atoi(argv[1]) : 0;
    
    /* Initialize values to encode */
    int32_t value_int32 = -1000;
    int64_t value_int64 = -10000000000;

    uint32_t value_uint32 = 1000;
    uint64_t value_uint64 = 10000000000;

    bool value_bool = true;
    float value_float = 1000.0f;
    double value_double = 1000.0f;

    char *value_string = "1000";
    AllTypes_req_bytes_t value_req_bytes;
    AllTypes_rep_bytes_t value_rep_bytes;
    AllTypes_opt_bytes_t value_opt_bytes;

    SubMessage value_submessage = {0};
    MyEnum value_enum = MyEnum_Truth;
    EmptyMessage value_empty_message = {0};

    /* Initialize the structure with constants */
    AllTypes alltypes = {0};

    alltypes.req_int32         = &value_int32;
    alltypes.req_int64         = &value_int64;
    alltypes.req_uint32        = &value_uint32;
    alltypes.req_uint64        = &value_uint64;
    alltypes.req_sint32        = &value_int32;
    alltypes.req_sint64        = &value_int64;
    alltypes.req_bool          = &value_bool;
    
    alltypes.req_fixed32       = &value_uint32;
    alltypes.req_sfixed32      = &value_int32;
    alltypes.req_float         = &value_float;
    
    alltypes.req_fixed64       = &value_uint64;
    alltypes.req_sfixed64      = &value_int64;
    alltypes.req_double        = &value_double;

    value_req_bytes.bytes = (uint8_t*)"1000";
    value_req_bytes.size  = 4;
    
    alltypes.req_string        = value_string;
    alltypes.req_bytes         = &value_req_bytes;

    value_submessage.substuff1 = value_string;
    value_submessage.substuff2 = &value_int32;

    alltypes.req_submsg        = &value_submessage;
    alltypes.req_enum          = &value_enum;
    alltypes.req_emptymsg      = &value_empty_message;
    
    alltypes.rep_int32_count = 1; alltypes.rep_int32 = &value_int32;
    alltypes.rep_int64_count = 1; alltypes.rep_int64 = &value_int64;
    alltypes.rep_uint32_count = 1; alltypes.rep_uint32 = &value_uint32;
    alltypes.rep_uint64_count = 1; alltypes.rep_uint64 = &value_uint64;
    alltypes.rep_sint32_count = 1; alltypes.rep_sint32 = &value_int32;
    alltypes.rep_sint64_count = 1; alltypes.rep_sint64 = &value_int64;
    alltypes.rep_bool_count = 1; alltypes.rep_bool = &value_bool;
    
    alltypes.rep_fixed32_count = 1; alltypes.rep_fixed32 = &value_uint32;
    alltypes.rep_sfixed32_count = 1; alltypes.rep_sfixed32 = &value_int32;
    alltypes.rep_float_count = 1; alltypes.rep_float = &value_float;
    
    alltypes.rep_fixed64_count = 1; alltypes.rep_fixed64 = &value_uint64;
    alltypes.rep_sfixed64_count = 1; alltypes.rep_sfixed64 = &value_int64;
    alltypes.rep_double_count = 1; alltypes.rep_double = &value_double;

    value_rep_bytes.bytes = (uint8_t*)"1000";
    value_rep_bytes.size  = 4;

    alltypes.rep_string_count = 1; alltypes.rep_string = (char **)&value_string;
    alltypes.rep_bytes_count = 0; alltypes.rep_bytes = &value_rep_bytes;

    alltypes.rep_submsg_count = 1; alltypes.rep_submsg = &value_submessage;
    alltypes.rep_enum_count = 1; alltypes.rep_enum = &value_enum;
    alltypes.rep_emptymsg_count = 1; alltypes.rep_emptymsg = &value_empty_message;
    
    if (mode != 0)
    {
        /* Fill in values for optional fields */
      alltypes.opt_int32         = &value_int32;
      alltypes.opt_int64         = &value_int64;
      alltypes.opt_uint32        = &value_uint32;
      alltypes.opt_uint64        = &value_uint64;
      alltypes.opt_sint32        = &value_int32;
      alltypes.opt_sint64        = &value_int64;
      alltypes.opt_bool          = &value_bool;
    
      alltypes.opt_fixed32       = &value_uint32;
      alltypes.opt_sfixed32      = &value_int32;
      alltypes.opt_float         = &value_float;
    
      alltypes.opt_fixed64       = &value_uint64;
      alltypes.opt_sfixed64      = &value_int64;
      alltypes.opt_double        = &value_double;
    
      value_opt_bytes.bytes = (uint8_t*)"1000";
      value_opt_bytes.size  = 4;
    
      alltypes.opt_string        = value_string;
      alltypes.opt_bytes         = &value_opt_bytes;

      alltypes.opt_submsg        = &value_submessage;
      alltypes.opt_enum          = &value_enum;
      alltypes.opt_emptymsg      = &value_empty_message;
    }
    
    alltypes.end = &value_int32;
    
    {
        uint8_t buffer[4096];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        
        /* Now encode it and check if we succeeded. */
        if (pb_encode(&stream, AllTypes_fields, &alltypes))
        {
            /*SET_BINARY_MODE(stdout);
            fwrite(buffer, 1, stream.bytes_written, stdout);*/     /* TODO: use this to validate decoding, when implemented */
            return 0; /* Success */
        }
        else
        {
            fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1; /* Failure */
        }
    }
}
