# New features in nanopb 1.0

This document showcases new features in nanopb 1.0 that are not immediately visible, but
that you may want to take advantage of.

A lot of effort has been spent in retaining backwards and forwards
compatibility with previous nanopb versions. For a list of breaking
changes, see [migration document](migration.md).

## Encode and decode contexts

Previously used `pb_decode_ctx_t` and `pb_encode_ctx_t` types have been
expanded into `pb_decode_ctx_t` and `pb_encode_ctx_t`. The substream
implementation has been modified so that it is now safe to expand
the contexts with custom fields. The `ctx` pointer passed to callbacks
is always the same value that was passed to `decode`.

For example, this is allowed:

     typedef struct {
        pb_decode_ctx_t ctx;
        ...
        my_type_t extra_stuff;
        ...
     } my_dec_ctx_t;

     bool callback(pb_decode_ctx_t *ctx, const pb_field_t *field, void **arg)
     {
        my_dec_ctx_t *myctx = (my_dec_ctx_t*)ctx;

        ... use myctx.extra_stuff ...
     }

     bool decode(pb_byte_t *buf, size_t msglen)
     {
        my_dec_ctx_t ctx;
        MyMessage msg;
        if (!pb_init_decode_ctx_for_buffer(&ctx.ctx, buf, msglen)) return false;
        if (!pb_decode(&ctx.ctx, MyMessage_fields, &msg)) return false;

        ...
     }

Note that this does not apply to backwards compatibility functions
`pb_make_string_substream` and `pb_close_string_substream` which still make
a full copy of the context structure. These functions are only enabled
when [`PB_API_VERSION` build option](reference_buildopt.md#api-version-compatibility-flag) is defined.

## API updater script

## Per-context memory allocator support

## Per-context field callbacks

## Direct pointer to raw bytes data

## Single-pass encoding

## Better control over stack memory usage

## More granular feature disabling
