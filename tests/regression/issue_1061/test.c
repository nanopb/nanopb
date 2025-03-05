#include "submsg_ft_callback.pb.h"
#include <assert.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "unittests.h"
#include <stdio.h>
#include <string.h>

bool encode_string_cb(pb_ostream_t *stream, const pb_field_t *field, void * const * arg)
{
    const char* str = *arg;
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    return pb_encode_string(stream, (const pb_byte_t*)str, strlen(str));
}

bool decode_string_cb(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char* str = *arg;
    size_t len = stream->bytes_left;

    if (!pb_read(stream, (pb_byte_t*)str, len)) {
        return false;
    }

    str[len] = '\0';
    return true;
}


bool decode_msg_cb(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
    OneOfMessage *msg = field->message;

    if (field->tag == OneOfMessage_submsg1_tag)
    {
            msg->value.submsg1.arg = *arg;
            msg->value.submsg1.funcs.decode = decode_string_cb;
            return true;
    }

    return false;
}

int main(void)
{
    int status = 0;
    pb_byte_t buf[128];
    size_t msglen;
    char* tx_string = "broccoli";
    size_t tx_string_len = strlen(tx_string);

    {
        pb_ostream_t ostream = pb_ostream_from_buffer(buf, sizeof(buf));
        OneOfMessage msg = OneOfMessage_init_zero;

        msg.prefix = 123;

        msg.which_value = OneOfMessage_submsg1_tag;
        msg.value.submsg1.arg = tx_string;
        msg.value.submsg1.funcs.encode = encode_string_cb;

        msg.suffix = 321;

        if (!pb_encode(&ostream, OneOfMessage_fields, &msg))
        {
            fprintf(stderr, "pb_encode() failed: %s\n", PB_GET_ERROR(&ostream));
            return 1;
        }

        msglen = ostream.bytes_written;
        TEST(msglen > 0);
    }

    {
        pb_istream_t istream = pb_istream_from_buffer(buf, msglen);
        OneOfMessage msg = OneOfMessage_init_zero;
        char rx_string[16];

        msg.cb_value.arg = &rx_string;
        msg.cb_value.funcs.decode = decode_msg_cb;

        if (!pb_decode(&istream, OneOfMessage_fields, &msg))
        {
            fprintf(stderr, "pb_decode() failed: %s\n", PB_GET_ERROR(&istream));
            return 1;
        }

        assert(msg.prefix == 123);
        assert(msg.suffix == 321);

        if (msg.which_value != OneOfMessage_submsg1_tag) {
            fprintf(stderr, "msg.which_value not set correctly: %d\n", msg.which_value);
            return 1;
        }

        if (strncmp(tx_string, rx_string, tx_string_len) != 0) {
            fprintf(stderr, "tx and rx strings do not match after encoding and decoding.\n" \
                "tx: %s\n" \
                "rx: %s\n" \
                ,tx_string, rx_string);
            return 1;
        }

    }

    return status;
}
