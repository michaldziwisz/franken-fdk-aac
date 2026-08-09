# CHANGES

This is a **Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library** (per section 2 of the FDK AAC license, see `NOTICE.fdk-aac`).

## Releases

### Unreleased — SBR transient / missing-harmonics detectors + noise ceiling

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

### Unreleased — Parametric Stereo resolution (bands × envelopes)

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
