/*
 * Franken FDK AAC: --resample input stage. Copyright (C) 2026 Michal Dziwisz.
 * For conditions of distribution and use, see copyright notice in COPYING.
 */
#ifndef RESAMPLE_H
#define RESAMPLE_H

#include "pcm_reader.h"

/* "44100", "44", "44.1", "44,1" -> 44100. Only AAC sample rates.
   Returns 0 on success, -1 otherwise. */
int resample_parse_rate(const char *arg, unsigned *rate);

/* float32 in, float32 out at out_rate (no added delay). */
pcm_reader_t *resampler_open(pcm_reader_t *reader, unsigned out_rate,
                             int verbose);

/* float32 in, rounded signed 32-bit out. */
pcm_reader_t *pcm_open_s32_rounder(pcm_reader_t *reader);

#endif
