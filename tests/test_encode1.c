#include <stdio.h>
#include <pb_encode.h>
#include "person.h"

/* This test has only one source file anyway.. */
#include "person.c"

bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
}

int main()
{
    Person person = {"Test Person 99", 99, true, "test@person.com",
        1, {{"555-12345678", true, Person_PhoneType_MOBILE}}};
    
    pb_ostream_t stream = {&streamcallback, stdout, SIZE_MAX, 0};
    
    pb_encode(&stream, Person_fields, &person);
    
    return 0;
}
