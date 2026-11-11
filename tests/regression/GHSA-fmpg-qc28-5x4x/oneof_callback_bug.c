/* Regression test for a bug in how callbacks inside oneofs were cleared */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <assert.h>
#include "oneof.pb.h"
#include "test_helpers.h"
#include "unittests.h"

int main(int argc, char **argv)
{
    uint8_t msg1data[32];
    size_t msg1len = 0;
    uint8_t msg2data[32];
    size_t msg2len = 0;
    int status = 0;

    {
        pb_ostream_t stream = pb_ostream_from_buffer(msg1data, sizeof(msg1data));
        pb_encode_tag(&stream, PB_WT_STRING, OneOfMessage_submsg2_tag);
        pb_encode_varint(&stream, 5);
        pb_encode_tag(&stream, PB_WT_32BIT, SubMsg2_intvalue_tag);
        uint32_t value = 0xDEADBEEF;
        pb_encode_fixed32(&stream, &value);
        TEST(stream.bytes_written > 0);
        msg1len = stream.bytes_written;
    }

    {
        pb_ostream_t stream = pb_ostream_from_buffer(msg2data, sizeof(msg2data));
        pb_encode_tag(&stream, PB_WT_STRING, OneOfMessage_submsg1_tag);
        pb_encode_varint(&stream, 3);
        pb_encode_tag(&stream, PB_WT_STRING, SubMsg1_strvalue_tag);
        pb_byte_t charx = 'x';
        pb_encode_string(&stream, &charx, 1);
        TEST(stream.bytes_written > 0);
        msg2len = stream.bytes_written;
    }

    {
        pb_istream_t stream;
        OneOfMessage msg = OneOfMessage_init_zero;
    
        stream = pb_istream_from_buffer(msg1data, msg1len);
        TEST(pb_decode(&stream, OneOfMessage_fields, &msg));

        stream = pb_istream_from_buffer(msg2data, msg2len);
        TEST(pb_decode(&stream, OneOfMessage_fields, &msg));
    }

    return status;
}
