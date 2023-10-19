/*
 * Tests if expected names are generated when M_STRIP_PACKAGE is used in one of the files.
 */

#include <stdio.h>
#include "unittests.h"
#include "replace_package_b.pb.h"

int main()
{
    MessageA msgA1 = package_a_MessageA_init_default;
    package_a_MessageA msgA2 = MessageA_init_default;

    package_b_MessageB msgB1 = ReplacedName_MessageB_init_zero;
    ReplacedName_MessageB msgB2 = package_b_MessageB_init_zero;
    
    package_a_EnumA e1 = EnumA_VALUE_A_0;
    EnumA e2 = EnumA_VALUE_A_1;
    e2 = _package_a_EnumA_MIN;
    e2 = _EnumA_MIN;
    e2 = _package_a_EnumA_MAX;
    e2 = _EnumA_MAX;
    e2 = _package_a_EnumA_ARRAYSIZE;
    e2 = _EnumA_ARRAYSIZE;

    return msgA1.enum_a_field + msgA2.enum_a_field + msgB1.nested_enum + msgB2.nested_enum + e1 + e2; /* marks variables as used */
}
