#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "oneofmsg.pb.h"
#include "unittests.h"

int main(int argc, char **argv)
{
    int status = 0;
    uint8_t buffer[512];
    pb_size_t msglen = 0;
    
    {
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buffer, sizeof(buffer));
        BodyMessage msg = BodyMessage_init_zero;
        
        msg.which_body_type = BodyMessage_device_data_crypted_tag;
        msg.body_type.device_data_crypted.size = 252;
        memset(msg.body_type.device_data_crypted.bytes, 0xAA, 252);
        
        TEST(pb_encode(&stream, BodyMessage_fields, &msg));

        msglen = stream.bytes_written;
        TEST(msglen > 252);
    }
    
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);
        BodyMessage msg = BodyMessage_init_zero;
        
        TEST(pb_decode(&stream, BodyMessage_fields, &msg));
        
        TEST(msg.which_body_type == BodyMessage_device_data_crypted_tag);
        TEST(msg.body_type.device_data_crypted.size == 252);
        TEST(msg.body_type.device_data_crypted.bytes[251] == 0xAA);
    }
    
    return status;
}

