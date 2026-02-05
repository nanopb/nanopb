// Example of encoding a protobuf message using SLIP framing.
// The message end is indicated by 0xC0, which is escaped in data.
// See: https://en.wikipedia.org/wiki/Serial_Line_Internet_Protocol

#include <stdio.h>
#include <pb_encode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "framing_testdata.h"

typedef struct
{
    // Original (lower level) stream callback and its state
    pb_encode_ctx_write_callback_t stream_callback;
    void *stream_callback_state;
} slip_ostream_t;

bool write_slip_callback(pb_encode_ctx_t *ctx, const pb_byte_t *buf, pb_size_t count)
{
    // Take our state variable and restore the state of the underlying callback
    slip_ostream_t *slip = (slip_ostream_t*)ctx->stream_callback_state;
    ctx->stream_callback_state = slip->stream_callback_state;

    // Check each output byte for the special values before writing it out.
    // Spans of non-escaped bytes are combined to blocks for efficiency.
    bool status = true;
    pb_size_t bytes_written = 0;
    while (bytes_written < count && status)
    {
        // Check if the first byte needs to be escaped
        pb_byte_t byte = buf[bytes_written];
        if (byte == 0xC0 || byte == 0xDB)
        {
            // Escape special character
            pb_byte_t escaped[2] = {0xDB, (byte == 0xC0) ? 0xDC : 0xDD};
            status = slip->stream_callback(ctx, escaped, 2);
            bytes_written++;
            continue;
        }
        
        // Search for longest span of non-escaped characters
        const pb_byte_t *block_start = buf + bytes_written;
        pb_size_t block_size;
        for (block_size = 0; block_size < count - bytes_written; block_size++)
        {
            byte = *(block_start + block_size);
            if (byte == 0xC0 || byte == 0xDB) break;
        }
        
        // Write out the non-escaped characters
        if (block_size > 0)
        {
            status = slip->stream_callback(ctx, block_start, block_size);
            bytes_written += block_size;
        }
        else
        {
            break;
        }
    }

    // Restore state and return success status
    ctx->stream_callback_state = slip;
    return status;
}

void init_slip_ostream(pb_encode_ctx_t *ctx, slip_ostream_t *slip)
{
    memset(slip, 0, sizeof(slip_ostream_t));
    slip->stream_callback = ctx->stream_callback;
    slip->stream_callback_state = ctx->stream_callback_state;
    ctx->stream_callback = write_slip_callback;
    ctx->stream_callback_state = slip;
}

bool write_slip_frame_end(pb_encode_ctx_t *ctx)
{
    // Take our state variable and restore the state of the underlying callback
    slip_ostream_t *slip = (slip_ostream_t*)ctx->stream_callback_state;
    ctx->stream_callback_state = slip->stream_callback_state;

    pb_byte_t frame_end = 0xC0;
    bool status = slip->stream_callback(ctx, &frame_end, 1);

    ctx->stream_callback_state = slip;
    return status;
}

int main()
{
    // Prepare the stream, output goes directly to stdout
    pb_encode_ctx_t stream;
    init_encode_ctx_for_stdio(&stream, stdout, PB_SIZE_MAX, NULL, 0);
    
    // Perform SLIP format escaping
    slip_ostream_t slip;
    init_slip_ostream(&stream, &slip);

    // Write two messages into same stream
    if (!pb_encode(&stream, Person_fields, &framing_testdata_msg1) ||
        !write_slip_frame_end(&stream) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg2) ||
        !write_slip_frame_end(&stream) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg3) ||
        !write_slip_frame_end(&stream))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    return 0; /* Success */
}
