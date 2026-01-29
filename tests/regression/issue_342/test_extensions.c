#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "extensions.pb.h"
#include "unittests.h"

static bool write_string(pb_encode_ctx_t *stream, const pb_field_t *field, void * const *arg)
{
    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, (const void*)"abc", 3);
}

int main(int argc, char **argv)
{
    int status = 0;
    uint8_t buffer[64];
    pb_size_t msglen = 0;
    
    {
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buffer, sizeof(buffer));
        pb_callback_t callback_def = {{0}};
        pb_extension_t ext = {0};
        BaseMessage msg = {0};

        callback_def.funcs.encode = &write_string;
        ext.type = &string_extension;
        ext.dest = &callback_def;
        msg.extensions = &ext;
        
        TEST(pb_encode(&stream, BaseMessage_fields, &msg));
        
        msglen = stream.bytes_written;
        TEST(msglen > 3);
    }
    
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);
        pb_extension_t ext = {0};
        BaseMessage msg = {0};
        
        ext.type = &string_extension;
        /* Note: ext.dest remains null to trigger bug #342 */
        msg.extensions = &ext;
    
        TEST(pb_decode(&stream, BaseMessage_fields, &msg));
    }
    
    return status;
}

