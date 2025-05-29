#include <assert.h>
#include <pb_decode.h>
#include "unittests.h"
#include "test.pb.h"
#include <stdio.h>

const uint8_t input_data[] = {
    0x08, 0x01,
};

const size_t input_len = sizeof(input_data);

bool stream_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    size_t cursor = (size_t)(uintptr_t)stream->state;

    if ((cursor + count) > input_len)
    {
        stream->bytes_left = 0;
        return false;
    }

    memcpy(buf, &input_data[cursor], count);
    cursor += count;

    stream->state = (void*)(uintptr_t)cursor;

    return true;
}

int main()
{
    int status = 0;

    /* test buffer stream */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(input_data, input_len);
        TEST(pb_decode(&stream, TestMessage_fields, &msg));
        TEST(msg.foo == 0x1);
        TEST(stream.errmsg == NULL);
    }

    /* test callback stream */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_istream_t stream = {&stream_callback, 0, SIZE_MAX};
        TEST(pb_decode(&stream, TestMessage_fields, &msg));
        TEST(msg.foo == 0x1);
        TEST(stream.errmsg == NULL);
    }

    return status;
}
