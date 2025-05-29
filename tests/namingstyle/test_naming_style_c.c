#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "unittests.h"
#include "naming_style.pb.h"

int main()
{
    int status = 0;
    main_message_t defaultMessage = MAIN_MESSAGE_INIT_DEFAULT;
    main_message_t message = MAIN_MESSAGE_INIT_ZERO;

    /* Verify the default value was initialized */
    TEST(defaultMessage.my_enum5 == MY_ENUM1_ENTRY_SECOND);

    /* Verify that all members have the expected names */
    message.lucky_number = 13;
    message.required_number = 1;
    message.repeated_number[0] = 1;
    message.repeated_ints = NULL;

    message.my_enum1 = MY_ENUM1_ENUM_THIRD;
    message.my_enum2 = MY_ENUM2_ENUM2_ENTRY;
    message.my_enum3 = MY_ENUM2_ENUM2_ENTRY;
    message.my_enum4.arg = NULL;

    message.string_values1 = NULL;
    message.string_values2[0][0] = 'a';
    message.optional_string.arg = NULL;
    message.required_string[0] = 'a';

    message.repeated_fixed32[0] = 1;

    message.required_bytes1[0] = 0;
    message.required_bytes2.size = 0;
    message.repeated_bytes1_count = 0;
    message.repeated_bytes2 = NULL;

    message.has_sub_message1 = true;
    message.sub_message1.has_test_value = true;
    message.sub_message1.test_value = 0;
    message.sub_message2.arg = NULL;
    message.sub_message3.test_value = 0;

    message.which_one_of_name = MAIN_MESSAGE_TEST_MESSAGE2_TAG;
    union main_message_one_of_name one_of_name;
    one_of_name.test_message2.has_test_value = true;
    one_of_name.test_message2.test_value = 5;
    message.one_of_name = one_of_name;

    message.which_one_of_name2 = MAIN_MESSAGE_TEST_MESSAGE5_TAG;
    message.test_message5.test_value = 5;

    TEST(strcmp("ENTRY_FIRST", my_enum1_name(MY_ENUM1_ENTRY_FIRST)) == 0);
    TEST(my_enum1_valid(MY_ENUM1_ENTRY_FIRST) == true);
    TEST(my_enum2_valid(MY_ENUM2_ENUM2_ENTRY) == true);

    /* Verify that the descriptor structure is at least mostly correct
     * by doing a round-trip encoding test.
     */
    {
        uint8_t buffer1[256];
        uint8_t buffer2[256];
        pb_ostream_t ostream1 = pb_ostream_from_buffer(buffer1, sizeof(buffer1));
        pb_ostream_t ostream2 = pb_ostream_from_buffer(buffer2, sizeof(buffer2));
        pb_istream_t istream;
        main_message_t message2 = MAIN_MESSAGE_INIT_ZERO;

        TEST(pb_encode(&ostream1, &main_message_t_msg, &message));

        istream = pb_istream_from_buffer(buffer1, ostream1.bytes_written);
        TEST(pb_decode(&istream, &main_message_t_msg, &message2));

        /* Encoding a second time should produce same output */
        TEST(pb_encode(&ostream2, &main_message_t_msg, &message2));

        TEST(ostream2.bytes_written == ostream1.bytes_written);
        TEST(memcmp(buffer1, buffer2, ostream1.bytes_written) == 0);
    }

    return status;
}
