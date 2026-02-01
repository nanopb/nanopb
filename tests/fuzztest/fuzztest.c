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
#ifndef FUZZTEST_BUFSIZE
#define FUZZTEST_BUFSIZE 256*1024
#endif
#ifndef FUZZTEST_MAX_STANDALONE_BUFSIZE
#define FUZZTEST_MAX_STANDALONE_BUFSIZE 16384
#endif
static size_t g_bufsize = FUZZTEST_BUFSIZE;

/* Focusing on a single test case at a time improves fuzzing performance.
 * If no test case is specified, enable all tests.
 */
#if !defined(FUZZTEST_PROTO2_STATIC) && \
    !defined(FUZZTEST_PROTO3_STATIC) && \
    !defined(FUZZTEST_PROTO2_POINTER) && \
    !defined(FUZZTEST_PROTO3_POINTER) && \
    !defined(FUZZTEST_IO_ERRORS)
#define FUZZTEST_PROTO2_STATIC
#define FUZZTEST_PROTO3_STATIC

#if !PB_NO_MALLOC
#define FUZZTEST_PROTO2_POINTER
#define FUZZTEST_PROTO3_POINTER
#endif

#define FUZZTEST_IO_ERRORS
#endif

static uint32_t xor32_checksum(const void *data, size_t len)
{
    const uint8_t *buf = (const uint8_t*)data;
    uint32_t checksum = 1234;
    for (; len > 0; len--)
    {
        checksum ^= checksum << 13;
        checksum ^= checksum >> 17;
        checksum ^= checksum << 5;
        checksum += *buf++;
    }
    return checksum;
}

static bool do_decode(const uint8_t *buffer, size_t msglen, size_t structsize,
    const pb_msgdesc_t *msgtype, pb_decode_ctx_flags_t flags, bool assert_success)
{
    bool status;
    pb_decode_ctx_t stream;
    size_t initial_alloc_count = get_alloc_count();
    uint8_t *buf2 = malloc_with_check(g_bufsize); /* This is just to match the amount of memory allocations in do_roundtrips(). */
    void *msg = malloc_with_check(structsize);
    alltypes_static_TestExtension extmsg = alltypes_static_TestExtension_init_zero;
    pb_extension_t ext = pb_extension_init_zero;
    assert(msg);

    memset(msg, 0, structsize);
    ext.type = &alltypes_static_TestExtension_testextension;
    ext.dest = &extmsg;
    ext.next = NULL;

    if (msgtype == alltypes_static_AllTypes_fields)
    {
        ((alltypes_static_AllTypes*)msg)->extensions = &ext;
    }
    else if (msgtype == alltypes_pointer_AllTypes_fields)
    {
        ((alltypes_pointer_AllTypes*)msg)->extensions = &ext;
    }

    pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);
    stream.flags |= flags;
    status = pb_decode_s(&stream, msgtype, msg, structsize);

    if (status)
    {
        validate_message(msg, structsize, msgtype);
    }

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
    }

    if (status)
    {
        /* On error return, pb_release() should be called automatically. */
        pb_release_s(&stream, msgtype, msg, structsize);
    }

    free_with_check(msg);
    free_with_check(buf2);
    assert(get_alloc_count() == initial_alloc_count);
    
    return status;
}

static bool do_stream_decode(const uint8_t *buffer, size_t msglen, size_t fail_after, size_t structsize,
    const pb_msgdesc_t *msgtype, pb_decode_ctx_flags_t flags, bool assert_success)
{
#if PB_NO_STREAM_CALLBACK
    return true;
#else
    bool status;
    flakystream_t stream;
    size_t initial_alloc_count = get_alloc_count();
    void *msg = malloc_with_check(structsize);
    assert(msg);

    memset(msg, 0, structsize);

    uint8_t tmpbuf[27];
    flakystream_init(&stream, buffer, msglen, fail_after, tmpbuf, sizeof(tmpbuf));
    stream.stream.flags |= flags;
    status = pb_decode_s(&stream.stream, msgtype, msg, structsize);

    if (status)
    {
        validate_message(msg, structsize, msgtype);
    }

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream.stream));
        assert(status);
    }

    if (status)
    {
        /* On error return, pb_release() should be called automatically. */
        pb_release_s(&stream.stream, msgtype, msg, structsize);
    }

    free_with_check(msg);
    assert(get_alloc_count() == initial_alloc_count);

    return status;
#endif
}

static int g_sentinel;

static bool field_callback(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    assert(stream);
    assert(field);
    assert(*arg == &g_sentinel);
    return pb_read(stream, NULL, stream->bytes_left);
}

static bool submsg_callback(pb_decode_ctx_t *stream, const pb_field_t *field, void **arg)
{
    assert(stream);
    assert(field);
    assert(*arg == &g_sentinel);
    return true;
}

bool do_callback_decode(const uint8_t *buffer, size_t msglen, pb_decode_ctx_flags_t flags, bool assert_success)
{
    bool status;
    pb_decode_ctx_t stream;
    size_t initial_alloc_count = get_alloc_count();
    alltypes_callback_AllTypes *msg = malloc_with_check(sizeof(alltypes_callback_AllTypes));
    assert(msg);

    memset(msg, 0, sizeof(alltypes_callback_AllTypes));
    pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);
    stream.flags |= flags;

    msg->rep_int32.funcs.decode = &field_callback;
    msg->rep_int32.arg = &g_sentinel;
    msg->rep_string.funcs.decode = &field_callback;
    msg->rep_string.arg = &g_sentinel;
    msg->rep_farray.funcs.decode = &field_callback;
    msg->rep_farray.arg = &g_sentinel;
    msg->req_limits.int64_min.funcs.decode = &field_callback;
    msg->req_limits.int64_min.arg = &g_sentinel;
    msg->cb_oneof.funcs.decode = &submsg_callback;
    msg->cb_oneof.arg = &g_sentinel;

    status = pb_decode(&stream, alltypes_callback_AllTypes_fields, msg);

    if (assert_success)
    {
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
    }

    pb_release(&stream, alltypes_callback_AllTypes_fields, msg);
    free_with_check(msg);
    assert(get_alloc_count() == initial_alloc_count);

    return status;
}

/* Do a decode -> encode -> decode -> encode roundtrip */
void do_roundtrip(const uint8_t *buffer, size_t msglen, size_t structsize,
    const pb_msgdesc_t *msgtype,
    pb_decode_ctx_flags_t dec_flags, pb_encode_ctx_flags_t enc_flags)
{
    bool status;
    uint32_t checksum2, checksum3;
    size_t msglen2, msglen3;
    uint8_t *buf2 = malloc_with_check(g_bufsize);
    void *msg = malloc_with_check(structsize);

    /* For proto2 types, we also test extension fields */
    alltypes_static_TestExtension extmsg = alltypes_static_TestExtension_init_zero;
    pb_extension_t ext = pb_extension_init_zero;
    pb_extension_t **ext_field = NULL;
    ext.type = &alltypes_static_TestExtension_testextension;
    ext.dest = &extmsg;
    ext.next = NULL;

    assert(buf2 && msg);

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
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buffer, msglen);
        stream.flags |= dec_flags;
        memset(msg, 0, structsize);
        if (ext_field) *ext_field = &ext;
        status = pb_decode_s(&stream, msgtype, msg, structsize);
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);

        validate_message(msg, structsize, msgtype);
    }
    
    {
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buf2, g_bufsize);
        stream.flags |= enc_flags;
        status = pb_encode_s(&stream, msgtype, msg, structsize);

#if !PB_NO_ERRMSG
        // Some messages expand when re-encoding and might no longer fit
        // in the buffer.
        bool stream_full = (strcmp(PB_GET_ERROR(&stream), "stream full") == 0);
#else
        // We can't accurately know the error cause for testing when errmsg
        // is disabled.
        bool stream_full = (!status && stream.bytes_written > msglen);
#endif

        if (!status && !stream_full)
        {
            fprintf(stderr, "pb_encode: %s\n", PB_GET_ERROR(&stream));
            assert(status);
        }

        msglen2 = stream.bytes_written;
        checksum2 = xor32_checksum(buf2, msglen2);
    }
    
    pb_release_s(NULL, msgtype, msg, structsize);

    /* Then decode from canonical format and re-encode. Result should remain the same. */
    if (status)
    {
        pb_decode_ctx_t stream;
        pb_init_decode_ctx_for_buffer(&stream, buf2, msglen2);
        stream.flags |= dec_flags;
        memset(msg, 0, structsize);
        if (ext_field) *ext_field = &ext;
        status = pb_decode_s(&stream, msgtype, msg, structsize);
        if (!status) fprintf(stderr, "pb_decode: %s\n", PB_GET_ERROR(&stream));
        assert(status);

        validate_message(msg, structsize, msgtype);
    }
    
    if (status)
    {
        pb_encode_ctx_t stream;
        pb_init_encode_ctx_for_buffer(&stream, buf2, g_bufsize);
        stream.flags |= enc_flags;
        status = pb_encode_s(&stream, msgtype, msg, structsize);
        if (!status) fprintf(stderr, "pb_encode: %s\n", PB_GET_ERROR(&stream));
        assert(status);
        msglen3 = stream.bytes_written;
        checksum3 = xor32_checksum(buf2, msglen3);

        assert(msglen2 == msglen3);
        assert(checksum2 == checksum3);
    }
    
    pb_release_s(NULL, msgtype, msg, structsize);
    free_with_check(msg);
    free_with_check(buf2);
}

/* Run all enabled test cases for a given input */
void do_roundtrips(const uint8_t *data, size_t size, bool expect_valid)
{
    size_t initial_alloc_count = get_alloc_count();
    PB_UNUSED(expect_valid); /* Potentially unused depending on configuration */

    // Disable UTF-8 validation for fuzzing, because it leads to too many
    // non-useful message rejections. UTF-8 validation is subjected to fuzzing
    // in the IO_ERRORS delimited case below.
    pb_decode_ctx_flags_t dec_flags = PB_DECODE_CTX_FLAG_NO_VALIDATE_UTF8;
    pb_encode_ctx_flags_t enc_flags = PB_ENCODE_CTX_FLAG_NO_VALIDATE_UTF8;

#ifdef FUZZTEST_PROTO2_STATIC
    if (do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, dec_flags, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, dec_flags, enc_flags);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, dec_flags, true);
        do_callback_decode(data, size, dec_flags, true);
    }
#endif

#ifdef FUZZTEST_PROTO3_STATIC
    if (do_decode(data, size, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, dec_flags, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, dec_flags, enc_flags);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_proto3_static_AllTypes), alltypes_proto3_static_AllTypes_fields, dec_flags, true);
    }
#endif

#ifdef FUZZTEST_PROTO2_POINTER
    if (do_decode(data, size, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, dec_flags, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, dec_flags, enc_flags);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, dec_flags, true);
    }
#endif

#ifdef FUZZTEST_PROTO3_POINTER
    if (do_decode(data, size, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, dec_flags, expect_valid))
    {
        do_roundtrip(data, size, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, dec_flags, enc_flags);
        do_stream_decode(data, size, SIZE_MAX, sizeof(alltypes_proto3_pointer_AllTypes), alltypes_proto3_pointer_AllTypes_fields, dec_flags, true);
    }
#endif

#ifdef FUZZTEST_IO_ERRORS
    {
        size_t orig_max_alloc_bytes = get_max_alloc_bytes();
        /* Test decoding when error conditions occur.
         * The decoding will end either when running out of memory or when stream returns IO error.
         * Testing proto2 is enough for good coverage here, as it has a superset of the field types of proto3.
         */
        set_max_alloc_bytes(get_alloc_bytes() + 4096);
        do_stream_decode(data, size, size - 16, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, dec_flags, false);
        do_stream_decode(data, size, size - 16, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, dec_flags, false);
        do_stream_decode(data, size, size - 16, sizeof(alltypes_pointer_AllTypes), alltypes_pointer_AllTypes_fields, PB_DECODE_CTX_FLAG_DELIMITED, false);
        set_max_alloc_bytes(orig_max_alloc_bytes);
    }

    /* Test pb_decode_ex() modes */
    do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, PB_DECODE_CTX_FLAG_NOINIT | PB_DECODE_CTX_FLAG_DELIMITED, false);
    do_decode(data, size, sizeof(alltypes_static_AllTypes), alltypes_static_AllTypes_fields, PB_DECODE_CTX_FLAG_NULLTERMINATED, false);

    /* Test callbacks also when message is not valid */
    do_callback_decode(data, size, dec_flags, false);

    PB_UNUSED(enc_flags);
#endif

    assert(get_alloc_count() == initial_alloc_count);
}

/* Fuzzer stub for Google OSSFuzz integration */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > g_bufsize)
        return 0;

    do_roundtrips(data, size, false);

    return 0;
}

#ifndef LLVMFUZZER

static bool generate_base_message(uint8_t *buffer, size_t *msglen)
{
    pb_encode_ctx_t stream;
    bool status;
    static const alltypes_static_AllTypes initval = alltypes_static_AllTypes_init_default;

    /* Allocate a message and fill it with defaults */
    alltypes_static_AllTypes *msg = malloc_with_check(sizeof(alltypes_static_AllTypes));
    memcpy(msg, &initval, sizeof(initval));

    /* Apply randomness to the data before encoding */
    while (rand_int(0, 7))
        rand_mess((uint8_t*)msg, sizeof(alltypes_static_AllTypes));

    msg->extensions = NULL;

    pb_init_encode_ctx_for_buffer(&stream, buffer, g_bufsize);
    stream.flags |= PB_ENCODE_CTX_FLAG_NO_VALIDATE_UTF8;
    status = pb_encode(&stream, alltypes_static_AllTypes_fields, msg);
    assert(stream.bytes_written <= g_bufsize);
    assert(stream.bytes_written <= alltypes_static_AllTypes_size);
    
    *msglen = stream.bytes_written;
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
        if (g_bufsize > FUZZTEST_MAX_STANDALONE_BUFSIZE)
            g_bufsize = FUZZTEST_MAX_STANDALONE_BUFSIZE;

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

        if (!feof(stdin))
        {
            /* Read any leftover input data if our buffer is smaller than
             * message size. */
            fprintf(stderr, "Warning: input message too long\n");
            while (fread(buffer, 1, g_bufsize, stdin) == g_bufsize);
        }

        free_with_check(buffer);
    }
    
    return 0;
}
#endif
