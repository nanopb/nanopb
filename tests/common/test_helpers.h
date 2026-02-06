/* Compatibility helpers for the test programs. */

#ifndef _TEST_HELPERS_H_
#define _TEST_HELPERS_H_

#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>

// Set stdio stream into binary mode on Windows
// Does nothing on other platforms
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#define SET_BINARY_MODE(file)
#endif

#if !PB_NO_STREAM_CALLBACK

// Bind stdio streams to nanopb streams
static bool stdio_write_cb(pb_encode_ctx_t *ctx, const uint8_t *buf, pb_size_t count)
{
    return fwrite(buf, 1, (size_t)count, (FILE*)ctx->stream_callback_state) == count;
}

static pb_size_t stdio_read_cb(pb_decode_ctx_t *ctx, uint8_t *buf, pb_size_t count)
{
    FILE *file = (FILE*)ctx->stream_callback_state;
    size_t ret = fread(buf, 1, count, file);

    if (ret != count && !feof(file))
    {
        PB_SET_ERROR(ctx, "fread failed");
        return PB_READ_ERROR;
    }

    // EOF is indicated by return value less than count
    return (pb_size_t)ret;
}

static inline void init_encode_ctx_for_stdio(pb_encode_ctx_t *ctx, FILE* file, pb_size_t max_len,
    pb_byte_t *buf, pb_size_t bufsize)
{
    SET_BINARY_MODE(file);
    pb_init_encode_ctx_for_callback(ctx, &stdio_write_cb, file, max_len, buf, bufsize);
}

static inline void init_decode_ctx_for_stdio(pb_decode_ctx_t *ctx, FILE* file, pb_size_t max_len,
    pb_byte_t *buf, pb_size_t bufsize)
{
    SET_BINARY_MODE(file);
    pb_init_decode_ctx_for_callback(ctx, &stdio_read_cb, file, max_len, buf, bufsize);
}

#endif

#if !PB_NO_ERRMSG
#define COMPARE_ERRMSG(stream, text) (strcmp((stream)->errmsg, text) == 0)
#else
#define COMPARE_ERRMSG(stream, text) true
#endif

#endif
