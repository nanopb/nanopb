#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "test_helpers.h"
#include "deep.pb.h"

/* This binds the pb_ostream_t into the stdout stream */
bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
}

int main(int argc, const char **argv)
{
    int mode = 0;
    pb_byte_t buffer[DeepMessage_size];
    pb_ostream_t stream = {&streamcallback, NULL, DeepMessage_size, 0};

    SET_BINARY_MODE(stdout);
    stream.state = stdout;

    /* In mode 0, buffer is used.
     * In other modes, direct stdout stream is used
     */
    if (argc > 1) mode = atoi(argv[1]);

    if (mode == 0)
    {
        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
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