// Example of decoding a protobuf message in 'delimited' format.
// The message length is indicated by a length prefix.

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "print_person.h"

int main()
{
    pb_decode_ctx_t stream;
    pb_byte_t tmpbuf[16];
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, tmpbuf, sizeof(tmpbuf));
    
    stream.flags |= PB_ENCODE_CTX_FLAG_DELIMITED;

    // Decode as many messages as come in
    while (true)
    {
        Person person = Person_init_zero;
        
        // Set maximum size per message
        stream.bytes_left = Person_size;

        if (!pb_decode(&stream, Person_fields, &person))
        {
            if (stream.bytes_left == 0)
            {
                // End of stream
                break;
            }

            printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }

        print_person(&person);

        printf("--------\n");
    }
    
    return 0;
}
