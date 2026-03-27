#include <stdio.h>
#include <string.h>
#include <pb_encode.h>
#include "submsg_as_bytes.pb.h"
#include "test_helpers.h"

int main(void)
{
    uint8_t buffer[256];
    pb_ostream_t stream;

    SET_BINARY_MODE(stdout);

    /* Encode using normal submessage */
    {
        OuterNormal msg = OuterNormal_init_zero;
        msg.sub.value = 42;
        strcpy(msg.sub.name, "hello");

        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        if (!pb_encode(&stream, OuterNormal_fields, &msg))
        {
            fprintf(stderr, "Encode failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
    }

    fwrite(buffer, 1, stream.bytes_written, stdout);
    return 0;
}
