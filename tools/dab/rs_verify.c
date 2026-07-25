/* Weryfikacja dab_rs_encode vs libfec (Phil Karn) na losowych danych.
 * Parametry DAB+: init_rs_char(8, 0x11d, 0, 1, 10, 135). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fec.h"
#include "dab_rs.h"

int main(void) {
  void *rs = init_rs_char(8, 0x11d, 0, 1, 10, 135);
  if (!rs) { fprintf(stderr, "init_rs_char FAIL\n"); return 2; }
  int trials = 5000, mism = 0;
  srand(12345);
  for (int t = 0; t < trials; t++) {
    unsigned char data[110], p_ref[10], p_mine[10];
    for (int i = 0; i < 110; i++) data[i] = rand() & 0xff;
    /* libfec: block of 110 data bytes, appends 10 parity */
    unsigned char blk[120];
    memcpy(blk, data, 110);
    memset(blk + 110, 0, 10);
    encode_rs_char(rs, blk, p_ref);
    dab_rs_encode(data, p_mine);
    if (memcmp(p_ref, p_mine, 10) != 0) {
      mism++;
      if (mism <= 3) {
        fprintf(stderr, "MISMATCH trial %d\n ref :", t);
        for (int i = 0; i < 10; i++) fprintf(stderr, " %02x", p_ref[i]);
        fprintf(stderr, "\n mine:");
        for (int i = 0; i < 10; i++) fprintf(stderr, " %02x", p_mine[i]);
        fprintf(stderr, "\n");
      }
    }
  }
  free_rs_char(rs);
  printf("trials=%d mismatches=%d -> %s\n", trials, mism,
         mism == 0 ? "RS_MATCH_OK" : "RS_MISMATCH");
  return mism == 0 ? 0 : 1;
}
