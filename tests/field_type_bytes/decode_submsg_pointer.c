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

    msglen = fread(buffer, 1, sizeof(buffer), stdin);

    COMMENT("Test: Decode into OuterPointer (FT_BYTES with FT_POINTER, malloc)")
    {
        OuterPointer msg = OuterPointer_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);

        TEST(pb_decode(&stream, OuterPointer_fields, &msg));
        TEST(msg.sub != NULL);
        TEST(msg.sub->size > 0);
        TEST(verify_inner(msg.sub->bytes, msg.sub->size));

        pb_release(OuterPointer_fields, &msg);
        TEST(msg.sub == NULL);
    }

    if (status != 0)
        fprintf(stderr, "Some tests FAILED!\n");

    return status;
}
