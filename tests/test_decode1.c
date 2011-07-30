#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "pb_decode.h"

/* Structures for "Person" message */

typedef enum {
    Person_PhoneType_MOBILE = 0,
    Person_PhoneType_HOME = 1,
    Person_PhoneType_WORK = 2
} Person_PhoneType;

typedef struct {
    char number[40];
    bool has_type;
    Person_PhoneType type;
} Person_PhoneNumber;

typedef struct {
    char name[40];
    int32_t id;
    bool has_email;
    char email[40];
    size_t phone_size;
    Person_PhoneNumber phone[5];
} Person;

/* Field descriptions */


const Person_PhoneType Person_PhoneNumber_type_default = Person_PhoneType_HOME;

const pb_field_t Person_PhoneNumber_fields[] = {
    {1, PB_HTYPE_REQUIRED | PB_LTYPE_STRING,
        offsetof(Person_PhoneNumber, number), 0,
        pb_membersize(Person_PhoneNumber, number), 0, 0},
        
    {2, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
        pb_delta(Person_PhoneNumber, type, number),
        pb_delta(Person_PhoneNumber, has_type, type),
        pb_membersize(Person_PhoneNumber, type), 0,
        &Person_PhoneNumber_type_default},
    
    PB_LAST_FIELD
};

const pb_field_t Person_fields[] = {
    {1, PB_HTYPE_REQUIRED | PB_LTYPE_STRING,
        offsetof(Person, name), 0,
        pb_membersize(Person, name), 0, 0},
    
    {2, PB_HTYPE_REQUIRED | PB_LTYPE_VARINT,
        pb_delta(Person, id, name), 0,
        pb_membersize(Person, id), 0, 0},
    
    {3, PB_HTYPE_OPTIONAL | PB_LTYPE_STRING,
        offsetof(Person, email) - offsetof(Person, id),
        pb_delta(Person, has_email, email),
        pb_membersize(Person, email), 0, 0},
    
    {4, PB_HTYPE_ARRAY | PB_LTYPE_SUBMESSAGE,
        offsetof(Person, phone) - offsetof(Person, email),
        pb_delta(Person, phone_size, phone),
        pb_membersize(Person, phone[0]),
        pb_arraysize(Person, phone),
        Person_PhoneNumber_fields},
    
    PB_LAST_FIELD
};

/* And now, the actual test program */

bool print_person(pb_istream_t *stream)
{
    int i;
    Person person;
    
    if (!pb_decode(stream, Person_fields, &person))
        return false;
    
    printf("Person: name '%s' id '%d' email '%s'\n", person.name, person.id, person.email);
    
    for (i = 0; i < person.phone_size; i++)
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
