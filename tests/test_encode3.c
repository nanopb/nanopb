/* Attempts to test all the datatypes supported by ProtoBuf.
 * Currently only tests the 'required' variety.
 */

#include <stdio.h>
#include <string.h>
#include <pb_encode.h>
#include "alltypes.pb.h"

int main()
{
    /* Initialize the structure with constants */
    AllTypes alltypes = {0};
    
    alltypes.req_int32         = 1001;
    alltypes.req_int64         = 1002;
    alltypes.req_uint32        = 1003;
    alltypes.req_uint64        = 1004;
    alltypes.req_sint32        = 1005;
    alltypes.req_sint64        = 1006;
    alltypes.req_bool          = true;
    
    alltypes.req_fixed32       = 1008;
    alltypes.req_sfixed32      = 1009;
    alltypes.req_float         = 1010.0f;
    
    alltypes.req_fixed64       = 1011;
    alltypes.req_sfixed64      = 1012;
    alltypes.req_double        = 1013.0;
    
    strcpy(alltypes.req_string, "1014");
    alltypes.req_bytes.size = 4;
    memcpy(alltypes.req_bytes.bytes, "1015", 4);
    strcpy(alltypes.req_submsg.substuff1, "1016");
    alltypes.req_submsg.substuff2 = 1016;
    alltypes.req_enum = MyEnum_Truth;
    
    alltypes.rep_int32_count = 5; alltypes.rep_int32[4] = 2001;
    alltypes.rep_int64_count = 5; alltypes.rep_int64[4] = 2002;
    alltypes.rep_uint32_count = 5; alltypes.rep_uint32[4] = 2003;
    alltypes.rep_uint64_count = 5; alltypes.rep_uint64[4] = 2004;
    alltypes.rep_sint32_count = 5; alltypes.rep_sint32[4] = 2005;
    alltypes.rep_sint64_count = 5; alltypes.rep_sint64[4] = 2006;
    alltypes.rep_bool_count = 5; alltypes.rep_bool[4] = true;
    
    alltypes.rep_fixed32_count = 5; alltypes.rep_fixed32[4] = 2008;
    alltypes.rep_sfixed32_count = 5; alltypes.rep_sfixed32[4] = 2009;
    alltypes.rep_float_count = 5; alltypes.rep_float[4] = 2010.0f;
    
    alltypes.rep_fixed64_count = 5; alltypes.rep_fixed64[4] = 2011;
    alltypes.rep_sfixed64_count = 5; alltypes.rep_sfixed64[4] = 2012;
    alltypes.rep_double_count = 5; alltypes.rep_double[4] = 2013.0;
    
    alltypes.rep_string_count = 5; strcpy(alltypes.rep_string[4], "2014");
    alltypes.rep_bytes_count = 5; alltypes.rep_bytes[4].size = 4;
    memcpy(alltypes.rep_bytes[4].bytes, "2015", 4);

    alltypes.rep_submsg_count = 5;
    strcpy(alltypes.rep_submsg[4].substuff1, "2016");
    alltypes.rep_submsg[4].substuff2 = 2016;
    
    alltypes.rep_enum_count = 5; alltypes.rep_enum[4] = MyEnum_Truth;
    
    alltypes.end = 1099;
    
    uint8_t buffer[512];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    /* Now encode it and check if we succeeded. */
    if (pb_encode(&stream, AllTypes_fields, &alltypes))
    {
        fwrite(buffer, 1, stream.bytes_written, stdout);
        return 0; /* Success */
    }
    else
    {
        return 1; /* Failure */
    }
}
