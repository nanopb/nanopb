#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "test.pb.h"
#include "unittests.h"

int main(int argc, char **argv)
{
    int status = 0;
    uint8_t buffer[512] = {0};
    int i;
    pb_encode_ctx_t ostream;
    
    Reply reply = Reply_init_zero;
    Reply_Result request_result = Reply_Result_OK;

    pb_init_encode_ctx_for_buffer(&ostream, buffer, sizeof(buffer));
    reply.result = request_result;
    if (!pb_encode(&ostream, Reply_fields, &reply)) {
        fprintf(stderr, "Encode failed: %s\n", PB_GET_ERROR(&ostream));
        return 1;
    }

    printf("response payload (%d):", (int)ostream.bytes_written);
    for (i = 0; i < ostream.bytes_written; i++) {
        printf("0x%02X ", buffer[i]);
    }
    printf("\n");

    pb_byte_t expected[] = {
        0x08, 0x01, // Reply.result == Reply_Result_OK
        0x12, 0x00, // Reply.error = no fields in submsg
        0x1A, 0x02, // Reply.submessage
            0x0A, 0x00 // Reply.submessage.subsubmessageA = no fields in submsg
    };
    TEST(ostream.bytes_written == sizeof(expected));
    TEST(memcmp(buffer, expected, sizeof(expected)) == 0);
    
    return status;
}

