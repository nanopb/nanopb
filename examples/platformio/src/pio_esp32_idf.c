#include "pb_encode.h"
#include "pb_decode.h"

#include "test.h"

#include "pio_without_options.pb.h"

void app_main() {
    int status = 0;

    uint8_t buffer[256];
    pb_ostream_t ostream;
    pb_istream_t istream;
    size_t written;

    TestMessageWithoutOptions original = TestMessageWithoutOptions_init_zero;
    original.number = 45;

    ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    TEST(pb_encode(&ostream, &TestMessageWithoutOptions_msg, &original));

    written = ostream.bytes_written;

    istream = pb_istream_from_buffer(buffer, written);

    TestMessageWithoutOptions decoded = TestMessageWithoutOptions_init_zero;

    TEST(pb_decode(&istream, &TestMessageWithoutOptions_msg, &decoded));

    TEST(decoded.number == 45);
}
