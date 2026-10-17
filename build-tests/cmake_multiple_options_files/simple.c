#include "simple.pb.h"

/* name comes from common.options, value is overridden by extra.options
 * and note by simple.options, which is read last. */
typedef char check_common_options_applied[
    sizeof(((SimpleMessage *)0)->name) == 4 ? 1 : -1
];
typedef char check_extra_options_applied[
    sizeof(((SimpleMessage *)0)->value) == 8 ? 1 : -1
];
typedef char check_proto_options_applied[
    sizeof(((SimpleMessage *)0)->note) == 16 ? 1 : -1
];

int main(void)
{
    SimpleMessage message = SimpleMessage_init_zero;
    return message.name[0] + message.value[0] + message.note[0];
}
