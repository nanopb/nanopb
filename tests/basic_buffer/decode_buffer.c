/* A very simple decoding test case, using person.proto.
 * Produces output compatible with protoc --decode.
 * Reads the encoded data from stdin and prints the values
 * to stdout as text.
 *
 * Run e.g. ./test_encode1 | ./test_decode1
 */

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "print_person.h"
#include "test_helpers.h"

int main()
{
    uint8_t buffer[Person_size];
    pb_decode_ctx_t stream;
    size_t count;
    
    /* Read the data into buffer */
    SET_BINARY_MODE(stdin);
    count = fread(buffer, 1, sizeof(buffer), stdin);
    
    if (!feof(stdin))
    {
    	printf("Message does not fit in buffer\n");
    	return 1;
    }
    
    /* Construct a pb_istream_t for reading from the buffer */
    pb_init_decode_ctx_for_buffer(&stream, buffer, count);
    
    /* Decode and print out the stuff */
    Person person = Person_init_zero;
    if (!pb_decode(&stream, Person_fields, &person))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    print_person(&person);
    return 0;
}
