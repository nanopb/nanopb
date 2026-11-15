/* Regression test for submsg_callback on an extension field.
 * The decoder and encoder used to take the message callback from right
 * before extension->found, and called extension->dest as a function. */

#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "extension.pb.h"
#include "unittests.h"

int main(int argc, char **argv)
{
    int status = 0;
    pb_byte_t buffer[32];
    size_t msglen;

    {
        BaseMessage msg = BaseMessage_init_zero;
        ExtMessage ext = ExtMessage_init_zero;
        pb_extension_t extension;
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        ext.has_value = true;
        ext.value = 42;
        extension.type = &ext_submsg;
        extension.dest = &ext;
        extension.next = NULL;
        extension.found = false;
        msg.extensions = &extension;

        TEST(pb_encode(&stream, BaseMessage_fields, &msg));
        msglen = stream.bytes_written;
    }

    {
        BaseMessage msg = BaseMessage_init_zero;
        ExtMessage ext = ExtMessage_init_zero;
        pb_extension_t extension;
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);

        extension.type = &ext_submsg;
        extension.dest = &ext;
        extension.next = NULL;
        extension.found = false;
        msg.extensions = &extension;

        TEST(pb_decode(&stream, BaseMessage_fields, &msg));
        TEST(extension.found);
        TEST(ext.has_value && ext.value == 42);
    }

    return status;
}
