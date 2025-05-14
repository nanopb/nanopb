Nanopb example "simple"
=======================

This example demonstrates the very basic use of nanopb. It encodes and
decodes a simple message.

The code uses four different API functions:

  * pb_ostream_from_buffer() to declare the output buffer that is to be used
  * pb_encode() to encode a message
  * pb_istream_from_buffer() to declare the input buffer that is to be used
  * pb_decode() to decode a message

Example usage
-------------


Linux
~~~~~

To prepare your environment to run the generator, execute the following commands:

  python3 -m venv venv 
  source venv/bin/activate
  pip install -r requirements.txt


Then, simply type "make" to build the example. After that, you can
run it with the command: ./simple


Other platforms
~~~~~~~~~~~~~~~

You first have to compile the protocol definition using
the following command:

  ../../generator-bin/protoc --nanopb_out=. simple.proto

After that, add the following five files to your project and compile:

  simple.c  simple.pb.c  pb_encode.c  pb_decode.c  pb_common.c


