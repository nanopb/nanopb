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

int main()
{
    uint8_t buffer[512];
    size_t size = fread(buffer, 1, 512, stdin);
    
    pb_istream_t stream = pb_istream_from_buffer(buffer, size);
    if (!print_person(&stream))
        printf("Parsing failed.\n");
    
    return 0;
}
