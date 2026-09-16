/* C++ half of the mixed language test. Built as C++11, so it uses the
 * "enum Foo : uint8_t" branch of the generated header.
 */

#include <stddef.h>
#include "enum_intsize_mixed.h"

extern "C" int check_from_cpp(const IntSizeMessage *msg, unsigned c_sizeof_msg, unsigned c_offset_d)
{
    if (sizeof(IntSizeInt8) != 1) return 1;
    if (sizeof(IntSizeInt16) != 2) return 2;
    if (sizeof(IntSizeInt32) != 4) return 3;
    if (sizeof(IntSizeInt64) != 8) return 4;

    /* The struct must look the same as it does to the C compiler */
    if (sizeof(IntSizeMessage) != c_sizeof_msg) return 5;
    if (offsetof(IntSizeMessage, d) != c_offset_d) return 6;

    /* And the values written by the C side must arrive unchanged */
    if (msg->a != I8_B) return 7;
    if (msg->b != I16_B) return 8;
    if (msg->c != I32_B) return 9;
    if (msg->d != I64_B) return 10;

    return 0;
}
