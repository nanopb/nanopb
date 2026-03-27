#include <stdio.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "submsg_as_bytes.pb.h"
#include "unittests.h"
#include "test_helpers.h"

/* Re-decode raw bytes as InnerMessage to verify content */
static bool verify_inner(const uint8_t *bytes, size_t len)
{
    InnerMessage inner = InnerMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(bytes, len);
    if (!pb_decode(&stream, InnerMessage_fields, &inner))
        return false;
    if (inner.value != 42)
        return false;
    if (strcmp(inner.name, "hello") != 0)
        return false;
    return true;
}

int main(int argc, char *argv[])
{
    int status = 0;
    uint8_t buffer[256];
    size_t msglen;

    SET_BINARY_MODE(stdin);

    /* Read encoded data from stdin */
    msglen = fread(buffer, 1, sizeof(buffer), stdin);

    COMMENT("Test 1: Decode into OuterStatic (FT_BYTES with max_size, inline option)")
    {
        OuterStatic msg = OuterStatic_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);

        TEST(pb_decode(&stream, OuterStatic_fields, &msg));
        TEST(msg.sub.size > 0);
        TEST(verify_inner(msg.sub.bytes, msg.sub.size));
    }

    COMMENT("Test 2: Decode into OuterOptions (FT_BYTES with max_size, .options file)")
    {
        OuterOptions msg = OuterOptions_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);

        TEST(pb_decode(&stream, OuterOptions_fields, &msg));
        TEST(msg.sub.size > 0);
        TEST(verify_inner(msg.sub.bytes, msg.sub.size));
    }

    COMMENT("Test 3: Round-trip - encode as bytes, decode as normal submessage")
    {
        /* First decode the wire data into OuterStatic (bytes form) */
        OuterStatic bytes_msg = OuterStatic_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);
        TEST(pb_decode(&stream, OuterStatic_fields, &bytes_msg));

        /* Now re-encode OuterStatic to new wire data */
        uint8_t rebuffer[256];
        pb_ostream_t ostream = pb_ostream_from_buffer(rebuffer, sizeof(rebuffer));
        TEST(pb_encode(&ostream, OuterStatic_fields, &bytes_msg));

        /* Decode re-encoded data as OuterNormal (normal submessage) */
        OuterNormal normal_msg = OuterNormal_init_zero;
        pb_istream_t istream2 = pb_istream_from_buffer(rebuffer, ostream.bytes_written);
        TEST(pb_decode(&istream2, OuterNormal_fields, &normal_msg));
        TEST(normal_msg.sub.value == 42);
        TEST(strcmp(normal_msg.sub.name, "hello") == 0);
    }

    COMMENT("Test 4: Repeated submessage as bytes")
    {
        uint8_t repbuffer[256];
        size_t replen;

        {
            pb_ostream_t ostream = pb_ostream_from_buffer(repbuffer, sizeof(repbuffer));

            InnerMessage inner1 = InnerMessage_init_zero;
            inner1.value = 10;
            strcpy(inner1.name, "first");

            InnerMessage inner2 = InnerMessage_init_zero;
            inner2.value = 20;
            strcpy(inner2.name, "second");

            /* tag 1, wire type 2 (length-delimited) for each repeated entry */
            TEST(pb_encode_tag(&ostream, PB_WT_STRING, 1));
            TEST(pb_encode_submessage(&ostream, InnerMessage_fields, &inner1));
            TEST(pb_encode_tag(&ostream, PB_WT_STRING, 1));
            TEST(pb_encode_submessage(&ostream, InnerMessage_fields, &inner2));
            replen = ostream.bytes_written;
        }

        {
            OuterRepeated msg = OuterRepeated_init_zero;
            pb_istream_t istream = pb_istream_from_buffer(repbuffer, replen);
            TEST(pb_decode(&istream, OuterRepeated_fields, &msg));
            TEST(msg.subs_count == 2);

            /* Verify each repeated entry contains a valid serialized InnerMessage */
            {
                InnerMessage inner = InnerMessage_init_zero;
                pb_istream_t s = pb_istream_from_buffer(msg.subs[0].bytes, msg.subs[0].size);
                TEST(pb_decode(&s, InnerMessage_fields, &inner));
                TEST(inner.value == 10);
                TEST(strcmp(inner.name, "first") == 0);
            }
            {
                InnerMessage inner = InnerMessage_init_zero;
                pb_istream_t s = pb_istream_from_buffer(msg.subs[1].bytes, msg.subs[1].size);
                TEST(pb_decode(&s, InnerMessage_fields, &inner));
                TEST(inner.value == 20);
                TEST(strcmp(inner.name, "second") == 0);
            }
        }
    }

    if (status != 0)
        fprintf(stderr, "Some tests FAILED!\n");

    return status;
}
