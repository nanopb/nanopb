/* Tests extension fields.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "alltypes.pb.h"
#include "extensions.pb.h"

int main(int argc, char **argv)
{
    AllTypes alltypes = {0};
    int32_t extensionfield1 = 12345;
    pb_extension_t ext1 = {&AllTypes_extensionfield1, &extensionfield1, NULL};
    
    alltypes.extensions = &ext1;
    
    uint8_t buffer[1024];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    /* Now encode it and check if we succeeded. */
    if (pb_encode(&stream, AllTypes_fields, &alltypes))
    {
        fwrite(buffer, 1, stream.bytes_written, stdout);
        return 0; /* Success */
    }
    else
    {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1; /* Failure */
    }
}

