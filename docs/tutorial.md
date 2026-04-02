# Getting stated with nanopb

## Protobuf basics

The basic idea in protobuf is to define Messages, which can then be encoded
and transferred between systems. All protobuf libraries use a common `.proto` format
for describing the message formats and the encoded message data is compatible.

Protobuf libraries for different programming languages vary in how the data is
represented to the user program. For nanopb, the data is stored in C structs.

## Generated structures

For starters, consider this simple message:

```
message Example {
    required int32 value = 1;
}
```

Save this in `message.proto` and run the nanopb generator:

    user@host:~$ python3 nanopb/generator/nanopb_generator.py message.proto
    Writing to message.pb.h and message.pb.c

You should now have `message.pb.h` which contains, among other definitions, the structure:

    typedef struct Example {
       int32_t value;
    } Example;

## Encoding messages

To use nanopb from your own code, include the nanopb library header and the generated message header:

    #include <pb_encode.h>
    #include "message.pb.h"

Then in your main function fill in the data for the structure:

    Example mymessage = {};
    mymessage.value = 42;

It is recommended to initialize message structures before use, to ensure that uninitialized fields do not contain garbage values. Either `= {}` initializer, the generated `Example_init_zero` macro or `memset()` can be used.

Before encoding the message, you need to create an encoding context that defines where the output data will be written. In this example we will use a simple memory buffer.
The automatically generated macro `Example_size` gives the maximum length of the message.

    pb_encode_ctx_t stream;
    uint8_t buffer[Example_size];
    pb_init_encode_ctx_for_buffer(&stream, buffer, sizeof(buffer));

To process the structure, a matching message descriptor is needed.
This is declared in the automatically generated `.pb.h` header, and is called `MessageName_fields`.

Finally, we will call `pb_encode()` to convert data from the structure `mymessage` into binary protobuf data in `buffer`.
The function will return success or error status, which you should check.

    if (!pb_encode(&stream, Example_fields, &mymessage))
    {
        printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
    }

The encoded message may be shorter than the provided buffer.
Check `stream.bytes_written` to determine the actual message length.
The result data can then be e.g. written to a file:

    FILE *file = fopen("example.pb", "wb");
    fwrite(buffer, 1, stream.bytes_written, file);
    fclose(file);

You can feed the message to `protoc --decode=Example message.proto` to verify its validity.

## Decoding messages

Decoding received message data works very similarly.
Include the nanopb decoder header and the message header:

    #include <pb_decode.h>
    #include "message.pb.h"

Load the data to be decoded. In this case it comes from a file, but you can use any communication interface:

    uint8_t buffer[Example_size];
    FILE *file = fopen("example.pb", "rb");
    size_t msglen = fread(buffer, 1, sizeof(buffer), file);
    fclose(file)

Prepare the input stream. Note that you need to pass the actual message length, not the size of the buffer:

    pb_decode_ctx_t stream;
    pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);

Prepare the structure where the decoded data is stored:

    Example mymessage = {};

And finally call `pb_decode()` and check for errors:

    if (!pb_decode(&stream, Example_fields, &mymessage))
    {
        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
    }

After the decoding, you can access the fields in the structure to process the data:

    printf("We received value: %d\n", (int)mymessage.value);

## Further reading

For a complete example of the simple case, see [examples/simple/simple.c](https://github.com/nanopb/nanopb/blob/master/examples/simple/simple.c).
For a more complex example with network interface, see the [examples/network_server](https://github.com/nanopb/nanopb/tree/master/examples/network_server) subdirectory.
