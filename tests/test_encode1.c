/* A very simple encoding test case using person.proto.
 * Just puts constant data in the fields and writes the
 * data to stdout.
 */

#include <stdio.h>
#include <pb_encode.h>
#include "person.pb.h"

/* This binds the pb_ostream_t into the stdout stream */
bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
}

int main()
{
    /* Initialize the structure with constants */
    Person person = {"Test Person 99", 99, true, "test@person.com",
        1, {{"555-12345678", true, Person_PhoneType_MOBILE}}};
    
    /* Prepare the stream, output goes directly to stdout */
    pb_ostream_t stream = {&streamcallback, stdout, SIZE_MAX, 0};
    
    /* Now encode it and check if we succeeded. */
    if (pb_encode(&stream, Person_fields, &person))
        return 0; /* Success */
    else
        return 1; /* Failure */
}
