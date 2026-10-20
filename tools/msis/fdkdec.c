/* Minimalny dekoder ADTS na libfdk-aac (dekoder Fraunhofera) - trzeci,
 * niezalezny od ffmpeg/faad sedzia poprawnosci strumienia SBR.
 * Wypisuje liczbe ramek i liczbe bledow aacDecoder_DecodeFrame/Fill.
 * Budowa: gcc -O1 fdkdec.c -I$INST/include/fdk-aac $INST/lib/libfdk-aac.a -lm -o fdkdec
 */
#include <stdio.h>
#include <stdlib.h>
#include "aacdecoder_lib.h"

int main(int argc, char **argv) {
  FILE *f;
  HANDLE_AACDECODER h;
  static UCHAR buf[1 << 16];
  static INT_PCM pcm[8 * 2048 * 2];
  int frames = 0, errs = 0, first = -1;
  if (argc < 2) return 2;
  f = fopen(argv[1], "rb");
  if (!f) return 2;
  h = aacDecoder_Open(TT_MP4_ADTS, 1);
  for (;;) {
    UCHAR hdr[7];
    UINT len, valid, sz;
    UCHAR *p = buf;
    AAC_DECODER_ERROR e;
    if (fread(hdr, 1, 7, f) != 7) break;
    len = ((hdr[3] & 3) << 11) | (hdr[4] << 3) | (hdr[5] >> 5);
    if (len < 7 || len > sizeof(buf)) { errs++; break; }
    for (sz = 0; sz < 7; sz++) buf[sz] = hdr[sz];
    if (fread(buf + 7, 1, len - 7, f) != len - 7) break;
    sz = len; valid = len;
    e = aacDecoder_Fill(h, &p, &sz, &valid);
    if (e != AAC_DEC_OK) { errs++; if (first < 0) first = frames; }
    e = aacDecoder_DecodeFrame(h, pcm, sizeof(pcm) / sizeof(pcm[0]), 0);
    if (e != AAC_DEC_OK && e != AAC_DEC_NOT_ENOUGH_BITS) {
      errs++;
      if (first < 0) first = frames;
      if (errs <= 3) fprintf(stderr, "ramka %d: blad 0x%x\n", frames, e);
    }
    frames++;
  }
  printf("frames=%d errors=%d first=%d\n", frames, errs, first);
  aacDecoder_Close(h);
  fclose(f);
  return errs ? 1 : 0;
}
