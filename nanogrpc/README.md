## Callback issue

User needs to decode incomming data by himself, because it is impossible to set init structure
For now `nanogrpc` uses doubled buffers.

## Using buffers.
When first stream arrives, grpc

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
