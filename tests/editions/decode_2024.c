/* Decode test message using protobuf 2024 edition
 * Prints data in a format identical to protoc --decode
 */

#include <stdio.h>
#include <pb_decode.h>
#include "edition_2024.pb.h"
#include "test_helpers.h"

static void print_editionmsg(const EditionMessage *msg)
{
    if (msg->has_mystring)
    {
        printf("mystring: \"%s\"\n", msg->mystring);
    }
    
    if (msg->has_submsg)
    {
        printf("submsg {\n");
        
        if (msg->submsg.has_foo)
            printf("  foo: %d\n", (int)msg->submsg.foo);
        
        printf("}\n");
    }

    printf("required_field: %d\n", (int)msg->required_field);
    
    if (msg->has_optional_field)
    {
        printf("optional_field: %d\n", (int)msg->optional_field);
    }

    printf("implicit_field: %d\n", (int)msg->implicit_field);
}

int main()
{
    uint8_t buffer[EditionMessage_size];
    pb_istream_t stream;
    size_t count;
    EditionMessage msg = EditionMessage_init_zero;

    /* Read the data into buffer */
    SET_BINARY_MODE(stdin);
    count = fread(buffer, 1, sizeof(buffer), stdin);

    if (!feof(stdin))
    {
        printf("Message does not fit in buffer\n");
        return 1;
    }

    /* Construct a pb_istream_t for reading from the buffer */
    stream = pb_istream_from_buffer(buffer, count);

    /* Decode and print out the stuff */
    if (!pb_decode(&stream, &EditionMessage_msg, &msg))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    print_editionmsg(&msg);
    return 0;
}
