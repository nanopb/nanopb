/* POSITIVE CONTROL: a legitimately registered oneof callback must keep working
   after the patch. The app sets funcs.decode before pb_decode(); the wire
   carries ONLY the callback member (tag 2), so which_value is still 0 when the
   member is decoded and the patch must NOT clear the union. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <nanopb/pb_decode.h>
#include "variant.pb.h"

static int fired = 0;
static bool my_cb(pb_decode_ctx_t *s, const pb_field_iter_t *f, void **arg) {
    (void)f; (void)arg;
    pb_byte_t buf[32]; size_t n = s->bytes_left;
    if (n > sizeof buf) n = sizeof buf;
    if (!pb_read(s, buf, n)) return false;
    fired = 1;
    printf("[poc] registered callback FIRED, consumed %zu bytes:", n);
    size_t i;
    for (i=0;i<n;i++) printf(" %02x", buf[i]);
    printf("\n");
    return true;
}

int main(void) {
    /* s = SubMsg1{ strvalue: "hi" }  -> 12 04 0a 02 68 69 */
    static const uint8_t wire[] = { 0x12, 0x04, 0x0a, 0x02, 0x68, 0x69 };
    printf("[poc] nanopb %s\n[poc] mode=POSITIVE-CONTROL (app registers a oneof callback)\n", NANOPB_VERSION);
    printf("[poc] wire:");
    size_t i;
    for (i=0;i<sizeof wire;i++) printf(" %02x", wire[i]);
    printf("\n");

    VariantMessage msg = VariantMessage_init_zero;
    msg.value.s.funcs.decode = my_cb;          /* the legitimate registration */
    printf("[poc] funcs.decode registered at %p\n", (void*)my_cb);

    pb_decode_ctx_t is;
    pb_init_decode_ctx_for_buffer(&is, wire, sizeof wire);
    bool ok = pb_decode(&is, VariantMessage_fields, &msg);
    printf("[poc] pb_decode -> %d (%s)\n", ok, ok ? "ok" : PB_GET_ERROR(&is));
    printf("[poc] RESULT: callback %s\n", fired ? "FIRED (correct)" : "DID NOT FIRE (REGRESSION)");
    return fired ? 0 : 1;
}
