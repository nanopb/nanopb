#include <stdio.h>
#include <pb_decode.h>
#include <stdlib.h>
#include "deep.pb.h"
#include "test_helpers.h"
#include "unittests.h"

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
    pb_decode_ctx_t stream;
    pb_byte_t buffer[DeepMessage_size];
    
    /* In mode 0, buffer is used.
     * In other modes, direct stdin stream is used
     */
    if (argc > 1) mode = atoi(argv[1]);

    if (mode == 0)
    {
        SET_BINARY_MODE(stdin);
        size_t count = fread(buffer, 1, sizeof(buffer), stdin);
        pb_init_decode_ctx_for_buffer(&stream, buffer, count);
    }
    else
    {
        init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, NULL, 0);
    }

    if (!check_message(&stream))
    {
        fprintf(stderr, "Test failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    } else {
        return 0;
    }
}
