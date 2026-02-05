// Example of encoding a null-terminated protobuf stream.
// Because 0 tag is invalid in protobuf format, it can be used to indicate message end.
// Support for this feature was previously built-in to the nanopb library, but
// removed in version 1.0. It can still be implemented in user code like this.

#include <stdio.h>
#include <pb_encode.h>
#include "person.pb.h"
#include "test_helpers.h"
#include "framing_testdata.h"

bool write_terminator(pb_encode_ctx_t *stream)
{
    return pb_write(stream, (const pb_byte_t*)"\0", 1) &&
           pb_flush_write_buffer(stream);
}

int main()
{
    // Prepare the stream, output goes directly to stdout
    pb_encode_ctx_t stream;
    init_encode_ctx_for_stdio(&stream, stdout, PB_SIZE_MAX, NULL, 0);
    
    // Write multiple messages delimited by the null bytes
    if (!pb_encode(&stream, Person_fields, &framing_testdata_msg1) ||
        !write_terminator(&stream) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg2) ||
        !write_terminator(&stream) ||
        !pb_encode(&stream, Person_fields, &framing_testdata_msg3) ||
        !write_terminator(&stream))
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    return 0; /* Success */
}
