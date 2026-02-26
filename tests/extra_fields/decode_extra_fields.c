// Test usage of a field callback to catch any unknown fields in a message.

#include <stdio.h>
#include <pb_decode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "print_person.h"

bool field_callback(pb_decode_ctx_t *ctx, const pb_field_iter_t *field)
{
    printf("Got unknown field with tag %u, type 0x%02x, length %u\n",
        (unsigned int)field->tag, (unsigned int)field->type, (unsigned int)ctx->bytes_left);
    return true;
}

int main()
{
    pb_decode_ctx_t stream;
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, NULL, 0);
    stream.field_callback = field_callback;

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
