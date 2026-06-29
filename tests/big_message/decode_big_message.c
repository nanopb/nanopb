#include <stdio.h>
#include <pb_decode.h>
#include <stdlib.h>
#include "big_message.pb.h"
#include "test_helpers.h"
#include "unittests.h"

/* This binds the pb_istream_t to stdin */
bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    FILE *file = (FILE*)stream->state;
    size_t len = fread(buf, 1, count, file);
    
    if (len == count)
    {
        return true;
    }
    else
    {
        stream->bytes_left = 0;
        return false;
    }
}

bool check_incrementing(pb_bytes_array_t *buf, pb_size_t count)
{
    pb_size_t i;

    if (buf->size != count) return false;

    for (i = 0; i < count; i++)
    {
        if (buf->bytes[i] != (pb_byte_t)i) return false;
    }

    return true;
}

bool check_message(pb_istream_t *stream)
{
    One one = {0};

    if (!pb_decode(stream, One_fields, &one))
        return false;

    int status = 0;

    TEST(one.start = 42);
    TEST(check_incrementing((pb_bytes_array_t*)&one.two.large, 20000));
    TEST(check_incrementing((pb_bytes_array_t*)&one.two.three.large, 20000));
    TEST(check_incrementing((pb_bytes_array_t*)&one.two.three.small, 20));
    TEST(one.end == 43);

    return status == 0;
}

int main(int argc, const char **argv)
{
    int mode = 0;
    pb_istream_t stream;
    pb_byte_t buffer[One_size];
#ifndef PB_BUFFER_ONLY
    stream = (pb_istream_t){&callback, NULL, SIZE_MAX}
#endif

    SET_BINARY_MODE(stdin);
    stream.state = stdin;

    /* In mode 0, buffer is used.
     * In other modes, direct stdin stream is used
     */
    if (argc > 1) mode = atoi(argv[1]);

    if (mode == 0)
    {
        size_t count = fread(buffer, 1, sizeof(buffer), stdin);
        stream = pb_istream_from_buffer(buffer, count);
    }

    if (!check_message(&stream))
    {
        fprintf(stderr, "Test failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    } else {
        return 0;
    }
}
