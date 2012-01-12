/* Attempts to test all the datatypes supported by ProtoBuf.
 * Currently only tests the 'required' variety.
 */

#include <stdio.h>
#include <pb_encode.h>
#include "alltypes.pb.h"

int main()
{
    /* Initialize the structure with constants */
    AllTypes alltypes = {
        1001,
        1002,
        1003,
        1004,
        1005,
        1006,
        true,
        
        1008,
        1009,
        1010.0f,
        
        1011,
        1012,
        1013.0,
        
        "1014",
        {4, "1015"},
        {"1016", 1016},
        MyEnum_Truth,
        
        1099
    };
    
    uint8_t buffer[512];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    /* Now encode it and check if we succeeded. */
    if (pb_encode(&stream, AllTypes_fields, &alltypes))
    {
        fwrite(buffer, 1, stream.bytes_written, stdout);
        return 0; /* Success */
    }
    else
    {
        return 1; /* Failure */
    }
}
