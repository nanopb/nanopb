// Example of parsing a null-terminated protobuf stream.
// Because 0 tag is invalid in protobuf format, it can be used to indicate message end.
// Support for this feature was previously built-in to the nanopb library, but
// removed in version 1.0. It can still be implemented in user code like this.
//
// Note that other protobuf libraries do not support null termination, though in most
// of them it can be implemented similar to the code below.

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "print_person.h"

typedef enum {
    NTS_FIRST_BYTE,
    NTS_BEGIN_TAG,
    NTS_WAIT_TAG_END,
    NTS_TAG_DONE,
    NTS_READ_LENGTH,
    NTS_WAIT_VARINT_END,
    NTS_WAIT_NEXT_TAG,
    NTS_END_OF_STREAM
} nts_state_t;

typedef struct
{
    // Original (lower level) stream callback and its state
    pb_decode_ctx_read_callback_t stream_callback;
    void *stream_callback_state;

    // Indicates EOF of the underlying stream
    bool stream_ended;

    // Our parsing state
    nts_state_t state;
    pb_wire_type_t wiretype;
    uint8_t bitpos;
    pb_byte_t first_byte;
    pb_size_t length_to_next_tag;
} nullterminated_istream_t;

pb_size_t nullterminated_istream_callback(pb_decode_ctx_t *ctx, pb_byte_t *buf, pb_size_t count)
{
    // Take our state variable and restore the state of the underlying callback
    nullterminated_istream_t *nts = (nullterminated_istream_t*)ctx->stream_callback_state;
    ctx->stream_callback_state = nts->stream_callback_state;

    // We need to parse enough of the protobuf structure to detect where a new tag begins
    pb_size_t bytes_read = 0;
    while (bytes_read < count && nts->state != NTS_END_OF_STREAM)
    {
        pb_byte_t byte = 0;
        pb_size_t to_read = 0, was_read = 0;

        if (nts->state == NTS_FIRST_BYTE)
        {
            // First byte has already been read to test whether the underlying stream continues.
            byte = buf[0] = nts->first_byte;
            bytes_read++;
            to_read = was_read = 1;
            nts->state = NTS_BEGIN_TAG;
        }
        else
        {
            // If we know the length of current element, we can read more than 1 byte.
            to_read = (nts->state == NTS_WAIT_NEXT_TAG) ? nts->length_to_next_tag : 1;
            if (to_read > count - bytes_read) to_read = count - bytes_read;
            
            // Read more data from callback and check for lower level EOF/error
            was_read = nts->stream_callback(ctx, buf + bytes_read, to_read);
            if (was_read > to_read)
            {
                // IO error occurred
                bytes_read = PB_READ_ERROR;
                nts->state = NTS_END_OF_STREAM;
                nts->stream_ended = true;
                break;
            }

            bytes_read += was_read;
            if (was_read < to_read)
            {
                // EOF in underlying stream
                nts->state = NTS_END_OF_STREAM;
                nts->stream_ended = true;
                break;
            }

            byte = buf[bytes_read - 1];
        }
        
        // Process protobuf elements
        switch(nts->state)
        {
            case NTS_BEGIN_TAG:
                nts->wiretype = (pb_wire_type_t)(byte & 0x07);

                if (byte == 0x00)
                {
                    // The terminating null byte shouldn't be passed to nanopb
                    bytes_read -= 1;
                    nts->state = NTS_END_OF_STREAM;
                }
                else if (byte & 0x80)
                {
                    nts->state = NTS_WAIT_TAG_END;
                }
                else
                {
                    nts->state = NTS_TAG_DONE;
                }
                break;
            
            case NTS_WAIT_TAG_END:
                if ((byte & 0x80) == 0) nts->state = NTS_TAG_DONE;
                break;
                
            case NTS_READ_LENGTH:
                nts->length_to_next_tag |= ((byte & 0x7F) << nts->bitpos);
                nts->bitpos += 7;
                if ((byte & 0x80) == 0) nts->state = NTS_WAIT_NEXT_TAG;
                break;
            
            case NTS_WAIT_VARINT_END:
                if ((byte & 0x80) == 0) nts->state = NTS_BEGIN_TAG;
                break;
            
            case NTS_WAIT_NEXT_TAG:
                nts->length_to_next_tag -= was_read;
                if (nts->length_to_next_tag == 0) nts->state = NTS_BEGIN_TAG;
                break;
            
            default:
                nts->state = NTS_END_OF_STREAM;
                break;
        }

        // This pseudo-state dispatches based on decoded wire_type
        if (nts->state == NTS_TAG_DONE)
        {
            switch (nts->wiretype)
            {
                case PB_WT_32BIT:
                    nts->length_to_next_tag = 4;
                    nts->state = NTS_WAIT_NEXT_TAG;
                    break;
                
                case PB_WT_64BIT:
                    nts->length_to_next_tag = 8;
                    nts->state = NTS_WAIT_NEXT_TAG;
                    break;
                
                case PB_WT_STRING:
                    nts->length_to_next_tag = 0;
                    nts->bitpos = 0;
                    nts->state = NTS_READ_LENGTH;
                    break;
                
                case PB_WT_VARINT:
                    nts->state = NTS_WAIT_VARINT_END;
                    break;
                
                default:
                    nts->state = NTS_END_OF_STREAM;
            }
        }
    }

    // Restore our own state variable and return number of bytes read
    ctx->stream_callback_state = nts;
    return bytes_read;
}

// Applies the nullterminated input stream wrapper to existing stream.
// The wrapper will read data from the underlying stream and detect 0x00
// bytes when they occur at the tag position.
void init_nullterminated_istream(pb_decode_ctx_t *ctx, nullterminated_istream_t *nts)
{
    memset(nts, 0, sizeof(nullterminated_istream_t));
    nts->stream_callback = ctx->stream_callback;
    nts->stream_callback_state = ctx->stream_callback_state;
    nts->state = NTS_BEGIN_TAG;
    ctx->stream_callback = nullterminated_istream_callback;
    ctx->stream_callback_state = nts;
}

// Begin reading a null-terminated message.
// Returns false if the underlying stream has ended.
bool nts_begin_message(pb_decode_ctx_t *ctx, pb_size_t max_msglen)
{
    nullterminated_istream_t *nts = (nullterminated_istream_t*)ctx->stream_callback_state;
    if (nts->stream_ended)
    {
        return false;
    }

    // Read first byte of the new message, to detect EOF.
    ctx->bytes_left = max_msglen;
    ctx->rdpos = NULL;
    ctx->stream_callback_state = nts->stream_callback_state;
    pb_byte_t retval = nts->stream_callback(ctx, &nts->first_byte, 1);
    ctx->stream_callback_state = nts;

    if (retval == 1)
    {
        nts->state = NTS_FIRST_BYTE;
        return true;
    }
    else
    {
        nts->state = NTS_END_OF_STREAM;
        nts->stream_ended = true;
        return false;
    }
}

int main()
{
    pb_decode_ctx_t stream;
    nullterminated_istream_t nts;
    pb_byte_t tmpbuf[16];
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, tmpbuf, sizeof(tmpbuf));
    init_nullterminated_istream(&stream, &nts);

    // Decode as many messages as come in, delimited by null bytes.
    while (nts_begin_message(&stream, Person_size))
    {
        Person person = Person_init_zero;
        
        if (!pb_decode(&stream, Person_fields, &person))
        {
            printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }

        print_person(&person);

        if (nts.state != NTS_END_OF_STREAM)
        {
            printf("Missing null terminator\n");
            return 2;
        }

        printf("--------\n");
    }
    
    return 0;
}
