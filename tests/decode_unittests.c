#include <stdio.h>
#include <string.h>
#include "pb_decode.h"
#include "unittests.h"

#define S(x) pb_istream_from_buffer((uint8_t*)x, sizeof(x))

bool stream_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    if (stream->state != NULL)
        return false; /* Simulate error */
    
    if (buf != NULL)
        memset(buf, 'x', count);
    return true;
}

int main()
{
    int status = 0;
    
    {
        uint8_t buffer1[] = "foobartest1234";
        uint8_t buffer2[sizeof(buffer1)];
        pb_istream_t stream = pb_istream_from_buffer(buffer1, sizeof(buffer1));
        
        COMMENT("Test pb_read and pb_istream_t");
        TEST(pb_read(&stream, buffer2, 6))
        TEST(memcmp(buffer2, "foobar", 6) == 0)
        TEST(stream.bytes_left == sizeof(buffer1) - 6)
        TEST(pb_read(&stream, buffer2 + 6, stream.bytes_left))
        TEST(memcmp(buffer1, buffer2, sizeof(buffer1)) == 0)
        TEST(stream.bytes_left == 0)
        TEST(!pb_read(&stream, buffer2, 1))
    }
    
    {
        uint8_t buffer[20];
        pb_istream_t stream = {&stream_callback, NULL, 20};
        
        COMMENT("Test pb_read with custom callback");
        TEST(pb_read(&stream, buffer, 5))
        TEST(memcmp(buffer, "xxxxx", 5) == 0)
        TEST(!pb_read(&stream, buffer, 50))
        stream.state = (void*)1; /* Simulated error return from callback */
        TEST(!pb_read(&stream, buffer, 5))
        stream.state = NULL;
        TEST(pb_read(&stream, buffer, 15))
    }
    
    {
        pb_istream_t s;
        uint32_t u;
        int32_t i;
        
        COMMENT("Test pb_decode_varint32");
        TEST((s = S("\x00"), pb_decode_varint32(&s, &u) && u == 0));
        TEST((s = S("\x01"), pb_decode_varint32(&s, &u) && u == 1));
        TEST((s = S("\xAC\x02"), pb_decode_varint32(&s, &u) && u == 300));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint32(&s, &u) && u == UINT32_MAX));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint32(&s, (uint32_t*)&i) && i == -1));
    }
    
    {
        pb_istream_t s;
        uint64_t u;
        int64_t i;
        
        COMMENT("Test pb_decode_varint64");
        TEST((s = S("\x00"), pb_decode_varint64(&s, &u) && u == 0));
        TEST((s = S("\x01"), pb_decode_varint64(&s, &u) && u == 1));
        TEST((s = S("\xAC\x02"), pb_decode_varint64(&s, &u) && u == 300));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint64(&s, &u) && u == UINT32_MAX));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint64(&s, (uint64_t*)&i) && i == UINT32_MAX));
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint64(&s, (uint64_t*)&i) && i == -1));
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint64(&s, &u) && u == UINT64_MAX));
    }
    
    {
        pb_istream_t s;
        COMMENT("Test pb_skip_varint");
        TEST((s = S("\x00""foobar"), pb_skip_varint(&s) && s.bytes_left == 7))
        TEST((s = S("\xAC\x02""foobar"), pb_skip_varint(&s) && s.bytes_left == 7))
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01""foobar"),
              pb_skip_varint(&s) && s.bytes_left == 7))
    }
    
    {
        pb_istream_t s;
        COMMENT("Test pb_skip_string")
        TEST((s = S("\x00""foobar"), pb_skip_string(&s) && s.bytes_left == 7))
        TEST((s = S("\x04""testfoobar"), pb_skip_string(&s) && s.bytes_left == 7))
    }
    
    {
        pb_istream_t s = S("\x01\xFF\xFF\x03");
        pb_field_t f = {1, PB_LTYPE_VARINT, 0, 0, 4, 0, 0};
        uint32_t d;
        COMMENT("Test pb_dec_varint using uint32_t")
        TEST(pb_dec_varint(&s, &f, &d) && d == 1)
        
        /* Verify that no more than data_size is written. */
        d = 0;
        f.data_size = 1;
        TEST(pb_dec_varint(&s, &f, &d) && (d == 0xFF || d == 0xFF000000))
    }
    
    {
        pb_istream_t s;
        pb_field_t f = {1, PB_LTYPE_SVARINT, 0, 0, 4, 0, 0};
        int32_t d;
        
        COMMENT("Test pb_dec_svarint using int32_t")
        TEST((s = S("\x01"), pb_dec_svarint(&s, &f, &d) && d == -1))
        TEST((s = S("\x02"), pb_dec_svarint(&s, &f, &d) && d == 1))
        TEST((s = S("\xfe\xff\xff\xff\x0f"), pb_dec_svarint(&s, &f, &d) && d == INT32_MAX))
        TEST((s = S("\xff\xff\xff\xff\x0f"), pb_dec_svarint(&s, &f, &d) && d == INT32_MIN))
    }
    
    {
        pb_istream_t s;
        pb_field_t f = {1, PB_LTYPE_SVARINT, 0, 0, 8, 0, 0};
        uint64_t d;
        
        COMMENT("Test pb_dec_svarint using uint64_t")
        TEST((s = S("\x01"), pb_dec_svarint(&s, &f, &d) && d == -1))
        TEST((s = S("\x02"), pb_dec_svarint(&s, &f, &d) && d == 1))
        TEST((s = S("\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"), pb_dec_svarint(&s, &f, &d) && d == INT64_MAX))
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"), pb_dec_svarint(&s, &f, &d) && d == INT64_MIN))
    }
    
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");
    
    return status;
}
