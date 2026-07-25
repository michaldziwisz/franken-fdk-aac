/*
 * dab_rs.c - Reed-Solomon RS(120,110) encoder for DAB+ superframes.
 *
 * DAB+ (ETSI TS 102 563) protects each audio superframe with a shortened
 * RS(120,110) t=5 code over GF(2^8), shortened from RS(255,245):
 *   symsize=8, gfpoly=0x11d, fcr=0, prim=1, nroots=10, pad=135.
 * (255 - 135 pad - 10 parity = 110 data bytes -> 120 total per row.)
 *
 * Encoder only (decoding happens in the receiver). Algorithm after
 * Phil Karn's libfec encode_rs_char (public domain), specialised to the
 * fixed DAB+ parameters so we carry no external dependency.
 */
#include "dab_rs.h"
#include <string.h>

#define MM 8
#define NN 255
#define NROOTS 10
#define FCR 0
#define PRIM 1
#define A0 NN /* index_of[0]: log of zero = 255 (sentinel) */
#define GFPOLY 0x11d
#define NDATA 110

static unsigned char alpha_to[NN + 1]; /* antilog: alpha_to[i] = a^i          */
static unsigned char index_of[NN + 1]; /* log:     index_of[a^i] = i          */
static unsigned char genpoly[NROOTS + 1];
static int rs_init_done = 0;

/* x mod 255 for sums of two logs (each < 255, so sum < 510) */
static int mod255(int x) {
  while (x >= NN) {
    x -= NN;
    x = (x >> 8) + (x & NN);
  }
  return x;
}

static void dab_rs_init(void) {
  int i, j, root;
  int sr = 1;

  /* Build GF(2^8) log/antilog tables from primitive polynomial 0x11d. */
  index_of[0] = A0;
  alpha_to[A0] = 0;
  for (i = 0; i < NN; i++) {
    index_of[sr] = (unsigned char)i;
    alpha_to[i] = (unsigned char)sr;
    sr <<= 1;
    if (sr & (1 << MM)) sr ^= GFPOLY;
    sr &= NN;
  }

  /* Generator polynomial g(x) = prod_{i=0}^{NROOTS-1} (x - a^(FCR+i)*PRIM). */
  genpoly[0] = 1;
  for (i = 0, root = FCR * PRIM; i < NROOTS; i++, root += PRIM) {
    genpoly[i + 1] = 1;
    for (j = i; j > 0; j--) {
      if (genpoly[j] != 0)
        genpoly[j] = genpoly[j - 1] ^
                     alpha_to[mod255(index_of[genpoly[j]] + root)];
      else
        genpoly[j] = genpoly[j - 1];
    }
    genpoly[0] = alpha_to[mod255(index_of[genpoly[0]] + root)];
  }
  /* Store generator in index (log) form for the encode inner loop. */
  for (i = 0; i <= NROOTS; i++) genpoly[i] = index_of[genpoly[i]];

  rs_init_done = 1;
}

void dab_rs_encode(const unsigned char *data, unsigned char *parity) {
  int i, j;
  unsigned char feedback;

  if (!rs_init_done) dab_rs_init();

  memset(parity, 0, NROOTS);
  for (i = 0; i < NDATA; i++) {
    feedback = index_of[data[i] ^ parity[0]];
    if (feedback != A0) {
      for (j = 1; j < NROOTS; j++)
        parity[j] ^= alpha_to[mod255(feedback + genpoly[NROOTS - j])];
    }
    memmove(&parity[0], &parity[1], NROOTS - 1);
    if (feedback != A0)
      parity[NROOTS - 1] = alpha_to[mod255(feedback + genpoly[0])];
    else
      parity[NROOTS - 1] = 0;
  }
}
