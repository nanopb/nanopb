#include <assert.h>
#include <pb_decode.h>
#include "unittests.h"
#include "test.pb.h"
#include <stdio.h>

const uint8_t input_data[] = {
    0x08, 0x01,
};

const size_t input_len = sizeof(input_data);

#if !PB_NO_STREAM_CALLBACK
pb_size_t stream_callback(pb_decode_ctx_t *stream, uint8_t *buf, pb_size_t count)
{
    pb_size_t cursor = (pb_size_t)(uintptr_t)stream->stream_callback_state;

    if ((cursor + count) > input_len)
    {
        count = input_len - cursor;
    }

    memcpy(buf, &input_data[cursor], count);
    cursor += count;

    stream->stream_callback_state = (void*)(uintptr_t)cursor;

    return count;
}
#endif

int main()
{
    int status = 0;

    /* test buffer stream */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, input_data, input_len);
        TEST(pb_decode(&stream, TestMessage_fields, &msg));
        TEST(msg.foo == 0x1);

#if !PB_NO_ERRMSG
        TEST(stream.errmsg == NULL);
#endif
    }

#if !PB_NO_STREAM_CALLBACK
    /* test callback stream */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_callback(&stream, &stream_callback, 0, PB_SIZE_MAX, NULL, 0);
        TEST(pb_decode(&stream, TestMessage_fields, &msg));
        TEST(msg.foo == 0x1);

#if !PB_NO_ERRMSG
        TEST(stream.errmsg == NULL);
#endif
    }
#endif

    return status;
}
