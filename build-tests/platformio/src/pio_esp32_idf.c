#include "nanopb/pb_encode.h"
#include "nanopb/pb_decode.h"

#include "test.h"

#include "pio_without_options.pb.h"

void app_main() {
    int status = 0;

    uint8_t buffer[256];
    pb_encode_ctx_t ostream;
    pb_decode_ctx_t istream;
    size_t written;

    TestMessageWithoutOptions original = TestMessageWithoutOptions_init_zero;
    original.number = 45;

    pb_init_encode_ctx_for_buffer(&ostream, buffer, sizeof(buffer));

    TEST(pb_encode(&ostream, &TestMessageWithoutOptions_msg, &original));

    written = ostream.bytes_written;

    pb_init_decode_ctx_for_buffer(&istream, buffer, written);

    TestMessageWithoutOptions decoded = TestMessageWithoutOptions_init_zero;

    TEST(pb_decode(&istream, &TestMessageWithoutOptions_msg, &decoded));

    TEST(decoded.number == 45);
}
