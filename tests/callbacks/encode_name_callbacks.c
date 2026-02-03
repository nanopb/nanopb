/* Testcase and example for encoding using field callbacks
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
#include <string.h>
#include <pb_encode.h>
#include "callbacks_name.pb.h"
#include "test_helpers.h"

// This callback is called for any callback field inside SubMessage
bool SubMessage_callback(pb_decode_ctx_t *istream, pb_encode_ctx_t *ostream, const pb_field_iter_t *field)
{
    if (!ostream) return true; // Only encode callbacks implemented here

    if (field->tag == SubMessage_stringvalue_tag)
    {
        char *str = "Hello world!";
    
        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_string(ostream, (uint8_t*)str, strlen(str));
    }
    else if (field->tag == SubMessage_int32value_tag)
    {
        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_varint(ostream, 42);
    }
    else if (field->tag == SubMessage_fixed32value_tag)
    {
        uint32_t value = 42;

        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_fixed32(ostream, &value);
    }
    else if (field->tag == SubMessage_fixed64value_tag)
    {
        uint64_t value = 42;

        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_fixed64(ostream, &value);
    }

    return true;
}

// This callback is called for any callback field inside TestMessage
bool TestMessage_callback(pb_decode_ctx_t *istream, pb_encode_ctx_t *ostream, const pb_field_iter_t *field)
{
    if (!ostream) return true; // Only encode callbacks implemented here

    if (field->tag == TestMessage_stringvalue_tag)
    {
        char *str = "Hello world!";
    
        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_string(ostream, (uint8_t*)str, strlen(str));
    }
    else if (field->tag == TestMessage_int32value_tag)
    {
        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_varint(ostream, 42);
    }
    else if (field->tag == TestMessage_fixed32value_tag)
    {
        uint32_t value = 42;

        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_fixed32(ostream, &value);
    }
    else if (field->tag == TestMessage_fixed64value_tag)
    {
        uint64_t value = 42;

        if (!pb_encode_tag_for_field(ostream, field))
            return false;
        
        return pb_encode_fixed64(ostream, &value);
    }
    else if (field->tag == TestMessage_repeatedstring_tag)
    {
        char *str[4] = {"Hello world!", "", "Test", "Test2"};
        int i;
        
        for (i = 0; i < 4; i++)
        {
            if (!pb_encode_tag_for_field(ostream, field))
                return false;
            
            if (!pb_encode_string(ostream, (uint8_t*)str[i], strlen(str[i])))
                return false;
        }
        return true;
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
    
    if (!pb_encode(&stream, TestMessage_fields, &testmessage))
        return 1;
    
    SET_BINARY_MODE(stdout);
    if (fwrite(buffer, stream.bytes_written, 1, stdout) != 1)
        return 2;
    
    return 0;
}
