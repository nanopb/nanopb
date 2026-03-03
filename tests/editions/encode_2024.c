// Encode test message using protobuf 2024 edition

#include <stdio.h>
#include <pb_encode.h>
#include "edition_2024.pb.h"
#include "test_helpers.h"

int main()
{
    EditionMessage msg = EditionMessage_init_default;

    msg.has_mystring = true; // Note: using the default value from .proto

    msg.has_submsg = true;
    msg.submsg.has_foo = true;
    msg.submsg.foo = 1001;
    msg.required_field = 1003;
    msg.has_optional_field = true;
    msg.optional_field = 1004;
    msg.implicit_field = 1005;
    
    // Prepare the stream, output goes directly to stdout
    pb_encode_ctx_t stream;
    init_encode_ctx_for_stdio(&stream, stdout, PB_SIZE_MAX, NULL, 0);
    
    // Now encode it and check if we succeeded.
    if (pb_encode(&stream, &EditionMessage_msg, &msg))
    {
        return 0;
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }
}
