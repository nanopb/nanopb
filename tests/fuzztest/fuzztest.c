/* Fuzz testing for the nanopb core.
 * Attempts to verify all the properties defined in the security model document.
 *
 * This program can run in three configurations:
 * - Standalone fuzzer, generating its own inputs and testing against them.
 * - Fuzzing target, reading input on stdin.
 * - LLVM libFuzzer target, taking input as a function argument.
 */

#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc_wrappers.h>
#include "random_data.h"
#include "validation.h"
#include "flakystream.h"
#include "test_helpers.h"
#include "alltypes_static.pb.h"
#include "alltypes_pointer.pb.h"
#include "alltypes_callback.pb.h"
#include "alltypes_proto3_static.pb.h"
#include "alltypes_proto3_pointer.pb.h"

/* Longer buffer size allows hitting more branches, but lowers performance. */
#if defined(LLVMFUZZER)
static size_t g_bufsize = 256*1024;
#elif defined(__AVR__)
static size_t g_bufsize = 2048;
#else
static size_t g_bufsize = 4096;
#endif

static bool do_decode(const uint8_t *buffer, size_t msglen, size_t structsize, const pb_msgdesc_t *msgtype, unsigned flags, bool assert_success)
{
    bool status;
    pb_istream_t stream;
    size_t initial_alloc_count = get_alloc_count();
    void *msg = malloc_with_check(structsize);
    
    memset(msg, 0, structsize);
    stream = pb_istream_from_buffer(buffer, msglen);
    status = pb_decode_ex(&stream, msgtype, msg, flags);

    if (status)
    {
        validate_message(msg, structsize, msgtype);
    }

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
    }

    pb_release(msgtype, msg);
    free_with_check(msg);
    assert(get_alloc_count() == initial_alloc_count);
    
    return status;
}

static bool do_stream_decode(const uint8_t *buffer, size_t msglen, size_t fail_after, size_t structsize, const pb_msgdesc_t *msgtype, bool assert_success)
{
    bool status;
    flakystream_t stream;
    size_t initial_alloc_count = get_alloc_count();
    void *msg = malloc_with_check(structsize);

    memset(msg, 0, structsize);
    flakystream_init(&stream, buffer, msglen, fail_after);
    status = pb_decode(&stream.stream, msgtype, msg);

    if (status)
    {
        validate_message(msg, structsize, msgtype);
    }

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream.stream));
        assert(status);
    }

    pb_release(msgtype, msg);
    free_with_check(msg);
    assert(get_alloc_count() == initial_alloc_count);

    return status;
}

static int g_sentinel;

static bool field_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    assert(*arg == &g_sentinel);
    return pb_read(stream, NULL, stream->bytes_left);
}

static bool do_callback_decode(const uint8_t *buffer, size_t msglen, bool assert_success)
{
    bool status;
    pb_istream_t stream;
    size_t initial_alloc_count = get_alloc_count();
    alltypes_callback_AllTypes *msg = malloc_with_check(sizeof(alltypes_callback_AllTypes));

    memset(msg, 0, sizeof(alltypes_callback_AllTypes));
    stream = pb_istream_from_buffer(buffer, msglen);

    msg->rep_int32.funcs.decode = &field_callback;
    msg->rep_int32.arg = &g_sentinel;
    msg->rep_string.funcs.decode = &field_callback;
    msg->rep_string.arg = &g_sentinel;
    msg->rep_farray.funcs.decode = &field_callback;
    msg->rep_farray.arg = &g_sentinel;
    msg->req_limits.int64_min.funcs.decode = &field_callback;
    msg->req_limits.int64_min.arg = &g_sentinel;
    msg->cb_oneof.funcs.decode = &field_callback;
    msg->cb_oneof.arg = &g_sentinel;

    status = pb_decode(&stream, alltypes_callback_AllTypes_fields, msg);

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
    }

    pb_release(alltypes_callback_AllTypes_fields, msg);
    free_with_check(msg);
    assert(get_alloc_count() == initial_alloc_count);

    return status;
}

/* Do a decode -> encode -> decode -> encode roundtrip */
static void do_roundtrip(const uint8_t *buffer, size_t msglen, size_t structsize, const pb_msgdesc_t *msgtype)
{
    bool status;
    uint8_t *buf2 = malloc_with_check(g_bufsize);
    uint8_t *buf3 = malloc_with_check(g_bufsize);
    size_t msglen2, msglen3;
    void *msg = malloc_with_check(structsize);

    /* For proto2 types, we also test extension fields */
    alltypes_static_TestExtension extmsg = alltypes_static_TestExtension_init_zero;
    pb_extension_t ext = pb_extension_init_zero;
    pb_extension_t **ext_field = NULL;
    ext.type = &alltypes_static_TestExtension_testextension;
    ext.dest = &extmsg;
    ext.next = NULL;

    if (msgtype == alltypes_static_AllTypes_fields)
    {
        ext_field = &((alltypes_static_AllTypes*)msg)->extensions;
    }
    else if (msgtype == alltypes_pointer_AllTypes_fields)
    {
        ext_field = &((alltypes_pointer_AllTypes*)msg)->extensions;
    }
    
    /* Decode and encode the input data.
     * This will bring it into canonical format.
     */
    {
        pb_istream_t stream = pb_istream_from_buffer(buffer, msglen);
        memset(msg, 0, structsize);
        if (ext_field) *ext_field = &ext;
        status = pb_decode(&stream, msgtype, msg);
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);

        validate_message(msg, structsize, msgtype);
    }
    
    {
        pb_ostream_t stream = pb_ostream_from_buffer(buf2, g_bufsize);
        status = pb_encode(&stream, msgtype, msg);
        if (!status) fprintf(stderr, "pb_encode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
        msglen2 = stream.bytes_written;
    }
    
    pb_release(msgtype, msg);

    /* Then decode and encode again. Result should remain the same. */
    {
        pb_istream_t stream = pb_istream_from_buffer(buf2, msglen2);
        memset(msg, 0, structsize);
        if (ext_field) *ext_field = &ext;
        status = pb_decode(&stream, msgtype, msg);
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);

        validate_message(msg, structsize, msgtype);
    }
    
    {
        pb_ostream_t stream = pb_ostream_from_buffer(buf3, g_bufsize);
        status = pb_encode(&stream, msgtype, msg);
        if (!status) fprintf(stderr, "pb_encode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
        msglen3 = stream.bytes_written;
    }
    
    assert(msglen2 == msglen3);
    assert(memcmp(buf2, buf3, msglen2) == 0);
    
    pb_release(msgtype, msg);
    free_with_check(msg);
    free_with_check(buf2);
    free_with_check(buf3);
}

void do_roundtrips(const uint8_t *data, size_t size, bool expect_valid)
{
    size_t initial_alloc_count = get_alloc_count();

    /* Check decoding as static fields */
    if (do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, 0, expect_valid))
    {
        /* Any message that is decodeable as proto2 should be decodeable as proto3 */
        do_roundtrip(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields);
        do_roundtrip(data, size, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields);

        /* Test successful decoding also when using a stream */
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, true);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, true);

        /* Test callbacks */
        do_callback_decode(data, size, true);
    }
    else if (do_decode(data, size, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, 0, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields);
    }

    /* Check decoding as pointer fields */
    if (do_decode(data, size, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, 0, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields);
        do_roundtrip(data, size, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields);

        /* Test successful decoding also when using a stream */
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, true);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, true);
    }
    else if (do_decode(data, size, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, 0, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields);
    }
    
    /* Test decoding on a failing stream */
    do_stream_decode(data, size, size - 16, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, false);
    do_stream_decode(data, size, size - 16, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, false);
    do_stream_decode(data, size, size - 16, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, false);
    do_stream_decode(data, size, size - 16, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, false);

    /* Test pb_decode_ex() modes */
    do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, PB_DECODE_NOINIT, false);
    do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, PB_DECODE_DELIMITED, false);
    do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, PB_DECODE_NULLTERMINATED, false);

    /* Test callbacks also when message is not valid */
    do_callback_decode(data, size, false);

    assert(get_alloc_count() == initial_alloc_count);
}

/* Fuzzer stub for Google OSSFuzz integration */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* In some cases, when we re-encode input it can expand close to 2x.
     * This is because 0xFFFFFFFF in int32 is decoded as -1, but the
     * canonical encoding for it is 0xFFFFFFFFFFFFFFFF.
     * Thus we reserve double the space for intermediate buffers.
     */
    if (size > g_bufsize / 2)
        return 0;

    do_roundtrips(data, size, false);

    return 0;
}

#ifndef LLVMFUZZER

static bool generate_base_message(uint8_t *buffer, size_t *msglen)
{
    pb_ostream_t stream;
    bool status;
    static const alltypes_static_AllTypes initval = alltypes_static_AllTypes_init_default;

    /* Allocate a message and fill it with defaults */
    alltypes_static_AllTypes *msg = malloc_with_check(sizeof(alltypes_static_AllTypes));
    memcpy(msg, &initval, sizeof(initval));

    /* Apply randomness to the data before encoding */
    while (rand_int(0, 7))
        rand_mess((uint8_t*)msg, sizeof(alltypes_static_AllTypes));

    msg->extensions = NULL;

    stream = pb_ostream_from_buffer(buffer, g_bufsize);
    status = pb_encode(&stream, alltypes_static_AllTypes_fields, msg);
    assert(stream.bytes_written <= g_bufsize);
    assert(stream.bytes_written <= alltypes_static_AllTypes_size);
    
    *msglen = stream.bytes_written;
    pb_release(alltypes_static_AllTypes_fields, msg);
    free_with_check(msg);
    
    return status;
}

/* Stand-alone fuzzer iteration, generates random data itself */
static void run_iteration()
{
    uint8_t *buffer = malloc_with_check(g_bufsize);
    size_t msglen;
    
    /* Fill the whole buffer with noise, to prepare for length modifications */
    rand_fill(buffer, g_bufsize);

    if (generate_base_message(buffer, &msglen))
    {
        rand_protobuf_noise(buffer, g_bufsize, &msglen);
    
        /* At this point the message should always be valid */
        do_roundtrips(buffer, msglen, true);
        
        /* Apply randomness to the encoded data */
        while (rand_bool())
            rand_mess(buffer, g_bufsize);
        
        /* Apply randomness to encoded data length */
        if (rand_bool())
            msglen = rand_int(0, g_bufsize);
        
        /* In this step the message may be valid or invalid */
        do_roundtrips(buffer, msglen, false);
    }
    
    free_with_check(buffer);
    assert(get_alloc_count() == 0);
}

int main(int argc, char **argv)
{
    int i;
    int iterations;

    if (argc >= 2)
    {
        /* Run in stand-alone mode */
        random_set_seed(strtoul(argv[1], NULL, 0));
        iterations = (argc >= 3) ? atol(argv[2]) : 10000;

        for (i = 0; i < iterations; i++)
        {
            printf("Iteration %d/%d, seed %lu\n", i, iterations, (unsigned long)random_get_seed());
            run_iteration();
        }
    }
    else
    {
        /* Run as a stub for afl-fuzz and similar */
        uint8_t *buffer;
        size_t msglen;

        buffer = malloc_with_check(g_bufsize);

        SET_BINARY_MODE(stdin);
        msglen = fread(buffer, 1, g_bufsize, stdin);
        LLVMFuzzerTestOneInput(buffer, msglen);

        free_with_check(buffer);
    }
    
    return 0;
}
#endif
