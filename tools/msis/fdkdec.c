/* Dekoder ADTS/LOAS na libfdk-aac (dekoder Fraunhofera) - niezalezny sedzia
 * poprawnosci strumienia i zrodlo PCM do pomiaru wejscia w srodek strumienia.
 * Obsluguje AAC-ELD z LD-SBR, ktorego ffmpeg nie dekoduje.
 *
 * Uzycie: fdkdec plik [wyjscie.wav [probek_na_ramke]]
 *   Format rozpoznawany po pierwszych bajtach (0xFFF = ADTS, 0x2B7 = LOAS).
 *   Ramki, ktorych nie da sie jeszcze zdekodowac (brak konfiguracji po wejsciu
 *   w srodek strumienia), zapisuje jako cisze o dlugosci ostatniej znanej ramki,
 *   zeby os czasu zgadzala sie z referencja.
 * Wypisuje: frames=N errors=E first=K sr=S frame=F
 *   (bledy Fill/DecodeFrame; przed pierwsza konfiguracja w LOAS to normalne).
 * Budowa: gcc -O1 fdkdec.c -I$INST/include/fdk-aac $INST/lib/libfdk-aac.a -lm -lstdc++ -o fdkdec
 */
#include <stdio.h>
#include <stdlib.h>
#include "aacdecoder_lib.h"

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

int main(int argc, char **argv) {
  FILE *f, *w = NULL;
  static UCHAR buf[1 << 16];
  static INT_PCM pcm[8 * 2048 * 2];
  int frames = 0, errs = 0, first = -1, loas;
  int ch = 0, sr = 0, flen = 0;
  long data_bytes = 0;
  HANDLE_AACDECODER h;
  UCHAR s[3];
  if (argc < 2) return 2;
  f = fopen(argv[1], "rb");
  if (!f || fread(s, 1, 3, f) != 3) return 2;
  fseek(f, 0, SEEK_SET);
  loas = (((s[0] << 3) | (s[1] >> 5)) == 0x2B7);
  h = aacDecoder_Open(loas ? TT_MP4_LOAS : TT_MP4_ADTS, 1);
  aacDecoder_SetParam(h, AAC_CONCEAL_METHOD, 0);
  if (argc > 2) {
    w = fopen(argv[2], "wb");
    if (!w) return 2;
    fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, w);
    put32(w, 16); put16(w, 1); put16(w, 2); put32(w, 0); put32(w, 0); put16(w, 4); put16(w, 16);
    fwrite("data\0\0\0\0", 1, 8, w);
  }
  for (;;) {
    UINT len, valid, sz, i;
    UCHAR *p = buf;
    AAC_DECODER_ERROR e;
    if (loas) {
      if (fread(buf, 1, 3, f) != 3) break;
      len = 3 + (((buf[1] & 0x1F) << 8) | buf[2]);
      if (fread(buf + 3, 1, len - 3, f) != len - 3) break;
    } else {
      if (fread(buf, 1, 7, f) != 7) break;
      len = ((buf[3] & 3) << 11) | (buf[4] << 3) | (buf[5] >> 5);
      if (len < 7 || len > sizeof(buf)) { errs++; break; }
      if (fread(buf + 7, 1, len - 7, f) != len - 7) break;
    }
    sz = len; valid = len;
    aacDecoder_Fill(h, &p, &sz, &valid);
    e = aacDecoder_DecodeFrame(h, pcm, sizeof(pcm) / sizeof(pcm[0]), 0);
    if (e == AAC_DEC_OK) {
      CStreamInfo *si = aacDecoder_GetStreamInfo(h);
      ch = si->numChannels; sr = si->sampleRate; flen = si->frameSize;
      if (w) {
        for (i = 0; i < (UINT)flen; i++) {
          short l = pcm[i * ch], r = (ch > 1) ? pcm[i * ch + 1] : l;
          put16(w, (unsigned short)l); put16(w, (unsigned short)r);
        }
        data_bytes += (long)flen * 4;
      }
    } else {
      if (e != AAC_DEC_NOT_ENOUGH_BITS) {
        errs++;
        if (first < 0) first = frames;
      }
      /* Ramka bez dzwieku (np. brak konfiguracji po wejsciu w srodek LOAS
       * zwraca NOT_ENOUGH_BITS): cisza, zeby os czasu zgadzala sie z
       * referencja. Bez tego pomiar opoznienia wychodzi zanizony. */
      if (flen <= 0 && argc > 3) flen = atoi(argv[3]); /* znana dlugosc ramki */
      if (w && flen > 0) { /* zachowaj os czasu */
        for (i = 0; i < (UINT)flen; i++) { put16(w, 0); put16(w, 0); }
        data_bytes += (long)flen * 4;
      }
    }
    frames++;
  }
  if (w) {
    if (sr <= 0) sr = 48000;
    fseek(w, 4, SEEK_SET); put32(w, (unsigned)(36 + data_bytes));
    fseek(w, 24, SEEK_SET); put32(w, (unsigned)sr); put32(w, (unsigned)sr * 4);
    fseek(w, 40, SEEK_SET); put32(w, (unsigned)data_bytes);
    fclose(w);
  }
  printf("frames=%d errors=%d first=%d sr=%d frame=%d\n", frames, errs, first, sr, flen);
  aacDecoder_Close(h);
  fclose(f);
  return 0;
}
