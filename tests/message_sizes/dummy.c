/* Just test that the file can be compiled successfully. */

#include "messages2.pb.h"

int main()
{
    PB_STATIC_ASSERT(MESSAGES2_PB_H_MAX_SIZE == xmit_size, INCORRECT_MAX_SIZE);
    return xmit_size;
}

