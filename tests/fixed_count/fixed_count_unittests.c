#include <stdio.h>
#include <string.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <malloc_wrappers.h>
#include "unittests.h"
#include "fixed_count.pb.h"

int main()
{
    int status = 0;

    {
      pb_byte_t buffer[Message1_size];
      Message1 msg_a = Message1_init_zero;
      Message1 msg_b = Message1_init_zero;

      pb_encode_ctx_t ostream;
      pb_decode_ctx_t istream;
      size_t message_length;

      msg_a.data[0] = 1;
      msg_a.data[0] = 2;
      msg_a.data[0] = 3;

      COMMENT("Test encode and decode with three entries");

      pb_init_encode_ctx_for_buffer(&ostream, buffer, Message1_size);
      TEST(pb_encode(&ostream, Message1_fields, &msg_a));
      message_length = ostream.bytes_written;

      pb_init_decode_ctx_for_buffer(&istream, buffer, message_length);
      TEST(pb_decode(&istream, Message1_fields, &msg_b));

      TEST(istream.bytes_left == 0);
      TEST(memcmp(&msg_b, &msg_a, sizeof(msg_a)) == 0);
    }

    {
      pb_byte_t input[] = {0x08, 0x00, 0x08, 0x01, 0x08, 0x02, 0x08, 0x03};
      pb_decode_ctx_t stream;
      Message1 msg = Message1_init_zero;

      COMMENT("Test wrong number of entries");

      pb_init_decode_ctx_for_buffer(&stream, input, 6);
      TEST(pb_decode(&stream, Message1_fields, &msg));
      TEST(msg.data[0] == 0 && msg.data[1] == 1 && msg.data[2] == 2);

      pb_init_decode_ctx_for_buffer(&stream, input, 8);
      TEST(!pb_decode(&stream, Message1_fields, &msg));
      TEST(strcmp(stream.errmsg, "array overflow") == 0);

      pb_init_decode_ctx_for_buffer(&stream, input, 4);
      TEST(!pb_decode(&stream, Message1_fields, &msg));
      TEST(strcmp(stream.errmsg, "wrong size for fixed count field") == 0);
    }

    {
      pb_byte_t buffer[Message2_size];
      Message2 msg_a = Message2_init_zero;
      Message2 msg_b = Message2_init_zero;

      pb_encode_ctx_t ostream;
      pb_decode_ctx_t istream;
      size_t message_length;

      COMMENT("Test encode and decode with nested messages");

      msg_a.data[0].data[0] = 1;
      msg_a.data[0].data[1] = 2;
      msg_a.data[0].data[2] = 3;
      msg_a.data[1].data[0] = 4;
      msg_a.data[1].data[1] = 5;
      msg_a.data[1].data[2] = 6;

      pb_init_encode_ctx_for_buffer(&ostream, buffer, Message2_size);
      TEST(pb_encode(&ostream, Message2_fields, &msg_a));
      message_length = ostream.bytes_written;

      pb_init_decode_ctx_for_buffer(&istream, buffer, message_length);
      TEST(pb_decode(&istream, Message2_fields, &msg_b));

      TEST(istream.bytes_left == 0);
      TEST(memcmp(&msg_b, &msg_a, sizeof(msg_a)) == 0);
    }

    {
      pb_byte_t buffer[Message3_size];
      Message3 msg_a = Message3_init_zero;
      Message3 msg_b = Message3_init_zero;

      pb_encode_ctx_t ostream;
      pb_decode_ctx_t istream;
      size_t message_length;

      COMMENT("Test encode and decode with two nested messages");

      msg_a.data1[0].data[0].data[0] = 1;
      msg_a.data1[0].data[0].data[1] = 2;
      msg_a.data1[0].data[0].data[2] = 3;
      msg_a.data1[0].data[1].data[0] = 4;
      msg_a.data1[0].data[1].data[1] = 5;
      msg_a.data1[0].data[1].data[2] = 6;

      msg_a.data1[1].data[0].data[0] = 7;
      msg_a.data1[1].data[0].data[1] = 8;
      msg_a.data1[1].data[0].data[2] = 9;
      msg_a.data1[1].data[1].data[0] = 10;
      msg_a.data1[1].data[1].data[1] = 11;
      msg_a.data1[1].data[1].data[2] = 12;

      msg_a.data2[0].data[0].data[0] = 11;
      msg_a.data2[0].data[0].data[1] = 12;
      msg_a.data2[0].data[0].data[2] = 13;
      msg_a.data2[0].data[1].data[0] = 14;
      msg_a.data2[0].data[1].data[1] = 15;
      msg_a.data2[0].data[1].data[2] = 16;

      msg_a.data2[1].data[0].data[0] = 17;
      msg_a.data2[1].data[0].data[1] = 18;
      msg_a.data2[1].data[0].data[2] = 19;
      msg_a.data2[1].data[1].data[0] = 110;
      msg_a.data2[1].data[1].data[1] = 111;
      msg_a.data2[1].data[1].data[2] = 112;

      pb_init_encode_ctx_for_buffer(&ostream, buffer, Message3_size);
      TEST(pb_encode(&ostream, Message3_fields, &msg_a));
      message_length = ostream.bytes_written;

      pb_init_decode_ctx_for_buffer(&istream, buffer, message_length);
      TEST(pb_decode(&istream, Message3_fields, &msg_b));

      TEST(istream.bytes_left == 0);
      TEST(memcmp(&msg_b, &msg_a, sizeof(msg_a)) == 0);
    }

    {
      pb_byte_t buffer[256];
      Message4 msg_a = Message4_init_zero;
      Message4 msg_b = Message4_init_zero;

      pb_encode_ctx_t ostream;
      pb_decode_ctx_t istream;
      size_t message_length;

      COMMENT("Test encode and decode with pointer type fixarray");

      SubMessage submsgs[pb_arraysize(Message4, submsgs[0])] = {SubMessage_init_zero};
      submsgs[0].a = 1;
      submsgs[1].a = 5;
      submsgs[2].a = 999;

      char a[5] = "a";
      char b[5] = "b";
      char abc[5] = "abc";
      char *strings[pb_arraysize(Message4, strings[0])] = {a, b, abc};

      pb_byte_t bytes3[3] = {1, 2, 3};
      pb_bytes_t bytes[pb_arraysize(Message4, bytes[0])] = {{0, NULL}, {0, NULL}, {3, bytes3}};

      msg_a.submsgs = &submsgs;
      msg_a.strings = &strings;
      msg_a.bytes = &bytes;

      pb_init_encode_ctx_for_buffer(&ostream, buffer, Message3_size);
      TEST(pb_encode(&ostream, Message4_fields, &msg_a));
      message_length = ostream.bytes_written;

      TEST(get_alloc_count() == 0);

      pb_init_decode_ctx_for_buffer(&istream, buffer, message_length);
      TEST(pb_decode(&istream, Message4_fields, &msg_b));

      TEST(istream.bytes_left == 0);

      TEST((*msg_b.submsgs)[0].a == 1);
      TEST((*msg_b.submsgs)[1].a == 5);
      TEST((*msg_b.submsgs)[2].a == 999);

      TEST(strcmp((*msg_b.strings)[0], "a") == 0);
      TEST(strcmp((*msg_b.strings)[1], "b") == 0);
      TEST(strcmp((*msg_b.strings)[2], "abc") == 0);

      TEST((*msg_b.bytes)[0].size == 0);
      TEST((*msg_b.bytes)[1].size == 0);
      TEST((*msg_b.bytes)[2].size == 3 && memcmp((*msg_b.bytes)[2].bytes, bytes3, 3) == 0);

      pb_release(&istream, Message4_fields, &msg_b);

      TEST(get_alloc_count() == 0);
    }

    if (status != 0)
        fprintf(stdout, "\n\nSome tests FAILED!\n");

    return status;
}
