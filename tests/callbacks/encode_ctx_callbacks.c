/* Testcase and example for encoding using field callbacks
 * defined by function name.
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
#include <string.h>
#include <pb_encode.h>
#include "callbacks.pb.h"
#include "test_helpers.h"

// This callback is called for any callback fields decoded using
// the pb_decode_ctx_t where we set the field_callback pointer.
bool my_encode_callback(pb_encode_ctx_t *stream, const pb_field_iter_t *field)
{
    if (field->descriptor == &SubMessage_msg)
    {
        // Handle callback fields defined in SubMessage
        if (field->tag == SubMessage_stringvalue_tag)
        {
            char *str = "Hello world!";
        
            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_string(stream, (uint8_t*)str, strlen(str));
        }
        else if (field->tag == SubMessage_int32value_tag)
        {
            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_varint(stream, 42);
        }
        else if (field->tag == SubMessage_fixed32value_tag)
        {
            uint32_t value = 42;

            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_fixed32(stream, &value);
        }
        else if (field->tag == SubMessage_fixed64value_tag)
        {
            uint64_t value = 42;

            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_fixed64(stream, &value);
        }
    }
    else if (field->descriptor == &TestMessage_msg)
    {
        if (field->tag == TestMessage_stringvalue_tag)
        {
            char *str = "Hello world!";
        
            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_string(stream, (uint8_t*)str, strlen(str));
        }
        else if (field->tag == TestMessage_int32value_tag)
        {
            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_varint(stream, 42);
        }
        else if (field->tag == TestMessage_fixed32value_tag)
        {
            uint32_t value = 42;

            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_fixed32(stream, &value);
        }
        else if (field->tag == TestMessage_fixed64value_tag)
        {
            uint64_t value = 42;

            if (!pb_encode_tag_for_field(stream, field))
                return false;
            
            return pb_encode_fixed64(stream, &value);
        }
        else if (field->tag == TestMessage_repeatedstring_tag)
        {
            char *str[4] = {"Hello world!", "", "Test", "Test2"};
            int i;
            
            for (i = 0; i < 4; i++)
            {
                if (!pb_encode_tag_for_field(stream, field))
                    return false;
                
                if (!pb_encode_string(stream, (uint8_t*)str[i], strlen(str[i])))
                    return false;
            }
            return true;
        }
    }

    return true;
}

int main()
{
    uint8_t buffer[1024];
    pb_encode_ctx_t stream;
    TestMessage testmessage = {{{NULL}}};
    
    testmessage.has_submsg = true;

    pb_init_encode_ctx_for_buffer(&stream, buffer, 1024);
    stream.field_callback = &my_encode_callback;
    
    if (!pb_encode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    SET_BINARY_MODE(stdout);
    if (fwrite(buffer, stream.bytes_written, 1, stdout) != 1)
        return 2;
    
    return 0;
}
