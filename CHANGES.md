# CHANGES

This is a **Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library** (per section 2 of the FDK AAC license, see `NOTICE.fdk-aac`).

## Releases

### v1.4.0-beta.2 (in progress) — band switches in short blocks, input resampling

#### Faster encoding, unchanged bits

- Windows builds (local `tools/build_win.sh` and CI) compile with
  `-O3 -DNDEBUG -flto` plus PGO: the instrumented exe trains on a fixed set of
  encodes, then the library and the frontend are rebuilt with the collected
  profiles. Native measurements (5 min stereo 48 kHz, interleaved medians):
  +13% over the old plain `-O2` (LC 82.6 → 93.3x, HE 66.2 → 74.5x,
  HEv2 121.8 → 137.7x real time). Every output stays bit for bit identical
  to the previous build: verified on ADTS for LC 128k, HE 48k, HEv2 32k
  (30 s and 5 min), `--resample 44100` and `--dab 96`. `FRANKEN_NO_PGO=1`
  skips the training phase.
- The progress bar now writes each refresh as a single stderr write and at
  most 10 times per second (the console write cost is real on Windows).
  The line format is unchanged. Long files encode measurably faster.
- Regular input and output files get a 1 MiB stdio buffer. Streams are not
  touched: when the input comes from stdin or the output goes to stdout
  (pipe use), buffering and block behavior stay exactly as before.

#### New: `--resample <rate>`

- Converts the input to another AAC sample rate before encoding. Hz (`44100`) or kHz
  shorthand (`44`, `44.1`, `44,1`, `22.05`, `32` ...); `-1` = off. No automatic
  choice: the rate is used only when asked for.
- Placed right after the PCM reader (WAV, CAF, raw, file or pipe), before the
  limiter, the edge extrapolator and the encoder, so the library, the M4A timescale,
  the gapless length and `--verbose` all see only the new rate. Input already at the
  requested rate: untouched chain, bit for bit the stream without the switch.
- Exact rational ratio, windowed sinc (Kaiser), float; flat to 95% of the lower
  Nyquist, 120 dB stopband, symmetric kernel aligned with the input = no added delay.
  The float result passes the stock limiter and is rounded (not truncated) to the
  library's integer PCM. Own code (`src-fdkaac/src/resample.c`), no external library.
- Measured on the converter alone (`tools/resample/`): passband within 0.0001 dB,
  aliases -120..-132 dB, THD+N -135..-141 dB, phase shift 0, exact length, output
  independent of the read size; 37..106x real time stereo on one core. After encoding
  at 32 kbps stereo `-w 9500`: within 0.03 dB of SoX at its highest precision.
- Also lets a 44.1 kHz source go to `--dab` (`--resample 48`).
- Fix: `--help` printed garbage instead of "~50% at 192" in the `--block-bias` text
  (an unescaped `%` in the printf format).
- Fix: `--minsnr-clamp-hi` above 320 overflowed the internal Q30 value (0.8 x n/256 above
  1.0, then shifted left): every band got a garbage SNR requirement (CBR 96 kbps: error
  2.4 dB higher; VBR 64 kbps: file 2.8 times larger). The value is now limited to
  `1..320` (320 = 0 dB) and both SNR limits are capped inside the representable range.
  The help and the documentation had the direction of both clamps reversed: `-hi`
  moves the coarsest SNR a band is held to, `-lo` the finest it may demand.
- Docs (PL): README and the manual edited for clarity with an agreed glossary: "stock"
  becomes "fabryczny"/"domyślny"; "zagłodzona ramka" kept in quotes and explained;
  scale factor band, bias, noise floor, knee, masker and gate get the English term in
  brackets at first use; "podłoga" and "bramka" replaced by what they mean; no second
  person, no dashes. Code, headings and every number were checked unchanged by script.
  Manual EN: dashes removed, Table 1 empty cells marked n/a.
- Fix: `--sbr-stereo-mode 0` on stereo input crashed the encoder (access violation, no
  message, empty output file): SBR_MONO is not a mode of a two channel SBR element. The
  switch now takes `1..3`; mono input gets a mono high band from FDK by itself.
- Docs: group A reordered by use (`--verbose`, `--resample`, `--speech`,
  `--is-aggression`, `--unlock-bitrate`, `--uncap-bandwidth`). Group C now opens with the
  SBR layout and closes with `--core-cutoff`, rewritten from measurement: without SBR it
  is bit identical to `-w`; under SBR raising it is overwritten by the decoder's SBR and
  lowering it opens a hole, so the core is widened with `--sbr-start`. The recipe "HE-AAC
  v2 with a wider core" used `--core-cutoff 7500`, which narrowed the core and left a
  -34 dB hole at 7.7..8.9 kHz; it now uses `--sbr-start 14`. `--sbr-stereo-mode`: the
  default is 3 below 60 kbps stereo and 1 from 60 kbps, not always 3. Manual: section 14
  rewritten, Table 2 replaced with measured core bandwidths per start index at 32, 44.1
  and 48 kHz, IPD/OPD no longer described as unsupported, recipes and the symptom list
  updated (`--sbr-start` for a narrow core, `--sbr-softness` for a metallic top).
- Docs: the MS section ordered by use. `--side-bias` and `--side-knee` are the two
  levers, `--side-cutoff` the low bitrate trade, the rest (`--msmask`, `--msbands*`,
  `--ms-bias`, `--ms-precision`, `--mid-bias`) a laboratory, with measured mid/side
  error for each. `--is-admit` moved to the IS cards, `--mask-slope` to group D. The
  masking threshold switches of group D (`--ath-scale`, `--minsnr-*`, `--spread-mask`,
  `--reduce-clamp`, `--mask-slope`) moved to an advanced subsection, with CBR and VBR
  measurements. Corrected: `--ms-bias` saturates from about 8, not 1;
  `--side-knee` has no step, but in starved frames a small negative value already
  removes most of the side.


#### Short blocks: band switches follow the frequency

Every switch that takes a band number takes a LONG-block scalefactor band. A short
frame has its own, much coarser table (48 kHz: 49 long bands, 14 short ones, counted
from 0 in every window group). Up to beta.1 the numbers were compared with the short
table directly, so in short frames they pointed at other frequencies. Measured on
beta.1 (48 kHz, 96 kbps, MS mask read from the bitstream): `--msbands-lo 18
--msbands-hi 30` (2.5..9 kHz) left short frames with no MS at all, `--msbands 18`
(MS up to 2.5 kHz) gave short frames MS on every band up to 24 kHz, `--is-lo 33`
(10.5 kHz) removed IS from short frames.

- `--msbands`, `--msbands-lo/-hi`, `--is-lo/-hi`, `--is-force-lo/-hi`: at init each
  range is moved to the short table by frequency, from the real tables of the sample
  rate and frame length (also DAB+ 960/120). Each edge goes to the nearest short band
  edge in Hz, a tie goes up; a narrow range inside one short band takes that band.
  Window groups repeat the table, so `--short-groups` makes no difference.
- `--isbands`: counted per window group (the first groups of a transient frame used
  to use up the cap and leave the rest of the frame without IS), scaled to the short
  table (48 kHz: 6 long -> 2 short per group).
- `--hole-start` without `--hole-start-short` now also moves the short start to the
  same frequency (stock 15/3 maps onto itself at 44.1/48 kHz). An explicit
  `--hole-start-short` wins.
- `--hole-spread`: the value is for long blocks, short blocks get 2/3 of it (stock
  3 dB / 2 dB). `--hole-spread 3` used to change short frames.
- `--tns-start` also moves the TNS start of short blocks (stock 0 Hz there).
- `--side-cutoff` unchanged: both tables round up, so the short edge can only move up
  (less side destroyed).
- `--verbose` prints the long and the short range in bands and Hz.
- Without these switches, and on material without short frames, the stream is bit
  for bit the beta.1 stream (checked against the beta.1 binary).
- Test aid: `FRANKEN_BANDLOG=<file>` logs every written section per window group;
  `tools/msis/bandlog.py` reads it. `parse_ms.py` takes `--slo/--shi/--smax`.

#### `--side-cutoff`: level kept, intensity stereo above the cutoff on request

- Above the cutoff both channels play the mid, which carries less energy than the
  original whenever L and R differ: uncorrelated L/R lost 3 dB, partly correlated
  1..2 dB (measured at 96 kbps, beta.1). The mid is now raised to the energy of the
  original (at most +6 dB, only the side above its masking threshold counts). The
  band is now as loud as without the switch (-0.2 dB vs -0.2 dB, also on real music
  within 0.4 dB). Anti-phase content and the direction of a hard pan still cannot
  be carried by mid; that is what IS is for:
- IS above the cutoff stays when it is asked for: `--is-force-lo/-hi` as before, and
  now also FDK's own IS decision when its criteria are set (`--is-aggression`,
  `--is-corr-thresh`, `--is-lr-ratio`, `--is-min-sfbs`, `--is-admit` > 5). Single
  panned regions keep their direction (3..6 kHz source panned 4 dB: cutoff alone
  0 dB, with `--is-aggression 100` 3..4 dB).

#### New: `--pns-short <0|1>`

`--pns-above` also in short frames, but only in a frame coded as one group of 8
windows (`--short-groups 1`); with the stock grouping it changes nothing, bit for
bit. Measured on a sample with an attack in every frame: in smaller groups the
noise energies took 19% of the frame instead of 2% and the attack came out 2.6..6 dB
quieter, so they keep the spectrum. With one group: 24 kbps file -6%, the noise
band at the source level (+1.1 dB instead of -1.4 dB), at the cost of the band
smeared over the frame (pre-echo +1 dB, attack about 2 dB quieter).

#### `--block-bias`: the share of short blocks follows the slider

Up to beta.1 the switch scaled the attack threshold linearly (10 * 128/bias). Above
128 the threshold only went from 10 to 5: on eurodance 1.6% short frames at 128,
6.5% at 255. Just below 128 it jumped past most real attacks (at 64 already 0.3%), so
the middle seemed to do nothing. Measured on the encoder: the detector ratio is
1.2..1.5 in an ordinary frame of music and above 10 in only 2..3% of frames.

- The threshold now comes from a table calibrated on the encoder (six excerpts:
  acoustic stereo, eurodance, vocals, drums; 64 kbps; share of short frames from the
  bitstream), interpolated on a log scale: the share of short frames grows about
  evenly, roughly 12% of the frames per 16 steps above 128 (eurodance: 23% at 160, 57%
  at 192, 87% at 224, 100% at 240..255).
- `0` = no attack ever (long blocks only), `255` = an attack in every frame (short
  blocks only, silence included). Above 224 the quiet-frame gate of the detector is
  lowered too, so quiet passages follow.
- `128` and no switch: stock code path, bit for bit as beta.1 (18 encodes checked).
- Only the attack flag changes; the window sequence comes from the encoder's own
  transitions and channel sync. 300 encodes (0..255, LC/HE/HEv2, short groups 1/3,
  silence, sine, music): 0 illegal window transitions, 0 decoder errors.

#### DAB+: forced profile (`-p`) and `-b 8` fixed

- `--dab` with `-p 2`, `-p 5` or `-p 29` stopped with "aacEncEncode() failed" for
  `-p 2` (and with an assertion in the superframe writer for some other values): the
  plain profile number did not select the 960 transform. It is now mapped to the DAB+
  profile, and the stream is the same, bit for bit, as the automatic pick gives for
  that profile (`-p 5` and `-p 29` used to differ because of the edge extrapolator,
  which DAB+ does not use). Other `-p` values are refused, `-p 29` on a mono input
  too.
- `--dab -b 8` multiplied the bitrate by 1000 twice and aborted; it is now 8 kbps
  (120 bytes per superframe).
- A forced profile below the FDK tables (`-p 2` at 8 kbps, `-p 5` at 8 kbps stereo)
  explains the error and points to the automatic pick or `--unlock-bitrate`; the SBR
  floor note is no longer printed in DAB+ mode, where it did not apply.
- Checked: the switches added since v1.3.2 and the band switches in DAB+ (58 settings
  x LC 96 kbps on music and on attacks, HE-AAC 64 kbps, HE-AAC mono 48 kbps, HE-AAC v2
  32 kbps): every stream legal and decoded by faad2 behind dablin without an error.

### v1.4.0-beta.1 — side knee as an offset, side cutoff, IS gate, PNS that saves bits, bit statistics, documentation audit (plus the SBR estimator knobs, `--sbr-softness`, real low-bitrate unlock)

#### Core transients, pre-echo and spectral holes (LC / LD / ELD)

New, all opt-in (bit-identical to stock when unset): `--hole-ladder`,
`--avoid-holes`, `--hole-start`, `--hole-start-short`, `--hole-spread`,
`--preecho-rise`, `--preecho-depth`, `--attack-decay`, `--attack-floor`,
`--tns-thresh`, `--tns-start`, `--window-shape`, `--short-groups`.
Measured after decoding (`tools/core/`): `--hole-ladder 2` cuts zeroed bands
from 37% to 8% at LC 32 kbps / 17 kHz and to 1.7% at 40 kbps; `--window-shape 1`
lowers pre-echo by 3.4..6.1 dB on average. Stock switches the avoid-holes
shaping off below 20 kbps per channel, i.e. below 40 kbps stereo.
`--window-shape` also writes the forced shape into short blocks, which stock
always signals as sine. 396 extreme-value encodes across LC, HE-AAC, LD and ELD
decode with 0 errors in the Fraunhofer decoder and ffmpeg.

#### Coverage vs precision: what the bits are spent on

At a fixed bitrate the encoder can code every band coarsely, or fewer bands
cleanly. New switches move that balance; all are opt-in, the default output
is unchanged byte for byte.

- `--coverage -5..5`: one macro for the axis. + = everything filled, no holes,
  grainier; - = marginal bands dropped, what remains is cleaner. Explicit
  switches below win; `--verbose` prints the effective values.
- `--band-knee <dB>`: the `--side-knee` idea for every band of every channel.
  + keeps bands just under their threshold at the coarsest step; - gives up
  bands that only just clear it, and the bits go to the rest.
- `--quant-round <x>`: rounding offset of the quantizer (stock 0.405).
- `--pns-above <Hz>`: every long-block band above is noise-substituted, a high
  band "like SBR, without SBR". Works below the PNS bitrate gate.
- `--thr-hold <n>`: the masking model is refreshed only every n frames.
- `--scf-range <n>`: maximum scalefactor spread, i.e. how much the noise may be
  shaped across the spectrum.
- Measured at LC 32 kbps, 17 kHz: zeroed bands 51% (-5) .. 11.7% (stock) ..
  0.2% (+4) on mixed material; error in the bands that remain -4.75 dB (-5)
  against -1.96 dB (stock).
- Fix: `--side-knee +N` computed `threshold - energy` in 32 bits; for a silent
  band that overflowed (undefined behaviour), so the same source could encode
  differently depending on the compiler. Now 64 bit; silent bands are never
  rescued.
- `tools/core/core_measure.py` also reports `nsr_kept`, the error in the
  bands that were not cut.
- Documentation and help checked switch by switch against the parser (151 switches, 449
  examples from the README run on the binary). Fixed: `-1`, documented as "off" for 61
  switches, was rejected by the parser; it now means the same as leaving the switch out
  (also after an earlier value on the same line). `--ath-scale`, `--core-cutoff` and
  `--ms-bias` now reject out-of-range values instead of silently ignoring or clamping them.
  Corrected directions: `--sbr-freqscale 1` is the finest grid (12 bands per octave), `3`
  the coarsest; a higher `--pns-refpower` means FEWER PNS bands. Corrected numbers: the
  6144-bit frame ceiling at 48 kHz stereo is about 576 kbps, `--dab` keeps `-b` in kbps
  also with `--unlock-bitrate`. New cards for `--coverage`, `--band-knee`, `--quant-round`,
  `--pns-above`, `--thr-hold`, `--scf-range`, `--bit-stats`, `--enfac-patch` and the
  `--side-knee` diagnostics; MANUAL EN/PL rewritten for `--side-knee` and given a new part
  on extreme bitrates.
- `--pns-budget 0|1`: books the bits of PNS noise levels. Stock plans a PNS band as
  free; with a lot of PNS at a low rate the frame overflows and crash recovery cuts
  through the noise region into the coded spectrum (whole frames lose everything above
  ~1.5 kHz, a crackle in frame rhythm). Now the spectrum target is lowered by the noise
  cost and the cut gets those bits back. 24 kbps, PNS from 2.2 kHz: frames cut 42/766 to 1.
- `--pns-shared 0|1`: bands that both channels code as PNS carry one shared noise (1) or
  independent noise (0) uniformly, so the MS mask can fall back to 2 bits (28 to 2 bits
  per frame measured). Noise levels stay per channel, the format has no shared level.
- `--pns-hold <dB>`: neighbouring PNS bands within the tolerance share one level (delta
  0 = 1 bit). 24 kbps: noise levels 94 to 70 bits per frame at 6 dB.
- `--side-cutoff <Hz>`: from this frequency up every stereo band is coded as pure
  mid: MS forced, side zeroed, FDK's own IS decision cleared there (forced IS still
  wins). Bands that both channels code as PNS are left alone (their ms_used means
  correlated noise). Measured on acoustic music at 48 kbps, 5 kHz: side info 238 to
  222 bits per frame, mid error 0.7 dB lower, side above the cutoff 26..31 dB down,
  below it untouched. At 14 kbps the gain is small (FDK already zeroes most of the
  side there); the big lever at that rate is IS, see the next item.
- `--is-admit <x>`: FDK's IS gate (`bitrate / bandwidth < 5`) with x instead of 5;
  0 = never. At 14 kbps stereo on dense material `--is-admit 0` cuts zeroed bands
  from 85% to 70% (IS forces the per-band MS mask, ~22 bits, plus IS positions).
- `--side-knee` rebuilt as an offset on each side band's own margin (energy above
  threshold) near the coded/zeroed edge. It reads the threshold the rate control
  wanted before avoid-holes pinned the band at 0.97 dB, so the ~10% of side bands
  stacked on that value are spread by their real level. The old version dropped
  that whole crowd at once between -0.95 and -1.0 dB; now coded side bands per
  frame fall evenly (acoustic music, 96 kbps: 30.2 stock, 25.4 at -1, 18.1 at -6,
  1.1 at -24) and the + side grades the rescued bands instead of coding them all
  at the same coarse step. Streams with `--side-knee` set differ from 1.3.x.
  Diagnostic: `--side-knee-zone` (fade-out of the offset, stock 3x the knee) and
  `--side-knee-spread` (dB of negative knee to fully spread the rescued crowd).
- `--bit-stats`: counts where the bits of the written frames go (transport
  header, element and ics headers, MS mask, section data, scalefactors, PNS,
  TNS, spectrum, SBR/PS payload, fill), per channel where it applies, plus coded,
  zero, PNS and IS bands per frame. Printed when the encoder closes; the
  stream is unchanged byte for byte.
- `--enfac-patch <0|1>`: the flat-spectrum energy re-weighting (FDK
  `calcWeighting`, CBR and VBR). Stock applies it only to long frames whose
  audible spectrum is flat (noise-like); 0 never (every frame treated as
  tonal), 1 every long frame. Measured LC 14..32 kbps CBR: zeroed bands move by
  under 1 point, error within 0.1 dB, so in CBR it is a fine knob, not a lever.
  In VBR it changes the size (`-m 3`: 1 sets +29% on tonal-plus-noise, 0 sets
  -3% on dense noise).

#### Fixes found while rewriting the documentation

- The x100 SBR knobs (`--sbr-tran-thr`, `--sbr-tran-split`, `--sbr-tran-quiet`, the
  `--sbr-mh-*` scale knobs) silently ignored every value above 100: the Q31 factor was
  clamped to 1.0. They now scale in both directions; below 100 results are unchanged.
- `--is-aggression 100` wrapped a Q31 threshold negative and turned intensity stereo OFF.
  It now saturates, so 100 is the real maximum.
- `--max-bits-frame` (and `--min-bits-frame`) were overwritten later in the encoder setup
  and had no effect. They are now applied after the bitrate-mode block; with
  `--peak-bitrate` the explicit cap wins.
- `--sbr-noise-floor-offset` outside 0..11 aborted the encoder on an FDK assertion. The
  frontend now rejects it with an error.
- `--help` groups renumbered A..G; `--sbr-tran-quiet` / `--sbr-tran-dom` help text
  corrected (they act in ELD+SBR, LD/ELD do work, only ADTS is not possible for them).
- README rewritten as a guide plus a full switch reference (one card per switch, with
  examples in both directions and measured values) in English and Polish.

#### Decimal switches: fractions were silently dropped

`--side-bias`, `--side-knee`, `--mask-slope` and every other decimal switch were
read with `sscanf("%lf")`, which stops at a comma: `-0,23` became `-0` (knob off),
`-1,5` became `-1`. The side knobs were also stored in tenths of a dB. Now: `.`
or `,`, three decimal places (dB*1000 in the API: `AACENC_FRANKEN_SIDE_BIAS`,
`_SIDE_KNEE`, `_MASK_SLOPE` change unit from dB*10), anything else rejected,
range checked on the rounded value. Whole-number switches are strict too
(`--block-bias 128,5` is an error instead of 128). The audible step of
`--side-bias` near -0.97 dB is real: the side threshold crosses FDK's -0.97 dB
minimum-SNR line there.

Without the new switches the output is bit-identical to v1.3.2 (checked on HE,
HEv2, LC, LD, ELD and ELD+SBR at several rates and channel counts).

#### SBR inverse filtering and noise floor estimators exposed

Inverse filtering stays dynamic per band and frame; the new knobs bound, shift
and reweigh the estimator instead of freezing one level like `--sbr-invf`:
`--sbr-invf-min/-max`, `--sbr-invf-bias`, `--sbr-invf-table`,
`--sbr-invf-hyst`, `--sbr-invf-sbr-thr`, `--sbr-invf-orig-thr`,
`--sbr-invf-smooth`, `--sbr-nf-weight`, `--sbr-nf-weight-from`,
`--sbr-nf-smooth 0`. The speech parts of the estimators (decision table and
noise weight) are now separate from `--speech`.

New macro `--sbr-softness 0..7`: `2` = stock music estimators, `4` = stock
speech estimators (both bit-identical to stock), monotonic from hard to soft.
Measured from the bitstream on the decoder side (`tools/invf/`): mean invf level
0 -> about 3, flatness of the decoded SBR band 0.37 -> 0.42 (speech) and
0.40 -> 0.52 (tonal), frame-to-frame switching at the stock level. Shifting the
tonality borders made the decision flicker (0.2..0.29 changes per band and
frame), so the macro does not use them.

Finding: the noise weight is capped by the noise ceiling. With the tuning-table
ceiling (0.125) about 95% of the bands are already clipped, so the weight alone
often changes nothing; levels 5..7 of the macro also lift the ceiling.

#### `--unlock-bitrate` now really reaches below the HE-AAC 16 kbps floor

The old warning and documentation said HE-AAC / HE-AAC v2 have a hard ~16 kbps
floor and that the flag only helps AAC-LC. That was wrong: 16 kbps is just the
lowest row of the SBR tuning table for 48 kHz stereo. With the flag:

- HE-AAC, HE-AAC v2 and ELD+SBR below the table reuse its lowest row;
- LD/ELD no longer require 8 kbps per channel nor reserve a 208-bit transport
  guess (the real header overhead is still enforced), and use the narrowest
  bandwidth below the LD/ELD bandwidth table;
- frames that cannot fit their own side information overshoot (reservoir
  clamped at zero, bounded quantisation loop) instead of aborting the encode;
- below the SBR payload estimate the encoder fails cleanly instead of
  hitting an assertion.

Lowest `-b` that encodes at 48 kHz, now (stock): HE-AAC stereo 5050 (16008),
mono 2505 (12008), HE-AAC v2 3122 (12008), LD 8291 (19500), ELD 9062 (19500),
ELD+SBR about 10500..11300 (36008..60008), LC 2659 (9750). All streams decode
with 0 errors in the Fraunhofer decoder. Dense passages can come out above
`-b` (e.g. HE-AAC stereo `-b 6000` = 7290 bps real on music).

### v1.3.2 — MS/IS fixes, SBR noise bands, SBR header interval for streaming

Bug-fix release plus one streaming switch (`--sbr-header-time`). No tuning changes. Without `--msbands`,
`--msbands-lo/-hi` and `--is-force-lo/-hi` the output is bit-identical to v1.3.1;
`--sbr-noise-bands 4|5` is now rejected.
Reported by Patryk (review of the MS/IS band logic), confirmed by reading the
bitstream back on the decoder side (`tools/msis/parse_ms.py`).

#### MS band range ignored every short-block group but the first

The MS band range compared the flattened grouped index (`sfb + sfboffs`) with
the switch value. In a short block every window group restarts at frequency
band 0, so groups 1..n fell outside any range and lost MS completely. The range
is now compared with the band index inside the group, the same convention the
IS range (`--is-lo/-hi`) already used. Affects `--msbands`, `--msbands-lo` and
`--msbands-hi`. Long blocks have a single group, so they are unchanged.
Measured on a transient-heavy correlated sample: MS in groups >0 went from 0 to
2256 bands for `--msbands-lo 0 --msbands-hi 8`.

#### `--msbands-lo/-hi` did not stop the MS_MASK_ALL promotion

When L/R stays on fewer than 9 bands, FDK promotes the frame to "MS on all
bands" and applies the M/S butterfly to every band. `--msmask` and `--msbands`
blocked that promotion, the lo/hi range did not, so a range that switched off
a few top bands got MS back on them. Measured: `--pns 0 --msbands-hi 34` gave
47 of 131 frames with MS on all bands; now 0.

#### Forced IS could abort the encoder

`--is-force-lo/-hi` on neighbouring bands panned hard in OPPOSITE directions
produced an intensity-position delta above 60, outside the scalefactor
Huffman book, and the encoder stopped on an assertion (truncated output).
Stock FDK never gets there because its merging pass smooths the positions;
forced IS skips that pass. The delta is now clamped to the codable range, which
only softens the pan in that band. A no-op whenever every delta is in range.

The energy check in forced IS stays `left OR right`: the division uses the L+R
(or L-R) energy and fails only when BOTH channels are silent. The code comment
that claimed otherwise is corrected.

#### `--sbr-noise-bands` accepted values the SBR header cannot carry

`bs_noise_bands` is a 2-bit field of the SBR header, so only 0..3 can be
signalled: 0 = a single noise band, 1/2/3 = about 1/2/3 bands per octave. The
switch accepted 1..5. For 4 and 5 the header writer masked the value to 2 bits
(4 -> 0, 5 -> 1) while the noise-floor estimator kept the full value, so the
decoder derived a different number of noise bands than the encoder wrote and
misparsed the rest of the SBR data (ffmpeg: `noise_facs_q ... is invalid`,
`env_facs_q ... is invalid`, around 10 errors per 4 s at 32-48 kbps). The
frontend now accepts -1 and 0..3, and the library ignores anything above 3
regardless of the frontend. Reported by Patryk. Values 0..3 are bit-identical
to v1.3.1; 0 was always legal and is now documented.

#### SBR header interval: no one-second ceiling, new `--sbr-header-time`

Stock FDK clamps any explicit SBR header period to "at least once per second"
(`sampleFreq / frameSize` frames), so `--sbr-header-period 999` still produced
a header every ~0.98 s and a stream operator could not ask for, say, one valid
SBR header every 3.5 s. Measured on the bitstream: before, the longest gap was
0.981 s whatever the value; now `--sbr-header-period` is taken literally.

New `--sbr-header-time <s>` (decimal seconds, 0.01..600) sets the same
interval as a time and wins over the frame count. It covers:

- HE-AAC and HE-AAC v2: the in-band `sbr_header`, in every transport (ADTS,
  LATM/LOAS, M4A).
- AAC-ELD with SBR (`-p 39 -L 1`): ELD has no in-band SBR header, its
  `ld_sbr_header` lives only in the stream config. The switch therefore sets
  the LATM/LOAS StreamMuxConfig repetition period (`-f 6/7/10`), which stock
  takes from an 8-bit field (at most ~2.7 s at 48 kHz). The LATM frame
  counter was widened so periods above 255 frames work.

Decoder-side check (join mid-stream at 20-25 random points, measure when the
SBR band appears): with `--sbr-header-time 3.5` the longest wait was 3.41 s
(HE-AAC, ffmpeg), 3.46 s (HE-AAC v2, ffmpeg) and 3.52 s (ELD, Fraunhofer
decoder; ffmpeg cannot decode ELD with SBR).

The encoder can still insert an extra SBR header on its own when the dynamic
crossover changes; that is a correctness requirement, not a scheduling one, so
the interval is a maximum gap between scheduled headers, not an exact cadence.

`--verbose` now reports the SBR header period in real milliseconds: it
ignored the SBR ratio and showed half the true time for dual-rate HE-AAC
(213 ms instead of 427 ms at 48 kHz). For ELD it reports the config period.
Without the two switches the output is bit-identical to v1.3.1.

#### Tests

`make check` gained 13 checks: the MS range in short-block groups (3), the
promotion (2), forced IS with alternating panning (2), `--sbr-noise-bands` 4/5
rejected (2) and 0..3 decodable (4), plus opt-in identity checks. Every
regression check fails on the v1.3.1 binary and passes on v1.3.2.

### v1.3.1 — Parametric Stereo phase: two bitstream fixes

Bug-fix release. No new switches, no tuning changes: without `--ps-ipd` the
output is bit-identical to v1.3.0. If you use `--ps-ipd` / `--ps-opd`, upgrade —
v1.3.0 could produce streams that decoders reject.

Both defects were in the phase extension added in v1.3.0, and both only showed
up in frames carrying more than one PS envelope, which is why the v1.3.0 test
matrix missed them: `envelopeReducible` collapses most musical material to a
single envelope.

#### The phase extension could produce an undecodable stream

Symptom: ffmpeg reported `ps extension overflow` followed by `illegal icc`, and
faad truncated its output.

1. **Delta-time referenced the wrong envelope.** In delta-time mode the decoder
   predicts every envelope after the first from the *previous envelope of the
   same frame*, and only envelope 0 from the previous frame. The writer kept its
   reference pinned to the previous frame for all envelopes, so from the second
   envelope on the Huffman deltas were resolved against a different base than
   the decoder used. The stock writer already handles this correctly for IID and
   ICC; IPD/OPD simply missed the step.

2. **Phase data was written without signalling IID.** The number of IPD/OPD
   parameter bands is not carried in the extension — the decoder derives it from
   `iid_mode`, and only inside the header branch guarded by `enable_iid`. FDK
   enables IID from a loudness-difference heuristic, so on material whose
   channels sit at nearly the same level (near-antiphase content being the
   extreme case) it legitimately signalled `enable_iid = 0`. The decoder was
   then left with zero phase bands: it consumed fewer bits than were written,
   and — worth stating plainly — discarded the transmitted phase entirely, so
   those bits bought nothing. When IPD is active, IID is now signalled.

#### Verification

- `make check` 149/149 (was 143); three of the new cases are regression tests
  for the above, each verified to FAIL on the pre-fix binary — a test that also
  passes on the broken code proves nothing.
- The decoder-error detector in the test suite was itself blind: it grepped for
  `error|invalid|exceeds`, which matches NEITHER string ffmpeg prints for a
  broken PS stream. That is how a corrupt bitstream passed 143/143.
- Sweep of 1200 combinations (samples × bitrate × `--ps-bands` × envelope modes
  × `--ps-noenv-skip`): zero decoder errors, checked against ffmpeg and faad.
- Measured on real material, including recordings with genuine production phase
  problems: phase accuracy is unchanged by these fixes (they are correctness
  fixes, not tuning), while OPD itself remains worth having — the absolute phase
  error of each channel improves by up to 33 degrees on a track with one channel
  inverted, and by ~9 degrees on material through a cheap radio stereo enhancer.

### v1.3.0 — Parametric Stereo phase (IPD/OPD), PS resolution, SBR detectors

Four groups of work, all opt-in: with no new switches the output is byte-identical
to v1.2.0. The headline item is that Parametric Stereo now transmits **phase** —
something stock FDK never did, despite already computing the data it needs.

#### Parametric Stereo resolution (bands × envelopes)

- **`--ps-bands <10|20>`** — number of PS stereo bands, i.e. the *frequency*
  resolution of the stereo parameters. Stock FDK derives this from the bitrate
  alone (`psTuningTable`, `sbrenc_rom.cpp:899`), so from 22 kbps upwards only 20
  bands were ever reachable.
- **`--ps-env <1|2|4>`** — PS parameter envelopes per frame, i.e. the *time*
  resolution of the stereo parameters. Above 36 kbps stock FDK always picks 4.
- **`--ps-env-reduce <0|1>`** — `0` disables the automatic envelope-halving loop
  (`envelopeReducible`, `ps_encode.cpp:278`), which silently collapses 4
  envelopes to 1 whenever neighbouring envelopes look similar. Without this,
  `--ps-env 4` is a no-op at 48 kbps: the output is byte-identical to stock.
- **`--ps-noenv-skip <0|1>`** — `0` forbids parameter-less PS frames. Stock FDK
  may emit up to `MAX_NOENV_CNT` (10) consecutive frames carrying no stereo
  parameters at all when successive IID/ICC sets look similar.
- `--verbose` now reports the effective PS band count and envelope count, plus
  the state of both hidden heuristics above.
- Measured (HE-AAC v2, 48 kbps, 4 s probe with a 0.25 Hz panorama sweep and
  alternating L/R transients, panorama trajectory vs. source): stock 0.177 RMS
  error / 0.9655 correlation; `--ps-env 4 --ps-env-reduce 0` 0.117 / 0.9944 — a
  34 % reduction in panorama error at essentially the same file size.
- Documentation now states honestly that the ICC rotation mode
  (`--ps-icc-mode`) is signalling-only, and corrects the previous claim about
  IPD/OPD: the encoder already computes the imaginary part of the L/R
  cross-spectrum and discards the phase, and the IPD/OPD Huffman tables and
  bitstream writers already exist in `ps_bitenc.cpp`.
- Source areas: `libSBRenc/src/sbr_encoder.cpp` (PS tuning-table override),
  `libSBRenc/src/ps_encode.cpp` (envelope reduction, parameter-less frames).
- `make check` 106/106 PASS, no-regression ADTS bit-identical to stock.

#### SBR transient / missing-harmonics detectors + noise ceiling

- **`--sbr-tran-thr <n>`** — scales the master SBR transient threshold. Measured
  as the strongest knob in this group: on a 24-attack percussive probe, high-band
  pre-echo (energy appearing *before* an attack that is absent from the source)
  drops from 1.610 to **0.041** at 32 kbps and from 1.186 to 0.059 at 64 kbps when
  set to 40. Monotonic, saturates below 60, byte-identical to stock at 100+.
- **`--sbr-tran-peak <n>`** — raw x100 replacement for the hardcoded 0.90
  "peakiness" constant (`tran_det.cpp:699,715`).
- **`--sbr-tran-split <n>`** — scales the FIXFIX envelope-split threshold.
- **`--sbr-tran-quiet` / `--sbr-tran-dom`** — exposed for completeness but
  **no-ops in this build**: they live in the fast transient detector, only reached
  under AAC-LD/ELD, and `-p 23` / `-p 39` fail to initialise here *including in the
  stock binary* (inherited limitation). Documented as such rather than advertised.
- **Missing harmonics detector** (`mh_det.cpp` `paramsAac`/`paramsAacLd`, both
  `const` — cloned into a writable block only when a knob is set):
  `--sbr-mh-tone`, `--sbr-mh-diff`, `--sbr-mh-decay-orig`, `--sbr-mh-decay-diff`,
  `--sbr-mh-sfm-sbr`, `--sbr-mh-sfm-orig`, `--sbr-mh-maxcomp`,
  `--sbr-mh-deltatime`. These govern whether SBR fabricates synthetic harmonics —
  too eager gives whistling/metallic artefacts, too conservative dulls bells and
  cymbals. `invThresHoldTone` is moved inversely to stay consistent with
  `thresHoldTone`.
- **`--sbr-noise-max <6|3|-3>`** — ceiling on injected SBR noise (1.0 / 0.5 /
  0.125), i.e. the "air vs hiss" limit. Already a config field in FDK
  (`sbr_encoder.cpp:526`), previously only settable from the bitrate tuning table.
- **Fixed an overflow bug caught by the new test**: computing a x1.00 multiplier as
  `((INT64)100 << 31) / 100` yields 2^31, which does not fit a signed 32-bit
  `FIXP_DBL` and wrapped negative — so a "neutral" knob silently altered the audio.
  Neutral values now return the constant untouched, and multipliers are clamped.
- **New `make check` section `neutral-identity`**: every knob at its neutral value
  must produce byte-identical output to no knob at all. This is what caught the
  overflow above.
- `--verbose` lists each of these overrides when set.
- `make check` 129/129 PASS, no-regression ADTS bit-identical to stock.

#### IPD: inter-channel phase in Parametric Stereo (`--ps-ipd`)

- **`--ps-ipd <0|1>`** — transmit IPD (inter-channel phase difference) in the PS
  extension. Stock FDK hardcodes phase to zero ("IPD OPD not supported right
  now"), yet it already accumulates the full complex L/R cross-spectrum and uses
  only its magnitude for ICC. IPD is that discarded angle: `fixp_atan2(pwrCi,
  pwrCr)` (already in libFDK), quantised to the 8 defined pi/4 steps.
- Coded for the lower **5 / 11 / 17** parameter bands, NOT the 10 / 20 / 34 used
  by IID/ICC. Verified against ffmpeg's independent decoder
  (`aacps_common.c: nr_iidopd_par_tab[] = {5, 11, 17, ...}`); it also coincides
  with the border already used by `calculateICC()`. The writer previously used
  `getNoBands()` (10/20) — corrected via a dedicated `getNoIpdOpdBands()`.
- **Fixed two latent stock-FDK bugs** that only surface once the extension carries
  data: (a) `encodeIpdOpd()` wrote the `enable_ipdopd` flag without counting its
  bit, so the sizing pass returned one bit less than the write pass, desyncing the
  extension byte count and tripping the SBR alignment assertion
  (`env_bit.cpp:229`); (b) the extension was emitted only in frames carrying a PS
  header, although `enable_ext` is a header field that stays in force — the
  decoder kept expecting data that was not there ("Expected to read N PS bits
  actually read M", then "illegal icc").
- Phase delta-coding now wraps modulo 8, matching the decoder, which accumulates
  the Huffman deltas and masks with `&0x07`. Without the wrap every negative phase
  step was rejected as an error.
- MEASURED on probes where stereo is carried purely by inter-channel delay (equal
  levels, so IID/ICC cannot represent it), phase error inside the IPD range
  (60-690 Hz) versus source: 0.25 ms delay 35.0 -> **14.9** deg, 0.4 ms 56.1 ->
  **24.9** deg, identical at 32 and 48 kbps. At 0.7 ms it stops helping (67.4 ->
  71.8) because by ~690 Hz that delay has swung through nearly +/-180 deg and a
  45 deg grid can no longer resolve it — the classic ITD phase ambiguity, a limit
  of the representation rather than a defect.
- OPD deliberately left at zero: the decoder couples the pair (OPD rotates both
  downmix paths, IPD shifts only the second). The physically motivated
  OPD = IPD/2 was measured and did not reconstruct source phase either, so the
  defined neutral is kept rather than guessing at the decoder's model.
- COMPATIBILITY verified, not assumed: byte-identical output with the switch off
  (and with `--ps-ipd 0`), clean decode by ffmpeg **and** faad at 24/32/48/64 kbps
  with it on. The payload sits in a length-prefixed extension, which decoders
  without phase synthesis may skip — explicitly permitted by ISO/IEC 14496-3.
- `make check` 137/137 PASS, including new opt-in and parser-error assertions.

#### OPD: the other half of the phase pair (`--ps-opd`)

- **`--ps-opd <0|1>`** (default on with `--ps-ipd 1`) — transmit OPD, the phase of
  the LEFT channel relative to the MONO downmix. The decoder rotates the
  downmix-fed paths by `OPD` and the others by `OPD - IPD`; the difference always
  reduces to IPD, but the placement of that rotation relative to the downmix is
  set by OPD alone.
- Derived from existing accumulators, no new analysis and no downmix signal
  needed: the downmix is `L + R`, so `sum(L*conj(L+R))` has real part
  `pwrL + pwrCr` and imaginary part `pwrCi`, giving
  `OPD = atan2(pwrCi, pwrL + pwrCr)`. Reduces analytically to `IPD/2` in the
  equal-level case, but stays correct when the channels differ in level.
- **Corrects a wrong verdict in the previous release.** It claimed measurement
  showed `OPD = IPD/2` did not help. That measurement was scoped to
  `arg(L) - arg(R)` — a metric structurally blind to OPD, since OPD cancels in the
  inter-channel difference by construction. Re-measured on each channel's ABSOLUTE
  phase against the source (60-690 Hz), OPD clearly helps:

  | delay | OPD=0 (L / R) | OPD computed (L / R) |
  |---|---|---|
  | 0.25 ms | 14.7 / 20.1 | 14.4 / **17.3** |
  | 0.4 ms | 23.0 / 28.0 | **15.5** / **17.8** |
  | 0.7 ms | 48.1 / 56.2 | **40.9** / **36.3** |

  Average gain +6.4 deg, +13 deg at 0.7 ms — including the 0.7 ms case where the
  plain IPD difference metric showed no benefit at all, because inter-channel phase
  had already exceeded what a 45 deg grid resolves while absolute phase had not.
  At `OPD = 0` the channels are always lopsided (14.7 vs 20.1, 48.1 vs 56.2); with
  OPD they converge, which is precisely the asymmetry the parameter exists to fix.
- `--ps-opd 0` retained so the two can be A/B'd on identical material.
- Compatibility unchanged and re-verified: byte-identical with IPD off, clean
  decode by ffmpeg and faad at 24/32/48/64 kbps with both parameters on.
- New assertions: `--ps-opd 0` and `1` must produce DIFFERENT streams (proving OPD
  reaches the bitstream) and both must decode without parser errors.
- `make check` 143/143 PASS.

### v1.2.0 — DAB+ digital-radio output

- **New `--dab` output mode** (ETSI TS 102 563): emits a DAB+ super-frame stream
  (960-sample transform, 120 ms super frame, firecode + Reed-Solomon RS(120,110)
  over GF(256)) as a raw `.dabp` stream for multiplexers such as `odr-dabmux`.
- Profile **auto-selected** from bitrate and channel count (AAC-LC / HE-AAC /
  HE-AAC v2), like `odr-audioenc`; override with `-p`. Supports 32/48 kHz,
  mono/stereo, bitrate 8..192 kbps in 8 kbps steps.
- **`--dab-label "<text>"`** — static DLS (Dynamic Label Segment) carried as
  X-PAD in the super frame; shown by DAB+ receivers as the station name / title.
- Verified against an independent faad2 decoder (dablin) across all nine
  sr×channel×profile combinations; LC output is bit-identical to `odr-audioenc`.
- Windows x64 + x86 binaries build unchanged (mingw-w64). Without `--dab` the
  encoder is bit-identical to the previous release.

## Modifications

Modified by **Michał Dziwisz** in 2026 (subject-matter consultant: Patryk
Faliszewski). The modifications add command-line control over encoder-internal
decisions that upstream FDK keeps hardcoded. Summary of changed FDK source areas
(all changes gated behind sentinels, so default behavior stays bit-identical to
stock FDK):

- `libAACenc/src/franken.{h,cpp}` — new central override module (`g_franken`).
- `libAACenc/include/aacenc_lib.h` — new `AACENC_PARAM` values in the `0xF0xx` range.
- `libAACenc/src/aacenc_lib.cpp` — SetParam/GetParam dispatch for the new params.
- `libAACenc/src/ms_stereo.cpp` — per-band MS control, MS band range, MS precision, MS bias, plus a mid bit-split bias (`--mid-bias`).
- `libAACenc/src/intensity.cpp`, `psy_configuration.cpp` — intensity-stereo gating / aggression, IS band range (`--is-lo/--is-hi`) and forced IS (`--is-force-lo/--is-force-hi`); MusePack-style per-band min-SNR control (`--minsnr-scale`, `--minsnr-clamp-hi/-lo`).
- `libAACenc/src/adj_thr.cpp` — optional removal of the 29 dB threshold-reduction clamp (`--reduce-clamp`); Masking-Slope-Adaptation start-threshold shift (`--mask-slope`).
- `libAACenc/src/sf_estim.cpp` — per-channel side bit-split at the coded-vs-zeroed decision: side masking-threshold shift (`--side-bias`) and coded↔zeroed knee shaping (`--side-knee`).
- `libAACenc/src/bandwidth.cpp` — bandwidth cap lift (`--uncap-bandwidth`).
- `libAACenc/src/pnsparam.cpp` — PNS start / force-pns.
- `libAACenc/src/aacenc.cpp` — TNS mask, quasi-VBR knobs, lower-bitrate unlock.
- `libAACenc/src/aacenc_tns.cpp` — TNS order cap.
- `libSBRenc/src/sbr_encoder.cpp` — SBR density/grid/stereo-mode/noise-floor + SBR header period (`--sbr-header-period`, streaming SBR sync) + effective-value readback.
- `libSBRenc/src/invf_est.cpp` — forced SBR inverse filtering.
- `libSBRenc/src/ps_encode.cpp` — PS IID/ICC overrides.
- Frontend (`nu774/fdkaac`: `main.c`, `aacenc.c`, `aacenc.h`) — CLI switches,
  parsing, `--verbose` dump, encoder-identifier tag rebranded to "PompAAC based on…",
  MP4 metadata switches (`--no-tool-tag`, `--minimal-moov`).
- DAB+ output (`--dab`, `--dab-label`) — new digital-radio output mode per ETSI TS
  102 563: 960-sample transform, 120 ms super frame with firecode (Fire CRC) and
  Reed-Solomon RS(120,110) over GF(256), raw `.dabp` stream for `odr-dabmux`.
  Profile (AAC-LC / HE-AAC / HE-AAC v2) auto-selected from bitrate and channel
  count (like `odr-audioenc`); 32/48 kHz, mono/stereo, bitrate 8..192 kbps in 8k
  steps. `--dab-label` carries a static DLS (Dynamic Label Segment) as X-PAD.
  Streams verified against an independent faad2 decoder (dablin); LC output
  bit-identical to `odr-audioenc`. Gated: without `--dab` behavior is unchanged.

The term "Fraunhofer FDK AAC Codec Library for Android" is, per the license,
replaced by "Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library for Android" wherever the modified library is referred to.
