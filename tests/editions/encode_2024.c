/* Encode test message using protobuf 2024 edition */

#include <stdio.h>
#include <pb_encode.h>
#include "edition_2024.pb.h"
#include "test_helpers.h"

int main()
{
    EditionMessage msg = EditionMessage_init_default;
    uint8_t buffer[EditionMessage_size];
    pb_ostream_t stream;

    msg.has_mystring = true; /* Note: using the default value from .proto */

    msg.has_submsg = true;
    msg.submsg.has_foo = true;
    msg.submsg.foo = 1001;
    msg.required_field = 1003;
    msg.has_optional_field = true;
    msg.optional_field = 1004;
    msg.implicit_field = 1005;
    
    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    /* Now encode it and check if we succeeded. */
    if (pb_encode(&stream, &EditionMessage_msg, &msg))
    {
        SET_BINARY_MODE(stdout);
        fwrite(buffer, 1, stream.bytes_written, stdout);
        return 0;
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }
}
