#include <unittests.h>
#include <pb_encode.h>
#include "short_array.pb.h"

int main()
{
    int status = 0;
    
    COMMENT("Test message length calculation for short arrays");
    {
        uint8_t buffer[TestMessage_size] = {0};
        pb_encode_ctx_t ostream;
        pb_init_encode_ctx_for_buffer(&ostream, buffer, TestMessage_size);
        TestMessage msg = TestMessage_init_zero;
        
        msg.rep_uint32_count = 1;
        msg.rep_uint32[0] = ((uint32_t)1 << 31);
        
        TEST(pb_encode(&ostream, TestMessage_fields, &msg));
        TEST(ostream.bytes_written == TestMessage_size);
    }
    
    return status;
}

