Nanopb example "lowlevel"
=========================

This example demonstrates usage of low-level nanopb encoding
and decoding functions to handle situations where more control
over the processing is needed.

The example shows a top-level `UnionMessage` type, which contains
several submessages. One can encode and decode the `UnionMessage` directly,
like any protocol buffers message. But sometimes it is easier to
organize the user code so that each submessage type is handled in a
separate part of the code.

By using some of the lower level nanopb APIs, we can manually generate the
top level message, so that we only need to encode the one submessage that
we actually want. Similarly when decoding, we can manually read the tag of
the top level message, and only then decode the submessage
after we already know its type.

Example usage
-------------

Type `make` to run the example. It will build it and run commands like
following:

./encode 1 | ./decode
Got MsgType1: 42
./encode 2 | ./decode
Got MsgType2: true
./encode 3 | ./decode
Got MsgType3: 3 1415

This simply demonstrates that the "decode" program has correctly identified
the type of the received message, and managed to decode it.


Details of implementation
-------------------------

unionproto.proto contains the protocol used in the example. It consists of
three messages: MsgType1, MsgType2 and MsgType3, which are collected together
into UnionMessage.

encode.c takes one command line argument, which should be a number 1-3. It
then fills in and encodes the corresponding message, and writes it to stdout.

decode.c reads a UnionMessage from stdin. Then it calls the function
decode_unionmessage_type() to determine the type of the message. After that,
the corresponding message is decoded and the contents of it printed to the
screen.

