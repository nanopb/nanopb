#include <stdio.h>
#include "unittests.h"
#include "enum.pb.h"

int main()
{
    int status = 0;
    TEST(MyEnum_valid(MyEnum_VALUE1) == true);
    TEST(MyEnum_valid(MyEnum_VALUE2) == true);
    TEST(MyEnum_valid(MyEnum_VALUE15) == true);
    TEST(MyShortNameEnum_valid(MSNE_VALUE256) == true);
    TEST(MyShortNameEnum_valid(9999) == false);
    
    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}

