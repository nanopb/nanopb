# Getting started

For starters, consider this simple message:

~~~~ protobuf
message Example {
    required int32 value = 1;
}
~~~~

Save this in `message.proto` and compile it:

    user@host:~$ python nanopb/generator/nanopb_generator.py message.proto

You should now have in `message.pb.h`:

    typedef struct {
       int32_t value;
    } Example;

    extern const pb_msgdesc_t Example_msg;
    #define Example_fields &Example_msg

Then you have to include the nanopb headers and the generated header:

    #include <pb_encode.h>
    #include "message.pb.h"

Now in your main program do this to encode a message:

    Example mymessage = {42};
    uint8_t buffer[10];
    pb_encode_ctx_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    pb_encode(&stream, Example_fields, &mymessage);

After that, buffer will contain the encoded message. The number of bytes
in the message is stored in `stream.bytes_written`. You can feed the
message to `protoc --decode=Example message.proto` to verify its
validity.

For a complete example of the simple case, see [examples/simple/simple.c](https://github.com/nanopb/nanopb/blob/master/examples/simple/simple.c).
For a more complex example with network interface, see the [examples/network_server](https://github.com/nanopb/nanopb/tree/master/examples/network_server) subdirectory.
