#include "size_corruption.pb.h"
#include <pb_decode.h>

int main()
{
    MainMessage msg = MainMessage_init_zero;
    msg.bar_count = (pb_size_t)-1;
    pb_release(NULL /* Uses default allocator */, MainMessage_fields, &msg);
    
    return 0;
}

