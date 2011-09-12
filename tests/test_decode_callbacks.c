/* Decoding testcase for callback fields.
 * Run e.g. ./test_encode_callbacks | ./test_decode_callbacks
 */

#include <stdio.h>
#include <pb_decode.h>
#include "callbacks.pb.h"

bool print_string(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    uint8_t buffer[1024];
    
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > sizeof(buffer))
        return false;
    
    if (!pb_read(stream, buffer, stream->bytes_left))
        return false;
    
    /* Print the string, in format comparable with protoc --decode. */
    printf("%s: \"%s\"\n", (char*)arg, buffer);
    return true;
}

int main()
{
    uint8_t buffer[1024];
    size_t length = fread(buffer, 1, 1024, stdin);
    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    
    /* Note: empty initializer list initializes the struct with all-0.
     * This is recommended so that unused callbacks are set to NULL instead
     * of crashing at runtime.
     */
    TestMessage testmessage = {};
    
    testmessage.stringvalue.funcs.decode = &print_string;
    testmessage.stringvalue.arg = "stringvalue";
    
    if (!pb_decode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    return 0;
}