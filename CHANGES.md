# CHANGES

This is a **Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library** (per section 2 of the FDK AAC license, see `NOTICE.fdk-aac`).

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
- `libAACenc/src/adj_thr.cpp` — optional removal of the 29 dB threshold-reduction clamp (`--reduce-clamp`).
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

The term "Fraunhofer FDK AAC Codec Library for Android" is, per the license,
replaced by "Third-Party Modified Version of the Fraunhofer FDK AAC Codec
Library for Android" wherever the modified library is referred to.
