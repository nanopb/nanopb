/* Encoding testcase for callback fields */

#include <stdio.h>
#include <string.h>
#include <pb_encode.h>
#include "callbacks.pb.h"

bool encode_string(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    char *str = "Hello world!";
    
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

int main()
{
    uint8_t buffer[1024];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, 1024);
    TestMessage testmessage = {};
    
    testmessage.stringvalue.funcs.encode = &encode_string;
    
    if (!pb_encode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    if (fwrite(buffer, stream.bytes_written, 1, stdout) != 1)
        return 2;
    
    return 0;
}
