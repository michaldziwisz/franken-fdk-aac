# CHANGES

This is a **Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library** (per section 2 of the FDK AAC license, see `NOTICE.fdk-aac`).

## Releases

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
