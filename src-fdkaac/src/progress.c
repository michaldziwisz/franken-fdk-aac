/* 
 * Copyright (C) 2013 nu774
 * For conditions of distribution and use, see copyright notice in COPYING
 */
#if HAVE_CONFIG_H
#  include "config.h"
#endif
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#if HAVE_INTTYPES_H
#  include <inttypes.h>
#elif defined _MSC_VER
#  define PRId64 "I64d"
#endif
#include "compat.h"
#include "progress.h"

static
void seconds_to_hms(double seconds, int *h, int *m, int *s, int *millis)
{
    *h = (int)(seconds / 3600.0);
    seconds -= *h * 3600;
    *m = (int)(seconds / 60.0);
    seconds -= *m * 60;
    *s = (int)seconds;
    *millis = (int)((seconds - *s) * 1000.0 + 0.5);
}

static
int sprint_seconds(char *buf, size_t size, double seconds)
{
    int h, m, s, millis;
    seconds_to_hms(seconds, &h, &m, &s, &millis);
    if (h)
        return snprintf(buf, size, "%d:%02d:%02d.%03d", h, m, s, millis);
    return snprintf(buf, size, "%02d:%02d.%03d", m, s, millis);
}

static
void print_seconds(FILE *fp, double seconds)
{
    char buf[32];
    sprint_seconds(buf, sizeof buf, seconds);
    fputs(buf, fp);
}

void aacenc_progress_init(aacenc_progress_t *progress, int64_t total,
                          int32_t timescale)
{
    progress->start = aacenc_timer();
    progress->timescale = timescale;
    progress->total = total;
    progress->last_print = 0.0;
}

void aacenc_progress_update(aacenc_progress_t *progress, int64_t current,
                            int period)
{
    double now, ellapsed, speed, eta, seconds;
    int percent;
    char a[32], b[32], c[32];

    if (current < progress->processed + period)
        return;
    now = aacenc_timer();
    /* Jedna linia jednym zapisem i nie czesciej niz co 100 ms (konsola
       Windows placi za kazde wywolanie zapisu). period == 0 to wymuszony
       stan koncowy z aacenc_progress_finish: wtedy throttle nie obowiazuje. */
    if (period > 0 && now - progress->last_print < 100.0)
        return;

    seconds = current / progress->timescale;
    ellapsed = (now - progress->start) / 1000.0;
    speed = ellapsed ? seconds / ellapsed : 1.0;
    percent = progress->total ? 100.0 * current / progress->total + .5
                              : 100;
    eta = current ? ellapsed * (progress->total / (double)current - 1.0)
                  : progress->total ? DBL_MAX : 0;

    if (progress->total == INT64_MAX) {
        sprint_seconds(a, sizeof a, seconds);
        fprintf(stderr, "\r%s (%.0fx)   ", a, speed);
    } else {
        sprint_seconds(a, sizeof a, seconds);
        sprint_seconds(b, sizeof b, progress->total / progress->timescale);
        sprint_seconds(c, sizeof c, eta);
        fprintf(stderr, "\r[%d%%] %s/%s (%.0fx), ETA %s   ",
                percent, a, b, speed, c);
    }
    progress->processed = current;
    progress->last_print = now;
}

void aacenc_progress_finish(aacenc_progress_t *progress, int64_t current)
{
    double ellapsed = (aacenc_timer() - progress->start) / 1000.0;
    aacenc_progress_update(progress, current, 0);
    if (progress->total == INT64_MAX)
        fprintf(stderr, "\n%" PRId64 " samples processed in ", current);
    else
        fprintf(stderr, "\n%" PRId64 "/%" PRId64 " samples processed in ",
                current, progress->total);
    print_seconds(stderr, ellapsed);
    putc('\n', stderr);
}
