#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "max_depth.pb.h"
#include "unittests.h"

int main()
{
    uint8_t buffer[128];
    size_t message_length;
    int status = 0;
    
    {
        COMMENT("Test with depth 8");
        MaxDepth1 message = MaxDepth1_init_zero;
        message.has_submsg = true;
        message.submsg.has_submsg = true;
        message.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        TEST(pb_encode(&stream, MaxDepth1_fields, &message));
        message_length = stream.bytes_written;
        TEST(message_length > 10);
    }
    
    {
        MaxDepth1 dec_msg = MaxDepth1_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
        TEST(pb_decode(&stream, MaxDepth1_fields, &dec_msg));
        TEST(dec_msg.submsg.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg);
    }
    
    {
        COMMENT("Test with depth 9");
        MaxDepth1 message = MaxDepth1_init_zero;
        message.has_submsg = true;
        message.submsg.has_submsg = true;
        message.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        message.submsg.submsg.submsg.submsg.submsg.submsg.submsg.submsg.has_submsg = true;
        
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        TEST(pb_encode(&stream, MaxDepth1_fields, &message));
        message_length = stream.bytes_written;
        TEST(message_length > 10);
    }
    
    {
        MaxDepth1 dec_msg = MaxDepth1_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
        TEST(!pb_decode(&stream, MaxDepth1_fields, &dec_msg));
        TEST(strcmp(PB_GET_ERROR(&stream), "max depth") == 0);
    }
    
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}

