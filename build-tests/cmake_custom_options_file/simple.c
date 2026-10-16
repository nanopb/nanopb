#include "simple.pb.h"

typedef char check_custom_options_applied[
    sizeof(((SimpleMessage *)0)->value) == 8 ? 1 : -1
];

int main(void)
{
    SimpleMessage message = SimpleMessage_init_zero;
    return message.value[0];
}
