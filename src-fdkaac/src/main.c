/* 
 * Copyright (C) 2013 nu774
 * For conditions of distribution and use, see copyright notice in COPYING
 */
#if HAVE_CONFIG_H
#  include "config.h"
#endif
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#if HAVE_INTTYPES_H
#  include <inttypes.h>
#elif defined(_MSC_VER)
#  define SCNd64 "I64d"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <locale.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SIGACTION
#include <signal.h>
#endif
#ifdef _WIN32
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "compat.h"
#include "pcm_reader.h"
#include "aacenc.h"
#include "dab_rs.h"
#include "dab_pad.h"
#include "m4af.h"
#include "progress.h"
#include "version.h"
#include "metadata.h"

#define PROGNAME "fdkaac"

static volatile int g_interrupted = 0;

/* Strict integer parser for the Frankenstein switches: sscanf("%d") silently
 * read "128,5" or "3x" as 128 / 3. Rejects anything that is not a whole
 * decimal number that fits in an int. Returns 1 on success (drop-in for the
 * old "sscanf(...) != 1" test). */
static int fr_scan_int(const char *arg, int *out)
{
    char *end;
    long v;
    if (!arg || !*arg || isspace((unsigned char)*arg)) return 0;
    errno = 0;
    v = strtol(arg, &end, 10);
    if (errno || end == arg || *end || v > 2147483647L || v < -2147483647L)
        return 0;
    *out = (int)v;
    return 1;
}

/* Frankenstein: strict decimal parser for every fractional switch.
 * Accepts '.' or ',' as the decimal separator (a Polish/European keyboard
 * habit: "-0,23" used to be read by sscanf as "-0" and silently turned the
 * knob OFF, "-1,5" as "-1"). Rejects trailing junk, exponents, NaN/Inf and
 * values that would overflow an int. Extra fractional digits are not an
 * error: the value is rounded half-away-from-zero to 1/scale, and the RANGE
 * IS CHECKED ON THE ROUNDED VALUE, so what is accepted is exactly what the
 * encoder gets. Returns 0 on success. */
static int fr_parse_dec(const char *arg, double lo, double hi, int scale,
                        int *out)
{
    char buf[64], *end;
    size_t i, len;
    double v, r;
    if (!arg) return -1;
    len = strlen(arg);
    if (len == 0 || len >= sizeof(buf)) return -1;
    for (i = 0; i < len; ++i) {
        char c = arg[i];
        if (c == ',') c = '.';
        if (!(isdigit((unsigned char)c) || c == '.' || c == '-' || c == '+'))
            return -1; /* no exponents, spaces, "nan", "inf" */
        buf[i] = c;
    }
    buf[len] = 0;
    if (strchr(buf, '.') != strrchr(buf, '.')) return -1; /* "1.2.3" */
    errno = 0;
    v = strtod(buf, &end);
    if (errno || end == buf || *end) return -1;
    r = v * scale;
    r = (r < 0) ? -floor(-r + 0.5) : floor(r + 0.5);
    if (r > 2000000000.0 || r < -2000000000.0) return -1; /* int overflow */
    if (r / scale < lo - 1e-9 || r / scale > hi + 1e-9) return -1;
    *out = (int)r;
    return 0;
}

#if HAVE_SIGACTION
static void signal_handler(int signum)
{
    g_interrupted = 1;
}
static void handle_signals(void)
{
    int i, sigs[] = { SIGINT, SIGHUP, SIGTERM };
    for (i = 0; i < sizeof(sigs)/sizeof(sigs[0]); ++i) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = signal_handler;
        sa.sa_flags |= SA_RESTART;
        sigaction(sigs[i], &sa, 0);
    }
}
#elif defined(_WIN32)
static BOOL WINAPI signal_handler(DWORD type)
{
    g_interrupted = 1;
    return TRUE;
}

static void handle_signals(void)
{
    SetConsoleCtrlHandler(signal_handler, TRUE);
}
#else
static void handle_signals(void)
{
}
#endif

static
int read_callback(void *cookie, void *data, uint32_t size)
{
    size_t rc = fread(data, 1, size, (FILE*)cookie);
    return ferror((FILE*)cookie) ? -1 : (int)rc;
}

static
int write_callback(void *cookie, const void *data, uint32_t size)
{
    size_t rc = fwrite(data, 1, size, (FILE*)cookie);
    return ferror((FILE*)cookie) ? -1 : (int)rc;
}

static
int seek_callback(void *cookie, int64_t off, int whence)
{
    return fseeko((FILE*)cookie, off, whence);
}

static
int64_t tell_callback(void *cookie)
{
    return ftello((FILE*)cookie);
}

static
void usage(void)
{
    printf(
PROGNAME " %s\n"
"Usage: " PROGNAME " [options] input_file\n"
"Options:\n"
" -h, --help                    Print this help message\n"
" -p, --profile <n>             Profile (audio object type)\n"
"                                 2: MPEG-4 AAC LC (default)\n"
"                                 5: MPEG-4 HE-AAC (SBR)\n"
"                                29: MPEG-4 HE-AAC v2 (SBR+PS)\n"
"                                23: MPEG-4 AAC LD\n"
"                                39: MPEG-4 AAC ELD\n"
" -b, --bitrate <n>             Bitrate in bits per seconds (for CBR)\n"
" -m, --bitrate-mode <n>        Bitrate configuration\n"
"                                 0: CBR (default)\n"
"                                 1-5: VBR\n"
"                               (VBR mode is not officially supported, and\n"
"                                works only on a certain combination of\n"
"                                parameter settings, sample rate, and\n"
"                                channel configuration)\n"
" -w, --bandwidth <n>           Frequency bandwidth in Hz (AAC LC only)\n"
" -a, --afterburner <n>         Afterburner\n"
"                                 0: Off\n"
"                                 1: On(default)\n"
" -L, --lowdelay-sbr <-1|0|1>   Configure SBR activity on AAC ELD\n"
"                                -1: Use ELD SBR auto configurator\n"
"                                 0: Disable SBR on ELD (default)\n"
"                                 1: Enable SBR on ELD\n"
" -s, --sbr-ratio <0|1|2>       Controls activation of downsampled SBR\n"
"                                 0: Use lib default (default)\n"
"                                 1: downsampled SBR (default for ELD+SBR)\n"
"                                 2: dual-rate SBR (default for HE-AAC)\n"
" -f, --transport-format <n>    Transport format\n"
"                                 0: RAW (default, muxed into M4A)\n"
"                                 1: ADIF\n"
"                                 2: ADTS\n"
"                                 6: LATM MCP=1\n"
"                                 7: LATM MCP=0\n"
"                                10: LOAS/LATM (LATM within LOAS)\n"
" -C, --adts-crc-check          Add CRC protection on ADTS header\n"
" -h, --header-period <n>       StreamMuxConfig/PCE repetition period in\n"
"                               transport layer\n"
"\n"
" -o <filename>                 Output filename\n"
" -G, --gapless-mode <n>        Encoder delay signaling for gapless playback\n"
"                                 0: iTunSMPB (default)\n"
"                                 1: ISO standard (edts + sgpd)\n"
"                                 2: Both\n"
" --include-sbr-delay           Count SBR decoder delay in encoder delay\n"
"                               This is not iTunes compatible, but is default\n"
"                               behavior of FDK library.\n"
" -I, --ignorelength            Ignore length of WAV header\n"
" -S, --silent                  Don't print progress messages\n"
" --moov-before-mdat            Place moov box before mdat box on m4a output\n"
" --no-tool-tag                 Don't write the PompAAC encoder-identifier tag (\302\251too)\n"
" --minimal-moov                Smallest legal m4a: skip the whole udta/metadata\n"
"                               block (encoder tag, gapless iTunSMPB, all tags).\n"
"                               Playback skeleton (moov/trak/...) is kept - that\n"
"                               part is mandatory and cannot be removed.\n"
" --no-timestamp                Don't inject timestamp in the file\n"
"\n"
"Options for raw (headerless) input:\n"
" -R, --raw                     Treat input as raw (by default WAV is\n"
"                               assumed)\n"
" --raw-channels <n>            Number of channels (default: 2)\n"
" --raw-rate     <n>            Sample rate (default: 44100)\n"
" --raw-format   <spec>         Sample format, default is \"S16L\".\n"
"                               Spec is as follows:\n"
"                                1st char: S(igned)|U(nsigned)|F(loat)\n"
"                                2nd part: bits per channel\n"
"                                Last char: L(ittle)|B(ig)\n"
"                               Last char can be omitted, in which case L is\n"
"                               assumed. Spec is case insensitive, therefore\n"
"                               \"u16b\" is same as \"U16B\".\n"
"\n"
"Tagging options:\n"
" --title <string>\n"
" --artist <string>\n"
" --album <string>\n"
" --genre <string>\n"
" --date <string>\n"
" --composer <string>\n"
" --grouping <string>\n"
" --comment <string>\n"
" --album-artist <string>\n"
" --track <number[/total]>\n"
" --disk <number[/total]>\n"
" --tempo <n>\n"
" --tag <fcc>:<value>          Set iTunes predefined tag with four char code.\n"
" --tag-from-file <fcc>:<filename>\n"
"                              Same as above, but value is read from file.\n"
" --long-tag <name>:<value>    Set arbitrary tag as iTunes custom metadata.\n"
" --tag-from-json <filename[?dot_notation]>\n"
"                              Read tags from JSON. By default, tags are\n"
"                              assumed to be direct children of the root\n"
"                              object(dictionary).\n"
"                              Optionally, position of the dictionary\n"
"                              that contains tags can be specified with\n"
"                              dotted notation.\n"
"                              Example:\n"
"                                --tag-from-json /path/to/json?format.tags\n"
"\n"
"Frankenstein switches (override FDK automatics). Grouped; roughly easy->geeky.\n"
"Without any of these the encoder is bit-identical to stock FDK.\n"
"\n== A. Start here (consumer knobs) ==\n"
" --verbose                   Dump the real, initialised encoder parameters to stderr\n"
"                               before encoding. Use it to see what actually took effect.\n"
" --is-aggression <0..100>    Intensity-stereo aggressiveness. 0=stock FDK (very shy),\n"
"                               100=maximal (drops the bitrate/bandwidth gate, loosens\n"
"                               correlation/direction/region thresholds). Higher=more bands\n"
"                               coded as IS, weaker stereo separation. Start 30-50.\n"
" --speech                    Tune SBR for human SPEECH (different inverse-filtering,\n"
"                               noise floor, no parametric coding). HE-AAC only; LC has no\n"
"                               speech mode. For clean voice at low bitrate.\n"
" --uncap-bandwidth           Lift the hard 20 kHz core cap so --core-cutoff can reach up\n"
"                               to Nyquist (e.g. full band from 96 kHz input). Use WITH\n"
"                               --core-cutoff <hz>; alone it does nothing.\n"
" --unlock-bitrate            Remove the LOWER bitrate floors: the 40 bit/channel core\n"
"                               minimum, the LD/ELD 8 kbps/ch rule and 208-bit transport\n"
"                               guess, the LD/ELD bandwidth table start and the SBR/PS\n"
"                               tuning-table floor (HE/HEv2/ELD+SBR reuse the lowest row).\n"
"                               -b is then read literally as bps. Real limit left = the\n"
"                               transport header (ADTS 56 bit/frame). Upper 6144*ch\n"
"                               ceiling stays. Off-spec, deliberate opt-in.\n"
"\n== B. Stereo coding ==\n"
"   -- MS (mid/side), core -- \n"
" --msmask <n>                Force MS: -1 auto(def), 0 off (pure L/R), 1 on (all bands).\n"
" --msbands <n>               MS only on the N LOWEST SFBs (0..n-1). -1 no limit(def).\n"
" --msbands-lo <n>            MS band RANGE start (lowest SFB number). Pair with -hi.\n"
" --msbands-hi <n>            MS band RANGE end (highest SFB). Together lo..hi = MS only\n"
"                               in that range (e.g. 5 highest bands). -1 = off.\n"
" --ms-bias <n>               Bias L/R-vs-MS threshold, Q8 (128=+0.5 ld64). >0=MS\n"
"                               eager. WEAK effect (MS is lossless, only affects\n"
"                               bit-packing) - for real control use --msmask 0/1.\n"
"                               0..255, -1 off(def).\n"
" --ms-precision <n>          Q8, >256 = shallower holes in MS bands (more bits spent,\n"
"                               LAME -q feel). 256=no change, 384~1.5x, 512~2x. Audible\n"
"                               up to ~600-800; higher hits FDK's threshold floor and\n"
"                               under CBR just shifts bits around. No upper limit.\n"
" --mid-bias <n>              Q8, >256 = deliberately raise the MID (L+R) threshold to\n"
"                               free bits for the side channel. Use sparingly (mid carries\n"
"                               most loudness). 256=off.\n"
" --side-bias <dB>            -24.0..50.0, 0=off. Shift the SIDE (L-R) masking threshold\n"
"                               on MS bands. + steers MORE bits to side (cleaner stereo\n"
"                               width/ambience) at mid's expense; - deliberately DEGRADES\n"
"                               side (narrower, for extreme low bitrate, your call). This\n"
"                               is the same energy-vs-threshold lever LAME and MusePack --ms\n"
"                               ride for their stereo. Try +3..+9.\n"
" --side-knee <dB>            -24.0..50.0, 0=off. Shape where a side SFB flips coded<->\n"
"                               zeroed, as an OFFSET on each band's own margin: precision\n"
"                               follows how loud the band really is (also bands FDK\n"
"                               rescued at 0.97 dB). + = finer, bands under thr kept;\n"
"                               - = coarser, the weakest leave first. Try -3..+6.\n"
" --side-cutoff <Hz>          1000..24000, off by default. Above it every stereo band is\n"
"                               pure mid: side zeroed, no IS there, cheap MS mask. The\n"
"                               side bits go to the rest. Stereo stays below the cutoff.\n"
" --is-admit <x>              0.0..20.0, stock 5. FDK allows intensity stereo only when\n"
"                               bitrate / bandwidth (bit/s per Hz) is below x. 0 = never.\n"
"                               --is-aggression still opens the gate.\n"
" --side-knee-zone <x>        1.0..20.0, stock 3. Diagnostic: the offset fades out\n"
"                               between |knee| and x*|knee| dB above the threshold.\n"
" --side-knee-spread <dB>     0.1..24.0, stock 1. Diagnostic: knee (-) needed to spread\n"
"                               the rescued bands fully by their real margin.\n"
" --mask-slope <dB>           -24.0..50.0, 0=off. Global (mid+side) non-masking heuristic:\n"
"                               FDK relaxes required SNR for SFBs whose energy sits far\n"
"                               (>~10 dB stock) below the frame average - starving quiet\n"
"                               bands to save bits. + raises that threshold => fewer bands\n"
"                               starved => more detail in reverb tails/quiet passages (costs\n"
"                               bits); - lowers it => starve quiet bands harder (leaner,\n"
"                               hollower). Same family as --side-bias but for both channels.\n"
"   -- IS (intensity) --      \n"
" --is <n>                    Intensity stereo: -1 auto(def), 0 off, 1 on.\n"
" --isbands <n>               Max SFBs allowed to use intensity. -1 no limit(def).\n"
" --is-min-sfbs <n>           Min contiguous SFBs before IS kicks in. Higher = IS rarer.\n"
"                               -1 def(6). (Advanced; prefer --is-aggression.)\n"
" --is-corr-thresh <n>        IS L/R correlation threshold, Q8 (256=1.0). LOWER = IS more\n"
"                               eager (counter-intuitive). -1 def(~243=0.95).\n"
" --is-lr-ratio <n>           IS panning L/R ratio threshold, Q8. -1 def(~179=0.7).\n"
" --is-lo <sfb>               Allow IS only from this SFB up (band range). Outside the\n"
"                               range stays L/R. Only RESTRICTS where FDK may place IS.\n"
" --is-hi <sfb>               Allow IS only up to this SFB (inclusive). -1 off.\n"
" --is-force-lo <sfb>         FORCE intensity stereo from this SFB, bypassing the\n"
"                               correlation/min-sfbs/loudness gates. Laboratory mode: can\n"
"                               deliberately wreck the stereo image. Stream stays legal.\n"
" --is-force-hi <sfb>         Upper SFB of the forced-IS range (inclusive). -1 off.\n"
"   -- PS (parametric stereo, HE-AAC v2) -- \n"
" --ps <n>                    Force PS IID sending: -1 auto(def), 0 off, 1 on.\n"
" --ps-iid-quant <n>          PS IID quant grid: -1 def, 0 coarse, 1 fine.\n"
" --ps-icc <n>                Force ICC (channel coherence): -1 auto, 0 off, 1 on.\n"
" --ps-ipd <0|1>              Transmit IPD (inter-channel PHASE difference) in the PS\n"
"                               extension. 0/-1 = off (stock FDK behaviour, phase is\n"
"                               never sent). PS otherwise reconstructs the stereo image\n"
"                               from level differences and coherence alone; IPD adds the\n"
"                               phase relationship, which matters for low/low-mid content,\n"
"                               wide soft sources, reverb and inter-channel micro-delays.\n"
"                               Coded for the lower 5 (10-band) / 11 (20-band) parameter\n"
"                               bands per the MPEG-4 PS syntax. SAFE: the data lives in a\n"
"                               length-prefixed extension, so decoders without phase\n"
"                               synthesis skip it (explicitly permitted by the standard).\n"
"                               OPD is not sent - the decoder derives it from a joint\n"
"                               model rather than measuring it.\n"
" --ps-opd <0|1>              Only meaningful with --ps-ipd 1. OPD is the phase of the\n"
"                               LEFT channel relative to the MONO downmix, and the\n"
"                               decoder needs both: it rotates the downmix-fed paths by\n"
"                               OPD and the others by (OPD - IPD), so the DIFFERENCE is\n"
"                               always IPD but where it sits relative to the downmix is\n"
"                               set by OPD. Default (1) computes it from the same\n"
"                               accumulators; 0 pins it to zero, which makes the phase\n"
"                               error asymmetric between channels. Provided so the two\n"
"                               can be compared on identical material.\n"
" --ps-icc-mode <n>           ICC rotation: -1 def, 0 ROT_A, 1 ROT_B. Signalling only --\n"
"                               same matrix, computed differently by the decoder.\n"
" --ps-bands <10|20>          PS stereo bands = FREQUENCY resolution of the stereo\n"
"                               parameters. -1 = pick from bitrate (>=22k always 20).\n"
"                               Fewer bands = coarser image, fewer bits. Audibly more\n"
"                               significant than the ICC rotation mode.\n"
" --ps-env <1|2|4>            PS parameter envelopes per frame = TIME resolution of the\n"
"                               stereo parameters. -1 = pick from bitrate (>=36k gives 4).\n"
"                               More envelopes track moving panorama/transients better.\n"
" --ps-env-reduce <0|1>       0 = disable the automatic envelope-halving loop, which\n"
"                               otherwise silently collapses 4 envelopes to 1 whenever\n"
"                               successive envelopes look similar. 0 makes --ps-env literal.\n"
" --ps-noenv-skip <0|1>       0 = never emit parameter-less PS frames. Stock FDK may send\n"
"                               up to 10 consecutive frames with NO stereo parameters when\n"
"                               successive IID/ICC sets look similar (can be heard as the\n"
"                               image briefly collapsing). 0 = always send.\n"
"\n== C. Bandwidth & SBR (high band) ==\n"
" --sbr-tran-peak <n>         SBR transient detector: how PEAKY an attack must be, raw\n"
"                               x100 (90 = stock 0.90). A slot counts as a transient only\n"
"                               once the signal drops below this fraction of the previous\n"
"                               slot. Higher = fewer transients (attacks smear across the\n"
"                               frame), lower = more. -1 = default.\n"
" --sbr-tran-quiet <n>        ELD+SBR only (-p 39 -L 1): quiet-slot energy floor of the\n"
"                               FAST transient detector, x100 (100 = stock). Lower =\n"
"                               quiet passages produce SBR transients more readily.\n"
"                               Measured: 30..99 change the stream, above 100 no change\n"
"                               was seen. No effect in HE-AAC/HE-AAC v2.\n"
" --sbr-tran-dom <n>          ELD+SBR only: how much louder (x100, 140 = stock 1.4) a\n"
"                               slot must be than the two before it when those are\n"
"                               already transients. Measured: no stream change on our\n"
"                               test material (100..500); kept for experiments.\n"
" --sbr-tran-thr <n>          Scale the master transient threshold, value*100. Lower =\n"
"                               detector fires more readily overall.\n"
" --sbr-tran-split <n>        Scale the envelope-split threshold, value*100. Lower = split\n"
"                               a transient-free frame into 2 envelopes more often (better\n"
"                               time resolution, costs bits).\n"
" --sbr-mh-tone <n>           Missing harmonics: scale how TONAL a band must be before SBR\n"
"                               adds a synthetic harmonic, value*100. Lower = more added\n"
"                               tones (risk of whistling); higher = fewer (dull bells and\n"
"                               cymbals, lost partials).\n"
" --sbr-mh-diff <n>           Scale the orig-vs-patched tonality DIFFERENCE that triggers\n"
"                               harmonic compensation, value*100.\n"
" --sbr-mh-decay-orig <n>     Scale how long an already-found tone keeps being tracked as\n"
"                               it decays, value*100. Audible on bell/cymbal tails.\n"
" --sbr-mh-decay-diff <n>     Same, for the difference guide, value*100.\n"
" --sbr-mh-sfm-sbr <n>        Scale the spectral-flatness threshold above which the PATCHED\n"
"                               band counts as noise-like rather than tonal, value*100.\n"
" --sbr-mh-sfm-orig <n>       Same for the ORIGINAL band, value*100. Together these two\n"
"                               catch \"one tone in the original became several after\n"
"                               patching\".\n"
" --sbr-mh-maxcomp <n>        Cap on envelope compensation around a synthetic harmonic,\n"
"                               raw (50 = stock). Affects neighbouring bands and how much\n"
"                               noise sits next to the added tone.\n"
" --sbr-mh-deltatime <n>      Max transient distance for a frame to count as transient in\n"
"                               the MH detector, raw (9 for AAC, 16 for LD).\n"
" --sbr-noise-max <6|3|-3>    Ceiling on how loud the noise SBR injects may get: 6 = 1.0,\n"
"                               3 = 0.5, -3 = 0.125. The \"air vs hiss\" limit up top.\n"
"  SBR estimators (inverse filtering stays DYNAMIC, these bound/bias/weigh it;\n"
"  --sbr-invf forces a fixed level and wins over all of them):\n"
" --sbr-softness <0..7>       One macro: 0 = harder/more metallic than music tuning,\n"
"                               2 = stock music estimators, 4 = stock speech\n"
"                               estimators (crossover untouched), 7 = very soft.\n"
"                               1 = never HIGH whitening, 3 = at least LOW, 5..7 also\n"
"                               lift the noise ceiling (5,6 = 0.5, 7 = 1.0).\n"
"                               Explicit knobs below win over it. -1 off.\n"
" --sbr-invf-min <0..3>       Lowest invf level the estimator may pick. -1 off.\n"
" --sbr-invf-max <0..3>       Highest invf level the estimator may pick (ceiling,\n"
"                               e.g. 2 = never HIGH). -1 off.\n"
" --sbr-invf-bias <-3..3>     Shift every dynamic invf decision by n steps (+ = more\n"
"                               whitening). Applied before min/max. 0 = off.\n"
" --sbr-invf-table <0|1>      invf decision table: 0 music, 1 speech. Only the table,\n"
"                               no crossover change. -1 = follows --speech.\n"
" --sbr-invf-hyst <x>         Hysteresis of the decision borders, decimal (1.0 =\n"
"                               stock, 0 = none = more frame-to-frame switching).\n"
" --sbr-invf-sbr-thr <x>      Shift the PATCHED-band tonality borders, about dB\n"
"                               (-20..20). + = patched band counts as tonal sooner =\n"
"                               more invf.\n"
" --sbr-invf-orig-thr <x>     Shift the ORIGINAL-band tonality borders, about dB\n"
"                               (-20..20). + = source counts as tonal later = more invf.\n"
" --sbr-invf-smooth <0..2>    Frames the estimator averages (2 = stock, 0 = this frame\n"
"                               only = faster reaction, more switching). -1 off.\n"
" --sbr-nf-weight <x>         Noise-floor weight of the orig-vs-SBR tonality gap,\n"
"                               0.0..1.0 (music 0.25, speech 1.0). Higher = more fill\n"
"                               noise where the patch is too tonal. -1 = follows --speech.\n"
"                               Audible only below the noise ceiling: with the table\n"
"                               ceiling (0.125) most bands are clipped, so pair it\n"
"                               with --sbr-noise-max 3 or 6.\n"
" --sbr-nf-weight-from <1..3> Lowest invf level whose bands get that weight (3 = stock,\n"
"                               HIGH only; 1 = LOW, MID and HIGH). -1 off.\n"
" --sbr-nf-smooth <0>         0 = no time smoothing of the noise floor (stock smooths\n"
"                               over 4 frames). -1 off.\n"

" --core-cutoff <hz>          Force core bandwidth in Hz even under SBR (where -w is\n"
"                               ignored). 0..48000, 0=default; the encoder stops at the core\n"
"                               Nyquist (half the core sample rate). See --uncap-bandwidth.\n"
" --sbr-start <n>             SBR bs_start_freq index: -1 def, 0..15. (Validated by FDK;\n"
"                               bad start/stop pair => init fails.)\n"
" --sbr-stop <n>              SBR bs_stop_freq index: -1 def, 0..13.\n"
" --sbr-freqscale <n>         Freq grouping: -1 def, 0..3. 0 = linear, 1/2/3 = 12/10/8\n"
"                               bands per octave (1 finest, 3 coarsest and cheapest).\n"
" --sbr-alterscale <n>        Alternative scale resolution: -1 def, 0/1.\n"
" --sbr-noise-bands <n>       SBR noise-floor density (bs_noise_bands, 2-bit field):\n"
"                               -1 def, 0 = one band, 1/2/3 = ~1/2/3 bands per octave.\n"
" --sbr-amp-res <n>           Envelope amplitude res: -1 def, 0=1.5dB, 1=3.0dB.\n"
" --sbr-data-extra <n>        Write extra SBR header data: -1 def, 0/1.\n"
" --sbr-num-env <1|2|4>       Envelopes per frame = SBR time resolution. FORCES static\n"
"                               framing (ignores transient detector); worse on sharp attacks.\n"
"                               -1 off.\n"
" --sbr-freqres-fixfix <0|1>  FIXFIX envelope freq resolution (0 low, 1 high). -1 off.\n"
" --sbr-stereo-mode <0..3>    SBR stereo: 0 mono, 1 LR (full separation), 2 coupling\n"
"                               (cheap), 3 switch-LRC (default, per-frame). -1 off.\n"
" --sbr-invf <0..3>           Force inverse filtering: 0 off,1 low,2 mid,3 high. Higher =\n"
"                               less metallic top, less detail. -1 = auto (tonality est.).\n"
" --sbr-noise-floor-offset <n> SBR noise floor offset (small int). Bigger = more fill\n"
"                               noise in the reconstruction. off=-128.\n"
" --sbr-header-period <n>     Frames between SBR headers = how fast SBR locks when a\n"
"                               decoder tunes into an Icecast/Shoutcast/HLS stream.\n"
"                               1 = SBR config in every frame => near-instant (no core-\n"
"                               only moment); higher = longer core-only period. FDK\n"
"                               default ~10 frames (~0.46 s @44.1k). Taken literally, no\n"
"                               1 s ceiling. -1 off. See verbose.\n"
" --sbr-header-time <s>       The same as a TIME: seconds between SBR headers, decimal\n"
"                               (0.01..600, e.g. 3.5). Wins over --sbr-header-period.\n"
"                               HE-AAC/HEv2: in-band sbr_header. AAC-ELD+SBR: its SBR\n"
"                               header exists only in the config, so this sets the\n"
"                               LATM/LOAS StreamMuxConfig period (-f 6/7/10).\n"
"\n== D. Masking / noise shaping / detail ==\n"
" --ath-scale <n>             Global masking-threshold scale Q8 (256=x1.0 def). >256 =\n"
"                               noisier/fewer bits, <256 = cleaner/more detail. 1..4096.\n"
"                               This is the main global SMR knob.\n"
" --spread-mask <n>           Q8, scales inter-band masking spread. <256 = LESS masking =\n"
"                               more detail kept (like relaxing tone-masks-noise). Biggest\n"
"                               effect where bits are tight (96-192k). 256=no change.\n"
" --minsnr-scale <n>          MusePack-style: scale the required per-band coding SNR. Q8,\n"
"                               <256 = demand HIGHER SNR (more detail/bits), >256 = coarser.\n"
"                               More effective than --ath-scale (it drives what avoid-holes\n"
"                               clamps back to). 256=off.\n"
" --minsnr-clamp-hi <n>       Q8 scale on FDK's MAX_SNR ceiling (~-1 dB). >256 lets bands\n"
"                               demand more than the stock cap. 256=off.\n"
" --minsnr-clamp-lo <n>       Q8 scale on FDK's MIN_SNR floor (~-25 dB). 256=off.\n"
" --reduce-clamp <0|1>        1=default. 0 drops the 29 dB threshold-reduction ceiling in\n"
"                               CBR, letting thresholds be pushed deeper (pairs with\n"
"                               --minsnr-scale for extreme detail).\n"
" --tns-mask <n>              TNS enable mask: -1 def(0xF), 0..15.\n"
" --tns-order <n>             Max TNS order: -1 def, 1..12 (short<=5).\n"
" --pns <n>                   Perceptual Noise Substitution: -1 def, 0 off, 1 on.\n"
"                               (FDK forces off under VBR or when TNS is off.)\n"
" --pns-start <hz>            PNS start freq Hz: -1 def (lower = more noise-coded).\n"
" --force-pns                 Bypass the low-bitrate PNS gate (tuning table disables PNS\n"
"                               below ~28 kbps). Lets you force PNS e.g. at 24 kbps.\n"
" --pns-gain <x>              Loudness of the fabricated PNS noise. x>=0.0, 1.0=unchanged\n"
"                               (noise energy = original band). >1.0 louder, <1.0 quieter.\n"
"                               Geeky: deliberately over/under-drive the noise fill.\n"
" --pns-shared <0|1>          Bands that BOTH channels code as PNS: 1 = one shared noise\n"
"                               (same noise in L and R), 0 = independent noise. Stock picks\n"
"                               per band by correlation, which forces a per-band MS mask\n"
"                               (up to ~25 bits/frame); a uniform choice lets the mask fall\n"
"                               back to 2 bits. Noise levels stay per channel (format).\n"
" --pns-hold <dB>             0.1..12.0. Neighbouring PNS bands of a channel whose noise\n"
"                               levels differ by no more than this share one level: the\n"
"                               delta costs 1 bit instead of 3..9. Coarser noise envelope.\n"
" --pns-budget <0|1>          1 = count the bits PNS noise levels take: the bit target for\n"
"                               the spectrum is lowered by them, and when a frame overflows\n"
"                               the emergency cut gets their bits back instead of cutting\n"
"                               on into the coded spectrum below (stock books PNS as free;\n"
"                               at low rates that zeroes whole frames up to ~1.5 kHz and\n"
"                               crackles in frame rhythm). 0 = stock.\n"
" --pns-tonality <x>          Scales the PNS tonality detection threshold. 1.0=default,\n"
"                               higher = more (also less-noisy) bands become PNS = wider\n"
"                               noise substitution.\n"
" --pns-refpower <x>          Scales the PNS reference-power detection threshold. 1.0=def.\n"
"                               Higher = stricter gate, FEWER PNS bands; lower = more.\n"
" --pns-gapfill <x>           Scales the PNS gap-fill threshold. 1.0=default.\n"
" --pns-min-width <n>         Minimum SFB width for PNS (raw). Lower = PNS on narrower bands.\n"
"\n== E. Core transients, pre-echo and spectral holes (LC/LD/ELD core) ==\n"
"   All decimal knobs take '.' or ',' and up to 3 places; out of range = error.\n"
" --preecho-rise <dB>         0.0..24.0. How far a band's masking threshold may RISE\n"
"                               from one long frame to the next. Stock 3.01 (x2).\n"
"                               Lower = quantisation noise cannot run ahead of an\n"
"                               attack inside a long block (less pre-echo, more bits).\n"
"                               Above 3.01 = looser. Off by default.\n"
" --preecho-depth <dB>        0.0..60.0. How far pre-echo control may LOWER a threshold\n"
"                               at most. Stock 20.0. Pairs with --preecho-rise.\n"
" --attack-decay <x>          0.0..0.99. Memory of the attack detector: share of the\n"
"                               running energy kept per sub-window. Stock 0.70. Lower =\n"
"                               compares with the very last moment (catches attacks right\n"
"                               after loud passages), higher = with a longer average.\n"
" --attack-floor <dB>         -60.0..30.0. Shift of the minimum energy an attack must\n"
"                               have. - = quiet attacks (brushes, soft hi-hats) also\n"
"                               switch to short blocks; + = only loud ones. 0 = stock.\n"
" --tns-thresh <x>            0.5..4.0. Multiplier on the prediction gain TNS needs to\n"
"                               switch on (stock 1.0). <1 = TNS shapes noise in time on\n"
"                               more frames (tighter attacks, LD/ELD's main tool, as\n"
"                               they have no short blocks); >1 = more rarely.\n"
" --tns-start <Hz>            Lowest frequency of the lower TNS filter in long blocks.\n"
"                               Stock ~1.4 kHz @48k. Lower = TNS reaches deeper. LC only.\n"
" --window-shape <0|1>        AAC-LC: 0 = sine window for every block, 1 = KBD for every\n"
"                               block. Stock: KBD long, sine short. KBD separates distant\n"
"                               frequencies better, sine is sharper next to a peak.\n"
" --short-groups <0..3>       How the 8 short windows are grouped (one scalefactor set\n"
"                               per group). 0 stock table, 1 one group (cheapest), 2 four\n"
"                               pairs, 3 tight: attack window alone, most bits there.\n"
" --avoid-holes <0|1>         Peak/valley shaping that keeps narrow bands from being\n"
"                               zeroed. Stock switches it OFF below 20 kbps per channel,\n"
"                               i.e. below 40 kbps STEREO. 1 = force on, 0 = force off.\n"
" --hole-start <sfb>          0..51. First long-block SFB where bands may be cut to\n"
"                               zero when bits run out. Stock 15 (0 when holes are off).\n"
"                               Higher = the lower spectrum is protected.\n"
" --hole-start-short <sfb>    0..15. The same for short blocks. Stock 3.\n"
" --hole-spread <dB>          0.0..24.0. How much neighbour (spread) energy is lowered\n"
"                               before a band counts as covered by its neighbours and\n"
"                               may become a hole. Stock 3.0 long, 2.0 short. Higher =\n"
"                               fewer bands count as covered = more are protected.\n"
" --hole-ladder <1..8>         Energy steps of the hole cutting. Stock 8. Fewer = only\n"
"                               the quietest bands may be cut; the rate loop then gets\n"
"                               coarser instead of cutting more bands.\n"
"\n== E2. Coverage vs precision (what the bits are spent on) ==\n"
" --coverage <-5..5>          One axis at a fixed bitrate. + = code EVERY band, coarsely\n"
"                               (no holes, everything filled, grainier). - = code fewer\n"
"                               bands, cleanly (marginal bands dropped, holes allowed).\n"
"                               0 = stock. Sets the six switches below (+4/+5 also\n"
"                               --pns-above); any of them\n"
"                               given explicitly wins.\n"
" --band-knee <dB>            -24.0..24.0, 0 = off. Every band of every channel: + keeps\n"
"                               bands up to N dB under their threshold at the coarsest\n"
"                               scalefactor instead of zeroing them; - zeroes bands that\n"
"                               clear the threshold by less than N dB. (--side-knee does\n"
"                               the same for the side channel only and wins there.)\n"
" --quant-round <x>           0.0..0.5. Rounding offset of the quantizer. Stock 0.405.\n"
"                               0.5 = round to nearest (fewer small lines become zero,\n"
"                               costs bits); lower = more lines to zero (cheaper).\n"
" --pns-above <Hz>            1000..48000. Every long-block band from this frequency up\n"
"                               is replaced by noise of the right level (PNS), whatever\n"
"                               its tonality: a high band \"like SBR, without SBR\".\n"
"                               Turns PNS on even below its bitrate gate. AAC-LC only.\n"
" --thr-hold <n>              1..32 frames, 1 = off. The masking model is re-measured\n"
"                               only every n long frames; between, each band keeps its\n"
"                               last noise-to-signal offset. Noise lags the music.\n"
" --scf-range <n>             0..60, stock 60. Max spread between the finest and the\n"
"                               coarsest band of a channel, steps of 1.5 dB. Lower =\n"
"                               flatter noise, less psychoacoustic shaping.\n"
" --bit-stats                 Count where the bits go (transport header, element\n"
"                               headers, MS mask, sections, scalefactors, PNS, TNS,\n"
"                               spectrum per channel/mid/side, SBR/PS, fill) and\n"
"                               print the table at the end. Does not change the stream.\n"
" --enfac-patch <0|1>         Energy re-weighting of flat (noise-like) long frames,\n"
"                               CBR and VBR. Stock: only when the audible spectrum is\n"
"                               flat; it moves bits between bands. 0 = never (every\n"
"                               frame treated as tonal), 1 = every long frame.\n"
"\n== F. Block switching & bitrate control ==\n"
" --block-bias <n>            Bias short/long block decision. 128=default(unchanged),\n"
"                               >128 favours short (transient-like), <128 long, 0=long\n"
"                               only. Always standards-compliant. 0..255, -1 off.\n"
" --vbr-reservoir <bits>      Quasi-VBR: bit-reservoir size. Bigger = more per-frame\n"
"                               breathing around the average. Auto-clamped to\n"
"                               6144*ch - avg-bits. Start ~2-3x default. -1 off.\n"
" --peak-bitrate <bps>        Allow short-term peaks up to this while keeping the average.\n"
"                               Use ABOVE your -b (e.g. -b 128000 --peak-bitrate 160000).\n"
"                               -1 off.\n"
" --max-bits-frame <bits>     Hard ceiling of bits in ONE frame. >=avg, <=6144*ch.\n"
"                               Too low starves loud frames. -1 off.\n"
" --min-bits-frame <bits>     Hard floor of bits per frame. 0..avg. Higher wastes bits on\n"
"                               silence. Keep 0 unless experimenting. -1 off.\n"
" --bitres-mode <n>           Reservoir mode: 0 full(def), 1 reduced, 2 disabled (rigid\n"
"                               per-frame CBR). -1 leave to encoder.\n"
"  TIP quasi-VBR that does not choke after peaks (~128k stereo):\n"
"    -b 128000 --peak-bitrate 200000 --vbr-reservoir 12000\n"
"  Cap for all: 6144 bits/channel/frame. Do NOT set max-bits below avg or\n"
"  min-bits above it - that fights the target and hurts quality.\n"
"\n== G. DAB+ output ==\n"
" --dab                       Emit a DAB+ superframe bitstream (ETSI TS 102 563):\n"
"                               transform 960, superframe + firecode + Reed-Solomon.\n"
"                               Requires sample rate 32000 or 48000 and bitrate a\n"
"                               multiple of 8 kbps (8..192). Profile is auto-picked\n"
"                               from bitrate/channels (AAC-LC / HE-AAC / HE-AAC v2),\n"
"                               override with -p. Output is a raw .dabp superframe\n"
"                               stream (feed odr-dabmux etc.).\n"
" --dab-label <text>          Static Dynamic Label (DLS) carried as X-PAD in the\n"
"                               superframe (shown by DAB receivers). Up to ~48 chars.\n"
    , fdkaac_version);
}

typedef struct aacenc_param_ex_t {
    AACENC_PARAMS

    char *input_filename;
    FILE *input_fp;
    char *output_filename;
    FILE *output_fp;
    unsigned gapless_mode;
    unsigned include_sbr_delay;
    unsigned ignore_length;
    int silent;
    int moov_before_mdat;
    int no_tool_tag;    /* skip the "©too" encoder-identifier tag */
    int minimal_moov;   /* skip the whole udta/meta metadata block */

    int is_raw;
    unsigned raw_channels;
    unsigned raw_rate;
    const char *raw_format;

    int no_timestamp;

    aacenc_tag_store_t tags;
    aacenc_tag_store_t source_tags;
    aacenc_translate_generic_text_tag_ctx_t source_tag_ctx;

    char *json_filename;
} aacenc_param_ex_t;

static
int parse_options(int argc, char **argv, aacenc_param_ex_t *params)
{
    int ch;
    int n;

#define OPT_INCLUDE_SBR_DELAY    M4AF_FOURCC('s','d','l','y')
#define OPT_MOOV_BEFORE_MDAT     M4AF_FOURCC('m','o','o','v')
#define OPT_NO_TOOL_TAG          M4AF_FOURCC('n','t','t','g')
#define OPT_MINIMAL_MOOV         M4AF_FOURCC('m','n','m','v')
#define OPT_RAW_CHANNELS         M4AF_FOURCC('r','c','h','n')
#define OPT_RAW_RATE             M4AF_FOURCC('r','r','a','t')
#define OPT_RAW_FORMAT           M4AF_FOURCC('r','f','m','t')
#define OPT_SHORT_TAG            M4AF_FOURCC('s','t','a','g')
#define OPT_SHORT_TAG_FILE       M4AF_FOURCC('s','t','g','f')
#define OPT_LONG_TAG             M4AF_FOURCC('l','t','a','g')
#define OPT_TAG_FROM_JSON        M4AF_FOURCC('t','f','j','s')

/* ---- Frankenstein debug switches ---- */
#define OPT_FR_MS_MASK           M4AF_FOURCC('f','m','s','m')
#define OPT_FR_MS_BANDS          M4AF_FOURCC('f','m','s','b')
#define OPT_FR_IS                M4AF_FOURCC('f','i','s','_')
#define OPT_FR_IS_BANDS          M4AF_FOURCC('f','i','s','b')
#define OPT_FR_IS_MINSFBS        M4AF_FOURCC('f','i','s','m')
#define OPT_FR_IS_CORR           M4AF_FOURCC('f','i','s','c')
#define OPT_FR_IS_LRRATIO        M4AF_FOURCC('f','i','s','r')
#define OPT_FR_CORE_CUTOFF       M4AF_FOURCC('f','c','c','o')
#define OPT_FR_SBR_START         M4AF_FOURCC('f','s','b','1')
#define OPT_FR_SBR_STOP          M4AF_FOURCC('f','s','b','2')
#define OPT_FR_SBR_FREQSCALE     M4AF_FOURCC('f','s','b','3')
#define OPT_FR_SBR_ALTERSCALE    M4AF_FOURCC('f','s','b','4')
#define OPT_FR_SBR_NOISE_BANDS   M4AF_FOURCC('f','s','b','5')
#define OPT_FR_SBR_AMP_RES       M4AF_FOURCC('f','s','b','6')
#define OPT_FR_SBR_DATA_EXTRA    M4AF_FOURCC('f','s','b','7')
#define OPT_FR_PS                M4AF_FOURCC('f','p','s','_')
#define OPT_FR_PS_IID_QUANT      M4AF_FOURCC('f','p','s','q')
#define OPT_FR_TNS_MASK          M4AF_FOURCC('f','t','n','m')
#define OPT_FR_TNS_ORDER         M4AF_FOURCC('f','t','n','o')
#define OPT_FR_PNS               M4AF_FOURCC('f','p','n','s')
#define OPT_FR_PNS_START         M4AF_FOURCC('f','p','n','1')
#define OPT_FR_ATH_SCALE         M4AF_FOURCC('f','a','t','h')
#define OPT_FR_BLOCK_BIAS        M4AF_FOURCC('f','b','b','i')
#define OPT_FR_VBR_RESERVOIR     M4AF_FOURCC('f','v','r','s')
#define OPT_FR_MAX_BITS_FRAME    M4AF_FOURCC('f','m','x','b')
#define OPT_FR_MIN_BITS_FRAME    M4AF_FOURCC('f','m','n','b')
#define OPT_FR_BITRES_MODE       M4AF_FOURCC('f','b','r','m')
#define OPT_FR_MS_BIAS           M4AF_FOURCC('f','m','s','x')
#define OPT_FR_UNCAP_BW          M4AF_FOURCC('f','u','b','w')
#define OPT_FR_IS_AGGRESSION     M4AF_FOURCC('f','i','a','g')
#define OPT_FR_FORCE_PNS         M4AF_FOURCC('f','f','p','n')
#define OPT_FR_UNLOCK_BITRATE    M4AF_FOURCC('f','u','b','r')
#define OPT_FR_SPEECH            M4AF_FOURCC('f','s','p','c')
#define OPT_FR_SPREAD_MASK       M4AF_FOURCC('f','s','p','m')
#define OPT_FR_MS_BAND_LO        M4AF_FOURCC('f','m','l','o')
#define OPT_FR_MS_BAND_HI        M4AF_FOURCC('f','m','h','i')
#define OPT_FR_MS_PRECISION      M4AF_FOURCC('f','m','p','r')
#define OPT_FR_SBR_NUM_ENV       M4AF_FOURCC('f','s','n','e')
#define OPT_FR_SBR_FREQRES_FIXFIX M4AF_FOURCC('f','s','f','r')
#define OPT_FR_SBR_STEREO_MODE   M4AF_FOURCC('f','s','s','m')
#define OPT_FR_SBR_INVF          M4AF_FOURCC('f','s','i','v')
#define OPT_FR_SBR_NF_OFFSET     M4AF_FOURCC('f','s','n','f')
#define OPT_FR_PS_ICC            M4AF_FOURCC('f','p','i','c')
#define OPT_FR_PS_ICC_MODE       M4AF_FOURCC('f','p','i','m')
#define OPT_FR_PS_BANDS          M4AF_FOURCC('f','p','b','d')
#define OPT_FR_PS_ENV            M4AF_FOURCC('f','p','e','v')
#define OPT_FR_PS_ENV_REDUCE     M4AF_FOURCC('f','p','e','r')
#define OPT_FR_PS_NOENV_SKIP     M4AF_FOURCC('f','p','n','v')
#define OPT_FR_SBR_TRAN_PEAK     M4AF_FOURCC('f','t','p','k')
#define OPT_FR_SBR_TRAN_QUIET    M4AF_FOURCC('f','t','q','t')
#define OPT_FR_SBR_TRAN_DOM      M4AF_FOURCC('f','t','d','m')
#define OPT_FR_SBR_TRAN_THR      M4AF_FOURCC('f','t','t','h')
#define OPT_FR_SBR_TRAN_SPLIT    M4AF_FOURCC('f','t','s','p')
#define OPT_FR_SBR_MH_TONE       M4AF_FOURCC('f','m','t','n')
#define OPT_FR_SBR_MH_DIFF       M4AF_FOURCC('f','m','d','f')
#define OPT_FR_SBR_MH_DECAY_ORIG M4AF_FOURCC('f','m','d','o')
#define OPT_FR_SBR_MH_DECAY_DIFF M4AF_FOURCC('f','m','d','d')
#define OPT_FR_SBR_MH_SFM_SBR    M4AF_FOURCC('f','m','f','s')
#define OPT_FR_SBR_MH_SFM_ORIG   M4AF_FOURCC('f','m','s','o')
#define OPT_FR_SBR_MH_MAXCOMP    M4AF_FOURCC('f','m','m','c')
#define OPT_FR_SBR_MH_DELTATIME  M4AF_FOURCC('f','m','d','t')
#define OPT_FR_SBR_NOISE_MAX     M4AF_FOURCC('f','s','n','m')
#define OPT_FR_SBR_INVF_MIN      M4AF_FOURCC('f','s','i','n')
#define OPT_FR_SBR_INVF_MAX      M4AF_FOURCC('f','s','i','x')
#define OPT_FR_SBR_INVF_BIAS     M4AF_FOURCC('f','s','i','b')
#define OPT_FR_SBR_INVF_TABLE    M4AF_FOURCC('f','s','i','t')
#define OPT_FR_SBR_INVF_HYST     M4AF_FOURCC('f','s','i','h')
#define OPT_FR_SBR_INVF_SBR_THR  M4AF_FOURCC('f','s','i','s')
#define OPT_FR_SBR_INVF_ORIG_THR M4AF_FOURCC('f','s','i','o')
#define OPT_FR_SBR_INVF_SMOOTH   M4AF_FOURCC('f','s','i','m')
#define OPT_FR_SBR_NF_WEIGHT     M4AF_FOURCC('f','n','f','w')
#define OPT_FR_SBR_NF_WFROM      M4AF_FOURCC('f','n','f','f')
#define OPT_FR_SBR_NF_SMOOTH     M4AF_FOURCC('f','n','f','s')
#define OPT_FR_SBR_SOFTNESS      M4AF_FOURCC('f','s','s','o')
#define OPT_FR_PREECHO_RISE       M4AF_FOURCC('f','q','e','r')
#define OPT_FR_PREECHO_DEPTH      M4AF_FOURCC('f','q','d','p')
#define OPT_FR_ATTACK_DECAY       M4AF_FOURCC('f','q','a','d')
#define OPT_FR_ATTACK_FLOOR       M4AF_FOURCC('f','q','a','f')
#define OPT_FR_TNS_THRESH         M4AF_FOURCC('f','q','t','t')
#define OPT_FR_TNS_START          M4AF_FOURCC('f','q','t','s')
#define OPT_FR_WINDOW_SHAPE       M4AF_FOURCC('f','q','w','s')
#define OPT_FR_SHORT_GROUPS       M4AF_FOURCC('f','q','s','g')
#define OPT_FR_AVOID_HOLES        M4AF_FOURCC('f','q','a','h')
#define OPT_FR_HOLE_START         M4AF_FOURCC('f','q','h','l')
#define OPT_FR_HOLE_START_SHORT   M4AF_FOURCC('f','q','h','s')
#define OPT_FR_HOLE_SPREAD        M4AF_FOURCC('f','q','h','p')
#define OPT_FR_HOLE_LADDER        M4AF_FOURCC('f','q','h','d')
#define OPT_FR_BAND_KNEE          M4AF_FOURCC('f','q','b','k')
#define OPT_FR_QUANT_ROUND        M4AF_FOURCC('f','q','q','r')
#define OPT_FR_PNS_ABOVE          M4AF_FOURCC('f','q','p','a')
#define OPT_FR_THR_HOLD           M4AF_FOURCC('f','q','t','h')
#define OPT_FR_SCF_RANGE          M4AF_FOURCC('f','q','s','r')
#define OPT_FR_COVERAGE           M4AF_FOURCC('f','q','c','v')
#define OPT_FR_ENFAC_PATCH        M4AF_FOURCC('f','q','e','f')
#define OPT_FR_BIT_STATS          M4AF_FOURCC('f','q','b','s')
#define OPT_FR_SIDE_KNEE_ZONE     M4AF_FOURCC('f','s','k','z')
#define OPT_FR_SIDE_CUTOFF        M4AF_FOURCC('f','s','c','o')
#define OPT_FR_PNS_SHARED         M4AF_FOURCC('f','p','s','h')
#define OPT_FR_PNS_HOLD           M4AF_FOURCC('f','p','h','d')
#define OPT_FR_PNS_BUDGET         M4AF_FOURCC('f','p','b','g')
#define OPT_FR_IS_ADMIT           M4AF_FOURCC('f','i','a','d')
#define OPT_FR_SIDE_KNEE_SPREAD   M4AF_FOURCC('f','s','k','s')
#define OPT_FR_PS_IPD            M4AF_FOURCC('f','p','i','d')
#define OPT_FR_PS_OPD            M4AF_FOURCC('f','p','o','d')
#define OPT_FR_IS_BAND_LO        M4AF_FOURCC('f','i','l','o')
#define OPT_FR_IS_BAND_HI        M4AF_FOURCC('f','i','h','i')
#define OPT_FR_IS_FORCE_LO       M4AF_FOURCC('f','i','f','l')
#define OPT_FR_IS_FORCE_HI       M4AF_FOURCC('f','i','f','h')
#define OPT_FR_MINSNR_SCALE      M4AF_FOURCC('f','m','s','s')
#define OPT_FR_MINSNR_CLAMP_HI   M4AF_FOURCC('f','m','c','h')
#define OPT_FR_MINSNR_CLAMP_LO   M4AF_FOURCC('f','m','c','l')
#define OPT_FR_REDUCE_CLAMP      M4AF_FOURCC('f','r','d','c')
#define OPT_FR_MID_BIAS          M4AF_FOURCC('f','m','d','b')
#define OPT_FR_SIDE_BIAS         M4AF_FOURCC('f','s','d','b')
#define OPT_FR_SIDE_KNEE         M4AF_FOURCC('f','s','d','k')
#define OPT_FR_MASK_SLOPE        M4AF_FOURCC('f','m','s','l')
#define OPT_FR_SBR_HEADER_PERIOD M4AF_FOURCC('f','s','h','p')
#define OPT_FR_SBR_HEADER_TIME   M4AF_FOURCC('f','s','h','t')
#define OPT_FR_PNS_GAIN          M4AF_FOURCC('f','p','g','a')
#define OPT_FR_PNS_TONALITY      M4AF_FOURCC('f','p','t','o')
#define OPT_FR_PNS_REFPOWER      M4AF_FOURCC('f','p','r','p')
#define OPT_FR_PNS_GAPFILL       M4AF_FOURCC('f','p','g','f')
#define OPT_FR_PNS_MIN_WIDTH     M4AF_FOURCC('f','p','m','w')
#define OPT_FR_PEAK_BITRATE      M4AF_FOURCC('f','p','k','b')
#define OPT_FR_VERBOSE           M4AF_FOURCC('f','v','r','b')
#define OPT_FR_DAB               M4AF_FOURCC('f','d','a','b')
#define OPT_FR_DAB_LABEL         M4AF_FOURCC('f','d','a','l')

    static const struct option long_options[] = {
        { "help",             no_argument,       0, 'h' },
        { "profile",          required_argument, 0, 'p' },
        { "bitrate",          required_argument, 0, 'b' },
        { "bitrate-mode",     required_argument, 0, 'm' },
        { "bandwidth",        required_argument, 0, 'w' },
        { "afterburner",      required_argument, 0, 'a' },
        { "lowdelay-sbr",     required_argument, 0, 'L' },
        { "sbr-ratio",        required_argument, 0, 's' },
        { "transport-format", required_argument, 0, 'f' },
        { "adts-crc-check",   no_argument,       0, 'C' },
        { "header-period",    required_argument, 0, 'P' },

        { "gapless-mode",     required_argument, 0, 'G' },
        { "include-sbr-delay", no_argument,      0, OPT_INCLUDE_SBR_DELAY  },
        { "ignorelength",     no_argument,       0, 'I' },
        { "silent",           no_argument,       0, 'S' },
        { "moov-before-mdat", no_argument,       0, OPT_MOOV_BEFORE_MDAT   },
        { "no-tool-tag",      no_argument,       0, OPT_NO_TOOL_TAG        },
        { "minimal-moov",     no_argument,       0, OPT_MINIMAL_MOOV       },

        { "raw",              no_argument,       0, 'R' },
        { "raw-channels",     required_argument, 0, OPT_RAW_CHANNELS       },
        { "raw-rate",         required_argument, 0, OPT_RAW_RATE           },
        { "raw-format",       required_argument, 0, OPT_RAW_FORMAT         },

        { "title",            required_argument, 0, M4AF_TAG_TITLE         },
        { "artist",           required_argument, 0, M4AF_TAG_ARTIST        },
        { "album",            required_argument, 0, M4AF_TAG_ALBUM         },
        { "genre",            required_argument, 0, M4AF_TAG_GENRE         },
        { "date",             required_argument, 0, M4AF_TAG_DATE          },
        { "composer",         required_argument, 0, M4AF_TAG_COMPOSER      },
        { "grouping",         required_argument, 0, M4AF_TAG_GROUPING      },
        { "comment",          required_argument, 0, M4AF_TAG_COMMENT       },
        { "album-artist",     required_argument, 0, M4AF_TAG_ALBUM_ARTIST  },
        { "track",            required_argument, 0, M4AF_TAG_TRACK         },
        { "disk",             required_argument, 0, M4AF_TAG_DISK          },
        { "tempo",            required_argument, 0, M4AF_TAG_TEMPO         },
        { "tag",              required_argument, 0, OPT_SHORT_TAG          },
        { "tag-from-file",    required_argument, 0, OPT_SHORT_TAG_FILE     },
        { "long-tag",         required_argument, 0, OPT_LONG_TAG           },
        { "tag-from-json",    required_argument, 0, OPT_TAG_FROM_JSON      },

        { "no-timestamp",     no_argument,       0, '#' },

        /* ---- Frankenstein debug switches ---- */
        { "msmask",           required_argument, 0, OPT_FR_MS_MASK         },
        { "msbands",          required_argument, 0, OPT_FR_MS_BANDS        },
        { "is",               required_argument, 0, OPT_FR_IS              },
        { "isbands",          required_argument, 0, OPT_FR_IS_BANDS        },
        { "is-min-sfbs",      required_argument, 0, OPT_FR_IS_MINSFBS      },
        { "is-corr-thresh",   required_argument, 0, OPT_FR_IS_CORR         },
        { "is-lr-ratio",      required_argument, 0, OPT_FR_IS_LRRATIO      },
        { "core-cutoff",      required_argument, 0, OPT_FR_CORE_CUTOFF     },
        { "sbr-start",        required_argument, 0, OPT_FR_SBR_START       },
        { "sbr-stop",         required_argument, 0, OPT_FR_SBR_STOP        },
        { "sbr-freqscale",    required_argument, 0, OPT_FR_SBR_FREQSCALE   },
        { "sbr-alterscale",   required_argument, 0, OPT_FR_SBR_ALTERSCALE  },
        { "sbr-noise-bands",  required_argument, 0, OPT_FR_SBR_NOISE_BANDS },
        { "sbr-amp-res",      required_argument, 0, OPT_FR_SBR_AMP_RES     },
        { "sbr-data-extra",   required_argument, 0, OPT_FR_SBR_DATA_EXTRA  },
        { "ps",               required_argument, 0, OPT_FR_PS              },
        { "ps-iid-quant",     required_argument, 0, OPT_FR_PS_IID_QUANT    },
        { "tns-mask",         required_argument, 0, OPT_FR_TNS_MASK        },
        { "tns-order",        required_argument, 0, OPT_FR_TNS_ORDER       },
        { "pns",              required_argument, 0, OPT_FR_PNS             },
        { "pns-start",        required_argument, 0, OPT_FR_PNS_START       },
        { "ath-scale",        required_argument, 0, OPT_FR_ATH_SCALE       },
        { "block-bias",       required_argument, 0, OPT_FR_BLOCK_BIAS      },
        { "vbr-reservoir",    required_argument, 0, OPT_FR_VBR_RESERVOIR   },
        { "max-bits-frame",   required_argument, 0, OPT_FR_MAX_BITS_FRAME  },
        { "min-bits-frame",   required_argument, 0, OPT_FR_MIN_BITS_FRAME  },
        { "bitres-mode",      required_argument, 0, OPT_FR_BITRES_MODE     },
        { "ms-bias",          required_argument, 0, OPT_FR_MS_BIAS         },
        { "uncap-bandwidth",  no_argument,       0, OPT_FR_UNCAP_BW        },
        { "is-aggression",    required_argument, 0, OPT_FR_IS_AGGRESSION   },
        { "force-pns",        no_argument,       0, OPT_FR_FORCE_PNS       },
        { "unlock-bitrate",   no_argument,       0, OPT_FR_UNLOCK_BITRATE  },
        { "speech",           no_argument,       0, OPT_FR_SPEECH          },
        { "spread-mask",      required_argument, 0, OPT_FR_SPREAD_MASK     },
        { "msbands-lo",          required_argument, 0, OPT_FR_MS_BAND_LO        },
        { "msbands-hi",          required_argument, 0, OPT_FR_MS_BAND_HI        },
        { "ms-precision",        required_argument, 0, OPT_FR_MS_PRECISION      },
        { "sbr-num-env",         required_argument, 0, OPT_FR_SBR_NUM_ENV       },
        { "sbr-freqres-fixfix",  required_argument, 0, OPT_FR_SBR_FREQRES_FIXFIX },
        { "sbr-stereo-mode",     required_argument, 0, OPT_FR_SBR_STEREO_MODE   },
        { "sbr-invf",            required_argument, 0, OPT_FR_SBR_INVF          },
        { "sbr-noise-floor-offset",required_argument, 0, OPT_FR_SBR_NF_OFFSET     },
        { "ps-icc",              required_argument, 0, OPT_FR_PS_ICC            },
        { "ps-icc-mode",         required_argument, 0, OPT_FR_PS_ICC_MODE       },
        { "ps-bands",            required_argument, 0, OPT_FR_PS_BANDS          },
        { "ps-env",              required_argument, 0, OPT_FR_PS_ENV            },
        { "ps-env-reduce",       required_argument, 0, OPT_FR_PS_ENV_REDUCE     },
        { "ps-noenv-skip",       required_argument, 0, OPT_FR_PS_NOENV_SKIP     },
        { "sbr-tran-peak",         required_argument, 0, OPT_FR_SBR_TRAN_PEAK },
        { "sbr-tran-quiet",        required_argument, 0, OPT_FR_SBR_TRAN_QUIET },
        { "sbr-tran-dom",          required_argument, 0, OPT_FR_SBR_TRAN_DOM },
        { "sbr-tran-thr",          required_argument, 0, OPT_FR_SBR_TRAN_THR },
        { "sbr-tran-split",        required_argument, 0, OPT_FR_SBR_TRAN_SPLIT },
        { "sbr-mh-tone",           required_argument, 0, OPT_FR_SBR_MH_TONE },
        { "sbr-mh-diff",           required_argument, 0, OPT_FR_SBR_MH_DIFF },
        { "sbr-mh-decay-orig",     required_argument, 0, OPT_FR_SBR_MH_DECAY_ORIG },
        { "sbr-mh-decay-diff",     required_argument, 0, OPT_FR_SBR_MH_DECAY_DIFF },
        { "sbr-mh-sfm-sbr",        required_argument, 0, OPT_FR_SBR_MH_SFM_SBR },
        { "sbr-mh-sfm-orig",       required_argument, 0, OPT_FR_SBR_MH_SFM_ORIG },
        { "sbr-mh-maxcomp",        required_argument, 0, OPT_FR_SBR_MH_MAXCOMP },
        { "sbr-mh-deltatime",      required_argument, 0, OPT_FR_SBR_MH_DELTATIME },
        { "sbr-noise-max",       required_argument, 0, OPT_FR_SBR_NOISE_MAX },
        { "sbr-invf-min",        required_argument, 0, OPT_FR_SBR_INVF_MIN },
        { "sbr-invf-max",        required_argument, 0, OPT_FR_SBR_INVF_MAX },
        { "sbr-invf-bias",       required_argument, 0, OPT_FR_SBR_INVF_BIAS },
        { "sbr-invf-table",      required_argument, 0, OPT_FR_SBR_INVF_TABLE },
        { "sbr-invf-hyst",       required_argument, 0, OPT_FR_SBR_INVF_HYST },
        { "sbr-invf-sbr-thr",    required_argument, 0, OPT_FR_SBR_INVF_SBR_THR },
        { "sbr-invf-orig-thr",   required_argument, 0, OPT_FR_SBR_INVF_ORIG_THR },
        { "sbr-invf-smooth",     required_argument, 0, OPT_FR_SBR_INVF_SMOOTH },
        { "sbr-nf-weight",       required_argument, 0, OPT_FR_SBR_NF_WEIGHT },
        { "sbr-nf-weight-from",  required_argument, 0, OPT_FR_SBR_NF_WFROM },
        { "sbr-nf-smooth",       required_argument, 0, OPT_FR_SBR_NF_SMOOTH },
        { "sbr-softness",        required_argument, 0, OPT_FR_SBR_SOFTNESS },
        { "preecho-rise",         required_argument, 0, OPT_FR_PREECHO_RISE },
        { "preecho-depth",        required_argument, 0, OPT_FR_PREECHO_DEPTH },
        { "attack-decay",         required_argument, 0, OPT_FR_ATTACK_DECAY },
        { "attack-floor",         required_argument, 0, OPT_FR_ATTACK_FLOOR },
        { "tns-thresh",           required_argument, 0, OPT_FR_TNS_THRESH },
        { "tns-start",            required_argument, 0, OPT_FR_TNS_START },
        { "window-shape",         required_argument, 0, OPT_FR_WINDOW_SHAPE },
        { "short-groups",         required_argument, 0, OPT_FR_SHORT_GROUPS },
        { "avoid-holes",          required_argument, 0, OPT_FR_AVOID_HOLES },
        { "hole-start",           required_argument, 0, OPT_FR_HOLE_START },
        { "hole-start-short",     required_argument, 0, OPT_FR_HOLE_START_SHORT },
        { "hole-spread",          required_argument, 0, OPT_FR_HOLE_SPREAD },
        { "hole-ladder",          required_argument, 0, OPT_FR_HOLE_LADDER },
        { "band-knee",            required_argument, 0, OPT_FR_BAND_KNEE },
        { "quant-round",          required_argument, 0, OPT_FR_QUANT_ROUND },
        { "pns-above",            required_argument, 0, OPT_FR_PNS_ABOVE },
        { "thr-hold",             required_argument, 0, OPT_FR_THR_HOLD },
        { "scf-range",            required_argument, 0, OPT_FR_SCF_RANGE },
        { "coverage",             required_argument, 0, OPT_FR_COVERAGE },
        { "enfac-patch",          required_argument, 0, OPT_FR_ENFAC_PATCH },
        { "bit-stats",            no_argument,       0, OPT_FR_BIT_STATS },
        { "side-knee-zone",       required_argument, 0, OPT_FR_SIDE_KNEE_ZONE },
        { "side-cutoff",          required_argument, 0, OPT_FR_SIDE_CUTOFF },
        { "pns-shared",           required_argument, 0, OPT_FR_PNS_SHARED },
        { "pns-hold",             required_argument, 0, OPT_FR_PNS_HOLD },
        { "pns-budget",           required_argument, 0, OPT_FR_PNS_BUDGET },
        { "is-admit",             required_argument, 0, OPT_FR_IS_ADMIT },
        { "side-knee-spread",     required_argument, 0, OPT_FR_SIDE_KNEE_SPREAD },
        { "ps-ipd",              required_argument, 0, OPT_FR_PS_IPD            },
        { "ps-opd",              required_argument, 0, OPT_FR_PS_OPD            },
        { "is-lo",               required_argument, 0, OPT_FR_IS_BAND_LO        },
        { "is-hi",               required_argument, 0, OPT_FR_IS_BAND_HI        },
        { "is-force-lo",         required_argument, 0, OPT_FR_IS_FORCE_LO       },
        { "is-force-hi",         required_argument, 0, OPT_FR_IS_FORCE_HI       },
        { "minsnr-scale",        required_argument, 0, OPT_FR_MINSNR_SCALE      },
        { "minsnr-clamp-hi",     required_argument, 0, OPT_FR_MINSNR_CLAMP_HI   },
        { "minsnr-clamp-lo",     required_argument, 0, OPT_FR_MINSNR_CLAMP_LO   },
        { "reduce-clamp",        required_argument, 0, OPT_FR_REDUCE_CLAMP      },
        { "mid-bias",            required_argument, 0, OPT_FR_MID_BIAS          },
        { "side-bias",           required_argument, 0, OPT_FR_SIDE_BIAS         },
        { "side-knee",           required_argument, 0, OPT_FR_SIDE_KNEE         },
        { "mask-slope",          required_argument, 0, OPT_FR_MASK_SLOPE        },
        { "sbr-header-period",   required_argument, 0, OPT_FR_SBR_HEADER_PERIOD },
        { "sbr-header-time",     required_argument, 0, OPT_FR_SBR_HEADER_TIME   },
        { "pns-gain",            required_argument, 0, OPT_FR_PNS_GAIN          },
        { "pns-tonality",        required_argument, 0, OPT_FR_PNS_TONALITY      },
        { "pns-refpower",        required_argument, 0, OPT_FR_PNS_REFPOWER      },
        { "pns-gapfill",         required_argument, 0, OPT_FR_PNS_GAPFILL       },
        { "pns-min-width",       required_argument, 0, OPT_FR_PNS_MIN_WIDTH     },
        { "peak-bitrate",     required_argument, 0, OPT_FR_PEAK_BITRATE    },
        { "verbose",          no_argument,       0, OPT_FR_VERBOSE         },
        { "dab",              no_argument,       0, OPT_FR_DAB             },
        { "dab-label",        required_argument, 0, OPT_FR_DAB_LABEL       },

        { 0,                  0,                 0, 0                      },
    };
    params->afterburner = 1;
    /* Frankenstein sentinels: -1 means "leave FDK default". */
    params->fr_ms_mask = -1;
    params->fr_ms_bands = -1;
    params->fr_is = -1;
    params->fr_is_bands = -1;
    params->fr_is_minsfbs = -1;
    params->fr_is_corr = -1;
    params->fr_is_lrratio = -1;
    params->fr_core_cutoff = 0;
    params->fr_sbr_start = -1;
    params->fr_sbr_stop = -1;
    params->fr_sbr_freqscale = -1;
    params->fr_sbr_alterscale = -1;
    params->fr_sbr_noise_bands = -1;
    params->fr_sbr_amp_res = -1;
    params->fr_sbr_data_extra = -1;
    params->fr_ps = -1;
    params->fr_ps_iid_quant = -1;
    params->fr_tns_mask = -1;
    params->fr_tns_order = -1;
    params->fr_pns = -1;
    params->fr_pns_start = -1;
    params->fr_ath_scale = -1;
    params->fr_block_bias = -1;
    params->fr_vbr_reservoir = -1;
    params->fr_max_bits_frame = -1;
    params->fr_min_bits_frame = -1;
    params->fr_bitres_mode = -1;
    params->fr_ms_bias = -1;
    params->fr_uncap_bw = -1;
    params->fr_is_aggression = -1;
    params->fr_force_pns = -1;
    params->fr_unlock_bitrate = -1;
    params->fr_speech = -1;
    params->fr_spread_mask = -1;
    params->fr_ms_band_lo = -1;
    params->fr_ms_band_hi = -1;
    params->fr_ms_precision = -1;
    params->fr_sbr_num_env = -1;
    params->fr_sbr_freqres_fixfix = -1;
    params->fr_sbr_stereo_mode = -1;
    params->fr_sbr_invf = -1;
    params->fr_sbr_noise_floor_offset = -128;
    params->fr_ps_icc = -1;
    params->fr_ps_icc_mode = -1;
    params->fr_ps_bands = -1;
    params->fr_ps_env = -1;
    params->fr_ps_env_reduce = -1;
    params->fr_ps_noenv_skip = -1;
    params->fr_sbr_tran_peak = -1;
    params->fr_sbr_tran_quiet = -1;
    params->fr_sbr_tran_dom = -1;
    params->fr_sbr_tran_thr = -1;
    params->fr_sbr_tran_split = -1;
    params->fr_sbr_mh_tone = -1;
    params->fr_sbr_mh_diff = -1;
    params->fr_sbr_mh_decay_orig = -1;
    params->fr_sbr_mh_decay_diff = -1;
    params->fr_sbr_mh_sfm_sbr = -1;
    params->fr_sbr_mh_sfm_orig = -1;
    params->fr_sbr_mh_maxcomp = -1;
    params->fr_sbr_mh_deltatime = -1;
    params->fr_sbr_noise_max = -1;
    params->fr_sbr_invf_min = -1;
    params->fr_sbr_invf_max = -1;
    params->fr_sbr_invf_bias = AACENC_FRANKEN_OFF;
    params->fr_sbr_invf_table = -1;
    params->fr_sbr_invf_hyst = -1;
    params->fr_sbr_invf_sbr_thr = AACENC_FRANKEN_OFF;
    params->fr_sbr_invf_orig_thr = AACENC_FRANKEN_OFF;
    params->fr_sbr_invf_smooth = -1;
    params->fr_sbr_nf_weight = -1;
    params->fr_sbr_nf_weight_from = -1;
    params->fr_sbr_nf_smooth = -1;
    params->fr_sbr_softness = -1;
    params->fr_preecho_rise = AACENC_FRANKEN_OFF;
    params->fr_preecho_depth = AACENC_FRANKEN_OFF;
    params->fr_attack_decay = AACENC_FRANKEN_OFF;
    params->fr_attack_floor = AACENC_FRANKEN_OFF;
    params->fr_tns_thresh = AACENC_FRANKEN_OFF;
    params->fr_tns_start = -1;
    params->fr_window_shape = -1;
    params->fr_short_groups = -1;
    params->fr_avoid_holes = -1;
    params->fr_hole_start = -1;
    params->fr_hole_start_short = -1;
    params->fr_hole_spread = AACENC_FRANKEN_OFF;
    params->fr_hole_ladder = -1;
    params->fr_band_knee = AACENC_FRANKEN_OFF;
    params->fr_quant_round = -1;
    params->fr_pns_above = -1;
    params->fr_thr_hold = -1;
    params->fr_scf_range = -1;
    params->fr_coverage = AACENC_FRANKEN_OFF;
    params->fr_enfac_patch = -1;
    params->fr_bit_stats = 0;
    params->fr_side_knee_zone = -1;
    params->fr_side_cutoff = -1;
    params->fr_pns_shared = -1;
    params->fr_pns_hold = -1;
    params->fr_pns_budget = -1;
    params->fr_is_admit = -1;
    params->fr_side_knee_spread = -1;
    params->fr_ps_ipd = -1;
    params->fr_ps_opd = -1;
    params->fr_is_band_lo = -1;
    params->fr_is_band_hi = -1;
    params->fr_is_force_lo = -1;
    params->fr_is_force_hi = -1;
    params->fr_minsnr_scale = -1;
    params->fr_minsnr_clamp_hi = -1;
    params->fr_minsnr_clamp_lo = -1;
    params->fr_reduce_clamp = -1;
    params->fr_mid_bias = -1;
    params->fr_side_bias = AACENC_FRANKEN_OFF;
    params->fr_side_knee = AACENC_FRANKEN_OFF;
    params->fr_mask_slope = AACENC_FRANKEN_OFF;
    params->fr_sbr_header_period = -1;
    params->fr_sbr_header_time = -1;
    params->fr_pns_gain = -1;
    params->fr_pns_tonality = -1;
    params->fr_pns_refpower = -1;
    params->fr_pns_gapfill = -1;
    params->fr_pns_min_width = -1;
    params->fr_peak_bitrate = -1;
    params->fr_verbose = 0;
    params->fr_dab = 0;
    params->fr_dab_label = 0;

    aacenc_getmainargs(&argc, &argv);
    while ((ch = getopt_long(argc, argv, "hp:b:m:w:a:L:s:f:CP:G:Io:SR",
                             long_options, 0)) != EOF) {
        /* Frankenstein: the documented "-1 = off / stock" of these switches.
         * -1 is the internal "not given" sentinel and is outside their parse
         * ranges, so it is handled here: -1 means the same as leaving the
         * switch out (the stock value stays; an earlier value is reset). */
        if (optarg && strcmp(optarg, "-1") == 0) {
            int frOff = 0;
            switch (ch) {
        case OPT_FR_BITRES_MODE:
        case OPT_FR_BLOCK_BIAS:
        case OPT_FR_IS_AGGRESSION:
        case OPT_FR_IS_FORCE_HI:
        case OPT_FR_IS_FORCE_LO:
        case OPT_FR_IS_BAND_HI:
        case OPT_FR_IS_BAND_LO:
        case OPT_FR_MAX_BITS_FRAME:
        case OPT_FR_MID_BIAS:
        case OPT_FR_MIN_BITS_FRAME:
        case OPT_FR_MINSNR_CLAMP_HI:
        case OPT_FR_MINSNR_CLAMP_LO:
        case OPT_FR_MINSNR_SCALE:
        case OPT_FR_MS_BIAS:
        case OPT_FR_MS_PRECISION:
        case OPT_FR_MS_BAND_HI:
        case OPT_FR_MS_BAND_LO:
        case OPT_FR_PEAK_BITRATE:
        case OPT_FR_PNS_GAIN:
        case OPT_FR_PNS_GAPFILL:
        case OPT_FR_PNS_MIN_WIDTH:
        case OPT_FR_PNS_REFPOWER:
        case OPT_FR_PNS_TONALITY:
        case OPT_FR_PS_BANDS:
        case OPT_FR_PS_ENV:
        case OPT_FR_PS_ENV_REDUCE:
        case OPT_FR_PS_ICC:
        case OPT_FR_PS_ICC_MODE:
        case OPT_FR_PS_IPD:
        case OPT_FR_PS_NOENV_SKIP:
        case OPT_FR_SBR_FREQRES_FIXFIX:
        case OPT_FR_SBR_HEADER_PERIOD:
        case OPT_FR_SBR_HEADER_TIME:
        case OPT_FR_SBR_INVF:
        case OPT_FR_SBR_INVF_HYST:
        case OPT_FR_SBR_INVF_MAX:
        case OPT_FR_SBR_INVF_MIN:
        case OPT_FR_SBR_INVF_SMOOTH:
        case OPT_FR_SBR_INVF_TABLE:
        case OPT_FR_SBR_MH_DECAY_DIFF:
        case OPT_FR_SBR_MH_DECAY_ORIG:
        case OPT_FR_SBR_MH_DELTATIME:
        case OPT_FR_SBR_MH_DIFF:
        case OPT_FR_SBR_MH_MAXCOMP:
        case OPT_FR_SBR_MH_SFM_ORIG:
        case OPT_FR_SBR_MH_SFM_SBR:
        case OPT_FR_SBR_MH_TONE:
        case OPT_FR_SBR_NF_SMOOTH:
        case OPT_FR_SBR_NF_WEIGHT:
        case OPT_FR_SBR_NF_WFROM:
        case OPT_FR_SBR_NOISE_MAX:
        case OPT_FR_SBR_NUM_ENV:
        case OPT_FR_SBR_SOFTNESS:
        case OPT_FR_SBR_STEREO_MODE:
        case OPT_FR_SBR_TRAN_DOM:
        case OPT_FR_SBR_TRAN_PEAK:
        case OPT_FR_SBR_TRAN_QUIET:
        case OPT_FR_SBR_TRAN_SPLIT:
        case OPT_FR_SBR_TRAN_THR:
        case OPT_FR_SPREAD_MASK:
        case OPT_FR_VBR_RESERVOIR:
                frOff = 1;
                break;
            default:
                break;
            }
            if (frOff) {
                aacenc_param_ex_t *p = params;
                switch (ch) {
        case OPT_FR_BITRES_MODE: p->fr_bitres_mode = -1; break;
        case OPT_FR_BLOCK_BIAS: p->fr_block_bias = -1; break;
        case OPT_FR_IS_AGGRESSION: p->fr_is_aggression = -1; break;
        case OPT_FR_IS_FORCE_HI: p->fr_is_force_hi = -1; break;
        case OPT_FR_IS_FORCE_LO: p->fr_is_force_lo = -1; break;
        case OPT_FR_IS_BAND_HI: p->fr_is_band_hi = -1; break;
        case OPT_FR_IS_BAND_LO: p->fr_is_band_lo = -1; break;
        case OPT_FR_MAX_BITS_FRAME: p->fr_max_bits_frame = -1; break;
        case OPT_FR_MID_BIAS: p->fr_mid_bias = -1; break;
        case OPT_FR_MIN_BITS_FRAME: p->fr_min_bits_frame = -1; break;
        case OPT_FR_MINSNR_CLAMP_HI: p->fr_minsnr_clamp_hi = -1; break;
        case OPT_FR_MINSNR_CLAMP_LO: p->fr_minsnr_clamp_lo = -1; break;
        case OPT_FR_MINSNR_SCALE: p->fr_minsnr_scale = -1; break;
        case OPT_FR_MS_BIAS: p->fr_ms_bias = -1; break;
        case OPT_FR_MS_PRECISION: p->fr_ms_precision = -1; break;
        case OPT_FR_MS_BAND_HI: p->fr_ms_band_hi = -1; break;
        case OPT_FR_MS_BAND_LO: p->fr_ms_band_lo = -1; break;
        case OPT_FR_PEAK_BITRATE: p->fr_peak_bitrate = -1; break;
        case OPT_FR_PNS_GAIN: p->fr_pns_gain = -1; break;
        case OPT_FR_PNS_GAPFILL: p->fr_pns_gapfill = -1; break;
        case OPT_FR_PNS_MIN_WIDTH: p->fr_pns_min_width = -1; break;
        case OPT_FR_PNS_REFPOWER: p->fr_pns_refpower = -1; break;
        case OPT_FR_PNS_TONALITY: p->fr_pns_tonality = -1; break;
        case OPT_FR_PS_BANDS: p->fr_ps_bands = -1; break;
        case OPT_FR_PS_ENV: p->fr_ps_env = -1; break;
        case OPT_FR_PS_ENV_REDUCE: p->fr_ps_env_reduce = -1; break;
        case OPT_FR_PS_ICC: p->fr_ps_icc = -1; break;
        case OPT_FR_PS_ICC_MODE: p->fr_ps_icc_mode = -1; break;
        case OPT_FR_PS_IPD: p->fr_ps_ipd = -1; break;
        case OPT_FR_PS_NOENV_SKIP: p->fr_ps_noenv_skip = -1; break;
        case OPT_FR_SBR_FREQRES_FIXFIX: p->fr_sbr_freqres_fixfix = -1; break;
        case OPT_FR_SBR_HEADER_PERIOD: p->fr_sbr_header_period = -1; break;
        case OPT_FR_SBR_HEADER_TIME: p->fr_sbr_header_time = -1; break;
        case OPT_FR_SBR_INVF: p->fr_sbr_invf = -1; break;
        case OPT_FR_SBR_INVF_HYST: p->fr_sbr_invf_hyst = -1; break;
        case OPT_FR_SBR_INVF_MAX: p->fr_sbr_invf_max = -1; break;
        case OPT_FR_SBR_INVF_MIN: p->fr_sbr_invf_min = -1; break;
        case OPT_FR_SBR_INVF_SMOOTH: p->fr_sbr_invf_smooth = -1; break;
        case OPT_FR_SBR_INVF_TABLE: p->fr_sbr_invf_table = -1; break;
        case OPT_FR_SBR_MH_DECAY_DIFF: p->fr_sbr_mh_decay_diff = -1; break;
        case OPT_FR_SBR_MH_DECAY_ORIG: p->fr_sbr_mh_decay_orig = -1; break;
        case OPT_FR_SBR_MH_DELTATIME: p->fr_sbr_mh_deltatime = -1; break;
        case OPT_FR_SBR_MH_DIFF: p->fr_sbr_mh_diff = -1; break;
        case OPT_FR_SBR_MH_MAXCOMP: p->fr_sbr_mh_maxcomp = -1; break;
        case OPT_FR_SBR_MH_SFM_ORIG: p->fr_sbr_mh_sfm_orig = -1; break;
        case OPT_FR_SBR_MH_SFM_SBR: p->fr_sbr_mh_sfm_sbr = -1; break;
        case OPT_FR_SBR_MH_TONE: p->fr_sbr_mh_tone = -1; break;
        case OPT_FR_SBR_NF_SMOOTH: p->fr_sbr_nf_smooth = -1; break;
        case OPT_FR_SBR_NF_WEIGHT: p->fr_sbr_nf_weight = -1; break;
        case OPT_FR_SBR_NF_WFROM: p->fr_sbr_nf_weight_from = -1; break;
        case OPT_FR_SBR_NOISE_MAX: p->fr_sbr_noise_max = -1; break;
        case OPT_FR_SBR_NUM_ENV: p->fr_sbr_num_env = -1; break;
        case OPT_FR_SBR_SOFTNESS: p->fr_sbr_softness = -1; break;
        case OPT_FR_SBR_STEREO_MODE: p->fr_sbr_stereo_mode = -1; break;
        case OPT_FR_SBR_TRAN_DOM: p->fr_sbr_tran_dom = -1; break;
        case OPT_FR_SBR_TRAN_PEAK: p->fr_sbr_tran_peak = -1; break;
        case OPT_FR_SBR_TRAN_QUIET: p->fr_sbr_tran_quiet = -1; break;
        case OPT_FR_SBR_TRAN_SPLIT: p->fr_sbr_tran_split = -1; break;
        case OPT_FR_SBR_TRAN_THR: p->fr_sbr_tran_thr = -1; break;
        case OPT_FR_SPREAD_MASK: p->fr_spread_mask = -1; break;
        case OPT_FR_VBR_RESERVOIR: p->fr_vbr_reservoir = -1; break;
                default: break;
                }
                continue;
            }
        }
        switch (ch) {
        case 'h':
            return usage(), -1;
        case 'p':
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for profile\n");
                return -1;
            }
            params->profile = n;
            break;
        case 'b':
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for bitrate\n");
                return -1;
            }
            params->bitrate = n;
            break;
        case 'm':
            if (sscanf(optarg, "%u", &n) != 1 || n > 5) {
                fprintf(stderr, "invalid arg for bitrate-mode\n");
                return -1;
            }
            params->bitrate_mode = n;
            break;
        case 'w':
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for bandwidth\n");
                return -1;
            }
            params->bandwidth = n;
            break;
        case 'a':
            if (sscanf(optarg, "%u", &n) != 1 || n > 1) {
                fprintf(stderr, "invalid arg for afterburner\n");
                return -1;
            }
            params->afterburner = n;
            break;
        case 'L':
            if (sscanf(optarg, "%d", &n) != 1 || n < -1 || n > 1) {
                fprintf(stderr, "invalid arg for lowdelay-sbr\n");
                return -1;
            }
            params->lowdelay_sbr = n;
            break;
        case 's':
            if (sscanf(optarg, "%u", &n) != 1 || n > 2) {
                fprintf(stderr, "invalid arg for sbr-ratio\n");
                return -1;
            }
            params->sbr_ratio = n;
            break;
        case 'f':
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for transport-format\n");
                return -1;
            }
            params->transport_format = n;
            break;
        case 'C':
            params->adts_crc_check = 1;
            break;
        case 'P':
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for header-period\n");
                return -1;
            }
            params->header_period = n;
            break;
        case 'o':
            params->output_filename = optarg;
            break;
        case 'G':
            if (sscanf(optarg, "%u", &n) != 1 || n > 2) {
                fprintf(stderr, "invalid arg for gapless-mode\n");
                return -1;
            }
            params->gapless_mode = n;
            break;
        case OPT_INCLUDE_SBR_DELAY:
            params->include_sbr_delay = 1;
            break;
        case 'I':
            params->ignore_length = 1;
            break;
        case 'S':
            params->silent = 1;
            break;
        case OPT_MOOV_BEFORE_MDAT:
            params->moov_before_mdat = 1;
            break;
        case OPT_NO_TOOL_TAG:
            params->no_tool_tag = 1;
            break;
        case OPT_MINIMAL_MOOV:
            params->minimal_moov = 1;
            break;
        case 'R':
            params->is_raw = 1;
            break;
        case OPT_RAW_CHANNELS:
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for raw-channels\n");
                return -1;
            }
            params->raw_channels = n;
            break;
        case OPT_RAW_RATE:
            if (sscanf(optarg, "%u", &n) != 1) {
                fprintf(stderr, "invalid arg for raw-rate\n");
                return -1;
            }
            params->raw_rate = n;
            break;
        case OPT_RAW_FORMAT:
            params->raw_format = optarg;
            break;
        case M4AF_TAG_TITLE:
        case M4AF_TAG_ARTIST:
        case M4AF_TAG_ALBUM:
        case M4AF_TAG_GENRE:
        case M4AF_TAG_DATE:
        case M4AF_TAG_COMPOSER:
        case M4AF_TAG_GROUPING:
        case M4AF_TAG_COMMENT:
        case M4AF_TAG_ALBUM_ARTIST:
        case M4AF_TAG_TRACK:
        case M4AF_TAG_DISK:
        case M4AF_TAG_TEMPO:
            aacenc_add_tag_to_store(&params->tags, ch, 0, optarg,
                                    strlen(optarg), 0);
            break;
        case OPT_SHORT_TAG:
        case OPT_SHORT_TAG_FILE:
        case OPT_LONG_TAG:
            {
                char *val;
                size_t klen;
                unsigned fcc = M4AF_FOURCC('-','-','-','-');

                if ((val = strchr(optarg, ':')) == 0) {
                    fprintf(stderr, "invalid arg for tag\n");
                    return -1;
                }
                *val++ = '\0';
                if (ch == OPT_SHORT_TAG || ch == OPT_SHORT_TAG_FILE) {
                    /*
                     * take care of U+00A9(COPYRIGHT SIGN).
                     * 1) if length of fcc is 3, we prepend '\xa9'.
                     * 2) U+00A9 becomes "\xc2\xa9" in UTF-8. Therefore
                     *    we remove first '\xc2'.
                     */
                    if (optarg[0] == '\xc2')
                        ++optarg;
                    if ((klen = strlen(optarg))== 3)
                        fcc = 0xa9;
                    else if (klen != 4) {
                        fprintf(stderr, "invalid arg for tag\n");
                        return -1;
                    }
                    for (; *optarg; ++optarg)
                        fcc = ((fcc << 8) | (*optarg & 0xff));
                }
                aacenc_add_tag_to_store(&params->tags, fcc, optarg,
                                        val, strlen(val),
                                        ch == OPT_SHORT_TAG_FILE);
            }
            break;
        case OPT_TAG_FROM_JSON:
            params->json_filename = optarg;
            break;
        case '#':
            params->no_timestamp = 1;
            break;

        /* ---- Frankenstein debug switches ---- */
        case OPT_FR_MS_MASK:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for msmask\n"); return -1; }
            params->fr_ms_mask = n; break;
        case OPT_FR_MS_BANDS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for msbands\n"); return -1; }
            params->fr_ms_bands = n; break;
        case OPT_FR_IS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for is\n"); return -1; }
            params->fr_is = n; break;
        case OPT_FR_IS_BANDS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for isbands\n"); return -1; }
            params->fr_is_bands = n; break;
        case OPT_FR_IS_MINSFBS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for is-min-sfbs\n"); return -1; }
            params->fr_is_minsfbs = n; break;
        case OPT_FR_IS_CORR:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for is-corr-thresh\n"); return -1; }
            params->fr_is_corr = n; break;
        case OPT_FR_IS_LRRATIO:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for is-lr-ratio\n"); return -1; }
            params->fr_is_lrratio = n; break;
        case OPT_FR_CORE_CUTOFF:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 48000) { fprintf(stderr, "invalid arg for core-cutoff (0..48000 Hz, 0 = stock)\n"); return -1; }
            params->fr_core_cutoff = n; break;
        case OPT_FR_SBR_START:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-start\n"); return -1; }
            params->fr_sbr_start = n; break;
        case OPT_FR_SBR_STOP:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-stop\n"); return -1; }
            params->fr_sbr_stop = n; break;
        case OPT_FR_SBR_FREQSCALE:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-freqscale\n"); return -1; }
            params->fr_sbr_freqscale = n; break;
        case OPT_FR_SBR_ALTERSCALE:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-alterscale\n"); return -1; }
            params->fr_sbr_alterscale = n; break;
        case OPT_FR_SBR_NOISE_BANDS:
            /* bs_noise_bands is a 2-bit header field: 0..3 only. Larger values
             * were written truncated (4 -> 0, 5 -> 1) while the encoder used the
             * full value, so encoder and decoder disagreed on the band count. */
            if (fr_scan_int(optarg, &n) != 1 || n < -1 || n > 3) { fprintf(stderr, "invalid arg for sbr-noise-bands (-1, 0..3)\n"); return -1; }
            params->fr_sbr_noise_bands = n; break;
        case OPT_FR_SBR_AMP_RES:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-amp-res\n"); return -1; }
            params->fr_sbr_amp_res = n; break;
        case OPT_FR_SBR_DATA_EXTRA:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for sbr-data-extra\n"); return -1; }
            params->fr_sbr_data_extra = n; break;
        case OPT_FR_PS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for ps\n"); return -1; }
            params->fr_ps = n; break;
        case OPT_FR_PS_IID_QUANT:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for ps-iid-quant\n"); return -1; }
            params->fr_ps_iid_quant = n; break;
        case OPT_FR_TNS_MASK:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for tns-mask\n"); return -1; }
            params->fr_tns_mask = n; break;
        case OPT_FR_TNS_ORDER:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for tns-order\n"); return -1; }
            params->fr_tns_order = n; break;
        case OPT_FR_PNS:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for pns\n"); return -1; }
            params->fr_pns = n; break;
        case OPT_FR_PNS_START:
            if (fr_scan_int(optarg, &n) != 1) { fprintf(stderr, "invalid arg for pns-start\n"); return -1; }
            params->fr_pns_start = n; break;
        case OPT_FR_ATH_SCALE:
            if (fr_scan_int(optarg, &n) != 1 || n < 1 || n > 4096) { fprintf(stderr, "invalid arg for ath-scale (1..4096, 256 = x1.0)\n"); return -1; }
            params->fr_ath_scale = n; break;
        case OPT_FR_BLOCK_BIAS:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 255) { fprintf(stderr, "invalid arg for block-bias (0..255)\n"); return -1; }
            params->fr_block_bias = n; break;
        case OPT_FR_VBR_RESERVOIR:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for vbr-reservoir\n"); return -1; }
            params->fr_vbr_reservoir = n; break;
        case OPT_FR_MAX_BITS_FRAME:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for max-bits-frame\n"); return -1; }
            params->fr_max_bits_frame = n; break;
        case OPT_FR_MIN_BITS_FRAME:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for min-bits-frame\n"); return -1; }
            params->fr_min_bits_frame = n; break;
        case OPT_FR_BITRES_MODE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 2) { fprintf(stderr, "invalid arg for bitres-mode (0..2)\n"); return -1; }
            params->fr_bitres_mode = n; break;
        case OPT_FR_MS_BIAS:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 255) { fprintf(stderr, "invalid arg for ms-bias (0..255)\n"); return -1; }
            params->fr_ms_bias = n; break;
        case OPT_FR_UNCAP_BW:
            params->fr_uncap_bw = 1; break;
        case OPT_FR_IS_AGGRESSION:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 100) { fprintf(stderr, "invalid arg for is-aggression (0..100)\n"); return -1; }
            params->fr_is_aggression = n; break;
        case OPT_FR_FORCE_PNS:
            params->fr_force_pns = 1; break;
        case OPT_FR_UNLOCK_BITRATE:
            params->fr_unlock_bitrate = 1; break;
        case OPT_FR_SPEECH:
            params->fr_speech = 1; break;
        case OPT_FR_SPREAD_MASK:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for spread-mask (>=0, 256=neutral)\n"); return -1; }
            params->fr_spread_mask = n; break;
        case OPT_FR_MS_BAND_LO:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for msbands-lo\n"); return -1; }
            params->fr_ms_band_lo = n; break;
        case OPT_FR_MS_BAND_HI:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for msbands-hi\n"); return -1; }
            params->fr_ms_band_hi = n; break;
        case OPT_FR_MS_PRECISION:
            if (fr_scan_int(optarg, &n) != 1 || n < 256) { fprintf(stderr, "invalid arg for ms-precision (>=256)\n"); return -1; }
            params->fr_ms_precision = n; break;
        case OPT_FR_SBR_NUM_ENV:
            if (fr_scan_int(optarg, &n) != 1 || (n!=1 && n!=2 && n!=4)) { fprintf(stderr, "invalid arg for sbr-num-env (1,2,4; 8 exceeds standard frame grid)\n"); return -1; }
            params->fr_sbr_num_env = n; break;
        case OPT_FR_SBR_FREQRES_FIXFIX:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for sbr-freqres-fixfix (0,1)\n"); return -1; }
            params->fr_sbr_freqres_fixfix = n; break;
        case OPT_FR_SBR_STEREO_MODE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-stereo-mode (0..3)\n"); return -1; }
            params->fr_sbr_stereo_mode = n; break;
        case OPT_FR_SBR_INVF:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-invf (0..3)\n"); return -1; }
            params->fr_sbr_invf = n; break;
        case OPT_FR_SBR_NF_OFFSET:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 11) { fprintf(stderr, "invalid arg for sbr-noise-floor-offset (0..11; FDK aborts outside this range)\n"); return -1; }
            params->fr_sbr_noise_floor_offset = n; break;
        case OPT_FR_PS_ICC:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-icc (0,1)\n"); return -1; }
            params->fr_ps_icc = n; break;
        case OPT_FR_PS_ICC_MODE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-icc-mode (0,1)\n"); return -1; }
            params->fr_ps_icc_mode = n; break;
        case OPT_FR_PS_BANDS:
            if (fr_scan_int(optarg, &n) != 1 || (n != 10 && n != 20)) { fprintf(stderr, "invalid arg for ps-bands (10 or 20)\n"); return -1; }
            params->fr_ps_bands = n; break;
        case OPT_FR_PS_ENV:
            if (fr_scan_int(optarg, &n) != 1 || (n != 1 && n != 2 && n != 4)) { fprintf(stderr, "invalid arg for ps-env (1, 2 or 4)\n"); return -1; }
            params->fr_ps_env = n; break;
        case OPT_FR_PS_ENV_REDUCE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-env-reduce (0,1)\n"); return -1; }
            params->fr_ps_env_reduce = n; break;
        case OPT_FR_PS_NOENV_SKIP:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-noenv-skip (0,1)\n"); return -1; }
            params->fr_ps_noenv_skip = n; break;
        case OPT_FR_SBR_TRAN_PEAK:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=200)) { fprintf(stderr, "invalid arg for sbr-tran-peak: peak (1..200, 90=stock 0.90)\n"); return -1; }
            params->fr_sbr_tran_peak = n; break;
        case OPT_FR_SBR_TRAN_QUIET:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-tran-quiet: quiet (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_tran_quiet = n; break;
        case OPT_FR_SBR_TRAN_DOM:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=100 && n<=500)) { fprintf(stderr, "invalid arg for sbr-tran-dom: dom (100..500, 140=stock 1.4)\n"); return -1; }
            params->fr_sbr_tran_dom = n; break;
        case OPT_FR_SBR_TRAN_THR:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-tran-thr: thr (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_tran_thr = n; break;
        case OPT_FR_SBR_TRAN_SPLIT:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-tran-split: split (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_tran_split = n; break;
        case OPT_FR_SBR_MH_TONE:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-tone: tone (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_tone = n; break;
        case OPT_FR_SBR_MH_DIFF:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-diff: diff (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_diff = n; break;
        case OPT_FR_SBR_MH_DECAY_ORIG:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-decay-orig: decay-orig (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_decay_orig = n; break;
        case OPT_FR_SBR_MH_DECAY_DIFF:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-decay-diff: decay-diff (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_decay_diff = n; break;
        case OPT_FR_SBR_MH_SFM_SBR:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-sfm-sbr: sfm-sbr (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_sfm_sbr = n; break;
        case OPT_FR_SBR_MH_SFM_ORIG:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=1 && n<=10000)) { fprintf(stderr, "invalid arg for sbr-mh-sfm-orig: sfm-orig (1..10000, 100=1.00)\n"); return -1; }
            params->fr_sbr_mh_sfm_orig = n; break;
        case OPT_FR_SBR_MH_MAXCOMP:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=0 && n<=200)) { fprintf(stderr, "invalid arg for sbr-mh-maxcomp: maxcomp (0..200, 50=stock)\n"); return -1; }
            params->fr_sbr_mh_maxcomp = n; break;
        case OPT_FR_SBR_MH_DELTATIME:
            if (fr_scan_int(optarg, &n) != 1 || !(n>=0 && n<=64)) { fprintf(stderr, "invalid arg for sbr-mh-deltatime: deltatime (0..64)\n"); return -1; }
            params->fr_sbr_mh_deltatime = n; break;
        case OPT_FR_SBR_NOISE_MAX:
            if (fr_scan_int(optarg, &n) != 1 || !(n==6 || n==3 || n==-3)) { fprintf(stderr, "invalid arg for sbr-noise-max: noise-max (6, 3 or -3)\n"); return -1; }
            params->fr_sbr_noise_max = n; break;
        case OPT_FR_SBR_INVF_MIN:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-invf-min (0..3)\n"); return -1; }
            params->fr_sbr_invf_min = n; break;
        case OPT_FR_SBR_INVF_MAX:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-invf-max (0..3)\n"); return -1; }
            params->fr_sbr_invf_max = n; break;
        case OPT_FR_SBR_INVF_BIAS:
            if (fr_scan_int(optarg, &n) != 1 || n < -3 || n > 3) { fprintf(stderr, "invalid arg for sbr-invf-bias (-3..3)\n"); return -1; }
            params->fr_sbr_invf_bias = (n == 0) ? AACENC_FRANKEN_OFF : n; break;
        case OPT_FR_SBR_INVF_TABLE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for sbr-invf-table (0 music, 1 speech)\n"); return -1; }
            params->fr_sbr_invf_table = n; break;
        case OPT_FR_SBR_INVF_HYST:
            if (fr_parse_dec(optarg, 0.0, 2.0, 100, &params->fr_sbr_invf_hyst)) { fprintf(stderr, "invalid arg for sbr-invf-hyst (0.0..2.0, 1.0 = stock; above 2 the decision borders overlap)\n"); return -1; }
            break;
        case OPT_FR_SBR_INVF_SBR_THR:
            if (fr_parse_dec(optarg, -20.0, 20.0, 100, &params->fr_sbr_invf_sbr_thr)) { fprintf(stderr, "invalid arg for sbr-invf-sbr-thr (-20.0..20.0)\n"); return -1; }
            break;
        case OPT_FR_SBR_INVF_ORIG_THR:
            if (fr_parse_dec(optarg, -20.0, 20.0, 100, &params->fr_sbr_invf_orig_thr)) { fprintf(stderr, "invalid arg for sbr-invf-orig-thr (-20.0..20.0)\n"); return -1; }
            break;
        case OPT_FR_SBR_INVF_SMOOTH:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 2) { fprintf(stderr, "invalid arg for sbr-invf-smooth (0..2, 2 = stock)\n"); return -1; }
            params->fr_sbr_invf_smooth = n; break;
        case OPT_FR_SBR_NF_WEIGHT:
            if (fr_parse_dec(optarg, 0.0, 1.0, 1000, &params->fr_sbr_nf_weight)) { fprintf(stderr, "invalid arg for sbr-nf-weight (0.0..1.0; music 0.25, speech 1.0)\n"); return -1; }
            break;
        case OPT_FR_SBR_NF_WFROM:
            if (fr_scan_int(optarg, &n) != 1 || n < 1 || n > 3) { fprintf(stderr, "invalid arg for sbr-nf-weight-from (1..3, 3 = stock)\n"); return -1; }
            params->fr_sbr_nf_weight_from = n; break;
        case OPT_FR_SBR_NF_SMOOTH:
            if (fr_scan_int(optarg, &n) != 1 || n != 0) { fprintf(stderr, "invalid arg for sbr-nf-smooth (only 0 = off)\n"); return -1; }
            params->fr_sbr_nf_smooth = n; break;
        case OPT_FR_SBR_SOFTNESS:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 7) { fprintf(stderr, "invalid arg for sbr-softness (0 hard .. 7 soft)\n"); return -1; }
            params->fr_sbr_softness = n; break;
        case OPT_FR_PREECHO_RISE:
            if (fr_parse_dec(optarg, 0.0, 24.0, 1000, &params->fr_preecho_rise)) { fprintf(stderr, "invalid arg for preecho-rise (0.0..24.0, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_PREECHO_DEPTH:
            if (fr_parse_dec(optarg, 0.0, 60.0, 1000, &params->fr_preecho_depth)) { fprintf(stderr, "invalid arg for preecho-depth (0.0..60.0, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_ATTACK_DECAY:
            if (fr_parse_dec(optarg, 0.0, 0.99, 1000, &params->fr_attack_decay)) { fprintf(stderr, "invalid arg for attack-decay (0.0..0.99, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_ATTACK_FLOOR:
            if (fr_parse_dec(optarg, -60.0, 30.0, 1000, &params->fr_attack_floor)) { fprintf(stderr, "invalid arg for attack-floor (-60.0..30.0, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_TNS_THRESH:
            if (fr_parse_dec(optarg, 0.5, 4.0, 1000, &params->fr_tns_thresh)) { fprintf(stderr, "invalid arg for tns-thresh (0.5..4.0, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_TNS_START:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 24000) { fprintf(stderr, "invalid arg for tns-start (0..24000)\n"); return -1; }
            params->fr_tns_start = n; break;
        case OPT_FR_WINDOW_SHAPE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for window-shape (0..1)\n"); return -1; }
            params->fr_window_shape = n; break;
        case OPT_FR_SHORT_GROUPS:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for short-groups (0..3)\n"); return -1; }
            params->fr_short_groups = n; break;
        case OPT_FR_AVOID_HOLES:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for avoid-holes (0..1)\n"); return -1; }
            params->fr_avoid_holes = n; break;
        case OPT_FR_HOLE_START:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 51) { fprintf(stderr, "invalid arg for hole-start (0..51)\n"); return -1; }
            params->fr_hole_start = n; break;
        case OPT_FR_HOLE_START_SHORT:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 15) { fprintf(stderr, "invalid arg for hole-start-short (0..15)\n"); return -1; }
            params->fr_hole_start_short = n; break;
        case OPT_FR_HOLE_SPREAD:
            if (fr_parse_dec(optarg, 0.0, 24.0, 1000, &params->fr_hole_spread)) { fprintf(stderr, "invalid arg for hole-spread (0.0..24.0, decimal, up to 3 places)\n"); return -1; }
            break;
        case OPT_FR_HOLE_LADDER:
            if (fr_scan_int(optarg, &n) != 1 || n < 1 || n > 8) { fprintf(stderr, "invalid arg for hole-ladder (1..8)\n"); return -1; }
            params->fr_hole_ladder = n; break;
        case OPT_FR_BAND_KNEE:
            if (fr_parse_dec(optarg, -24.0, 24.0, 1000, &params->fr_band_knee)) { fprintf(stderr, "invalid arg for band-knee (-24.0..24.0 dB; + = keep near-miss bands coarse, - = drop marginal bands, 0 = off)\n"); return -1; }
            if (params->fr_band_knee == 0) params->fr_band_knee = AACENC_FRANKEN_OFF;
            break;
        case OPT_FR_QUANT_ROUND:
            if (fr_parse_dec(optarg, 0.0, 0.5, 1000, &params->fr_quant_round)) { fprintf(stderr, "invalid arg for quant-round (0.0..0.5, stock 0.405)\n"); return -1; }
            break;
        case OPT_FR_PNS_ABOVE:
            if (fr_scan_int(optarg, &n) != 1 || n < 1000 || n > 48000) { fprintf(stderr, "invalid arg for pns-above (1000..48000 Hz)\n"); return -1; }
            params->fr_pns_above = n; break;
        case OPT_FR_THR_HOLD:
            if (fr_scan_int(optarg, &n) != 1 || n < 1 || n > 32) { fprintf(stderr, "invalid arg for thr-hold (1..32 frames, 1 = off)\n"); return -1; }
            params->fr_thr_hold = (n == 1) ? -1 : n; break;
        case OPT_FR_SCF_RANGE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 60) { fprintf(stderr, "invalid arg for scf-range (0..60 steps of 1.5 dB, stock 60)\n"); return -1; }
            params->fr_scf_range = (n == 60) ? -1 : n; break;
        case OPT_FR_COVERAGE:
            if (fr_scan_int(optarg, &n) != 1 || n < -5 || n > 5) { fprintf(stderr, "invalid arg for coverage (-5..5: - = fewer bands coded cleanly, + = everything coded coarsely, 0 = stock)\n"); return -1; }
            params->fr_coverage = (n == 0) ? AACENC_FRANKEN_OFF : n; break;
        case OPT_FR_PNS_SHARED:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for pns-shared (0|1)\n"); return -1; }
            params->fr_pns_shared = n; break;
        case OPT_FR_PNS_HOLD:
            if (fr_parse_dec(optarg, 0.1, 12.0, 1000, &params->fr_pns_hold)) { fprintf(stderr, "invalid arg for pns-hold (0.1..12.0 dB)\n"); return -1; }
            break;
        case OPT_FR_PNS_BUDGET:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for pns-budget (0|1)\n"); return -1; }
            params->fr_pns_budget = n; break;
        case OPT_FR_SIDE_CUTOFF:
            if (fr_scan_int(optarg, &n) != 1 || n < 1000 || n > 24000) { fprintf(stderr, "invalid arg for side-cutoff (1000..24000 Hz)\n"); return -1; }
            params->fr_side_cutoff = n; break;
        case OPT_FR_IS_ADMIT:
            if (fr_parse_dec(optarg, 0.0, 20.0, 100, &params->fr_is_admit)) { fprintf(stderr, "invalid arg for is-admit (0.0..20.0, stock 5)\n"); return -1; }
            break;
        case OPT_FR_SIDE_KNEE_ZONE:
            if (fr_parse_dec(optarg, 1.0, 20.0, 1000, &params->fr_side_knee_zone)) { fprintf(stderr, "invalid arg for side-knee-zone (1.0..20.0, stock 3)\n"); return -1; }
            break;
        case OPT_FR_SIDE_KNEE_SPREAD:
            if (fr_parse_dec(optarg, 0.1, 24.0, 1000, &params->fr_side_knee_spread)) { fprintf(stderr, "invalid arg for side-knee-spread (0.1..24.0 dB, stock 1)\n"); return -1; }
            break;
        case OPT_FR_BIT_STATS:
            params->fr_bit_stats = 1; break;
        case OPT_FR_ENFAC_PATCH:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for enfac-patch (0 never, 1 every long frame)\n"); return -1; }
            params->fr_enfac_patch = n; break;
        case OPT_FR_PS_IPD:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-ipd (0,1)\n"); return -1; }
            params->fr_ps_ipd = n; break;
        case OPT_FR_PS_OPD:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-opd (0,1)\n"); return -1; }
            params->fr_ps_opd = n; break;
        case OPT_FR_IS_BAND_LO:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-lo (>=0)\n"); return -1; }
            params->fr_is_band_lo = n; break;
        case OPT_FR_IS_BAND_HI:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-hi (>=0)\n"); return -1; }
            params->fr_is_band_hi = n; break;
        case OPT_FR_IS_FORCE_LO:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-force-lo (>=0)\n"); return -1; }
            params->fr_is_force_lo = n; break;
        case OPT_FR_IS_FORCE_HI:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-force-hi (>=0)\n"); return -1; }
            params->fr_is_force_hi = n; break;
        case OPT_FR_MINSNR_SCALE:
            if (fr_scan_int(optarg, &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-scale (>=1, 256=neutral, <256=more detail)\n"); return -1; }
            params->fr_minsnr_scale = n; break;
        case OPT_FR_MINSNR_CLAMP_HI:
            if (fr_scan_int(optarg, &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-clamp-hi (>=1, 256=neutral)\n"); return -1; }
            params->fr_minsnr_clamp_hi = n; break;
        case OPT_FR_MINSNR_CLAMP_LO:
            if (fr_scan_int(optarg, &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-clamp-lo (>=1, 256=neutral)\n"); return -1; }
            params->fr_minsnr_clamp_lo = n; break;
        case OPT_FR_REDUCE_CLAMP:
            if (fr_scan_int(optarg, &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for reduce-clamp (0 off,1 on)\n"); return -1; }
            params->fr_reduce_clamp = n; break;
        case OPT_FR_MID_BIAS:
            if (fr_scan_int(optarg, &n) != 1 || n < 256) { fprintf(stderr, "invalid arg for mid-bias (>=256, 256=neutral)\n"); return -1; }
            params->fr_mid_bias = n; break;
        case OPT_FR_SIDE_BIAS:
            if (fr_parse_dec(optarg, -24.0, 50.0, 1000, &params->fr_side_bias)) { fprintf(stderr, "invalid arg for side-bias (-24.0..50.0 dB; + = more side, - = destroy side, 0=off)\n"); return -1; }
            break;
        case OPT_FR_SIDE_KNEE:
            if (fr_parse_dec(optarg, -24.0, 50.0, 1000, &params->fr_side_knee)) { fprintf(stderr, "invalid arg for side-knee (-24.0..50.0 dB; + = soft knee, - = hard cutoff, 0=off)\n"); return -1; }
            break;
        case OPT_FR_MASK_SLOPE:
            if (fr_parse_dec(optarg, -24.0, 50.0, 1000, &params->fr_mask_slope)) { fprintf(stderr, "invalid arg for mask-slope (-24.0..50.0 dB; + = more detail in quiet bands, - = starve harder, 0=off)\n"); return -1; }
            break;
        case OPT_FR_SBR_HEADER_PERIOD:
            if (fr_scan_int(optarg, &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for sbr-header-period (>=1; 1=near-instant SBR sync)\n"); return -1; }
            params->fr_sbr_header_period = n; break;
        case OPT_FR_SBR_HEADER_TIME:
            if (fr_parse_dec(optarg, 0.01, 600.0, 1000, &params->fr_sbr_header_time)) { fprintf(stderr, "invalid arg for sbr-header-time (0.01..600 s, e.g. 3.5)\n"); return -1; }
            break;
        case OPT_FR_PNS_GAIN:
            if (fr_parse_dec(optarg, 0.0, 1e9, 100, &params->fr_pns_gain)) { fprintf(stderr, "invalid arg for pns-gain (>=0.0; 1.0=unchanged)\n"); return -1; }
            break;
        case OPT_FR_PNS_TONALITY:
            if (fr_parse_dec(optarg, 0.0, 1e9, 100, &params->fr_pns_tonality)) { fprintf(stderr, "invalid arg for pns-tonality (>=0.0; 1.0=unchanged)\n"); return -1; }
            break;
        case OPT_FR_PNS_REFPOWER:
            if (fr_parse_dec(optarg, 0.0, 1e9, 100, &params->fr_pns_refpower)) { fprintf(stderr, "invalid arg for pns-refpower (>=0.0; 1.0=unchanged)\n"); return -1; }
            break;
        case OPT_FR_PNS_GAPFILL:
            if (fr_parse_dec(optarg, 0.0, 1e9, 100, &params->fr_pns_gapfill)) { fprintf(stderr, "invalid arg for pns-gapfill (>=0.0; 1.0=unchanged)\n"); return -1; }
            break;
        case OPT_FR_PNS_MIN_WIDTH:
            if (fr_scan_int(optarg, &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for pns-min-width (>=1)\n"); return -1; }
            params->fr_pns_min_width = n; break;
        case OPT_FR_PEAK_BITRATE:
            if (fr_scan_int(optarg, &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for peak-bitrate\n"); return -1; }
            params->fr_peak_bitrate = n; break;
        case OPT_FR_VERBOSE:
            params->fr_verbose = 1; break;
        case OPT_FR_DAB:
            params->fr_dab = 1; break;
        case OPT_FR_DAB_LABEL:
            params->fr_dab_label = optarg; break;

        default:
            return usage(), -1;
        }
    }
    if (argc == optind)
        return usage(), -1;

    if (!params->bitrate && !params->bitrate_mode) {
        fprintf(stderr, "bitrate or bitrate-mode is mandatory\n");
        return -1;
    }
    /* DAB+ mode: DAB+ superframe transport + validate. AOT (LC/HE/HEv2) is
       auto-selected from bitrate and channel count after the input is opened
       (see main), unless the user forced one with -p. */
    if (params->fr_dab) {
        if (params->bitrate < 8 || params->bitrate > 192 ||
            params->bitrate % 8 != 0) {
            fprintf(stderr,
                    "DAB+: bitrate must be a multiple of 8 kbps in 8..192 "
                    "(got %u)\n", params->bitrate);
            return -1;
        }
        params->bitrate *= 1000;          /* -b is kbps for DAB+ */
        params->transport_format = TT_DABPLUS;
        params->bitrate_mode = 0;         /* CBR */
    }
    if (params->output_filename && !strcmp(params->output_filename, "-") &&
        !params->transport_format) {
        fprintf(stderr, "stdout streaming is not available on M4A output\n");
        return -1;
    }
    /* nu774 convenience: small -b values are treated as kbps (x1000). With
     * --unlock-bitrate the user may want genuine ultra-low bps (e.g. 6000 = 6k),
     * so skip the x1000 promotion in that mode and take -b verbatim. */
    if (params->bitrate && params->bitrate < 10000 && params->fr_unlock_bitrate != 1)
        params->bitrate *= 1000;

    /* Disambiguation guard: with --unlock-bitrate, -b is read literally as bps.
     * A value that looks like "kbps shorthand" (e.g. 128, 1152) is almost
     * certainly a mistake there - warn so the user does not silently get a few
     * hundred bps snapped up to the floor. */
    if (params->fr_unlock_bitrate == 1 && params->bitrate > 0 && params->bitrate < 4000)
        fprintf(stderr,
                "WARNING: --unlock-bitrate reads -b as bits/sec, so -b %d = %d bps "
                "(not kbps). For %d kbps write -b %d000. Continuing with %d bps.\n",
                params->bitrate, params->bitrate, params->bitrate, params->bitrate,
                params->bitrate);

    /* SBR floor note: stock FDK raises HE-AAC / HE-AAC v2 below the lowest SBR
     * tuning-table row (16 kbps for 24/48 kHz stereo, 8-12 kbps for mono or
     * lower rates, 8 kbps for HEv2). That is a table limit, not a format one:
     * --unlock-bitrate reuses the lowest row and keeps the requested bitrate.
     * The encoder prints the bitrate it really uses in --verbose. */
    if (params->bitrate > 0 && params->bitrate < 16000 &&
        params->fr_unlock_bitrate != 1 &&
        (params->profile == 5 || params->profile == 29)) {
        fprintf(stderr,
                "NOTE: below the lowest SBR tuning row FDK raises -b %d (e.g. to "
                "16 kbps for 48 kHz stereo HE-AAC). Add --unlock-bitrate to keep "
                "it (then -b is bits/sec).\n", params->bitrate);
    }

    if (params->is_raw) {
        if (!params->raw_channels)
            params->raw_channels = 2;
        if (!params->raw_rate)
            params->raw_rate = 44100;
        if (!params->raw_format)
            params->raw_format = "S16L";
    }
    params->input_filename = argv[optind];
    return 0;
};

static
int write_sample(FILE *ofp, m4af_ctx_t *m4af, aacenc_frame_t *frame)
{
    if (!m4af) {
        fwrite(frame->data, 1, frame->size, ofp);
        if (ferror(ofp)) {
            fprintf(stderr, "ERROR: fwrite(): %s\n", strerror(errno));
            return -1;
        }
    } else if (m4af_write_sample(m4af, 0, frame->data, frame->size, 0) < 0) {
        fprintf(stderr, "ERROR: failed to write m4a sample\n");
        return -1;
    }
    return 0;
}

/*
 * DAB+ superframe finalisation: the encoder hands us a superframe whose data
 * area is subchannel_index*110 bytes (already firecode+au_start+CRC framed).
 * ETSI TS 102 563 protects it with RS(120,110): view the 110*subch bytes as a
 * column-major matrix of subch rows x 110 cols, append 10 parity bytes per row,
 * giving subch*120 bytes. Interleaving is column-major (byte i of AU stream ->
 * row i%subch, col i/subch), matching the receiver's de-interleaver.
 */
static
int write_sample_dab(FILE *ofp, aacenc_frame_t *frame, int subch)
{
    unsigned char row[110], parity[10];
    int r, c;
    const int data_len = subch * 110;
    const int total_len = subch * 120;
    static unsigned char *sf = NULL;
    static int sf_cap = 0;

    if (frame->size != (uint32_t)data_len) {
        fprintf(stderr, "DAB+: unexpected superframe size %u (want %d)\n",
                frame->size, data_len);
        return -1;
    }
    if (sf_cap < total_len) {
        unsigned char *p = realloc(sf, total_len);
        if (!p) return -1;
        sf = p; sf_cap = total_len;
    }
    memcpy(sf, frame->data, data_len);
    for (r = 0; r < subch; r++) {
        for (c = 0; c < 110; c++)
            row[c] = frame->data[subch * c + r];
        dab_rs_encode(row, parity);
        for (c = 0; c < 10; c++)
            sf[subch * (110 + c) + r] = parity[c];
    }
    fwrite(sf, 1, total_len, ofp);
    if (ferror(ofp)) {
        fprintf(stderr, "ERROR: fwrite(): %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int do_smart_padding(int profile)
{
    return profile == 2 || profile == 5 || profile == 29;
}

static
int encode(aacenc_param_ex_t *params, pcm_reader_t *reader,
           HANDLE_AACENCODER encoder, uint32_t frame_length, 
           m4af_ctx_t *m4af)
{
    INT_PCM *ibuf = 0, *ip;
    aacenc_frame_t obuf[2] = {{ 0 }}, *obp;
    unsigned flip = 0;
    int nread = 1;
    int rc = -1;
    int remaining, consumed;
    int frames_written = 0, encoded = 0;
    aacenc_progress_t progress = { 0 };
    const pcm_sample_description_t *fmt = pcm_get_format(reader);
    const int is_padding = do_smart_padding(params->profile) && !params->fr_dab;
    const int dab_subch = params->fr_dab ? (int)(params->bitrate / 8000) : 0;

    ibuf = malloc(frame_length * fmt->bytes_per_frame);
    aacenc_progress_init(&progress, pcm_get_length(reader), fmt->sample_rate);

    for (;;) {
        if (g_interrupted)
            nread = 0;
        if (nread > 0) {
            if ((nread = pcm_read_frames(reader, ibuf, frame_length)) < 0) {
                fprintf(stderr, "ERROR: read failed\n");
                goto END;
            }
            if (!params->silent)
                aacenc_progress_update(&progress, pcm_get_position(reader),
                                       fmt->sample_rate * 2);
        }
        ip = ibuf;
        remaining = nread;
        do {
            obp = &obuf[flip];
            consumed = aac_encode_frame(encoder, fmt, ip, remaining, obp);
            if (consumed < 0) goto END;
            if (consumed == 0 && obp->size == 0) goto DONE;
            if (obp->size == 0) break;

            remaining -= consumed;
            ip += consumed * fmt->channels_per_frame;
            if (is_padding) {
            /*
             * As we pad 1 frame at beginning and ending by our extrapolator,
             * we want to drop them.
             * We delay output by 1 frame by double buffering, and discard
             * second frame and final frame from the encoder.
             * Since sbr_header is included in the first frame (in case of
             * SBR), we cannot discard first frame. So we pick second instead.
             */
                flip ^= 1;
                ++encoded;
                if (encoded == 1 || encoded == 3)
                    continue;
            }
            if (dab_subch) {
                if (write_sample_dab(params->output_fp, &obuf[flip], dab_subch) < 0)
                    goto END;
            } else if (write_sample(params->output_fp, m4af, &obuf[flip]) < 0)
                goto END;
            ++frames_written;
        } while (remaining > 0);
    }
DONE:
    /*
     * When interrupted, we haven't pulled out last extrapolated frames
     * from the reader. Therefore, we have to write the final outcome.
     */
    if (g_interrupted) {
        if (write_sample(params->output_fp, m4af, &obp[flip^1]) < 0)
            goto END;
        ++frames_written;
    }
    if (!params->silent)
        aacenc_progress_finish(&progress, pcm_get_position(reader));
    rc = frames_written;
END:
    if (ibuf) free(ibuf);
    if (obuf[0].data) free(obuf[0].data);
    if (obuf[1].data) free(obuf[1].data);
    return rc;
}

static
void put_tool_tag(m4af_ctx_t *m4af, const aacenc_param_ex_t *params,
                  HANDLE_AACENCODER encoder)
{
    char tool_info[256];
    char *p = tool_info;
    LIB_INFO lib_info;

    /* Encoder identity: PompAAC, crediting the open-source bases it is built on. */
    p += sprintf(p, "PompAAC based on " PROGNAME " %s, ", fdkaac_version);
    aacenc_get_lib_info(&lib_info);
    p += sprintf(p, "libfdk-aac %s, ", lib_info.versionStr);
    if (params->bitrate_mode)
        sprintf(p, "VBR mode %d", params->bitrate_mode);
    else
        sprintf(p, "CBR %dkbps",
                aacEncoder_GetParam(encoder, AACENC_BITRATE) / 1000);

    m4af_add_itmf_string_tag(m4af, M4AF_TAG_TOOL, tool_info);
}

static
int finalize_m4a(m4af_ctx_t *m4af, const aacenc_param_ex_t *params,
                 HANDLE_AACENCODER encoder)
{
    unsigned i;
    aacenc_tag_entry_t *tag;

    /* --minimal-moov: emit the smallest legal moov - skip ALL auto metadata
     * (tool tag + gapless iTunSMPB + user/json tags). Playback skeleton stays. */
    if (!params->minimal_moov) {
        tag = params->source_tags.tag_table;
        for (i = 0; i < params->source_tags.tag_count; ++i, ++tag)
            aacenc_write_tag_entry(m4af, tag);

        if (params->json_filename)
            aacenc_write_tags_from_json(m4af, params->json_filename);

        tag = params->tags.tag_table;
        for (i = 0; i < params->tags.tag_count; ++i, ++tag)
            aacenc_write_tag_entry(m4af, tag);

        if (!params->no_tool_tag)
            put_tool_tag(m4af, params, encoder);
    }

    if (m4af_finalize(m4af, params->moov_before_mdat) < 0) {
        fprintf(stderr, "ERROR: failed to finalize m4a\n");
        return -1;
    }
    return 0;
}

static
char *generate_output_filename(const char *filename, const char *ext)
{
    char *p = 0;
    size_t ext_len = strlen(ext);

    if (strcmp(filename, "-") == 0) {
        p = malloc(ext_len + 6);
        sprintf(p, "stdin%s", ext);
    } else {
        const char *base = aacenc_basename(filename);
        size_t ilen = strlen(base);
        const char *ext_org = strrchr(base, '.');
        if (ext_org) ilen = ext_org - base;
        p = malloc(ilen + ext_len + 1);
        sprintf(p, "%.*s%s", (int)ilen, base, ext);
    }
    return p;
}

static
int parse_raw_spec(const char *spec, pcm_sample_description_t *desc)
{
    unsigned bits;
    unsigned char c_type, c_endian = 'L';
    int type;

    if (sscanf(spec, "%c%u%c", &c_type, &bits, &c_endian) < 2)
        return -1;
    c_type = toupper(c_type);
    c_endian = toupper(c_endian);

    if (c_type == 'S')
        type = 1;
    else if (c_type == 'U')
        type = 2;
    else if (c_type == 'F')
        type = 4;
    else
        return -1;

    if (c_endian == 'B')
        type |= 8;
    else if (c_endian != 'L')
        return -1;

    if (c_type == 'F' && bits != 32 && bits != 64)
        return -1;
    if (c_type != 'F' && (bits < 8 || bits > 32))
        return -1;

    desc->sample_type = type;
    desc->bits_per_channel = bits;
    return 0;
}

static pcm_io_vtbl_t pcm_io_vtbl = {
    read_callback, seek_callback, tell_callback
};
static pcm_io_vtbl_t pcm_io_vtbl_noseek = { read_callback, 0, tell_callback };

static
pcm_reader_t *open_input(aacenc_param_ex_t *params)
{
    pcm_io_context_t io = { 0 };
    pcm_reader_t *reader = 0;

    if ((params->input_fp = aacenc_fopen(params->input_filename, "rb")) == 0) {
        aacenc_fprintf(stderr, "ERROR: %s: %s\n", params->input_filename,
                       strerror(errno));
        goto FAIL;
    }
    io.cookie = params->input_fp;
    if (aacenc_seekable(params->input_fp))
        io.vtbl = &pcm_io_vtbl;
    else
        io.vtbl = &pcm_io_vtbl_noseek;

    if (params->is_raw) {
        int bytes_per_channel;
        pcm_sample_description_t desc = { 0 };
        if (parse_raw_spec(params->raw_format, &desc) < 0) {
            fprintf(stderr, "ERROR: invalid raw-format spec\n");
            goto FAIL;
        }
        desc.sample_rate = params->raw_rate;
        desc.channels_per_frame = params->raw_channels;
        bytes_per_channel = (desc.bits_per_channel + 7) / 8;
        desc.bytes_per_frame = params->raw_channels * bytes_per_channel;
        if ((reader = raw_open(&io, &desc)) == 0) {
            fprintf(stderr, "ERROR: failed to open raw input\n");
            goto FAIL;
        }
    } else {
        int c;
        ungetc(c = getc(params->input_fp), params->input_fp);

        switch (c) {
        case 'R':
            if ((reader = wav_open(&io, params->ignore_length)) == 0) {
                fprintf(stderr, "ERROR: broken / unsupported input file\n");
                goto FAIL;
            }
            break;
        case 'c':
            params->source_tag_ctx.add = aacenc_add_tag_entry_to_store;
            params->source_tag_ctx.add_ctx = &params->source_tags;
            if ((reader = caf_open(&io,
                                   aacenc_translate_generic_text_tag,
                                   &params->source_tag_ctx)) == 0) {
                fprintf(stderr, "ERROR: broken / unsupported input file\n");
                goto FAIL;
            }
            break;
        default:
            fprintf(stderr, "ERROR: unsupported input file\n");
            goto FAIL;
        }
    }
    reader = pcm_open_native_converter(reader);
    if (reader && PCM_IS_FLOAT(pcm_get_format(reader)))
        reader = limiter_open(reader);
    if (reader && (reader = pcm_open_sint16_converter(reader)) != 0) {
        if (do_smart_padding(params->profile))
            reader = extrapolater_open(reader);
    }
    return reader;
FAIL:
    return 0;
}

int main(int argc, char **argv)
{
    static m4af_io_callbacks_t m4af_io = {
        read_callback, write_callback, seek_callback, tell_callback
    };
    aacenc_param_ex_t params = { 0 };

    int result = 2;
    char *output_filename = 0;
    pcm_reader_t *reader = 0;
    HANDLE_AACENCODER encoder = 0;
    AACENC_InfoStruct aacinfo = { 0 };
    m4af_ctx_t *m4af = 0;
    const pcm_sample_description_t *sample_format;
    int frame_count = 0;
    int sbr_mode = 0;
    unsigned scale_shift = 0;

    setlocale(LC_CTYPE, "");
    setbuf(stderr, 0);

    if (parse_options(argc, argv, &params) < 0)
        return 1;

    if ((reader = open_input(&params)) == 0)
        goto END;

    sample_format = pcm_get_format(reader);

    /* DAB+: auto-select virtual AOT from bitrate + channels (like odr-audioenc),
       unless the user forced a profile with -p. */
    if (params.fr_dab && params.profile == 0) {
        int subch = params.bitrate / 8000;
        int ch = sample_format->channels_per_frame;
        if (ch == 2 && subch <= 6)
            params.profile = AOT_DABPLUS_PS;      /* HE-AAC v2 */
        else if ((ch == 1 && subch <= 8) || (ch == 2 && subch <= 10))
            params.profile = AOT_DABPLUS_SBR;     /* HE-AAC */
        else
            params.profile = AOT_DABPLUS_AAC_LC;  /* AAC-LC */
    }

    sbr_mode = aacenc_is_sbr_active((aacenc_param_t*)&params);
    if (sbr_mode && !aacenc_is_sbr_ratio_available()) {
        fprintf(stderr, "WARNING: Only dual-rate SBR is available "
                        "for this version\n");
        params.sbr_ratio = 2;
    }
    scale_shift = aacenc_is_dual_rate_sbr((aacenc_param_t*)&params);
    params.sbr_signaling = 0;
    if (sbr_mode) {
        if (params.transport_format == TT_MP4_LOAS || !scale_shift)
            params.sbr_signaling = 2;
        if (params.transport_format == TT_MP4_RAW &&
            aacenc_is_explicit_bw_compatible_sbr_signaling_available())
            params.sbr_signaling = 1;
    }
    if (aacenc_init(&encoder, (aacenc_param_t*)&params, sample_format,
                    &aacinfo) < 0)
        goto END;

    /* DAB+ static DLS label: build the X-PAD once and register it for injection.
       PAD size 16 B (X-PAD payload) fits a single ~14-char DLS segment; larger
       labels use up to 3 segments in one PAD. */
    if (params.fr_dab && params.fr_dab_label && params.fr_dab_label[0]) {
        static unsigned char dab_pad_buf[197];
        int pad_size = 48;   /* variable-size X-PAD payload incl F-PAD */
        int n = dab_pad_build_dls(dab_pad_buf, pad_size, params.fr_dab_label, 0);
        if (n <= 0) {
            fprintf(stderr, "DAB+: label too long for one PAD (max ~%d chars)\n",
                    3 * 16);
            goto END;
        }
        aacenc_set_dab_pad(dab_pad_buf, n);
        if (params.fr_verbose)
            fprintf(stderr, " DAB+ DLS label       : \"%s\" (%d PAD bytes)\n",
                    params.fr_dab_label, n);
    }

    if (!params.output_filename) {
        const char *ext = params.transport_format ? ".aac" : ".m4a";
        output_filename = generate_output_filename(params.input_filename, ext);
        params.output_filename = output_filename;
    }

    if ((params.output_fp = aacenc_fopen(params.output_filename, "wb+")) == 0) {
        aacenc_fprintf(stderr, "ERROR: %s: %s\n", params.output_filename,
                       strerror(errno));
        goto END;
    }
    handle_signals();

    if (!params.transport_format) {
        uint32_t scale;
        unsigned framelen = aacinfo.frameLength;
        scale = sample_format->sample_rate >> scale_shift;
        if ((m4af = m4af_create(M4AF_CODEC_MP4A, scale, &m4af_io,
                                params.output_fp, params.no_timestamp)) < 0)
            goto END;
        m4af_set_num_channels(m4af, 0, sample_format->channels_per_frame);
        m4af_set_fixed_frame_duration(m4af, 0, framelen >> scale_shift);
        if (aacenc_is_explicit_bw_compatible_sbr_signaling_available())
            m4af_set_decoder_specific_info(m4af, 0,
                                           aacinfo.confBuf, aacinfo.confSize);
        else {
            uint8_t mp4asc[32];
            uint32_t ascsize = sizeof(mp4asc);
            aacenc_mp4asc((aacenc_param_t*)&params, aacinfo.confBuf,
                          aacinfo.confSize, mp4asc, &ascsize);
            m4af_set_decoder_specific_info(m4af, 0, mp4asc, ascsize);
        }
        m4af_set_vbr_mode(m4af, 0, params.bitrate_mode);
        /* --minimal-moov also drops the gapless iTunSMPB tag (part of udta). */
        m4af_set_priming_mode(m4af, params.minimal_moov ? 0 : params.gapless_mode + 1);
        m4af_begin_write(m4af);
    }
    frame_count = encode(&params, reader, encoder, aacinfo.frameLength, m4af);
    if (frame_count < 0)
        goto END;
    if (m4af) {
        uint32_t padding;
#if AACENCODER_LIB_VL0 < 4
        uint32_t delay = aacinfo.encoderDelay;
        if (sbr_mode && params.profile != AOT_ER_AAC_ELD
            && !params.include_sbr_delay)
            delay -= 481 << scale_shift;
#else
        uint32_t delay = params.include_sbr_delay ? aacinfo.nDelay
                                                  : aacinfo.nDelayCore;
#endif
        int64_t frames_read = pcm_get_position(reader);

        padding = frame_count * aacinfo.frameLength - frames_read - delay;
        m4af_set_priming(m4af, 0, delay >> scale_shift, padding >> scale_shift);
        if (finalize_m4a(m4af, &params, encoder) < 0)
            goto END;
    }
    result = 0;
END:
    if (reader) pcm_teardown(&reader);
    if (params.input_fp) fclose(params.input_fp);
    if (m4af) m4af_teardown(&m4af);
    if (params.output_fp) fclose(params.output_fp);
    if (encoder) aacEncClose(&encoder);
    if (output_filename) free(output_filename);
    if (params.tags.tag_table)
        aacenc_free_tag_store(&params.tags);
    if (params.source_tags.tag_table)
        aacenc_free_tag_store(&params.source_tags);

    return result;
}
