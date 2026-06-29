#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include "test_helpers.h"
#include "deep.pb.h"
#include <time.h>

long get_ms(void);

long get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}


int main(void)
{
    pb_byte_t buffer[DeepMessage_size];
    pb_ostream_t stream;

    int i;
    long start;
    long duration = 0;

    DeepMessage msg = {0};

    strcpy(msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.strval, "abcd");
    msg.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.intval = 987654321;
    msg.first = 888;
    msg.last = 999;

    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    start = get_ms();
    for (i = 0; i < 10000; i++)
    {
        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        if (!pb_encode(&stream, DeepMessage_fields, &msg))
            return 1;
        
    }
    duration = get_ms() - start;
    fprintf(stderr, "took %ld ms\n", duration);
    return 0;
}
