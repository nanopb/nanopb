/* Regression test for issue #1200: decoding a message that carries a custom
 * pb_extension_type_t (with its own decode/encode callbacks) used to crash in
 * pb_field_iter_begin_extension(), because pb_field_set_to_default() and
 * pb_release_single_field() unconditionally treated type->arg as a
 * pb_msgdesc_t. This test covers both the custom-extension path (the bug) and
 * the default-extension path (decode/encode NULL, arg is a descriptor), which
 * must keep working.
 *
 * Hand-written descriptors, no protoc required. Build and run with:
 *   cc -I. pb_common.c pb_decode.c custom_extension.c -o custom_extension
 *   cc -DPB_ENABLE_MALLOC -I. pb_common.c pb_decode.c custom_extension.c ...
 */

#include <stdio.h>
#include <string.h>
#include "pb_decode.h"

/* ---- Custom extension scenario (issue #1200) -------------------------------
 * message Msg { optional int32 x = 1; extensions 100 to 199; }
 * The descriptor below is what nanopb_generator writes for this message. */

typedef struct _Msg { bool has_x; int32_t x; pb_extension_t *extensions; } Msg;

#define Msg_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, INT32,    x,                 1) \
X(a, CALLBACK, OPTIONAL, EXTENSION, extensions,      100)
#define Msg_CALLBACK NULL
#define Msg_DEFAULT NULL
extern const pb_msgdesc_t Msg_msg;
PB_BIND(Msg, Msg, 2)

static uint64_t seen_value;
static int seen;

static bool my_decode(pb_istream_t *stream, pb_extension_t *ext, uint32_t tag, pb_wire_type_t wire_type)
{
    if (tag != 100 || wire_type != PB_WT_VARINT)
        return true;                    /* not ours: leave it */
    ext->found = true;
    seen = 1;
    return pb_decode_varint(stream, &seen_value);
}

static bool my_encode(pb_ostream_t *stream, const pb_extension_t *ext)
{
    (void) stream;
    (void) ext;
    return true;
}

/* decode/encode non-NULL: type->arg is a free field, not a descriptor. */
static const pb_extension_type_t my_type = {my_decode, my_encode, NULL};

static int test_custom_extension(void)
{
    /* field 1 (x) varint = 7, field 100 varint = 42 */
    static const pb_byte_t input[] = {0x08, 0x07, 0xa0, 0x06, 0x2a};
    /* field 1 (x) only, no extension field: exercises the found reset path */
    static const pb_byte_t input_empty[] = {0x08, 0x07};
    pb_extension_t ext = {&my_type, NULL, NULL, false};
    Msg msg;
    pb_istream_t stream;
    bool ok;

    seen = 0;
    seen_value = 0;
    memset(&msg, 0, sizeof msg);
    msg.extensions = &ext;
    stream = pb_istream_from_buffer(input, sizeof input);
    ok = pb_decode(&stream, &Msg_msg, &msg);
    if (!ok || !seen || seen_value != 42 || !ext.found)
        return 1;

    /* Decoding again without the extension must reset ext->found in
     * pb_field_set_to_default(). A custom type's arg is a free field, so it is
     * skipped as a descriptor, but found must still be cleared. */
    stream = pb_istream_from_buffer(input_empty, sizeof input_empty);
    ok = pb_decode(&stream, &Msg_msg, &msg);
#ifdef PB_ENABLE_MALLOC
    pb_release(&Msg_msg, &msg);
#endif
    return (ok && !ext.found) ? 0 : 1;
}

/* ---- Default extension scenario (regression guard) -------------------------
 * extend Outer { optional int32 value = 100; }
 * decode/encode NULL: type->arg points to a pb_msgdesc_t. */

typedef struct _ExtValue { bool has_value; int32_t value; } ExtValue;

#define ExtValue_FIELDLIST(X, a) \
X(a, STATIC, OPTIONAL, INT32, value, 100)
#define ExtValue_CALLBACK NULL
#define ExtValue_DEFAULT NULL
extern const pb_msgdesc_t ExtValue_msg;
PB_BIND(ExtValue, ExtValue, 2)

typedef struct _Outer { pb_extension_t *extensions; } Outer;

#define Outer_FIELDLIST(X, a) \
X(a, CALLBACK, OPTIONAL, EXTENSION, extensions, 100)
#define Outer_CALLBACK NULL
#define Outer_DEFAULT NULL
extern const pb_msgdesc_t Outer_msg;
PB_BIND(Outer, Outer, 2)

static const pb_extension_type_t ExtValue_type = {NULL, NULL, &ExtValue_msg};

static int test_default_extension(void)
{
    /* field 100 varint = 42 */
    static const pb_byte_t input[] = {0xa0, 0x06, 0x2a};
    ExtValue ext_value = {false, 0};
    pb_extension_t ext = {&ExtValue_type, &ext_value, NULL, false};
    Outer outer = {&ext};
    pb_istream_t stream = pb_istream_from_buffer(input, sizeof input);
    bool ok;

    ok = pb_decode(&stream, &Outer_msg, &outer);
#ifdef PB_ENABLE_MALLOC
    pb_release(&Outer_msg, &outer);
#endif
    return (ok && ext.found && ext_value.value == 42) ? 0 : 1;
}

int main(void)
{
    int status = 0;

    status |= test_custom_extension();
    status |= test_default_extension();

    if (status)
        printf("custom_extension: FAILED\n");
    else
        printf("custom_extension: ok\n");

    return status;
}
