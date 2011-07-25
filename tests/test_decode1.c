#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "pb_decode.h"

/* Structures for "Person" message */

typedef enum {
    Person_PhoneType_MOBILE = 0,
    Person_PhoneType_HOME = 1,
    Person_PhoneType_WORK = 2,
    
    _Person_PhoneType_size = 0xFFFFFFFF // Force 32-bit enum
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

    pb_callback_t phone;
} Person;

/* Field descriptions */
#define membersize(st, m) (sizeof ((st*)0)->m)

const pb_field_t Person_PhoneNumber_fields[] = {
    {1, offsetof(Person_PhoneNumber, number), PB_ACT_STRING, membersize(Person_PhoneNumber, number)},
    {2, offsetof(Person_PhoneNumber, has_type), PB_ACT_HAS, membersize(Person_PhoneNumber, has_type)},
    {2, offsetof(Person_PhoneNumber, type), PB_ACT_UINT32, membersize(Person_PhoneNumber, type)},
    PB_LAST_FIELD
};

const pb_field_t Person_fields[] = {
    {1, offsetof(Person, name), PB_ACT_STRING, membersize(Person, name)},
    {2, offsetof(Person, id), PB_ACT_INT32, membersize(Person, id)},
    {3, offsetof(Person, email), PB_ACT_STRING, membersize(Person, email)},
    {4, offsetof(Person, phone), PB_ACT_SUBMESSAGE, membersize(Person, phone)}
};

/* Default value descriptions */
#define Person_PhoneNumber_default {"", false, Person_PhoneType_HOME};
#define Person_default {"", 0, false, "", {{0},0}};

/* And now, the actual test program */

bool print_phonenumber(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    Person_PhoneNumber x = Person_PhoneNumber_default;
    if (!pb_decode(stream, Person_PhoneNumber_fields, &x))
        return false;
    
    printf("PhoneNumber: number '%s' type '%d'\n", x.number, x.type);
    return true;
}

bool print_person(pb_istream_t *stream)
{
    Person x = Person_default;
    x.phone.funcs.decode = &print_phonenumber;
    
    if (!pb_decode(stream, Person_fields, &x))
        return false;
    
    printf("Person: name '%s' id '%d' email '%s'\n", x.name, x.id, x.email);
    return true;
}

bool my_read(pb_istream_t *stream, char *buf, size_t count)
{
    char *source = (char*)stream->state;
    
    if (!stream->bytes_left)
        return false;
    
    if (buf != NULL)
    {
        memcpy(buf, source, count);
    }
    
    stream->state = source + count;
    return true;
}

int main()
{
    char buffer[512];
    size_t size = fread(buffer, 1, 512, stdin);
    
    pb_istream_t stream = {&my_read, buffer, size};
    if (!print_person(&stream))
        printf("Parsing failed.\n");
    
    return 0;
}
