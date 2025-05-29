#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "unittests.h"
#include "custom_naming_style_other.pb.h"
#include "custom_naming_style_importer.pb.h"

int main()
{
    int status = 0;
    MainMessage defaultMessage = MAIN_MESSAGE_INIT_DEFAULT;

    TEST(defaultMessage.myOwnEnum == MY_ENUM_MY_ENUM_NONE);
    TEST(defaultMessage.importedEnum == THE_OTHER_ENUM_OTHER_UNSET);
    TEST(defaultMessage.anotherImportedEnum == ANOTHER_ENUM_ANOTHER_UNSET);

    return status;
}