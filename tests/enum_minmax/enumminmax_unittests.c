#include "unittests.h"
#include "enumminmax.pb.h"

int main()
{
    int status = 0;

    COMMENT("Verify min/max on unsorted enum");
    {
        TEST(ENUM_Language_MIN == Language_UNKNOWN);
        TEST(ENUM_Language_MAX == Language_SPANISH_ES_MX);
        TEST(ENUM_Language_ARRAYSIZE == (Language_SPANISH_ES_MX+1));
    }

    return status;
}
