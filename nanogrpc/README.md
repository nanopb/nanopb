# About
this is grpc implementaiton which aims communication over serial interfaces
like UART or RS232.

Instead of
[standard HTTP protocol](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md)
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
[HLDC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control) (which is my
goal)

## Path to methods
GRPC identifies methods by paths, which are basically strings.
In order to reduce traffic on serial line (which in real world might be very
  slow)
there is option of identifying method by hash of path, which could be for
example CRC32 from path. For now in `nanogrpc.proto` field `name_crc` is marked
required, because for now `nanopb` doesn't allow to use `oneOf`'s with
dynamically allocated memory.

### Method naming
Different services may have methods with same names, but they won't be the same
methods. There are two ways to solve this problem:
* Use unique names for methods
* Use separate source file for each service, define methods as static, but
adding callbacks or manipulating method request/response holders would require
referencing them by path (as string), and it sounds like a mess.

## Generator
`nanogrpc_generator.py` is a copy of `nanopb_generator.py` with removed unused
classes and methods.

## Examples

For now I am testing it on STM32F4DISCOVERY board as submodule so that I didn't
provide any examples yet. But I will do it as soon as I will have generator
working.

## What is done so far, how to play with it
##### Concept of call IDs
When calling method, client needs to provide random/unique id, that will
identify call. This allows to handle multuple methods as one time. In normal
grpc it is being done with http2 connections, but in order not to implement
whole stack and keep stuff simple (over single serial connection) call are
being identified with IDs.

##### Concept of contexts, blocking and non blocking parsing
Each method can hold pointer to structure (`context`) which holds pointers to
to request and response structures and pointer next context structure. It is
needed for implementing non blocking parsing.

In blocking mode (`ng_GrpcParseBlocking`) only one (default) context is being
used in order to keep it simple (to use). Callback cannot schedule response
for later time. In such case error message will be sent.

In nonblocking parsing when server calls method there it could be possible to
have several calls of the same method ongoing (with own contexts). For now only
one method at time is implemented. When call on specific method is ongoing, when
another one arrives, previous will be removed. In order to handle several
methods of one type, there is need of managing not finished methods. It can be
achieved by introducing timeout functionality or keeping linked list of
contexts in form of queue. Newly registered would be placed in front of queue,
and removed to the end. In case overflow the latest call be overridden, but
it won't block. It could be default behavior in case of not having timeout.

When call arrives, callback can respond immediately, schedule response for
later time or do both (and inform server by return code), but will allow for
next responses only if method definition allows for server streaming.

It could also possible to dynamically allocate contexts, but since user is given
pointer to context which might be removed before he finishes call (timeout) it
will be tricky to remove it an NULL all pointers pointing to it...
To be discussed. 

## Roadmap
* tests
* more examples
* some client side implementations in python, c++, and node-red

### about nanogrpc.proto
Inside of this file you can notice that messages are duplicated with `_CS`
postfix (for client side code). Those messages differ in options. On server side
data from request needs to be stored in under pointer because during time of
receiving we don't know what method is that (we could decode it immediately if
we knew method id or path in advance, grpc allows to change of order of tags),
but we can encode response in callback. On client side situation is opposite.
There are two sollutions - having duplicated messages or specifying options
during compilation time (which sounds problematic)


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
