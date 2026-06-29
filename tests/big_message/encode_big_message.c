#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "test_helpers.h"
#include "big_message.pb.h"

/* This binds the pb_ostream_t into the stdout stream */
bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
}

void write_incrementing(pb_bytes_array_t* buf, pb_size_t count)
{
    pb_size_t i;
    for (i = 0; i < count; i++)
        buf->bytes[i] = (pb_byte_t)i;
    buf->size = count;
}

int main(int argc, const char **argv)
{
    int mode = 0;
    pb_byte_t buffer[One_size];
    pb_ostream_t stream;
#ifndef PB_BUFFER_ONLY
    stream = (pb_ostream_t){&streamcallback, NULL, One_size, 0}
#endif

    SET_BINARY_MODE(stdout);
    stream.state = stdout;

    /*
     * In mode 0, buffer is used.
     * In other modes, direct stdout stream is used
     */
    if (argc > 1) mode = atoi(argv[1]);

    if (mode == 0)
    {
        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    }

    One one = One_init_default;
    one.start = 42;
    write_incrementing((pb_bytes_array_t*)&one.two.large, 20000);
    write_incrementing((pb_bytes_array_t*)&one.two.three.large, 20000);
    write_incrementing((pb_bytes_array_t*)&one.two.three.small, 20);
    one.end = 43;

    if (pb_encode(&stream, One_fields, &one))
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
