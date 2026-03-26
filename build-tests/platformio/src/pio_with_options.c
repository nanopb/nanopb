#include "nanopb/pb_encode.h"
#include "nanopb/pb_decode.h"

#include "test.h"

#include "pio_with_options.pb.h"

int main(int argc, char *argv[]) {

    int status = 0;

    uint8_t buffer[256];
    pb_encode_ctx_t ostream;
    pb_decode_ctx_t istream;
    size_t written;

    TestMessageWithOptions original = TestMessageWithOptions_init_zero;
    strcpy(original.str,"Hello");

    pb_init_encode_ctx_for_buffer(&ostream, buffer, sizeof(buffer));

    TEST(pb_encode(&ostream, &TestMessageWithOptions_msg, &original));

    written = ostream.bytes_written;

    pb_init_decode_ctx_for_buffer(&istream, buffer, written);

    TestMessageWithOptions decoded = TestMessageWithOptions_init_zero;

    TEST(pb_decode(&istream, &TestMessageWithOptions_msg, &decoded));

    TEST(strcmp(decoded.str,"Hello") == 0);

    return status;
}
