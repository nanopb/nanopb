// Example of encoding a protobuf message in 'delimited' format.
// The message length is indicated by a length prefix.

#include <stdio.h>
#include <pb_encode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "framing_testdata.h"

int main()
{
    // Prepare the stream, output goes directly to stdout
    pb_encode_ctx_t stream;
    init_encode_ctx_for_stdio(&stream, stdout, PB_SIZE_MAX, NULL, 0);
    
    // Request delimited encoding
    stream.flags |= PB_ENCODE_CTX_FLAG_DELIMITED;

    // Write multiple messages into same stream
    if (!pb_encode(&stream, Person_fields, &framing_testdata_msg1) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg2) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg3))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    return 0; /* Success */
}
