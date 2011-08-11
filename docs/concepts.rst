======================
Nanopb: Basic concepts
======================

The things outlined here are common to both the encoder and the decoder part.

Return values and error handling
================================

Most functions in nanopb return *bool*. *True* means success, *false* means failure.

Because code size is of the essence, nanopb doesn't give any information about the cause of the error. However, there are few possible sources of errors:

1) Running out of memory. Because everything is allocated from the stack, nanopb can't detect this itself. Encoding or decoding the same type of a message always takes the same amount of stack space. Therefore, if it works once, it works always.
2) Invalid field description. These are usually stored as constants, so if it works under the debugger, it always does.
3) IO errors in your own stream callbacks. Because encoding/decoding stops at the first error, you can overwrite the *state* field in the struct and store your own error code there.
4) Errors in your callback functions. You can use the state field in the callback structure.
5) Exceeding the max_size or bytes_left of a stream.
6) Exceeding the max_size of a string or array field
7) Invalid protocol buffers binary message. It's not like you could recover from it anyway, so a simple failure should be enough.

In my opinion, it is enough that 1) and 2) can be resolved using a debugger.

However, you may be interested which of the remaining conditions caused the error. For 3) and 4), you can check the state. If you have to detect 5) and 6), you should convert the fields to callback type. Any remaining problem is of type 7).

Streams
=======

Nanopb uses streams for accessing the data in encoded format.
The stream abstraction is very lightweight, and consists of a structure (*pb_ostream_t* or *pb_istream_t*) which contains a pointer to a callback function.

There are a few generic rules for callback functions:

#) Return false on IO errors. The encoding or decoding process will abort immediately.
#) Use state to store your own data, such as a file descriptor.
#) *bytes_written* and *bytes_left* are updated by *pb_write* and *pb_read*. Don't touch them.
#) Your callback may be used with substreams. In this case *bytes_left*, *bytes_written* and *max_size* have smaller values than the original stream. Don't use these values to calculate pointers.

Output streams
--------------

::

 struct _pb_ostream_t
 {
    bool (*callback)(pb_ostream_t *stream, const uint8_t *buf, size_t count);
    void *state;
    size_t max_size;
    size_t bytes_written;
 };

The *callback* for output stream may be NULL, in which case the stream simply counts the number of bytes written. In this case, *max_size* is ignored.

Otherwise, if *bytes_written* + bytes_to_be_written is larger than *max_size*, *pb_write* returns false before doing anything else. If you don't want to limit the size of the stream, pass SIZE_MAX.

Most commonly you want to initialize *bytes_written* to 0. It doesn't matter to the library, though.
 
**Example 1:**

This is the way to get the size of the message without storing it anywhere::

 Person myperson = ...;
 pb_ostream_t sizestream = {0};
 pb_encode(&sizestream, Person_fields, &myperson);
 printf("Encoded size is %d\n", sizestream.bytes_written);

**Example 2:**

Writing to stdout::

 bool streamcallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
 {
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
 }
 
 pb_ostream_t stdoutstream = {&streamcallback, stdout, SIZE_MAX, 0};

Input streams
-------------
For input streams, there are a few extra rules:
#) If buf is NULL, read from stream but don't store the data. This is used to skip unknown input.
#) You don't need to know the length of the message in advance. After getting EOF error when reading, set bytes_left to 0 and return false. Pb_decode will detect this and if the EOF was in a proper position, it will return true.

::

 struct _pb_istream_t
 {
    bool (*callback)(pb_istream_t *stream, uint8_t *buf, size_t count);
    void *state;
    size_t bytes_left;
 };

The *callback* must always be a function pointer.

*Bytes_left* is an upper limit on the number of bytes that will be read. You can use SIZE_MAX if your callback handles EOF as described above.

**Example**