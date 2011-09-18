======================
Nanopb: Basic concepts
======================

.. include :: menu.rst

The things outlined here are the underlying concepts of the nanopb design.

.. contents::

Proto files
===========
All Protocol Buffers implementations use .proto files to describe the message format.
The point of these files is to be a portable interface description language.

Compiling .proto files for nanopb
---------------------------------
Nanopb uses the Google's protoc compiler to parse the .proto file, and then a python script to generate the C header and source code from it::

    user@host:~$ protoc -omessage.pb message.proto
    user@host:~$ python ../generator/nanopb_generator.py message.pb
    Writing to message.h and message.c
    user@host:~$

Compiling .proto files with nanopb options
------------------------------------------
Nanopb defines two extensions for message fields described in .proto files: *max_size* and *max_count*.
These are the maximum size of a string and maximum count of items in an array::

    required string name = 1 [(nanopb).max_size = 40];
    repeated PhoneNumber phone = 4 [(nanopb).max_count = 5];

To use these extensions, you need to place an import statement in the beginning of the file::

    import "nanopb.proto";

This file, in turn, requires the file *google/protobuf/descriptor.proto*. This is usually installed under */usr/include*. Therefore, to compile a .proto file which uses options, use a protoc command similar to::

    protoc -I/usr/include -Inanopb/generator -I. -omessage.pb message.proto

Streams
=======

Nanopb uses streams for accessing the data in encoded format.
The stream abstraction is very lightweight, and consists of a structure (*pb_ostream_t* or *pb_istream_t*) which contains a pointer to a callback function.

There are a few generic rules for callback functions:

#) Return false on IO errors. The encoding or decoding process will abort immediately.
#) Use state to store your own data, such as a file descriptor.
#) *bytes_written* and *bytes_left* are updated by pb_write and pb_read.
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

Otherwise, if *bytes_written* + bytes_to_be_written is larger than *max_size*, pb_write returns false before doing anything else. If you don't want to limit the size of the stream, pass SIZE_MAX.
 
**Example 1:**

This is the way to get the size of the message without storing it anywhere::

 Person myperson = ...;
 pb_ostream_t sizestream = {0};
 pb_encode(&sizestream, Person_fields, &myperson);
 printf("Encoded size is %d\n", sizestream.bytes_written);

**Example 2:**

Writing to stdout::

 bool callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
 {
    FILE *file = (FILE*) stream->state;
    return fwrite(buf, 1, count, file) == count;
 }
 
 pb_ostream_t stdoutstream = {&callback, stdout, SIZE_MAX, 0};

Input streams
-------------
For input streams, there are a few extra rules:

#) If buf is NULL, read from stream but don't store the data. This is used to skip unknown input.
#) You don't need to know the length of the message in advance. After getting EOF error when reading, set bytes_left to 0 and return false. Pb_decode will detect this and if the EOF was in a proper position, it will return true.

Here is the structure::

 struct _pb_istream_t
 {
    bool (*callback)(pb_istream_t *stream, uint8_t *buf, size_t count);
    void *state;
    size_t bytes_left;
 };

The *callback* must always be a function pointer. *Bytes_left* is an upper limit on the number of bytes that will be read. You can use SIZE_MAX if your callback handles EOF as described above.

**Example:**

This function binds an input stream to stdin:

:: 

 bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
 {
    FILE *file = (FILE*)stream->state;
    bool status;
    
    if (buf == NULL)
    {
        while (count-- && fgetc(file) != EOF);
        return count == 0;
    }
    
    status = (fread(buf, 1, count, file) == count);
    
    if (feof(file))
        stream->bytes_left = 0;
    
    return status;
 }
 
 pb_istream_t stdinstream = {&callback, stdin, SIZE_MAX};

Data types
==========

Most Protocol Buffers datatypes have directly corresponding C datatypes, such as int32 is int32_t, float is float and bool is bool. However, the variable-length datatypes are more complex:

1) Strings, bytes and repeated fields of any type map to callback functions by default.
2) If there is a special option *(nanopb).max_size* specified in the .proto file, string maps to null-terminated char array and bytes map to a structure containing a char array and a size field.
3) If there is a special option *(nanopb).max_count* specified on a repeated field, it maps to an array of whatever type is being repeated. Another field will be created for the actual number of entries stored.

=============================================================================== =======================
      field in .proto                                                           autogenerated in .h
=============================================================================== =======================
required string name = 1;                                                       pb_callback_t name;
required string name = 1 [(nanopb).max_size = 40];                              char name[40];
repeated string name = 1 [(nanopb).max_size = 40];                              pb_callback_t name;
repeated string name = 1 [(nanopb).max_size = 40, (nanopb).max_count = 5];      | size_t name_count;
                                                                                | char name[5][40];
required bytes data = 1 [(nanopb).max_size = 40];                               | typedef struct {
                                                                                |    size_t size;
                                                                                |    uint8_t bytes[40];
                                                                                | } Person_data_t;
                                                                                | Person_data_t data;
=============================================================================== =======================

The maximum lengths are checked in runtime. If string/bytes/array exceeds the allocated length, *pb_decode* will return false. 

Field callbacks
===============
When a field has dynamic length, nanopb cannot statically allocate storage for it. Instead, it allows you to handle the field in whatever way you want, using a callback function.

The `pb_callback_t`_ structure contains a function pointer and a *void* pointer you can use for passing data to the callback. If the function pointer is NULL, the field will be skipped. The actual behavior of the callback function is different in encoding and decoding modes.

.. _`pb_callback_t`: reference.html#pb-callback-t

Encoding callbacks
------------------
::

    bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, const void *arg);

When encoding, the callback should write out complete fields, including the wire type and field number tag. It can write as many or as few fields as it likes. For example, if you want to write out an array as *repeated* field, you should do it all in a single call.

Usually you can use `pb_encode_tag_for_field`_ to encode the wire type and tag number of the field. However, if you want to encode a repeated field as a packed array, you must call `pb_encode_tag`_ instead to specify a wire type of *PB_WT_STRING*.

If the callback is used in a submessage, it will be called multiple times during a single call to `pb_encode`_. In this case, it must produce the same amount of data every time. If the callback is directly in the main message, it is called only once.

.. _`pb_encode`: reference.html#pb-encode
.. _`pb_encode_tag_for_field`: reference.html#pb-encode-tag-for-field
.. _`pb_encode_tag`: reference.html#pb-encode-tag

This callback writes out a dynamically sized string::

    bool write_string(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
    {
        char *str = get_string_from_somewhere();
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        return pb_encode_string(stream, (uint8_t*)str, strlen(str));
    }

Decoding callbacks
------------------
::

    bool (*decode)(pb_istream_t *stream, const pb_field_t *field, void *arg);

When decoding, the callback receives a length-limited substring that reads the contents of a single field. The field tag has already been read. For *string* and *bytes*, the length value has already been parsed, and is available at *stream->bytes_left*.

The callback will be called multiple times for repeated fields. For packed fields, you can either read multiple values until the stream ends, or leave it to `pb_decode`_ to call your function over and over until all values have been read.

.. _`pb_decode`: reference.html#pb-decode

This callback reads multiple integers and prints them::

    bool read_ints(pb_istream_t *stream, const pb_field_t *field, void *arg)
    {
        while (stream->bytes_left)
        {
            uint64_t value;
            if (!pb_decode_varint(stream, &value))
                return false;
            printf("%lld\n", value);
        }
        return true;
    }

Field description array
=======================

For using the *pb_encode* and *pb_decode* functions, you need an array of pb_field_t constants describing the structure you wish to encode. This description is usually autogenerated from .proto file.

For example this submessage in the Person.proto file::

 message Person {
    message PhoneNumber {
        required string number = 1 [(nanopb).max_size = 40];
        optional PhoneType type = 2 [default = HOME];
    }
 }

generates this field description array for the structure *Person_PhoneNumber*::

 const pb_field_t Person_PhoneNumber_fields[3] = {
    {1, PB_HTYPE_REQUIRED | PB_LTYPE_STRING,
    offsetof(Person_PhoneNumber, number), 0,
    pb_membersize(Person_PhoneNumber, number), 0, 0},

    {2, PB_HTYPE_OPTIONAL | PB_LTYPE_VARINT,
    pb_delta(Person_PhoneNumber, type, number),
    pb_delta(Person_PhoneNumber, has_type, type),
    pb_membersize(Person_PhoneNumber, type), 0,
    &Person_PhoneNumber_type_default},

    PB_LAST_FIELD
 };


Return values and error handling
================================

Most functions in nanopb return bool: *true* means success, *false* means failure. If this is enough for you, skip this section.

For simplicity, nanopb doesn't define it's own error codes. This might be added if there is a compelling need for it. You can however deduce something about the error causes:

1) Running out of memory. Because everything is allocated from the stack, nanopb can't detect this itself. Encoding or decoding the same type of a message always takes the same amount of stack space. Therefore, if it works once, it works always.
2) Invalid field description. These are usually stored as constants, so if it works under the debugger, it always does.
3) IO errors in your own stream callbacks. Because encoding/decoding stops at the first error, you can overwrite the *state* field in the struct and store your own error code there.
4) Errors that happen in your callback functions. You can use the state field in the callback structure.
5) Exceeding the max_size or bytes_left of a stream.
6) Exceeding the max_size of a string or array field
7) Invalid protocol buffers binary message. It's not like you could recover from it anyway, so a simple failure should be enough.

In my opinion, it is enough that 1. and 2. can be resolved using a debugger.

However, you may be interested which of the remaining conditions caused the error. For 3. and 4., you can set and check the state. If you have to detect 5. and 6., you should convert the fields to callback type. Any remaining problem is of type 7.
