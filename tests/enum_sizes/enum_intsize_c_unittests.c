/* Test that the enum_intsize option produces the requested enum size also
 * when compiling as C. C compilers older than C23 take the integer typedef
 * branch of the generated header, which has to give the same size and
 * signedness as the "enum Foo : uint8_t" branch used by C++.
 */

#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "enum_intsize.pb.h"
#include "unittests.h"

int main()
{
    int status = 0;

    TEST(sizeof(IntSizeInt8) == sizeof(uint8_t));
    TEST(sizeof(IntSizeInt16) == sizeof(uint16_t));
    TEST(sizeof(IntSizeInt32) == sizeof(uint32_t));
    TEST(sizeof(IntSizeInt64) == sizeof(uint64_t));

    /* The enum is unsigned in both branches of the generated header */
    TEST((IntSizeInt8)-1 > 0);

    /* The in-memory size must not affect the encoded message */
    {
        uint8_t buffer[64];
        pb_ostream_t ostream;
        pb_istream_t istream;
        IntSizeMessage msg = IntSizeMessage_init_zero;
        IntSizeMessage msg2 = IntSizeMessage_init_zero;

        msg.a = I8_B;
        msg.b = I16_B;
        msg.c = I32_B;
        msg.d = I64_B;

        ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        TEST(pb_encode(&ostream, IntSizeMessage_fields, &msg));
        TEST(ostream.bytes_written == 8);

        istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
        TEST(pb_decode(&istream, IntSizeMessage_fields, &msg2));
        TEST(msg2.a == I8_B && msg2.b == I16_B && msg2.c == I32_B && msg2.d == I64_B);
    }

    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
