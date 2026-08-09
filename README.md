1|# Franken FDK AAC — a laboratory/"geek" AAC encoder (FDK)
2|
3|Built on top of **libfdk-aac 2.0.3** (mstorsjo/fdk-aac) + the
4|**nu774/fdkaac 1.0.2** frontend, with bolted-on CLI switches that expose
5|the normally hardcoded, internal decisions of the FDK encoder. It serves
6|extreme debugging and experimentation with AAC/HE-AAC/HE-AAC v2.
7|
8|Binaries (static, no external DLLs):
9|- `fdkaac-franken-x64.exe` — Windows 64-bit (PE32+)
10|- `fdkaac-franken-x86.exe` — Windows 32-bit (PE32)
11|
12|## Download
13|
14|Prebuilt Windows binaries are published as GitHub Releases, built and hosted by
15|GitHub (no login needed to download):
16|
17|**→ https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**
18|
19|Grab `franken-fdk-aac-x64-vX.Y.Z.zip` (64-bit) or the `x86` one; each zip holds
20|the `.exe` plus the full documentation. There are no binaries committed to the
21|repo — see "Build" at the bottom for building it yourself.
22|
23|
24|All original nu774 frontend options (`-p/--profile`, `-b/--bitrate`,
25|`-m/--bitrate-mode`, `-w/--bandwidth`, `-a/--afterburner`, `-s/--sbr-ratio`,
26|`-f/--transport-format`, tagging, etc.) work as before. Below are only the
27|NEW switches. See also `fdkaac-franken-x64.exe --help`.
28|
29|NOTE: this is an "I-know-what-I'm-doing" tool. Most of these parameters
30|deliberately let you go beyond what the FDK automation does — you can
31|knowingly wreck the stereo image, the bandwidth, or the quality with them.
32|The sentinel `-1` (or `0` for `--core-cutoff`) = "leave FDK's default behavior".
33|
34|Author: Michał Dziwisz. Subject-matter consultant: Patryk Faliszewski.
35|Built on open source: libfdk-aac (Fraunhofer IIS) + the nu774/fdkaac frontend.
36|
37|---
38|
39|## Option groups overview (grouped the same way in `--help`)
40|
41|The options are ordered by topic, roughly from easiest to most geeky. The same
42|order applies in `--help` (groups A–E) and in the sections below:
43|
44|- **A. Start here (consumer):** `--verbose`, `--is-aggression`, `--speech`,
45|  `--uncap-bandwidth`, `--unlock-bitrate` — sections 0, 1, 2, 10.
46|- **B. Stereo:** MS (`--msmask`, `--msbands`, `--msbands-lo/-hi`, `--ms-bias`,
47|  `--ms-precision`), IS (`--is`, `--isbands`, `--is-*`), PS (`--ps`, `--ps-iid-quant`,
48|  `--ps-icc`, `--ps-icc-mode`) — sections 1, 4, 8.
49|- **C. Bandwidth and SBR:** `--core-cutoff`, `--sbr-*` — sections 2, 3.
50|- **D. Masking / noise / detail:** `--ath-scale`, `--spread-mask`, `--tns-*`,
51|  `--pns`, `--pns-start`, `--force-pns` — sections 5, 6.
52|- **E. Blocks and bitrate:** `--block-bias`, `--vbr-reservoir`, `--peak-bitrate`,
53|  `--max-bits-frame`, `--min-bits-frame`, `--bitres-mode` — sections 7, 9.
54|- **F. DAB+ digital radio:** `--dab`, `--dab-label` — section 14.
55|
56|Tip: at the end, `--verbose` prints a "franken overrides applied" section —
57|exactly those switches which, in a given run, deviate from pure FDK.
58|
59|---
60|
61|## 0. Diagnostics
62|
63|### `--verbose`
64|Before encoding, it dumps to stderr the REAL parameters chosen by the encoder
65|(not just your overrides): AOT, bitrate/mode, samplerate, channel-mode, the
66|EFFECTIVE core cutoff in Hz, afterburner, transport, signaling, plus the state
67|of the coding tools chosen by the encoder (TNS on/off, PNS on/off, Intensity
68|stereo on/off, MS stereo on/off). When SBR is active: sbr-ratio + effective
69|start/stop freq index, freq scale, noise bands, amp res. At the end, a list of
70|your overrides (-1/0 = not set, left to the encoder). Perfect for learning the
71|starting point (e.g. the default HE-AAC v2 48k cutoff = 8613 Hz).
72|
73|---
74|
75|## 1. Joint stereo — MS / IS / independent stereo
76|
77|By default FDK itself decides per-band about MS (mid/side) and IS (intensity).
78|Here you can override these decisions and force extreme configurations.
79|
80|| Switch | Values | Default | Description |
81||---|---|---|---|
82|| `--msmask <n>` | -1 auto, 0 off, 1 on | -1 | Force MS: `0` = all bands L/R (completely independent stereo), `1` = MS on all bands. |
83|| `--msbands <n>` | -1 no limit, 0..N | -1 | Maximum SFB number that may use MS (takes the N LOWEST bands). Above that — MS disabled. |
84|| `--msbands-lo <n>` | -1 off, 0..N | -1 | START (lowest SFB band number) of the range in which MS is allowed. |
85|| `--msbands-hi <n>` | -1 off, 0..N | -1 | END (highest SFB band number) of the MS range. Used together with `--msbands-lo` as a FROM–TO pair. Bands outside this range go pure L/R. |
86|| `--ms-precision <n>` | 256..no limit (Q8) | -1 (off) | *(Very experimental — you probably want `--side-bias` instead.)* Scales the precision of MS bands globally (both mid and side together), LAME `-q` style. 256=no change, 384~1.5x, 512~2x. In practice its reach is limited: above ~600-800 the threshold hits FDK's hard floor and under CBR bits are only shifted between bands, so the sound stops changing. Superseded for stereo tuning by `--side-bias`/`--side-knee`, which act per-channel exactly where it matters. |
87|| `--mid-bias <n>` | 256..no limit (Q8) | -1 (off) | *(Very experimental — rarely needed.)* `>256` RAISES the mid (L+R) threshold after the MS butterfly to free bits from mid for side. The cleaner, better-measured way to shift the mid↔side balance is `--side-bias` (which pulls from the same budget from the side end). Kept for completeness. 256=off. |
88|| `--side-bias <dB>` | -24.0 .. +50.0 | 0 (off) | **The main stereo-quality knob.** Shifts the SIDE (L−R) channel's masking threshold on MS-coded bands, at the exact point where FDK decides whether a scalefactor band is coded or dropped (`energy > threshold` in `sf_estim.cpp`). Sign = EFFECT: **`+` steers MORE bits to the side channel** (lower threshold → fewer side bands zeroed, the survivors quantized finer → cleaner stereo width, reverb tails, ambience), at the mid channel's expense; **`−` deliberately DEGRADES the side** (raises the threshold → side bands drop out → narrower, more mono image). This is the very same energy-vs-threshold decision LAME rides for its bit allocation and MusePack drives with `--ms` — nothing exotic. Because it is a fixed-budget tradeoff, at low bitrate the mid channel audibly gives up bits; that is expected, not a bug. Sane range **+3 .. +9** for "more spacious", negative only for extreme/artistic low-bitrate mangling. Measured on real material at 96 kbps: `+9` lifts side error in the 4–8 kHz region while mid degrades a few dB. 0 = off (bit-identical to stock). |
89|| `--side-knee <dB>` | -24.0 .. +50.0 | 0 (off) | Shapes HOW SHARPLY a side band flips between "coded" and "zeroed" at the threshold. Stock FDK is a hard cliff: the instant `energy ≤ threshold` the whole band is dropped to zero. **`+` = SOFT knee**: bands sitting up to N dB *below* the threshold are still kept (coded at the coarsest scalefactor) instead of dropped, so the side fades out gradually rather than switching off — smoother decay of reverb/air. **`−` = HARD knee**: bands that only just clear the threshold (within N dB *above* it) are forced to zero anyway, cutting the side off early — leaner, more aggressive. Ortogonal to `--side-bias` and combines with it. Sane range **+3 .. +6**. 0 = off. |
90|| `--mask-slope <dB>` | -24.0 .. +50.0 | 0 (off) | Global (mid **and** side) tuning of FDK's **Masking-Slope-Adaptation** — a NON-masking heuristic (`adj_thr.cpp`) that relaxes the required SNR for scalefactor bands whose energy sits far below the frame average (stock: more than ~10 dB below), i.e. it deliberately starves very quiet bands to save bits. This knob shifts that "how far below average before I stop caring" threshold. **`+` raises it → fewer quiet bands starved → more detail in quiet passages, reverb tails, decays** (costs bits); **`−` lowers it → quiet bands starved harder → leaner, hollower, more bits for the loud stuff**. Same family as `--side-bias` but applied to both channels and keyed on energy-vs-average rather than the MS threshold. Subtle on dense material (it only touches the quietest bands); most audible on sparse/reverberant content. Sane range **±6 .. ±12**. 0 = off. |
91|| `--is <n>` | -1 auto, 0 off, 1 on | -1 | Intensity stereo globally on/off. |
92|| `--isbands <n>` | -1 no limit, 0..N | -1 | Maximum number of SFBs that may use intensity. Above that — coded normally. |
93|| `--is-aggression <0..100>` | 0..100 | -1 (off) | CONSUMER slider: how hard the encoder should push intensity stereo. Start here, leave the advanced `--is-*` alone. |
94|| `--is-min-sfbs <n>` | -1 def(6), 0..N | -1 | (advanced) Min. number of contiguous SFBs before IS turns on. |
95|| `--is-corr-thresh <n>` | -1 def(243), Q8 | -1 | (advanced) L/R correlation threshold for IS in Q8 (256=1.0). |
96|| `--is-lr-ratio <n>` | -1 def(179), Q8 | -1 | (advanced) L/R energy balance threshold for IS in Q8 (256=1.0). |
97|| `--is-lo <sfb>` | -1 off, 0..N | -1 | Allow intensity stereo ONLY from this SFB upward. Bands below stay pure L/R. Only RESTRICTS where FDK may place IS — never forces it on. |
98|| `--is-hi <sfb>` | -1 off, 0..N | -1 | Allow IS only up to this SFB (inclusive). Pair with `--is-lo` as a range. TIP: IS usually lands on LOW bands at low bitrate, so scan small values to see the effect. |
99|| `--is-force-lo <sfb>` | -1 off, 0..N | -1 | FORCE intensity stereo from this SFB, bypassing the correlation / min-sfbs / loudness gates. Laboratory mode: can deliberately wreck the stereo image (IS is lossy + directional — the right channel is zeroed, only a panning coefficient survives). The stream stays legal. |
100|| `--is-force-hi <sfb>` | -1 off, 0..N | -1 | Upper SFB of the forced-IS range (inclusive). |
101|
102|### Intensity stereo in practice (how to use it, not the formulas)
103|
104|What it is: intensity stereo (IS) in the upper bands drops separate L/R and sends
105|ONE energy envelope + direction (panning) information. The ear localizes high
106|tones poorly, so this saves quite a few bits — but at the cost of stereo
107|separation (the width of the scene at the top of the band narrows). You pay for it
108|especially on material with a real L/R difference in the highs (cymbals on one
109|side, spatial effects).
110|
111|By default FDK is very CAUTIOUS with IS (hence your observation "you can barely
112|hear the difference"). There are three reasons, and that is what these knobs are
113|for:
114|
115|1. IS admission gate: FDK considers IS at all only when `bitrate/band < 5`. At
116|   higher bitrates IS is disallowed entirely. `--is-aggression >=1` removes this
117|   gate.
118|2. Correlation threshold (`--is-corr-thresh`, Q8, 256=1.0, default 243 ~= 0.95):
119|   both channels must be at least ~95% similar to each other in a given band for
120|   IS to turn on. That is very high. Lower it -> IS catches more often, even when
121|   the channels are less similar. E.g. 180 (~0.70) = much more aggressive. Too low
122|   = audible direction errors.
123|3. Min. region length (`--is-min-sfbs`, default 6): IS turns on only on a band of
124|   at least 6 consecutive SFBs. Lower it to 1-2 -> IS also catches short
125|   fragments.
126|
127|The relationship between them: for a given SFB to go into IS, ALL conditions
128|MUST be met at once — the admission gate AND correlation above the threshold AND
129|a sufficiently long region AND a stable direction. That is why lowering just one
130|threshold often does nothing (another still blocks it) — and that is why you
131|usually see no difference by manipulating correlation alone. `--is-aggression`
132|moves ALL of them at once, coherently.
133|
134|How to set it:
135|- Simplest: `--is 1 --is-aggression 40` and listen. Too little IS -> raise to 70,
136|  100. Too much (the scene "glues together" at the top, direction artifacts) ->
137|  go down.
138|- 0 = FDK default (practically IS is barely active at typical bitrates).
139|- 100 = maximum: gate removed, correlation loose (~0.475), region from 1 SFB,
140|  wide direction tolerance. Many bands in IS, strongly audible, saves bits.
141|- Manual tuning only when you want precision: set `--is-aggression 0` and turn
142|  `--is-corr-thresh` (the main one), then `--is-min-sfbs`, finally `--is-lr-ratio`.
143|  The --is-* values OVERRIDE what the aggression slider set.
144|- Diagnostics: `--verbose` shows the effective thresholds (IS corr threshold Q8,
145|  min SFBs), so you see what actually went into the encoder.
146|
147|MS/IS bias (point 2): the above `--is-*` control WHEN the encoder chooses
148|intensity stereo (decision thresholds from the FDK tuning table), independently
149|of the hard on/off. `--msbands` limits MS to the lower bands (correctly — the
150|mask and the L/R->M/S butterfly are synchronized, no "left=center, right=rest"
151|artifact).
152|- Completely independent stereo: `--msmask 0 --is 0`.
153|- "Laboratory" restriction of MS to the lower bands: e.g. `--msbands 6`.
154|- Forced full MS: `--msmask 1`.
155|- More eager IS: lower `--is-corr-thresh` (e.g. 150) and/or `--is-min-sfbs`.
156|
157|### MS band range: --msbands, --msbands-lo, --msbands-hi (IMPORTANT, often confused)
158|
159|The spectrum bands are numbered FROM THE BOTTOM: band 0 = lowest frequencies
160|(bass), the higher the number, the higher in the spectrum. In a typical LC stereo
161|there are about 49 of them.
162|
163|There are TWO independent ways to limit where MS is applied:
164|
165|1. `--msbands <n>` — "the lower N bands". MS allowed ONLY in bands 0..(n-1),
166|   i.e. from the bass upward to number n. This is always counted FROM THE BOTTOM.
167|   Example: `--msbands 6` = MS only on the 6 lowest bands, the rest pure L/R.
168|
169|2. `--msbands-lo <lo>` + `--msbands-hi <hi>` — "FROM-TO range". MS allowed ONLY
170|   in bands numbered from `lo` to `hi` inclusive. Outside that range, pure L/R.
171|   It is a pair — you provide both. It lets you place MS ANYWHERE, including at
172|   the very top.
173|
174|A concrete example (assuming ~49 bands in LC):
175|- You want MS ONLY on the 5 HIGHEST bands (e.g. to merge noise at the top and
176|  leave the bottom in full independent stereo)? The highest bands are numbers
177|  44..48: `--msbands-lo 44 --msbands-hi 48`.
178|- You want MS only in the MIDDLE of the band (e.g. 10..30)? `--msbands-lo 10 --msbands-hi 30`.
179|- You want MS on the 6 LOWEST? Simpler `--msbands 6` (or `--msbands-lo 0
180|  --msbands-hi 5` — the same).
181|
182|Mnemonic: `--msbands` = "from the bottom up to", `--msbands-lo/-hi` = "from..to".
183|How many bands you actually have for a given mode/samplerate is shown by
184|`--verbose` (the "active SFBs" field).
185|
186|## 2. Cutoff of the AAC core when SBR is active
187|
188|The standard `-w/--bandwidth` in FDK is IGNORED when SBR is active
189|(HE-AAC v1/v2) — because `sbrEncoder_Init()` overrides the bandwidth with a value
190|from the SBR table.
191|
192|| Switch | Values | Default | Description |
193||---|---|---|---|
194|| `--core-cutoff <hz>` | 0 = default, >0 = Hz | 0 | Forces the AAC core bandwidth IN Hz even under SBR. Resistant to being overridden by SBR. |
195|
196|Example (your case — 7.5 kHz of core at HE-AAC v2 48 kbps, where the table gives
197|less):
198|```
199|fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
200|```
201|Verified: `--core-cutoff 7500` -> effective bandwidth 7500 Hz; stock `-w 7500`
202|under SBR stays 8613 Hz (ignored).
203|
204|NOTE: you police the core limits yourself. The max is the core Nyquist (`sr/2`),
205|and with **dual-rate SBR the target samplerate is divided by 2** — keep that in
206|mind when choosing values.
207|
208|## 3. Density / precision of SBR data
209|
210|Overrides the settings from the SBR tuning table (after it is loaded).
211|
212|| Switch | Values | Default | Description |
213||---|---|---|---|
214|| `--sbr-start <n>` | -1 def, 0..15 | -1 | `bs_start_freq` index (start of the SBR band). |
215|| `--sbr-stop <n>` | -1 def, 0..13 | -1 | `bs_stop_freq` index (end of the SBR band). |
216|| `--sbr-freqscale <n>` | -1 def, 0..3 | -1 | Frequency grouping (0 = linear, higher = finer log). |
217|| `--sbr-alterscale <n>` | -1 def, 0/1 | -1 | Alternative scale resolution. |
218|| `--sbr-noise-bands <n>` | -1 def, 1..5 | -1 | Number of SBR noise bands (density of the noise description). |
219|| `--sbr-amp-res <n>` | -1 def, 0/1 | -1 | Envelope amplitude resolution: 0 = 1.5 dB, 1 = 3.0 dB. |
220|| `--sbr-data-extra <n>` | -1 def, 0/1 | -1 | Write extra SBR header data. |
221|| `--sbr-num-env <1\|2\|4>` | -1 off | -1 | Number of envelopes per frame. FORCES a static time grid (ignores the transient detector). More = better temporal resolution of the upper band, worse on attacks. (8 exceeds the standard grid — rejected.) |
222|| `--sbr-freqres-fixfix <0\|1>` | -1 off | -1 | Frequency resolution of the FIXFIX envelope (0 low, 1 high). |
223|| `--sbr-stereo-mode <0..3>` | -1 off | -1 | SBR stereo mode: 0 mono, 1 LR (full separation of the upper band), 2 coupling (economical, shared envelope + level), 3 switch-LRC (by default the coder chooses per-frame). Force 1 for max separation, 2 for economy. |
224|| `--sbr-invf <0..3>` | -1 auto | -1 | Force SBR inverse filtering: 0 off, 1 low, 2 mid, 3 high. Normally driven by the tonality estimator. Higher = stronger "whitening" of tonal SBR (less metallicness at the cost of detail). |
225|| `--sbr-noise-floor-offset <n>` | -128 off | -128 | SBR noise floor offset (small integer). Larger = more filling noise in the SBR reconstruction. |
226|| `--sbr-header-period <n>` | -1 off, >=1 | -1 | Frames between SBR headers = how fast the SBR high band "kicks in" when a decoder tunes into a live HE-AAC stream (Icecast/Shoutcast). The SBR CONFIG lives in a periodic header, not in every frame; a decoder joining mid-stream plays core-only (muffled) until the next header arrives. `1` = header in every frame → near-instant SBR lock (~23 ms); higher = longer core-only moment. FDK default is ~10 frames (~0.23 s HE dual-rate / ~0.46 s LC). FDK caps this to at most once per second, so very large values are clamped (e.g. 40 → 21 frames @44.1k). See `--verbose` for the effective period in ms. |
227|
228|NOTE: `--sbr-start`/`--sbr-stop` are validated BY FDK — an incorrect start/stop
229|COMBINATION (wrong number of master bands) will give "encoder initialization
230|failed". This is a limitation of SBR itself, not a bug. Choose pairs (e.g. for
231|64k stereo start=5 stop=9, start=8 stop=14 work).
232|
233|## 4. Parametric Stereo (HE-AAC v2)
234|
235|PS describes stereo with a few parameters (IID/ICC...). Here you can control them,
236|even at the cost of the stereo image.
237|
238|| Switch | Values | Default | Description |
239||---|---|---|---|
240|| `--ps <n>` | -1 auto, 0 off, 1 on | -1 | Force sending the PS IID parameter. `0` = never (flattens the stereo image), `1` = always. Overrides the loudness-difference heuristic. |
241|| `--ps-iid-quant <n>` | -1 def, 0 coarse, 1 fine | -1 | IID quantization grid: coarse vs. fine. |
242|| `--ps-icc <n>` | -1 auto, 0 off, 1 on | -1 | Force ICC (Interchannel Coherence — channel similarity/coherence) on/off. |
243|| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | ICC rotation mode: 0 = ROT_A, 1 = ROT_B. Signalling only — the same matrix, derived differently by the decoder, so treat this as a compatibility knob rather than a quality one. |
244|| `--ps-bands <n>` | 10 or 20 | -1 (bitrate table) | Number of PS stereo bands = **frequency** resolution of the stereo parameters. Stock FDK derives this from the bitrate alone, so at 22 kbps and above you always get 20 and can never audition 10. Fewer bands = coarser stereo image, fewer parameter bits. |
245|| `--ps-env <n>` | 1, 2 or 4 | -1 (bitrate table) | PS parameter envelopes per frame = **time** resolution of the stereo parameters. Above 36 kbps stock FDK always picks 4. More envelopes track a moving panorama and transients more closely. |
246|| `--ps-env-reduce <n>` | 0, 1 | -1 (on) | `0` disables the automatic envelope-halving loop (`envelopeReducible`). By default FDK keeps collapsing 4 envelopes to 2 to 1 whenever neighbouring envelopes look similar by a hardcoded error threshold, so the envelope count you configured is often *not* what gets transmitted. `0` makes `--ps-env` literal. |
247|| `--ps-noenv-skip <n>` | 0, 1 | -1 (on) | `0` forbids parameter-less PS frames. By default FDK may emit up to 10 consecutive frames carrying **no** stereo parameters at all when successive IID/ICC sets look similar, which can be heard as the stereo image briefly collapsing and snapping back. `0` = always send parameters. |
248|
249|NOTE about PS resolution: `--ps-bands` and `--ps-env` are the two axes that
250|change *how many* stereo parameters are actually transmitted — in frequency and
251|in time respectively. That makes them considerably more audible than
252|`--ps-icc-mode`, which only changes how the same matrix is signalled. Measured on
253|a 4-second stereo probe with a deliberately moving panorama (0.25 Hz) plus
254|alternating L/R transients, encoded as HE-AAC v2 at 48 kbps, comparing the
255|panorama trajectory of the decoded file against the source:
256|
257|| Setting | Panorama error (RMS) | Correlation with source |
258||---|---|---|
259|| stock (20 bands / 4 envelopes) | 0.177 | 0.9655 |
260|| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
261|| `--ps-env 4 --ps-env-reduce 0` | **0.117** | **0.9944** |
262|
263|The interesting result is that requesting 4 envelopes alone changes nothing —
264|stock output and `--ps-env 4` are byte-identical, because the automatic halving
265|loop immediately collapses them again. The gain only appears once
266|`--ps-env-reduce 0` stops that loop: a 34 % reduction in panorama error for
267|roughly the same file size. If you only take one thing from this group, take
268|`--ps-env-reduce 0`.
269|
270|NOTE about IPD/OPD: FDK codes IID (loudness differences) and ICC (coherence)
271|only. The interchannel *phase* parameters are not emitted — `ps_encode.cpp`
272|literally writes zeros and comments `"IPD OPD not supported right now"`. Note
273|however that the encoder already computes both the real and the imaginary part of
274|the L/R cross-spectrum (`pwrCr` / `pwrCi`) and currently uses only their
275|magnitude, so the phase information is present but discarded; the Huffman tables
276|and bitstream writers for IPD/OPD also already exist in `ps_bitenc.cpp`.
277|
278|## 5. Noise substitution/shaping — TNS / PNS / afterburner
279|
280|What, at medium bitrates, is replaced by noise or resynthesized.
281|
282|| Switch | Values | Default | Description |
283||---|---|---|---|
284|| `--tns-mask <n>` | -1 def (0xF), 0..15 | -1 | TNS enable mask (bitwise, per block type). |
285|| `--tns-order <n>` | -1 def, 1..12 | -1 | Max. TNS filter order (short blocks additionally capped to 5). |
286|| `--pns <n>` | -1 def, 0/1 | -1 | Perceptual Noise Substitution on/off. NOTE: FDK forces PNS=off when SBR or VBR is active. |
287|| `--pns-start <hz>` | -1 def, Hz | -1 | PNS start frequency. Lower = more of the spectrum replaced by noise. |
288|| `--force-pns` | flag | off | Bypass the low-bitrate gate for PNS. |
289|| `--pns-gain <x>` | >=0.0 | -1 (off) | Loudness of the fabricated PNS noise. `1.0` = unchanged (noise energy = original band). `>1.0` = louder-than-original noise fill, `<1.0` = quieter. Directly scales the coded noise energy — this is the "how loud is the noise" knob. Decimal input. |
290|| `--pns-tonality <x>` | >=0.0 | -1 (off) | Scales the PNS tonality detection threshold. `1.0` = default; higher = more (even less-noisy) bands qualify as PNS = WIDER noise substitution. |
291|| `--pns-refpower <x>` | >=0.0 | -1 (off) | Scales the PNS reference-power detection threshold. `1.0` = default. |
292|| `--pns-gapfill <x>` | >=0.0 | -1 (off) | Scales the PNS gap-fill threshold (fills PNS holes between two PNS bands). `1.0` = default. Advanced/subtle — rarely visible. |
293|| `--pns-min-width <n>` | -1 off, >=1 | -1 | Minimum SFB width for PNS. Effective above the built-in default (LC=16); e.g. 32/64 restricts PNS to wider bands. |
294|| `--afterburner <n>` | 0/1 (also stock `-a`) | 1 | Afterburner (more precise quantization). |
295|
296|IMPORTANT about PNS at low bitrate: FDK has a tuning table (`levelTable`) that
297|COMPLETELY disables PNS below ~28 kbps (the bitrate row 0-27999 = all zeros for
298|every samplerate). That is why at 24 kbps `--pns`/`--pns-start` do NOTHING (the
299|audio sounds "like MP3/MDCT"), while at 64 kbps the difference is large.
300|`--force-pns` bypasses this gate (uses the first active row of the table), so PNS
301|also works at 24k. FDK limitation: PNS still requires TNS enabled and a non-VBR
302|mode — otherwise it is zeroed higher up in the chain (we can't do anything about
303|it without a deeper rebuild).
304|
305|## 6. Masking / ATH
306|
307|| Switch | Values | Default | Description |
308||---|---|---|---|
309|| `--ath-scale <n>` | 1..~4096 (Q8) | 256 | Masking threshold scale in Q8 (256 = x1.0). `>256` raises the thresholds (more noise, fewer bits per band), `<256` lowers them (cleaner, more bits). Works in FDK's ld64 domain as an additive log2 offset. NOTE: this only touches the log-domain threshold copy and is partly undone downstream by the min-SNR / 29 dB clamps — for a stronger, more direct effect prefer `--minsnr-scale` below. |
310|| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Scales the spreading of masking between bands. `<256` = less masking = more detail. Biggest effect where bits are limited (96-192k). |
311|| `--minsnr-scale <n>` | 1..no limit (Q8) | -1 (off) | MusePack-style: scales the REQUIRED per-band coding SNR (`sfbMinSnrLdData`, FDK's closest thing to TMN/NMT). `<256` = demand HIGHER SNR = more detail/bits; `>256` = coarser. More effective than `--ath-scale` because min-SNR is what the avoid-holes logic clamps thresholds back to. 256=off. |
312|| `--minsnr-clamp-hi <n>` | 1..no limit (Q8) | -1 (off) | Scales FDK's MAX_SNR ceiling (~−1 dB). `>256` lets bands demand more than the stock cap. 256=off. |
313|| `--minsnr-clamp-lo <n>` | 1..no limit (Q8) | -1 (off) | Scales FDK's MIN_SNR floor (~−25 dB). 256=off. |
314|| `--reduce-clamp <0\|1>` | 0, 1 | 1 (on) | `0` drops the "29 dB Ratio" threshold-reduction ceiling in the CBR quantizer, letting thresholds be pushed deeper (more bits into demanding bands). Pairs with `--minsnr-scale` for extreme detail. CBR only (VBR uses a different path). |
315|
316|### What really helps at low and medium bitrate (10-144 kbps)
317|
318|A common question: can you still squeeze something out of coding
319|accuracy/efficiency (Huffman, quantization iterations, etc.)? The honest answer
320|after reviewing the FDK code:
321|
322|- Huffman coding (section merging, codebook selection in `dyn_bits.cpp`) is
323|  already optimal (greedy section merging giving min. bits). There is no
324|  meaningful knob there — and exposing it would only worsen the result.
325|- The quantization iteration loop (`maxIterations`) is a RESCUE mechanism for bit
326|  shortage; increasing it does nothing (details in the manual, chapter 9a).
327|- Internal thresholds (minSnr adaptation, `bits2PeFactor`) are fixed-point
328|  arithmetic with hard ranges — moving them risks instability, not improvement.
329|
330|The REAL set of quality levers for 10-144 kbps is ALREADY exposed:
331|- `--ath-scale <256` — globally lower the masking threshold (more detail for bits).
332|- `--spread-mask <256` — less inter-band masking (more bands coded).
333|- `--ms-precision >256` — shallower holes in the MS bands.
334|- `--is-aggression` — control intensity stereo (crucial at low bitrate).
335|- `--force-pns` + `--pns-start` — noise control at very low bitrate.
336|- under HE-AAC: `--sbr-invf`, `--sbr-noise-floor-offset`, `--speech` (speech).
337|
338|This is not missing functionality — these are the same levers that professional
339|tuning uses, just manually. Start with `--ath-scale` and `--spread-mask`, one at
340|a time.
341|
342|## 7. Bias of short/long block switching (any profile)
343|
344|| Switch | Values | Default | Description |
345||---|---|---|---|
346|| `--block-bias <n>` | 0..255 | -1 (off) | Shifts the short/long decision threshold. 128 = encoder default (no change), >128 favors short blocks (more "transient"), <128 favors long ones, 0 = practically only long. |
347|
348|IMPORTANT: `--block-bias` always produces a standard-compliant stream (it shifts
349|the attack-detection threshold, it doesn't forcibly impose a block type). It
350|replaced the old `--allshort`/`--alllong`, which created an ILLEGAL stream (hard
351|forcing of the short window without recomputing SFB/grouping -> decoders rejected
352|it, Winamp "skipped like on a scratched disc"). If you want maximum long blocks:
353|`--block-bias 0`; maximum short: `--block-bias 255`.
354|
355|## 8. MS decision bias (honestly: a tool with a WEAK effect)
356|
357|| Switch | Values | Default | Description |
358||---|---|---|---|
359|| `--ms-bias <n>` | 0..255 (Q8) | -1 (off) | Shifts the L/R vs MS decision threshold. Q8, 128 = +0.5 in FDK's ld64 units. >0 = MS more eager. Reacts already from ~32 (after scale recalibration). |
360|
361|HONESTLY about `--ms-bias` — this is the weakest tool in the whole set, and now
362|we know why "it does little". MS (mid/side) is a LOSSLESS transform: mid=L+R,
363|side=L-R reconstructs exactly back to L/R. Enabling/disabling MS on a given band
364|does NOT change the sound — it only changes HOW MANY BITS the encoding takes. The
365|encoder chooses a near-optimal decision per band anyway; `--ms-bias` only shifts a
366|few BORDERLINE bands. Measurement (L/R correlation after decoding, ADTS size):
367|effect on the order of <0.1% of the size and correlation changes in the 4th
368|decimal place.
369|
370|Technical note: in the previous version the bias scale was ~256x too weak
371|(multiplier <<15 instead of <<23) — hence "128 did nothing, only 2048 moved
372|something". Now 128 = a real +0.5 ld64 as in the description, so it reacts from
373|~32. But even a correctly scaled bias inherently has a small impact (see above).
374|
375|Want to REALLY control MS? Use the hard switches, not the bias:
376|- `--msmask 0` — DISABLE MS entirely (pure, independent L/R). This is the right
377|  choice for center-cancel / vocal removal (zero channel mixing by the coder).
378|- `--msmask 1` — force MS on all bands (max bit economy).
379|- `--msbands` / `--msbands-lo/-hi` — restrict MS to selected bands.
380|Measurement: msmask 0 vs 1 gives ~900 B difference on a 2s sample; ms-bias only ~2 B.
381|
382|## 9. Quasi-constrained VBR (CBR engine + wider breathing)
383|
384|IMPORTANT: without these switches, CBR is 100% UNCHANGED (verified: bit-identical
385|to the original binary). You enable them deliberately.
386|
387|How AAC breathes: even in CBR, frames borrow from the bit-reservoir, so one frame
388|can be ~122 kbps and the next ~141, as long as the average = target. These knobs
389|widen/narrow that breathing.
390|
391|HARD CEILING for everything: an AAC frame holds MAX 6144 bits per channel
392|(=768 bytes/channel); stereo => 12288 bits/frame. At 44100 Hz one frame =
393|1024 samples = ~23.2 ms, so bits/frame = kbps * 23.22. (E.g. 128k stereo:
394|average ~2972 bits/frame; ceiling 12288.)
395|
396|| Switch | Values | Default | Description |
397||---|---|---|---|
398|| `--vbr-reservoir <bits>` | 0..(6144*channels - average) | -1 (off) | Bit-reservoir size. More = wider frame spread around the average. min 0 (stick to the target tightly). Auto-clamped to the ceiling - you can't overdo it. Safe start: 2-3x default. |
399|| `--peak-bitrate <bps>` | > target | -1 (off) | Allows short peaks up to this value while keeping the average. Set ABOVE `-b` (e.g. -b 128000 --peak-bitrate 160000). Below the target it is ignored. |
400|| `--max-bits-frame <bits>` | average..12288(st.) | -1 (off) | Hard ceiling of bits in ONE frame. Must be >= average and <= 6144*channels (otherwise clamped). Reasonable cap ~1.5x average (~4500 for 128k st.). Too low = starves loud frames (audible). |
401|| `--min-bits-frame <bits>` | 0..average | -1 (off) | Hard floor of bits/frame. A higher floor wastes bits on silence. Leave at 0 unless experimenting. |
402|| `--bitres-mode <n>` | 0/1/2 | -1 (def) | Reservoir mode: 0 full (like default), 1 reduced, 2 disabled (rigid, closest to hard per-frame CBR). |
403|
404|HOW TO SET IT OPTIMALLY (for the less experienced - so you don't overdo it):
405|- Safe quasi-CVBR ~128k stereo: `-b 128000 --peak-bitrate 160000 --vbr-reservoir 6000`.
406|- Do NOT set `--max-bits-frame` BELOW the average or `--min-bits-frame` ABOVE the
407|  average - that fights the target and ruins quality.
408|- `--vbr-reservoir` is auto-clamped to the ceiling, so it's safe to experiment.
409|- Measured for real (4s variable signal, 128k stereo): CBR default spread 95-167
410|  kbps; with `--vbr-reservoir 8000 --peak-bitrate 192000` spread 36-158 kbps
411|  (breathes harder, average held); `--bitres-mode 2` spread 127.8-128.2
412|  (rigid). All fully decodable.
413|- Limitation: this is a CBR+reservoir engine, not true ABR like LAME. Swings are
414|  moderate (6144 bits/channel limit), but this is that "light flight" of AAC.
415|
416|## 10. Audiophile / extreme (opt-in, beyond the typical range)
417|
418|| Switch | Values | Default | Description |
419||---|---|---|---|
420|| `--uncap-bandwidth` | flag | off | Remove the hard 20 kHz core cap. `--core-cutoff` can then reach all the way to Nyquist. |
421|| `--is-aggression <0..100>` | 0..100 | -1 (off) | IS aggression slider (see section 1). |
422|| `--force-pns` | flag | off | PNS below the ~28 kbps gate (see section 5). |
423|| `--unlock-bitrate` | flag | off | Remove the LOWER bitrate threshold. Allows extremely low: 8k HE-AAC stereo, 6k LC. IMPORTANT: in this mode `-b` is taken LITERALLY as bps (without the nu774 x1000 convention), so `-b 6000` = 6000 bps. The upper ceiling 6144*channels stays (hard AAC limit). Residual floor ~10 kbps = the minimum of AAC headers. |
424|| `--speech` | flag | off | SBR tuning mode for human SPEECH (different inverse-filtering thresholds, noise level, no parametric coding). Applies to HE-AAC (SBR); LC has no separate speech mode. For pure speech at low bitrate. |
425|| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Scales the spreading of masking between bands. `<256` = LESS masking = more detail (equivalent to loosening tone-masks-noise). Biggest effect where bits are limited (96-192k). 256=no change. Combine with `--ath-scale <256`. |
426|
427|BANDWIDTH ABOVE 20 kHz (audiophile): FDK has a hardcoded cap `min(20000, sr/2)` on
428|the core bandwidth — even with a 96 kHz input and high bitrate, nothing above
429|20 kHz is actually coded (your suspicion was correct). `--uncap-bandwidth` lifts
430|this cap; then `--core-cutoff` controls the bandwidth all the way up to sr/2.
431|
432|Measured (96 kHz input, LC 400k, broadband noise):
433|- without uncap, `--core-cutoff 40000`: verbose 20000 Hz, energy >20 kHz ~= 0%.
434|- `--core-cutoff 40000 --uncap-bandwidth`: verbose 40000 Hz, energy 20-24 kHz
435|  ~10%, 24-32 kHz ~20%, 32-44 kHz ~20% — full band up to 40 kHz actually coded.
436|
437|Audiophile preset (full bandwidth + manual masking, for 190+ kbps):
438|```
439|fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 \
440|    --ath-scale 200 -o out.m4a in96k.wav
441|```
442|`--ath-scale <256` lowers the masking thresholds (cleaner, more bits for detail) —
443|sensible when you have a large bitrate margin. Note: bandwidth >20 kHz and extreme
444|settings may be rejected by some decoders (outside the typical spec) — a conscious
445|opt-in.
446|
447|---
448|
449|## 11. MP4/M4A container — which boxes are necessary, and what can be cut
450|
451|An .m4a file is a set of nested "boxes" (atoms). Verified what is what:
452|
453|MANDATORY (without them the file is UNPLAYABLE — there are no switches for them):
454|`ftyp`, `mdat` (raw audio data), and the skeleton `moov` → `mvhd` + `trak` →
455|`tkhd` → `mdia` (`mdhd`/`hdlr`/`minf` → `smhd`/`dinf`/`stbl` with the tables
456|`stsd`/`stts`/`stsc`/`stsz`/`stco`). This is the minimal file map required by the
457|ISO standard — every decoder needs it to even find and play the sound. Cutting
458|them makes no sense (result: a corrupt file).
459|
460|OPTIONAL (can be disabled) — the entire block `udta` → `meta` → `ilst`, i.e.:
461|- the encoder identification tag (`©too`, now "PompAAC based on …"),
462|- `iTunSMPB` — encoder delay data for seamless (gapless) playback,
463|- all tags (title, artist, album, etc.).
464|
465|| Switch | Works on | Description |
466||---|---|---|
467|| `--no-tool-tag` | .m4a | Do not write the encoder identification tag (`©too`). The rest of the tags and gapless stay. |
468|| `--minimal-moov` | .m4a | The smallest legal .m4a: omits the ENTIRE `udta`/metadata block (encoder tag + gapless iTunSMPB + all tags). The playback skeleton stays intact. |
469|
470|How much it saves (2 s, 128k stereo, measured): default ~34381 B →
471|`--no-tool-tag` ~34297 B (−84) → `--minimal-moov` ~34048 B (−333). These are small
472|numbers — the MP4 overhead is mostly the mandatory skeleton, which can't be
473|removed. Want truly zero container overhead? Use raw ADTS: `-f 2 -o out.aac`
474|(a stream without any boxes, but also without tags and gapless).
475|
476|NOTE on gapless: `--minimal-moov` removes iTunSMPB, so when joining tracks
477|micro-gaps may appear (the encoder delay won't be signaled). For ordinary
478|listening this doesn't matter; for "gapless" albums leave the defaults.
479|
480|---
481|
482|## 12. Legend for reading `--verbose`
483|
484|`--verbose` prints RAW values (without hints in parentheses, so as not to clutter).
485|Below is what the non-obvious ones mean:
486|
487|| Field | How to read it |
488||---|---|
489|| `AOT (profile)` | 2=AAC-LC, 5=HE-AAC, 29=HE-AAC v2, 23=AAC-LD, 39=AAC-ELD. |
490|| `bitrate-mode` | 0=CBR, 1..5=VBR (higher=better). |
491|| `channel-mode` | 1=mono, 2=stereo (for HE-AAC v2 the core is mono, stereo is done by PS). |
492|| `core bandwidth` | Upper frequency of the AAC core, **anchored to the nearest SFB boundary** (the real cutoff, which can differ from the `-w`/`--core-cutoff` value you typed, e.g. `-w 17300` → `17915 Hz (SFB-anchored)`). In parentheses the SOURCE: `from -w`, `from --core-cutoff`, or `auto`. Under SBR this is only the core — SBR plays higher. |
493|| `final BW (AAC+SBR)` | Shown only when SBR is active: approximate UPPER frequency of the whole signal (core + SBR), computed from the `sbr stop freq index`. This is the equivalent of `core bandwidth`, but for the full HE-AAC band. |
494|| `signaling-mode` | Way of signaling SBR/PS: 0=implicit, 1=explicit backward-compat, 2=explicit hierarchical, auto=the library chooses. |
495|| `SBR mode` | Internal SBR mode (-1/0 when unused). |
496|| `sbr-ratio` | 1=downsampled (single-rate), 2=dual-rate (core at half the frequency). |
497|| `sbr amp res` | 0=1.5 dB, 1=3.0 dB (envelope amplitude resolution). |
498|| `granule/frame length` | Frame length in samples (1024 for LC, 512/480 for LD/ELD). |
499|| `codec delay` | Codec delay in samples/channel (total and the core alone). For gapless. |
500|| `IS corr threshold` / `IS L/R ratio` | Thresholds on the Q8 scale: 256 = 1.0. A lower correlation threshold = intensity stereo MORE eager (counterintuitively). |
501|| `IS min contiguous SFBs` | How many adjacent bands must "agree" before IS turns on. |
502|| `TNS mask` | Bitmask 0x0..0xF of which TNS filters are active. |
503|| `MS/IS bands: auto up to N` | Upper band index up to which the tool may operate. |
504|| `franken overrides applied` | List of switches that in THIS run deviate from pure FDK (or "none"). |
505|
506|Q8 values (like `IS corr threshold`, `--ms-precision`, `--ms-bias`,
507|`--ath-scale`, `--spread-mask`) are fixed-point numbers where 256 = 1.0;
508|e.g. 243 means 243/256 ≈ 0.95.
509|
510|---
511|
512|## 13. Reference tables (from the FDK tuning tables)
513|
514|Three orientation tables, so you can consciously choose `--msbands`, `--sbr-start/stop`
515|and `-w`. The values are computed from the tables in the FDK code; they are
516|APPROXIMATE (the SFB grid is stepped), but they show the right order of magnitude.
517|
518|### Table 1 — approximate upper band frequency (SFB) [Hz]
519|
520|Bands numbered from the bottom (0=bass). Shown every 4th band; the last row = the
521|number of bands and the Nyquist frequency. Useful for `--msbands`/`--isbands`/`--msbands-lo/-hi`.
522|
523|| SFB | 16 kHz | 22.05 kHz | 32 kHz | 44.1 kHz | 48 kHz | 96 kHz |
524||----:|-------:|----------:|-------:|---------:|-------:|-------:|
525|| 0   | 62   | 43   | 62   | 86    | 94    | 188   |
526|| 4   | 312  | 215  | 312  | 431   | 469   | 938   |
527|| 8   | 562  | 388  | 562  | 775   | 844   | 1688  |
528|| 12  | 875  | 646  | 1000 | 1378  | 1500  | 2438  |
529|| 16  | 1250 | 991  | 1500 | 2067  | 2250  | 3750  |
530|| 20  | 1656 | 1335 | 2250 | 3101  | 3375  | 5625  |
531|| 24  | 2188 | 1852 | 3375 | 4651  | 5062  | 8062  |
532|| 28  | 2875 | 2584 | 5000 | 6891  | 7500  | 12938 |
533|| 32  | 3844 | 3618 | 7000 | 9647  | 10500 | 24000 |
534|| 36  | 5188 | 5039 | 9000 | 12403 | 13500 | 36000 |
535|| 40  | 7000 | 7020 | 11000| 15159 | 16500 | 48000 |
536|| 44  | —    | 9647 | 13000| 17916 | 19500 | —     |
537|| 48  | —    | —    | 15000| 22050 | 24000 | —     |
538|| **number of bands / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |
539|
540|### Table 2 — SBR: start freq index → approximate crossover frequency [Hz]
541|
542|This is the frequency from which SBR takes over the band above the AAC core
543|(`--sbr-start`, index 0..15; lower = SBR starts lower = narrower core). "core" is
544|the core frequency; with dual-rate the output is twice as high (e.g. core 24k →
545|output 48k).
546|
547|| start index | core 16 kHz | core 24 kHz | core 32 kHz | core 44.1 kHz | core 48 kHz |
548||----:|----:|----:|----:|----:|----:|
549|| 0 | 2750 | 2250 | 2500 | 1378 | 1500 |
550|| 1 | 3000 | 2625 | 3000 | 2067 | 2250 |
551|| 2 | 3250 | 3000 | 3500 | 2756 | 3000 |
552|| 3 | 3500 | 3375 | 4000 | 3445 | 3750 |
553|| 4 | 3750 | 3750 | 4500 | 4134 | 4500 |
554|| 5 | 4000 | 4125 | 5000 | 4823 | 5250 |
555|| 6 | 4250 | 4500 | 5500 | 5512 | 6000 |
556|| 7 | 4500 | 4875 | 6000 | 6202 | 6750 |
557|| 8 | 4750 | 5250 | 6500 | 6891 | 7500 |
558|
559|Stop freq (`--sbr-stop`, 0..13) works analogously on the upper SBR boundary —
560|a higher index = SBR reaches higher (closer to the output Nyquist). By default the
561|library chooses both according to bitrate.
562|
563|### Table 3 — AAC-LC: approximate cutoff (`-w`/auto) by bitrate per channel [Hz]
564|
565|When you don't provide `-w`, FDK picks the bandwidth from this table according to
566|bitrate PER CHANNEL (stereo 128k = 64k/channel). The values are interpolated; the
567|mono and stereo columns differ. Helps assess whether `-w` makes sense (providing a
568|higher value than auto has an effect only if there are spare bits).
569|
570|| bitrate/channel | bandwidth (mono) | bandwidth (stereo+) |
571||----:|----:|----:|
572|| 0–12 kbps  | 3700  | 5000  |
573|| 20 kbps    | 6900  | 9640  |
574|| 28 kbps    | 9600  | 13050 |
575|| 40 kbps    | 12060 | 14260 |
576|| 56 kbps    | 13950 | 15500 |
577|| 72 kbps    | 14200 | 16120 |
578|| ≥96 kbps   | 17000 | 17000 |
579|
580|Note: this table is SHARED for 32/44.1/48 kHz and higher — FDK indexes it by
581|bitrate per channel, not samplerate (samplerate only affects the upper limit =
582|Nyquist). For LC without SBR the real ceiling is ~17 kHz with auto; higher only
583|via `-w` (with spare bits) or `--uncap-bandwidth` at sr≥96k.
584|
585|---
586|
587|## 14. DAB+ output (`--dab`, `--dab-label`)
588|
589|A dedicated output mode that emits a DAB+ digital-radio stream instead of an
590|MP4/M4A file or a bare ADTS stream. The encoder produces the AAC audio exactly as
591|the DAB+ system expects it (960-sample transform, 120 ms super frame, error
592|protection), so the stream can be handed straight to a multiplexer such as
593|`odr-dabmux` → ETI → transmitter (or a soft receiver like welle.io / dablin).
594|
595|DAB+ is not "AAC in a different box": it uses the 960-sample MDCT (not 1024), packs
596|audio into 120 ms **super frames**, guards the header with a **firecode** (Fire
597|CRC), and protects the payload with **Reed-Solomon RS(120,110)** over GF(256). The
598|output is a raw `.dabp` stream — successive super frames back to back — which is
599|what a DAB+ multiplexer ingests.
600|
601|### `--dab`
602|Turn on DAB+ super-frame output. The result is a raw `.dabp` stream (no MP4, no
603|ADTS). Constraints imposed by the standard:
604|
605|| Requirement | Value |
606||---|---|
607|| Sample rate | MUST be `32000` or `48000` Hz |
608|| Bitrate | multiple of 8 kbps, range 8..192 kbps |
609|| Channels | mono or stereo |
610|| Profiles | AAC-LC, HE-AAC, HE-AAC v2 (all three) |
611|
612|The profile (AOT) is picked AUTOMATICALLY from bitrate and channel count, the same
613|way `odr-audioenc` does it — you normally don't set `-p`:
614|
615|- stereo ≤48 kbps (subchannel ≤6) → **HE-AAC v2** (PS),
616|- mono ≤64k or stereo ≤80k → **HE-AAC** (SBR),
617|- higher → **AAC-LC**.
618|
619|You can still force the profile with `-p` (`2`=LC, `5`=HE-AAC, `29`=HE-AAC v2) if
620|you know what you want.
621|
622|### `--dab-label <text>`
623|A static **DLS** (Dynamic Label Segment) carried as X-PAD inside the super frame;
624|DAB+ receivers show it as the station name / title. Up to ~48 characters (three
625|segments in one PAD). "Static" means one fixed string for the whole file —
626|time-varying labels and MOT slideshow (the `ODR-PadEnc` model) are planned for
627|later. Without `--dab` the label is ignored.
628|
629|### Examples
630|
631|```
632|# 48k/32k stereo, ~96k → auto AAC-LC:
633|fdkaac --dab -b 96 -o out.dabp input.wav
634|
635|# → auto HE-AAC (SBR):
636|fdkaac --dab -b 64 -o out.dabp input.wav
637|
638|# → auto HE-AAC v2 (PS):
639|fdkaac --dab -b 32 -o out.dabp input.wav
640|
641|# with a station label:
642|fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav
643|
644|# broadcast chain:
645|# out.dabp → odr-dabmux → ETI → transmitter / decoder
646|```
647|
648|Note: without `--dab` the encoder behaves exactly as before — zero impact,
649|bit-identical to stock. Verified: the streams decode with an independent faad2
650|decoder (dablin), the DLS label is readable, and the LC output is bit-identical to
651|the reference `odr-audioenc`. Nine combinations (48/32 kHz × mono/stereo × the
652|three profiles) were tested, each independently decodable.
653|
654|---
655|
656|## Examples
657|
658|```
659|# Starting point — what the encoder set:
660|fdkaac-franken-x64.exe -p 29 -b 48000 --verbose -o out.m4a in.wav
661|
662|# Quasi-constrained VBR ~128k (safe preset):
663|fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav
664|
665|# Aggressive intensity stereo at 64k:
666|fdkaac-franken-x64.exe -p 2 -b 64000 --is 1 --is-aggression 70 -o out.m4a in.wav
667|
668|# Force PNS at 24k (otherwise the FDK gate disables it):
669|fdkaac-franken-x64.exe -p 2 -b 24000 --pns 1 --force-pns -o out.m4a in.wav
670|
671|# Audiophile full 40 kHz band from a 96 kHz input:
672|fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav
673|
674|# MS on the 5 highest bands + shallower holes (not the lowest):
675|fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 --ms-precision 448 -o out.m4a in.wav
676|
677|# Extremely low HE-AAC v2: 8000 bps stereo 48 kHz:
678|fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav
679|
680|# SBR: full stereo separation LR + forced inverse filtering:
681|fdkaac-franken-x64.exe -p 5 -b 96000 --sbr-stereo-mode 1 --sbr-invf 2 -o out.m4a in.wav
682|
683|# Your case: 7.5 kHz of core under HE-AAC v2 48k:
684|fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
685|
686|# Completely independent stereo (no MS/IS) on LC 128k:
687|fdkaac-franken-x64.exe -p 2 -b 128000 --msmask 0 --is 0 -o out.m4a in.wav
688|
689|# MS only on the 6 lower bands:
690|fdkaac-franken-x64.exe -p 2 -b 96000 --msmask 1 --msbands 6 -o out.m4a in.wav
691|
692|# Only short blocks + limited TNS:
693|# Only long blocks (bias) + limited TNS:
694|fdkaac-franken-x64.exe -p 2 -b 96000 --block-bias 0 --tns-order 2 -o out.m4a in.wav
695|
696|# More aggressive masking (thresholds x2) + earlier PNS:
697|fdkaac-franken-x64.exe -p 2 -b 80000 --ath-scale 512 --pns 1 --pns-start 4000 -o out.m4a in.wav
698|
699|# Denser SBR noise description + more precise amplitude:
700|fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-noise-bands 5 --sbr-amp-res 0 -o out.m4a in.wav
701|
702|# HE-AAC v2 with PS disabled (flattened stereo):
703|fdkaac-franken-x64.exe -p 29 -b 32000 --ps 0 -o out.m4a in.wav
704|```
705|
706|---
707|
708|## Default quality vs the original binary (fdkaac2.exe)
709|
710|Verification (2026-07-21) whether franken's default settings = the original
711|supplied binary (dBpoweramp R17, the same libfdk-aac version 4.0.1 / package 2.0.3):
712|- AAC-LC: **bit-identical** (the same md5) — zero regressions.
713|- HE-AAC v1/v2: the bytes differ, BUT: the HE-AAC v2 spectrum is identical to
714|  0.000 dB, and the HE-AAC v1 error relative to the original is practically the
715|  same as in the original (7.05e11 vs 7.05e11). The byte difference stems from a
716|  different compiler (mingw/gcc vs MSVC) and fixed-point rounding, NOT from worse
717|  quality. The audible SBR "otherness" is a different, equally correct
718|  implementation, not a quality loss.
719|
720|---
721|
722|## Tests (make check)
723|
724|Upstream fdk-aac/nu774 have no unit tests (`make check` in them = a no-op). This
725|project has its OWN functional test suite in `tests/check.sh`, run with:
726|
727|```
728|make check
729|```
730|
731|It checks (on the x64 + x86 binaries if present): completeness of the switches in
732|--help, decodability of the stream for each switch (ffmpeg, 0 errors), real
733|operation of quasi-CVBR (CVBR breathes wider than the rigid bitres-mode2), verbose
734|without -1 values, and NO REGRESSION (default CBR without franken flags = ADTS
735|bit-identical to the original fdkaac2.exe). Requires ffmpeg + python3 (present in
736|WSL). Exit 0 = OK, 1 = errors, 77 = missing dependencies. Last result: 27/27 PASS.
737|
738|---
739|
740|The sources with the applied patches live in `src-fdk-aac/` and `src-fdkaac/`.
741|All changes in libfdk are wired through a single module
742|`libAACenc/src/franken.{h,cpp}` (the global `g_franken` block, sentinels = FDK
743|defaults), and the new `AACENC_PARAM`s (range `0xF0xx`) are exposed in
744|`aacenc_lib.h`.
745|
746|```bash
747|sudo apt-get install -y mingw-w64          # + autotools (autoconf/automake/libtool)
748|
749|# --- libfdk-aac (x64) ---
750|cd src-fdk-aac && autoreconf -i
751|./configure --host=x86_64-w64-mingw32 --prefix=$PWD/../inst-x64 \
752|    --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2
753|make -j && make install
754|
755|# --- frontend (x64) ---
756|cd ../src-fdkaac && autoreconf -i
757|PKG_CONFIG_PATH=$PWD/../inst-x64/lib/pkgconfig ./configure \
758|    --host=x86_64-w64-mingw32 CFLAGS="-O2 -I$PWD/../inst-x64/include" \
759|    LDFLAGS="-static -static-libgcc -L$PWD/../inst-x64/lib"
760|make -j     # -> fdkaac.exe
761|
762|# x86: the same with --host=i686-w64-mingw32 and a separate inst-x86 prefix.
763|```
764|
765|## List of changed FDK files (mapped to points)
766|- `libAACenc/src/franken.{h,cpp}` — new control module.
767|- `libAACenc/include/aacenc_lib.h` — new `AACENC_PARAM` 0xF0xx.
768|- `libAACenc/src/aacenc_lib.cpp` — SetParam dispatch + override of useMS/IS/PNS/
769|  afterburner/cutoff + cutoff guard under SBR (after `sbrEncoder_Init`) + read-only
770|  GetParam mirrors for verbose (useTns/Pns/IS/MS, effective SBR).
771|- `libAACenc/src/ms_stereo.cpp` — per-band MS control IN THE DECISION LOOP (mask
772|  + L/R->M/S butterfly synchronized; fixed the "left=center" artifact) + MS bias
773|  + MS band range (--msbands-lo/-hi) + MS precision (--ms-precision, ld64 threshold).
774|- `libAACenc/src/intensity.cpp` — IS band cap (consistent with the mask) + IS
775|  threshold bias (initIsParams: min_is_sfbs, corr_thresh, left_right_ratio) + --is-aggression.
776|- `libAACenc/src/psy_configuration.cpp` — read-back of the SFB count + removal of the IS gate.
777|- `libAACenc/src/bandwidth.cpp` — --uncap-bandwidth (removing the 20kHz cap).
778|- `libAACenc/src/pnsparam.cpp` — PNS start override + --force-pns (bypass the table gate).
779|- `libAACenc/src/aacenc.cpp` — TNS mask override + quasi-CVBR + --unlock-bitrate
780|  (removal of the lower bitrate floor in FDKaacEnc_LimitBitrate).
781|- `libSBRenc/src/sbr_encoder.cpp` — SBR density override + --sbr-num-env (static
782|  framing) + --sbr-freqres-fixfix + --sbr-stereo-mode + --sbr-noise-floor-offset.
783|- `libSBRenc/src/invf_est.cpp` — --sbr-invf (forced inverse filtering level).
784|- `libSBRenc/src/ps_encode.cpp` — PS IID override + --ps-icc/--ps-icc-mode.
785|- `libAACenc/src/aacenc_tns.cpp` — TNS order cap.
786|- `libAACenc/src/pnsparam.cpp` — PNS start frequency override.
787|- `libSBRenc/src/sbr_encoder.cpp` — SBR density/precision override + writing the
788|  effective SBR values to g_franken (for verbose).
789|- `libSBRenc/src/ps_encode.cpp` — PS override (forcing IID + quantization mode).
790|- `libAACenc/src/main.c`, `aacenc.c`, `aacenc.h` (frontend) — CLI switches,
791|  parsing, passing to SetParam, `--verbose` dump, help.
792|

# PL Wersja polska

1|# Franken FDK AAC — laboratoryjny/"geekowski" enkoder AAC (FDK)
2|
3|Zbudowany na bazie **libfdk-aac 2.0.3** (mstorsjo/fdk-aac) + frontendu
4|**nu774/fdkaac 1.0.2**, z doklejonymi przełącznikami CLI, które odslaniaja
5|normalnie zahardkodowane, wewnętrzne decyzję enkodera FDK. Sluzy do
6|ekstremalnego debugowania i eksperymentów z AAC/HE-AAC/HE-AAC v2.
7|
8|Binarki (statyczne, bez zewnętrznych DLL):
9|- `fdkaac-franken-x64.exe` — Windows 64-bit (PE32+)
10|- `fdkaac-franken-x86.exe` — Windows 32-bit (PE32)
11|
12|## Pobieranie
13|
14|Gotowe binarki Windows są publikowane jako GitHub Releases — budowane i
15|hostowane przez GitHub (do pobrania bez logowania):
16|
17|**→ https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**
18|
19|Pobierz `franken-fdk-aac-x64-vX.Y.Z.zip` (64-bit) albo wersję `x86`; każdy zip
20|zawiera `.exe` oraz komplet dokumentacji. W repozytorium nie ma żadnych binarek
21|— jak zbudować samodzielnie, patrz sekcja „Budowanie" na dole.
22|
23|
24|Wszystkie oryginalne opcje frontendu nu774 (`-p/--profile`, `-b/--bitrate`,
25|`-m/--bitrate-mode`, `-w/--bandwidth`, `-a/--afterburner`, `-s/--sbr-ratio`,
26|`-f/--transport-format`, tagowanie itd.) działają jak wczesniej. Poniżej tylko
27|NOWE przełączniki. Zobacz tez `fdkaac-franken-x64.exe --help`.
28|
29|UWAGA: to jest narzędzie "wiem-co-robię". Większość tych parametrów celowo
30|pozwala wyjść poza to, co robi automatyka FDK — można nimi świadomie zepsuć
31|obraz stereo, pasmo albo jakość. Sentinel `-1` (lub `0` dla `--core-cutoff`)
32|= "zostaw domyślne zachowanie FDK".
33|
34|Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski.
35|Zbudowane na oprogramowaniu open source: libfdk-aac (Fraunhofer IIS) oraz
36|frontend nu774/fdkaac.
37|
38|---
39|
40|## Spis grup opcji (tak samo pogrupowane w `--help`)
41|
42|Opcje są uporządkowane tematycznie, z grubsza od najlatwiejszych do najbardziej
43|geekowskich. Ta sama kolejność obowiazuje w `--help` (grupy A–E) i w sekcjach
44|poniżej:
45|
46|- **A. Zacznij tutaj (konsumenckie):** `--verbose`, `--is-aggression`, `--speech`,
47|  `--uncap-bandwidth`, `--unlock-bitrate` — sekcje 0, 1, 2, 10.
48|- **B. Stereo:** MS (`--msmask`, `--msbands`, `--msbands-lo/-hi`, `--side-bias`,
49|  `--side-knee`, `--mask-slope`), IS (`--is`, `--isbands`, `--is-*`), PS (`--ps`, `--ps-iid-quant`,
50|  `--ps-icc`, `--ps-icc-mode`) — sekcje 1, 4, 8.
51|- **C. Pasmo i SBR:** `--core-cutoff`, `--sbr-*` — sekcje 2, 3.
52|- **D. Maskowanie / szum / detal:** `--ath-scale`, `--spread-mask`, `--tns-*`,
53|  `--pns`, `--pns-start`, `--force-pns` — sekcje 5, 6.
54|- **E. Bloki i bitrate:** `--block-bias`, `--vbr-reservoir`, `--peak-bitrate`,
55|  `--max-bits-frame`, `--min-bits-frame`, `--bitres-mode` — sekcje 7, 9.
56|- **F. Radio cyfrowe DAB+:** `--dab`, `--dab-label` — sekcja 14.
57|
58|Wskazowka: `--verbose` na koncu wypisuje sekcje "franken overrides applied" —
59|dokładnie te przełączniki, które w danym uruchomieniu odbiegają od czystego FDK.
60|
61|---
62|
63|## 0. Diagnostyka
64|
65|### `--verbose`
66|Przed enkodowaniem wypluwa na stderr REALNE, wybrane przez enkoder parametry
67|(nie tylko Twoje nadpisania): AOT, bitrate/tryb, samplerate, channel-mode,
68|EFEKTYWNY cutoff rdzenia w Hz, afterburner, transport, signaling, oraz stan
69|narzędzi kodujacych wybrany przez enkoder (TNS on/off, PNS on/off, Intensity
70|stereo on/off, MS stereo on/off). Gdy SBR aktywny: sbr-ratio + efektywne
71|start/stop freq index, freq scale, noise bands, amp res. Na koncu lista Twoich
72|override'ow (-1/0 = nie ustawione, zostawione enkoderowi). Idealne, by poznać
73|punkt wyjścia (np. domyślny cutoff HE-AAC v2 48k = 8613 Hz).
74|
75|---
76|
77|## 1. Joint stereo — MS / IS / niezależne stereo
78|
79|FDK domyślnie sam decyduje per-pasmo o MS (mid/side) i IS (intensity). Tu można
80|te decyzję nadpisac i wymusic ekstremalne konfiguracje.
81|
82|| Switch | Wartości | Domyślnie | Opis |
83||---|---|---|---|
84|| `--msmask <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś MS: `0` = wszystkie pasma L/R (kompletnie niezależne stereo), `1` = MS na wszystkich pasmach. |
85|| `--msbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalny numer SFB, który może użyć MS (bierze N NAJNIŻSZYCH pasm). Powyżej — MS wyłączone. |
86|| `--msbands-lo <n>` | -1 off, 0..N | -1 | POCZATEK (najniższy numer pasma SFB) zakresu, w którym dozwolone jest MS. |
87|| `--msbands-hi <n>` | -1 off, 0..N | -1 | KONIEC (najwyższy numer pasma SFB) zakresu MS. Używane razem z `--msbands-lo` jako para OD–DO. Pasma poza tym zakresem ida czystym L/R. |
88|| `--ms-precision <n>` | 256..bez limitu (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny, użyj `--side-bias`.)* Skaluje precyzję pasm MS globalnie (mid i side razem), w stylu LAME `-q`. 256=bez zmian, 384~1.5x, 512~2x. W praktyce jego zasięg jest ograniczony: powyżej ~600-800 prog uderza w twarda podłogę FDK, a przy CBR bity są tylko PRZESUWANE między pasmami, więc brzmienie przestaje sie zmieniac. Do strojenia stereo zastąpiony przez `--side-bias`/`--side-knee`, które działają per-kanał dokładnie tam, gdzie trzeba. |
89|| `--mid-bias <n>` | 256..bez limitu (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny.)* `>256` PODNOSI prog kanału mid (L+R) po motylku MS, żeby uwolnić bity z mid dla side. Czystszym, lepiej zmierzonym sposobem przesunięcia balansu mid↔side jest `--side-bias` (który sięga po ten sam budzet od strony side). Zostawiony dla kompletności. 256=off. |
90|| `--side-bias <dB>` | -24.0 .. +50.0 | 0 (off) | **Główne pokrętło jakości stereo.** Przesuwa prog maskowania kanału SIDE (L−R) na pasmach kodowanych w MS, dokładnie w miejscu, gdzie FDK decyduje, czy pasmo skali (SFB) jest kodowane czy zerowane (`energia > prog` w `sf_estim.cpp`). Znak = EFEKT: **`+` kieruje WIĘCEJ bitow do kanału side** (niższy prog → mniej pasm side zerowanych, przetrwałe kwantyzowane dokładniej → czystsza szerokość stereo, ogony pogłosu, ambience), kosztem kanału mid; **`−` celowo DEGRADUJE side** (podnosi prog → pasma side wypadają → węższy, bardziej mono obraz). To dokładnie ta sama zależność energia-vs-prog, której LAME używa do alokacji bitow, a MusePack steruje przez `--ms` — nic egzotycznego. Ponieważ to tradeoff przy stałym budzecie, przy niskim bitrate kanał mid słyszalnie oddaje bity; to oczekiwane, nie błąd. Rozsądny zakres **+3 .. +9** dla „szerzej", ujemne tylko do skrajnego/artystycznego niszczenia przy niskim bitrate. 0 = off (bit-identyczny ze stockiem). |
91|| `--side-knee <dB>` | -24.0 .. +50.0 | 0 (off) | Kształtuje JAK OSTRO pasmo side przełącza się między „kodowane" a „wyzerowane" na progu. Stock FDK to twardy klif: w chwili gdy `energia ≤ prog`, całe pasmo spada do zera. **`+` = MIĘKKIE kolano**: pasma leżące do N dB *poniżej* progu są nadal zachowane (kodowane na najzgrubszym scalefactorze) zamiast zerowane, więc side gaśnie stopniowo zamiast wyłączać się — łagodniejsze wybrzmienie pogłosu/powietrza. **`−` = TWARDE kolano**: pasma, które ledwo przekraczają prog (do N dB *nad* nim), są mimo to zerowane, odcinając side wcześniej — chudziej, agresywniej. Ortogonalny do `--side-bias` i łączy się z nim. Rozsądny zakres **+3 .. +6**. 0 = off. |
92|| `--mask-slope <dB>` | -24.0 .. +50.0 | 0 (off) | Globalne (mid **i** side) strojenie **Masking-Slope-Adaptation** FDK — heurystyki NIE-maskującej (`adj_thr.cpp`), która rozluźnia wymagany SNR dla pasm SFB o energii dużo poniżej średniej ramki (stock: ponad ~10 dB poniżej), czyli celowo głodzi bardzo ciche pasma, żeby oszczędzić bity. To pokrętło przesuwa prog „jak daleko poniżej średniej, zanim przestanę się przejmować". **`+` podnosi go → mniej cichych pasm głodzonych → więcej detalu w cichych fragmentach, ogonach pogłosu, wybrzmieniach** (kosztuje bity); **`−` obniża go → ciche pasma głodzone mocniej → chudziej, bardziej pusto, więcej bitow na głośne rzeczy**. Ta sama rodzina co `--side-bias`, ale stosowana do obu kanałów i zakotwiczona na energii-vs-średnia zamiast progu MS. Subtelny na gęstym materiale (rusza tylko najcichsze pasma); najbardziej słyszalny na rzadkiej/pogłosowej treści. Rozsądny zakres **±6 .. ±12**. 0 = off. |
93|| `--is <n>` | -1 auto, 0 off, 1 on | -1 | Intensity stereo globalnie wł/wył. |
94|| `--isbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalna liczba SFB, które mogą użyć intensity. Powyżej — kodowane normalnie. |
95|| `--is-aggression <0..100>` | 0..100 | -1 (off) | KONSUMENCKI suwak: jak bardzo enkoder ma isc w intensity stereo. Zaczynaj tu, zaawansowane `--is-*` zostaw. |
96|| `--is-min-sfbs <n>` | -1 def(6), 0..N | -1 | (zaawansowane) Min. liczba ciaglych SFB, zanim IS sie włączy. |
97|| `--is-corr-thresh <n>` | -1 def(243), Q8 | -1 | (zaawansowane) Prog korelacji L/R dla IS w Q8 (256=1.0). |
98|| `--is-lr-ratio <n>` | -1 def(179), Q8 | -1 | (zaawansowane) Prog balansu energii L/R dla IS w Q8 (256=1.0). |
99|| `--is-lo <sfb>` | -1 off, 0..N | -1 | Pozwól na intensity stereo TYLKO od tego SFB w górę. Pasma poniżej zostają czyste L/R. Tylko OGRANICZA gdzie FDK może użyć IS — nigdy go nie wymusza. |
100|| `--is-hi <sfb>` | -1 off, 0..N | -1 | Pozwól na IS tylko do tego SFB (włącznie). Używaj z `--is-lo` jako zakres. WSKAZOWKA: IS zwykle ląduje na NISKICH pasmach przy niskim bitrate, więc skanuj małe wartości, by zobaczyć efekt. |
101|| `--is-force-lo <sfb>` | -1 off, 0..N | -1 | WYMUSZA intensity stereo od tego SFB, omijając bramki korelacji / min-sfbs / głośności. Tryb laboratoryjny: może celowo rozbić obraz stereo (IS jest stratne i kierunkowe — prawy kanał zostaje wyzerowany, zostaje tylko współczynnik panoramy). Strumien pozostaje legalny. |
102|| `--is-force-hi <sfb>` | -1 off, 0..N | -1 | Górny SFB wymuszonego zakresu IS (włącznie). |
103|
104|### Intensity stereo w praktyce (jak tego używać, nie wzory)
105|
106|Co to jest: intensity stereo (IS) w górnych pasmach porzuca osobne L/R i wysyla
107|JEDNA obwiednię energii + informację o kierunku (panoramie). Ucho słabo lokalizuje
108|wysokie tony, więc to oszczędza sporo bitow — ale za cena separacji stereo (szerokość
109|sceny w górze pasma sie zwęża). Płaci sie za to zwłaszcza na materiale z realna
110|różnica L/R w wysokich (blachy z jednej strony, efekty przestrzenne).
111|
112|FDK domyślnie jest bardzo OSTROŻNY z IS (stąd Twoja obserwacja "ledwo słychać
113|różnice"). Powody są trzy i po to są te pokretla:
114|
115|1. Bramka wpuszczenia IS: FDK w ogole rozważa IS tylko gdy `bitrate/pasmo < 5`.
116|   Przy wyższych bitrate'ach IS jest w ogole niedopuszczone. `--is-aggression >=1`
117|   zdejmuje te bramkę.
118|2. Prog korelacji (`--is-corr-thresh`, Q8, 256=1.0, default 243 ~= 0.95): oba
119|   kanały muszą byc do siebie podobne w danym pasmie w co najmniej ~95%, żeby IS
120|   sie włączył. To bardzo wysoko. Obniżasz -> IS lapie czesciej, nawet gdy kanały
121|   mniej podobne. Np. 180 (~0.70) = dużo agresywniej. Za nisko = słyszalne
122|   przekłamania kierunku.
123|3. Min. długość regionu (`--is-min-sfbs`, default 6): IS włącza sie dopiero na
124|   pasmie co najmniej 6 kolejnych SFB. Obniżasz do 1-2 -> IS lapie tez krótkie
125|   fragmenty.
126|
127|Zależność między nimi: żeby dany SFB poszedł w IS, MUSZA byc spełnione WSZYSTKIE
128|naraz — bramka wpuszczenia ORAZ korelacja powyżej progu ORAZ region odpowiednio
129|długi ORAZ kierunek stabilny. Dlatego samo obniżenie jednego progu często nic nie
130|daje (inny nadal blokuje) — i dlatego zwykle nie widac różnicy manipulując tylko
131|korelacja. `--is-aggression` rusza WSZYSTKIE naraz, spójnie.
132|
133|Jak ustawiac:
134|- Najprościej: `--is 1 --is-aggression 40` i słuchaj. Za malo IS -> podnos do 70,
135|  100. Za dużo (scena sie "skleja" w górze, artefakty kierunku) -> zejdź.
136|- 0 = domyślne FDK (praktycznie IS ledwo aktywne przy typowym bitrate).
137|- 100 = maksimum: bramka zdjeta, korelacja luzna (~0.475), region od 1 SFB,
138|  szeroka tolerancja kierunku. Duzo pasm w IS, mocno słyszalne, oszczędza bity.
139|- Ręczny tuning tylko gdy chcesz precyzji: ustaw `--is-aggression 0` i kreç
140|  `--is-corr-thresh` (glowny), potem `--is-min-sfbs`, na koncu `--is-lr-ratio`.
141|  Wartości --is-* NADPISUJA to co ustawil suwak agresywności.
142|- Diagnoza: `--verbose` pokazuje efektywne progi (IS corr threshold Q8, min SFBs),
143|  więc widzisz co realnie poszło do enkodera.
144|
145|Bias MS/IS (punkt 2): powyższe `--is-*` sterują tym KIEDY enkoder wybiera
146|intensity stereo (progi decyzji z tabeli strojenia FDK), niezależnie od twardego
147|wł/wył. `--msbands` ogranicza MS do dolnych pasm (poprawnie — maska i motylek
148|L/R->M/S są zsynchronizowane, brak artefaktu "lewy=center, prawy=reszta").
149|- Kompletnie niezależne stereo: `--msmask 0 --is 0`.
150|- "Laboratoryjne" ograniczenie MS do dolnych pasm: np. `--msbands 6`.
151|- Wymuszony pelny MS: `--msmask 1`.
152|- IS chętniejsze: obniż `--is-corr-thresh` (np. 150) i/lub `--is-min-sfbs`.
153|
154|### Zakres pasm MS: --msbands, --msbands-lo, --msbands-hi (WAZNE, często mylone)
155|
156|Pasma widma są numerowane OD DOŁU: pasmo 0 = najniższe częstotliwości (basy),
157|im wyższy numer, tym wyżej w widmie. W typowym LC stereo jest ich okolo 49.
158|
159|Są DWA niezależne sposoby ograniczenia, gdzie stosowane jest MS:
160|
161|1. `--msbands <n>` — "dolne N pasm". MS dozwolone TYLKO w pasmach 0..(n-1),
162|   czyli od basu w górę do numeru n. To jest zawsze liczone OD DOŁU.
163|   Przykład: `--msbands 6` = MS tylko na 6 najniższych pasmach, reszta czyste L/R.
164|
165|2. `--msbands-lo <lo>` + `--msbands-hi <hi>` — "zakres OD-DO". MS dozwolone TYLKO
166|   w pasmach o numerach od `lo` do `hi` włącznie. Poza tym zakresem czyste L/R.
167|   To para — podajesz oba. Pozwala umiescic MS GDZIEKOLWIEK, w tym na samej górze.
168|
169|Konkretny przykład (zakladajac ~49 pasm w LC):
170|- Chcesz MS TYLKO na 5 najWYŻSZYCH pasmach (np. scalic szum w górze, a dół
171|  zostawić w pelnym niezaleznym stereo)? Najwyższe pasma to numery 44..48:
172|  `--msbands-lo 44 --msbands-hi 48`.
173|- Chcesz MS tylko w SRODKU pasma (np. 10..30)? `--msbands-lo 10 --msbands-hi 30`.
174|- Chcesz MS na 6 najNIŻSZYCH? Prościej `--msbands 6` (albo `--msbands-lo 0
175|  --msbands-hi 5` — to samo).
176|
177|Zasada pamięciowa: `--msbands` = "od dołu do", `--msbands-lo/-hi` = "od..do".
178|Ile masz realnie pasm dla danego trybu/samplerate pokazuje `--verbose`
179|(pole "active SFBs").
180|
181|## 2. Cutoff (odcięcie) rdzenia AAC gdy działa SBR
182|
183|Standardowe `-w/--bandwidth` w FDK jest IGNOROWANE gdy aktywny jest SBR
184|(HE-AAC v1/v2) — bo `sbrEncoder_Init()` nadpisuje pasmo wartością z tabeli SBR.
185|
186|| Switch | Wartości | Domyślnie | Opis |
187||---|---|---|---|
188|| `--core-cutoff <hz>` | 0 = default, >0 = Hz | 0 | Wymusza pasmo rdzenia AAC W Hz nawet pod SBR. Odporny na nadpisanie przez SBR. |
189|
190|Przykład (Twoj przypadek — 7.5 kHz rdzenia przy HE-AAC v2 48 kbps, gdzie tabela
191|daje mniej):
192|```
193|fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
194|```
195|Zweryfikowane: `--core-cutoff 7500` -> efektywne pasmo 7500 Hz; stock `-w 7500`
196|pod SBR pozostaje 8613 Hz (ignorowane).
197|
198|UWAGA: sam pilnujesz limitow rdzenia. Maks. to Nyquist rdzenia (`sr/2`), a przy
199|**dual-rate SBR docelowy samplerate jest dzielony przez 2** — miej to na
200|uwadze przy doborze wartości.
201|
202|## 3. Gęstość / dokładność danych SBR
203|
204|Nadpisuje ustawienia z tabeli tuningowej SBR (po jej załadowaniu).
205|
206|| Switch | Wartości | Domyślnie | Opis |
207||---|---|---|---|
208|| `--sbr-start <n>` | -1 def, 0..15 | -1 | Indeks `bs_start_freq` (start pasma SBR). |
209|| `--sbr-stop <n>` | -1 def, 0..13 | -1 | Indeks `bs_stop_freq` (koniec pasma SBR). |
210|| `--sbr-freqscale <n>` | -1 def, 0..3 | -1 | Grupowanie częstotliwości (0 = liniowe, wyżej = drobniejsze log). |
211|| `--sbr-alterscale <n>` | -1 def, 0/1 | -1 | Alternatywna rozdzielczość skali. |
212|| `--sbr-noise-bands <n>` | -1 def, 1..5 | -1 | Liczba pasm szumu SBR (gęstość opisu szumu). |
213|| `--sbr-amp-res <n>` | -1 def, 0/1 | -1 | Rozdzielczość amplitudy obwiedni: 0 = 1.5 dB, 1 = 3.0 dB. |
214|| `--sbr-data-extra <n>` | -1 def, 0/1 | -1 | Zapis dodatkowych danych nagłówka SBR. |
215|| `--sbr-num-env <1\|2\|4>` | -1 off | -1 | Liczba obwiedni na ramke. WYMUSZA statyczna siatke czasowa (ignoruje detektor transientów). Więcej = lepsza rozdzielczość czasowa górnego pasma, gorzej na atakach. (8 przekracza standardowy grid — odrzucone.) |
216|| `--sbr-freqres-fixfix <0\|1>` | -1 off | -1 | Rozdzielczość częstotliwości obwiedni FIXFIX (0 low, 1 high). |
217|| `--sbr-stereo-mode <0..3>` | -1 off | -1 | Tryb stereo SBR: 0 mono, 1 LR (pelna separacja górnego pasma), 2 coupling (oszczędny, wspolna obwiednia + poziom), 3 switch-LRC (domyślnie koder wybiera per-ramke). Wymuś 1 dla max separacji, 2 dla oszczędności. |
218|| `--sbr-invf <0..3>` | -1 auto | -1 | Wymuś inverse filtering SBR: 0 off, 1 low, 2 mid, 3 high. Sterowane normalnie estymatorem tonalności. Wyżej = mocniejsze "wybielanie" tonalnego SBR (mniej metaliczności kosztem detalu). |
219|| `--sbr-noise-floor-offset <n>` | -128 off | -128 | Offset poziomu szumu SBR (mala l. calkowita). Wieksze = więcej szumu wypełniającego w rekonstrukcji SBR. |
220|| `--sbr-header-period <n>` | -1 off, >=1 | -1 | Liczba ramek między nagłówkami SBR = jak szybko górne pasmo SBR \"wchodzi\", gdy dekoder podłącza się do strumienia HE-AAC na żywo (Icecast/Shoutcast). KONFIGURACJA SBR jest w okresowym nagłówku, nie w każdej ramce; dekoder wpięty w środek gra sam rdzeń (przytłumiony) do nadejścia kolejnego nagłówka. `1` = nagłówek w każdej ramce → niemal natychmiastowy sync SBR (~23 ms); wyżej = dłuższy moment core-only. Domyślnie FDK ~10 ramek (~0.23 s HE dual-rate / ~0.46 s LC). FDK kapuje to do maks. raz na sekundę, więc bardzo duże wartości są przycinane (np. 40 → 21 ramek @44.1k). Efektywny okres w ms pokazuje `--verbose`. |
221|
222|UWAGA: `--sbr-start`/`--sbr-stop` są walidowane PRZEZ FDK — niepoprawna
223|KOMBINACJA start/stop (zła liczba pasm master) da "encoder initialization
224|failed". To ograniczenie samego SBR, nie buga. Dobieraj pary (np. dla 64k
225|stereo działa start=5 stop=9, start=8 stop=14).
226|
227|## 4. Parametric Stereo (HE-AAC v2)
228|
229|PS opisuje stereo kilkoma parametrami (IID/ICC...). Tu można nimi sterować,
230|nawet kosztem obrazu stereo.
231|
232|| Switch | Wartości | Domyślnie | Opis |
233||---|---|---|---|
234|| `--ps <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś wysylanie parametru IID PS. `0` = nigdy (spłaszcza obraz stereo), `1` = zawsze. Nadpisuje heurystykę różnicy głośności. |
235|| `--ps-iid-quant <n>` | -1 def, 0 coarse, 1 fine | -1 | Siatka kwantyzacji IID: gruba vs. dokładna. |
236|| `--ps-icc <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś ICC (Interchannel Coherence — podobieństwo/spójność kanałów) on/off. |
237|| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | Tryb rotacji ICC: 0 = ROT_A, 1 = ROT_B. To wyłącznie sygnalizacja — ta sama macierz, tylko inaczej wyliczana przez dekoder, więc traktuj to jako pokrętło kompatybilności, nie jakości. |
238|| `--ps-bands <n>` | 10 albo 20 | -1 (tabela bitrate) | Liczba pasm stereo PS = rozdzielczość **częstotliwościowa** parametrów stereo. Stock FDK wybiera ją wyłącznie z bitrate, więc od 22 kbps w górę zawsze dostajesz 20 i nigdy nie usłyszysz 10. Mniej pasm = zgrubniejszy obraz stereo, mniej bitów na parametry. |
239|| `--ps-env <n>` | 1, 2 albo 4 | -1 (tabela bitrate) | Liczba obwiedni parametrów PS na ramkę = rozdzielczość **czasowa** parametrów stereo. Powyżej 36 kbps stock FDK zawsze bierze 4. Więcej obwiedni wierniej nadąża za ruchomą panoramą i transjentami. |
240|| `--ps-env-reduce <n>` | 0, 1 | -1 (włączone) | `0` wyłącza automatyczną pętlę połowienia obwiedni (`envelopeReducible`). Domyślnie FDK zwija 4 obwiednie do 2 i do 1, gdy sąsiednie obwiednie wyglądają podobnie według zaszytego progu błędu — więc liczba obwiedni, którą ustawiłeś, często *nie* jest tym, co realnie leci w strumieniu. `0` sprawia, że `--ps-env` działa literalnie. |
241|| `--ps-noenv-skip <n>` | 0, 1 | -1 (włączone) | `0` zabrania ramek PS bez parametrów. Domyślnie FDK może wysłać do 10 kolejnych ramek **bez żadnych** parametrów stereo, gdy kolejne zestawy IID/ICC wyglądają podobnie — słychać to jako chwilowe zapadnięcie się obrazu stereo i powrót. `0` = wysyłaj zawsze. |
242|
243|UWAGA o rozdzielczości PS: `--ps-bands` i `--ps-env` to dwie osie, które zmieniają
244|*ile* parametrów stereo realnie leci w strumieniu — odpowiednio w częstotliwości
245|i w czasie. To czyni je znacznie bardziej słyszalnymi niż `--ps-icc-mode`, który
246|zmienia tylko sposób sygnalizacji tej samej macierzy. Pomiar na 4-sekundowej
247|próbce stereo z celowo ruchomą panoramą (0,25 Hz) i naprzemiennymi transjentami
248|L/R, kodowanej jako HE-AAC v2 przy 48 kbps, porównując trajektorię panoramy
249|zdekodowanego pliku z oryginałem:
250|
251|| Ustawienie | Błąd panoramy (RMS) | Korelacja z oryginałem |
252||---|---|---|
253|| stock (20 pasm / 4 obwiednie) | 0,177 | 0,9655 |
254|| `--ps-env 2 --ps-env-reduce 0` | 0,138 | 0,9896 |
255|| `--ps-env 4 --ps-env-reduce 0` | **0,117** | **0,9944** |
256|
257|Ciekawy jest wynik, że samo zażądanie 4 obwiedni nie daje NIC — wyjście stock i
258|`--ps-env 4` są bit-identyczne, bo automatyczna pętla natychmiast je z powrotem
259|zwija. Zysk pojawia się dopiero, gdy `--ps-env-reduce 0` tę pętlę zatrzyma: o 34 %
260|mniejszy błąd panoramy przy praktycznie tym samym rozmiarze pliku. Jeśli masz
261|zapamiętać jedną rzecz z tej grupy — zapamiętaj `--ps-env-reduce 0`.
262|
263|UWAGA o IPD/OPD: FDK koduje wyłącznie IID (różnice głośności) i ICC (koherencję).
264|Parametry *fazy* międzykanałowej nie są wysyłane — `ps_encode.cpp` dosłownie
265|wpisuje zera z komentarzem `"IPD OPD not supported right now"`. Warto jednak
266|wiedzieć, że enkoder już teraz liczy zarówno część rzeczywistą, jak i urojoną
267|widma skrośnego L/R (`pwrCr` / `pwrCi`) i używa tylko ich modułu — informacja o
268|fazie jest więc obecna, tylko wyrzucana; tablice Huffmana i writery bitstreamu
269|dla IPD/OPD również już istnieją w `ps_bitenc.cpp`.
270|
271|## 5. Substytucja/ksztaltowanie szumu — TNS / PNS / afterburner
272|
273|To, co w średnich bitrate'ach jest zastępowane szumem lub resyntezowane.
274|
275|| Switch | Wartości | Domyślnie | Opis |
276||---|---|---|---|
277|| `--tns-mask <n>` | -1 def (0xF), 0..15 | -1 | Maska włączenia TNS (bitowa, per typ bloku). |
278|| `--tns-order <n>` | -1 def, 1..12 | -1 | Maks. rząd filtra TNS (bloki short dodatkowo kapowane do 5). |
279|| `--pns <n>` | -1 def, 0/1 | -1 | Perceptual Noise Substitution wł/wył. UWAGA: FDK wymusza PNS=off gdy aktywne SBR albo VBR. |
280|| `--pns-start <hz>` | -1 def, Hz | -1 | Częstotliwość startowa PNS. Niżej = więcej widma zastępowane szumem. |
281|| `--force-pns` | flaga | off | Obejdz bramkę niskiego bitrate dla PNS. |
282|| `--pns-gain <x>` | >=0.0 | -1 (off) | Głośność dorabianego szumu PNS. `1.0` = bez zmian (energia szumu = oryginalne pasmo). `>1.0` = szum głośniejszy niż oryginał, `<1.0` = cichszy. Wprost skaluje energię kodowanego szumu — to pokrętło „jak głośny szum". Wejście dziesiętne. |
283|| `--pns-tonality <x>` | >=0.0 | -1 (off) | Skaluje prog detekcji tonalności PNS. `1.0` = domyślnie; wyżej = więcej (nawet mniej-szumiacych) pasm kwalifikuje się do PNS = SZERSZY szum. Wejście dziesiętne. |
284|| `--pns-refpower <x>` | >=0.0 | -1 (off) | Skaluje prog mocy referencyjnej detekcji PNS. `1.0` = domyślnie. Wejście dziesiętne. |
285|| `--pns-gapfill <x>` | >=0.0 | -1 (off) | Skaluje prog wypełniania luk PNS (wypełnia dziury PNS między dwoma pasmami PNS). `1.0` = domyślnie. Zaawansowane/subtelne — rzadko widoczne. Wejście dziesiętne. |
286|| `--pns-min-width <n>` | -1 off, >=1 | -1 | Minimalna szerokość SFB dla PNS. Skuteczny powyżej wbudowanej domyślnej (LC=16); np. 32/64 ogranicza PNS do szerszych pasm. |
287|
288|WAZNE o PNS przy niskim bitrate: FDK ma tabele tuningowa (`levelTable`), która
289|CALKOWICIE wyłącza PNS poniżej ~28 kbps (wiersz bitrate 0-27999 = same zera dla
290|kazdego samplerate). Dlatego przy 24 kbps `--pns`/`--pns-start` nie robia NIC (audio
291|brzmi "jak MP3/MDCT"), a przy 64 kbps różnica jest duza. `--force-pns` omija te
292|bramkę (używa pierwszego aktywnego wiersza tabeli), więc PNS działa tez przy 24k.
293|Ograniczenie FDK: PNS i tak wymaga włączonego TNS i trybu nie-VBR — inaczej jest
294|zerowane wyżej w łańcuchu (nic na to nie poradzimy bez głębszej przebudowy).
295|
296|## 6. Maskowanie / ATH
297|
298|| Switch | Wartości | Domyślnie | Opis |
299||---|---|---|---|
300|| `--ath-scale <n>` | 1..~4096 (Q8) | 256 | Skala progu maskowania w Q8 (256 = x1.0). `>256` podnosi progi (więcej szumu, mniej bitow na pasmo), `<256` obniża (czysciej, więcej bitow). Działa w domenie ld64 FDK jako addytywny offset log2. |
301|| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = mniej maskowania = więcej detalu. Największy efekt gdzie bity ograniczone (96-192k). |
302|| `--minsnr-scale <n>` | 1..bez limitu (Q8) | -1 (off) | Styl MusePack: skaluje WYMAGANY per-pasmo SNR kodowania (`sfbMinSnrLdData`, najbliższy FDK-owy odpowiednik TMN/NMT). `<256` = wymagaj WYŻSZEGO SNR = więcej detalu/bitow; `>256` = zgrubniej. Skuteczniejszy niż `--ath-scale`, bo to do min-SNR logika avoid-holes cofa progi. 256=off. |
303|| `--minsnr-clamp-hi <n>` | 1..bez limitu (Q8) | -1 (off) | Skaluje sufit MAX_SNR FDK (~−1 dB). `>256` pozwala pasmom wymagać więcej niż fabryczny cap. 256=off. |
304|| `--minsnr-clamp-lo <n>` | 1..bez limitu (Q8) | -1 (off) | Skaluje podłogę MIN_SNR FDK (~−25 dB). 256=off. |
305|| `--reduce-clamp <0\|1>` | 0, 1 | 1 (on) | `0` zdejmuje sufit \"29 dB Ratio\" redukcji progów w kwantyzatorze CBR, pozwalając wepchnąć progi głębiej (więcej bitow do wymagających pasm). Łączy się z `--minsnr-scale` dla ekstremalnego detalu. Tylko CBR (VBR używa innej ścieżki). |
306|
307|### Co realnie pomaga w niskim i średnim bitrate (10-144 kbps)
308|
309|Częste pytanie: czy da sie jeszcze cos wycisnac na dokładności/efektywności
310|kodowania (Huffman, iteracje kwantyzacji itp.)? Uczciwa odpowiedz po przejrzeniu
311|kodu FDK:
312|
313|- Kodowanie Huffmana (łączenie sekcji, wybór codebookow w `dyn_bits.cpp`) jest juz
314|  optymalne (zachlanne łączenie sekcji dajace min. bitow). Nie ma tam sensownego
315|  pokretla — a wystawienie tego tylko by pogarszalo wynik.
316|- Pętla iteracji kwantyzacji (`maxIterations`) to mechanizm RATUNKOWY przy
317|  niedoborze bitow; zwiekszanie jej nic nie daje (szczegóły w manualu, rozdz. 9a).
318|- Wewnętrzne progi (adaptacja minSnr, `bits2PeFactor`) to arytmetyka staloprzecinkowa
319|  z twardymi zakresami — ruszanie ich grozi niestabilnością, nie poprawa.
320|
321|REALNY zestaw dźwigni jakości dla 10-144 kbps jest JUZ wystawiony:
322|- `--ath-scale <256` — globalnie obniż prog maskowania (więcej detalu za bity).
323|- `--spread-mask <256` — mniej maskowania międzypasmowego (więcej pasm kodowanych).
324|- `--side-bias >0` — więcej bitow do kanału side (czystsza szerokość stereo).
325|- `--is-aggression` — steruj intensity stereo (kluczowe przy niskim bitrate).
326|- `--force-pns` + `--pns-start` — kontrola szumu przy bardzo niskim bitrate.
327|- pod HE-AAC: `--sbr-invf`, `--sbr-noise-floor-offset`, `--speech` (mowa).
328|
329|To nie brak funkcji — to te same dźwignie, których używa profesjonalny tuning,
330|tyle ze ręcznie. Zacznij od `--ath-scale` i `--spread-mask`, po jednej naraz.
331|
332|## 7. Bias przełączania blokow short/long (dowolny profil)
333|
334|| Switch | Wartości | Domyślnie | Opis |
335||---|---|---|---|
336|| `--block-bias <n>` | 0..255 | -1 (off) | Przesuwa prog decyzji short/long. 128 = domyślne enkodera (bez zmian), >128 faworyzuje bloki krótkie (bardziej "transient"), <128 faworyzuje długie, 0 = praktycznie tylko długie. |
337|
338|WAZNE: `--block-bias` zawsze produkuje strumien zgodny ze standardem (przesuwa
339|prog detekcji ataku, nie wymusza na sile typu bloku). Zastąpił dawne
340|`--allshort`/`--alllong`, które tworzyły NIELEGALNY strumien (twarde wymuszenie
341|short window bez przeliczenia SFB/grupowania -> dekoder odrzucał, Winamp
342|"skakał jak po porysowanej płycie"). Jesli chcesz maksimum długich: `--block-bias 0`;
343|maksimum krótkich: `--block-bias 255`.
344|
345|## 8. Bias decyzji MS (uczciwie: narzędzie o SŁABYM efekcie)
346|
347|| Switch | Wartości | Domyślnie | Opis |
348||---|---|---|---|
349|| `--ms-bias <n>` | 0..255 (Q8) | -1 (off) | *(Bardzo eksperymentalny — raczej niepotrzebny, użyj `--side-bias`.)* Przesuwa prog decyzji L/R vs MS. Q8, 128 = +0.5 w jednostkach ld64 FDK. >0 = MS chętniejsze. Reaguje juz od ~32 (po rekalibracji skali). Do realnego strojenia balansu stereo właściwym narzędziem jest `--side-bias`, nie ten bias. |
350|
351|UCZCIWIE o `--ms-bias` — to najsłabsze narzędzie z całego zestawu i teraz wiadomo
352|dlaczego "niewiele robi". MS (mid/side) to transformacja BEZSTRATNA: mid=L+R,
353|side=L-R odtwarza sie dokładnie z powrotem na L/R. Włączenie/wyłączenie MS na
354|danym pasmie NIE zmienia brzmienia — zmienia tylko ILE BITOW zajmie zapis. Enkoder
355|i tak wybiera bliska optymalnej decyzję per pasmo; `--ms-bias` tylko przesuwa
356|kilka GRANICZNYCH pasm. Pomiar (korelacja L/R po dekodowaniu, rozmiar ADTS):
357|efekt rzędu <0.1% rozmiaru i zmian korelacji w 4. miejscu po przecinku.
358|
359|Uwaga techniczna: w poprzedniej wersji skala biasu byla ~256x za słaba (mnożnik
360|<<15 zamiast <<23) — stąd "128 nic nie robilo, dopiero 2048 cos ruszalo". Teraz
361|128 = realne +0.5 ld64 jak w opisie, więc reaguje od ~32. Ale nawet poprawnie
362|wyskalowany bias ma z natury maly wpływ (patrz wyżej).
363|
364|Chcesz REALNIE sterować MS? Użyj twardych przełączników, nie biasu:
365|- `--msmask 0` — WYŁĄCZ MS całkowicie (czyste, niezależne L/R). To właściwy
366|  wybór do center-cancel / usuwania wokalu (zero mieszania kanałów przez koder).
367|- `--msmask 1` — wymuś MS na wszystkich pasmach (maks. oszczędność bitow).
368|- `--msbands` / `--msbands-lo/-hi` — ogranicz MS do wybranych pasm.
369|Pomiar: msmask 0 vs 1 daje ~900 B różnicy na 2s probce; ms-bias tylko ~2 B.
370|
371|## 9. Quasi-constrained VBR (silnik CBR + szersze oddychanie)
372|
373|WAZNE: bez tych switchy CBR jest 100% NIEZMIENIONY (zweryfikowane: bit-identyczny
374|z oryginalna binarka). Włączasz je świadomie.
375|
376|Jak AAC oddycha: nawet w CBR ramki pożyczają z bit-reservoir, więc jedna ramka
377|może miec ~122 kbps a następna ~141, byle średnia = target. Te pokretla
378|poszerzają/ograniczaja to oddychanie.
379|
380|TWARDY SUFIT dla wszystkiego: ramka AAC miesci MAX 6144 bitow na kanał
381|(=768 bajtow/kanał); stereo => 12288 bitow/ramke. Przy 44100 Hz jedna ramka =
382|1024 probki = ~23.2 ms, więc bity/ramke = kbps * 23.22. (Np. 128k stereo:
383|średnia ~2972 bitow/ramke; sufit 12288.)
384|
385|| Switch | Wartości | Domyślnie | Opis |
386||---|---|---|---|
387|| `--vbr-reservoir <bity>` | 0..(6144*kanały - średnia) | -1 (off) | Rozmiar bit-reservoir. Więcej = większy rozrzut ramek wokol średniej. min 0 (trzymaj sie targetu ciasno). Auto-clamp do sufitu - nie przegniesz. Bezpieczny start: 2-3x default. |
388|| `--peak-bitrate <bps>` | > target | -1 (off) | Dopuszcza krótkie szczyty do tej wartości, trzymając średnia. Ustaw POWYŻEJ `-b` (np. -b 128000 --peak-bitrate 160000). Poniżej targetu ignorowane. |
389|| `--max-bits-frame <bity>` | średnia..12288(st.) | -1 (off) | Twardy sufit bitow w JEDNEJ ramce. Musi byc >= średnia i <= 6144*kanały (inaczej clamp). Rozsądny cap ~1.5x średnia (~4500 dla 128k st.). Za nisko = głodzi głośne ramki (słyszalne). |
390|| `--min-bits-frame <bity>` | 0..średnia | -1 (off) | Twarda podłoga bitow/ramke. Wyższa podłoga marnuje bity na ciszę. Zostaw 0 chyba ze eksperymentujesz. |
391|| `--bitres-mode <n>` | 0/1/2 | -1 (def) | Tryb reservoir: 0 pelny (jak default), 1 zredukowany, 2 wyłączony (sztywny, najbliżej twardego CBR per-ramka). |
392|
393|JAK USTAWIAC OPTYMALNIE (dla mniej doświadczonych - żeby nie przegiąć):
394|- Bezpieczny quasi-CVBR ~128k stereo: `-b 128000 --peak-bitrate 160000 --vbr-reservoir 6000`.
395|- NIE ustawiaj `--max-bits-frame` PONIŻEJ średniej ani `--min-bits-frame` POWYŻEJ
396|  średniej - to walczy z targetem i psuje jakość.
397|- `--vbr-reservoir` jest auto-clampowany do sufitu, więc bezpiecznie eksperymentować.
398|- Zmierzone realnie (sygnal 4s zmienny, 128k stereo): CBR default rozrzut 95-167
399|  kbps; z `--vbr-reservoir 8000 --peak-bitrate 192000` rozrzut 36-158 kbps
400|  (mocniej oddycha, średnia trzymana); `--bitres-mode 2` rozrzut 127.8-128.2
401|  (sztywny). Wszystkie w pelni dekodowalne.
402|- Ograniczenie: to silnik CBR+reservoir, nie prawdziwy ABR jak LAME. Wahania
403|  umiarkowane (limit 6144 bitow/kanał), ale to jest ten "lekki lot" AAC.
404|
405|## 10. Audiofilskie / ekstremalne (opt-in, poza typowym zakresem)
406|
407|| Switch | Wartości | Domyślnie | Opis |
408||---|---|---|---|
409|| `--uncap-bandwidth` | flaga | off | Zdejmij twardy cap 20 kHz rdzenia. `--core-cutoff` może wtedy sięgnąć az do Nyquista. |
410|| `--is-aggression <0..100>` | 0..100 | -1 (off) | Suwak agresywności IS (patrz sekcja 1). |
411|| `--force-pns` | flaga | off | PNS poniżej bramki ~28 kbps (patrz sekcja 5). |
412|| `--unlock-bitrate` | flaga | off | Zdejmij DOLNY prog bitrate. Pozwala na skrajnie niskie: 8k HE-AAC stereo, 6k LC. WAZNE: w tym trybie `-b` bierzemy DOSLOWNIE jako bps (bez konwencji nu774 x1000), więc `-b 6000` = 6000 bps. Górny sufit 6144*kanały zostaje (twardy limit AAC). Rezydualny floor ~10 kbps = minimum nagłówków AAC. |
413|| `--speech` | flaga | off | Tryb strojenia SBR pod MOWE ludzka (inne progi inverse filtering, poziom szumu, bez parametric coding). Dotyczy HE-AAC (SBR); LC nie ma osobnego trybu mowy. Dla czystej mowy w niskim bitrate. |
414|| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = MNIEJ maskowania = więcej detali (odpowiednik luzowania tone-masks-noise). Największy efekt gdzie bity ograniczone (96-192k). 256=bez zmian. Łącz z `--ath-scale <256`. |
415|
416|PASMO POWYŻEJ 20 kHz (audiofilskie): FDK ma zaszyty cap `min(20000, sr/2)` na
417|pasmo rdzenia — nawet przy wejściu 96 kHz i wysokim bitrate realnie nic powyżej
418|20 kHz nie jest kodowane (Twoje podejrzenie bylo trafne). `--uncap-bandwidth` znosi
419|ten cap; wtedy `--core-cutoff` steruje pasmem az do sr/2.
420|
421|Zmierzone (96 kHz wejście, LC 400k, szum szerokopasmowy):
422|- bez uncap, `--core-cutoff 40000`: verbose 20000 Hz, energia >20 kHz ~= 0%.
423|- `--core-cutoff 40000 --uncap-bandwidth`: verbose 40000 Hz, energia 20-24 kHz
424|  ~10%, 24-32 kHz ~20%, 32-44 kHz ~20% — pelne pasmo do 40 kHz realnie kodowane.
425|
426|Preset audiofilski (pelne pasmo + ręczne maskowanie, dla 190+ kbps):
427|```
428|fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 \
429|    --ath-scale 200 -o out.m4a in96k.wav
430|```
431|`--ath-scale <256` obniża progi maskowania (czysciej, więcej bitow na detale) —
432|sensowne gdy masz duzy zapas bitrate. Uwaga: pasmo >20 kHz i skrajne ustawienia
433|mogą byc odrzucone przez czesc dekoderow (poza typowa specyfikacja) — świadomy
434|opt-in.
435|
436|---
437|
438|## 11. Kontener MP4/M4A — które boxy są konieczne, a co można wyciąć
439|
440|Plik .m4a to zestaw zagnieżdżonych "boxow" (atomow). Sprawdzone, co jest czym:
441|
442|OBOWIAZKOWE (bez nich plik jest NIEGRYWALNY — nie ma do nich przełączników):
443|`ftyp`, `mdat` (surowe dane audio), oraz szkielet `moov` → `mvhd` + `trak` →
444|`tkhd` → `mdia` (`mdhd`/`hdlr`/`minf` → `smhd`/`dinf`/`stbl` z tabelami
445|`stsd`/`stts`/`stsc`/`stsz`/`stco`). To jest minimalna mapa pliku wymagana przez
446|standard ISO — kazdy dekoder tego potrzebuje, żeby w ogole znaleźć i odtworzyć
447|dźwięk. Wycinanie ich nie ma sensu (efekt: uszkodzony plik).
448|
449|OPCJONALNE (można wyłączyć) — cały blok `udta` → `meta` → `ilst`, czyli:
450|- tag identyfikacyjny kodera (`©too`, teraz "PompAAC based on …"),
451|- `iTunSMPB` — dane o opóźnieniu kodera do bezszwowego (gapless) odtwarzania,
452|- wszystkie tagi (tytuł, artysta, album itd.).
453|
454|| Switch | Działa na | Opis |
455||---|---|---|
456|| `--no-tool-tag` | .m4a | Nie zapisuj tagu identyfikującego koder (`©too`). Reszta tagow i gapless zostaje. |
457|| `--minimal-moov` | .m4a | Najmniejszy legalny .m4a: pomija CALY blok `udta`/metadata (tag kodera + gapless iTunSMPB + wszystkie tagi). Szkielet odtwarzania zostaje nienaruszony. |
458|
459|Ile to oszczędza (2 s, 128k stereo, pomiar): default ~34381 B →
460|`--no-tool-tag` ~34297 B (−84) → `--minimal-moov` ~34048 B (−333). To male
461|liczby — narzut MP4 to glownie obowiazkowy szkielet, którego usunąć sie nie da.
462|Chcesz naprawde zero narzutu kontenera? Użyj surowego ADTS: `-f 2 -o out.aac`
463|(strumien bez żadnych boxow, ale tez bez tagow i gapless).
464|
465|UWAGA gapless: `--minimal-moov` usuwa iTunSMPB, więc przy łączeniu utworow mogą
466|pojawić sie mikro-przerwy (encoder delay nie bedzie zasygnalizowany). Do
467|zwykłego odsłuchu bez znaczenia; do płyt "bez przerw" zostaw domyślne.
468|
469|---
470|
471|## 12. Legenda odczytu `--verbose`
472|
473|`--verbose` wypisuje SUROWE wartości (bez podpowiedzi w nawiasach, żeby nie
474|zaśmiecać). Poniżej co oznaczają te, które nie są oczywiste:
475|
476|| Pole | Jak czytac |
477||---|---|
478|| `AOT (profile)` | 2=AAC-LC, 5=HE-AAC, 29=HE-AAC v2, 23=AAC-LD, 39=AAC-ELD. |
479|| `bitrate-mode` | 0=CBR, 1..5=VBR (wyżej=lepiej). |
480|| `channel-mode` | 1=mono, 2=stereo (dla HE-AAC v2 rdzen jest mono, stereo robi PS). |
481|| `core bandwidth` | Górna częstotliwość rdzenia AAC, **zakotwiczona na najbliższej granicy SFB** (realny cutoff, który może różnić się od podanej wartości `-w`/`--core-cutoff`, np. `-w 17300` → `17915 Hz (SFB-anchored)`). W nawiasie ZRODLO: `from -w`, `from --core-cutoff`, albo `auto`. Pod SBR to tylko rdzen — SBR gra wyżej. |
482|| `final BW (AAC+SBR)` | Pokazywane tylko gdy SBR aktywny: orientacyjna GÓRNA częstotliwość całego sygnalu (rdzen + SBR), wyliczona ze `sbr stop freq index`. To odpowiednik `core bandwidth`, ale dla pelnego pasma HE-AAC. |
483|| `signaling-mode` | Sposób sygnalizacji SBR/PS: 0=implicit, 1=explicit backward-compat, 2=explicit hierarchical, auto=biblioteka wybiera. |
484|| `SBR mode` | Wewnętrzny tryb SBR (-1/0 gdy nieuzywany). |
485|| `sbr-ratio` | 1=downsampled (single-rate), 2=dual-rate (rdzen na polowie częstotliwości). |
486|| `sbr amp res` | 0=1.5 dB, 1=3.0 dB (rozdzielczość amplitudy obwiedni). |
487|| `granule/frame length` | Długość ramki w probkach (1024 dla LC, 512/480 dla LD/ELD). |
488|| `codec delay` | Opóźnienie kodeka w probkach/kanał (total i sam rdzen). Do gapless. |
489|| `IS corr threshold` / `IS L/R ratio` | Progi w skali Q8: 256 = 1.0. Niższy prog korelacji = intensity stereo CHĘTNIEJSZE (odwrotnie do intuicji). |
490|| `IS min contiguous SFBs` | Ile sąsiadujących pasm musi sie "zgodzić", zanim włączy sie IS. |
491|| `TNS mask` | Maska bitowa 0x0..0xF które filtry TNS aktywne. |
492|| `MS/IS bands: auto up to N` | Górny indeks pasma, do którego narzędzie może działać. |
493|| `franken overrides applied` | Lista przełączników które w TYM uruchomieniu odbiegają od czystego FDK (albo "none"). |
494|
495|Wartości Q8 (jak `IS corr threshold`, `--ms-precision`, `--ms-bias`,
496|`--ath-scale`, `--spread-mask`) to liczby staloprzecinkowe gdzie 256 = 1.0;
497|np. 243 oznacza 243/256 ≈ 0.95.
498|
499|---
500|
501|## 13. Tabele referencyjne (z tablic strojenia FDK)
502|
503|Trzy tabele orientacyjne, żeby świadomie dobierać `--msbands`, `--sbr-start/stop`
504|i `-w`. Wartości wyliczone z tablic w kodzie FDK; są PRZYBLIZONE (siatka SFB jest
505|schodkowa), ale pokazuja właściwy rząd wielkości.
506|
507|### Tabela 1 — orientacyjna górna częstotliwość pasma (SFB) [Hz]
508|
509|Pasma numerowane od dołu (0=bas). Pokazano co 4. pasmo; ostatni wiersz = liczba
510|pasm i częstotliwość Nyquista. Użyteczne przy `--msbands`/`--isbands`/`--msbands-lo/-hi`.
511|
512|| SFB | 16 kHz | 22.05 kHz | 32 kHz | 44.1 kHz | 48 kHz | 96 kHz |
513||----:|-------:|----------:|-------:|---------:|-------:|-------:|
514|| 0   | 62   | 43   | 62   | 86    | 94    | 188   |
515|| 4   | 312  | 215  | 312  | 431   | 469   | 938   |
516|| 8   | 562  | 388  | 562  | 775   | 844   | 1688  |
517|| 12  | 875  | 646  | 1000 | 1378  | 1500  | 2438  |
518|| 16  | 1250 | 991  | 1500 | 2067  | 2250  | 3750  |
519|| 20  | 1656 | 1335 | 2250 | 3101  | 3375  | 5625  |
520|| 24  | 2188 | 1852 | 3375 | 4651  | 5062  | 8062  |
521|| 28  | 2875 | 2584 | 5000 | 6891  | 7500  | 12938 |
522|| 32  | 3844 | 3618 | 7000 | 9647  | 10500 | 24000 |
523|| 36  | 5188 | 5039 | 9000 | 12403 | 13500 | 36000 |
524|| 40  | 7000 | 7020 | 11000| 15159 | 16500 | 48000 |
525|| 44  | —    | 9647 | 13000| 17916 | 19500 | —     |
526|| 48  | —    | —    | 15000| 22050 | 24000 | —     |
527|| **liczba pasm / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |
528|
529|### Tabela 2 — SBR: indeks start freq → orientacyjna częstotliwość przejścia [Hz]
530|
531|To częstotliwość, od której SBR przejmuje pasmo powyżej rdzenia AAC (`--sbr-start`,
532|indeks 0..15; niższy = SBR startuje niżej = wezszy rdzen). "core" to częstotliwość
533|rdzenia; przy dual-rate wyjście jest dwa razy wyższe (np. core 24k → wyjście 48k).
534|
535|| indeks start | core 16 kHz | core 24 kHz | core 32 kHz | core 44.1 kHz | core 48 kHz |
536||----:|----:|----:|----:|----:|----:|
537|| 0 | 2750 | 2250 | 2500 | 1378 | 1500 |
538|| 1 | 3000 | 2625 | 3000 | 2067 | 2250 |
539|| 2 | 3250 | 3000 | 3500 | 2756 | 3000 |
540|| 3 | 3500 | 3375 | 4000 | 3445 | 3750 |
541|| 4 | 3750 | 3750 | 4500 | 4134 | 4500 |
542|| 5 | 4000 | 4125 | 5000 | 4823 | 5250 |
543|| 6 | 4250 | 4500 | 5500 | 5512 | 6000 |
544|| 7 | 4500 | 4875 | 6000 | 6202 | 6750 |
545|| 8 | 4750 | 5250 | 6500 | 6891 | 7500 |
546|
547|Stop freq (`--sbr-stop`, 0..13) działa analogicznie na górnej granicy SBR — wyższy
548|indeks = SBR sięga wyżej (bliżej Nyquista wyjścia). Domyślnie biblioteka dobiera
549|oba pod bitrate.
550|
551|### Tabela 3 — AAC-LC: orientacyjne odcięcie (`-w`/auto) wg bitrate na kanał [Hz]
552|
553|Gdy nie podasz `-w`, FDK dobiera pasmo z tej tablicy wg bitrate NA KANAL (stereo
554|128k = 64k/kanał). Wartości interpolowane; kolumna mono i stereo różna. Pomaga
555|ocenic, czy `-w` ma sens (podawanie wyższego niż auto ma efekt tylko jesli jest zapas bitow).
556|
557|| bitrate/kanał | pasmo (mono) | pasmo (stereo+) |
558||----:|----:|----:|
559|| 0–12 kbps  | 3700  | 5000  |
560|| 20 kbps    | 6900  | 9640  |
561|| 28 kbps    | 9600  | 13050 |
562|| 40 kbps    | 12060 | 14260 |
563|| 56 kbps    | 13950 | 15500 |
564|| 72 kbps    | 14200 | 16120 |
565|| ≥96 kbps   | 17000 | 17000 |
566|
567|Uwaga: ta tablica jest WSPOLNA dla 32/44.1/48 kHz i wyższych — FDK indeksuje ja
568|bitratem na kanał, nie samplerate (samplerate wpływa tylko na górny limit =
569|Nyquist). Dla LC bez SBR realny sufit to ~17 kHz z auto; wyżej tylko przez `-w`
570|(z zapasem bitow) lub `--uncap-bandwidth` przy sr≥96k.
571|
572|---
573|
574|## 14. Wyjście DAB+ (`--dab`, `--dab-label`)
575|
576|Dedykowany tryb wyjściowy, który zamiast pliku MP4/M4A czy surowego strumienia
577|ADTS emituje strumień radia cyfrowego DAB+. Enkoder przygotowuje dźwięk AAC
578|dokładnie tak, jak wymaga tego system DAB+ (transformata 960 próbek, super-ramka
579|120 ms, zabezpieczenie przed błędami), więc strumień można podać wprost do
580|multipleksera takiego jak `odr-dabmux` → ETI → nadajnik (albo do programowego
581|odbiornika w rodzaju welle.io / dablin).
582|
583|DAB+ to nie jest "AAC w innym pudełku": używa transformaty MDCT na 960 próbkach
584|(nie 1024), pakuje dźwięk w **super-ramki** po 120 ms, strzeże nagłówka
585|**firecode'em** (CRC Fire) i chroni ładunek kodem **Reed-Solomon RS(120,110)** nad
586|GF(256). Wyjściem jest surowy strumień `.dabp` — kolejne super-ramki jedna po
587|drugiej — czyli dokładnie to, co połyka multiplekser DAB+.
588|
589|### `--dab`
590|Włącza wyjście super-ramek DAB+. Wynikiem jest surowy strumień `.dabp` (bez MP4,
591|bez ADTS). Ograniczenia narzucone przez standard:
592|
593|| Wymaganie | Wartość |
594||---|---|
595|| Częstotliwość próbkowania | MUSI być `32000` lub `48000` Hz |
596|| Bitrate | wielokrotność 8 kbps, zakres 8..192 kbps |
597|| Kanały | mono lub stereo |
598|| Profile | AAC-LC, HE-AAC, HE-AAC v2 (wszystkie trzy) |
599|
600|Profil (AOT) dobierany jest AUTOMATYCZNIE z bitrate i liczby kanałów, tak samo jak
601|robi to `odr-audioenc` — zwykle nie ustawiasz `-p`:
602|
603|- stereo ≤48 kbps (subkanał ≤6) → **HE-AAC v2** (PS),
604|- mono ≤64k lub stereo ≤80k → **HE-AAC** (SBR),
605|- wyżej → **AAC-LC**.
606|
607|Profil można nadal wymusić przełącznikiem `-p` (`2`=LC, `5`=HE-AAC, `29`=HE-AAC
608|v2), jeśli wiesz, czego chcesz.
609|
610|### `--dab-label <tekst>`
611|Statyczna etykieta **DLS** (Dynamic Label Segment) niesiona jako X-PAD wewnątrz
612|super-ramki; odbiorniki DAB+ pokazują ją jako nazwę stacji / tytuł. Do ~48 znaków
613|(trzy segmenty w jednym PAD). "Statyczna" znaczy jeden stały tekst przez cały plik
614|— etykiety zmienne w czasie oraz pokaz slajdów MOT (model `ODR-PadEnc`) to plan na
615|przyszłość. Bez `--dab` etykieta jest ignorowana.
616|
617|### Przykłady
618|
619|```
620|# 48k/32k stereo, ~96k → auto AAC-LC:
621|fdkaac --dab -b 96 -o out.dabp input.wav
622|
623|# → auto HE-AAC (SBR):
624|fdkaac --dab -b 64 -o out.dabp input.wav
625|
626|# → auto HE-AAC v2 (PS):
627|fdkaac --dab -b 32 -o out.dabp input.wav
628|
629|# z etykietą stacji:
630|fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav
631|
632|# łańcuch nadawczy:
633|# out.dabp → odr-dabmux → ETI → nadajnik / dekoder
634|```
635|
636|Uwaga: bez `--dab` enkoder zachowuje się dokładnie jak dotychczas — zero wpływu,
637|bit-identycznie ze stockiem. Zweryfikowane: strumienie dekodują się niezależnym
638|dekoderem faad2 (dablin), etykieta DLS jest odczytywalna, a wyjście LC jest
639|bit-identyczne z referencyjnym `odr-audioenc`. Przetestowano dziewięć kombinacji
640|(48/32 kHz × mono/stereo × trzy profile), każda dekodowalna niezależnie.
641|
642|---
643|
644|## Przykłady
645|
646|```
647|# Punkt wyjscia — co ustawil enkoder:
648|fdkaac-franken-x64.exe -p 29 -b 48000 --verbose -o out.m4a in.wav
649|
650|# Quasi-constrained VBR ~128k (bezpieczny preset):
651|fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav
652|
653|# Agresywne intensity stereo przy 64k:
654|fdkaac-franken-x64.exe -p 2 -b 64000 --is 1 --is-aggression 70 -o out.m4a in.wav
655|
656|# PNS na sile przy 24k (inaczej bramka FDK je wyłącza):
657|fdkaac-franken-x64.exe -p 2 -b 24000 --pns 1 --force-pns -o out.m4a in.wav
658|
659|# Audiofilskie pelne pasmo 40 kHz z wejscia 96 kHz:
660|fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav
661|
662|# MS na 5 najwyzszych pasmach + plytsze dziury (nie najnizsze):
663|fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 --ms-precision 448 -o out.m4a in.wav
664|
665|# Skrajnie niski HE-AAC v2: 8000 bps stereo 48 kHz:
666|fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav
667|
668|# SBR: pelna separacja stereo LR + wymuszony inverse filtering:
669|fdkaac-franken-x64.exe -p 5 -b 96000 --sbr-stereo-mode 1 --sbr-invf 2 -o out.m4a in.wav
670|
671|# Twoj przypadek: 7.5 kHz rdzenia pod HE-AAC v2 48k:
672|fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
673|
674|# Kompletnie niezalezne stereo (bez MS/IS) na LC 128k:
675|fdkaac-franken-x64.exe -p 2 -b 128000 --msmask 0 --is 0 -o out.m4a in.wav
676|
677|# MS tylko na 6 dolnych pasmach:
678|fdkaac-franken-x64.exe -p 2 -b 96000 --msmask 1 --msbands 6 -o out.m4a in.wav
679|
680|# Tylko krotkie bloki + ograniczony TNS:
681|# Tylko dlugie bloki (bias) + ograniczony TNS:
682|fdkaac-franken-x64.exe -p 2 -b 96000 --block-bias 0 --tns-order 2 -o out.m4a in.wav
683|
684|# Agresywniejsze maskowanie (progi x2) + wczesniejszy PNS:
685|fdkaac-franken-x64.exe -p 2 -b 80000 --ath-scale 512 --pns 1 --pns-start 4000 -o out.m4a in.wav
686|
687|# Gestszy opis szumu SBR + dokladniejsza amplituda:
688|fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-noise-bands 5 --sbr-amp-res 0 -o out.m4a in.wav
689|
690|# HE-AAC v2 z wyłączonym PS (splaszczone stereo):
691|fdkaac-franken-x64.exe -p 29 -b 32000 --ps 0 -o out.m4a in.wav
692|```
693|
694|---
695|
696|## Jakość domyślna vs oryginalna binarka (fdkaac2.exe)
697|
698|Weryfikacja (21.07.2026) czy domyślne ustawienia franken = oryginalna dostarczona
699|binarka (dBpoweramp R17, ta sama wersja libfdk-aac 4.0.1 / pakiet 2.0.3):
700|- AAC-LC: **bit-identyczne** (ten sam md5) — zero regresji.
701|- HE-AAC v1/v2: bajty sie różnią, ALE: widmo HE-AAC v2 identyczne co do 0.000 dB,
702|  a błąd HE-AAC v1 wzgledem oryginału jest praktycznie taki sam jak w oryginale
703|  (7.05e11 vs 7.05e11). Różnica bajtow wynika z innego kompilatora (mingw/gcc vs
704|  MSVC) i zaokrągleń fixed-point, NIE z gorszej jakości. Słyszalna "inność" SBR
705|  to inna, równie poprawna realizacja, nie strata jakości.
706|
707|---
708|
709|## Testy (make check)
710|
711|Upstream fdk-aac/nu774 nie maja testów jednostkowych (`make check` w nich = no-op).
712|Ten projekt ma WŁASNY funkcjonalny test suite w `tests/check.sh`, uruchamiany:
713|
714|```
715|make check
716|```
717|
718|Sprawdza (na binarkach x64 + x86 jesli obecne): kompletność switchy w --help,
719|dekodowalność strumienia dla kazdego przełącznika (ffmpeg, 0 błędów), realne
720|działanie quasi-CVBR (CVBR oddycha szerzej niż sztywny bitres-mode2), verbose bez
721|wartości -1, oraz BRAK REGRESJI (domyślny CBR bez franken-flag = ADTS
722|bit-identyczny z oryginalna fdkaac2.exe). Wymaga ffmpeg + python3 (są w WSL).
723|Exit 0 = OK, 1 = błędy, 77 = brak zależności. Ostatni wynik: 27/27 PASS.
724|
725|---
726|
727|Zrodla z naniesionymi patchami leza w `src-fdk-aac/` i `src-fdkaac/`.
728|Wszystkie zmiany w libfdk są spięte przez jeden modul
729|`libAACenc/src/franken.{h,cpp}` (globalny blok `g_franken`, sentinele = domyślne
730|FDK), a nowe `AACENC_PARAM` (zakres `0xF0xx`) są wystawione w `aacenc_lib.h`.
731|
732|```bash
733|sudo apt-get install -y mingw-w64          # + autotools (autoconf/automake/libtool)
734|
735|# --- libfdk-aac (x64) ---
736|cd src-fdk-aac && autoreconf -i
737|./configure --host=x86_64-w64-mingw32 --prefix=$PWD/../inst-x64 \
738|    --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2
739|make -j && make install
740|
741|# --- frontend (x64) ---
742|cd ../src-fdkaac && autoreconf -i
743|PKG_CONFIG_PATH=$PWD/../inst-x64/lib/pkgconfig ./configure \
744|    --host=x86_64-w64-mingw32 CFLAGS="-O2 -I$PWD/../inst-x64/include" \
745|    LDFLAGS="-static -static-libgcc -L$PWD/../inst-x64/lib"
746|make -j     # -> fdkaac.exe
747|
748|# x86: to samo z --host=i686-w64-mingw32 i osobnym prefixem inst-x86.
749|```
750|
751|## Lista zmienionych plikow FDK (mapa na punkty)
752|- `libAACenc/src/franken.{h,cpp}` — nowy modul sterujący.
753|- `libAACenc/include/aacenc_lib.h` — nowe `AACENC_PARAM` 0xF0xx.
754|- `libAACenc/src/aacenc_lib.cpp` — dispatch SetParam + override useMS/IS/PNS/
755|  afterburner/cutoff + guard cutoffu pod SBR (po `sbrEncoder_Init`) + read-only
756|  GetParam mirrors dla verbose (useTns/Pns/IS/MS, efektywne SBR).
757|- `libAACenc/src/ms_stereo.cpp` — kontrola MS per-pasmo W PETLI decyzyjnej (maska
758|  + motylek L/R->M/S zsynchronizowane; naprawiony artefakt "lewy=center") + MS bias
759|  + zakres pasm MS (--msbands-lo/-hi) + precyzja MS (--ms-precision, prog ld64).
760|- `libAACenc/src/intensity.cpp` — kap pasm IS (spójny z maska) + bias progow IS
761|  (initIsParams: min_is_sfbs, corr_thresh, left_right_ratio) + --is-aggression.
762|- `libAACenc/src/psy_configuration.cpp` — read-back liczby SFB + zniesienie gate IS.
763|- `libAACenc/src/bandwidth.cpp` — --uncap-bandwidth (zdjęcie capa 20kHz).
764|- `libAACenc/src/pnsparam.cpp` — override PNS start + --force-pns (bypass bramki tabeli).
765|- `libAACenc/src/aacenc.cpp` — override maski TNS + quasi-CVBR + --unlock-bitrate
766|  (zniesienie dolnego floora bitrate w FDKaacEnc_LimitBitrate).
767|- `libSBRenc/src/sbr_encoder.cpp` — override gęstości SBR + --sbr-num-env (static
768|  framing) + --sbr-freqres-fixfix + --sbr-stereo-mode + --sbr-noise-floor-offset.
769|- `libSBRenc/src/invf_est.cpp` — --sbr-invf (wymuszony poziom inverse filtering).
770|- `libSBRenc/src/ps_encode.cpp` — override PS IID + --ps-icc/--ps-icc-mode.
771|- `libAACenc/src/aacenc_tns.cpp` — kap rzędu TNS.
772|- `libAACenc/src/pnsparam.cpp` — override startowej częstotliwości PNS.
773|- `libSBRenc/src/sbr_encoder.cpp` — override gęstości/dokładności SBR + zapis
774|  efektywnych wartości SBR do g_franken (dla verbose).
775|- `libSBRenc/src/ps_encode.cpp` — override PS (wymuszenie IID + tryb kwantyzacji).
776|- `libAACenc/src/main.c`, `aacenc.c`, `aacenc.h` (frontend) — switche CLI,
777|  parsowanie, przekazanie do SetParam, dump `--verbose`, help.
778|