#include <stdio.h>
#include <string.h>
#include <pb_decode.h>
#include <malloc_wrappers.h>
#include "merge.pb.h"
#include "unittests.h"

/* opt { a: 1 values: [1, 2] text: "first" } */
#define OPT1 0x0a, 0x0d, 0x08, 0x01, 0x18, 0x01, 0x18, 0x02, 0x22, 0x05, 'f', 'i', 'r', 's', 't'
/* opt { b: 2 values: [3] } */
#define OPT2 0x0a, 0x04, 0x10, 0x02, 0x18, 0x03
/* sub { a: 5 values: [7] } */
#define SUB1 0x12, 0x04, 0x08, 0x05, 0x18, 0x07
/* sub { b: 6 text: "x" } */
#define SUB2 0x12, 0x05, 0x10, 0x06, 0x22, 0x01, 'x'
/* number: 9 */
#define NUM 0x18, 0x09

static bool decode(TestMessage *msg, const pb_byte_t *data, size_t size)
{
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    memset(msg, 0, sizeof(*msg));
    return pb_decode(&stream, TestMessage_fields, msg);
}

int main(void)
{
    int status = 0;
    TestMessage msg;

    {
        /* Optional pointer submessage given twice is merged. */
        const pb_byte_t data[] = {OPT1, OPT2};
        TEST(decode(&msg, data, sizeof(data)));
        TEST(msg.opt != NULL);
        TEST(msg.opt->has_a && msg.opt->a == 1);
        TEST(msg.opt->has_b && msg.opt->b == 2);
        TEST(msg.opt->values_count == 3);
        TEST(msg.opt->values[0] == 1 && msg.opt->values[1] == 2 && msg.opt->values[2] == 3);
        TEST(msg.opt->text != NULL && strcmp(msg.opt->text, "first") == 0);
        TEST(!msg.opt->has_c && msg.opt->c == 42);
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }

    {
        /* Same oneof member given twice is merged. */
        const pb_byte_t data[] = {SUB1, SUB2};
        TEST(decode(&msg, data, sizeof(data)));
        TEST(msg.which_choice == TestMessage_sub_tag);
        TEST(msg.choice.sub->has_a && msg.choice.sub->a == 5);
        TEST(msg.choice.sub->has_b && msg.choice.sub->b == 6);
        TEST(msg.choice.sub->values_count == 1 && msg.choice.sub->values[0] == 7);
        TEST(msg.choice.sub->text != NULL && strcmp(msg.choice.sub->text, "x") == 0);
        TEST(msg.choice.sub->c == 42);
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }

    {
        /* A different oneof member in between still replaces the submessage. */
        const pb_byte_t data[] = {SUB1, NUM, SUB2};
        TEST(decode(&msg, data, sizeof(data)));
        TEST(msg.which_choice == TestMessage_sub_tag);
        TEST(!msg.choice.sub->has_a);
        TEST(msg.choice.sub->has_b && msg.choice.sub->b == 6);
        TEST(msg.choice.sub->values_count == 0);
        TEST(msg.choice.sub->text != NULL && strcmp(msg.choice.sub->text, "x") == 0);
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }

    {
        /* A single occurrence still gets the default values. */
        const pb_byte_t data[] = {OPT2};
        TEST(decode(&msg, data, sizeof(data)));
        TEST(msg.opt != NULL);
        TEST(!msg.opt->has_a && msg.opt->has_b && msg.opt->b == 2);
        TEST(!msg.opt->has_c && msg.opt->c == 42);
        TEST(msg.opt->text == NULL);
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }

    {
        /* Combining two messages with PB_DECODE_NOINIT merges them too. */
        const pb_byte_t data1[] = {OPT1};
        const pb_byte_t data2[] = {OPT2};
        pb_istream_t stream;
        TEST(decode(&msg, data1, sizeof(data1)));
        stream = pb_istream_from_buffer(data2, sizeof(data2));
        TEST(pb_decode_ex(&stream, TestMessage_fields, &msg, PB_DECODE_NOINIT));
        TEST(msg.opt != NULL);
        TEST(msg.opt->has_a && msg.opt->a == 1);
        TEST(msg.opt->has_b && msg.opt->b == 2);
        TEST(msg.opt->values_count == 3);
        TEST(msg.opt->text != NULL && strcmp(msg.opt->text, "first") == 0);
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }

    {
        /* Failure while merging must not leak or double free. */
        const pb_byte_t data[] = {OPT1, 0x0a, 0x04, 0x10, 0x02, 0x18};
        TEST(!decode(&msg, data, sizeof(data)));
        TEST(get_alloc_count() == 0);
    }

    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
