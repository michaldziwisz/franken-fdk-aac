/*
 * Franken FDK AAC: sample-rate converter stage of the PCM input chain
 * (--resample). Copyright (C) 2026 Michal Dziwisz.
 * For conditions of distribution and use, see copyright notice in COPYING.
 *
 * Exact rational polyphase resampler, windowed sinc (Kaiser), float.
 *
 *  - Ratio L/M is exact (no drift, no fractional phase accumulator).
 *  - Zero group delay: output sample k is computed at the input instant
 *    k*M/L with a symmetric (non-causal) kernel, so the output timeline is
 *    not shifted. Only the kernel half-length is read ahead internally;
 *    nothing is added to the encoder delay or to the gapless priming.
 *  - Passband flat to 95% of the lower Nyquist, stopband from the lower
 *    Nyquist on with RS_ATTEN_DB attenuation, so nothing aliases into the
 *    audible band. Each polyphase branch is normalised to unity DC gain.
 *  - Output length is ceil(input_length * L / M): every output instant that
 *    lies inside the input is produced, so position/length stay consistent
 *    for progress and gapless trimming.
 *
 * The float result goes through the stock limiter (overshoot of the band
 * limit above 0 dBFS) and is then rounded, not truncated, to the integer PCM
 * the library takes.
 */
#if HAVE_CONFIG_H
#  include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#include "pcm_reader.h"
#include "resample.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#define RS_ATTEN_DB   120.0   /* stopband attenuation */
#define RS_PASS       0.95    /* passband edge, fraction of the lower Nyquist */
#define RS_CHUNK      4096    /* max input frames read per refill */
#define RS_MAX_TABLE  (64u << 20) /* bytes, refuse absurd ratios */

static const unsigned aac_rates[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000   /* 7350 is not supported by the encoder */
};

int resample_parse_rate(const char *arg, unsigned *rate)
{
    char buf[32], *end;
    double v;
    size_t i, n = strlen(arg);

    if (n == 0 || n >= sizeof(buf)) return -1;
    for (i = 0; i < n; ++i) {
        char c = arg[i];
        if (c == ',') c = '.';
        if (!((c >= '0' && c <= '9') || c == '.')) return -1;
        buf[i] = c;
    }
    buf[n] = 0;
    v = strtod(buf, &end);
    if (*end || v <= 0.0) return -1;
    if (v >= 1000.0) {
        /* exact Hz: must be one of the AAC rates */
        if (v != floor(v)) return -1;
        for (i = 0; i < sizeof(aac_rates) / sizeof(aac_rates[0]); ++i)
            if ((double)aac_rates[i] == v) { *rate = aac_rates[i]; return 0; }
        return -1;
    }
    /* kHz shorthand: 44 / 44.1 / 44,1 -> 44100, 22 -> 22050, 11 -> 11025 */
    for (i = 0; i < sizeof(aac_rates) / sizeof(aac_rates[0]); ++i) {
        double r = aac_rates[i] / 1000.0;
        if (fabs(r - v) < 1e-9 || (v == floor(v) && floor(r) == v)) {
            *rate = aac_rates[i];
            return 0;
        }
    }
    return -1;
}

typedef struct resampler_t {
    pcm_reader_vtbl_t *vtbl;
    pcm_reader_t *src;
    pcm_sample_description_t format;
    unsigned nch, L, M, taps;
    float *table;          /* L phases x taps */
    float **x;             /* per channel input history */
    unsigned cap;          /* frames per channel buffer */
    unsigned fill;         /* frames held */
    int64_t base;          /* absolute input index of x[c][0] */
    int64_t in_total;      /* input frames read so far */
    int eof;
    int64_t nfloor;        /* floor(k*M/L) of the next output */
    unsigned frac;         /* (k*M) mod L */
    int64_t position;      /* output frames delivered */
    float *ibuf;           /* interleaved read buffer */
    double fc_hz, pass_hz;
} resampler_t;

static unsigned gcd_u(unsigned a, unsigned b)
{
    while (b) { unsigned t = a % b; a = b; b = t; }
    return a;
}

static double bessel_i0(double x)
{
    double sum = 1.0, term = 1.0, q = x * x / 4.0;
    int k;
    for (k = 1; k < 200; ++k) {
        term *= q / ((double)k * k);
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

static int build_table(resampler_t *self, unsigned fin, unsigned fout)
{
    const double nyq_low = 0.5 * (fin < fout ? fin : fout);
    const double pass = RS_PASS * nyq_low, stop = nyq_low;
    const double fc = 0.5 * (pass + stop);
    const double dw = 2.0 * M_PI * (stop - pass) / fin;
    const double beta = 0.1102 * (RS_ATTEN_DB - 8.7);
    const double i0b = bessel_i0(beta);
    const double g = 2.0 * fc / fin;
    unsigned T = (unsigned)ceil((RS_ATTEN_DB - 7.95) / (2.285 * dw)) + 1;
    unsigned p, i;
    double half;

    T = (T + 3) & ~3u;
    if ((size_t)self->L * T * sizeof(float) > RS_MAX_TABLE)
        return -1;
    self->taps = T;
    self->fc_hz = fc;
    self->pass_hz = pass;
    self->table = malloc((size_t)self->L * T * sizeof(float));
    if (!self->table) return -1;
    half = 0.5 * T;
    for (p = 0; p < self->L; ++p) {
        double fr = (double)p / self->L, sum = 0.0;
        double *tmp = malloc(T * sizeof(double));
        if (!tmp) return -1;
        for (i = 0; i < T; ++i) {
            double t = ((double)i - half + 1.0) - fr;
            double u = t / half, w, s, a = M_PI * g * t;
            if (u <= -1.0 || u >= 1.0) { tmp[i] = 0.0; continue; }
            w = bessel_i0(beta * sqrt(1.0 - u * u)) / i0b;
            s = fabs(a) < 1e-12 ? 1.0 : sin(a) / a;
            tmp[i] = g * s * w;
            sum += tmp[i];
        }
        for (i = 0; i < T; ++i)
            self->table[(size_t)p * T + i] = (float)(tmp[i] / sum);
        free(tmp);
    }
    return 0;
}

static inline pcm_reader_t *get_source(pcm_reader_t *reader)
{
    return ((resampler_t *)reader)->src;
}

static const pcm_sample_description_t *get_format(pcm_reader_t *reader)
{
    return &((resampler_t *)reader)->format;
}

static int64_t get_length(pcm_reader_t *reader)
{
    resampler_t *self = (resampler_t *)reader;
    int64_t n = pcm_get_length(self->src);
    if (n == INT64_MAX || n < 0) return n;
    return (n * self->L + self->M - 1) / self->M;
}

static int64_t get_position(pcm_reader_t *reader)
{
    return ((resampler_t *)reader)->position;
}

/* make room so that index `need_lo` stays and `want` more frames fit */
static int compact(resampler_t *self, int64_t need_lo, unsigned want)
{
    unsigned c, drop = 0;
    if (need_lo > self->base)
        drop = (unsigned)(need_lo - self->base);
    if (drop > self->fill) drop = self->fill;
    if (drop) {
        for (c = 0; c < self->nch; ++c)
            memmove(self->x[c], self->x[c] + drop,
                    (self->fill - drop) * sizeof(float));
        self->fill -= drop;
        self->base += drop;
    }
    if (self->fill + want > self->cap) {
        unsigned ncap = self->cap;
        while (self->fill + want > ncap) ncap *= 2;
        for (c = 0; c < self->nch; ++c) {
            float *q = realloc(self->x[c], ncap * sizeof(float));
            if (!q) return -1;
            self->x[c] = q;
        }
        self->cap = ncap;
    }
    return 0;
}

/* append input (or zeros past the end) until index `upto` is buffered */
static int refill(resampler_t *self, int64_t upto, int64_t need_lo)
{
    while (self->base + self->fill <= upto) {
        unsigned c, i, got = 0;
        if (compact(self, need_lo, RS_CHUNK) < 0) return -1;
        if (!self->eof) {
            /* read only what this request needs (live pipes: no extra
               buffering beyond the kernel half-length) */
            int64_t need = upto - (self->base + self->fill) + 1;
            unsigned want = need < RS_CHUNK ? (unsigned)need : RS_CHUNK;
            int n = pcm_read_frames(self->src, self->ibuf, want);
            if (n < 0) return -1;
            got = (unsigned)n;
            if (got == 0) self->eof = 1;
            self->in_total += got;
            for (c = 0; c < self->nch; ++c) {
                float *d = self->x[c] + self->fill;
                for (i = 0; i < got; ++i)
                    d[i] = self->ibuf[i * self->nch + c];
            }
        }
        if (self->eof && got == 0) {
            /* past the end: silence, as before the start */
            unsigned z = RS_CHUNK;
            for (c = 0; c < self->nch; ++c)
                memset(self->x[c] + self->fill, 0, z * sizeof(float));
            got = z;
        }
        self->fill += got;
    }
    return 0;
}

static int read_frames(pcm_reader_t *reader, void *buffer, unsigned nframes)
{
    resampler_t *self = (resampler_t *)reader;
    float *op = buffer;
    const unsigned T = self->taps, nch = self->nch;
    const int64_t back = (int64_t)T / 2 - 1;
    unsigned k;

    if (nframes) {
        /* fetch the input of the whole request at once */
        int64_t last = self->nfloor +
            (int64_t)(((uint64_t)self->frac + (uint64_t)(nframes - 1) * self->M) / self->L);
        if (!(self->eof && self->nfloor >= self->in_total) &&
            refill(self, last + T / 2, self->nfloor - back) < 0)
            return -1;
    }
    for (k = 0; k < nframes; ++k) {
        int64_t lo = self->nfloor - back, hi = self->nfloor + T / 2;
        const float *h;
        unsigned c, off;
        /* stop at the first output instant outside the input */
        if (self->eof && self->nfloor >= self->in_total) break;
        if (refill(self, hi, lo) < 0) return -1;
        if (self->eof && self->nfloor >= self->in_total) break;
        h = self->table + (size_t)self->frac * T;
        off = (unsigned)(lo - self->base);
        for (c = 0; c < nch; ++c) {
            const float *xp = self->x[c] + off;
            float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
            unsigned i;
            for (i = 0; i < T; i += 4) {
                a0 += h[i] * xp[i];
                a1 += h[i + 1] * xp[i + 1];
                a2 += h[i + 2] * xp[i + 2];
                a3 += h[i + 3] * xp[i + 3];
            }
            op[k * nch + c] = (a0 + a1) + (a2 + a3);
        }
        self->frac += self->M;
        while (self->frac >= self->L) {
            self->frac -= self->L;
            ++self->nfloor;
        }
    }
    self->position += k;
    return k;
}

static void teardown(pcm_reader_t **reader)
{
    resampler_t *self = (resampler_t *)*reader;
    unsigned c;
    pcm_teardown(&self->src);
    if (self->x) {
        for (c = 0; c < self->nch; ++c) free(self->x[c]);
        free(self->x);
    }
    free(self->table);
    free(self->ibuf);
    free(self);
    *reader = 0;
}

static pcm_reader_vtbl_t my_vtable = {
    get_format, get_length, get_position, read_frames, teardown
};

pcm_reader_t *resampler_open(pcm_reader_t *reader, unsigned out_rate,
                             int verbose)
{
    resampler_t *self = 0;
    const pcm_sample_description_t *sfmt = pcm_get_format(reader);
    unsigned fin = sfmt->sample_rate, g, c;

    if (!PCM_IS_FLOAT(sfmt) || sfmt->bits_per_channel != 32 || !fin)
        goto FAIL;
    if ((self = calloc(1, sizeof(resampler_t))) == 0)
        goto FAIL;
    self->src = reader;
    self->vtbl = &my_vtable;
    memcpy(&self->format, sfmt, sizeof(self->format));
    self->format.sample_rate = out_rate;
    self->nch = sfmt->channels_per_frame;
    g = gcd_u(fin, out_rate);
    self->L = out_rate / g;
    self->M = fin / g;
    if (build_table(self, fin, out_rate) < 0) {
        fprintf(stderr, "ERROR: --resample %u -> %u Hz: ratio %u/%u too "
                "complex\n", fin, out_rate, self->L, self->M);
        goto FAIL;
    }
    self->cap = 2 * RS_CHUNK + self->taps;
    self->x = calloc(self->nch, sizeof(float *));
    self->ibuf = malloc((size_t)RS_CHUNK * self->nch * sizeof(float));
    if (!self->x || !self->ibuf) goto FAIL;
    for (c = 0; c < self->nch; ++c) {
        self->x[c] = calloc(self->cap, sizeof(float));
        if (!self->x[c]) goto FAIL;
    }
    /* history before the first sample is silence */
    self->base = -(int64_t)self->taps;
    self->fill = self->taps;
    if (verbose)
        fprintf(stderr, " resample             : %u -> %u Hz (ratio %u/%u, "
                "windowed sinc %u taps, flat to %.0f Hz, -%.0f dB from %.0f "
                "Hz, no added delay)\n", fin, out_rate, self->L, self->M,
                self->taps, self->pass_hz, RS_ATTEN_DB,
                0.5 * (fin < out_rate ? fin : out_rate));
    return (pcm_reader_t *)self;
FAIL:
    if (self) {
        self->src = 0;
        if (self->x) {
            for (c = 0; c < self->nch; ++c) free(self->x[c]);
            free(self->x);
        }
        free(self->table);
        free(self->ibuf);
        free(self);
    }
    return 0;
}

/* ---- float -> s32 with rounding (the stock float path truncates) ---- */

typedef struct rounder_t {
    pcm_reader_vtbl_t *vtbl;
    pcm_reader_t *src;
    pcm_sample_description_t format;
} rounder_t;

static const pcm_sample_description_t *r_get_format(pcm_reader_t *reader)
{
    return &((rounder_t *)reader)->format;
}

static int64_t r_get_length(pcm_reader_t *reader)
{
    return pcm_get_length(((rounder_t *)reader)->src);
}

static int64_t r_get_position(pcm_reader_t *reader)
{
    return pcm_get_position(((rounder_t *)reader)->src);
}

static int r_read_frames(pcm_reader_t *reader, void *buffer, unsigned nframes)
{
    rounder_t *self = (rounder_t *)reader;
    unsigned i, count;
    int n = pcm_read_frames(self->src, buffer, nframes);
    float *ip = buffer;
    int32_t *op = buffer;
    if (n <= 0) return n;
    count = (unsigned)n * self->format.channels_per_frame;
    for (i = 0; i < count; ++i) {
        double v = floor((double)ip[i] * 2147483648.0 + 0.5);
        if (v > 2147483647.0) v = 2147483647.0;
        if (v < -2147483648.0) v = -2147483648.0;
        op[i] = (int32_t)v;
    }
    return n;
}

static void r_teardown(pcm_reader_t **reader)
{
    rounder_t *self = (rounder_t *)*reader;
    pcm_teardown(&self->src);
    free(self);
    *reader = 0;
}

static pcm_reader_vtbl_t r_vtable = {
    r_get_format, r_get_length, r_get_position, r_read_frames, r_teardown
};

pcm_reader_t *pcm_open_s32_rounder(pcm_reader_t *reader)
{
    rounder_t *self = calloc(1, sizeof(rounder_t));
    if (!self) return 0;
    self->src = reader;
    self->vtbl = &r_vtable;
    memcpy(&self->format, pcm_get_format(reader), sizeof(self->format));
    self->format.sample_type = PCM_TYPE_SINT;
    self->format.bits_per_channel = 32;
    self->format.bytes_per_frame = 4 * self->format.channels_per_frame;
    return (pcm_reader_t *)self;
}
