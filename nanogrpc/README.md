# About
this is grpc implementaiton which aims communication over serial interfaces
like UART or RS232.

Instead of [standard HTTP protocol](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md)
it wraps request and response into `protobuf` messages which definition can be
found in `nanogrpc.proto` file.

This implementation is not going to provide server functionality. User needs to
implement it separately and provide `istream` and `ostream` to `ng_GrpcParse`
function which decodes `GrpcRequest`, looks for specific method, decodes it
with call callback (specified by user), and encodes everything back to ostream.

For now I am encoding data into frames base64 encoded wrapped in `>` `<`
```
>TGlrZSB0aGlz<
```
But it can be eaistly integrated for example with
[HLDC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control) (which is my goal)

## Path

GRPC identifies methods by paths, which are basically strings.
In order to reduce traffic on serial line (which in real world might be very slow)
there is option of identifying method by hash of path, which could be for
example CRC32 from path. For now in `nanogrpc.proto` field `name_crc` is marked
required, because for now `nanopb` doesn't allow to use `oneOf`'s with dynamically allocated memory.

## Examples

For now I am testing it on STM32F4DISCOVERY board as submodule so that I didn't
provide any examples yet. But I will do it as soon as I will have generator
working.


### other disorganized thoughts

When encoding `GrpcResponse` we could use callback to decode method request
on the fly, but we would have to be sure, that field with path have been decoded
previously. Protocol buffers specification allows to change order of encoding
encoding fields, so that decoding method request requires static buffer or
dynamic allocation.

Encoding whole `GrpcResponse` directly to `handle->ouptut` using callback for
encoding method response leads to unwanted behavior: While encoding to non
buffer stream, when during encoding (in callback) error occurs, some bytes
are alresdy written to stream, so that where is no option to undo it and change
status code. It implies to using separate buffer for storing encoded
method response.
On the other, before assigning callback to `GrpcResponse` one can calculate
size of method response to know it it there are no problem in encoding.
