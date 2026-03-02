# API reference: pb.h

The [pb.h](../pb.h) file contains type and macro definitions shared between nanopb components.
It also validates feature enables for preprocessor `#if` directives and includes the needed system headers.

## Data types

### pb_allocator_t

An [extensible structure](concepts.md#extensible-structures) for providing a custom allocator to nanob decoder functions. The allocator is used when [generator options](reference_generator.md) set a field type to `FT_POINTER`.

    typedef struct pb_allocator_s pb_allocator_t;
    struct pb_allocator_s {
        void* (*realloc)(pb_allocator_t *actx, void *ptr, size_t size);
        void (*free)(pb_allocator_t *actx, void *ptr);
        void *state;
    };


The two functions follow the behavior of standard C `realloc()` and `free()` functions.

The `actx` arguments is a pointer to the `pb_allocator_t` structure, which the allocator implementation can use to store its state. The free pointer field `state` is for user code, it is not used by nanopb. Alternatively the user code can extend the structure definition.

See [tests/custom_allocator](tests/custom_allocator] for an example implementation of an arena allocator.

#### Custom realloc() implementation
If the `ptr` argument is `NULL`, `realloc()` will make a new memory allocation of `size` bytes.

If the `ptr` argument is not `NULL`, the size of the existing allocation is adjusted to new size.
Up to `size` bytes of data from the old allocation are retained.
The `realloc()` implementation is allowed to make a new allocation, copy the data to the new location and release the old allocation.

Nanopb does not call `realloc()` with `size = 0`.

The function returns either `NULL` on failed allocation, or a valid pointer to an allocation of at least `size` bytes long.

#### Custom free() implementation
Releases a previously made allocation.
The `ptr` argument is the return value from a previous call to `realloc()`.

Nanopb does not call `free()` with `ptr = NULL`.

### pb_byte_t

Type used for storing byte-sized data, such as raw binary input and
bytes-type fields.

    typedef uint_least8_t pb_byte_t;

For most platforms this is equivalent to `uint8_t`. Some platforms
however do not support 8-bit variables, and on those platforms 16 or 32
bits need to be used for each byte.

### pb_tag_t

Type used for storing field tag numbers.
By default this is `uint32_t`:

    typedef uint32_t pb_tag_t;

If `PB_NO_LARGEMSG` build option is given, then `uint_least16_t` is used instead.
This saves some stack space and is sufficient due to the 4095 tag number limit for small messages.

### pb_size_t

Type used for storing stream, field and array sizes.tag numbers and sizes of message fields.
By default this is equivalent to `size_t`:

    typedef size_t pb_size_t;

If `PB_NO_LARGEMSG` build option is given, then `uint_least16_t` is used instead.
This saves some stack space and is sufficient due to the 4 kB limit for small messages.

Optionally `PB_SIZE_T_OVERRIDE` can be used to customize the type.
For example, `uint8_t` could be used for processing tiny messages on a 8-bit platform.

### pb_fieldidx_t

Type used for storing index of a field in the protobuf message descriptors.
Equivalent to `uint_least16_t`:

    typedef uint_least16_t pb_fieldidx_t;

The value is multiplied by `5` when accessing large format descriptors.
This makes a 16-bit type sufficient for up to 13000 fields.
According to Google's [Proto Limits](https://protobuf.dev/programming-guides/proto-limits/),
typical protobuf implementations are limited to about 4000 fields per message, so this should be sufficient.

### pb_type_t

Type used to store the type of each field, to control the
encoder/decoder behaviour.

    typedef uint_least16_t pb_type_t;

The macros `PB_LTYPE()`, `PB_HTYPE()` and `PB_ATYPE()` can be used to access individual components of the field type.

The low-order nibble of the enumeration values defines the function that
can be used for encoding and decoding the field data:

| LTYPE identifier                 |Value  |Storage format
| ---------------------------------|-------|------------------------------------------------
| `PB_LTYPE_BOOL`                  |0x00   |Boolean.
| `PB_LTYPE_VARINT`                |0x01   |Integer.
| `PB_LTYPE_UVARINT`               |0x02   |Unsigned integer.
| `PB_LTYPE_SVARINT`               |0x03   |Integer, zigzag encoded.
| `PB_LTYPE_FIXED32`               |0x04   |32-bit integer or floating point.
| `PB_LTYPE_FIXED64`               |0x05   |64-bit integer or floating point.
| `PB_LTYPE_LAST_PACKABLE`         |0x05   |LTYPEs up to this can be stored in a [packed](https://protobuf.dev/programming-guides/encoding/#packed) array
| `PB_LTYPE_BYTES`                 |0x06   |Structure with `pb_size_t` field and byte array.
| `PB_LTYPE_STRING`                |0x07   |Null-terminated string.
| `PB_LTYPE_SUBMESSAGE`            |0x08   |Submessage structure.
| `PB_LTYPE_SUBMSG_W_CB`           |0x09   |Submessage with pre-decoding callback.
| `PB_LTYPE_EXTENSION`             |0x0A   |Pointer to `pb_extension_t`.
| `PB_LTYPE_FIXED_LENGTH_BYTES`    |0x0B   |Inline `pb_byte_t` array of fixed size.

The bits 4-5 define whether the field is required, optional or repeated.
There are separate definitions for semantically different modes, even
though some of them share values and are distinguished based on values
of other fields:

 |HTYPE identifier     |Value  |Field handling
 |---------------------|-------|--------------------------------------------------------------------------------------------
 |`PB_HTYPE_REQUIRED`  |0x00   |Verify that field exists in decoded message.
 |`PB_HTYPE_OPTIONAL`  |0x10   |Use separate `has_<field>` boolean to specify whether the field is present.
 |`PB_HTYPE_SINGULAR`  |0x10   |Proto3 field, which is present when its value is non-zero.
 |`PB_HTYPE_REPEATED`  |0x20   |A repeated field with preallocated array. Separate `<field>_count` for number of items.
 |`PB_HTYPE_FIXARRAY`  |0x20   |A repeated field that has constant length.
 |`PB_HTYPE_ONEOF`     |0x30   |Oneof-field, only one of each group can be present.

The bits 6-7 define the how the storage for the field is allocated:

|ATYPE identifier     |Value  |Allocation method
|---------------------|-------|--------------------------------------------------------------------------------------------
|`PB_ATYPE_STATIC`    |0x00   |Statically allocated storage in the structure.
|`PB_ATYPE_POINTER`   |0x80   |Dynamically allocated storage. Struct field contains a pointer to the storage.
|`PB_ATYPE_CALLBACK`  |0x40   |A field with dynamic storage size. Struct field contains a pointer to a callback function.

Upper bits `8-10` are available in the large-format message descriptors, but they are reserved for future use.

### pb_msgdesc_t

Autogenerated structure that contains information about a message and
pointers to the field descriptors. Use functions defined in
`pb_common.h` to process the field information.

    typedef struct pb_msgdesc_s pb_msgdesc_t;
    struct pb_msgdesc_s {
        pb_size_t struct_size;
        pb_fieldidx_t field_count;
        pb_fieldidx_t required_field_count;
        pb_tag_t largest_tag;
        pb_msgflag_t msg_flags;
        const uint32_t *field_info;
        const pb_msgdesc_t * const * submsg_info;
        const pb_byte_t *default_value;

        bool (*field_callback)(pb_decode_ctx_t *istream, pb_encode_ctx_t *ostream, const pb_field_iter_t *field);
    };

|                 |                                                        |
|-----------------|--------------------------------------------------------|
|`struct_size`    | Memory size of the associated structure, in bytes.
|`field_count`    | Total number of fields in the message.
|`required_field_count` | Number of fields that have the proto2 `required` specifier.
|`largest_tag`    | Largest tag number used in the fields of the message.
|`msg_flags`      | Informs whether the message contains e.g. pointer fields.
|`field_info`     | Pointer to compact representation of the field information.
|`submsg_info`    | Pointer to array of pointers to descriptors for submessages.
|`default_value`  | Default values for this message as an encoded protobuf message.
|`field_callback` | Function used to handle all callback fields in this message. By default `pb_default_field_callback()`  which loads per-field callbacks from a `pb_callback_t` structure.

### pb_msgflag_t

Message flags are stored in the `pb_msgdesc_t` and provide high-level metadata about the message.
They are primarily used for nanopb code to skip unnecessary steps if the message does not contain particular features.

User code can use the flags to e.g. check if the message contains any pointers that need to be released.

Currently defined message flags are listed below. Flags with `PB_MSGFLAG_R_` are recursive, they
get set on the parent message if they apply to any submessages.

|                                |                                                                |
|--------------------------------|----------------------------------------------------------------|
| `PB_MSGFLAG_LARGEDESC`         | Descriptor uses the 5 words per field descriptor format.
| `PB_MSGFLAG_EXTENSIBLE`        | Message contains proto2 extension fields.
| `PB_MSGFLAG_R_HAS_PTRS`        | Message or its submessages contain pointer fields.
| `PB_MSGFLAG_R_HAS_DEFVAL`      | Message or its submessages have default values.
| `PB_MSGFLAG_R_HAS_CBS`         | Message or its submessages have callback fields.
| `PB_MSGFLAG_R_HAS_EXTS`        | Message or its submessages have extension fields.

### pb_field_iter_t

Describes a single structure field with memory position in relation to
others. The field information is stored in a compact format and loaded
into `pb_field_iter_t` by the functions defined in [pb_common.h](reference_pb_common_h.md),
such as `pb_field_iter_next()`.

    typedef struct pb_field_iter_s pb_field_iter_t;
    struct pb_field_iter_s {
        const pb_msgdesc_t *descriptor;
        void *message;

        pb_fieldidx_t index;
        pb_fieldidx_t required_field_index;
        pb_fieldidx_t submessage_index;
        pb_fieldidx_t field_info_index;

        pb_tag_t tag;
        pb_size_t data_size;
        pb_size_t array_size;
        pb_type_t type;

        void *pField;
        void *pData;
        void *pSize;

        const pb_msgdesc_t *submsg_desc;
    };

|                      |                                                        |
|----------------------|--------------------------------------------------------|
| descriptor           | Pointer to `pb_msgdesc_t` for the message that contains this field.
| message              | Pointer to the start of the message structure.
| index                | Index of the field inside the message
| required_field_index | Index that counts only the required fields
| submessage_index     | Index that counts only submessages
| field_info_index     | Index to the internal `field_info` array
| tag                  | Tag number defined in `.proto` file for this field.
| data_size            | `sizeof()` of the field in the structure. For repeated fields this is for a single array entry.
| array_size           | Maximum number of items in a statically allocated array.
| type                 | Protobuf data type of the field.
| pField               | Pointer to the field storage in the structure.
| pData                | Pointer to data contents. For arrays and pointers this can be different than `pField`.
| pSize                | Pointer to count or has field, or NULL if this field doesn't have such.
| submsg_desc          | For submessage fields, points to the descriptor for the submessage.


### pb_bytes_array_t

An byte array with a field for storing the length:

    typedef struct {
        pb_size_t size;
        pb_byte_t bytes[1];
    } pb_bytes_array_t;

In an actual array, the length of `bytes` is set by [generator options](reference_generator.md).
The macros `PB_BYTES_ARRAY_T()` and `PB_BYTES_ARRAY_T_ALLOCSIZE()`
are used to allocate variable length storage for bytes fields.

### pb_callback_t

Part of a message structure, for fields with type `PB_HTYPE_CALLBACK`:

    typedef struct pb_callback_s pb_callback_t;
    struct pb_callback_s {
        union {
            bool (*decode)(pb_decode_ctx_t *stream, const pb_field_iter_t *field, void **arg);
            bool (*encode)(pb_encode_ctx_t *stream, const pb_field_iter_t *field, void * const *arg);
        } funcs;

        void *arg;
    };

A pointer to the *arg* is passed to the callback when calling. It can be
used to store any information that the callback might need. Note that
this is a double pointer. If you set `field.arg` to point to
`&data` in your main code, in the callback you can access it like this:

    myfunction(*arg);           /* Gives pointer to data as argument */
    myfunction(*(data_t*)*arg); /* Gives value of data as argument */
    *arg = newdata;             /* Alters value of field.arg in structure */

When calling `pb_encode()`, `funcs.encode` is used, and
similarly when calling `pb_decode()`, `funcs.decode` is used.
The function pointers are stored in the same memory location but are of
incompatible types. You can set the function pointer to `NULL` to skip the
field.

### pb_wire_type_t

Protocol Buffers wire types. These are used with `pb_encode_tag()`:

    typedef enum {
        PB_WT_VARINT = 0,
        PB_WT_64BIT  = 1,
        PB_WT_STRING = 2,
        PB_WT_32BIT  = 5
    } pb_wire_type_t;

### pb_extension_t

Ties together the extension field type and the storage for the field
value. For message structs that have extensions, the generator will
add a `pb_extension_t*` field. It should point to a linked list of
extensions.

    typedef struct {
        const pb_msgdesc_t *type;
        void *dest;
        pb_extension_t *next;
        bool found;
    } pb_extension_t;

|                      |                                                        |
|----------------------|--------------------------------------------------------|
| type                 | Pointer to the automatically generated descriptor for the extension field contents.
| dest                 | Pointer to the variable that stores the field value.
| next                 | Pointer to the next extension handler, or `NULL` for last handler.
| found                | Decoder sets this to true if the extension was found.

## Utility macros

### PB_GET_ERROR

Get the current error message from a context, or a placeholder string if there is no error message:

    #define PB_GET_ERROR(ctx) (string expression)

The `ctx` can have type `pb_encode_ctx_t*` or `pb_decode_ctx_t*`.

If the error message is not set, `"(none)"` is returned.
If error messages are disabled with `PB_NO_ERRMSG`, `"(errmsg disabled)"` is returned.

This should be used for printing errors, for example:

    if (!pb_decode(&ctx, ...))
    {
        printf("Decode failed: %s\n", PB_GET_ERROR(&ctx));
    }

The macro only returns pointers to constant strings (in code memory), so
that there is no need to release the returned pointer.

### PB_SET_ERROR

Set the error message if it has not been set yet.

    #define PB_SET_ERROR(ctx, msg) (set errmsg if it is null)

If multiple errors occur (for example IO error followed by failed decoding), the first error message will persist.

The `msg` parameter must be a constant string.

### PB_RETURN_ERROR

Set the error message if it is not already set, and return false:

    #define PB_RETURN_ERROR(ctx, msg) (PB_SET_ERROR() and returns false)

This should be used to handle error conditions inside nanopb functions
and user callback functions:

    if (error_condition)
    {
        PB_RETURN_ERROR(ctx, "something went wrong");
    }

The `msg` parameter must be a constant string.

## PB_READ_ERROR

Sentinel value used by `pb_decode_ctx_t` stream callbacks to indicate error condition.

    #define PB_READ_ERROR PB_SIZE_MAX

The largest value representable by `pb_size_t` is reserved for error indication by `pb_init_decode_ctx_for_callback()` limiting the stream length to `PB_SIZE_MAX - 1`.

### PB_BIND

This macro generates the [pb_msgdesc_t](#pb_msgdesc_t) and associated
arrays, based on a list of fields in [X-macro](https://en.wikipedia.org/wiki/X_Macro) format. :

    #define PB_BIND(msgname, structname, width) ...

|                      |                                                        |
|----------------------|--------------------------------------------------------|
| msgname              | Name of the message type. Expects `msgname_FIELDLIST` macro to exist.
| structname           | Name of the C structure to bind to.
| width                | `S` for small messages up to 4kB, `L` for large messages.

This macro is automatically invoked inside the autogenerated `.pb.c`
files. User code can also call it to bind message types with custom
structures or class types.

### pb_arraysize()

Get the number of entries in an array-type member of a structure.
Example usage:

    MyMessage msg;
    for (int i = 0; i < pb_arraysize(MyMessage, my_repeated_int); i++)
    {
        printf("%d\n", msg.my_repeated_int[i]);
    }

### PB_CONST_CAST

Nanopb encoding functions only read from the message structure and thus take `const` pointer to it.
But internally a part of the logic is shared, resulting in a need to cast away the `const` qualifier.
This is done with care, so that no writes are done through the pointer.

The macro implementation uses `uintptr_t` as an intermediate type to avoid warnings on most compilers:

    #define PB_CONST_CAST(x) ((void*)(uintptr_t)(x))

### PB_UNUSED

This macro is used to supress compiler warnings about unused function
arguments. Typically needed when disabled features result in some arguments being ignored.

    #define PB_UNUSED(x) (void)(x)

### PB_OPT_ASSERT

Optional assertions that are useful for early detection of problems.
This is in particular targeted for nanopb developers, though the assertions do not have significant runtime cost either.
All [security properties](security.md) are fulfilled even if assertions are disabled using `PB_NO_OPT_ASSERT`.

    #define PB_OPT_ASSERT(cond) assert(cond)

### PB_STATIC_ASSERT

Compile-time assertions, using either C11 `_Static_assert` keyword or the negative-size-array mechanism in C99.

    #define PB_STATIC_ASSERT(COND,MSG) _Static_assert(COND,#MSG);

The implementation of static assertions can be controlled using build options `PB_NO_STATIC_ASSERT` and `PB_C99_STATIC_ASSERT`.
