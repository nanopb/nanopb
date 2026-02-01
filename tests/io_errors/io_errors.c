/* Simulate IO errors after each byte in a stream.
 * Verifies proper error propagation.
 */

#include <stdio.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include "alltypes.pb.h"
#include "test_helpers.h"
#include "malloc_wrappers.h"

typedef struct
{
    uint8_t *buffer;
    size_t fail_after;
} faulty_stream_t;

bool read_callback(pb_decode_ctx_t *stream, uint8_t *buf, size_t count)
{
    faulty_stream_t *state = stream->state;
    
    while (count--)
    {
        if (state->fail_after == 0)
            PB_RETURN_ERROR(stream, "simulated");
        state->fail_after--;
        *buf++ = *state->buffer++;
    }
    
    return true;
}
bool write_callback(pb_encode_ctx_t *stream, const uint8_t *buf, size_t count)
{
    faulty_stream_t *state = stream->state;
    
    while (count--)
    {
        if (state->fail_after == 0)
            PB_RETURN_ERROR(stream, "simulated");
        state->fail_after--;
        *state->buffer++ = *buf++;
    }
    
    return true;
}

int main()
{
    uint8_t buffer[2048];
    size_t msglen;
    AllTypes msg = AllTypes_init_zero;
    
    /* Get some base data to run the tests with */
    SET_BINARY_MODE(stdin);
    msglen = fread(buffer, 1, sizeof(buffer), stdin);
    
    /* Test IO errors on decoding */
    {
        bool status;
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_callback(&stream, &read_callback, NULL, PB_SIZE_MAX, NULL, 0);
        faulty_stream_t fs;
        size_t i;

#if PB_NO_DEFAULT_ALLOCATOR && !PB_NO_MALLOC
        stream.allocator = malloc_wrappers_allocator;
#endif
        
        for (i = 0; i < msglen; i++)
        {
            stream.bytes_left = msglen;
            stream.state = &fs;
            fs.buffer = buffer;
            fs.fail_after = i;

            status = pb_decode(&stream, AllTypes_fields, &msg);
            if (status != false)
            {
                fprintf(stderr, "Unexpected success in decode\n");
                return 2;
            }
#if !PB_NO_ERRMSG
            else if (strcmp(stream.errmsg, "simulated") != 0)
#else
            else if (fs.fail_after > 0)
#endif
            {
                fprintf(stderr, "Wrong error in decode: %s\n", PB_GET_ERROR(&stream));
                return 3;
            }
        }
        
        stream.bytes_left = msglen;
        stream.state = &fs;
        fs.buffer = buffer;
        fs.fail_after = msglen;
        status = pb_decode(&stream, AllTypes_fields, &msg);
        
        if (!status)
        {
            fprintf(stderr, "Decoding failed: %s\n", PB_GET_ERROR(&stream));
            return 4;
        }
    }
    
    /* Test IO errors on encoding */
    {
        bool status;
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_callback(&stream, &write_callback, NULL, PB_SIZE_MAX, NULL, 0);
        faulty_stream_t fs;
        size_t i;
        
        for (i = 0; i < msglen; i++)
        {
            stream.max_size = msglen;
            stream.bytes_written = 0;
            stream.state = &fs;
            fs.buffer = buffer;
            fs.fail_after = i;

            status = pb_encode(&stream, AllTypes_fields, &msg);
            if (status != false)
            {
                fprintf(stderr, "Unexpected success in encode\n");
                return 5;
            }
#if !PB_NO_ERRMSG
            else if (strcmp(stream.errmsg, "simulated") != 0)
#else
            else if (fs.fail_after > 0)
#endif
            {
                fprintf(stderr, "Wrong error in encode: %s\n", PB_GET_ERROR(&stream));
                return 6;
            }
        }
        
        stream.max_size = msglen;
        stream.bytes_written = 0;
        stream.state = &fs;
        fs.buffer = buffer;
        fs.fail_after = msglen;
        status = pb_encode(&stream, AllTypes_fields, &msg);
        
        if (!status)
        {
            fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return 7;
        }
    }

#if !PB_NO_DEFAULT_ALLOCATOR
    pb_release(NULL, AllTypes_fields, &msg);
#elif !PB_NO_MALLOC
    {
        pb_decode_ctx_t stream;
        stream.allocator = malloc_wrappers_allocator;
        pb_release(&stream, AllTypes_fields, &msg);
    }
#endif

#if !PB_NO_MALLOC
    assert(get_alloc_count() == 0);
#endif

    return 0;   
}

