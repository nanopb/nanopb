// Decode test message using protobuf 2024 edition
// Prints data in a format identical to protoc --decode

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
    pb_decode_ctx_t stream;
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, NULL, 0);

    /* Decode and print out the stuff */
    EditionMessage msg = EditionMessage_init_zero;
    if (!pb_decode(&stream, &EditionMessage_msg, &msg))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    print_editionmsg(&msg);
    return 0;
}
