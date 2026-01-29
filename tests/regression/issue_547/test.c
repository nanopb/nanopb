#include <string.h>
#include <pb_encode.h>
#include <unittests.h>
#include "test.pb.h"

int main()
{
    pb_byte_t buf[512];
    MyMessage msg = MyMessage_init_zero;
    pb_encode_ctx_t stream;
    pb_init_encode_ctx_for_buffer(&stream, buf, sizeof(buf));

    msg.mybytes.size = -1;
    
    if (pb_encode(&stream, MyMessage_fields, &msg))
    {
        fprintf(stderr, "Failure: expected pb_encode() to fail.\n");
        return 1;
    }
    else if (strcmp(PB_GET_ERROR(&stream), "bytes size exceeded") != 0)
    {
        fprintf(stderr, "Unexpected encoding error: %s\n", PB_GET_ERROR(&stream));
        return 2;
    }
    else
    {
        return 0;
    }
}
