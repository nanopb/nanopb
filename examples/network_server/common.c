/* Simple binding of nanopb streams to TCP sockets.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "common.h"

static bool write_callback(pb_encode_ctx_t *stream, const uint8_t *buf, size_t count)
{
    int fd = (intptr_t)stream->state;
    return send(fd, buf, count, 0) == count;
}

static bool read_callback(pb_decode_ctx_t *stream, uint8_t *buf, size_t count)
{
    int fd = (intptr_t)stream->state;
    int result;
    
    if (count == 0)
        return true;

    result = recv(fd, buf, count, MSG_WAITALL);
    
    if (result == 0)
        stream->bytes_left = 0; /* EOF */
    
    return result == count;
}

pb_encode_ctx_t pb_ostream_from_socket(int fd)
{
    pb_encode_ctx_t stream;
    pb_init_encode_ctx_for_callback(&stream, &write_callback, (void*)(intptr_t)fd, PB_SIZE_MAX, NULL, 0);
    return stream;
}

pb_decode_ctx_t pb_istream_from_socket(int fd)
{
    pb_decode_ctx_t stream;
    pb_init_decode_ctx_for_callback(&stream, &read_callback, (void*)(intptr_t)fd, PB_SIZE_MAX, NULL, 0);
    return stream;
}
