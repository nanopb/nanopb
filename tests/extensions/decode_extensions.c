/* Test decoding of extension fields. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pb_decode.h>
#include "alltypes.pb.h"
#include "extensions.pb.h"

#define TEST(x) if (!(x)) { \
    printf("Test " #x " failed.\n"); \
    return 2; \
    }

int main(int argc, char **argv)
{
    uint8_t buffer[1024];
    size_t count = fread(buffer, 1, sizeof(buffer), stdin);
    pb_istream_t stream = pb_istream_from_buffer(buffer, count);
    
    AllTypes alltypes = {};
    
    int32_t extensionfield1;
    pb_extension_t ext1 = {&AllTypes_extensionfield1, &extensionfield1, NULL};
    alltypes.extensions = &ext1;
        
    ExtensionMessage extensionfield2 = {};
    pb_extension_t ext2 = {&ExtensionMessage_AllTypes_extensionfield2, &extensionfield2, NULL};
    ext1.next = &ext2;
    
    if (!pb_decode(&stream, AllTypes_fields, &alltypes))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    TEST(extensionfield1 == 12345)
    TEST(strcmp(extensionfield2.test1, "test") == 0)
    TEST(extensionfield2.test2 == 54321)
    
    return 0;
}

