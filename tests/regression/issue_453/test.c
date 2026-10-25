#include "test.pb.h"
#include "unittests.h"

int main()
{
    int status = 0;
    MyMessage msg = MyMessage_init_default;
    TEST(msg.myfield >= 1.23399f);
    TEST(msg.myfield <= 1.23401f);
    TEST(msg.myfield2 == 1000000.0f);
    TEST(msg.myfield3 >= 0.99999e-5f);
    TEST(msg.myfield3 <= 1.00001e-5f);
    return status;
}
