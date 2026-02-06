#ifndef _PB_EXAMPLE_COMMON_H_
#define _PB_EXAMPLE_COMMON_H_

#include <pb.h>

void init_pb_encode_ctx_for_socket(pb_encode_ctx_t *ctx, int fd, pb_byte_t *tmpbuf, pb_size_t bufsize);
void init_pb_decode_ctx_for_socket(pb_decode_ctx_t *ctx, int fd, pb_size_t max_msglen,
                                   pb_byte_t *tmpbuf, pb_size_t bufsize);

#endif
