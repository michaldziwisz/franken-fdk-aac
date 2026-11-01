/* Harness: run the --resample stage alone.
 * rs_test <in_rate> <out_rate> <channels> [chunk]  < raw f32le  > raw f32le
 * Reads stdin without knowing its length (like a pipe), prints
 * "length=<n> position=<n>" of the stage to stderr at the end.
 * Build: gcc -O2 -DHAVE_STDINT_H -Isrc-fdkaac/src -Isrc-fdkaac tools/resample/rs_test.c src-fdkaac/src/resample.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pcm_reader.h"
#include "resample.h"

typedef struct { pcm_reader_vtbl_t *vtbl; pcm_sample_description_t f; int64_t pos; } src_t;
static const pcm_sample_description_t *gf(pcm_reader_t *r) { return &((src_t *)r)->f; }
static int64_t gl(pcm_reader_t *r) { (void)r; return INT64_MAX; }
static int64_t gp(pcm_reader_t *r) { return ((src_t *)r)->pos; }
static int rf(pcm_reader_t *r, void *b, unsigned n)
{
    src_t *s = (src_t *)r;
    size_t got = fread(b, s->f.bytes_per_frame, n, stdin);
    s->pos += got;
    return (int)got;
}
static void td(pcm_reader_t **r) { free(*r); *r = 0; }
static pcm_reader_vtbl_t vt = { gf, gl, gp, rf, td };

int pcm_read_frames(pcm_reader_t *r, void *data, unsigned nframes)
{
    int n; unsigned count = 0; uint8_t *bp = data;
    unsigned bpf = pcm_get_format(r)->bytes_per_frame;
    do {
        n = r->vtbl->read_frames(r, bp, nframes - count);
        if (n > 0) { count += n; bp += n * bpf; }
    } while (n > 0 && count < nframes);
    return count;
}

int main(int argc, char **argv)
{
    unsigned in = atoi(argv[1]), out = atoi(argv[2]), ch = atoi(argv[3]);
    unsigned chunk = argc > 4 ? atoi(argv[4]) : 1024;
    src_t *s = calloc(1, sizeof *s);
    pcm_reader_t *r;
    float *buf;
    int n;
    s->vtbl = &vt;
    s->f.sample_type = PCM_TYPE_FLOAT; s->f.sample_rate = in;
    s->f.bits_per_channel = 32; s->f.channels_per_frame = ch;
    s->f.bytes_per_frame = 4 * ch;
    r = resampler_open((pcm_reader_t *)s, out, 1);
    if (!r) return 1;
    buf = malloc(chunk * ch * 4);
    while ((n = pcm_read_frames(r, buf, chunk)) > 0)
        fwrite(buf, 4 * ch, n, stdout);
    fprintf(stderr, "position=%lld\n", (long long)pcm_get_position(r));
    pcm_teardown(&r);
    return 0;
}
