#include <pb_encode.h>
#include <unittests.h>
#include <string.h>
#include "padding.pb.h"

int main()
{
    int status = 0;
    
    TestMessage msg;
    
    /* Set padding bytes to garbage */
    memset(&msg, 0xAA, sizeof(msg));
    
    /* Set all meaningful fields to 0 */
    msg.submsg.boolfield = false;
    msg.submsg.intfield = 0;
    
    /* Test encoding */
    {
        pb_byte_t buf[128] = {0};
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buf, sizeof(buf));
        TEST(pb_encode(&stream, TestMessage_fields, &msg));
        
        /* The submessage will be encoded as an empty message. */
        TEST(stream.bytes_written == 2 &&
             buf[0] == ((1 << 3) | PB_WT_STRING) &&
             buf[1] == 0);
    }
    
    return status;
}

