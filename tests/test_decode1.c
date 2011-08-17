#include <stdio.h>
#include <pb_decode.h>
#include "person.h"

bool print_person(pb_istream_t *stream)
{
    int i;
    Person person;
    
    if (!pb_decode(stream, Person_fields, &person))
        return false;
    
    printf("name: \"%s\"\n", person.name);
    printf("id: %d\n", person.id);
    
    if (person.has_email)
        printf("email: \"%s\"\n", person.email);
    
    for (i = 0; i < person.phone_count; i++)
    {
        Person_PhoneNumber *phone = &person.phone[i];
        printf("phone {\n");
        printf("  number: \"%s\"\n", phone->number);
        
        switch (phone->type)
        {
            case Person_PhoneType_WORK:
                printf("  type: WORK\n");
                break;
            
            case Person_PhoneType_HOME:
                printf("  type: HOME\n");
                break;
            
            case Person_PhoneType_MOBILE:
                printf("  type: MOBILE\n");
                break;
        }
        printf("}\n");
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
