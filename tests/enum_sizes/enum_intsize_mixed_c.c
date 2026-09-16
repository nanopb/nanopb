/* C half of the mixed language test. Built as C, so it uses the integer
 * typedef branch of the generated header unless the compiler supports C23.
 */

#include <stdio.h>
#include <stddef.h>
#include "enum_intsize_mixed.h"
#include "unittests.h"

int main()
{
    int status = 0;
    IntSizeMessage msg = IntSizeMessage_init_zero;

    TEST(sizeof(IntSizeInt8) == 1);
    TEST(sizeof(IntSizeInt16) == 2);
    TEST(sizeof(IntSizeInt32) == 4);
    TEST(sizeof(IntSizeInt64) == 8);

    msg.a = I8_B;
    msg.b = I16_B;
    msg.c = I32_B;
    msg.d = I64_B;

    TEST(check_from_cpp(&msg, (unsigned)sizeof(IntSizeMessage),
                        (unsigned)offsetof(IntSizeMessage, d)) == 0);

    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
