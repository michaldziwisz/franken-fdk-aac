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
#include "m4af.h"
#include "progress.h"
#include "version.h"
#include "metadata.h"

#define PROGNAME "fdkaac"

static volatile int g_interrupted = 0;

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
" --unlock-bitrate            Remove the LOWER bitrate floor (8k HE stereo, 6k LC). NOTE:\n"
"                               -b is then read literally as bps. Upper 6144*ch ceiling stays.\n"
"                               Off-spec, deliberate opt-in.\n"
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
"                               zeroed. + = SOFT: keep near-miss bands (just under thr) at\n"
"                               the coarsest scf so side fades out smoothly; - = HARD: drop\n"
"                               bands that only just clear thr = early cutoff. Try +3..+6.\n"
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
" --ps-icc-mode <n>           ICC rotation: -1 def, 0 ROT_A, 1 ROT_B.\n"
"\n== C. Bandwidth & SBR (high band) ==\n"
" --core-cutoff <hz>          Force core bandwidth in Hz even under SBR (where -w is\n"
"                               ignored). 0=default, max sr/2. See also --uncap-bandwidth.\n"
" --sbr-start <n>             SBR bs_start_freq index: -1 def, 0..15. (Validated by FDK;\n"
"                               bad start/stop pair => init fails.)\n"
" --sbr-stop <n>              SBR bs_stop_freq index: -1 def, 0..13.\n"
" --sbr-freqscale <n>         Freq grouping: -1 def, 0..3 (0=linear).\n"
" --sbr-alterscale <n>        Alternative scale resolution: -1 def, 0/1.\n"
" --sbr-noise-bands <n>       SBR noise bands (noise description density): -1 def, 1..5.\n"
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
"                               decoder tunes into an Icecast/Shoutcast HE-AAC stream.\n"
"                               1 = SBR config in every frame => near-instant (no core-\n"
"                               only moment); higher = longer core-only period. FDK\n"
"                               default ~10 frames (~0.46 s @44.1k). -1 off. See verbose.\n"
"\n== D. Masking / noise shaping / detail ==\n"
" --ath-scale <n>             Global masking-threshold scale Q8 (256=x1.0 def). >256 =\n"
"                               noisier/fewer bits, <256 = cleaner/more detail. min 1,\n"
"                               max ~4096. This is the main global SMR knob.\n"
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
" --pns-tonality <x>          Scales the PNS tonality detection threshold. 1.0=default,\n"
"                               higher = more (also less-noisy) bands become PNS = wider\n"
"                               noise substitution.\n"
" --pns-refpower <x>          Scales the PNS reference-power detection threshold. 1.0=def.\n"
" --pns-gapfill <x>           Scales the PNS gap-fill threshold. 1.0=default.\n"
" --pns-min-width <n>         Minimum SFB width for PNS (raw). Lower = PNS on narrower bands.\n"
"\n== E. Block switching & bitrate control ==\n"
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
#define OPT_FR_PNS_GAIN          M4AF_FOURCC('f','p','g','a')
#define OPT_FR_PNS_TONALITY      M4AF_FOURCC('f','p','t','o')
#define OPT_FR_PNS_REFPOWER      M4AF_FOURCC('f','p','r','p')
#define OPT_FR_PNS_GAPFILL       M4AF_FOURCC('f','p','g','f')
#define OPT_FR_PNS_MIN_WIDTH     M4AF_FOURCC('f','p','m','w')
#define OPT_FR_PEAK_BITRATE      M4AF_FOURCC('f','p','k','b')
#define OPT_FR_VERBOSE           M4AF_FOURCC('f','v','r','b')

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
        { "pns-gain",            required_argument, 0, OPT_FR_PNS_GAIN          },
        { "pns-tonality",        required_argument, 0, OPT_FR_PNS_TONALITY      },
        { "pns-refpower",        required_argument, 0, OPT_FR_PNS_REFPOWER      },
        { "pns-gapfill",         required_argument, 0, OPT_FR_PNS_GAPFILL       },
        { "pns-min-width",       required_argument, 0, OPT_FR_PNS_MIN_WIDTH     },
        { "peak-bitrate",     required_argument, 0, OPT_FR_PEAK_BITRATE    },
        { "verbose",          no_argument,       0, OPT_FR_VERBOSE         },

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
    params->fr_pns_gain = -1;
    params->fr_pns_tonality = -1;
    params->fr_pns_refpower = -1;
    params->fr_pns_gapfill = -1;
    params->fr_pns_min_width = -1;
    params->fr_peak_bitrate = -1;
    params->fr_verbose = 0;

    aacenc_getmainargs(&argc, &argv);
    while ((ch = getopt_long(argc, argv, "hp:b:m:w:a:L:s:f:CP:G:Io:SR",
                             long_options, 0)) != EOF) {
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
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for msmask\n"); return -1; }
            params->fr_ms_mask = n; break;
        case OPT_FR_MS_BANDS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for msbands\n"); return -1; }
            params->fr_ms_bands = n; break;
        case OPT_FR_IS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for is\n"); return -1; }
            params->fr_is = n; break;
        case OPT_FR_IS_BANDS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for isbands\n"); return -1; }
            params->fr_is_bands = n; break;
        case OPT_FR_IS_MINSFBS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for is-min-sfbs\n"); return -1; }
            params->fr_is_minsfbs = n; break;
        case OPT_FR_IS_CORR:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for is-corr-thresh\n"); return -1; }
            params->fr_is_corr = n; break;
        case OPT_FR_IS_LRRATIO:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for is-lr-ratio\n"); return -1; }
            params->fr_is_lrratio = n; break;
        case OPT_FR_CORE_CUTOFF:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for core-cutoff\n"); return -1; }
            params->fr_core_cutoff = n; break;
        case OPT_FR_SBR_START:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-start\n"); return -1; }
            params->fr_sbr_start = n; break;
        case OPT_FR_SBR_STOP:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-stop\n"); return -1; }
            params->fr_sbr_stop = n; break;
        case OPT_FR_SBR_FREQSCALE:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-freqscale\n"); return -1; }
            params->fr_sbr_freqscale = n; break;
        case OPT_FR_SBR_ALTERSCALE:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-alterscale\n"); return -1; }
            params->fr_sbr_alterscale = n; break;
        case OPT_FR_SBR_NOISE_BANDS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-noise-bands\n"); return -1; }
            params->fr_sbr_noise_bands = n; break;
        case OPT_FR_SBR_AMP_RES:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-amp-res\n"); return -1; }
            params->fr_sbr_amp_res = n; break;
        case OPT_FR_SBR_DATA_EXTRA:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-data-extra\n"); return -1; }
            params->fr_sbr_data_extra = n; break;
        case OPT_FR_PS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for ps\n"); return -1; }
            params->fr_ps = n; break;
        case OPT_FR_PS_IID_QUANT:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for ps-iid-quant\n"); return -1; }
            params->fr_ps_iid_quant = n; break;
        case OPT_FR_TNS_MASK:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for tns-mask\n"); return -1; }
            params->fr_tns_mask = n; break;
        case OPT_FR_TNS_ORDER:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for tns-order\n"); return -1; }
            params->fr_tns_order = n; break;
        case OPT_FR_PNS:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for pns\n"); return -1; }
            params->fr_pns = n; break;
        case OPT_FR_PNS_START:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for pns-start\n"); return -1; }
            params->fr_pns_start = n; break;
        case OPT_FR_ATH_SCALE:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for ath-scale\n"); return -1; }
            params->fr_ath_scale = n; break;
        case OPT_FR_BLOCK_BIAS:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 255) { fprintf(stderr, "invalid arg for block-bias (0..255)\n"); return -1; }
            params->fr_block_bias = n; break;
        case OPT_FR_VBR_RESERVOIR:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for vbr-reservoir\n"); return -1; }
            params->fr_vbr_reservoir = n; break;
        case OPT_FR_MAX_BITS_FRAME:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for max-bits-frame\n"); return -1; }
            params->fr_max_bits_frame = n; break;
        case OPT_FR_MIN_BITS_FRAME:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for min-bits-frame\n"); return -1; }
            params->fr_min_bits_frame = n; break;
        case OPT_FR_BITRES_MODE:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 2) { fprintf(stderr, "invalid arg for bitres-mode (0..2)\n"); return -1; }
            params->fr_bitres_mode = n; break;
        case OPT_FR_MS_BIAS:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for ms-bias\n"); return -1; }
            params->fr_ms_bias = n; break;
        case OPT_FR_UNCAP_BW:
            params->fr_uncap_bw = 1; break;
        case OPT_FR_IS_AGGRESSION:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 100) { fprintf(stderr, "invalid arg for is-aggression (0..100)\n"); return -1; }
            params->fr_is_aggression = n; break;
        case OPT_FR_FORCE_PNS:
            params->fr_force_pns = 1; break;
        case OPT_FR_UNLOCK_BITRATE:
            params->fr_unlock_bitrate = 1; break;
        case OPT_FR_SPEECH:
            params->fr_speech = 1; break;
        case OPT_FR_SPREAD_MASK:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for spread-mask (>=0, 256=neutral)\n"); return -1; }
            params->fr_spread_mask = n; break;
        case OPT_FR_MS_BAND_LO:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for msbands-lo\n"); return -1; }
            params->fr_ms_band_lo = n; break;
        case OPT_FR_MS_BAND_HI:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for msbands-hi\n"); return -1; }
            params->fr_ms_band_hi = n; break;
        case OPT_FR_MS_PRECISION:
            if (sscanf(optarg, "%d", &n) != 1 || n < 256) { fprintf(stderr, "invalid arg for ms-precision (>=256)\n"); return -1; }
            params->fr_ms_precision = n; break;
        case OPT_FR_SBR_NUM_ENV:
            if (sscanf(optarg, "%d", &n) != 1 || (n!=1 && n!=2 && n!=4)) { fprintf(stderr, "invalid arg for sbr-num-env (1,2,4; 8 exceeds standard frame grid)\n"); return -1; }
            params->fr_sbr_num_env = n; break;
        case OPT_FR_SBR_FREQRES_FIXFIX:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for sbr-freqres-fixfix (0,1)\n"); return -1; }
            params->fr_sbr_freqres_fixfix = n; break;
        case OPT_FR_SBR_STEREO_MODE:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-stereo-mode (0..3)\n"); return -1; }
            params->fr_sbr_stereo_mode = n; break;
        case OPT_FR_SBR_INVF:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 3) { fprintf(stderr, "invalid arg for sbr-invf (0..3)\n"); return -1; }
            params->fr_sbr_invf = n; break;
        case OPT_FR_SBR_NF_OFFSET:
            if (sscanf(optarg, "%d", &n) != 1) { fprintf(stderr, "invalid arg for sbr-noise-floor-offset\n"); return -1; }
            params->fr_sbr_noise_floor_offset = n; break;
        case OPT_FR_PS_ICC:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-icc (0,1)\n"); return -1; }
            params->fr_ps_icc = n; break;
        case OPT_FR_PS_ICC_MODE:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for ps-icc-mode (0,1)\n"); return -1; }
            params->fr_ps_icc_mode = n; break;
        case OPT_FR_IS_BAND_LO:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-lo (>=0)\n"); return -1; }
            params->fr_is_band_lo = n; break;
        case OPT_FR_IS_BAND_HI:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-hi (>=0)\n"); return -1; }
            params->fr_is_band_hi = n; break;
        case OPT_FR_IS_FORCE_LO:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-force-lo (>=0)\n"); return -1; }
            params->fr_is_force_lo = n; break;
        case OPT_FR_IS_FORCE_HI:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for is-force-hi (>=0)\n"); return -1; }
            params->fr_is_force_hi = n; break;
        case OPT_FR_MINSNR_SCALE:
            if (sscanf(optarg, "%d", &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-scale (>=1, 256=neutral, <256=more detail)\n"); return -1; }
            params->fr_minsnr_scale = n; break;
        case OPT_FR_MINSNR_CLAMP_HI:
            if (sscanf(optarg, "%d", &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-clamp-hi (>=1, 256=neutral)\n"); return -1; }
            params->fr_minsnr_clamp_hi = n; break;
        case OPT_FR_MINSNR_CLAMP_LO:
            if (sscanf(optarg, "%d", &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for minsnr-clamp-lo (>=1, 256=neutral)\n"); return -1; }
            params->fr_minsnr_clamp_lo = n; break;
        case OPT_FR_REDUCE_CLAMP:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0 || n > 1) { fprintf(stderr, "invalid arg for reduce-clamp (0 off,1 on)\n"); return -1; }
            params->fr_reduce_clamp = n; break;
        case OPT_FR_MID_BIAS:
            if (sscanf(optarg, "%d", &n) != 1 || n < 256) { fprintf(stderr, "invalid arg for mid-bias (>=256, 256=neutral)\n"); return -1; }
            params->fr_mid_bias = n; break;
        case OPT_FR_SIDE_BIAS:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < -24.0 || dv > 50.0) { fprintf(stderr, "invalid arg for side-bias (-24.0..50.0 dB; + = more side, - = destroy side, 0=off)\n"); return -1; }
              params->fr_side_bias = (int)(dv * 10.0 + (dv<0?-0.5:0.5)); } break;
        case OPT_FR_SIDE_KNEE:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < -24.0 || dv > 50.0) { fprintf(stderr, "invalid arg for side-knee (-24.0..50.0 dB; + = soft knee, - = hard cutoff, 0=off)\n"); return -1; }
              params->fr_side_knee = (int)(dv * 10.0 + (dv<0?-0.5:0.5)); } break;
        case OPT_FR_MASK_SLOPE:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < -24.0 || dv > 50.0) { fprintf(stderr, "invalid arg for mask-slope (-24.0..50.0 dB; + = more detail in quiet bands, - = starve harder, 0=off)\n"); return -1; }
              params->fr_mask_slope = (int)(dv * 10.0 + (dv<0?-0.5:0.5)); } break;
        case OPT_FR_SBR_HEADER_PERIOD:
            if (sscanf(optarg, "%d", &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for sbr-header-period (>=1; 1=near-instant SBR sync)\n"); return -1; }
            params->fr_sbr_header_period = n; break;
        case OPT_FR_PNS_GAIN:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < 0.0) { fprintf(stderr, "invalid arg for pns-gain (>=0.0; 1.0=unchanged)\n"); return -1; }
              params->fr_pns_gain = (int)(dv * 100.0 + 0.5); } break;
        case OPT_FR_PNS_TONALITY:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < 0.0) { fprintf(stderr, "invalid arg for pns-tonality (>=0.0; 1.0=unchanged)\n"); return -1; }
              params->fr_pns_tonality = (int)(dv * 100.0 + 0.5); } break;
        case OPT_FR_PNS_REFPOWER:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < 0.0) { fprintf(stderr, "invalid arg for pns-refpower (>=0.0; 1.0=unchanged)\n"); return -1; }
              params->fr_pns_refpower = (int)(dv * 100.0 + 0.5); } break;
        case OPT_FR_PNS_GAPFILL:
            { double dv; if (sscanf(optarg, "%lf", &dv) != 1 || dv < 0.0) { fprintf(stderr, "invalid arg for pns-gapfill (>=0.0; 1.0=unchanged)\n"); return -1; }
              params->fr_pns_gapfill = (int)(dv * 100.0 + 0.5); } break;
        case OPT_FR_PNS_MIN_WIDTH:
            if (sscanf(optarg, "%d", &n) != 1 || n < 1) { fprintf(stderr, "invalid arg for pns-min-width (>=1)\n"); return -1; }
            params->fr_pns_min_width = n; break;
        case OPT_FR_PEAK_BITRATE:
            if (sscanf(optarg, "%d", &n) != 1 || n < 0) { fprintf(stderr, "invalid arg for peak-bitrate\n"); return -1; }
            params->fr_peak_bitrate = n; break;
        case OPT_FR_VERBOSE:
            params->fr_verbose = 1; break;

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

    /* SBR floor guard: HE-AAC / HE-AAC v2 (profiles 5/29/...) cannot configure
     * SBR below ~16 kbps - the encoder would snap up silently (or fail). Warn so
     * the user is not surprised that -b 8000 under HE-AAC becomes 16 kbps. */
    if (params->bitrate > 0 && params->bitrate < 16000 &&
        (params->profile == 5 || params->profile == 29 ||
         params->profile == 4 /* SSR-ish */)) {
        fprintf(stderr,
                "WARNING: HE-AAC (SBR) has a hard ~16 kbps floor; -b %d will be "
                "raised to 16 kbps. For genuinely lower rates use AAC-LC (-p 2) "
                "with --unlock-bitrate.\n", params->bitrate);
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
    const int is_padding = do_smart_padding(params->profile);

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
            if (write_sample(params->output_fp, m4af, &obuf[flip]) < 0)
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
