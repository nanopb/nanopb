/* Testcase and example for decoding using field callbacks
 * defined by context variable.
 *
 * This callback mechanism was introduced in nanopb-1.0.
 * 
 * It has the benefit of permitting customizable data type
 * in the message structure, graceful handling of oneofs
 * and possibility of selecting the callback at runtime.
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
#include "callbacks.pb.h"
#include "test_helpers.h"

// This callback is called for any callback fields decoded using
// the pb_decode_ctx_t where we set the field_callback pointer.
bool my_decode_callback(pb_decode_ctx_t *stream, const pb_field_iter_t *field)
{
    if (field->descriptor == &SubMessage_msg)
    {
        // Handle callback fields defined in SubMessage

        if (field->tag == SubMessage_stringvalue_tag)
        {
            uint8_t buffer[1024] = {0};
        
            // We could read block-by-block to avoid the large buffer...
            if (stream->bytes_left > sizeof(buffer) - 1)
                return false;
            
            if (!pb_read(stream, buffer, stream->bytes_left))
                return false;
            
            // Print the string, in format comparable with protoc --decode.
            printf("submsg {\n  stringvalue: \"%s\"\n", buffer);
        }
        else if (field->tag == SubMessage_int32value_tag)
        {
            uint64_t value;
            if (!pb_decode_varint(stream, &value))
                return false;
            
            printf("  int32value: %ld\n", (long)value);
        }
        else if (field->tag == SubMessage_fixed32value_tag)
        {
            uint32_t value;
            if (!pb_decode_fixed32(stream, &value))
                return false;
            
            printf("  fixed32value: %ld\n", (long)value);
        }
        else if (field->tag == SubMessage_fixed64value_tag)
        {
            uint64_t value;
            if (!pb_decode_fixed64(stream, &value))
                return false;
            
            printf("  fixed64value: %ld\n}\n", (long)value);
        }
    }
    else if (field->descriptor == &TestMessage_msg)
    {
        // Handle callback fields defined in TestMessage
        if (field->tag == TestMessage_stringvalue_tag)
        {
            uint8_t buffer[1024] = {0};
        
            if (stream->bytes_left > sizeof(buffer) - 1)
                return false;
            
            if (!pb_read(stream, buffer, stream->bytes_left))
                return false;
            
            printf("stringvalue: \"%s\"\n", buffer);
        }
        else if (field->tag == TestMessage_int32value_tag)
        {
            uint64_t value;
            if (!pb_decode_varint(stream, &value))
                return false;
            
            printf("int32value: %ld\n", (long)value);
        }
        else if (field->tag == TestMessage_fixed32value_tag)
        {
            uint32_t value;
            if (!pb_decode_fixed32(stream, &value))
                return false;
            
            printf("fixed32value: %ld\n", (long)value);
        }
        else if (field->tag == TestMessage_fixed64value_tag)
        {
            uint64_t value;
            if (!pb_decode_fixed64(stream, &value))
                return false;
            
            printf("fixed64value: %ld\n", (long)value);
        }
        else if (field->tag == TestMessage_repeatedstring_tag)
        {
            uint8_t buffer[1024] = {0};
        
            // We could read block-by-block to avoid the large buffer...
            if (stream->bytes_left > sizeof(buffer) - 1)
                return false;
            
            if (!pb_read(stream, buffer, stream->bytes_left))
                return false;
            
            printf("repeatedstring: \"%s\"\n", buffer);
        }
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
    stream.field_callback = &my_decode_callback;

    if (!pb_decode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    return 0;
}
