#include "initializertest.pb.h"

int main()
{
    TestMessage msg1 = TestMessage_init_zero;
    TestMessage msg2 = TestMessage_init_default;
    return msg1.field1 + msg2.field1; /* Mark variables as used for compiler */
}
