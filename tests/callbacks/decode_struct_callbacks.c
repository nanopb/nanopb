/* Testcase and example for decoding using field callbacks
 * defined in pb_callback_t structure. 
 *
 * This was the primary callback mechanism before nanopb-0.4.0.
 * 
 * It has the downside of mixing callback pointers with other message
 * data. This works for normal static fields, but for e.g. oneof or
 * pointer submessages it is difficult to setup the callbacks during
 * decoding.
 * 
 * Run e.g. ./test_encode_struct_callbacks | ./test_decode_struct_callbacks
 * 
 * NOTE: Field callbacks are an advanced mechanism for
 * custom handling of field data. In this example, every
 * field is a callback, but that is purely for demonstration.
 * Normally callback fields are only used for special requirements.
 */

#include <stdio.h>
#include <pb_decode.h>
#include "callbacks.pb.h"
#include "test_helpers.h"

bool print_string(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    uint8_t buffer[1024] = {0};
    
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > sizeof(buffer) - 1)
        return false;
    
    if (!pb_read(stream, buffer, stream->bytes_left))
        return false;
    
    /* Print the string, in format comparable with protoc --decode.
     * Format comes from the arg defined in main().
     */
    printf((char*)*arg, buffer);
    return true;
}

bool print_int32(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;
    
    printf((char*)*arg, (long)value);
    return true;
}

bool print_fixed32(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    uint32_t value;
    if (!pb_decode_fixed32(stream, &value))
        return false;
    
    printf((char*)*arg, (long)value);
    return true;
}

bool print_fixed64(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    uint64_t value;
    if (!pb_decode_fixed64(stream, &value))
        return false;
    
    printf((char*)*arg, (long)value);
    return true;
}

int main()
{
    uint8_t buffer[1024];
    size_t length;
    pb_decode_ctx_t stream;
    /* Note: empty initializer list initializes the struct with all-0.
     * This is recommended so that unused callbacks are set to NULL instead
     * of crashing at runtime.
     */
    TestMessage testmessage = {{{NULL}}};
    
    SET_BINARY_MODE(stdin);
    length = fread(buffer, 1, 1024, stdin);
    pb_init_decode_ctx_for_buffer(&stream, buffer, length);
    
    testmessage.submsg.stringvalue.funcs.decode = &print_string;
    testmessage.submsg.stringvalue.arg = "submsg {\n  stringvalue: \"%s\"\n";
    testmessage.submsg.int32value.funcs.decode = &print_int32;
    testmessage.submsg.int32value.arg = "  int32value: %ld\n";
    testmessage.submsg.fixed32value.funcs.decode = &print_fixed32;
    testmessage.submsg.fixed32value.arg = "  fixed32value: %ld\n";
    testmessage.submsg.fixed64value.funcs.decode = &print_fixed64;
    testmessage.submsg.fixed64value.arg = "  fixed64value: %ld\n}\n";
    
    testmessage.stringvalue.funcs.decode = &print_string;
    testmessage.stringvalue.arg = "stringvalue: \"%s\"\n";
    testmessage.int32value.funcs.decode = &print_int32;
    testmessage.int32value.arg = "int32value: %ld\n";
    testmessage.fixed32value.funcs.decode = &print_fixed32;
    testmessage.fixed32value.arg = "fixed32value: %ld\n";
    testmessage.fixed64value.funcs.decode = &print_fixed64;
    testmessage.fixed64value.arg = "fixed64value: %ld\n";
    testmessage.repeatedstring.funcs.decode = &print_string;
    testmessage.repeatedstring.arg = "repeatedstring: \"%s\"\n";
    
    if (!pb_decode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    return 0;
}
