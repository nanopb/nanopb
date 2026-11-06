/* Make sure that all fields are freed in various scenarios. */

#include <pb_decode.h>
#include <pb_encode.h>
#include <malloc_wrappers.h>
#include <stdio.h>
#include <test_helpers.h>
#include "mem_release.pb.h"

#define TEST(x) if (!(x)) { \
    fprintf(stderr, "Test " #x " on line %d failed.\n", __LINE__); \
    return false; \
    }

static char *test_str_arr[] = {"1", "2", ""};
static SubMessage test_msg_arr[] = {SubMessage_init_zero, SubMessage_init_zero};

static bool test_TestMessage()
{
    uint8_t buffer[256];
    size_t msgsize;
    
    /* Construct a message with various fields filled in */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_extension_t ext2;
        pb_ostream_t stream;
        msg.static_req_submsg.dynamic_str = "12345";
        msg.static_req_submsg.dynamic_str_arr_count = 3;
        msg.static_req_submsg.dynamic_str_arr = test_str_arr;
        msg.static_req_submsg.dynamic_submsg_count = 2;
        msg.static_req_submsg.dynamic_submsg = test_msg_arr;
        msg.static_req_submsg.dynamic_submsg[1].dynamic_str = "abc";
        msg.static_opt_submsg.dynamic_str = "abc";
        msg.has_static_opt_submsg = true;
        msg.dynamic_submsg = &msg.static_req_submsg;
        
        msg.extensions = &ext2;
        ext2.type = &static_ext;
        ext2.dest = &msg.static_req_submsg;
        ext2.next = NULL;
        
        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        if (!pb_encode(&stream, TestMessage_fields, &msg))
        {
            fprintf(stderr, "Encode failed: %s\n", PB_GET_ERROR(&stream));
            return false;
        }
        msgsize = stream.bytes_written;
    }
    
    /* Output encoded message for debug */
    SET_BINARY_MODE(stdout);
    fwrite(buffer, 1, msgsize, stdout);
    
    /* Decode memory using dynamic allocation */
    {
        TestMessage msg = TestMessage_init_zero;
        pb_istream_t stream;
        SubMessage ext2_dest = SubMessage_init_zero;
        pb_extension_t ext2;
        
        msg.extensions = &ext2;
        ext2.type = &static_ext;
        ext2.dest = &ext2_dest;
        ext2.next = NULL;
        
        stream = pb_istream_from_buffer(buffer, msgsize);
        if (!pb_decode(&stream, TestMessage_fields, &msg))
        {
            fprintf(stderr, "Decode failed: %s\n", PB_GET_ERROR(&stream));
            return false;
        }
        
        /* Make sure it encodes back to same data */
        {
            uint8_t buffer2[256];
            pb_ostream_t ostream = pb_ostream_from_buffer(buffer2, sizeof(buffer2));
            TEST(pb_encode(&ostream, TestMessage_fields, &msg));
            TEST(ostream.bytes_written == msgsize);
            TEST(memcmp(buffer, buffer2, msgsize) == 0);
        }
        
        /* Make sure that malloc counters work */
        TEST(get_alloc_count() > 0);
        
        /* Make sure that pb_release releases everything */
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
        
        /* Check that double-free is a no-op */
        pb_release(TestMessage_fields, &msg);
        TEST(get_alloc_count() == 0);
    }
    
    return true;
}

/* Decoding fails on a static repeated submessage entry before the entry has
 * been initialized. The array contents are not cleared by pb_decode(), so the
 * pointers in the entry must not be taken from the old memory contents. */
static bool test_StaticRepeatedError()
{
    /* static_rep_submsg with wrong wire type */
    static uint8_t wrong_type[] = {0x20, 0x00};
    /* static_rep_submsg with length past the end of input */
    static uint8_t truncated[] = {0x22, 0x05};
    /* One valid entry with dynamic_str = "a", then wrong wire type */
    static uint8_t second_entry[] = {0x22, 0x03, 0x0A, 0x01, 0x61, 0x20, 0x00};

    const struct {
        uint8_t *data;
        size_t size;
    } inputs[] = {
        {wrong_type, sizeof(wrong_type)},
        {truncated, sizeof(truncated)},
        {second_entry, sizeof(second_entry)}
    };
    size_t i;

    for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
    {
        TestMessage msg;
        pb_istream_t stream = pb_istream_from_buffer(inputs[i].data, inputs[i].size);
        memset(&msg, 0xAA, sizeof(msg));
        msg.extensions = NULL;
        TEST(!pb_decode(&stream, TestMessage_fields, &msg));
        TEST(get_alloc_count() == 0);
    }

    return true;
}

int main()
{
    if (test_TestMessage() && test_StaticRepeatedError())
        return 0;
    else
        return 1;
}

