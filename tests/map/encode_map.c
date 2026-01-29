/* Encode a message using map field */

#include <stdio.h>
#include <stdlib.h>
#include <pb_encode.h>
#include "map.pb.h"
#include "test_helpers.h"

int main(int argc, char **argv)
{
    uint8_t buffer[MyMessage_size];
    MyMessage msg = MyMessage_init_zero;
    pb_encode_ctx_t stream;

    /* Fill in the map entries */
    msg.numbers_count = 3;
    strncpy(msg.numbers[0].key, "one", sizeof(msg.numbers[0].key));
    strncpy(msg.numbers[1].key, "two", sizeof(msg.numbers[1].key));
    strncpy(msg.numbers[2].key, "seven", sizeof(msg.numbers[2].key));
    msg.numbers[0].value = 1;
    msg.numbers[1].value = 2;
    msg.numbers[2].value = 7;

    pb_init_encode_ctx_for_buffer(&stream, buffer, sizeof(buffer));
    
    if (pb_encode(&stream, MyMessage_fields, &msg))
    {
        SET_BINARY_MODE(stdout);
        fwrite(buffer, 1, stream.bytes_written, stdout);
        return 0;
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }
}
