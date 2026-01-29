/* This includes the whole .c file to get access to static functions. */
#define PB_ENABLE_MALLOC
#include "pb_common.c"
#include "pb_decode.c"

#include <stdio.h>
#include <string.h>
#include "unittests.h"
#include "unittestproto.pb.h"

#define S(s,x) pb_init_decode_ctx_for_buffer(s,(uint8_t*)x, sizeof(x) - 1)

bool stream_callback(pb_decode_ctx_t *stream, uint8_t *buf, size_t count)
{
    if (stream->state != NULL)
        return false; /* Simulate error */

    if (buf != NULL)
        memset(buf, 'x', count);
    return true;
}

/* Verifies that the stream passed to callback matches the byte array pointed to by arg. */
bool callback_check(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    int i;
    uint8_t byte;
    pb_bytes_array_t *ref = (pb_bytes_array_t*) *arg;

    for (i = 0; i < ref->size; i++)
    {
        if (!pb_read(stream, &byte, 1))
            return false;

        if (byte != ref->bytes[i])
            return false;
    }

    return true;
}

int main()
{
    int status = 0;

    {
        uint8_t buffer1[] = "foobartest1234";
        uint8_t buffer2[sizeof(buffer1)];
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buffer1, sizeof(buffer1));

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
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_callback(&stream, &stream_callback, NULL, 20, NULL, 0);

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
        pb_decode_ctx_t s;
        uint64_t u;
        int64_t i;

        COMMENT("Test pb_decode_varint");
        TEST((S(&s,"\x00"), pb_decode_varint(&s, &u) && u == 0));
        TEST((S(&s,"\x01"), pb_decode_varint(&s, &u) && u == 1));
        TEST((S(&s,"\xAC\x02"), pb_decode_varint(&s, &u) && u == 300));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint(&s, &u) && u == UINT32_MAX));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint(&s, (uint64_t*)&i) && i == UINT32_MAX));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint(&s, (uint64_t*)&i) && i == -1));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              pb_decode_varint(&s, &u) && u == UINT64_MAX));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"),
              !pb_decode_varint(&s, &u)));
    }

    {
        pb_decode_ctx_t s;
        uint32_t u;

        COMMENT("Test pb_decode_varint32");
        TEST((S(&s,"\x00"), pb_decode_varint32(&s, &u) && u == 0));
        TEST((S(&s,"\x01"), pb_decode_varint32(&s, &u) && u == 1));
        TEST((S(&s,"\xAC\x02"), pb_decode_varint32(&s, &u) && u == 300));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x0F"), pb_decode_varint32(&s, &u) && u == UINT32_MAX));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x8F\x00"), pb_decode_varint32(&s, &u) && u == UINT32_MAX));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x10"), !pb_decode_varint32(&s, &u)));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\x40"), !pb_decode_varint32(&s, &u)));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\x01"), !pb_decode_varint32(&s, &u)));
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x80\x00"), !pb_decode_varint32(&s, &u)));
    }

    {
        pb_decode_ctx_t s;
        COMMENT("Test pb_skip_varint");
        TEST((S(&s,"\x00""foobar"), pb_skip_varint(&s) && s.bytes_left == 6))
        TEST((S(&s,"\xAC\x02""foobar"), pb_skip_varint(&s) && s.bytes_left == 6))
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01""foobar"),
              pb_skip_varint(&s) && s.bytes_left == 6))
        TEST((S(&s,"\xFF"), !pb_skip_varint(&s)))
    }

    {
        pb_decode_ctx_t s;
        COMMENT("Test pb_skip_string")
        TEST((S(&s,"\x00""foobar"), pb_skip_string(&s) && s.bytes_left == 6))
        TEST((S(&s,"\x04""testfoobar"), pb_skip_string(&s) && s.bytes_left == 6))
        TEST((S(&s,"\x04"), !pb_skip_string(&s)))
        TEST((S(&s,"\xFF"), !pb_skip_string(&s)))
    }

    {
        pb_decode_ctx_t s;
        S(&s,"\x01\x00");

        pb_field_iter_t f;
        uint32_t d;

        f.type = PB_LTYPE_VARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint using uint32_t")
        TEST(pb_dec_varint(&s, &f) && d == 1)

        /* Verify that no more than data_size is written. */
        d = 0xFFFFFFFF;
        f.data_size = 1;
        TEST(pb_dec_varint(&s, &f) && (d == 0xFFFFFF00 || d == 0x00FFFFFF))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        int32_t d;

        f.type = PB_LTYPE_SVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint using sint32_t")
        TEST((S(&s,"\x01"), pb_dec_varint(&s, &f) && d == -1))
        TEST((S(&s,"\x02"), pb_dec_varint(&s, &f) && d == 1))
        TEST((S(&s,"\xfe\xff\xff\xff\x0f"), pb_dec_varint(&s, &f) && d == INT32_MAX))
        TEST((S(&s,"\xff\xff\xff\xff\x0f"), pb_dec_varint(&s, &f) && d == INT32_MIN))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        int64_t d;

        f.type = PB_LTYPE_SVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint using sint64_t")
        TEST((S(&s,"\x01"), pb_dec_varint(&s, &f) && d == -1))
        TEST((S(&s,"\x02"), pb_dec_varint(&s, &f) && d == 1))
        TEST((S(&s,"\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"), pb_dec_varint(&s, &f) && d == INT64_MAX))
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"), pb_dec_varint(&s, &f) && d == INT64_MIN))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        int32_t d;

        f.type = PB_LTYPE_SVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint overflow detection using sint32_t");
        TEST((S(&s,"\xfe\xff\xff\xff\x0f"), pb_dec_varint(&s, &f)));
        TEST((S(&s,"\xfe\xff\xff\xff\x10"), !pb_dec_varint(&s, &f)));
        TEST((S(&s,"\xff\xff\xff\xff\x0f"), pb_dec_varint(&s, &f)));
        TEST((S(&s,"\xff\xff\xff\xff\x10"), !pb_dec_varint(&s, &f)));
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        uint32_t d;

        f.type = PB_LTYPE_UVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint using uint32_t")
        TEST((S(&s,"\x01"), pb_dec_varint(&s, &f) && d == 1))
        TEST((S(&s,"\x02"), pb_dec_varint(&s, &f) && d == 2))
        TEST((S(&s,"\xff\xff\xff\xff\x0f"), pb_dec_varint(&s, &f) && d == UINT32_MAX))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        uint64_t d;

        f.type = PB_LTYPE_UVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint using uint64_t")
        TEST((S(&s,"\x01"), pb_dec_varint(&s, &f) && d == 1))
        TEST((S(&s,"\x02"), pb_dec_varint(&s, &f) && d == 2))
        TEST((S(&s,"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01"), pb_dec_varint(&s, &f) && d == UINT64_MAX))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        uint32_t d;

        f.type = PB_LTYPE_UVARINT;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_varint overflow detection using uint32_t");
        TEST((S(&s,"\xff\xff\xff\xff\x0f"), pb_dec_varint(&s, &f)));
        TEST((S(&s,"\xff\xff\xff\xff\x10"), !pb_dec_varint(&s, &f)));
    }

    {
        pb_decode_ctx_t s;
        float d;

        COMMENT("Test pb_dec_fixed using float (failures here may be caused by imperfect rounding)")
        TEST((S(&s,"\x00\x00\x00\x00"), pb_decode_fixed32(&s, &d) && d == 0.0f))
        TEST((S(&s,"\x00\x00\xc6\x42"), pb_decode_fixed32(&s, &d) && d == 99.0f))
        TEST((S(&s,"\x4e\x61\x3c\xcb"), pb_decode_fixed32(&s, &d) && d == -12345678.0f))
        d = -12345678.0f;
        TEST((S(&s,"\x00"), !pb_decode_fixed32(&s, &d) && d == -12345678.0f))
    }

    if (sizeof(double) == 8)
    {
        pb_decode_ctx_t s;
        double d;

        COMMENT("Test pb_dec_fixed64 using double (failures here may be caused by imperfect rounding)")
        TEST((S(&s,"\x00\x00\x00\x00\x00\x00\x00\x00"), pb_decode_fixed64(&s, &d) && d == 0.0))
        TEST((S(&s,"\x00\x00\x00\x00\x00\xc0\x58\x40"), pb_decode_fixed64(&s, &d) && d == 99.0))
        TEST((S(&s,"\x00\x00\x00\xc0\x29\x8c\x67\xc1"), pb_decode_fixed64(&s, &d) && d == -12345678.0f))
    }

    {
        pb_decode_ctx_t s;
        struct { pb_size_t size; uint8_t bytes[5]; } d;
        pb_field_iter_t f;

        f.type = PB_LTYPE_BYTES;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_bytes")
        TEST((S(&s,"\x00"), pb_dec_bytes(&s, &f) && d.size == 0))
        TEST((S(&s,"\x01\xFF"), pb_dec_bytes(&s, &f) && d.size == 1 && d.bytes[0] == 0xFF))
        TEST((S(&s,"\x05xxxxx"), pb_dec_bytes(&s, &f) && d.size == 5))
        TEST((S(&s,"\x05xxxx"), !pb_dec_bytes(&s, &f)))

        /* Note: the size limit on bytes-fields is not strictly obeyed, as
         * the compiler may add some padding to the struct. Using this padding
         * is not a very good thing to do, but it is difficult to avoid when
         * we use only a single uint8_t to store the size of the field.
         * Therefore this tests against a 10-byte string, while otherwise even
         * 6 bytes should error out.
         */
        TEST((S(&s,"\x10xxxxxxxxxx"), !pb_dec_bytes(&s, &f)))
    }

    {
        pb_decode_ctx_t s;
        pb_field_iter_t f;
        char d[5];

        f.type = PB_LTYPE_STRING;
        f.data_size = sizeof(d);
        f.pData = &d;

        COMMENT("Test pb_dec_string")
        TEST((S(&s,"\x00"), pb_dec_string(&s, &f) && d[0] == '\0'))
        TEST((S(&s,"\x04xyzz"), pb_dec_string(&s, &f) && strcmp(d, "xyzz") == 0))
        TEST((S(&s,"\x05xyzzy"), !pb_dec_string(&s, &f)))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode with repeated int32 field")
        TEST((S(&s,""), pb_decode(&s, IntegerArray_fields, &dest) && dest.data_count == 0))
        TEST((S(&s,"\x08\x01\x08\x02"), pb_decode(&s, IntegerArray_fields, &dest)
             && dest.data_count == 2 && dest.data[0] == 1 && dest.data[1] == 2))
        S(&s,"\x08\x01\x08\x02\x08\x03\x08\x04\x08\x05\x08\x06\x08\x07\x08\x08\x08\x09\x08\x0A");
        TEST(pb_decode(&s, IntegerArray_fields, &dest) && dest.data_count == 10 && dest.data[9] == 10)
        S(&s,"\x08\x01\x08\x02\x08\x03\x08\x04\x08\x05\x08\x06\x08\x07\x08\x08\x08\x09\x08\x0A\x08\x0B");
        TEST(!pb_decode(&s, IntegerArray_fields, &dest))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode with packed int32 field")
        TEST((S(&s,"\x0A\x00"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 0))
        TEST((S(&s,"\x0A\x01\x01"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 1 && dest.data[0] == 1))
        TEST((S(&s,"\x0A\x0A\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 10 && dest.data[0] == 1 && dest.data[9] == 10))
        TEST((S(&s,"\x0A\x0B\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B"), !pb_decode(&s, IntegerArray_fields, &dest)))

        /* Test invalid wire data */
        TEST((S(&s,"\x0A\xFF"), !pb_decode(&s, IntegerArray_fields, &dest)))
        TEST((S(&s,"\x0A\x01"), !pb_decode(&s, IntegerArray_fields, &dest)))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode with unknown fields")
        TEST((S(&s,"\x18\x0F\x08\x01"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 1 && dest.data[0] == 1))
        TEST((S(&s,"\x19\x00\x00\x00\x00\x00\x00\x00\x00\x08\x01"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 1 && dest.data[0] == 1))
        TEST((S(&s,"\x1A\x00\x08\x01"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 1 && dest.data[0] == 1))
        TEST((S(&s,"\x1B\x08\x01"), !pb_decode(&s, IntegerArray_fields, &dest)))
        TEST((S(&s,"\x1D\x00\x00\x00\x00\x08\x01"), pb_decode(&s, IntegerArray_fields, &dest)
            && dest.data_count == 1 && dest.data[0] == 1))
    }

    {
        pb_decode_ctx_t s;
        CallbackArray dest;
        struct { pb_size_t size; uint8_t bytes[10]; } ref;
        dest.data.funcs.decode = &callback_check;
        dest.data.arg = &ref;

        COMMENT("Testing pb_decode with callbacks")
        /* Single varint */
        ref.size = 1; ref.bytes[0] = 0x55;
        TEST((S(&s,"\x08\x55"), pb_decode(&s, CallbackArray_fields, &dest)))
        /* Packed varint */
        ref.size = 3; ref.bytes[0] = ref.bytes[1] = ref.bytes[2] = 0x55;
        TEST((S(&s,"\x0A\x03\x55\x55\x55"), pb_decode(&s, CallbackArray_fields, &dest)))
        /* Packed varint with loop */
        ref.size = 1; ref.bytes[0] = 0x55;
        TEST((S(&s,"\x0A\x03\x55\x55\x55"), pb_decode(&s, CallbackArray_fields, &dest)))
        /* Single fixed32 */
        ref.size = 4; ref.bytes[0] = ref.bytes[1] = ref.bytes[2] = ref.bytes[3] = 0xAA;
        TEST((S(&s,"\x0D\xAA\xAA\xAA\xAA"), pb_decode(&s, CallbackArray_fields, &dest)))
        /* Single fixed64 */
        ref.size = 8; memset(ref.bytes, 0xAA, 8);
        TEST((S(&s,"\x09\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"), pb_decode(&s, CallbackArray_fields, &dest)))
        /* Unsupported field type */
        TEST((S(&s,"\x0B\x00"), !pb_decode(&s, CallbackArray_fields, &dest)))

        /* Just make sure that our test function works */
        ref.size = 1; ref.bytes[0] = 0x56;
        TEST((S(&s,"\x08\x55"), !pb_decode(&s, CallbackArray_fields, &dest)))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode message termination")
        TEST((S(&s,""), pb_decode(&s, IntegerArray_fields, &dest)))
        TEST((S(&s,"\x08\x01"), pb_decode(&s, IntegerArray_fields, &dest)))
        TEST((S(&s,"\x08"), !pb_decode(&s, IntegerArray_fields, &dest)))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode_ex null termination")

        S(&s,"\x00");
        s.flags |= PB_DECODE_CTX_FLAG_NULLTERMINATED;
        TEST(pb_decode(&s, IntegerArray_fields, &dest))

        S(&s,"\x08\x01\x00");
        s.flags |= PB_DECODE_CTX_FLAG_NULLTERMINATED;
        TEST(pb_decode(&s, IntegerArray_fields, &dest))
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing pb_decode with invalid tag numbers")
        TEST((S(&s,"\x9f\xea"), !pb_decode(&s, IntegerArray_fields, &dest)));
        TEST((S(&s,"\x00"), !pb_decode(&s, IntegerArray_fields, &dest)));
    }

    {
        pb_decode_ctx_t s;
        IntegerArray dest;

        COMMENT("Testing wrong message type detection")
        TEST((S(&s,"\x0A\x07\x0A\x05\x01\x02\x03\x04\x05"), !pb_decode(&s, CallbackArray_fields, &dest)));
        TEST(strcmp(s.errmsg, "struct_size mismatch") == 0);
    }

    {
        pb_decode_ctx_t s;
        IntegerContainer dest = {{0}};

        COMMENT("Testing pb_decode_delimited")
        S(&s,"\x09\x0A\x07\x0A\x05\x01\x02\x03\x04\x05");
        s.flags |= PB_DECODE_CTX_FLAG_DELIMITED;
        TEST(pb_decode(&s, IntegerContainer_fields, &dest) && dest.submsg.data_count == 5)
    }

    {
        pb_decode_ctx_t s = {0};
        void *data = NULL;

        COMMENT("Testing pb_allocate_field")
        TEST(pb_allocate_field(&s, &data, 10, 10) && data != NULL);
        TEST(pb_allocate_field(&s, &data, 10, 20) && data != NULL);

        {
            void *oldvalue = data;
            size_t very_big = (size_t)-1;
            size_t somewhat_big = very_big / 2 + 1;
            size_t not_so_big = (size_t)1 << (4 * sizeof(size_t));

            TEST(!pb_allocate_field(&s, &data, very_big, 2) && data == oldvalue);
            TEST(!pb_allocate_field(&s, &data, somewhat_big, 2) && data == oldvalue);
            TEST(!pb_allocate_field(&s, &data, not_so_big, not_so_big) && data == oldvalue);
        }

        pb_release_field(&s, data);
    }

    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
