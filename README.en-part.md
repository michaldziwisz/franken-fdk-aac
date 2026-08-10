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
- **F. DAB+ digital radio:** `--dab`, `--dab-label` — section 14.

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
| `--sbr-noise-max <n>` | 6, 3, -3 | -1 (tuning table) | Ceiling on how loud the noise SBR injects into the high band may get: `6` = 1.0, `3` = 0.5, `-3` = 0.125. This is the "air versus hiss" limit. Already a config field inside FDK, just never reachable from the command line. |
| `--sbr-tran-peak <n>` | 1..200 | -1 (90 = 0.90) | Transient detector: how *peaky* an attack must be. A slot counts as a transient only once the signal falls below this fraction of the previous slot. Raw x100, so `90` is the stock 0.90. Higher = fewer transients detected. |
| `--sbr-tran-thr <n>` | 1..10000 | -1 (100 = x1.00) | Scales the master transient threshold. **The most effective knob in this group** - see the pre-echo measurement below. |
| `--sbr-tran-split <n>` | 1..10000 | -1 (100 = x1.00) | Scales the envelope-split threshold: how readily a frame with no detected transient is still split into two envelopes. Lower = split more often (finer time resolution, costs bits). |
| `--sbr-tran-quiet <n>` | 1..10000 | -1 | **No-op in this build.** Lives in the *fast* transient detector, which is only reached under AAC-LD/ELD - and `-p 23` / `-p 39` fail to initialise here (also in the stock binary). Kept for completeness only. |
| `--sbr-tran-dom <n>` | 100..500 | -1 (140 = 1.4) | **No-op in this build.** Same unreachable fast-detector path as `--sbr-tran-quiet`. |
| `--sbr-mh-tone <n>` | 1..10000 | -1 (100 = x1.00) | Missing harmonics: scales how *tonal* a band must be before SBR fabricates a synthetic harmonic. Lower = more added tones (risk of whistling artefacts); higher = fewer (dull bells and cymbals, lost partials). |
| `--sbr-mh-diff <n>` | 1..10000 | -1 (100) | Scales the original-versus-patched tonality *difference* that triggers harmonic compensation. |
| `--sbr-mh-decay-orig <n>` | 1..10000 | -1 (100) | Scales how long an already-detected tone keeps being tracked as it decays. Audible on bell and cymbal tails. |
| `--sbr-mh-decay-diff <n>` | 1..10000 | -1 (100) | Same, for the difference guide vector. |
| `--sbr-mh-sfm-sbr <n>` | 1..10000 | -1 (100) | Scales the spectral-flatness threshold above which the *patched* band is judged noise-like rather than tonal. |
| `--sbr-mh-sfm-orig <n>` | 1..10000 | -1 (100) | Same for the *original* band. Together these two catch "one strong tone in the original became several after patching". |
| `--sbr-mh-maxcomp <n>` | 0..200 | -1 (50) | Cap on envelope compensation applied around a synthetic harmonic. Affects neighbouring bands and how much noise sits next to the added tone. |
| `--sbr-mh-deltatime <n>` | 0..64 | -1 (9 / 16) | Max transient distance for a frame to count as a transient frame in the missing-harmonics detector. |

NOTE on units: knobs documented as `x100` are multipliers on a stock constant,
where `100` means 1.00 and is an exact no-op. Knobs documented as *raw* x100
replace the constant outright (`--sbr-tran-peak 90` **is** the stock 0.90).
`--sbr-mh-maxcomp` and `--sbr-mh-deltatime` are plain integers, and
`--sbr-noise-max` takes only the three values FDK's noise estimator understands.

MEASURED - pre-echo in the SBR band. This is the one result worth acting on. Test
signal: 24 percussive attacks (hi-hat/clap character) plus four decaying tones
between 8 and 13 kHz. Metric: energy above 9 kHz appearing in the two 5.8 ms
windows *before* each attack that is not present in the source - i.e. audible
smearing of the attack backwards in time. Encoded as HE-AAC, decoded with ffmpeg,
lag-aligned by cross-correlation first:

| `--sbr-tran-thr` | pre-echo @ 32 kbps | pre-echo @ 64 kbps |
|---|---|---|
| stock | 1.610 | 1.186 |
| 20 / 40 / 60 | **0.041** | **0.059** |
| 80 | 0.661 | 0.502 |
| 100 and above | 1.610 (= stock) | 1.186 (= stock) |

Lowering the master transient threshold to 40 essentially removes pre-echo in the
replicated band: the detector then notices the attacks it was previously missing
and gives them their own envelope instead of averaging across the transient. The
effect is monotonic, reproducible at both bitrates, and saturates below 60 - so
`--sbr-tran-thr 40` is the recommended starting point on percussive material.
Everything from 100 upwards is byte-identical to stock, which also confirms the
knob is cleanly opt-in.

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
| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | ICC rotation mode: 0 = ROT_A, 1 = ROT_B. Signalling only — the same matrix, derived differently by the decoder, so treat this as a compatibility knob rather than a quality one. |
| `--ps-bands <n>` | 10 or 20 | -1 (bitrate table) | Number of PS stereo bands = **frequency** resolution of the stereo parameters. Stock FDK derives this from the bitrate alone, so at 22 kbps and above you always get 20 and can never audition 10. Fewer bands = coarser stereo image, fewer parameter bits. |
| `--ps-env <n>` | 1, 2 or 4 | -1 (bitrate table) | PS parameter envelopes per frame = **time** resolution of the stereo parameters. Above 36 kbps stock FDK always picks 4. More envelopes track a moving panorama and transients more closely. |
| `--ps-env-reduce <n>` | 0, 1 | -1 (on) | `0` disables the automatic envelope-halving loop (`envelopeReducible`). By default FDK keeps collapsing 4 envelopes to 2 to 1 whenever neighbouring envelopes look similar by a hardcoded error threshold, so the envelope count you configured is often *not* what gets transmitted. `0` makes `--ps-env` literal. |
| `--ps-noenv-skip <n>` | 0, 1 | -1 (on) | `0` forbids parameter-less PS frames. By default FDK may emit up to 10 consecutive frames carrying **no** stereo parameters at all when successive IID/ICC sets look similar, which can be heard as the stereo image briefly collapsing and snapping back. `0` = always send parameters. |
| `--ps-ipd <0|1>` | 0, 1 | -1 (off) | Transmit **IPD** — the inter-channel *phase* difference — in the PS extension. Stock FDK never sends phase: it computes the complex L/R cross-spectrum, keeps only the magnitude for ICC and discards the angle. This knob takes that same angle (`atan2` over data already in the loop), quantises it to the 8 defined steps of pi/4 and enables the existing bitstream writer. Coded for the lower 5 (10-band) / 11 (20-band) parameter bands, per the MPEG-4 PS syntax. **Compatible:** the payload lives in a length-prefixed extension, so decoders without phase synthesis skip it by its byte count — explicitly permitted by ISO/IEC 14496-3. Verified: byte-identical output when off, clean decode with ffmpeg *and* faad at 24/32/48/64 kbps when on. Note that enabling this also makes the stream signal IID, which is what carries the phase band count to the decoder (see CHANGES for v1.3.1); without it the decoder discards the phase. |
| `--ps-opd <0|1>` | 0, 1 | -1 (on, with `--ps-ipd 1`) | Only meaningful together with `--ps-ipd 1`. **OPD** is the phase of the left channel relative to the *mono downmix*, and the decoder needs both parameters: it rotates the downmix-fed paths by `OPD` and the others by `OPD - IPD`. The difference therefore always comes out as IPD, but *where* that rotation sits relative to the downmix is what OPD decides. `0` pins OPD to zero (the stock neutral), which leaves the phase error lopsided between channels. Computing it costs nothing extra — see below. |

WHY OPD IS NOT OPTIONAL, and why it costs nothing. With `OPD = 0` the left channel
is glued to the downmix phase and the whole rotation is dumped onto the right one.
The inter-channel *difference* is still correct, so a measurement of `arg(L) - arg(R)`
cannot see the problem at all — but each channel's absolute phase is wrong, and
asymmetrically so. Measuring the absolute phase of each channel against the source
inside the IPD range (60-690 Hz) shows exactly that, and shows OPD fixing it:

| Inter-channel delay | OPD = 0 (L / R) | OPD computed (L / R) |
|---|---|---|
| 0.25 ms | 14.7 / 20.1 deg | 14.4 / **17.3** deg |
| 0.4 ms | 23.0 / 28.0 deg | **15.5** / **17.8** deg |
| 0.7 ms | 48.1 / 56.2 deg | **40.9** / **36.3** deg |

Note the shape of it: at `OPD = 0` the two channels always disagree (14.7 against
20.1, 48.1 against 56.2), which is the asymmetry the parameter exists to remove.
With OPD computed the pair converges, and the gain grows with the delay — averaging
+6.4 degrees, reaching +13 at 0.7 ms.

Deriving it needs no new analysis, which is the neat part. The downmix is `L + R`, so

    sum(L * conj(L+R)) = sum(|L|^2) + sum(L * conj(R))

whose real part is `pwrL + pwrCr` and whose imaginary part is `pwrCi` — the very
accumulators already used for IID, ICC and IPD. So `OPD = atan2(pwrCi, pwrL + pwrCr)`,
one more `atan2` over data that was already there. For the equal-level delay case
this reduces analytically to `IPD/2` (because `arg(1 + e^{-i.phi}) = -phi/2`), which
is a useful sanity check — but the general form is also correct when the two channels
differ in level, where `IPD/2` would not be.

NOTE on what IPD buys you, and where it stops. Without phase, PS positions sound
using level differences (IID) and correlation (ICC) only. That works for anything
panned by level, but not for a source placed by *timing* — a real inter-channel
delay, which is how natural low-frequency localisation actually works. Measured on
probes built exactly that way (identical signal in both channels, equal level,
offset only by an inter-channel delay), comparing the decoded phase profile inside
the IPD range (60-690 Hz) against the source:

| Inter-channel delay | phase error, IPD off | phase error, IPD on |
|---|---|---|
| 0.25 ms | 35.0 deg | **14.9 deg** |
| 0.4 ms | 56.1 deg | **24.9 deg** |
| 0.7 ms | 67.4 deg | 71.8 deg |

At 0.25 and 0.4 ms the phase error drops by more than half, and the result is
identical at 32 and 48 kbps. At 0.7 ms it stops helping — and that is a property
of the representation, not a bug: by the top of the IPD range (~690 Hz) a 0.7 ms
delay has already run through nearly the full +/-180 deg, so a 45 deg grid can no
longer track it unambiguously. This is the classic phase-ambiguity limit of ITD,
and the same reason hearing relies on timing cues mainly at low frequencies.

Two things to keep in mind. First, IPD only covers roughly the lowest 690 Hz
(11 parameter bands sit inside the first three QMF bands), so measuring or
listening above that will show nothing — an earlier measurement of ours looked
like a regression purely because it was scoped to 100-1500 Hz. Second, **OPD is
deliberately left at zero.** The decoder couples the two: OPD rotates both
downmix paths while IPD only shifts the second. Sending zero is the defined
neutral; we also measured the physically-motivated OPD = IPD/2 and it did not
reconstruct the source phase either, so we are not guessing at their model.

NOTE about PS resolution: `--ps-bands` and `--ps-env` are the two axes that
change *how many* stereo parameters are actually transmitted — in frequency and
in time respectively. That makes them considerably more audible than
`--ps-icc-mode`, which only changes how the same matrix is signalled. Measured on
a 4-second stereo probe with a deliberately moving panorama (0.25 Hz) plus
alternating L/R transients, encoded as HE-AAC v2 at 48 kbps, comparing the
panorama trajectory of the decoded file against the source:

| Setting | Panorama error (RMS) | Correlation with source |
|---|---|---|
| stock (20 bands / 4 envelopes) | 0.177 | 0.9655 |
| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
| `--ps-env 4 --ps-env-reduce 0` | **0.117** | **0.9944** |

The interesting result is that requesting 4 envelopes alone changes nothing —
stock output and `--ps-env 4` are byte-identical, because the automatic halving
loop immediately collapses them again. The gain only appears once
`--ps-env-reduce 0` stops that loop: a 34 % reduction in panorama error for
roughly the same file size. If you only take one thing from this group, take
`--ps-env-reduce 0`.

NOTE about IPD/OPD: FDK codes IID (loudness differences) and ICC (coherence)
only. The interchannel *phase* parameters are not emitted — `ps_encode.cpp`
literally writes zeros and comments `"IPD OPD not supported right now"`. Note
however that the encoder already computes both the real and the imaginary part of
the L/R cross-spectrum (`pwrCr` / `pwrCi`) and currently uses only their
magnitude, so the phase information is present but discarded; the Huffman tables
and bitstream writers for IPD/OPD also already exist in `ps_bitenc.cpp`.

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

## 14. DAB+ output (`--dab`, `--dab-label`)

A dedicated output mode that emits a DAB+ digital-radio stream instead of an
MP4/M4A file or a bare ADTS stream. The encoder produces the AAC audio exactly as
the DAB+ system expects it (960-sample transform, 120 ms super frame, error
protection), so the stream can be handed straight to a multiplexer such as
`odr-dabmux` → ETI → transmitter (or a soft receiver like welle.io / dablin).

DAB+ is not "AAC in a different box": it uses the 960-sample MDCT (not 1024), packs
audio into 120 ms **super frames**, guards the header with a **firecode** (Fire
CRC), and protects the payload with **Reed-Solomon RS(120,110)** over GF(256). The
output is a raw `.dabp` stream — successive super frames back to back — which is
what a DAB+ multiplexer ingests.

### `--dab`
Turn on DAB+ super-frame output. The result is a raw `.dabp` stream (no MP4, no
ADTS). Constraints imposed by the standard:

| Requirement | Value |
|---|---|
| Sample rate | MUST be `32000` or `48000` Hz |
| Bitrate | multiple of 8 kbps, range 8..192 kbps |
| Channels | mono or stereo |
| Profiles | AAC-LC, HE-AAC, HE-AAC v2 (all three) |

The profile (AOT) is picked AUTOMATICALLY from bitrate and channel count, the same
way `odr-audioenc` does it — you normally don't set `-p`:

- stereo ≤48 kbps (subchannel ≤6) → **HE-AAC v2** (PS),
- mono ≤64k or stereo ≤80k → **HE-AAC** (SBR),
- higher → **AAC-LC**.

You can still force the profile with `-p` (`2`=LC, `5`=HE-AAC, `29`=HE-AAC v2) if
you know what you want.

### `--dab-label <text>`
A static **DLS** (Dynamic Label Segment) carried as X-PAD inside the super frame;
DAB+ receivers show it as the station name / title. Up to ~48 characters (three
segments in one PAD). "Static" means one fixed string for the whole file —
time-varying labels and MOT slideshow (the `ODR-PadEnc` model) are planned for
later. Without `--dab` the label is ignored.

### Examples

```
# 48k/32k stereo, ~96k → auto AAC-LC:
fdkaac --dab -b 96 -o out.dabp input.wav

# → auto HE-AAC (SBR):
fdkaac --dab -b 64 -o out.dabp input.wav

# → auto HE-AAC v2 (PS):
fdkaac --dab -b 32 -o out.dabp input.wav

# with a station label:
fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

# broadcast chain:
# out.dabp → odr-dabmux → ETI → transmitter / decoder
```

Note: without `--dab` the encoder behaves exactly as before — zero impact,
bit-identical to stock. Verified: the streams decode with an independent faad2
decoder (dablin), the DLS label is readable, and the LC output is bit-identical to
the reference `odr-audioenc`. Nine combinations (48/32 kHz × mono/stereo × the
three profiles) were tested, each independently decodable.

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
