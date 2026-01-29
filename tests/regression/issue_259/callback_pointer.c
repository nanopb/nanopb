#include "callback_pointer.pb.h"
#include <unittests.h>
#include <pb_decode.h>

int main()
{
    int status = 0;
    const uint8_t msgdata[] = {0x0A, 0x02, 0x08, 0x7F};

    MainMessage msg = MainMessage_init_zero;
    
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, msgdata, sizeof(msgdata));
        COMMENT("Running first decode");
        TEST(pb_decode(&stream, MainMessage_fields, &msg));
        pb_release(&stream, MainMessage_fields, &msg);
    }
    
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, msgdata, sizeof(msgdata));
        COMMENT("Running second decode");
        TEST(pb_decode(&stream, MainMessage_fields, &msg));
        pb_release(&stream, MainMessage_fields, &msg);
    }
    
    TEST(get_alloc_count() == 0);
    
    return status;
}

