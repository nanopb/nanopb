/* Same as test_decode1 but reads from stdin directly.
 */

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "print_person.h"

int main()
{
    pb_decode_ctx_t stream;
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, NULL, 0);

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
