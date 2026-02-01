#include "flakystream.h"
#include <string.h>

#if !PB_NO_STREAM_CALLBACK

bool flakystream_callback(pb_decode_ctx_t *stream, pb_byte_t *buf, size_t count)
{
    flakystream_t *state = (flakystream_t*)stream;

    if (state->position + count > state->msglen)
    {
        stream->bytes_left = 0;
        return false;
    }
    else if (state->position + count > state->fail_after)
    {
        PB_RETURN_ERROR(stream, "flaky error");
    }

    memcpy(buf, state->buffer + state->position, count);
    state->position += count;
    return true;
}

void flakystream_init(flakystream_t *stream, const uint8_t *buffer,
    size_t msglen, size_t fail_after,
    uint8_t *tmpbuf, size_t tmpbuf_size)
{
    pb_init_decode_ctx_for_callback(&stream->stream, flakystream_callback, NULL,
        msglen, tmpbuf, tmpbuf_size);

    stream->buffer = buffer;
    stream->position = 0;
    stream->msglen = msglen;
    stream->fail_after = fail_after;
}

#endif
