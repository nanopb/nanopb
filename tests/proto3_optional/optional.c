#include <pb_encode.h>
#include <pb_decode.h>
#include <unittests.h>
#include "optional.pb.h"

int main()
{
    int status = 0;
    uint8_t buf[256];
    size_t msglen;
    
    COMMENT("Test encoding message with optional field")
    {
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buf, sizeof(buf));
        TestMessage msg = TestMessage_init_zero;
        
        msg.has_opt_int = true;
        msg.opt_int = 99;
        msg.normal_int = 100;
        msg.opt_int2 = 101;
        
        TEST(pb_encode(&stream, TestMessage_fields, &msg));
        msglen = stream.bytes_written;
    }
    
    COMMENT("Test decoding message with optional field")
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buf, msglen);
        TestMessage msg = TestMessage_init_zero;
        
        /* These fields should be missing from the message
         * so the values wouldn't be overwritten. */
        msg.opt_int2 = 5;
        msg.normal_int2 = 6;
        
        stream.flags |= PB_DECODE_CTX_FLAG_NOINIT;
        TEST(pb_decode(&stream, TestMessage_fields, &msg));
        TEST(msg.has_opt_int);
        TEST(msg.opt_int == 99);
        TEST(msg.normal_int == 100);
        TEST(!msg.has_opt_int2);
        TEST(msg.opt_int2 == 5);
        TEST(msg.normal_int2 == 6);
    }

    return status;
}

