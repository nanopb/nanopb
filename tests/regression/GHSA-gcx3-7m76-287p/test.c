#include <pb_decode.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "test_helpers.h"
#include "test.pb.h"

int main()
{
    pb_decode_ctx_t stream;
    init_decode_ctx_for_stdio(&stream, stdin, PB_SIZE_MAX, NULL, 0);

#if PB_NO_DEFAULT_ALLOCATOR
    stream.allocator = malloc_wrappers_allocator;
#endif

    MyMessage msg = MyMessage_init_default;
    bool status;

    set_max_alloc_bytes(512);

    status = pb_decode(&stream, MyMessage_fields, &msg);
    assert(!status);
    assert(COMPARE_ERRMSG(&stream, "realloc failed"));

    return 0;
}

