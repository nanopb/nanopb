#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "test_helpers.h"
#include "deep.pb.h"

int main(int argc, const char **argv)
{
    pb_byte_t buffer[DeepMessage_size];
    pb_encode_ctx_t stream;

    /* In mode 0, buffer is used.
     * In other modes, direct stdout stream is used
     */
    int mode = 0;
    if (argc > 1) mode = atoi(argv[1]);

    if (mode == 0)
    {
        pb_init_encode_ctx_for_buffer(&stream, buffer, sizeof(buffer));
    }
    else
    {
        init_encode_ctx_for_stdio(&stream, stdout, DeepMessage_size, NULL, 0);
    }

    DeepMessage msg = {0};
    strcpy(msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.strval, "abcd");
    msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.intval = 987654321;
    msg.first = 888;
    msg.last = 999;

    if (pb_encode(&stream, DeepMessage_fields, &msg))
    {
        if (mode == 0)
        {
            SET_BINARY_MODE(stdout);
            fwrite(buffer, stream.bytes_written, 1, stdout);
        }

        return 0;
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }
}
