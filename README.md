# Franken FDK AAC — a laboratory/"geek" AAC encoder (FDK)

Built on top of **libfdk-aac 2.0.3** (mstorsjo/fdk-aac) + the
**nu774/fdkaac 1.0.2** frontend, with bolted-on CLI switches that expose
the normally hardcoded, internal decisions of the FDK encoder. It serves
extreme debugging and experimentation with AAC/HE-AAC/HE-AAC v2.

Binaries (static, no external DLLs):
- `fdkaac-franken-x64.exe` — Windows 64-bit (PE32+)
- `fdkaac-franken-x86.exe` — Windows 32-bit (PE32)

## Download

Prebuilt Windows binaries are published as GitHub Releases, built and hosted by
GitHub (no login needed to download):

**→ https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Grab `franken-fdk-aac-x64-vX.Y.Z.zip` (64-bit) or the `x86` one; each zip holds
the `.exe` plus the full documentation. There are no binaries committed to the
repo — see "Build" at the bottom for building it yourself.


All original nu774 frontend options (`-p/--profile`, `-b/--bitrate`,
`-m/--bitrate-mode`, `-w/--bandwidth`, `-a/--afterburner`, `-s/--sbr-ratio`,
`-f/--transport-format`, tagging, etc.) work as before. Below are only the
NEW switches. See also `fdkaac-franken-x64.exe --help`.

NOTE: this is an "I-know-what-I'm-doing" tool. Most of these parameters
deliberately let you go beyond what the FDK automation does — you can
knowingly wreck the stereo image, the bandwidth, or the quality with them.
The sentinel `-1` (or `0` for `--core-cutoff`) = "leave FDK's default behavior".

Author: Michał Dziwisz. Subject-matter consultant: Patryk Faliszewski.
Built on open source: libfdk-aac (Fraunhofer IIS) + the nu774/fdkaac frontend.

---

## Option groups overview (grouped the same way in `--help`)

The options are ordered by topic, roughly from easiest to most geeky. The same
order applies in `--help` (groups A–E) and in the sections below:

- **A. Start here (consumer):** `--verbose`, `--is-aggression`, `--speech`,
  `--uncap-bandwidth`, `--unlock-bitrate` — sections 0, 1, 2, 10.
- **B. Stereo:** MS (`--msmask`, `--msbands`, `--msbands-lo/-hi`, `--ms-bias`,
  `--ms-precision`), IS (`--is`, `--isbands`, `--is-*`), PS (`--ps`, `--ps-iid-quant`,
  `--ps-icc`, `--ps-icc-mode`) — sections 1, 4, 8.
- **C. Bandwidth and SBR:** `--core-cutoff`, `--sbr-*` — sections 2, 3.
- **D. Masking / noise / detail:** `--ath-scale`, `--spread-mask`, `--tns-*`,
  `--pns`, `--pns-start`, `--force-pns` — sections 5, 6.
- **E. Blocks and bitrate:** `--block-bias`, `--vbr-reservoir`, `--peak-bitrate`,
  `--max-bits-frame`, `--min-bits-frame`, `--bitres-mode` — sections 7, 9.

Tip: at the end, `--verbose` prints a "franken overrides applied" section —
exactly those switches which, in a given run, deviate from pure FDK.

---

## 0. Diagnostics

### `--verbose`
Before encoding, it dumps to stderr the REAL parameters chosen by the encoder
(not just your overrides): AOT, bitrate/mode, samplerate, channel-mode, the
EFFECTIVE core cutoff in Hz, afterburner, transport, signaling, plus the state
of the coding tools chosen by the encoder (TNS on/off, PNS on/off, Intensity
stereo on/off, MS stereo on/off). When SBR is active: sbr-ratio + effective
start/stop freq index, freq scale, noise bands, amp res. At the end, a list of
your overrides (-1/0 = not set, left to the encoder). Perfect for learning the
starting point (e.g. the default HE-AAC v2 48k cutoff = 8613 Hz).

---

## 1. Joint stereo — MS / IS / independent stereo

By default FDK itself decides per-band about MS (mid/side) and IS (intensity).
Here you can override these decisions and force extreme configurations.

| Switch | Values | Default | Description |
|---|---|---|---|
| `--msmask <n>` | -1 auto, 0 off, 1 on | -1 | Force MS: `0` = all bands L/R (completely independent stereo), `1` = MS on all bands. |
| `--msbands <n>` | -1 no limit, 0..N | -1 | Maximum SFB number that may use MS (takes the N LOWEST bands). Above that — MS disabled. |
| `--msbands-lo <n>` | -1 off, 0..N | -1 | START (lowest SFB band number) of the range in which MS is allowed. |
| `--msbands-hi <n>` | -1 off, 0..N | -1 | END (highest SFB band number) of the MS range. Used together with `--msbands-lo` as a FROM–TO pair. Bands outside this range go pure L/R. |
| `--ms-precision <n>` | 256..no limit (Q8) | -1 (off) | *(Very experimental — you probably want `--side-bias` instead.)* Scales the precision of MS bands globally (both mid and side together), LAME `-q` style. 256=no change, 384~1.5x, 512~2x. In practice its reach is limited: above ~600-800 the threshold hits FDK's hard floor and under CBR bits are only shifted between bands, so the sound stops changing. Superseded for stereo tuning by `--side-bias`/`--side-knee`, which act per-channel exactly where it matters. |
| `--mid-bias <n>` | 256..no limit (Q8) | -1 (off) | *(Very experimental — rarely needed.)* `>256` RAISES the mid (L+R) threshold after the MS butterfly to free bits from mid for side. The cleaner, better-measured way to shift the mid↔side balance is `--side-bias` (which pulls from the same budget from the side end). Kept for completeness. 256=off. |
| `--side-bias <dB>` | -24.0 .. +50.0 | 0 (off) | **The main stereo-quality knob.** Shifts the SIDE (L−R) channel's masking threshold on MS-coded bands, at the exact point where FDK decides whether a scalefactor band is coded or dropped (`energy > threshold` in `sf_estim.cpp`). Sign = EFFECT: **`+` steers MORE bits to the side channel** (lower threshold → fewer side bands zeroed, the survivors quantized finer → cleaner stereo width, reverb tails, ambience), at the mid channel's expense; **`−` deliberately DEGRADES the side** (raises the threshold → side bands drop out → narrower, more mono image). This is the very same energy-vs-threshold decision LAME rides for its bit allocation and MusePack drives with `--ms` — nothing exotic. Because it is a fixed-budget tradeoff, at low bitrate the mid channel audibly gives up bits; that is expected, not a bug. Sane range **+3 .. +9** for "more spacious", negative only for extreme/artistic low-bitrate mangling. Measured on real material at 96 kbps: `+9` lifts side error in the 4–8 kHz region while mid degrades a few dB. 0 = off (bit-identical to stock). |
| `--side-knee <dB>` | -24.0 .. +50.0 | 0 (off) | Shapes HOW SHARPLY a side band flips between "coded" and "zeroed" at the threshold. Stock FDK is a hard cliff: the instant `energy ≤ threshold` the whole band is dropped to zero. **`+` = SOFT knee**: bands sitting up to N dB *below* the threshold are still kept (coded at the coarsest scalefactor) instead of dropped, so the side fades out gradually rather than switching off — smoother decay of reverb/air. **`−` = HARD knee**: bands that only just clear the threshold (within N dB *above* it) are forced to zero anyway, cutting the side off early — leaner, more aggressive. Ortogonal to `--side-bias` and combines with it. Sane range **+3 .. +6**. 0 = off. |
| `--mask-slope <dB>` | -24.0 .. +50.0 | 0 (off) | Global (mid **and** side) tuning of FDK's **Masking-Slope-Adaptation** — a NON-masking heuristic (`adj_thr.cpp`) that relaxes the required SNR for scalefactor bands whose energy sits far below the frame average (stock: more than ~10 dB below), i.e. it deliberately starves very quiet bands to save bits. This knob shifts that "how far below average before I stop caring" threshold. **`+` raises it → fewer quiet bands starved → more detail in quiet passages, reverb tails, decays** (costs bits); **`−` lowers it → quiet bands starved harder → leaner, hollower, more bits for the loud stuff**. Same family as `--side-bias` but applied to both channels and keyed on energy-vs-average rather than the MS threshold. Subtle on dense material (it only touches the quietest bands); most audible on sparse/reverberant content. Sane range **±6 .. ±12**. 0 = off. |
| `--is <n>` | -1 auto, 0 off, 1 on | -1 | Intensity stereo globally on/off. |
| `--isbands <n>` | -1 no limit, 0..N | -1 | Maximum number of SFBs that may use intensity. Above that — coded normally. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | CONSUMER slider: how hard the encoder should push intensity stereo. Start here, leave the advanced `--is-*` alone. |
| `--is-min-sfbs <n>` | -1 def(6), 0..N | -1 | (advanced) Min. number of contiguous SFBs before IS turns on. |
| `--is-corr-thresh <n>` | -1 def(243), Q8 | -1 | (advanced) L/R correlation threshold for IS in Q8 (256=1.0). |
| `--is-lr-ratio <n>` | -1 def(179), Q8 | -1 | (advanced) L/R energy balance threshold for IS in Q8 (256=1.0). |
| `--is-lo <sfb>` | -1 off, 0..N | -1 | Allow intensity stereo ONLY from this SFB upward. Bands below stay pure L/R. Only RESTRICTS where FDK may place IS — never forces it on. |
| `--is-hi <sfb>` | -1 off, 0..N | -1 | Allow IS only up to this SFB (inclusive). Pair with `--is-lo` as a range. TIP: IS usually lands on LOW bands at low bitrate, so scan small values to see the effect. |
| `--is-force-lo <sfb>` | -1 off, 0..N | -1 | FORCE intensity stereo from this SFB, bypassing the correlation / min-sfbs / loudness gates. Laboratory mode: can deliberately wreck the stereo image (IS is lossy + directional — the right channel is zeroed, only a panning coefficient survives). The stream stays legal. |
| `--is-force-hi <sfb>` | -1 off, 0..N | -1 | Upper SFB of the forced-IS range (inclusive). |

### Intensity stereo in practice (how to use it, not the formulas)

What it is: intensity stereo (IS) in the upper bands drops separate L/R and sends
ONE energy envelope + direction (panning) information. The ear localizes high
tones poorly, so this saves quite a few bits — but at the cost of stereo
separation (the width of the scene at the top of the band narrows). You pay for it
especially on material with a real L/R difference in the highs (cymbals on one
side, spatial effects).

By default FDK is very CAUTIOUS with IS (hence your observation "you can barely
hear the difference"). There are three reasons, and that is what these knobs are
for:

1. IS admission gate: FDK considers IS at all only when `bitrate/band < 5`. At
   higher bitrates IS is disallowed entirely. `--is-aggression >=1` removes this
   gate.
2. Correlation threshold (`--is-corr-thresh`, Q8, 256=1.0, default 243 ~= 0.95):
   both channels must be at least ~95% similar to each other in a given band for
   IS to turn on. That is very high. Lower it -> IS catches more often, even when
   the channels are less similar. E.g. 180 (~0.70) = much more aggressive. Too low
   = audible direction errors.
3. Min. region length (`--is-min-sfbs`, default 6): IS turns on only on a band of
   at least 6 consecutive SFBs. Lower it to 1-2 -> IS also catches short
   fragments.

The relationship between them: for a given SFB to go into IS, ALL conditions
MUST be met at once — the admission gate AND correlation above the threshold AND
a sufficiently long region AND a stable direction. That is why lowering just one
threshold often does nothing (another still blocks it) — and that is why you
usually see no difference by manipulating correlation alone. `--is-aggression`
moves ALL of them at once, coherently.

How to set it:
- Simplest: `--is 1 --is-aggression 40` and listen. Too little IS -> raise to 70,
  100. Too much (the scene "glues together" at the top, direction artifacts) ->
  go down.
- 0 = FDK default (practically IS is barely active at typical bitrates).
- 100 = maximum: gate removed, correlation loose (~0.475), region from 1 SFB,
  wide direction tolerance. Many bands in IS, strongly audible, saves bits.
- Manual tuning only when you want precision: set `--is-aggression 0` and turn
  `--is-corr-thresh` (the main one), then `--is-min-sfbs`, finally `--is-lr-ratio`.
  The --is-* values OVERRIDE what the aggression slider set.
- Diagnostics: `--verbose` shows the effective thresholds (IS corr threshold Q8,
  min SFBs), so you see what actually went into the encoder.

MS/IS bias (point 2): the above `--is-*` control WHEN the encoder chooses
intensity stereo (decision thresholds from the FDK tuning table), independently
of the hard on/off. `--msbands` limits MS to the lower bands (correctly — the
mask and the L/R->M/S butterfly are synchronized, no "left=center, right=rest"
artifact).
- Completely independent stereo: `--msmask 0 --is 0`.
- "Laboratory" restriction of MS to the lower bands: e.g. `--msbands 6`.
- Forced full MS: `--msmask 1`.
- More eager IS: lower `--is-corr-thresh` (e.g. 150) and/or `--is-min-sfbs`.

### MS band range: --msbands, --msbands-lo, --msbands-hi (IMPORTANT, often confused)

The spectrum bands are numbered FROM THE BOTTOM: band 0 = lowest frequencies
(bass), the higher the number, the higher in the spectrum. In a typical LC stereo
there are about 49 of them.

There are TWO independent ways to limit where MS is applied:

1. `--msbands <n>` — "the lower N bands". MS allowed ONLY in bands 0..(n-1),
   i.e. from the bass upward to number n. This is always counted FROM THE BOTTOM.
   Example: `--msbands 6` = MS only on the 6 lowest bands, the rest pure L/R.

2. `--msbands-lo <lo>` + `--msbands-hi <hi>` — "FROM-TO range". MS allowed ONLY
   in bands numbered from `lo` to `hi` inclusive. Outside that range, pure L/R.
   It is a pair — you provide both. It lets you place MS ANYWHERE, including at
   the very top.

A concrete example (assuming ~49 bands in LC):
- You want MS ONLY on the 5 HIGHEST bands (e.g. to merge noise at the top and
  leave the bottom in full independent stereo)? The highest bands are numbers
  44..48: `--msbands-lo 44 --msbands-hi 48`.
- You want MS only in the MIDDLE of the band (e.g. 10..30)? `--msbands-lo 10 --msbands-hi 30`.
- You want MS on the 6 LOWEST? Simpler `--msbands 6` (or `--msbands-lo 0
  --msbands-hi 5` — the same).

Mnemonic: `--msbands` = "from the bottom up to", `--msbands-lo/-hi` = "from..to".
How many bands you actually have for a given mode/samplerate is shown by
`--verbose` (the "active SFBs" field).

## 2. Cutoff of the AAC core when SBR is active

The standard `-w/--bandwidth` in FDK is IGNORED when SBR is active
(HE-AAC v1/v2) — because `sbrEncoder_Init()` overrides the bandwidth with a value
from the SBR table.

| Switch | Values | Default | Description |
|---|---|---|---|
| `--core-cutoff <hz>` | 0 = default, >0 = Hz | 0 | Forces the AAC core bandwidth IN Hz even under SBR. Resistant to being overridden by SBR. |

Example (your case — 7.5 kHz of core at HE-AAC v2 48 kbps, where the table gives
less):
```
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
```
Verified: `--core-cutoff 7500` -> effective bandwidth 7500 Hz; stock `-w 7500`
under SBR stays 8613 Hz (ignored).

NOTE: you police the core limits yourself. The max is the core Nyquist (`sr/2`),
and with **dual-rate SBR the target samplerate is divided by 2** — keep that in
mind when choosing values.

## 3. Density / precision of SBR data

Overrides the settings from the SBR tuning table (after it is loaded).

| Switch | Values | Default | Description |
|---|---|---|---|
| `--sbr-start <n>` | -1 def, 0..15 | -1 | `bs_start_freq` index (start of the SBR band). |
| `--sbr-stop <n>` | -1 def, 0..13 | -1 | `bs_stop_freq` index (end of the SBR band). |
| `--sbr-freqscale <n>` | -1 def, 0..3 | -1 | Frequency grouping (0 = linear, higher = finer log). |
| `--sbr-alterscale <n>` | -1 def, 0/1 | -1 | Alternative scale resolution. |
| `--sbr-noise-bands <n>` | -1 def, 1..5 | -1 | Number of SBR noise bands (density of the noise description). |
| `--sbr-amp-res <n>` | -1 def, 0/1 | -1 | Envelope amplitude resolution: 0 = 1.5 dB, 1 = 3.0 dB. |
| `--sbr-data-extra <n>` | -1 def, 0/1 | -1 | Write extra SBR header data. |
| `--sbr-num-env <1\|2\|4>` | -1 off | -1 | Number of envelopes per frame. FORCES a static time grid (ignores the transient detector). More = better temporal resolution of the upper band, worse on attacks. (8 exceeds the standard grid — rejected.) |
| `--sbr-freqres-fixfix <0\|1>` | -1 off | -1 | Frequency resolution of the FIXFIX envelope (0 low, 1 high). |
| `--sbr-stereo-mode <0..3>` | -1 off | -1 | SBR stereo mode: 0 mono, 1 LR (full separation of the upper band), 2 coupling (economical, shared envelope + level), 3 switch-LRC (by default the coder chooses per-frame). Force 1 for max separation, 2 for economy. |
| `--sbr-invf <0..3>` | -1 auto | -1 | Force SBR inverse filtering: 0 off, 1 low, 2 mid, 3 high. Normally driven by the tonality estimator. Higher = stronger "whitening" of tonal SBR (less metallicness at the cost of detail). |
| `--sbr-noise-floor-offset <n>` | -128 off | -128 | SBR noise floor offset (small integer). Larger = more filling noise in the SBR reconstruction. |
| `--sbr-header-period <n>` | -1 off, >=1 | -1 | Frames between SBR headers = how fast the SBR high band "kicks in" when a decoder tunes into a live HE-AAC stream (Icecast/Shoutcast). The SBR CONFIG lives in a periodic header, not in every frame; a decoder joining mid-stream plays core-only (muffled) until the next header arrives. `1` = header in every frame → near-instant SBR lock (~23 ms); higher = longer core-only moment. FDK default is ~10 frames (~0.23 s HE dual-rate / ~0.46 s LC). FDK caps this to at most once per second, so very large values are clamped (e.g. 40 → 21 frames @44.1k). See `--verbose` for the effective period in ms. |

NOTE: `--sbr-start`/`--sbr-stop` are validated BY FDK — an incorrect start/stop
COMBINATION (wrong number of master bands) will give "encoder initialization
failed". This is a limitation of SBR itself, not a bug. Choose pairs (e.g. for
64k stereo start=5 stop=9, start=8 stop=14 work).

## 4. Parametric Stereo (HE-AAC v2)

PS describes stereo with a few parameters (IID/ICC...). Here you can control them,
even at the cost of the stereo image.

| Switch | Values | Default | Description |
|---|---|---|---|
| `--ps <n>` | -1 auto, 0 off, 1 on | -1 | Force sending the PS IID parameter. `0` = never (flattens the stereo image), `1` = always. Overrides the loudness-difference heuristic. |
| `--ps-iid-quant <n>` | -1 def, 0 coarse, 1 fine | -1 | IID quantization grid: coarse vs. fine. |
| `--ps-icc <n>` | -1 auto, 0 off, 1 on | -1 | Force ICC (Interchannel Coherence — channel similarity/coherence) on/off. |
| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | ICC rotation mode: 0 = ROT_A, 1 = ROT_B. |

NOTE about PS: FDK codes IID (loudness differences) and ICC (coherence). IPD/OPD
(phase differences) are NOT supported in the FDK encoder — the code literally
writes zeros (`ps_encode.cpp: "IPD OPD not supported right now"`). You cannot
expose them without writing interchannel phase analysis from scratch.

## 5. Noise substitution/shaping — TNS / PNS / afterburner

What, at medium bitrates, is replaced by noise or resynthesized.

| Switch | Values | Default | Description |
|---|---|---|---|
| `--tns-mask <n>` | -1 def (0xF), 0..15 | -1 | TNS enable mask (bitwise, per block type). |
| `--tns-order <n>` | -1 def, 1..12 | -1 | Max. TNS filter order (short blocks additionally capped to 5). |
| `--pns <n>` | -1 def, 0/1 | -1 | Perceptual Noise Substitution on/off. NOTE: FDK forces PNS=off when SBR or VBR is active. |
| `--pns-start <hz>` | -1 def, Hz | -1 | PNS start frequency. Lower = more of the spectrum replaced by noise. |
| `--force-pns` | flag | off | Bypass the low-bitrate gate for PNS. |
| `--pns-gain <x>` | >=0.0 | -1 (off) | Loudness of the fabricated PNS noise. `1.0` = unchanged (noise energy = original band). `>1.0` = louder-than-original noise fill, `<1.0` = quieter. Directly scales the coded noise energy — this is the "how loud is the noise" knob. Decimal input. |
| `--pns-tonality <x>` | >=0.0 | -1 (off) | Scales the PNS tonality detection threshold. `1.0` = default; higher = more (even less-noisy) bands qualify as PNS = WIDER noise substitution. |
| `--pns-refpower <x>` | >=0.0 | -1 (off) | Scales the PNS reference-power detection threshold. `1.0` = default. |
| `--pns-gapfill <x>` | >=0.0 | -1 (off) | Scales the PNS gap-fill threshold (fills PNS holes between two PNS bands). `1.0` = default. Advanced/subtle — rarely visible. |
| `--pns-min-width <n>` | -1 off, >=1 | -1 | Minimum SFB width for PNS. Effective above the built-in default (LC=16); e.g. 32/64 restricts PNS to wider bands. |
| `--afterburner <n>` | 0/1 (also stock `-a`) | 1 | Afterburner (more precise quantization). |

IMPORTANT about PNS at low bitrate: FDK has a tuning table (`levelTable`) that
COMPLETELY disables PNS below ~28 kbps (the bitrate row 0-27999 = all zeros for
every samplerate). That is why at 24 kbps `--pns`/`--pns-start` do NOTHING (the
audio sounds "like MP3/MDCT"), while at 64 kbps the difference is large.
`--force-pns` bypasses this gate (uses the first active row of the table), so PNS
also works at 24k. FDK limitation: PNS still requires TNS enabled and a non-VBR
mode — otherwise it is zeroed higher up in the chain (we can't do anything about
it without a deeper rebuild).

## 6. Masking / ATH

| Switch | Values | Default | Description |
|---|---|---|---|
| `--ath-scale <n>` | 1..~4096 (Q8) | 256 | Masking threshold scale in Q8 (256 = x1.0). `>256` raises the thresholds (more noise, fewer bits per band), `<256` lowers them (cleaner, more bits). Works in FDK's ld64 domain as an additive log2 offset. NOTE: this only touches the log-domain threshold copy and is partly undone downstream by the min-SNR / 29 dB clamps — for a stronger, more direct effect prefer `--minsnr-scale` below. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Scales the spreading of masking between bands. `<256` = less masking = more detail. Biggest effect where bits are limited (96-192k). |
| `--minsnr-scale <n>` | 1..no limit (Q8) | -1 (off) | MusePack-style: scales the REQUIRED per-band coding SNR (`sfbMinSnrLdData`, FDK's closest thing to TMN/NMT). `<256` = demand HIGHER SNR = more detail/bits; `>256` = coarser. More effective than `--ath-scale` because min-SNR is what the avoid-holes logic clamps thresholds back to. 256=off. |
| `--minsnr-clamp-hi <n>` | 1..no limit (Q8) | -1 (off) | Scales FDK's MAX_SNR ceiling (~−1 dB). `>256` lets bands demand more than the stock cap. 256=off. |
| `--minsnr-clamp-lo <n>` | 1..no limit (Q8) | -1 (off) | Scales FDK's MIN_SNR floor (~−25 dB). 256=off. |
| `--reduce-clamp <0\|1>` | 0, 1 | 1 (on) | `0` drops the "29 dB Ratio" threshold-reduction ceiling in the CBR quantizer, letting thresholds be pushed deeper (more bits into demanding bands). Pairs with `--minsnr-scale` for extreme detail. CBR only (VBR uses a different path). |

### What really helps at low and medium bitrate (10-144 kbps)

A common question: can you still squeeze something out of coding
accuracy/efficiency (Huffman, quantization iterations, etc.)? The honest answer
after reviewing the FDK code:

- Huffman coding (section merging, codebook selection in `dyn_bits.cpp`) is
  already optimal (greedy section merging giving min. bits). There is no
  meaningful knob there — and exposing it would only worsen the result.
- The quantization iteration loop (`maxIterations`) is a RESCUE mechanism for bit
  shortage; increasing it does nothing (details in the manual, chapter 9a).
- Internal thresholds (minSnr adaptation, `bits2PeFactor`) are fixed-point
  arithmetic with hard ranges — moving them risks instability, not improvement.

The REAL set of quality levers for 10-144 kbps is ALREADY exposed:
- `--ath-scale <256` — globally lower the masking threshold (more detail for bits).
- `--spread-mask <256` — less inter-band masking (more bands coded).
- `--ms-precision >256` — shallower holes in the MS bands.
- `--is-aggression` — control intensity stereo (crucial at low bitrate).
- `--force-pns` + `--pns-start` — noise control at very low bitrate.
- under HE-AAC: `--sbr-invf`, `--sbr-noise-floor-offset`, `--speech` (speech).

This is not missing functionality — these are the same levers that professional
tuning uses, just manually. Start with `--ath-scale` and `--spread-mask`, one at
a time.

## 7. Bias of short/long block switching (any profile)

| Switch | Values | Default | Description |
|---|---|---|---|
| `--block-bias <n>` | 0..255 | -1 (off) | Shifts the short/long decision threshold. 128 = encoder default (no change), >128 favors short blocks (more "transient"), <128 favors long ones, 0 = practically only long. |

IMPORTANT: `--block-bias` always produces a standard-compliant stream (it shifts
the attack-detection threshold, it doesn't forcibly impose a block type). It
replaced the old `--allshort`/`--alllong`, which created an ILLEGAL stream (hard
forcing of the short window without recomputing SFB/grouping -> decoders rejected
it, Winamp "skipped like on a scratched disc"). If you want maximum long blocks:
`--block-bias 0`; maximum short: `--block-bias 255`.

## 8. MS decision bias (honestly: a tool with a WEAK effect)

| Switch | Values | Default | Description |
|---|---|---|---|
| `--ms-bias <n>` | 0..255 (Q8) | -1 (off) | Shifts the L/R vs MS decision threshold. Q8, 128 = +0.5 in FDK's ld64 units. >0 = MS more eager. Reacts already from ~32 (after scale recalibration). |

HONESTLY about `--ms-bias` — this is the weakest tool in the whole set, and now
we know why "it does little". MS (mid/side) is a LOSSLESS transform: mid=L+R,
side=L-R reconstructs exactly back to L/R. Enabling/disabling MS on a given band
does NOT change the sound — it only changes HOW MANY BITS the encoding takes. The
encoder chooses a near-optimal decision per band anyway; `--ms-bias` only shifts a
few BORDERLINE bands. Measurement (L/R correlation after decoding, ADTS size):
effect on the order of <0.1% of the size and correlation changes in the 4th
decimal place.

Technical note: in the previous version the bias scale was ~256x too weak
(multiplier <<15 instead of <<23) — hence "128 did nothing, only 2048 moved
something". Now 128 = a real +0.5 ld64 as in the description, so it reacts from
~32. But even a correctly scaled bias inherently has a small impact (see above).

Want to REALLY control MS? Use the hard switches, not the bias:
- `--msmask 0` — DISABLE MS entirely (pure, independent L/R). This is the right
  choice for center-cancel / vocal removal (zero channel mixing by the coder).
- `--msmask 1` — force MS on all bands (max bit economy).
- `--msbands` / `--msbands-lo/-hi` — restrict MS to selected bands.
Measurement: msmask 0 vs 1 gives ~900 B difference on a 2s sample; ms-bias only ~2 B.

## 9. Quasi-constrained VBR (CBR engine + wider breathing)

IMPORTANT: without these switches, CBR is 100% UNCHANGED (verified: bit-identical
to the original binary). You enable them deliberately.

How AAC breathes: even in CBR, frames borrow from the bit-reservoir, so one frame
can be ~122 kbps and the next ~141, as long as the average = target. These knobs
widen/narrow that breathing.

HARD CEILING for everything: an AAC frame holds MAX 6144 bits per channel
(=768 bytes/channel); stereo => 12288 bits/frame. At 44100 Hz one frame =
1024 samples = ~23.2 ms, so bits/frame = kbps * 23.22. (E.g. 128k stereo:
average ~2972 bits/frame; ceiling 12288.)

| Switch | Values | Default | Description |
|---|---|---|---|
| `--vbr-reservoir <bits>` | 0..(6144*channels - average) | -1 (off) | Bit-reservoir size. More = wider frame spread around the average. min 0 (stick to the target tightly). Auto-clamped to the ceiling - you can't overdo it. Safe start: 2-3x default. |
| `--peak-bitrate <bps>` | > target | -1 (off) | Allows short peaks up to this value while keeping the average. Set ABOVE `-b` (e.g. -b 128000 --peak-bitrate 160000). Below the target it is ignored. |
| `--max-bits-frame <bits>` | average..12288(st.) | -1 (off) | Hard ceiling of bits in ONE frame. Must be >= average and <= 6144*channels (otherwise clamped). Reasonable cap ~1.5x average (~4500 for 128k st.). Too low = starves loud frames (audible). |
| `--min-bits-frame <bits>` | 0..average | -1 (off) | Hard floor of bits/frame. A higher floor wastes bits on silence. Leave at 0 unless experimenting. |
| `--bitres-mode <n>` | 0/1/2 | -1 (def) | Reservoir mode: 0 full (like default), 1 reduced, 2 disabled (rigid, closest to hard per-frame CBR). |

HOW TO SET IT OPTIMALLY (for the less experienced - so you don't overdo it):
- Safe quasi-CVBR ~128k stereo: `-b 128000 --peak-bitrate 160000 --vbr-reservoir 6000`.
- Do NOT set `--max-bits-frame` BELOW the average or `--min-bits-frame` ABOVE the
  average - that fights the target and ruins quality.
- `--vbr-reservoir` is auto-clamped to the ceiling, so it's safe to experiment.
- Measured for real (4s variable signal, 128k stereo): CBR default spread 95-167
  kbps; with `--vbr-reservoir 8000 --peak-bitrate 192000` spread 36-158 kbps
  (breathes harder, average held); `--bitres-mode 2` spread 127.8-128.2
  (rigid). All fully decodable.
- Limitation: this is a CBR+reservoir engine, not true ABR like LAME. Swings are
  moderate (6144 bits/channel limit), but this is that "light flight" of AAC.

## 10. Audiophile / extreme (opt-in, beyond the typical range)

| Switch | Values | Default | Description |
|---|---|---|---|
| `--uncap-bandwidth` | flag | off | Remove the hard 20 kHz core cap. `--core-cutoff` can then reach all the way to Nyquist. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | IS aggression slider (see section 1). |
| `--force-pns` | flag | off | PNS below the ~28 kbps gate (see section 5). |
| `--unlock-bitrate` | flag | off | Remove the LOWER bitrate threshold. Allows extremely low: 8k HE-AAC stereo, 6k LC. IMPORTANT: in this mode `-b` is taken LITERALLY as bps (without the nu774 x1000 convention), so `-b 6000` = 6000 bps. The upper ceiling 6144*channels stays (hard AAC limit). Residual floor ~10 kbps = the minimum of AAC headers. |
| `--speech` | flag | off | SBR tuning mode for human SPEECH (different inverse-filtering thresholds, noise level, no parametric coding). Applies to HE-AAC (SBR); LC has no separate speech mode. For pure speech at low bitrate. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Scales the spreading of masking between bands. `<256` = LESS masking = more detail (equivalent to loosening tone-masks-noise). Biggest effect where bits are limited (96-192k). 256=no change. Combine with `--ath-scale <256`. |

BANDWIDTH ABOVE 20 kHz (audiophile): FDK has a hardcoded cap `min(20000, sr/2)` on
the core bandwidth — even with a 96 kHz input and high bitrate, nothing above
20 kHz is actually coded (your suspicion was correct). `--uncap-bandwidth` lifts
this cap; then `--core-cutoff` controls the bandwidth all the way up to sr/2.

Measured (96 kHz input, LC 400k, broadband noise):
- without uncap, `--core-cutoff 40000`: verbose 20000 Hz, energy >20 kHz ~= 0%.
- `--core-cutoff 40000 --uncap-bandwidth`: verbose 40000 Hz, energy 20-24 kHz
  ~10%, 24-32 kHz ~20%, 32-44 kHz ~20% — full band up to 40 kHz actually coded.

Audiophile preset (full bandwidth + manual masking, for 190+ kbps):
```
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 \
    --ath-scale 200 -o out.m4a in96k.wav
```
`--ath-scale <256` lowers the masking thresholds (cleaner, more bits for detail) —
sensible when you have a large bitrate margin. Note: bandwidth >20 kHz and extreme
settings may be rejected by some decoders (outside the typical spec) — a conscious
opt-in.

---

## 11. MP4/M4A container — which boxes are necessary, and what can be cut

An .m4a file is a set of nested "boxes" (atoms). Verified what is what:

MANDATORY (without them the file is UNPLAYABLE — there are no switches for them):
`ftyp`, `mdat` (raw audio data), and the skeleton `moov` → `mvhd` + `trak` →
`tkhd` → `mdia` (`mdhd`/`hdlr`/`minf` → `smhd`/`dinf`/`stbl` with the tables
`stsd`/`stts`/`stsc`/`stsz`/`stco`). This is the minimal file map required by the
ISO standard — every decoder needs it to even find and play the sound. Cutting
them makes no sense (result: a corrupt file).

OPTIONAL (can be disabled) — the entire block `udta` → `meta` → `ilst`, i.e.:
- the encoder identification tag (`©too`, now "PompAAC based on …"),
- `iTunSMPB` — encoder delay data for seamless (gapless) playback,
- all tags (title, artist, album, etc.).

| Switch | Works on | Description |
|---|---|---|
| `--no-tool-tag` | .m4a | Do not write the encoder identification tag (`©too`). The rest of the tags and gapless stay. |
| `--minimal-moov` | .m4a | The smallest legal .m4a: omits the ENTIRE `udta`/metadata block (encoder tag + gapless iTunSMPB + all tags). The playback skeleton stays intact. |

How much it saves (2 s, 128k stereo, measured): default ~34381 B →
`--no-tool-tag` ~34297 B (−84) → `--minimal-moov` ~34048 B (−333). These are small
numbers — the MP4 overhead is mostly the mandatory skeleton, which can't be
removed. Want truly zero container overhead? Use raw ADTS: `-f 2 -o out.aac`
(a stream without any boxes, but also without tags and gapless).

NOTE on gapless: `--minimal-moov` removes iTunSMPB, so when joining tracks
micro-gaps may appear (the encoder delay won't be signaled). For ordinary
listening this doesn't matter; for "gapless" albums leave the defaults.

---

## 12. Legend for reading `--verbose`

`--verbose` prints RAW values (without hints in parentheses, so as not to clutter).
Below is what the non-obvious ones mean:

| Field | How to read it |
|---|---|
| `AOT (profile)` | 2=AAC-LC, 5=HE-AAC, 29=HE-AAC v2, 23=AAC-LD, 39=AAC-ELD. |
| `bitrate-mode` | 0=CBR, 1..5=VBR (higher=better). |
| `channel-mode` | 1=mono, 2=stereo (for HE-AAC v2 the core is mono, stereo is done by PS). |
| `core bandwidth` | Upper frequency of the AAC core, **anchored to the nearest SFB boundary** (the real cutoff, which can differ from the `-w`/`--core-cutoff` value you typed, e.g. `-w 17300` → `17915 Hz (SFB-anchored)`). In parentheses the SOURCE: `from -w`, `from --core-cutoff`, or `auto`. Under SBR this is only the core — SBR plays higher. |
| `final BW (AAC+SBR)` | Shown only when SBR is active: approximate UPPER frequency of the whole signal (core + SBR), computed from the `sbr stop freq index`. This is the equivalent of `core bandwidth`, but for the full HE-AAC band. |
| `signaling-mode` | Way of signaling SBR/PS: 0=implicit, 1=explicit backward-compat, 2=explicit hierarchical, auto=the library chooses. |
| `SBR mode` | Internal SBR mode (-1/0 when unused). |
| `sbr-ratio` | 1=downsampled (single-rate), 2=dual-rate (core at half the frequency). |
| `sbr amp res` | 0=1.5 dB, 1=3.0 dB (envelope amplitude resolution). |
| `granule/frame length` | Frame length in samples (1024 for LC, 512/480 for LD/ELD). |
| `codec delay` | Codec delay in samples/channel (total and the core alone). For gapless. |
| `IS corr threshold` / `IS L/R ratio` | Thresholds on the Q8 scale: 256 = 1.0. A lower correlation threshold = intensity stereo MORE eager (counterintuitively). |
| `IS min contiguous SFBs` | How many adjacent bands must "agree" before IS turns on. |
| `TNS mask` | Bitmask 0x0..0xF of which TNS filters are active. |
| `MS/IS bands: auto up to N` | Upper band index up to which the tool may operate. |
| `franken overrides applied` | List of switches that in THIS run deviate from pure FDK (or "none"). |

Q8 values (like `IS corr threshold`, `--ms-precision`, `--ms-bias`,
`--ath-scale`, `--spread-mask`) are fixed-point numbers where 256 = 1.0;
e.g. 243 means 243/256 ≈ 0.95.

---

## 13. Reference tables (from the FDK tuning tables)

Three orientation tables, so you can consciously choose `--msbands`, `--sbr-start/stop`
and `-w`. The values are computed from the tables in the FDK code; they are
APPROXIMATE (the SFB grid is stepped), but they show the right order of magnitude.

### Table 1 — approximate upper band frequency (SFB) [Hz]

Bands numbered from the bottom (0=bass). Shown every 4th band; the last row = the
number of bands and the Nyquist frequency. Useful for `--msbands`/`--isbands`/`--msbands-lo/-hi`.

| SFB | 16 kHz | 22.05 kHz | 32 kHz | 44.1 kHz | 48 kHz | 96 kHz |
|----:|-------:|----------:|-------:|---------:|-------:|-------:|
| 0   | 62   | 43   | 62   | 86    | 94    | 188   |
| 4   | 312  | 215  | 312  | 431   | 469   | 938   |
| 8   | 562  | 388  | 562  | 775   | 844   | 1688  |
| 12  | 875  | 646  | 1000 | 1378  | 1500  | 2438  |
| 16  | 1250 | 991  | 1500 | 2067  | 2250  | 3750  |
| 20  | 1656 | 1335 | 2250 | 3101  | 3375  | 5625  |
| 24  | 2188 | 1852 | 3375 | 4651  | 5062  | 8062  |
| 28  | 2875 | 2584 | 5000 | 6891  | 7500  | 12938 |
| 32  | 3844 | 3618 | 7000 | 9647  | 10500 | 24000 |
| 36  | 5188 | 5039 | 9000 | 12403 | 13500 | 36000 |
| 40  | 7000 | 7020 | 11000| 15159 | 16500 | 48000 |
| 44  | —    | 9647 | 13000| 17916 | 19500 | —     |
| 48  | —    | —    | 15000| 22050 | 24000 | —     |
| **number of bands / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### Table 2 — SBR: start freq index → approximate crossover frequency [Hz]

This is the frequency from which SBR takes over the band above the AAC core
(`--sbr-start`, index 0..15; lower = SBR starts lower = narrower core). "core" is
the core frequency; with dual-rate the output is twice as high (e.g. core 24k →
output 48k).

| start index | core 16 kHz | core 24 kHz | core 32 kHz | core 44.1 kHz | core 48 kHz |
|----:|----:|----:|----:|----:|----:|
| 0 | 2750 | 2250 | 2500 | 1378 | 1500 |
| 1 | 3000 | 2625 | 3000 | 2067 | 2250 |
| 2 | 3250 | 3000 | 3500 | 2756 | 3000 |
| 3 | 3500 | 3375 | 4000 | 3445 | 3750 |
| 4 | 3750 | 3750 | 4500 | 4134 | 4500 |
| 5 | 4000 | 4125 | 5000 | 4823 | 5250 |
| 6 | 4250 | 4500 | 5500 | 5512 | 6000 |
| 7 | 4500 | 4875 | 6000 | 6202 | 6750 |
| 8 | 4750 | 5250 | 6500 | 6891 | 7500 |

Stop freq (`--sbr-stop`, 0..13) works analogously on the upper SBR boundary —
a higher index = SBR reaches higher (closer to the output Nyquist). By default the
library chooses both according to bitrate.

### Table 3 — AAC-LC: approximate cutoff (`-w`/auto) by bitrate per channel [Hz]

When you don't provide `-w`, FDK picks the bandwidth from this table according to
bitrate PER CHANNEL (stereo 128k = 64k/channel). The values are interpolated; the
mono and stereo columns differ. Helps assess whether `-w` makes sense (providing a
higher value than auto has an effect only if there are spare bits).

| bitrate/channel | bandwidth (mono) | bandwidth (stereo+) |
|----:|----:|----:|
| 0–12 kbps  | 3700  | 5000  |
| 20 kbps    | 6900  | 9640  |
| 28 kbps    | 9600  | 13050 |
| 40 kbps    | 12060 | 14260 |
| 56 kbps    | 13950 | 15500 |
| 72 kbps    | 14200 | 16120 |
| ≥96 kbps   | 17000 | 17000 |

Note: this table is SHARED for 32/44.1/48 kHz and higher — FDK indexes it by
bitrate per channel, not samplerate (samplerate only affects the upper limit =
Nyquist). For LC without SBR the real ceiling is ~17 kHz with auto; higher only
via `-w` (with spare bits) or `--uncap-bandwidth` at sr≥96k.

---

## Examples

```
# Starting point — what the encoder set:
fdkaac-franken-x64.exe -p 29 -b 48000 --verbose -o out.m4a in.wav

# Quasi-constrained VBR ~128k (safe preset):
fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

# Aggressive intensity stereo at 64k:
fdkaac-franken-x64.exe -p 2 -b 64000 --is 1 --is-aggression 70 -o out.m4a in.wav

# Force PNS at 24k (otherwise the FDK gate disables it):
fdkaac-franken-x64.exe -p 2 -b 24000 --pns 1 --force-pns -o out.m4a in.wav

# Audiophile full 40 kHz band from a 96 kHz input:
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

# MS on the 5 highest bands + shallower holes (not the lowest):
fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 --ms-precision 448 -o out.m4a in.wav

# Extremely low HE-AAC v2: 8000 bps stereo 48 kHz:
fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

# SBR: full stereo separation LR + forced inverse filtering:
fdkaac-franken-x64.exe -p 5 -b 96000 --sbr-stereo-mode 1 --sbr-invf 2 -o out.m4a in.wav

# Your case: 7.5 kHz of core under HE-AAC v2 48k:
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav

# Completely independent stereo (no MS/IS) on LC 128k:
fdkaac-franken-x64.exe -p 2 -b 128000 --msmask 0 --is 0 -o out.m4a in.wav

# MS only on the 6 lower bands:
fdkaac-franken-x64.exe -p 2 -b 96000 --msmask 1 --msbands 6 -o out.m4a in.wav

# Only short blocks + limited TNS:
# Only long blocks (bias) + limited TNS:
fdkaac-franken-x64.exe -p 2 -b 96000 --block-bias 0 --tns-order 2 -o out.m4a in.wav

# More aggressive masking (thresholds x2) + earlier PNS:
fdkaac-franken-x64.exe -p 2 -b 80000 --ath-scale 512 --pns 1 --pns-start 4000 -o out.m4a in.wav

# Denser SBR noise description + more precise amplitude:
fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-noise-bands 5 --sbr-amp-res 0 -o out.m4a in.wav

# HE-AAC v2 with PS disabled (flattened stereo):
fdkaac-franken-x64.exe -p 29 -b 32000 --ps 0 -o out.m4a in.wav
```

---

## Default quality vs the original binary (fdkaac2.exe)

Verification (2026-07-21) whether franken's default settings = the original
supplied binary (dBpoweramp R17, the same libfdk-aac version 4.0.1 / package 2.0.3):
- AAC-LC: **bit-identical** (the same md5) — zero regressions.
- HE-AAC v1/v2: the bytes differ, BUT: the HE-AAC v2 spectrum is identical to
  0.000 dB, and the HE-AAC v1 error relative to the original is practically the
  same as in the original (7.05e11 vs 7.05e11). The byte difference stems from a
  different compiler (mingw/gcc vs MSVC) and fixed-point rounding, NOT from worse
  quality. The audible SBR "otherness" is a different, equally correct
  implementation, not a quality loss.

---

## Tests (make check)

Upstream fdk-aac/nu774 have no unit tests (`make check` in them = a no-op). This
project has its OWN functional test suite in `tests/check.sh`, run with:

```
make check
```

It checks (on the x64 + x86 binaries if present): completeness of the switches in
--help, decodability of the stream for each switch (ffmpeg, 0 errors), real
operation of quasi-CVBR (CVBR breathes wider than the rigid bitres-mode2), verbose
without -1 values, and NO REGRESSION (default CBR without franken flags = ADTS
bit-identical to the original fdkaac2.exe). Requires ffmpeg + python3 (present in
WSL). Exit 0 = OK, 1 = errors, 77 = missing dependencies. Last result: 27/27 PASS.

---

The sources with the applied patches live in `src-fdk-aac/` and `src-fdkaac/`.
All changes in libfdk are wired through a single module
`libAACenc/src/franken.{h,cpp}` (the global `g_franken` block, sentinels = FDK
defaults), and the new `AACENC_PARAM`s (range `0xF0xx`) are exposed in
`aacenc_lib.h`.

```bash
sudo apt-get install -y mingw-w64          # + autotools (autoconf/automake/libtool)

# --- libfdk-aac (x64) ---
cd src-fdk-aac && autoreconf -i
./configure --host=x86_64-w64-mingw32 --prefix=$PWD/../inst-x64 \
    --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2
make -j && make install

# --- frontend (x64) ---
cd ../src-fdkaac && autoreconf -i
PKG_CONFIG_PATH=$PWD/../inst-x64/lib/pkgconfig ./configure \
    --host=x86_64-w64-mingw32 CFLAGS="-O2 -I$PWD/../inst-x64/include" \
    LDFLAGS="-static -static-libgcc -L$PWD/../inst-x64/lib"
make -j     # -> fdkaac.exe

# x86: the same with --host=i686-w64-mingw32 and a separate inst-x86 prefix.
```

## List of changed FDK files (mapped to points)
- `libAACenc/src/franken.{h,cpp}` — new control module.
- `libAACenc/include/aacenc_lib.h` — new `AACENC_PARAM` 0xF0xx.
- `libAACenc/src/aacenc_lib.cpp` — SetParam dispatch + override of useMS/IS/PNS/
  afterburner/cutoff + cutoff guard under SBR (after `sbrEncoder_Init`) + read-only
  GetParam mirrors for verbose (useTns/Pns/IS/MS, effective SBR).
- `libAACenc/src/ms_stereo.cpp` — per-band MS control IN THE DECISION LOOP (mask
  + L/R->M/S butterfly synchronized; fixed the "left=center" artifact) + MS bias
  + MS band range (--msbands-lo/-hi) + MS precision (--ms-precision, ld64 threshold).
- `libAACenc/src/intensity.cpp` — IS band cap (consistent with the mask) + IS
  threshold bias (initIsParams: min_is_sfbs, corr_thresh, left_right_ratio) + --is-aggression.
- `libAACenc/src/psy_configuration.cpp` — read-back of the SFB count + removal of the IS gate.
- `libAACenc/src/bandwidth.cpp` — --uncap-bandwidth (removing the 20kHz cap).
- `libAACenc/src/pnsparam.cpp` — PNS start override + --force-pns (bypass the table gate).
- `libAACenc/src/aacenc.cpp` — TNS mask override + quasi-CVBR + --unlock-bitrate
  (removal of the lower bitrate floor in FDKaacEnc_LimitBitrate).
- `libSBRenc/src/sbr_encoder.cpp` — SBR density override + --sbr-num-env (static
  framing) + --sbr-freqres-fixfix + --sbr-stereo-mode + --sbr-noise-floor-offset.
- `libSBRenc/src/invf_est.cpp` — --sbr-invf (forced inverse filtering level).
- `libSBRenc/src/ps_encode.cpp` — PS IID override + --ps-icc/--ps-icc-mode.
- `libAACenc/src/aacenc_tns.cpp` — TNS order cap.
- `libAACenc/src/pnsparam.cpp` — PNS start frequency override.
- `libSBRenc/src/sbr_encoder.cpp` — SBR density/precision override + writing the
  effective SBR values to g_franken (for verbose).
- `libSBRenc/src/ps_encode.cpp` — PS override (forcing IID + quantization mode).
- `libAACenc/src/main.c`, `aacenc.c`, `aacenc.h` (frontend) — CLI switches,
  parsing, passing to SetParam, `--verbose` dump, help.

---

# 🇵🇱 Wersja polska (Polish version below)

---

# Franken FDK AAC — laboratoryjny/"geekowski" enkoder AAC (FDK)

Zbudowany na bazie **libfdk-aac 2.0.3** (mstorsjo/fdk-aac) + frontendu
**nu774/fdkaac 1.0.2**, z doklejonymi przełącznikami CLI, które odslaniaja
normalnie zahardkodowane, wewnętrzne decyzję enkodera FDK. Sluzy do
ekstremalnego debugowania i eksperymentów z AAC/HE-AAC/HE-AAC v2.

Binarki (statyczne, bez zewnętrznych DLL):
- `fdkaac-franken-x64.exe` — Windows 64-bit (PE32+)
- `fdkaac-franken-x86.exe` — Windows 32-bit (PE32)

## Pobieranie

Gotowe binarki Windows są publikowane jako GitHub Releases — budowane i
hostowane przez GitHub (do pobrania bez logowania):

**→ https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Pobierz `franken-fdk-aac-x64-vX.Y.Z.zip` (64-bit) albo wersję `x86`; każdy zip
zawiera `.exe` oraz komplet dokumentacji. W repozytorium nie ma żadnych binarek
— jak zbudować samodzielnie, patrz sekcja „Budowanie" na dole.


Wszystkie oryginalne opcje frontendu nu774 (`-p/--profile`, `-b/--bitrate`,
`-m/--bitrate-mode`, `-w/--bandwidth`, `-a/--afterburner`, `-s/--sbr-ratio`,
`-f/--transport-format`, tagowanie itd.) działają jak wczesniej. Poniżej tylko
NOWE przełączniki. Zobacz tez `fdkaac-franken-x64.exe --help`.

UWAGA: to jest narzędzie "wiem-co-robię". Większość tych parametrów celowo
pozwala wyjść poza to, co robi automatyka FDK — można nimi świadomie zepsuć
obraz stereo, pasmo albo jakość. Sentinel `-1` (lub `0` dla `--core-cutoff`)
= "zostaw domyślne zachowanie FDK".

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski.
Zbudowane na oprogramowaniu open source: libfdk-aac (Fraunhofer IIS) oraz
frontend nu774/fdkaac.

---

## Spis grup opcji (tak samo pogrupowane w `--help`)

Opcje są uporządkowane tematycznie, z grubsza od najlatwiejszych do najbardziej
geekowskich. Ta sama kolejność obowiazuje w `--help` (grupy A–E) i w sekcjach
poniżej:

- **A. Zacznij tutaj (konsumenckie):** `--verbose`, `--is-aggression`, `--speech`,
  `--uncap-bandwidth`, `--unlock-bitrate` — sekcje 0, 1, 2, 10.
- **B. Stereo:** MS (`--msmask`, `--msbands`, `--msbands-lo/-hi`, `--side-bias`,
  `--side-knee`, `--mask-slope`), IS (`--is`, `--isbands`, `--is-*`), PS (`--ps`, `--ps-iid-quant`,
  `--ps-icc`, `--ps-icc-mode`) — sekcje 1, 4, 8.
- **C. Pasmo i SBR:** `--core-cutoff`, `--sbr-*` — sekcje 2, 3.
- **D. Maskowanie / szum / detal:** `--ath-scale`, `--spread-mask`, `--tns-*`,
  `--pns`, `--pns-start`, `--force-pns` — sekcje 5, 6.
- **E. Bloki i bitrate:** `--block-bias`, `--vbr-reservoir`, `--peak-bitrate`,
  `--max-bits-frame`, `--min-bits-frame`, `--bitres-mode` — sekcje 7, 9.

Wskazowka: `--verbose` na koncu wypisuje sekcje "franken overrides applied" —
dokładnie te przełączniki, które w danym uruchomieniu odbiegają od czystego FDK.

---

## 0. Diagnostyka

### `--verbose`
Przed enkodowaniem wypluwa na stderr REALNE, wybrane przez enkoder parametry
(nie tylko Twoje nadpisania): AOT, bitrate/tryb, samplerate, channel-mode,
EFEKTYWNY cutoff rdzenia w Hz, afterburner, transport, signaling, oraz stan
narzędzi kodujacych wybrany przez enkoder (TNS on/off, PNS on/off, Intensity
stereo on/off, MS stereo on/off). Gdy SBR aktywny: sbr-ratio + efektywne
start/stop freq index, freq scale, noise bands, amp res. Na koncu lista Twoich
override'ow (-1/0 = nie ustawione, zostawione enkoderowi). Idealne, by poznać
punkt wyjścia (np. domyślny cutoff HE-AAC v2 48k = 8613 Hz).

---

## 1. Joint stereo — MS / IS / niezależne stereo

FDK domyślnie sam decyduje per-pasmo o MS (mid/side) i IS (intensity). Tu można
te decyzję nadpisac i wymusic ekstremalne konfiguracje.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--msmask <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś MS: `0` = wszystkie pasma L/R (kompletnie niezależne stereo), `1` = MS na wszystkich pasmach. |
| `--msbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalny numer SFB, który może użyć MS (bierze N NAJNIŻSZYCH pasm). Powyżej — MS wyłączone. |
| `--msbands-lo <n>` | -1 off, 0..N | -1 | POCZATEK (najniższy numer pasma SFB) zakresu, w którym dozwolone jest MS. |
| `--msbands-hi <n>` | -1 off, 0..N | -1 | KONIEC (najwyższy numer pasma SFB) zakresu MS. Używane razem z `--msbands-lo` jako para OD–DO. Pasma poza tym zakresem ida czystym L/R. |
| `--ms-precision <n>` | 256..bez limitu (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny, użyj `--side-bias`.)* Skaluje precyzję pasm MS globalnie (mid i side razem), w stylu LAME `-q`. 256=bez zmian, 384~1.5x, 512~2x. W praktyce jego zasięg jest ograniczony: powyżej ~600-800 prog uderza w twarda podłogę FDK, a przy CBR bity są tylko PRZESUWANE między pasmami, więc brzmienie przestaje sie zmieniac. Do strojenia stereo zastąpiony przez `--side-bias`/`--side-knee`, które działają per-kanał dokładnie tam, gdzie trzeba. |
| `--mid-bias <n>` | 256..bez limitu (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny.)* `>256` PODNOSI prog kanału mid (L+R) po motylku MS, żeby uwolnić bity z mid dla side. Czystszym, lepiej zmierzonym sposobem przesunięcia balansu mid↔side jest `--side-bias` (który sięga po ten sam budzet od strony side). Zostawiony dla kompletności. 256=off. |
| `--side-bias <dB>` | -24.0 .. +50.0 | 0 (off) | **Główne pokrętło jakości stereo.** Przesuwa prog maskowania kanału SIDE (L−R) na pasmach kodowanych w MS, dokładnie w miejscu, gdzie FDK decyduje, czy pasmo skali (SFB) jest kodowane czy zerowane (`energia > prog` w `sf_estim.cpp`). Znak = EFEKT: **`+` kieruje WIĘCEJ bitow do kanału side** (niższy prog → mniej pasm side zerowanych, przetrwałe kwantyzowane dokładniej → czystsza szerokość stereo, ogony pogłosu, ambience), kosztem kanału mid; **`−` celowo DEGRADUJE side** (podnosi prog → pasma side wypadają → węższy, bardziej mono obraz). To dokładnie ta sama zależność energia-vs-prog, której LAME używa do alokacji bitow, a MusePack steruje przez `--ms` — nic egzotycznego. Ponieważ to tradeoff przy stałym budzecie, przy niskim bitrate kanał mid słyszalnie oddaje bity; to oczekiwane, nie błąd. Rozsądny zakres **+3 .. +9** dla „szerzej", ujemne tylko do skrajnego/artystycznego niszczenia przy niskim bitrate. 0 = off (bit-identyczny ze stockiem). |
| `--side-knee <dB>` | -24.0 .. +50.0 | 0 (off) | Kształtuje JAK OSTRO pasmo side przełącza się między „kodowane" a „wyzerowane" na progu. Stock FDK to twardy klif: w chwili gdy `energia ≤ prog`, całe pasmo spada do zera. **`+` = MIĘKKIE kolano**: pasma leżące do N dB *poniżej* progu są nadal zachowane (kodowane na najzgrubszym scalefactorze) zamiast zerowane, więc side gaśnie stopniowo zamiast wyłączać się — łagodniejsze wybrzmienie pogłosu/powietrza. **`−` = TWARDE kolano**: pasma, które ledwo przekraczają prog (do N dB *nad* nim), są mimo to zerowane, odcinając side wcześniej — chudziej, agresywniej. Ortogonalny do `--side-bias` i łączy się z nim. Rozsądny zakres **+3 .. +6**. 0 = off. |
| `--mask-slope <dB>` | -24.0 .. +50.0 | 0 (off) | Globalne (mid **i** side) strojenie **Masking-Slope-Adaptation** FDK — heurystyki NIE-maskującej (`adj_thr.cpp`), która rozluźnia wymagany SNR dla pasm SFB o energii dużo poniżej średniej ramki (stock: ponad ~10 dB poniżej), czyli celowo głodzi bardzo ciche pasma, żeby oszczędzić bity. To pokrętło przesuwa prog „jak daleko poniżej średniej, zanim przestanę się przejmować". **`+` podnosi go → mniej cichych pasm głodzonych → więcej detalu w cichych fragmentach, ogonach pogłosu, wybrzmieniach** (kosztuje bity); **`−` obniża go → ciche pasma głodzone mocniej → chudziej, bardziej pusto, więcej bitow na głośne rzeczy**. Ta sama rodzina co `--side-bias`, ale stosowana do obu kanałów i zakotwiczona na energii-vs-średnia zamiast progu MS. Subtelny na gęstym materiale (rusza tylko najcichsze pasma); najbardziej słyszalny na rzadkiej/pogłosowej treści. Rozsądny zakres **±6 .. ±12**. 0 = off. |
| `--is <n>` | -1 auto, 0 off, 1 on | -1 | Intensity stereo globalnie wł/wył. |
| `--isbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalna liczba SFB, które mogą użyć intensity. Powyżej — kodowane normalnie. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | KONSUMENCKI suwak: jak bardzo enkoder ma isc w intensity stereo. Zaczynaj tu, zaawansowane `--is-*` zostaw. |
| `--is-min-sfbs <n>` | -1 def(6), 0..N | -1 | (zaawansowane) Min. liczba ciaglych SFB, zanim IS sie włączy. |
| `--is-corr-thresh <n>` | -1 def(243), Q8 | -1 | (zaawansowane) Prog korelacji L/R dla IS w Q8 (256=1.0). |
| `--is-lr-ratio <n>` | -1 def(179), Q8 | -1 | (zaawansowane) Prog balansu energii L/R dla IS w Q8 (256=1.0). |
| `--is-lo <sfb>` | -1 off, 0..N | -1 | Pozwól na intensity stereo TYLKO od tego SFB w górę. Pasma poniżej zostają czyste L/R. Tylko OGRANICZA gdzie FDK może użyć IS — nigdy go nie wymusza. |
| `--is-hi <sfb>` | -1 off, 0..N | -1 | Pozwól na IS tylko do tego SFB (włącznie). Używaj z `--is-lo` jako zakres. WSKAZOWKA: IS zwykle ląduje na NISKICH pasmach przy niskim bitrate, więc skanuj małe wartości, by zobaczyć efekt. |
| `--is-force-lo <sfb>` | -1 off, 0..N | -1 | WYMUSZA intensity stereo od tego SFB, omijając bramki korelacji / min-sfbs / głośności. Tryb laboratoryjny: może celowo rozbić obraz stereo (IS jest stratne i kierunkowe — prawy kanał zostaje wyzerowany, zostaje tylko współczynnik panoramy). Strumien pozostaje legalny. |
| `--is-force-hi <sfb>` | -1 off, 0..N | -1 | Górny SFB wymuszonego zakresu IS (włącznie). |

### Intensity stereo w praktyce (jak tego używać, nie wzory)

Co to jest: intensity stereo (IS) w górnych pasmach porzuca osobne L/R i wysyla
JEDNA obwiednię energii + informację o kierunku (panoramie). Ucho słabo lokalizuje
wysokie tony, więc to oszczędza sporo bitow — ale za cena separacji stereo (szerokość
sceny w górze pasma sie zwęża). Płaci sie za to zwłaszcza na materiale z realna
różnica L/R w wysokich (blachy z jednej strony, efekty przestrzenne).

FDK domyślnie jest bardzo OSTROŻNY z IS (stąd Twoja obserwacja "ledwo słychać
różnice"). Powody są trzy i po to są te pokretla:

1. Bramka wpuszczenia IS: FDK w ogole rozważa IS tylko gdy `bitrate/pasmo < 5`.
   Przy wyższych bitrate'ach IS jest w ogole niedopuszczone. `--is-aggression >=1`
   zdejmuje te bramkę.
2. Prog korelacji (`--is-corr-thresh`, Q8, 256=1.0, default 243 ~= 0.95): oba
   kanały muszą byc do siebie podobne w danym pasmie w co najmniej ~95%, żeby IS
   sie włączył. To bardzo wysoko. Obniżasz -> IS lapie czesciej, nawet gdy kanały
   mniej podobne. Np. 180 (~0.70) = dużo agresywniej. Za nisko = słyszalne
   przekłamania kierunku.
3. Min. długość regionu (`--is-min-sfbs`, default 6): IS włącza sie dopiero na
   pasmie co najmniej 6 kolejnych SFB. Obniżasz do 1-2 -> IS lapie tez krótkie
   fragmenty.

Zależność między nimi: żeby dany SFB poszedł w IS, MUSZA byc spełnione WSZYSTKIE
naraz — bramka wpuszczenia ORAZ korelacja powyżej progu ORAZ region odpowiednio
długi ORAZ kierunek stabilny. Dlatego samo obniżenie jednego progu często nic nie
daje (inny nadal blokuje) — i dlatego zwykle nie widac różnicy manipulując tylko
korelacja. `--is-aggression` rusza WSZYSTKIE naraz, spójnie.

Jak ustawiac:
- Najprościej: `--is 1 --is-aggression 40` i słuchaj. Za malo IS -> podnos do 70,
  100. Za dużo (scena sie "skleja" w górze, artefakty kierunku) -> zejdź.
- 0 = domyślne FDK (praktycznie IS ledwo aktywne przy typowym bitrate).
- 100 = maksimum: bramka zdjeta, korelacja luzna (~0.475), region od 1 SFB,
  szeroka tolerancja kierunku. Duzo pasm w IS, mocno słyszalne, oszczędza bity.
- Ręczny tuning tylko gdy chcesz precyzji: ustaw `--is-aggression 0` i kreç
  `--is-corr-thresh` (glowny), potem `--is-min-sfbs`, na koncu `--is-lr-ratio`.
  Wartości --is-* NADPISUJA to co ustawil suwak agresywności.
- Diagnoza: `--verbose` pokazuje efektywne progi (IS corr threshold Q8, min SFBs),
  więc widzisz co realnie poszło do enkodera.

Bias MS/IS (punkt 2): powyższe `--is-*` sterują tym KIEDY enkoder wybiera
intensity stereo (progi decyzji z tabeli strojenia FDK), niezależnie od twardego
wł/wył. `--msbands` ogranicza MS do dolnych pasm (poprawnie — maska i motylek
L/R->M/S są zsynchronizowane, brak artefaktu "lewy=center, prawy=reszta").
- Kompletnie niezależne stereo: `--msmask 0 --is 0`.
- "Laboratoryjne" ograniczenie MS do dolnych pasm: np. `--msbands 6`.
- Wymuszony pelny MS: `--msmask 1`.
- IS chętniejsze: obniż `--is-corr-thresh` (np. 150) i/lub `--is-min-sfbs`.

### Zakres pasm MS: --msbands, --msbands-lo, --msbands-hi (WAZNE, często mylone)

Pasma widma są numerowane OD DOŁU: pasmo 0 = najniższe częstotliwości (basy),
im wyższy numer, tym wyżej w widmie. W typowym LC stereo jest ich okolo 49.

Są DWA niezależne sposoby ograniczenia, gdzie stosowane jest MS:

1. `--msbands <n>` — "dolne N pasm". MS dozwolone TYLKO w pasmach 0..(n-1),
   czyli od basu w górę do numeru n. To jest zawsze liczone OD DOŁU.
   Przykład: `--msbands 6` = MS tylko na 6 najniższych pasmach, reszta czyste L/R.

2. `--msbands-lo <lo>` + `--msbands-hi <hi>` — "zakres OD-DO". MS dozwolone TYLKO
   w pasmach o numerach od `lo` do `hi` włącznie. Poza tym zakresem czyste L/R.
   To para — podajesz oba. Pozwala umiescic MS GDZIEKOLWIEK, w tym na samej górze.

Konkretny przykład (zakladajac ~49 pasm w LC):
- Chcesz MS TYLKO na 5 najWYŻSZYCH pasmach (np. scalic szum w górze, a dół
  zostawić w pelnym niezaleznym stereo)? Najwyższe pasma to numery 44..48:
  `--msbands-lo 44 --msbands-hi 48`.
- Chcesz MS tylko w SRODKU pasma (np. 10..30)? `--msbands-lo 10 --msbands-hi 30`.
- Chcesz MS na 6 najNIŻSZYCH? Prościej `--msbands 6` (albo `--msbands-lo 0
  --msbands-hi 5` — to samo).

Zasada pamięciowa: `--msbands` = "od dołu do", `--msbands-lo/-hi` = "od..do".
Ile masz realnie pasm dla danego trybu/samplerate pokazuje `--verbose`
(pole "active SFBs").

## 2. Cutoff (odcięcie) rdzenia AAC gdy działa SBR

Standardowe `-w/--bandwidth` w FDK jest IGNOROWANE gdy aktywny jest SBR
(HE-AAC v1/v2) — bo `sbrEncoder_Init()` nadpisuje pasmo wartością z tabeli SBR.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--core-cutoff <hz>` | 0 = default, >0 = Hz | 0 | Wymusza pasmo rdzenia AAC W Hz nawet pod SBR. Odporny na nadpisanie przez SBR. |

Przykład (Twoj przypadek — 7.5 kHz rdzenia przy HE-AAC v2 48 kbps, gdzie tabela
daje mniej):
```
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
```
Zweryfikowane: `--core-cutoff 7500` -> efektywne pasmo 7500 Hz; stock `-w 7500`
pod SBR pozostaje 8613 Hz (ignorowane).

UWAGA: sam pilnujesz limitow rdzenia. Maks. to Nyquist rdzenia (`sr/2`), a przy
**dual-rate SBR docelowy samplerate jest dzielony przez 2** — miej to na
uwadze przy doborze wartości.

## 3. Gęstość / dokładność danych SBR

Nadpisuje ustawienia z tabeli tuningowej SBR (po jej załadowaniu).

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--sbr-start <n>` | -1 def, 0..15 | -1 | Indeks `bs_start_freq` (start pasma SBR). |
| `--sbr-stop <n>` | -1 def, 0..13 | -1 | Indeks `bs_stop_freq` (koniec pasma SBR). |
| `--sbr-freqscale <n>` | -1 def, 0..3 | -1 | Grupowanie częstotliwości (0 = liniowe, wyżej = drobniejsze log). |
| `--sbr-alterscale <n>` | -1 def, 0/1 | -1 | Alternatywna rozdzielczość skali. |
| `--sbr-noise-bands <n>` | -1 def, 1..5 | -1 | Liczba pasm szumu SBR (gęstość opisu szumu). |
| `--sbr-amp-res <n>` | -1 def, 0/1 | -1 | Rozdzielczość amplitudy obwiedni: 0 = 1.5 dB, 1 = 3.0 dB. |
| `--sbr-data-extra <n>` | -1 def, 0/1 | -1 | Zapis dodatkowych danych nagłówka SBR. |
| `--sbr-num-env <1\|2\|4>` | -1 off | -1 | Liczba obwiedni na ramke. WYMUSZA statyczna siatke czasowa (ignoruje detektor transientów). Więcej = lepsza rozdzielczość czasowa górnego pasma, gorzej na atakach. (8 przekracza standardowy grid — odrzucone.) |
| `--sbr-freqres-fixfix <0\|1>` | -1 off | -1 | Rozdzielczość częstotliwości obwiedni FIXFIX (0 low, 1 high). |
| `--sbr-stereo-mode <0..3>` | -1 off | -1 | Tryb stereo SBR: 0 mono, 1 LR (pelna separacja górnego pasma), 2 coupling (oszczędny, wspolna obwiednia + poziom), 3 switch-LRC (domyślnie koder wybiera per-ramke). Wymuś 1 dla max separacji, 2 dla oszczędności. |
| `--sbr-invf <0..3>` | -1 auto | -1 | Wymuś inverse filtering SBR: 0 off, 1 low, 2 mid, 3 high. Sterowane normalnie estymatorem tonalności. Wyżej = mocniejsze "wybielanie" tonalnego SBR (mniej metaliczności kosztem detalu). |
| `--sbr-noise-floor-offset <n>` | -128 off | -128 | Offset poziomu szumu SBR (mala l. calkowita). Wieksze = więcej szumu wypełniającego w rekonstrukcji SBR. |
| `--sbr-header-period <n>` | -1 off, >=1 | -1 | Liczba ramek między nagłówkami SBR = jak szybko górne pasmo SBR \"wchodzi\", gdy dekoder podłącza się do strumienia HE-AAC na żywo (Icecast/Shoutcast). KONFIGURACJA SBR jest w okresowym nagłówku, nie w każdej ramce; dekoder wpięty w środek gra sam rdzeń (przytłumiony) do nadejścia kolejnego nagłówka. `1` = nagłówek w każdej ramce → niemal natychmiastowy sync SBR (~23 ms); wyżej = dłuższy moment core-only. Domyślnie FDK ~10 ramek (~0.23 s HE dual-rate / ~0.46 s LC). FDK kapuje to do maks. raz na sekundę, więc bardzo duże wartości są przycinane (np. 40 → 21 ramek @44.1k). Efektywny okres w ms pokazuje `--verbose`. |

UWAGA: `--sbr-start`/`--sbr-stop` są walidowane PRZEZ FDK — niepoprawna
KOMBINACJA start/stop (zła liczba pasm master) da "encoder initialization
failed". To ograniczenie samego SBR, nie buga. Dobieraj pary (np. dla 64k
stereo działa start=5 stop=9, start=8 stop=14).

## 4. Parametric Stereo (HE-AAC v2)

PS opisuje stereo kilkoma parametrami (IID/ICC...). Tu można nimi sterować,
nawet kosztem obrazu stereo.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ps <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś wysylanie parametru IID PS. `0` = nigdy (spłaszcza obraz stereo), `1` = zawsze. Nadpisuje heurystykę różnicy głośności. |
| `--ps-iid-quant <n>` | -1 def, 0 coarse, 1 fine | -1 | Siatka kwantyzacji IID: gruba vs. dokładna. |
| `--ps-icc <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś ICC (Interchannel Coherence — podobieństwo/spójność kanałów) on/off. |
| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | Tryb rotacji ICC: 0 = ROT_A, 1 = ROT_B. |

UWAGA o PS: FDK koduje IID (różnice głośności) i ICC (koherencja). IPD/OPD
(różnice fazy) NIE są wspierane w enkoderze FDK — kod dosłownie wpisuje zera
(`ps_encode.cpp: "IPD OPD not supported right now"`). Nie da sie ich wystawić bez
napisania od zera analizy fazy międzykanałowej.

## 5. Substytucja/ksztaltowanie szumu — TNS / PNS / afterburner

To, co w średnich bitrate'ach jest zastępowane szumem lub resyntezowane.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--tns-mask <n>` | -1 def (0xF), 0..15 | -1 | Maska włączenia TNS (bitowa, per typ bloku). |
| `--tns-order <n>` | -1 def, 1..12 | -1 | Maks. rząd filtra TNS (bloki short dodatkowo kapowane do 5). |
| `--pns <n>` | -1 def, 0/1 | -1 | Perceptual Noise Substitution wł/wył. UWAGA: FDK wymusza PNS=off gdy aktywne SBR albo VBR. |
| `--pns-start <hz>` | -1 def, Hz | -1 | Częstotliwość startowa PNS. Niżej = więcej widma zastępowane szumem. |
| `--force-pns` | flaga | off | Obejdz bramkę niskiego bitrate dla PNS. |
| `--pns-gain <x>` | >=0.0 | -1 (off) | Głośność dorabianego szumu PNS. `1.0` = bez zmian (energia szumu = oryginalne pasmo). `>1.0` = szum głośniejszy niż oryginał, `<1.0` = cichszy. Wprost skaluje energię kodowanego szumu — to pokrętło „jak głośny szum". Wejście dziesiętne. |
| `--pns-tonality <x>` | >=0.0 | -1 (off) | Skaluje prog detekcji tonalności PNS. `1.0` = domyślnie; wyżej = więcej (nawet mniej-szumiacych) pasm kwalifikuje się do PNS = SZERSZY szum. Wejście dziesiętne. |
| `--pns-refpower <x>` | >=0.0 | -1 (off) | Skaluje prog mocy referencyjnej detekcji PNS. `1.0` = domyślnie. Wejście dziesiętne. |
| `--pns-gapfill <x>` | >=0.0 | -1 (off) | Skaluje prog wypełniania luk PNS (wypełnia dziury PNS między dwoma pasmami PNS). `1.0` = domyślnie. Zaawansowane/subtelne — rzadko widoczne. Wejście dziesiętne. |
| `--pns-min-width <n>` | -1 off, >=1 | -1 | Minimalna szerokość SFB dla PNS. Skuteczny powyżej wbudowanej domyślnej (LC=16); np. 32/64 ogranicza PNS do szerszych pasm. |

WAZNE o PNS przy niskim bitrate: FDK ma tabele tuningowa (`levelTable`), która
CALKOWICIE wyłącza PNS poniżej ~28 kbps (wiersz bitrate 0-27999 = same zera dla
kazdego samplerate). Dlatego przy 24 kbps `--pns`/`--pns-start` nie robia NIC (audio
brzmi "jak MP3/MDCT"), a przy 64 kbps różnica jest duza. `--force-pns` omija te
bramkę (używa pierwszego aktywnego wiersza tabeli), więc PNS działa tez przy 24k.
Ograniczenie FDK: PNS i tak wymaga włączonego TNS i trybu nie-VBR — inaczej jest
zerowane wyżej w łańcuchu (nic na to nie poradzimy bez głębszej przebudowy).

## 6. Maskowanie / ATH

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ath-scale <n>` | 1..~4096 (Q8) | 256 | Skala progu maskowania w Q8 (256 = x1.0). `>256` podnosi progi (więcej szumu, mniej bitow na pasmo), `<256` obniża (czysciej, więcej bitow). Działa w domenie ld64 FDK jako addytywny offset log2. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = mniej maskowania = więcej detalu. Największy efekt gdzie bity ograniczone (96-192k). |
| `--minsnr-scale <n>` | 1..bez limitu (Q8) | -1 (off) | Styl MusePack: skaluje WYMAGANY per-pasmo SNR kodowania (`sfbMinSnrLdData`, najbliższy FDK-owy odpowiednik TMN/NMT). `<256` = wymagaj WYŻSZEGO SNR = więcej detalu/bitow; `>256` = zgrubniej. Skuteczniejszy niż `--ath-scale`, bo to do min-SNR logika avoid-holes cofa progi. 256=off. |
| `--minsnr-clamp-hi <n>` | 1..bez limitu (Q8) | -1 (off) | Skaluje sufit MAX_SNR FDK (~−1 dB). `>256` pozwala pasmom wymagać więcej niż fabryczny cap. 256=off. |
| `--minsnr-clamp-lo <n>` | 1..bez limitu (Q8) | -1 (off) | Skaluje podłogę MIN_SNR FDK (~−25 dB). 256=off. |
| `--reduce-clamp <0\|1>` | 0, 1 | 1 (on) | `0` zdejmuje sufit \"29 dB Ratio\" redukcji progów w kwantyzatorze CBR, pozwalając wepchnąć progi głębiej (więcej bitow do wymagających pasm). Łączy się z `--minsnr-scale` dla ekstremalnego detalu. Tylko CBR (VBR używa innej ścieżki). |

### Co realnie pomaga w niskim i średnim bitrate (10-144 kbps)

Częste pytanie: czy da sie jeszcze cos wycisnac na dokładności/efektywności
kodowania (Huffman, iteracje kwantyzacji itp.)? Uczciwa odpowiedz po przejrzeniu
kodu FDK:

- Kodowanie Huffmana (łączenie sekcji, wybór codebookow w `dyn_bits.cpp`) jest juz
  optymalne (zachlanne łączenie sekcji dajace min. bitow). Nie ma tam sensownego
  pokretla — a wystawienie tego tylko by pogarszalo wynik.
- Pętla iteracji kwantyzacji (`maxIterations`) to mechanizm RATUNKOWY przy
  niedoborze bitow; zwiekszanie jej nic nie daje (szczegóły w manualu, rozdz. 9a).
- Wewnętrzne progi (adaptacja minSnr, `bits2PeFactor`) to arytmetyka staloprzecinkowa
  z twardymi zakresami — ruszanie ich grozi niestabilnością, nie poprawa.

REALNY zestaw dźwigni jakości dla 10-144 kbps jest JUZ wystawiony:
- `--ath-scale <256` — globalnie obniż prog maskowania (więcej detalu za bity).
- `--spread-mask <256` — mniej maskowania międzypasmowego (więcej pasm kodowanych).
- `--side-bias >0` — więcej bitow do kanału side (czystsza szerokość stereo).
- `--is-aggression` — steruj intensity stereo (kluczowe przy niskim bitrate).
- `--force-pns` + `--pns-start` — kontrola szumu przy bardzo niskim bitrate.
- pod HE-AAC: `--sbr-invf`, `--sbr-noise-floor-offset`, `--speech` (mowa).

To nie brak funkcji — to te same dźwignie, których używa profesjonalny tuning,
tyle ze ręcznie. Zacznij od `--ath-scale` i `--spread-mask`, po jednej naraz.

## 7. Bias przełączania blokow short/long (dowolny profil)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--block-bias <n>` | 0..255 | -1 (off) | Przesuwa prog decyzji short/long. 128 = domyślne enkodera (bez zmian), >128 faworyzuje bloki krótkie (bardziej "transient"), <128 faworyzuje długie, 0 = praktycznie tylko długie. |

WAZNE: `--block-bias` zawsze produkuje strumien zgodny ze standardem (przesuwa
prog detekcji ataku, nie wymusza na sile typu bloku). Zastąpił dawne
`--allshort`/`--alllong`, które tworzyły NIELEGALNY strumien (twarde wymuszenie
short window bez przeliczenia SFB/grupowania -> dekoder odrzucał, Winamp
"skakał jak po porysowanej płycie"). Jesli chcesz maksimum długich: `--block-bias 0`;
maksimum krótkich: `--block-bias 255`.

## 8. Bias decyzji MS (uczciwie: narzędzie o SŁABYM efekcie)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ms-bias <n>` | 0..255 (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny, użyj `--side-bias`.)* Przesuwa prog decyzji L/R vs MS. Q8, 128 = +0.5 w jednostkach ld64 FDK. >0 = MS chętniejsze. Reaguje juz od ~32 (po rekalibracji skali). Do realnego strojenia balansu stereo właściwym narzędziem jest `--side-bias`, nie ten bias. |

UCZCIWIE o `--ms-bias` — to najsłabsze narzędzie z całego zestawu i teraz wiadomo
dlaczego "niewiele robi". MS (mid/side) to transformacja BEZSTRATNA: mid=L+R,
side=L-R odtwarza sie dokładnie z powrotem na L/R. Włączenie/wyłączenie MS na
danym pasmie NIE zmienia brzmienia — zmienia tylko ILE BITOW zajmie zapis. Enkoder
i tak wybiera bliska optymalnej decyzję per pasmo; `--ms-bias` tylko przesuwa
kilka GRANICZNYCH pasm. Pomiar (korelacja L/R po dekodowaniu, rozmiar ADTS):
efekt rzędu <0.1% rozmiaru i zmian korelacji w 4. miejscu po przecinku.

Uwaga techniczna: w poprzedniej wersji skala biasu byla ~256x za słaba (mnożnik
<<15 zamiast <<23) — stąd "128 nic nie robilo, dopiero 2048 cos ruszalo". Teraz
128 = realne +0.5 ld64 jak w opisie, więc reaguje od ~32. Ale nawet poprawnie
wyskalowany bias ma z natury maly wpływ (patrz wyżej).

Chcesz REALNIE sterować MS? Użyj twardych przełączników, nie biasu:
- `--msmask 0` — WYŁĄCZ MS całkowicie (czyste, niezależne L/R). To właściwy
  wybór do center-cancel / usuwania wokalu (zero mieszania kanałów przez koder).
- `--msmask 1` — wymuś MS na wszystkich pasmach (maks. oszczędność bitow).
- `--msbands` / `--msbands-lo/-hi` — ogranicz MS do wybranych pasm.
Pomiar: msmask 0 vs 1 daje ~900 B różnicy na 2s probce; ms-bias tylko ~2 B.

## 9. Quasi-constrained VBR (silnik CBR + szersze oddychanie)

WAZNE: bez tych switchy CBR jest 100% NIEZMIENIONY (zweryfikowane: bit-identyczny
z oryginalna binarka). Włączasz je świadomie.

Jak AAC oddycha: nawet w CBR ramki pożyczają z bit-reservoir, więc jedna ramka
może miec ~122 kbps a następna ~141, byle średnia = target. Te pokretla
poszerzają/ograniczaja to oddychanie.

TWARDY SUFIT dla wszystkiego: ramka AAC miesci MAX 6144 bitow na kanał
(=768 bajtow/kanał); stereo => 12288 bitow/ramke. Przy 44100 Hz jedna ramka =
1024 probki = ~23.2 ms, więc bity/ramke = kbps * 23.22. (Np. 128k stereo:
średnia ~2972 bitow/ramke; sufit 12288.)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--vbr-reservoir <bity>` | 0..(6144*kanały - średnia) | -1 (off) | Rozmiar bit-reservoir. Więcej = większy rozrzut ramek wokol średniej. min 0 (trzymaj sie targetu ciasno). Auto-clamp do sufitu - nie przegniesz. Bezpieczny start: 2-3x default. |
| `--peak-bitrate <bps>` | > target | -1 (off) | Dopuszcza krótkie szczyty do tej wartości, trzymając średnia. Ustaw POWYŻEJ `-b` (np. -b 128000 --peak-bitrate 160000). Poniżej targetu ignorowane. |
| `--max-bits-frame <bity>` | średnia..12288(st.) | -1 (off) | Twardy sufit bitow w JEDNEJ ramce. Musi byc >= średnia i <= 6144*kanały (inaczej clamp). Rozsądny cap ~1.5x średnia (~4500 dla 128k st.). Za nisko = głodzi głośne ramki (słyszalne). |
| `--min-bits-frame <bity>` | 0..średnia | -1 (off) | Twarda podłoga bitow/ramke. Wyższa podłoga marnuje bity na ciszę. Zostaw 0 chyba ze eksperymentujesz. |
| `--bitres-mode <n>` | 0/1/2 | -1 (def) | Tryb reservoir: 0 pelny (jak default), 1 zredukowany, 2 wyłączony (sztywny, najbliżej twardego CBR per-ramka). |

JAK USTAWIAC OPTYMALNIE (dla mniej doświadczonych - żeby nie przegiąć):
- Bezpieczny quasi-CVBR ~128k stereo: `-b 128000 --peak-bitrate 160000 --vbr-reservoir 6000`.
- NIE ustawiaj `--max-bits-frame` PONIŻEJ średniej ani `--min-bits-frame` POWYŻEJ
  średniej - to walczy z targetem i psuje jakość.
- `--vbr-reservoir` jest auto-clampowany do sufitu, więc bezpiecznie eksperymentować.
- Zmierzone realnie (sygnal 4s zmienny, 128k stereo): CBR default rozrzut 95-167
  kbps; z `--vbr-reservoir 8000 --peak-bitrate 192000` rozrzut 36-158 kbps
  (mocniej oddycha, średnia trzymana); `--bitres-mode 2` rozrzut 127.8-128.2
  (sztywny). Wszystkie w pelni dekodowalne.
- Ograniczenie: to silnik CBR+reservoir, nie prawdziwy ABR jak LAME. Wahania
  umiarkowane (limit 6144 bitow/kanał), ale to jest ten "lekki lot" AAC.

## 10. Audiofilskie / ekstremalne (opt-in, poza typowym zakresem)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--uncap-bandwidth` | flaga | off | Zdejmij twardy cap 20 kHz rdzenia. `--core-cutoff` może wtedy sięgnąć az do Nyquista. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | Suwak agresywności IS (patrz sekcja 1). |
| `--force-pns` | flaga | off | PNS poniżej bramki ~28 kbps (patrz sekcja 5). |
| `--unlock-bitrate` | flaga | off | Zdejmij DOLNY prog bitrate. Pozwala na skrajnie niskie: 8k HE-AAC stereo, 6k LC. WAZNE: w tym trybie `-b` bierzemy DOSLOWNIE jako bps (bez konwencji nu774 x1000), więc `-b 6000` = 6000 bps. Górny sufit 6144*kanały zostaje (twardy limit AAC). Rezydualny floor ~10 kbps = minimum nagłówków AAC. |
| `--speech` | flaga | off | Tryb strojenia SBR pod MOWE ludzka (inne progi inverse filtering, poziom szumu, bez parametric coding). Dotyczy HE-AAC (SBR); LC nie ma osobnego trybu mowy. Dla czystej mowy w niskim bitrate. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = MNIEJ maskowania = więcej detali (odpowiednik luzowania tone-masks-noise). Największy efekt gdzie bity ograniczone (96-192k). 256=bez zmian. Łącz z `--ath-scale <256`. |

PASMO POWYŻEJ 20 kHz (audiofilskie): FDK ma zaszyty cap `min(20000, sr/2)` na
pasmo rdzenia — nawet przy wejściu 96 kHz i wysokim bitrate realnie nic powyżej
20 kHz nie jest kodowane (Twoje podejrzenie bylo trafne). `--uncap-bandwidth` znosi
ten cap; wtedy `--core-cutoff` steruje pasmem az do sr/2.

Zmierzone (96 kHz wejście, LC 400k, szum szerokopasmowy):
- bez uncap, `--core-cutoff 40000`: verbose 20000 Hz, energia >20 kHz ~= 0%.
- `--core-cutoff 40000 --uncap-bandwidth`: verbose 40000 Hz, energia 20-24 kHz
  ~10%, 24-32 kHz ~20%, 32-44 kHz ~20% — pelne pasmo do 40 kHz realnie kodowane.

Preset audiofilski (pelne pasmo + ręczne maskowanie, dla 190+ kbps):
```
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 \
    --ath-scale 200 -o out.m4a in96k.wav
```
`--ath-scale <256` obniża progi maskowania (czysciej, więcej bitow na detale) —
sensowne gdy masz duzy zapas bitrate. Uwaga: pasmo >20 kHz i skrajne ustawienia
mogą byc odrzucone przez czesc dekoderow (poza typowa specyfikacja) — świadomy
opt-in.

---

## 11. Kontener MP4/M4A — które boxy są konieczne, a co można wyciąć

Plik .m4a to zestaw zagnieżdżonych "boxow" (atomow). Sprawdzone, co jest czym:

OBOWIAZKOWE (bez nich plik jest NIEGRYWALNY — nie ma do nich przełączników):
`ftyp`, `mdat` (surowe dane audio), oraz szkielet `moov` → `mvhd` + `trak` →
`tkhd` → `mdia` (`mdhd`/`hdlr`/`minf` → `smhd`/`dinf`/`stbl` z tabelami
`stsd`/`stts`/`stsc`/`stsz`/`stco`). To jest minimalna mapa pliku wymagana przez
standard ISO — kazdy dekoder tego potrzebuje, żeby w ogole znaleźć i odtworzyć
dźwięk. Wycinanie ich nie ma sensu (efekt: uszkodzony plik).

OPCJONALNE (można wyłączyć) — cały blok `udta` → `meta` → `ilst`, czyli:
- tag identyfikacyjny kodera (`©too`, teraz "PompAAC based on …"),
- `iTunSMPB` — dane o opóźnieniu kodera do bezszwowego (gapless) odtwarzania,
- wszystkie tagi (tytuł, artysta, album itd.).

| Switch | Działa na | Opis |
|---|---|---|
| `--no-tool-tag` | .m4a | Nie zapisuj tagu identyfikującego koder (`©too`). Reszta tagow i gapless zostaje. |
| `--minimal-moov` | .m4a | Najmniejszy legalny .m4a: pomija CALY blok `udta`/metadata (tag kodera + gapless iTunSMPB + wszystkie tagi). Szkielet odtwarzania zostaje nienaruszony. |

Ile to oszczędza (2 s, 128k stereo, pomiar): default ~34381 B →
`--no-tool-tag` ~34297 B (−84) → `--minimal-moov` ~34048 B (−333). To male
liczby — narzut MP4 to glownie obowiazkowy szkielet, którego usunąć sie nie da.
Chcesz naprawde zero narzutu kontenera? Użyj surowego ADTS: `-f 2 -o out.aac`
(strumien bez żadnych boxow, ale tez bez tagow i gapless).

UWAGA gapless: `--minimal-moov` usuwa iTunSMPB, więc przy łączeniu utworow mogą
pojawić sie mikro-przerwy (encoder delay nie bedzie zasygnalizowany). Do
zwykłego odsłuchu bez znaczenia; do płyt "bez przerw" zostaw domyślne.

---

## 12. Legenda odczytu `--verbose`

`--verbose` wypisuje SUROWE wartości (bez podpowiedzi w nawiasach, żeby nie
zaśmiecać). Poniżej co oznaczają te, które nie są oczywiste:

| Pole | Jak czytac |
|---|---|
| `AOT (profile)` | 2=AAC-LC, 5=HE-AAC, 29=HE-AAC v2, 23=AAC-LD, 39=AAC-ELD. |
| `bitrate-mode` | 0=CBR, 1..5=VBR (wyżej=lepiej). |
| `channel-mode` | 1=mono, 2=stereo (dla HE-AAC v2 rdzen jest mono, stereo robi PS). |
| `core bandwidth` | Górna częstotliwość rdzenia AAC, **zakotwiczona na najbliższej granicy SFB** (realny cutoff, który może różnić się od podanej wartości `-w`/`--core-cutoff`, np. `-w 17300` → `17915 Hz (SFB-anchored)`). W nawiasie ZRODLO: `from -w`, `from --core-cutoff`, albo `auto`. Pod SBR to tylko rdzen — SBR gra wyżej. |
| `final BW (AAC+SBR)` | Pokazywane tylko gdy SBR aktywny: orientacyjna GÓRNA częstotliwość całego sygnalu (rdzen + SBR), wyliczona ze `sbr stop freq index`. To odpowiednik `core bandwidth`, ale dla pelnego pasma HE-AAC. |
| `signaling-mode` | Sposób sygnalizacji SBR/PS: 0=implicit, 1=explicit backward-compat, 2=explicit hierarchical, auto=biblioteka wybiera. |
| `SBR mode` | Wewnętrzny tryb SBR (-1/0 gdy nieuzywany). |
| `sbr-ratio` | 1=downsampled (single-rate), 2=dual-rate (rdzen na polowie częstotliwości). |
| `sbr amp res` | 0=1.5 dB, 1=3.0 dB (rozdzielczość amplitudy obwiedni). |
| `granule/frame length` | Długość ramki w probkach (1024 dla LC, 512/480 dla LD/ELD). |
| `codec delay` | Opóźnienie kodeka w probkach/kanał (total i sam rdzen). Do gapless. |
| `IS corr threshold` / `IS L/R ratio` | Progi w skali Q8: 256 = 1.0. Niższy prog korelacji = intensity stereo CHĘTNIEJSZE (odwrotnie do intuicji). |
| `IS min contiguous SFBs` | Ile sąsiadujących pasm musi sie "zgodzić", zanim włączy sie IS. |
| `TNS mask` | Maska bitowa 0x0..0xF które filtry TNS aktywne. |
| `MS/IS bands: auto up to N` | Górny indeks pasma, do którego narzędzie może działać. |
| `franken overrides applied` | Lista przełączników które w TYM uruchomieniu odbiegają od czystego FDK (albo "none"). |

Wartości Q8 (jak `IS corr threshold`, `--ms-precision`, `--ms-bias`,
`--ath-scale`, `--spread-mask`) to liczby staloprzecinkowe gdzie 256 = 1.0;
np. 243 oznacza 243/256 ≈ 0.95.

---

## 13. Tabele referencyjne (z tablic strojenia FDK)

Trzy tabele orientacyjne, żeby świadomie dobierać `--msbands`, `--sbr-start/stop`
i `-w`. Wartości wyliczone z tablic w kodzie FDK; są PRZYBLIZONE (siatka SFB jest
schodkowa), ale pokazuja właściwy rząd wielkości.

### Tabela 1 — orientacyjna górna częstotliwość pasma (SFB) [Hz]

Pasma numerowane od dołu (0=bas). Pokazano co 4. pasmo; ostatni wiersz = liczba
pasm i częstotliwość Nyquista. Użyteczne przy `--msbands`/`--isbands`/`--msbands-lo/-hi`.

| SFB | 16 kHz | 22.05 kHz | 32 kHz | 44.1 kHz | 48 kHz | 96 kHz |
|----:|-------:|----------:|-------:|---------:|-------:|-------:|
| 0   | 62   | 43   | 62   | 86    | 94    | 188   |
| 4   | 312  | 215  | 312  | 431   | 469   | 938   |
| 8   | 562  | 388  | 562  | 775   | 844   | 1688  |
| 12  | 875  | 646  | 1000 | 1378  | 1500  | 2438  |
| 16  | 1250 | 991  | 1500 | 2067  | 2250  | 3750  |
| 20  | 1656 | 1335 | 2250 | 3101  | 3375  | 5625  |
| 24  | 2188 | 1852 | 3375 | 4651  | 5062  | 8062  |
| 28  | 2875 | 2584 | 5000 | 6891  | 7500  | 12938 |
| 32  | 3844 | 3618 | 7000 | 9647  | 10500 | 24000 |
| 36  | 5188 | 5039 | 9000 | 12403 | 13500 | 36000 |
| 40  | 7000 | 7020 | 11000| 15159 | 16500 | 48000 |
| 44  | —    | 9647 | 13000| 17916 | 19500 | —     |
| 48  | —    | —    | 15000| 22050 | 24000 | —     |
| **liczba pasm / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### Tabela 2 — SBR: indeks start freq → orientacyjna częstotliwość przejścia [Hz]

To częstotliwość, od której SBR przejmuje pasmo powyżej rdzenia AAC (`--sbr-start`,
indeks 0..15; niższy = SBR startuje niżej = wezszy rdzen). "core" to częstotliwość
rdzenia; przy dual-rate wyjście jest dwa razy wyższe (np. core 24k → wyjście 48k).

| indeks start | core 16 kHz | core 24 kHz | core 32 kHz | core 44.1 kHz | core 48 kHz |
|----:|----:|----:|----:|----:|----:|
| 0 | 2750 | 2250 | 2500 | 1378 | 1500 |
| 1 | 3000 | 2625 | 3000 | 2067 | 2250 |
| 2 | 3250 | 3000 | 3500 | 2756 | 3000 |
| 3 | 3500 | 3375 | 4000 | 3445 | 3750 |
| 4 | 3750 | 3750 | 4500 | 4134 | 4500 |
| 5 | 4000 | 4125 | 5000 | 4823 | 5250 |
| 6 | 4250 | 4500 | 5500 | 5512 | 6000 |
| 7 | 4500 | 4875 | 6000 | 6202 | 6750 |
| 8 | 4750 | 5250 | 6500 | 6891 | 7500 |

Stop freq (`--sbr-stop`, 0..13) działa analogicznie na górnej granicy SBR — wyższy
indeks = SBR sięga wyżej (bliżej Nyquista wyjścia). Domyślnie biblioteka dobiera
oba pod bitrate.

### Tabela 3 — AAC-LC: orientacyjne odcięcie (`-w`/auto) wg bitrate na kanał [Hz]

Gdy nie podasz `-w`, FDK dobiera pasmo z tej tablicy wg bitrate NA KANAL (stereo
128k = 64k/kanał). Wartości interpolowane; kolumna mono i stereo różna. Pomaga
ocenic, czy `-w` ma sens (podawanie wyższego niż auto ma efekt tylko jesli jest zapas bitow).

| bitrate/kanał | pasmo (mono) | pasmo (stereo+) |
|----:|----:|----:|
| 0–12 kbps  | 3700  | 5000  |
| 20 kbps    | 6900  | 9640  |
| 28 kbps    | 9600  | 13050 |
| 40 kbps    | 12060 | 14260 |
| 56 kbps    | 13950 | 15500 |
| 72 kbps    | 14200 | 16120 |
| ≥96 kbps   | 17000 | 17000 |

Uwaga: ta tablica jest WSPOLNA dla 32/44.1/48 kHz i wyższych — FDK indeksuje ja
bitratem na kanał, nie samplerate (samplerate wpływa tylko na górny limit =
Nyquist). Dla LC bez SBR realny sufit to ~17 kHz z auto; wyżej tylko przez `-w`
(z zapasem bitow) lub `--uncap-bandwidth` przy sr≥96k.

---

## Przykłady

```
# Punkt wyjscia — co ustawil enkoder:
fdkaac-franken-x64.exe -p 29 -b 48000 --verbose -o out.m4a in.wav

# Quasi-constrained VBR ~128k (bezpieczny preset):
fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

# Agresywne intensity stereo przy 64k:
fdkaac-franken-x64.exe -p 2 -b 64000 --is 1 --is-aggression 70 -o out.m4a in.wav

# PNS na sile przy 24k (inaczej bramka FDK je wyłącza):
fdkaac-franken-x64.exe -p 2 -b 24000 --pns 1 --force-pns -o out.m4a in.wav

# Audiofilskie pelne pasmo 40 kHz z wejscia 96 kHz:
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

# MS na 5 najwyzszych pasmach + plytsze dziury (nie najnizsze):
fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 --ms-precision 448 -o out.m4a in.wav

# Skrajnie niski HE-AAC v2: 8000 bps stereo 48 kHz:
fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

# SBR: pelna separacja stereo LR + wymuszony inverse filtering:
fdkaac-franken-x64.exe -p 5 -b 96000 --sbr-stereo-mode 1 --sbr-invf 2 -o out.m4a in.wav

# Twoj przypadek: 7.5 kHz rdzenia pod HE-AAC v2 48k:
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav

# Kompletnie niezalezne stereo (bez MS/IS) na LC 128k:
fdkaac-franken-x64.exe -p 2 -b 128000 --msmask 0 --is 0 -o out.m4a in.wav

# MS tylko na 6 dolnych pasmach:
fdkaac-franken-x64.exe -p 2 -b 96000 --msmask 1 --msbands 6 -o out.m4a in.wav

# Tylko krotkie bloki + ograniczony TNS:
# Tylko dlugie bloki (bias) + ograniczony TNS:
fdkaac-franken-x64.exe -p 2 -b 96000 --block-bias 0 --tns-order 2 -o out.m4a in.wav

# Agresywniejsze maskowanie (progi x2) + wczesniejszy PNS:
fdkaac-franken-x64.exe -p 2 -b 80000 --ath-scale 512 --pns 1 --pns-start 4000 -o out.m4a in.wav

# Gestszy opis szumu SBR + dokladniejsza amplituda:
fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-noise-bands 5 --sbr-amp-res 0 -o out.m4a in.wav

# HE-AAC v2 z wyłączonym PS (splaszczone stereo):
fdkaac-franken-x64.exe -p 29 -b 32000 --ps 0 -o out.m4a in.wav
```

---

## Jakość domyślna vs oryginalna binarka (fdkaac2.exe)

Weryfikacja (21.07.2026) czy domyślne ustawienia franken = oryginalna dostarczona
binarka (dBpoweramp R17, ta sama wersja libfdk-aac 4.0.1 / pakiet 2.0.3):
- AAC-LC: **bit-identyczne** (ten sam md5) — zero regresji.
- HE-AAC v1/v2: bajty sie różnią, ALE: widmo HE-AAC v2 identyczne co do 0.000 dB,
  a błąd HE-AAC v1 wzgledem oryginału jest praktycznie taki sam jak w oryginale
  (7.05e11 vs 7.05e11). Różnica bajtow wynika z innego kompilatora (mingw/gcc vs
  MSVC) i zaokrągleń fixed-point, NIE z gorszej jakości. Słyszalna "inność" SBR
  to inna, równie poprawna realizacja, nie strata jakości.

---

## Testy (make check)

Upstream fdk-aac/nu774 nie maja testów jednostkowych (`make check` w nich = no-op).
Ten projekt ma WŁASNY funkcjonalny test suite w `tests/check.sh`, uruchamiany:

```
make check
```

Sprawdza (na binarkach x64 + x86 jesli obecne): kompletność switchy w --help,
dekodowalność strumienia dla kazdego przełącznika (ffmpeg, 0 błędów), realne
działanie quasi-CVBR (CVBR oddycha szerzej niż sztywny bitres-mode2), verbose bez
wartości -1, oraz BRAK REGRESJI (domyślny CBR bez franken-flag = ADTS
bit-identyczny z oryginalna fdkaac2.exe). Wymaga ffmpeg + python3 (są w WSL).
Exit 0 = OK, 1 = błędy, 77 = brak zależności. Ostatni wynik: 27/27 PASS.

---

Zrodla z naniesionymi patchami leza w `src-fdk-aac/` i `src-fdkaac/`.
Wszystkie zmiany w libfdk są spięte przez jeden modul
`libAACenc/src/franken.{h,cpp}` (globalny blok `g_franken`, sentinele = domyślne
FDK), a nowe `AACENC_PARAM` (zakres `0xF0xx`) są wystawione w `aacenc_lib.h`.

```bash
sudo apt-get install -y mingw-w64          # + autotools (autoconf/automake/libtool)

# --- libfdk-aac (x64) ---
cd src-fdk-aac && autoreconf -i
./configure --host=x86_64-w64-mingw32 --prefix=$PWD/../inst-x64 \
    --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2
make -j && make install

# --- frontend (x64) ---
cd ../src-fdkaac && autoreconf -i
PKG_CONFIG_PATH=$PWD/../inst-x64/lib/pkgconfig ./configure \
    --host=x86_64-w64-mingw32 CFLAGS="-O2 -I$PWD/../inst-x64/include" \
    LDFLAGS="-static -static-libgcc -L$PWD/../inst-x64/lib"
make -j     # -> fdkaac.exe

# x86: to samo z --host=i686-w64-mingw32 i osobnym prefixem inst-x86.
```

## Lista zmienionych plikow FDK (mapa na punkty)
- `libAACenc/src/franken.{h,cpp}` — nowy modul sterujący.
- `libAACenc/include/aacenc_lib.h` — nowe `AACENC_PARAM` 0xF0xx.
- `libAACenc/src/aacenc_lib.cpp` — dispatch SetParam + override useMS/IS/PNS/
  afterburner/cutoff + guard cutoffu pod SBR (po `sbrEncoder_Init`) + read-only
  GetParam mirrors dla verbose (useTns/Pns/IS/MS, efektywne SBR).
- `libAACenc/src/ms_stereo.cpp` — kontrola MS per-pasmo W PETLI decyzyjnej (maska
  + motylek L/R->M/S zsynchronizowane; naprawiony artefakt "lewy=center") + MS bias
  + zakres pasm MS (--msbands-lo/-hi) + precyzja MS (--ms-precision, prog ld64).
- `libAACenc/src/intensity.cpp` — kap pasm IS (spójny z maska) + bias progow IS
  (initIsParams: min_is_sfbs, corr_thresh, left_right_ratio) + --is-aggression.
- `libAACenc/src/psy_configuration.cpp` — read-back liczby SFB + zniesienie gate IS.
- `libAACenc/src/bandwidth.cpp` — --uncap-bandwidth (zdjęcie capa 20kHz).
- `libAACenc/src/pnsparam.cpp` — override PNS start + --force-pns (bypass bramki tabeli).
- `libAACenc/src/aacenc.cpp` — override maski TNS + quasi-CVBR + --unlock-bitrate
  (zniesienie dolnego floora bitrate w FDKaacEnc_LimitBitrate).
- `libSBRenc/src/sbr_encoder.cpp` — override gęstości SBR + --sbr-num-env (static
  framing) + --sbr-freqres-fixfix + --sbr-stereo-mode + --sbr-noise-floor-offset.
- `libSBRenc/src/invf_est.cpp` — --sbr-invf (wymuszony poziom inverse filtering).
- `libSBRenc/src/ps_encode.cpp` — override PS IID + --ps-icc/--ps-icc-mode.
- `libAACenc/src/aacenc_tns.cpp` — kap rzędu TNS.
- `libAACenc/src/pnsparam.cpp` — override startowej częstotliwości PNS.
- `libSBRenc/src/sbr_encoder.cpp` — override gęstości/dokładności SBR + zapis
  efektywnych wartości SBR do g_franken (dla verbose).
- `libSBRenc/src/ps_encode.cpp` — override PS (wymuszenie IID + tryb kwantyzacji).
- `libAACenc/src/main.c`, `aacenc.c`, `aacenc.h` (frontend) — switche CLI,
  parsowanie, przekazanie do SetParam, dump `--verbose`, help.
