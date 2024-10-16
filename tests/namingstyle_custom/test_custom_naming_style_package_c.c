#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "unittests.h"
#include "custom_naming_style_package.pb.h"

int main()
{
    int status = 0;
    PackageMainMessage defaultMessage = PACKAGE_MAIN_MESSAGE_INIT_DEFAULT;
    PackageMainMessage message = PACKAGE_MAIN_MESSAGE_INIT_ZERO;

    /* Verify the default value was initialized */
    TEST(defaultMessage.myEnum5 == PACKAGE_MY_ENUM1_ENTRY_SECOND);

    /* Verify that all members have the expected names */
    message.luckyNumber = 13;
    message.requiredNumber = 1;
    message.repeatedNumber[0] = 1;
    message.repeatedInts = NULL;

    message.myEnum1 = PACKAGE_MY_ENUM1_ENUM_THIRD;
    message.myEnum2 = PACKAGE_MY_ENUM2_ENUM2_ENTRY;
    message.myEnum3 = PACKAGE_MY_ENUM2_ENUM2_ENTRY;
    message.myEnum4.arg = NULL;

    message.stringValues1 = NULL;
    message.stringValues2[0][0] = 'a';
    message.optionalString.arg = NULL;
    message.requiredString[0] = 'a';

    message.repeatedFixed32[0] = 1;

    message.requiredBytes1[0] = 0;
    message.requiredBytes2.size = 0;
    message.repeatedBytes1_count = 0;
    message.repeatedBytes2 = NULL;

    message.has_subMessage1 = true;
    message.subMessage1.has_testValue = true;
    message.subMessage1.testValue = 0;
    message.subMessage2.arg = NULL;
    message.subMessage3.testValue = 0;

    message.which_oneOfName = PACKAGE_MAIN_MESSAGE_TEST_MESSAGE2_TAG;
    message.oneOfName.testMessage2.has_testValue = true;
    message.oneOfName.testMessage2.testValue = 5;

    message.which_oneOfName2 = PACKAGE_MAIN_MESSAGE_TEST_MESSAGE5_TAG;
    message.testMessage5.testValue = 5;

    TEST(strcmp("ENTRY_FIRST", PackageMyEnum1Name(PACKAGE_MY_ENUM1_ENTRY_FIRST)) == 0);
    TEST(PackageMyEnum1Valid(PACKAGE_MY_ENUM1_ENTRY_FIRST) == true);
    TEST(PackageMyEnum2Valid(PACKAGE_MY_ENUM2_ENUM2_ENTRY) == true);

    /* Verify that the descriptor structure is at least mostly correct
     * by doing a round-trip encoding test.
     */
    {
        uint8_t buffer1[256];
        uint8_t buffer2[256];
        pb_ostream_t ostream1 = pb_ostream_from_buffer(buffer1, sizeof(buffer1));
        pb_ostream_t ostream2 = pb_ostream_from_buffer(buffer2, sizeof(buffer2));
        pb_istream_t istream;
        PackageMainMessage message2 = PACKAGE_MAIN_MESSAGE_INIT_ZERO;

        TEST(pb_encode(&ostream1, &PackageMainMessage_msg, &message));

        istream = pb_istream_from_buffer(buffer1, ostream1.bytes_written);
        TEST(pb_decode(&istream, &PackageMainMessage_msg, &message2));

        /* Encoding a second time should produce same output */
        TEST(pb_encode(&ostream2, &PackageMainMessage_msg, &message2));

        TEST(ostream2.bytes_written == ostream1.bytes_written);
        TEST(memcmp(buffer1, buffer2, ostream1.bytes_written) == 0);
    }

    return status;
}
