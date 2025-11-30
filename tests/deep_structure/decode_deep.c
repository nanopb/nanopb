#include <stdio.h>
#include <pb_decode.h>
#include <stdlib.h>
#include "deep.pb.h"
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

bool check_message(pb_istream_t *stream)
{
    DeepMessage msg = {0};

    if (!pb_decode(stream, DeepMessage_fields, &msg))
        return false;
    
    int status = 0;
    TEST(msg.first == 888);
    TEST(strcmp(msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.strval, "abcd") == 0);
    TEST(msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.intval == 987654321);
    TEST(msg.last == 999);
    
    return status == 0;
}

int main(int argc, const char **argv)
{
    int mode = 0;
    pb_istream_t stream = {&callback, NULL, SIZE_MAX};
    pb_byte_t buffer[DeepMessage_size];
    
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