#include <stdio.h>
#include <string.h>
#include "pb_decode.h"
#include "unittests.h"

#define S(x) pb_istream_from_buffer((uint8_t*)x, sizeof(x) - 1)

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
        uint64_t u;
        int64_t i;
        
        COMMENT("Test pb_decode_varint");
        TEST((s = S("\x00"), pb_decode_varint(&s, &u) && u == 0));
        TEST((s = S("\x01"), pb_decode_varint(&s, &u) && u == 1));
        TEST((s = S("\xAC\x02"), pb_decode_varint(&s, &u) && u == 300));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint(&s, &u) && u == UINT32_MAX));
        TEST((s = S("\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint(&s, (uint64_t*)&i) && i == UINT32_MAX));
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint(&s, (uint64_t*)&i) && i == -1));
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint(&s, &u) && u == UINT64_MAX));
    }
    
    {
        pb_istream_t s;
        COMMENT("Test pb_skip_varint");
        TEST((s = S("\x00""foobar"), pb_skip_varint(&s) && s.bytes_left == 6))
        TEST((s = S("\xAC\x02""foobar"), pb_skip_varint(&s) && s.bytes_left == 6))
        TEST((s = S("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01""foobar"),
              pb_skip_varint(&s) && s.bytes_left == 6))
    }
    
    {
        pb_istream_t s;
        COMMENT("Test pb_skip_string")
        TEST((s = S("\x00""foobar"), pb_skip_string(&s) && s.bytes_left == 6))
        TEST((s = S("\x04""testfoobar"), pb_skip_string(&s) && s.bytes_left == 6))
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
    
    {
        pb_istream_t s;
        pb_field_t f = {1, PB_LTYPE_FIXED, 0, 0, 4, 0, 0};
        float d;
        
        COMMENT("Test pb_dec_fixed using float (failures here may be caused by imperfect rounding)")
        TEST((s = S("\x00\x00\x00\x00"), pb_dec_fixed(&s, &f, &d) && d == 0.0f))
        TEST((s = S("\x00\x00\xc6\x42"), pb_dec_fixed(&s, &f, &d) && d == 99.0f))
        TEST((s = S("\x4e\x61\x3c\xcb"), pb_dec_fixed(&s, &f, &d) && d == -12345678.0f))
        TEST((s = S("\x00"), !pb_dec_fixed(&s, &f, &d) && d == -12345678.0f))
    }
    
    {
        pb_istream_t s;
        pb_field_t f = {1, PB_LTYPE_FIXED, 0, 0, 8, 0, 0};
        double d;
        
        COMMENT("Test pb_dec_fixed using double (failures here may be caused by imperfect rounding)")
        TEST((s = S("\x00\x00\x00\x00\x00\x00\x00\x00"), pb_dec_fixed(&s, &f, &d) && d == 0.0))
        TEST((s = S("\x00\x00\x00\x00\x00\xc0\x58\x40"), pb_dec_fixed(&s, &f, &d) && d == 99.0))
        TEST((s = S("\x00\x00\x00\xc0\x29\x8c\x67\xc1"), pb_dec_fixed(&s, &f, &d) && d == -12345678.0f))
    }
    
    {
        pb_istream_t s;
        struct { size_t size; uint8_t bytes[5]; } d;
        pb_field_t f = {1, PB_LTYPE_BYTES, 0, 0, 5, 0, 0};
        
        COMMENT("Test pb_dec_bytes")
        TEST((s = S("\x00"), pb_dec_bytes(&s, &f, &d) && d.size == 0))
        TEST((s = S("\x01\xFF"), pb_dec_bytes(&s, &f, &d) && d.size == 1 && d.bytes[0] == 0xFF))
        TEST((s = S("\x06xxxxxx"), !pb_dec_bytes(&s, &f, &d)))
        TEST((s = S("\x05xxxxx"), pb_dec_bytes(&s, &f, &d) && d.size == 5))
        TEST((s = S("\x05xxxx"), !pb_dec_bytes(&s, &f, &d)))
    }
    
    {
        pb_istream_t s;
        pb_field_t f = {1, PB_LTYPE_STRING, 0, 0, 5, 0, 0};
        char d[5];
        
        COMMENT("Test pb_dec_string")
        TEST((s = S("\x00"), pb_dec_string(&s, &f, &d) && d[0] == '\0'))
        TEST((s = S("\x04xyzz"), pb_dec_string(&s, &f, &d) && strcmp(d, "xyzz") == 0))
        TEST((s = S("\x05xyzzy"), !pb_dec_string(&s, &f, &d)))
    }
    
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");
    
    return status;
}
