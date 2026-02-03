/* Testcase and example for decoding using field callbacks
 * defined by function name.
 *
 * This was the primary callback mechanism in nanopb-0.4.x.
 * 
 * It has the benefit of requiring no runtime setup to find
 * the callback function, but the downside that the callback
 * function cannot be changed during runtime.
 * 
 * For passing data to/from the callback, generator option
 * 'callback_datatype' can be used to customize the field.
 * 
 * NOTE: Field callbacks are an advanced mechanism for
 * custom handling of field data. In this example, every
 * field is a callback, but that is purely for demonstration.
 * Normally callback fields are only used for special requirements.
 */

#include <stdio.h>
#include <pb_decode.h>
#include "callbacks_name.pb.h"
#include "test_helpers.h"

// This callback is called for any callback field inside SubMessage
bool SubMessage_callback(pb_decode_ctx_t *istream, pb_encode_ctx_t *ostream, const pb_field_iter_t *field)
{
    if (!istream) return true; // Only decode callbacks implemented here

    if (field->tag == SubMessage_stringvalue_tag)
    {
        uint8_t buffer[1024] = {0};
    
        // We could read block-by-block to avoid the large buffer...
        if (istream->bytes_left > sizeof(buffer) - 1)
            return false;
        
        if (!pb_read(istream, buffer, istream->bytes_left))
            return false;
        
        // Print the string, in format comparable with protoc --decode.
        printf("submsg {\n  stringvalue: \"%s\"\n", buffer);
    }
    else if (field->tag == SubMessage_int32value_tag)
    {
        uint64_t value;
        if (!pb_decode_varint(istream, &value))
            return false;
        
        printf("  int32value: %ld\n", (long)value);
    }
    else if (field->tag == SubMessage_fixed32value_tag)
    {
        uint32_t value;
        if (!pb_decode_fixed32(istream, &value))
            return false;
        
        printf("  fixed32value: %ld\n", (long)value);
    }
    else if (field->tag == SubMessage_fixed64value_tag)
    {
        uint64_t value;
        if (!pb_decode_fixed64(istream, &value))
            return false;
        
        printf("  fixed64value: %ld\n}\n", (long)value);
    }

    return true;
}

// This callback is called for any callback field inside TestMessage
// In this example it has quite a bit of duplication with SubMessage :)
bool TestMessage_callback(pb_decode_ctx_t *istream, pb_encode_ctx_t *ostream, const pb_field_iter_t *field)
{
    if (!istream) return true; // Only decode callbacks implemented here

    if (field->tag == TestMessage_stringvalue_tag)
    {
        uint8_t buffer[1024] = {0};
    
        if (istream->bytes_left > sizeof(buffer) - 1)
            return false;
        
        if (!pb_read(istream, buffer, istream->bytes_left))
            return false;
        
        printf("stringvalue: \"%s\"\n", buffer);
    }
    else if (field->tag == TestMessage_int32value_tag)
    {
        uint64_t value;
        if (!pb_decode_varint(istream, &value))
            return false;
        
        printf("int32value: %ld\n", (long)value);
    }
    else if (field->tag == TestMessage_fixed32value_tag)
    {
        uint32_t value;
        if (!pb_decode_fixed32(istream, &value))
            return false;
        
        printf("fixed32value: %ld\n", (long)value);
    }
    else if (field->tag == TestMessage_fixed64value_tag)
    {
        uint64_t value;
        if (!pb_decode_fixed64(istream, &value))
            return false;
        
        printf("fixed64value: %ld\n", (long)value);
    }
    else if (field->tag == TestMessage_repeatedstring_tag)
    {
        uint8_t buffer[1024] = {0};
    
        // We could read block-by-block to avoid the large buffer...
        if (istream->bytes_left > sizeof(buffer) - 1)
            return false;
        
        if (!pb_read(istream, buffer, istream->bytes_left))
            return false;
        
        printf("repeatedstring: \"%s\"\n", buffer);
    }

    return true;
}

int main()
{
    uint8_t buffer[1024];
    size_t length;
    pb_decode_ctx_t stream;
    
    TestMessage testmessage = {{{NULL}}};
    
    SET_BINARY_MODE(stdin);
    length = fread(buffer, 1, 1024, stdin);
    pb_init_decode_ctx_for_buffer(&stream, buffer, length);

    if (!pb_decode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    return 0;
}
