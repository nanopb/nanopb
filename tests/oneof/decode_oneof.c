/* Decode a message using oneof fields */

#include <stdio.h>
#include <stdlib.h>
#include <pb_decode.h>
#include "oneof.pb.h"
#include "test_helpers.h"
#include "unittests.h"

int main(int argc, char **argv)
{
    uint8_t buffer[OneOfMessage_size];
    OneOfMessage msg = OneOfMessage_init_zero;
    pb_istream_t stream;
    size_t count;
    int option;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: encode_oneof [number]\n");
        return 1;
    }
    option = atoi(argv[1]);

    SET_BINARY_MODE(stdin);
    count = fread(buffer, 1, sizeof(buffer), stdin);

    if (!feof(stdin))
    {
        printf("Message does not fit in buffer\n");
        return 1;
    }

    stream = pb_istream_from_buffer(buffer, count);

    if (!pb_decode(&stream, OneOfMessage_fields, &msg))
    {
        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    {
        int status = 0;

        /* Check that the basic fields work normally */
        TEST(msg.prefix == 123);
        TEST(msg.suffix == 321);

        /* Check that we got the right oneof according to command line */
        if (option == 1)
        {
            TEST(msg.which_values == OneOfMessage_first_tag);
            TEST(msg.values.first == 999);
        }
        else if (option == 2)
        {
            TEST(msg.which_values == OneOfMessage_second_tag);
            TEST(strcmp(msg.values.second, "abcd") == 0);
        }
        else if (option == 3)
        {
            TEST(msg.which_values == OneOfMessage_third_tag);
            TEST(msg.values.third.array[0] == 1);
            TEST(msg.values.third.array[1] == 2);
            TEST(msg.values.third.array[2] == 3);
            TEST(msg.values.third.array[3] == 4);
            TEST(msg.values.third.array[4] == 5);
        }

        return status;
    }
}