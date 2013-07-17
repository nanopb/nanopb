/* Test decoding of extension fields. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pb_decode.h>
#include "alltypes.pb.h"
#include "extensions.pb.h"

int main(int argc, char **argv)
{
    uint8_t buffer[1024];
    size_t count = fread(buffer, 1, sizeof(buffer), stdin);
    pb_istream_t stream = pb_istream_from_buffer(buffer, count);
    
    AllTypes alltypes = {};
    int32_t extensionfield1;
    pb_extension_t ext1 = {&AllTypes_extensionfield1, &extensionfield1, NULL};
    alltypes.extensions = &ext1;
    
    if (!pb_decode(&stream, AllTypes_fields, &alltypes))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    if (extensionfield1 != 12345)
    {
        printf("Wrong value for extension field: %d\n", extensionfield1);
        return 2;
    }    
    
    return 0;
}

