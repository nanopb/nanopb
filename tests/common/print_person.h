#ifndef PRINT_PERSON_H
#define PRINT_PERSON_H

#include "person.pb.h"
#include <stdio.h>

// Print a 'Person' message to stdout for testing
static void print_person(Person *person)
{
    pb_size_t i;

    printf("name: \"%s\"\n", person->name);
    printf("id: %ld\n", (long)person->id);
    
    if (person->has_email)
        printf("email: \"%s\"\n", person->email);
    
    for (i = 0; i < person->phone_count; i++)
    {
        Person_PhoneNumber *phone = &person->phone[i];
        printf("phone {\n");
        printf("  number: \"%s\"\n", phone->number);
        
        if (phone->has_type)
        {
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
        }
        printf("}\n");
    }
}

#endif
