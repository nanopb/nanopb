/* Simple binding of nanopb streams to TCP sockets.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "common.h"

static bool write_callback(pb_encode_ctx_t *stream, const uint8_t *buf, pb_size_t count)
{
    int fd = (intptr_t)stream->stream_callback_state;
    return send(fd, buf, count, 0) == count;
}

static pb_size_t read_callback(pb_decode_ctx_t *stream, uint8_t *buf, pb_size_t count)
{
    int fd = (intptr_t)stream->stream_callback_state;
    int result;
    
    if (count == 0)
        return true;

    result = recv(fd, buf, count, MSG_WAITALL);
    
    if (result < 0)
        return PB_READ_ERROR;
    
    return result;
}

void init_pb_encode_ctx_for_socket(pb_encode_ctx_t *ctx, int fd, pb_byte_t *tmpbuf, pb_size_t bufsize)
{
    pb_init_encode_ctx_for_callback(ctx, &write_callback, (void*)(intptr_t)fd, PB_SIZE_MAX, tmpbuf, bufsize);

}

void init_pb_decode_ctx_for_socket(pb_decode_ctx_t *ctx, int fd, pb_size_t max_msglen,
                                   pb_byte_t *tmpbuf, pb_size_t bufsize)
{
    pb_init_decode_ctx_for_callback(ctx, &read_callback, (void*)(intptr_t)fd, max_msglen, tmpbuf, bufsize);
}
