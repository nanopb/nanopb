/* Just test that the file can be compiled successfully. */

#include "messages2.pb.h"

int main()
{
    PB_STATIC_ASSERT(MESSAGES2_PB_H_MAX_SIZE == xmit_size, INCORRECT_MAX_SIZE);
    // Computed 13 bytes by waiting for compilation error.
    // Purpose is to check that if a message has a submessage callback,
    // the size of the message can still be computed.
    PB_STATIC_ASSERT(13 == WithCallbackSubmsg_size, INCORRECT_MAX_SIZE);
    // Computed 13 bytes by waiting for compilation error.
    // Purpose is to check that if a message has a type FT_POINTER,
    // the size of the message can still be computed.
    PB_STATIC_ASSERT(13 == WithPointerSubmsg_size, INCORRECT_MAX_SIZE);
    return xmit_size;
}