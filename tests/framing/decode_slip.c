// Example of decoding a protobuf message using SLIP framing.
// The message end is indicated by 0xC0, which is escaped in data.
// See: https://en.wikipedia.org/wiki/Serial_Line_Internet_Protocol

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "print_person.h"

typedef struct
{
    // Original (lower level) stream callback and its state
    pb_decode_ctx_read_callback_t stream_callback;
    void *stream_callback_state;

    bool frame_ended; // SLIP frame has ended
    bool stream_ended; // Underlying stream has ended

    // Temporary buffer for reading
    pb_byte_t tmpbuf[16];
    pb_size_t bytes_in_buf;
} slip_istream_t;

pb_size_t read_slip_callback(pb_decode_ctx_t *ctx, pb_byte_t *buf, pb_size_t count)
{
    // Take our state variable and restore the state of the underlying callback
    slip_istream_t *slip = (slip_istream_t*)ctx->stream_callback_state;
    ctx->stream_callback_state = slip->stream_callback_state;

    pb_size_t bytes_done = 0;
    while (bytes_done < count && !slip->stream_ended && !slip->frame_ended)
    {
        if (slip->bytes_in_buf == 0)
        {
            // Read more data into buffer
            pb_size_t was_read = slip->stream_callback(ctx, slip->tmpbuf, sizeof(slip->tmpbuf));
            
            if (was_read > sizeof(slip->tmpbuf))
            {
                bytes_done = PB_READ_ERROR;
                slip->stream_ended = true;
                break;
            }
            else if (was_read == 0)
            {
                slip->stream_ended = true;
            }
            
            slip->bytes_in_buf += was_read;
        }

        // Unescape any escaped characters
        pb_byte_t *wrpos = buf + bytes_done;
        pb_byte_t *wrend = buf + count;
        pb_byte_t *rdpos = slip->tmpbuf;
        pb_byte_t *rdend = slip->tmpbuf + slip->bytes_in_buf;
        while (rdpos < rdend && wrpos < wrend)
        {
            pb_byte_t byte = *rdpos++;

            if (byte == 0xC0)
            {
                // End of frame
                slip->frame_ended = true;
                break;
            }
            else if (byte == 0xDB)
            {
                if (rdpos == rdend)
                {
                    // Need one byte more
                    pb_size_t ret = slip->stream_callback(ctx, &byte, 1);
                    if (ret != 1)
                    {
                        // Underlying IO error or framing format error
                        bytes_done = PB_READ_ERROR;
                        slip->stream_ended = true;
                        break;
                    }
                }
                else
                {
                    byte = *rdpos++;
                }

                *wrpos++ = (byte == 0xDD) ? 0xDB : 0xC0;
            }
            else
            {
                *wrpos++ = byte;
            }
        }

        if (rdpos < rdend)
        {
            // Move remaining bytes to beginning of the temporary buffer
            pb_size_t remain = rdend - rdpos;
            memmove(slip->tmpbuf, rdpos, remain);
            slip->bytes_in_buf = remain;
        }
        else
        {
            slip->bytes_in_buf = 0;
        }

        if (bytes_done != PB_READ_ERROR)
        {
            // Check how many bytes were output
            bytes_done = wrpos - buf;
        }
    }

    // Restore state and return number of bytes read or error
    ctx->stream_callback_state = slip;
    return bytes_done;
}

// Applies SLIP-framing input stream wrapper to existing stream.
// The wrapper unescapes SLIP bytes and detects 0xC0 end-of-frame.
void init_slip_istream(pb_decode_ctx_t *ctx, slip_istream_t *slip)
{
    memset(slip, 0, sizeof(slip_istream_t));
    slip->stream_callback = ctx->stream_callback;
    slip->stream_callback_state = ctx->stream_callback_state;
    ctx->stream_callback = read_slip_callback;
    ctx->stream_callback_state = slip;
}

// Begin reading a slip message.
// Returns false if the underlying stream has ended.
bool slip_begin_message(pb_decode_ctx_t *ctx, pb_size_t max_msglen)
{
    // Take our state variable and restore the state of the underlying callback
    slip_istream_t *slip = (slip_istream_t*)ctx->stream_callback_state;
    ctx->stream_callback_state = slip->stream_callback_state;

    ctx->bytes_left = max_msglen;
    slip->frame_ended = false;

    if (slip->bytes_in_buf == 0 && !slip->stream_ended)
    {
        pb_size_t was_read = slip->stream_callback(ctx, slip->tmpbuf, 1);
        if (was_read != 1)
        {
            slip->stream_ended = true;
        }
        else
        {
            slip->bytes_in_buf = 1;
        }
    }
    
    // Restore state and return result status
    ctx->stream_callback_state = slip;
    return !slip->stream_ended;
}

int main()
{
    pb_decode_ctx_t stream;
    pb_byte_t tmpbuf[16];
    slip_istream_t slip;
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, tmpbuf, sizeof(tmpbuf));
    init_slip_istream(&stream, &slip);
    
    // Decode as many messages as come in
    while (slip_begin_message(&stream, Person_size))
    {
        Person person = Person_init_zero;
        
        if (!pb_decode(&stream, Person_fields, &person))
        {
            printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }

        print_person(&person);

        printf("--------\n");
    }
    
    return 0;
}
