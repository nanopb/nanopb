#include <stdio.h>
#include <pb_decode.h>
#include "person.h"

/* This test has only one source file anyway.. */
#include "person.c"

bool print_person(pb_istream_t *stream)
{
    int i;
    Person person;
    
    if (!pb_decode(stream, Person_fields, &person))
        return false;
    
    printf("Person: name '%s' id '%d' email '%s'\n", person.name, person.id, person.email);
    
    for (i = 0; i < person.phone_count; i++)
    {
        Person_PhoneNumber *phone = &person.phone[i];
        printf("PhoneNumber: number '%s' type '%d'\n", phone->number, phone->type);
    }
    
    return true;
}

bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    FILE *file = (FILE*)stream->state;
    bool status;
    
    if (buf == NULL)
    {
        while (count-- && fgetc(file) != EOF);
        return count == 0;
    }
    
    status = (fread(buf, 1, count, file) == count);
    
    if (feof(file))
        stream->bytes_left = 0;
    
    return status;
}

int main()
{
    pb_istream_t stream = {&callback, stdin, SIZE_MAX};
    if (!print_person(&stream))
        printf("Parsing failed.\n");
    
    return 0;
}
