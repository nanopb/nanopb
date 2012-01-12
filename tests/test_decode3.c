/* Tests the decoding of all types. Currently only in the 'required' variety.
 * This is the counterpart of test_encode3.
 * Run e.g. ./test_encode3 | ./test_decode3
 */

#include <stdio.h>
#include <string.h>
#include <pb_decode.h>
#include "alltypes.pb.h"

#define TEST(x) if (!(x)) { \
    printf("Test " #x " failed.\n"); \
    return false; \
    }

/* This function is called once from main(), it handles
   the decoding and checks the fields. */
bool check_alltypes(pb_istream_t *stream)
{
    AllTypes alltypes = {};
    
    if (!pb_decode(stream, AllTypes_fields, &alltypes))
        return false;
    
    TEST(alltypes.req_int32         == 1001);
    TEST(alltypes.req_int64         == 1002);
    TEST(alltypes.req_uint32        == 1003);
    TEST(alltypes.req_uint64        == 1004);
    TEST(alltypes.req_sint32        == 1005);
    TEST(alltypes.req_sint64        == 1006);
    TEST(alltypes.req_bool          == true);
    
    TEST(alltypes.req_fixed32       == 1008);
    TEST(alltypes.req_sfixed32      == 1009);
    TEST(alltypes.req_float         == 1010.0f);
    
    TEST(alltypes.req_fixed64       == 1011);
    TEST(alltypes.req_sfixed64      == 1012);
    TEST(alltypes.req_double        == 1013.0f);
    
    TEST(strcmp(alltypes.req_string, "1014") == 0);
    TEST(alltypes.req_bytes.size == 4);
    TEST(memcmp(alltypes.req_bytes.bytes, "1015", 4) == 0);
    TEST(strcmp(alltypes.req_submsg.substuff1, "1016") == 0);
    TEST(alltypes.req_submsg.substuff2 == 1016);
    TEST(alltypes.req_enum == MyEnum_Truth);
    
    TEST(alltypes.end == 1099);
    
    return true;
}

int main()
{
    /* Read the data into buffer */
    uint8_t buffer[512];
    size_t count = fread(buffer, 1, sizeof(buffer), stdin);
    
    /* Construct a pb_istream_t for reading from the buffer */
    pb_istream_t stream = pb_istream_from_buffer(buffer, count);
    
    /* Decode and print out the stuff */
    if (!check_alltypes(&stream))
    {
        printf("Parsing failed.\n");
        return 1;
    } else {
        return 0;
    }
}
