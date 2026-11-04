#include <pb_decode.h>
#include <string.h>
#include "test.pb.h"
#include "unittests.h"

typedef struct {
    const pb_byte_t *data;
    size_t size;
    size_t position;
} stream_state_t;

static bool stream_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    stream_state_t *state = stream->state;

    if (count > state->size - state->position)
    {
        stream->bytes_left = 0;
        return false;
    }

    if (buf != NULL)
        memcpy(buf, state->data + state->position, count);

    state->position += count;
    return true;
}

static pb_istream_t callback_stream(stream_state_t *state, const pb_byte_t *data, size_t size)
{
    pb_istream_t stream = {&stream_callback, state, SIZE_MAX};

    state->data = data;
    state->size = size;
    state->position = 0;
    return stream;
}

int main(void)
{
    int status = 0;
    static const pb_byte_t truncated_submessage[] = {0x0a, 0x04, 0x08, 0x01};
    static const pb_byte_t truncated_delimited[] = {0x04, 0x08, 0x01};
    static const pb_byte_t truncated_tag[] = {0x08, 0x01, 0x90};
    static const pb_byte_t complete_message[] = {0x08, 0x01};
    stream_state_t state;
    pb_istream_t stream;
    TestMessage msg;
    SubMessage submsg;

    COMMENT("Buffer and callback streams reject a truncated submessage");
    msg = (TestMessage)TestMessage_init_zero;
    stream = pb_istream_from_buffer(truncated_submessage, sizeof(truncated_submessage));
    TEST(!pb_decode(&stream, TestMessage_fields, &msg));

    msg = (TestMessage)TestMessage_init_zero;
    stream = callback_stream(&state, truncated_submessage, sizeof(truncated_submessage));
    TEST(!pb_decode(&stream, TestMessage_fields, &msg));

    COMMENT("Callback streams reject a truncated delimited message");
    submsg = (SubMessage)SubMessage_init_zero;
    stream = callback_stream(&state, truncated_delimited, sizeof(truncated_delimited));
    TEST(!pb_decode_ex(&stream, SubMessage_fields, &submsg, PB_DECODE_DELIMITED));

    COMMENT("Callback streams distinguish a truncated tag from clean EOF");
    submsg = (SubMessage)SubMessage_init_zero;
    stream = callback_stream(&state, truncated_tag, sizeof(truncated_tag));
    TEST(!pb_decode(&stream, SubMessage_fields, &submsg));

    submsg = (SubMessage)SubMessage_init_zero;
    stream = callback_stream(&state, complete_message, sizeof(complete_message));
    TEST(pb_decode(&stream, SubMessage_fields, &submsg));
    TEST(submsg.a == 1);

    return status;
}
