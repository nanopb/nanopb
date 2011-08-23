#include <stdio.h>
#include <string.h>
#include "pb_encode.h"
#include "unittests.h"

bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    /* Allow only 'x' to be written */
    while (count--)
    {
        if (*buf++ != 'x')
            return false;
    }
    return true;
}

/* Check that expression x writes data y.
 * Y is a string, which may contain null bytes. Null terminator is ignored.
 */
#define WRITES(x, y) \
memset(buffer, 0xAA, sizeof(buffer)), \
s = pb_ostream_from_buffer(buffer, sizeof(buffer)), \
(x) && \
memcmp(buffer, y, sizeof(y) - 1) == 0 && \
buffer[sizeof(y) - 1] == 0xAA

int main()
{
    int status = 0;
    
    {
        uint8_t buffer1[] = "foobartest1234";
        uint8_t buffer2[sizeof(buffer1)];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer2, sizeof(buffer1));
        
        COMMENT("Test pb_write and pb_ostream_t");
        TEST(pb_write(&stream, buffer1, sizeof(buffer1)));
        TEST(memcmp(buffer1, buffer2, sizeof(buffer1)) == 0);
        TEST(!pb_write(&stream, buffer1, 1));
        TEST(stream.bytes_written == sizeof(buffer1));
    }
    
    {
        uint8_t buffer1[] = "xxxxxxx";
        pb_ostream_t stream = {&streamcallback, 0, SIZE_MAX, 0};
        
        COMMENT("Test pb_write with custom callback");
        TEST(pb_write(&stream, buffer1, 5));
        buffer1[0] = 'a';
        TEST(!pb_write(&stream, buffer1, 5));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        
        COMMENT("Test pb_encode_varint")
        TEST(WRITES(pb_encode_varint(&s, 0), "\0"));
        TEST(WRITES(pb_encode_varint(&s, 1), "\1"));
        TEST(WRITES(pb_encode_varint(&s, 0x7F), "\x7F"));
        TEST(WRITES(pb_encode_varint(&s, 0x80), "\x80\x01"));
        TEST(WRITES(pb_encode_varint(&s, UINT32_MAX), "\xFF\xFF\xFF\xFF\x0F"));
        TEST(WRITES(pb_encode_varint(&s, UINT64_MAX), "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        
        COMMENT("Test pb_encode_tag")
        TEST(WRITES(pb_encode_tag(&s, PB_WT_STRING, 5), "\x2A"));
        TEST(WRITES(pb_encode_tag(&s, PB_WT_VARINT, 99), "\x98\x06"));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        pb_field_t field = {10, PB_LTYPE_SVARINT};
        
        COMMENT("Test pb_encode_tag_for_field")
        TEST(WRITES(pb_encode_tag_for_field(&s, &field), "\x50"));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        
        COMMENT("Test pb_encode_string")
        TEST(WRITES(pb_encode_string(&s, (const uint8_t*)"abcd", 4), "\x04""abcd"));
        TEST(WRITES(pb_encode_string(&s, (const uint8_t*)"abcd\x00", 5), "\x05""abcd\x00"));
        TEST(WRITES(pb_encode_string(&s, (const uint8_t*)"", 0), "\x00"));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        uint8_t value = 1;
        int8_t svalue = -1;
        int32_t max = INT32_MAX;
        int32_t min = INT32_MIN;
        int64_t lmax = INT64_MAX;
        int64_t lmin = INT64_MIN;
        pb_field_t field = {1, PB_LTYPE_VARINT, 0, 0, sizeof(value)};
        
        COMMENT("Test pb_enc_varint and pb_enc_svarint")
        TEST(WRITES(pb_enc_varint(&s, &field, &value), "\x01"));
        TEST(WRITES(pb_enc_svarint(&s, &field, &svalue), "\x01"));
        TEST(WRITES(pb_enc_svarint(&s, &field, &value), "\x02"));
        
        field.data_size = sizeof(max);
        TEST(WRITES(pb_enc_svarint(&s, &field, &max), "\xfe\xff\xff\xff\x0f"));
        TEST(WRITES(pb_enc_svarint(&s, &field, &min), "\xff\xff\xff\xff\x0f"));
        
        field.data_size = sizeof(lmax);
        TEST(WRITES(pb_enc_svarint(&s, &field, &lmax), "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"));
        TEST(WRITES(pb_enc_svarint(&s, &field, &lmin), "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"));
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        pb_field_t field = {1, PB_LTYPE_FIXED, 0, 0, sizeof(float)};
        float fvalue;
        double dvalue;
        
        COMMENT("Test pb_enc_fixed using float")
        fvalue = 0.0f;
        TEST(WRITES(pb_enc_fixed(&s, &field, &fvalue), "\x00\x00\x00\x00"))
        fvalue = 99.0f;
        TEST(WRITES(pb_enc_fixed(&s, &field, &fvalue), "\x00\x00\xc6\x42"))
        fvalue = -12345678.0f;
        TEST(WRITES(pb_enc_fixed(&s, &field, &fvalue), "\x4e\x61\x3c\xcb"))
    
        COMMENT("Test pb_enc_fixed using double")
        field.data_size = sizeof(double);
        dvalue = 0.0;
        TEST(WRITES(pb_enc_fixed(&s, &field, &dvalue), "\x00\x00\x00\x00\x00\x00\x00\x00"))
        dvalue = 99.0;
        TEST(WRITES(pb_enc_fixed(&s, &field, &dvalue), "\x00\x00\x00\x00\x00\xc0\x58\x40"))
        dvalue = -12345678.0;
        TEST(WRITES(pb_enc_fixed(&s, &field, &dvalue), "\x00\x00\x00\xc0\x29\x8c\x67\xc1"))
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        struct { size_t size; uint8_t bytes[5]; } value = {5, {'x', 'y', 'z', 'z', 'y'}};
    
        COMMENT("Test pb_enc_bytes")
        TEST(WRITES(pb_enc_bytes(&s, NULL, &value), "\x05xyzzy"))
        value.size = 0;
        TEST(WRITES(pb_enc_bytes(&s, NULL, &value), "\x00"))
    }
    
    {
        uint8_t buffer[30];
        pb_ostream_t s;
        char value[] = "xyzzy";
        
        COMMENT("Test pb_enc_string")
        TEST(WRITES(pb_enc_string(&s, NULL, &value), "\x05xyzzy"))
        value[0] = '\0';
        TEST(WRITES(pb_enc_string(&s, NULL, &value), "\x00"))
    }
    
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");
    
    return status;
}
