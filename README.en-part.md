# Franken FDK AAC

Franken FDK AAC is the Fraunhofer FDK AAC encoder (libfdk-aac 2.0.3, mstorsjo/fdk-aac) with
the nu774/fdkaac 1.0.2 frontend, extended with switches that expose decisions FDK
normally makes on its own: how stereo is coded, how the high band is rebuilt, where bands
may be cut to zero, how transients are handled, how bits are distributed over time. It is
meant for engineers who want to hear and measure what those decisions do, especially at
the extremes: very low bitrates, forced wide bands, speech, streaming.

Author: Michał Dziwisz. Subject-matter consultant: Patryk Faliszewski.
Built on open source: libfdk-aac (Fraunhofer IIS) and the nu774/fdkaac frontend.

Without any of the new switches the encoder produces exactly what stock FDK produces,
byte for byte. Every switch is opt-in, and every one of them can also make things worse:
they let you go where FDK's own tuning deliberately does not.

## Download

Prebuilt Windows binaries are published as GitHub Releases and download without an
account:

**https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Beta versions (for example `v1.4.0-beta.1`) are published as pre-releases, so the link
above still shows the last stable version; betas are on the full list:
**https://github.com/michaldziwisz/franken-fdk-aac/releases**

`franken-fdk-aac-x64-vX.Y.Z.zip` is 64-bit, the `x86` one is 32-bit. Each zip contains the
static encoder (no DLLs) and the full documentation. The repository itself holds sources
only; building is described at the end of this document.

## First steps

All original frontend options work as before (`-p`, `-b`, `-m`, `-w`, `-a`, `-s`, `-f`,
tagging); `fdkaac-franken-x64.exe --help` lists them together with the new ones.

1. Encode once with `--verbose` and read what the encoder actually set up:

       fdkaac-franken-x64.exe -p 2 -b 128000 --verbose -o out.m4a in.wav

2. Change one switch at a time and check in `--verbose`, under
   `franken overrides applied`, that it took effect.
3. Listen, and where it matters, measure: decode both versions and compare. The tools
   in `tools/` in the repository do this for holes, pre-echo and stereo error.

## Recipes

Each of these is a tested starting point, not a preset to apply blindly.

    # LC stereo at a low bitrate with a wide band: fewer holes, less pre-echo
    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

    # The same at 40 kbps, with a calmer attack detector for percussive material
    fdkaac-franken-x64.exe -p 2 -b 40000 -w 18000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 --attack-decay 0.9 -o out.m4a in.wav

    # More stereo width at the expense of the centre
    fdkaac-franken-x64.exe -p 2 -b 96000 --side-bias 6 -o out.m4a in.wav

    # Quasi-VBR around 128 kbps: CBR engine with room to breathe
    fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

    # HE-AAC with a softer, less metallic high band
    fdkaac-franken-x64.exe -p 5 -b 32000 --sbr-softness 4 -o out.m4a in.wav

    # HE-AAC v2 with a wider core than FDK picks (9 kHz -> 10.5 kHz)
    fdkaac-franken-x64.exe -p 29 -b 48000 --sbr-start 14 -o out.m4a in.wav

    # Stream that a listener tuning in decodes with SBR at once
    fdkaac-franken-x64.exe -p 5 -b 48000 --sbr-header-period 1 -f 2 -o out.aac in.wav

    # AAC-LD with a wider core and more TNS against pre-echo (LD needs LOAS, not ADTS)
    fdkaac-franken-x64.exe -p 23 -b 64000 --core-cutoff 12000 --tns-thresh 0.7 -f 10 -o out.loas in.wav

    # Extremely low HE-AAC v2: 8000 bps stereo
    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

    # Full 42 kHz core from a 96 kHz source
    fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

The last one reports `core bandwidth: 42000 Hz` in `--verbose`: the requested 40000 Hz is
snapped up to the next scalefactor band edge.

# Switch reference

This is the reference. Every switch gets its own card, and every
card is built the same way, so you can read only the card you need.

## How a card reads

The heading is the switch as you type it, with its argument. Under it:

- A single bold sentence: what the switch is for, in terms of sound or behaviour.
- **Applies to**: the profiles and situations where the switch has an effect. If a
  switch does nothing in a profile, it says so here.
- **Values**: the legal range, the unit, and the default. The default is always the
  value that means "stock FDK decides".
- **Input**: whether the switch takes a whole number, a decimal, a flag or text.
- Then the explanation: the problem first, the mechanism in plain words, then what
  happens as you move the value up and down.
- **Examples**: for a numeric switch with room to move, a mild, a strong and a
  deliberately exaggerated setting in each direction. Enumerated switches get a list
  of every value instead.
- **Works with / against**: interactions, and which other switch masks this one.
- **Measured**: only real measurements, with the material and conditions. The line is
  missing when there is no measurement.
- **Under the hood**: one or two lines for the curious. Skip it freely.

## Shared conventions

These hold for every card and are not repeated in each one.

**Off = stock.** Every Frankenstein switch is off unless you give it. With none of
them on the command line, the output is byte identical to stock FDK. "Off" is spelled
`-1` for almost all of them (the sentinel meaning "leave FDK's own decision"), and `0`
for `--core-cutoff`, `--side-bias`, `--side-knee` and `--mask-slope`, where `-1` would
be a legal working value.

**Decimal input.** Decimal switches accept `.` or `,`, up to three decimal places.
The value is rounded half away from zero, and the range is then checked on the rounded
value. Anything else is an error: `1e1`, `nan`, `3x`, embedded spaces. Whole number
switches reject fractions, so `128,5` where an integer is expected is an error, not a
silent truncation.

**Units.** Four conventions appear throughout:

- `dB`: a plain decibel offset, positive or negative.
- `x100`: an integer that is a hundred times the real value, so `100` means 1.00,
  `90` means 0.90, `400` means 4.00.
- `Q8`: a fixed point integer where `256` means 1.0. So `128` is 0.5, `384` is 1.5,
  `512` is 2.0, `4096` is 16.0. Q8 is FDK's own internal scale, which is why it leaks
  into the command line.
- `SFB`: a scalefactor band number, counted from `0` at the bottom of the spectrum
  upwards. A scalefactor band is a group of spectral lines that share one scalefactor,
  roughly following critical bandwidth: narrow at the bottom, wide at the top. How
  many bands exist depends on profile, sample rate and the coded bandwidth, so an SFB
  number is only meaningful once you know that count. `--verbose` prints it, for
  example `active SFBs (long): 49` at AAC-LC 48 kHz stereo.

**Checking what took effect.** `--verbose` prints the effective, initialised encoder
parameters before encoding starts, plus a line `franken overrides applied` listing the
switches that actually deviate from stock in this run. If a value you set does not
show up there, it did not take effect, and no amount of listening will tell you that
faster.

## How `-b` is read

Three conventions live side by side, and mixing them up is the most common way to get
a surprising file size.

- **Normal operation**: `-b` is bits per second, but the frontend accepts the short
  form too. Small numbers are read as kilobits, so `-b 128` and `-b 128000` both mean
  128 kbps, and `-b 1152` means 1152 kbps.
- **With `--unlock-bitrate`**: `-b` is read literally as bits per second, with no
  kilobit shortcut. `-b 6000` is 6000 bps. `-b 1152` in this mode is 1152 bps, which
  is practically nothing; the program warns when a value looks like that mistake.
- **With `--dab`**: `-b` is kilobits per second, in the range 8..192 and a multiple of
  8. The encoder multiplies by 1000 itself, so `-b 96` is 96 kbps, also together with
  `--unlock-bitrate`.

The upper ceiling is not a switch and cannot be lifted: an AAC frame holds at most
6144 bits per channel. At 48 kHz that is roughly 576 kbps per channel.

## Transport note for LD and ELD

AAC-LD (`-p 23`) and AAC-ELD (`-p 39`) work in this encoder, but they cannot be
written into ADTS. Asking for `-f 2` with those profiles ends in "encoder
initialization failed", and the failure is the transport format, not the profile. Use
M4A (`-f 0`, the default), LATM (`-f 6` or `-f 7`) or LOAS (`-f 10`). LOAS is the
practical choice when you want a bare stream to measure or to feed somewhere, because
it is a self contained byte stream the way ADTS is for LC and HE-AAC.

## Map of the groups

The switches are grouped in `--help` roughly from easy to geeky, and this reference
follows the same order.

- **A. Start here**: the consumer knobs. Diagnostics, input sample rate conversion,
  speech tuning, intensity stereo in one slider, and the two opt-in flags that remove
  built-in limits.
- **B. Stereo coding**: mid/side, intensity stereo in detail, parametric stereo.
- **C. Bandwidth and SBR**: where the core ends and SBR begins, then the high band's
  grid, detectors and estimators; `--core-cutoff` closes the group.
- **D. Masking, noise shaping and detail**: the psychoacoustic thresholds, the SNR
  the encoder demands per band, TNS and PNS.
- **E. Core transients, pre-echo and spectral holes**: the LC, LD and ELD core under
  bit pressure.
- **F. Block switching and bitrate control**: the long/short decision, the bit
  reservoir and the quasi-VBR knobs.
- **G. DAB+ output**: digital radio superframes.

## A. Start here

Six switches, in the order most people need them: `--verbose` shows what the encoder
really did, `--resample` picks the sample rate the whole encoder runs at, `--speech` and
`--is-aggression` tune it, and `--unlock-bitrate` and `--uncap-bandwidth` remove limits
that stock FDK enforces. If you are new to this build, read `--verbose` first and use it
after every other switch you try. `--bit-stats` (group E) is its companion when the
question is where the bits went.

### `--verbose`

**Prints the encoder's real, initialised settings to stderr before encoding, so you
can see what actually took effect.**

- Applies to: every profile and every transport. Changes nothing in the output stream.
- Values: flag, no argument; default: off
- Input: flag

The encoder makes a large number of decisions from your command line, and most of them
are not the values you typed. Bandwidth gets snapped to a scalefactor band boundary.
Bitrate selects a row in a tuning table that in turn sets PNS, intensity stereo and the
SBR grid. A switch you spelled correctly may still be ignored because the profile has
no such mechanism. `--verbose` is the only way to see the result of all that before you
spend time listening.

What it prints: the profile (AOT), bitrate and bitrate mode, sample rate, channel mode,
the effective core bandwidth in Hz together with its source (`from -w`, `from
--core-cutoff`, or `auto`), afterburner state, transport and signaling, and the state
of the coding tools the encoder picked for itself, that is TNS, PNS, intensity stereo
and MS stereo on or off. When SBR is active it adds the SBR ratio, the effective start
and stop frequency indices, the frequency scale, noise bands, amplitude resolution and,
for the whole band, a `final BW (AAC+SBR)` estimate. It prints the reservoir budget
block, which tells you the average bits per frame and the ceilings, and it prints
`active SFBs (long)`, which is the number you need before setting any SFB based switch.
At the end comes the list `franken overrides applied`, naming only the switches that
really deviate from stock in this run.

Three readings repay attention. The core bandwidth is SFB anchored, so a typed
`-w 17300` can come back as `17915 Hz`: the encoder cannot cut inside a band. Under SBR
the `-w` value is ignored but still labelled `[from -w]`; there the core follows
`--sbr-start`. And `active SFBs (long)`
is the ceiling for `--msbands`, `--isbands` and every `<sfb>` argument: asking for band
48 when only 40 are coded is not an error, it simply does nothing.

Values:
- `--verbose`: print the dump. There is no off switch, since omitting it is off.

Example in context:

    fdkaac -p 5 -b 64000 --verbose -o out.m4a in.wav

Works with / against: nothing. It is the diagnostic for everything else in this
reference, and the fastest answer to "did my switch do anything at all".

Measured: at AAC-LC 48 kHz stereo it reports `active SFBs (long): 49`. At HE-AAC v2
48 kbps, 48 kHz input, the core comes out at 9000 Hz with SBR start index 13.

### `--resample <rate>`

**Converts the input to another sample rate before anything else sees it, so the
whole encoder runs at the new rate.**

- Applies to: every profile and transport, `--dab` included. Works on files, pipes
  (`-` as input) and `--raw` input the same way.
- Values: an AAC sample rate, in Hz (`44100`) or in kHz (`44`, `44.1`, `44,1`,
  `22`, `22.05`, `32`, `48`, `96` ...). 8000..96000. `-1` = off; default: off.
- Input: rate

The converter sits at the very start of the PCM chain, right after the WAV/CAF/raw
reader and before the limiter, the edge extrapolator and the encoder. Nothing
downstream knows the original rate: the encoder picks its tables, bandwidth, SBR
layout and bit budget for the new rate, the M4A timescale and the gapless length are
counted in the new rate, and `--verbose` prints the new `samplerate`. A source already
at the requested rate goes through the untouched chain, byte for byte the stream you
get without the switch.

How it converts: an exact rational ratio (48000 to 44100 is 147/160, so there is no
drift over hours of audio), a windowed sinc (Kaiser) kernel computed at start, float
arithmetic. The passband is flat up to 95 percent of the lower Nyquist, the stopband
starts at that Nyquist with 120 dB attenuation, so nothing folds back into the audible
band. The kernel is symmetric and the output instants are aligned with the input, so
the conversion adds no delay: the encoder delay and the gapless priming stay what they
are at the new rate. The kernel needs a few milliseconds of look-ahead inside the
converter, which is invisible from the outside. No external library, no DLL.

Why you would use it: below about 48 kbps stereo with a narrow `-w`, a 48 kHz core
spends bits on scalefactor bands and a frame grid made for 20 kHz audio. A lower rate
gives finer bands below the cutoff and a longer frame. Long frames are also the price:
at 24 kHz one long block is 85 ms, so pre-echo before sharp attacks grows. A higher
rate is useful too: `--dab` accepts only 32 and 48 kHz, so a 44.1 kHz source needs
`--resample 48`.

Values:
- omitted or `-1`: no conversion (stock).
- `--resample 32`: 48 or 44.1 kHz source coded at 32 kHz. With `-w 9500` and 32 kbps
  stereo, the usual sweet spot (see Measured).
- `--resample 24`: more of the same, with longer blocks; worth it only for material
  without sharp attacks.
- `--resample 48`: 44.1 kHz source for `--dab`, or for HE-AAC at 48 kHz.

Example in context:

    fdkaac -p 2 -b 32000 -w 9500 --resample 32 -o out.m4a in48k.wav
    ffmpeg -i in.flac -f wav - | fdkaac --dab -b 64 --resample 48 -o out.dabp -

Works with / against: `-w` and `--core-cutoff` are in Hz, so they mean the same
frequency at any rate, but they cannot exceed the new Nyquist. `--raw-rate` gives the
rate of raw input; `--resample` then converts from it. Band-number switches
(`--msbands`, `--is-lo` ...) count bands of the new rate's table, check them with
`--verbose`.

Measured: the converter alone (harness `tools/resample/rs_test.c`,
`tools/resample/rs_quality.py`) for 48 to 32, 48 to 24, 44.1 to 32, 48 to 44.1, 44.1
to 22.05, 32 to 48, 44.1 to 48 and 48 to 16 kHz: passband deviation below 0.0001 dB up
to 95 percent of Nyquist, aliases of tones between the new and the old Nyquist at
-120 to -132 dB, THD+N of a 1 kHz tone -135 to -141 dB, phase shift 0, output length
exactly `ceil(n * new / old)`, the same samples whatever the read size. Speed: 37 to
106 times real time for stereo on one core (the 48 to 32 kHz case: an encode of 30 s
took 0.94 s against 0.62 s without the switch). After encoding (AAC-LC 32 kbps stereo,
`-w 9500`, error measured 200..9000 Hz against the 48 kHz original): the same within
0.03 dB as converting first with SoX at its highest precision. Against coding at 48 kHz,
noise-to-signal on four music samples 48 / 32 / 24 kHz: -7.3 / -8.2 / -9.0 dB,
-11.1 / -14.8 / -16.8 dB, -7.6 / -8.4 / -9.1 dB, -8.3 / -7.7 / -8.6 dB; spectral holes
5.6 / 0.1 / 0 percent on the first sample. Pre-echo on a test signal with attacks:
-33.9 dB at 48 kHz, -28.4 dB at 32 kHz, -28.1 dB at 24 kHz.

### `--speech`

**Switches the SBR layer to its speech tuning: different inverse filtering, a different
noise floor, and no parametric coding of the high band.**

- Applies to: HE-AAC and HE-AAC v2, that is wherever SBR is running, including ELD with
  SBR. No effect on AAC-LC, which has no separate speech mode anywhere in FDK.
- Values: flag; default: off (music tuning)
- Input: flag

SBR rebuilds the high band from the coded low band, and it needs to know how tonal the
source is to decide how much to whiten the patched band and how much noise to fill in.
Music and speech have very different answers. Speech at low bitrate suffers a specific
artifact when the music tuning is used: the reconstructed top gurgles, because the
estimator keeps deciding the patch is tonal when it is really a moving formant plus
breath.

The flag changes several things at once: the inverse filtering decision table switches
to the speech table, the noise floor weighting goes to the speech value, and parametric
coding of the high band is disabled. For a podcast, an audiobook or dialogue at 24..48
kbps the result is a more natural top with less gurgling.

If you want only part of what the flag does, the pieces are available separately:
`--sbr-invf-table 1` gives just the speech decision table, `--sbr-nf-weight 1.0` just
the speech noise weight, and `--sbr-softness 4` both of them, in each case without
touching the crossover and without disabling parametric coding.

For speech in plain AAC-LC there is no equivalent, and this reference will not pretend
otherwise. The LC psychoacoustic model is the same for speech and music. What helps
there instead is narrowing the band, since speech has little content high up, with
`--core-cutoff 12000` to `14000`, and at very low bitrate turning on noise substitution
with `--pns 1 --force-pns`.

Values:
- omitted: music tuning (stock).
- `--speech`: speech tuning.

Example in context:

    fdkaac -p 5 -b 32000 --speech -o speech.m4a podcast.wav

Works with / against: the explicit SBR estimator switches in group C win over it, so
`--sbr-invf` forces a fixed inverse filtering level regardless. `--sbr-invf-table` and
`--sbr-nf-weight` follow `--speech` when left at `-1`, which is how a single flag can
move both.

Measured: no effect at all on AAC-LC; changes the HE-AAC stream.

### `--is-aggression <0..100>`

**One slider for how eagerly the encoder collapses upper bands to intensity stereo,
trading stereo separation at the top for bits.**

- Applies to: profiles with a real stereo core, so AAC-LC, AAC-LD and AAC-ELD, and the
  core of HE-AAC. No effect on mono. Under HE-AAC v2 the stereo image is carried by
  parametric stereo on a mono core, so intensity stereo has nothing to act on.
- Values: `0..100`, where `0` is stock FDK; default: `-1`, off (identical to stock)
- Input: whole number

Intensity stereo (IS) replaces separate left and right spectra in the upper bands with
one shared energy envelope plus a direction. The ear localises high tones poorly, so
this is cheap: you drop half the data in those bands and pay in stereo separation at
the top. On material with genuinely different highs in the two channels, for instance
cymbals placed hard to one side or wide spatial effects, you pay audibly.

Stock FDK is extremely shy about IS, which is why people conclude the switch does
nothing. There are four gates, and a band needs to pass all of them at once: an
admission gate that only considers IS when bitrate per band is below a threshold, a
correlation threshold of about 0.95 between the channels, a minimum run of 6
consecutive bands, and a stable direction. Loosening one gate usually changes nothing,
because another still blocks. `--is-aggression` moves all of them together, coherently:
it drops the bitrate and bandwidth gate and loosens the correlation, direction and
region requirements as the number rises.

Higher means more bands coded as IS, fewer bits spent on the top of the spectrum, and
weaker stereo separation up there. `100` is the real maximum: gate removed, correlation
loosened to roughly 0.475, a region of a single band is enough, and the direction
tolerance is wide. Low values are not useful as a fine trim, because a small loosening
often still leaves another gate binding; `1` changed nothing on our test material. The
useful working window starts around 30.

There is only one direction to travel, since `0` is already stock and stock is close to
"never".

Examples:
- Mild, more IS: `--is-aggression 30`. Bands start going to IS on correlated material.
  Image at the top narrows slightly, a few percent of bits freed for the rest.
- Strong, more IS: `--is-aggression 50`. Clearly more bands in IS. Audible narrowing on
  cymbals and reverb tails, a real bit saving at 48..96 kbps.
- Exaggerated, more IS: `--is-aggression 100`. Almost anything that can go to IS does.
  The top of the image glues to the centre and panned highs drift. Useful at 32 kbps
  and below, and as a caricature to hear what IS sounds like when you let it run.

Works with / against: the explicit `--is-*` switches in group B override whatever the
slider set, so use the slider first and only reach for `--is-corr-thresh`,
`--is-min-sfbs` and `--is-lr-ratio` if you need precision. `--is 0` disables intensity
stereo outright and makes this switch irrelevant. `--is-force-lo` and `--is-force-hi`
bypass every gate, so they do not need this slider at all. `--verbose` prints the
effective correlation threshold and minimum band count, which is how you confirm the
slider landed.

Measured: on mix48 at AAC-LC 64 kbps, `1` produced no stream change, while `30`, `50`
and `100` each changed the stream.

### `--unlock-bitrate`

**Removes the lower bitrate floors that every profile enforces, and makes `-b` literal
bits per second.**

- Applies to: every profile. The floors removed differ by profile, see below.
- Values: flag; default: off (all floors enforced)
- Input: flag

Stock FDK refuses to encode below a floor, and the floor is different in each profile
and is often mistaken for a format limit. It is not. The common claim that HE-AAC
cannot go below 16 kbps describes the lowest row of the SBR tuning table for 48 kHz
stereo, nothing more. Ask for less and stock FDK quietly raises your bitrate to that
row.

The flag removes five separate floors:

- the 40 bit per channel core minimum;
- the LD and ELD rule of 8 kbps per channel, and the 208 bit transport reserve those
  profiles guess at (the real header overhead is still checked against the real
  transport encoder);
- the start of the LD and ELD bandwidth table, so those profiles can use a band
  narrower than the table's first row;
- the SBR and PS tuning table floor: HE-AAC, HE-AAC v2 and ELD with SBR below the table
  reuse its lowest row, keeping the same SBR grid while your bitrate is honoured and
  the AAC core simply gets fewer bits;
- the rule that a frame unable to fit its own side information aborts the encode. Such
  frames overshoot instead, and the reservoir absorbs it over the following frames.

What remains is the transport header, 56 bits per frame for ADTS, and the upper ceiling
of 6144 bits per channel, which no switch touches. Below the SBR payload estimate,
about 2.5 kbps per channel plus parametric stereo, the core would have no bits at all
and the encoder stops with a clean "encoder initialization failed" rather than an
assertion.

The flag removes floors. It does not guarantee any particular bitrate. At these values
dense passages routinely come out above what you asked for, because headers and side
information alone cost more than the budget, so judge the result by the file size and
not by the number in `--verbose`.

Mind the `-b` change. In unlock mode the number is bits per second, literally. `-b
6000` is 6000 bps. `-b 6` is 6 bps and will not encode anything useful, and `-b 1152`
typed in the belief that it means 1152 kbps gives you 1152 bps. Always write the full
number of bits here.

Values:
- omitted: all floors enforced, `-b` read with the kilobit shortcut (stock).
- `--unlock-bitrate`: floors removed, `-b` literal bps.

Example in context:

    fdkaac -p 29 -b 6000 --unlock-bitrate -f 2 -o out.aac in48k.wav

Works with / against: `--vbr-reservoir` and `--peak-bitrate` become more interesting
here, since overshoot is the normal case in this mode and the reservoir is what pays
for it.
With `--dab`, the DAB+ kilobit convention for `-b` is replaced by literal bps. Nothing
in group D or F restores the floors.

Measured: real bitrate computed from the size of the ADTS or LOAS stream, with no
container overhead:

| Mode | `-b` | real bps | FDK decoder | ffmpeg 6.1 |
|---|---|---|---|---|
| HE-AAC 48 kHz stereo | 8000 / 6000 | 9232 / 7290 | 0 errors | 0 errors |
| HE-AAC 48 kHz mono | 4000 / 3000 | 4736 / 4348 | 0 | 0 |
| HE-AAC v2 48 kHz | 6000 / 4000 | 6678 / 4894 | 0 | 1 (see below) |
| ELD+SBR 48 kHz stereo | 12000 | 12984 | 0 | see below |
| ELD / LD 32 kHz stereo | 8000 | 8154 | 0 | see below |
| AAC-LC 48 kHz stereo | 4000 / 3000 | 6012 / 7054 | 0 | 0 |

Where the real bitrate exceeds `-b`, those are dense frames overshooting on headers and
side information alone. The ffmpeg 6.1 column needs reading with care: it reports the
same errors on the stock encoder in these modes, one `noise_facs_q is invalid` on
HE-AAC v2 at 12..14 kbps, which is inside the stock tables, and many errors on every
ELD or LD LOAS stream at any bitrate. ffmpeg does not implement LD and ELD fully, so
judge those streams with the Fraunhofer decoder.

### `--uncap-bandwidth`

**Removes FDK's hard 20 kHz limit on core bandwidth so `--core-cutoff` can reach up to
Nyquist.**

- Applies to: any profile with a core you can widen, in practice AAC-LC at high bitrate
  from a high sample rate source. Meaningless without `-w` or `--core-cutoff`.
- Values: flag; default: off (the 20 kHz cap applies)
- Input: flag

FDK clamps core bandwidth to `min(20000, sr/2)`. The reasoning is sound for normal
material: almost nobody hears above 20 kHz, and the bits are better spent lower. The
consequence is that a 96 kHz source encoded at a generous bitrate still codes nothing
above 20 kHz, no matter what you ask for, and the extra sample rate buys you only a
gentler antialiasing slope.

This flag lifts the clamp. On its own it does nothing at all: it only raises the
ceiling, it does not move the bandwidth. The switches that move the bandwidth are
`-w` and `--core-cutoff <hz>` (identical without SBR), and with the cap lifted they can
go up to half the sample rate.

Be clear about what you are buying. Content above 20 kHz costs bits that come out of
the audible band unless your bitrate has real headroom, and streams with bandwidth
beyond the usual range sit outside typical practice, so some decoders may balk. This is
a deliberate opt-in, which is why it is a separate flag rather than a side effect of
`-w`.

Values:
- omitted: core bandwidth clamped at 20 kHz (stock).
- `--uncap-bandwidth`: clamp lifted, `-w` and `--core-cutoff` free up to `sr/2`.

Example in context:

    fdkaac -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 200 \
        -o out.m4a in96k.wav

Works with / against: requires `-w` or `--core-cutoff`; without SBR the two give the
same stream. Under SBR the 20 kHz cap does not apply, because the core stops at the SBR
start, far lower. Pairs naturally with `--ath-scale` below 256 when you have bitrate
to spare, since there is no point coding 40 kHz of band at thresholds tuned for
scarcity.

Measured: 96 kHz input, AAC-LC 400 kbps, broadband noise. Without the flag,
`--core-cutoff 40000` gives `20000 Hz` in `--verbose` and essentially zero energy above
20 kHz. With the flag, `--verbose` reports `40000 Hz` and the decoded file carries about
10 percent of its energy in 20..24 kHz, about 20 percent in 24..32 kHz and about 20
percent in 32..44 kHz. This encoder has no 96 kHz test signal checked in, so treat that
as a prior measurement rather than one you can reproduce from the shipped material.

## B. Stereo coding

Three different machines in this encoder turn two channels into fewer bits, and they
work on different parts of the signal, with different costs. Before reaching for any
switch in this group, get clear on which of the three you are actually talking about.

**MS (mid/side)** is a lossless rotation of the channel pair. Instead of left and right,
the encoder stores mid (L+R) and side (L-R) for a given scalefactor band. Nothing is
thrown away: the decoder rotates back and gets L/R exactly. On correlated music the side
signal is small, so it quantises to far fewer bits than a second full channel would. MS
is a decision about bit economy, not about sound, and it is taken per band and per frame.
Available in the AAC core, which means LC, LD, ELD, and the core under HE-AAC when that
core is still stereo.

**IS (intensity stereo)** is lossy and directional. Over a range of upper bands the
encoder stops sending two spectra and sends one energy envelope plus a direction, so the
decoder paints both channels from the same spectrum with a pan coefficient. That is cheap
and the ear tolerates it up high, where it localises fine spectral detail poorly. It also
genuinely removes information: anything that differed between left and right in those
bands, cymbals placed hard to one side, spatial effects, out-of-phase content, collapses
toward a single steered source. IS lives in the core too, and the encoder is cautious
with it by default.

**PS (parametric stereo)** is the HE-AAC v2 machine. The core becomes mono: one AAC
spectrum, one SBR high band, and the stereo image rides along as a handful of parameters
per frame (level differences, coherence, optionally phase). Below roughly 32 kbps that
buys you a usable stereo picture where real two-channel coding would be starving. PS is
not a refinement of MS or IS, it replaces them: with `-p 29` there is no side channel to
bias and no intensity bands to place, so every `--ms*` and `--is*` switch in this group
is inert. Conversely `--ps*` does nothing outside HE-AAC v2.

In practice: at 96 kbps and up MS is doing almost all the work and you tune the mid/side
bit split (`--side-bias`, `--side-knee`). Between about 40 and 80 kbps intensity stereo
starts to earn its keep in the top bands (`--is-aggression`, then the `--is*` placement
switches). Below about 32 kbps stereo, HE-AAC v2 and the PS parameters are the sane
route, and the interesting knobs there are the resolution ones (`--ps-env-reduce`,
`--ps-bands`).

**Reading SFB numbers.** Every band switch here counts scalefactor bands from 0 at the
BOTTOM of the spectrum. Band 0 is bass, the highest number is the top of the coded range.
How many bands exist depends on profile and sample rate: at LC 48 kHz stereo `--verbose`
prints `active SFBs (long): 49`, so legal band numbers run 0..48. Always read that line
before setting a range, because the count changes with the configuration and a number you
remembered from another encode may be out of the spectrum entirely.

**Short blocks.** The numbers you give are always LONG-block bands. A short frame has its
own, much coarser table (around 14 bands at 44.1 and 48 kHz, counted from 0 inside each
window group), so the same number would mean another frequency there. The encoder
therefore moves every range to the short table by frequency: each edge goes to the
nearest short band edge in Hz, an exact tie goes up. A short band is in the range when
at least half of it is. A narrow range that falls inside one wide short band takes that
band. The result is the same region of the spectrum in long and short frames, for every
sample rate, for DAB+ (960/120) and for every `--short-groups` setting. `--verbose` prints
both, for example `long 18..30 (2531..9000 Hz) -> short 3..7 (2250..8250 Hz)`. Up to
1.4.0-beta.1 the numbers were compared directly: `--msbands-lo 18` left short frames with
no MS at all, and `--msbands 18` (MS up to 2.5 kHz) gave short frames MS up to 24 kHz.

**Why a range above the coded bandwidth does nothing.** The SFB count is the full table
for the profile, but only bands up to the coded bandwidth carry anything. Bands above the
cutoff are zero, and a stereo decision about a band of zeros changes no bits at all. This
is a real trap with the high-band examples: at the default LC bandwidth of about 15.75 kHz,
`--msbands-lo 44 --msbands-hi 48` produces exactly the same stream as `--msmask 0`,
because bands 44..48 sit above the coded band and MS is therefore switched off everywhere
that matters. Force the band open with `-w 20000` and the two stop being equal. So check
your range against the coded bandwidth, not just against the SFB count.

All switches in this group are off unless you give them. Off means stock: the sentinel
`-1`, and `0` for `--side-bias` and `--side-knee`, leaves FDK's own decision alone.
`--side-bias` and `--side-knee` take decimals, with `.` or `,` and up to 3 decimal places;
`--side-cutoff` takes Hz; the rest are whole numbers and reject fractions.

### MS

The MS rotation itself costs nothing in sound, because it is lossless. What decides how a
stereo image comes through the encoder is the split of bits between the mid and the side
channel once a band is coded in MS, and above all the decision which side bands are coded
at all. Every side band cut to zero is a piece of the image moved to the centre.

The switches in this section fall into three tiers:

- **The two levers**: `--side-bias` moves the line at which side bands are kept or
  dropped, `--side-knee` grades the bands near that line by their own level. Width against
  centre is set with these two, alone or together.
- **A trade for low bitrates**: `--side-cutoff` gives up the side above a frequency and
  hands its bits to the mid.
- **Laboratory**: `--msmask`, `--msbands`, `--msbands-lo`, `--msbands-hi`, `--ms-bias`,
  `--ms-precision`, `--mid-bias`. They reach into the MS decision and the MS thresholds
  directly. They are there to reproduce a technical condition or to find out what the
  encoder does; measured on music, none of them beats the two levers, and several make
  the result worse than stock.

How the measurements in this section were taken: AAC-LC stereo, Fraunhofer decoder, the
decoded file aligned to the source and compared in the mid/side domain from 100 Hz to the
coded bandwidth. *Mid error* and *side error* are the energy of the coding error relative
to the energy of that channel in the source, in dB, so lower is cleaner. *Side bands cut*
is the number of side bands per frame coded as zero, from `--bit-stats`. The main material
is acoustic stereo music at 44.1 kHz; `dense48` is the synthetic dense signal of group E.

#### `--side-bias <dB>`

**Moves the line at which side bands are kept or dropped: the main lever for stereo width
against centre quality.**

- Applies to: stereo AAC core (LC, LD, ELD and the stereo core under HE-AAC), bands coded
  in MS. No effect in HE-AAC v2, nothing to do under `--msmask 0`.
- Values: `-24.0..50.0` dB; `0` = off (default, stock threshold)
- Input: decimal, up to 3 places, `.` or `,`

For every band in MS the encoder compares the energy of the side signal with its masking
threshold. Below the threshold the side band is cut to zero; above it the band is coded,
and the further above, the more finely. The side signal is everything that differs between
left and right, so this one comparison is where the width of the image is decided.
`--side-bias` shifts the side threshold of every MS band by the given number of dB.

A positive value lowers the threshold. Fewer side bands are cut, the remaining ones are
quantised more finely, and what you hear is a steadier image: reverb tails that decay
instead of stopping, room sound that stays where it was. The bit budget does not grow, so
the bits come from the mid, and at low bitrate the centre audibly pays for them. Between
+3 and +9 the trade is usually worth it. Beyond about +12 the side stops improving while
the mid keeps losing.

A negative value raises the threshold: side bands fall out, the image narrows toward mono,
and the freed bits go to the mid. Small negative values act smoothly. At about -0.97 dB
there is a step. There the raised threshold crosses the minimum SNR that FDK's hole
avoidance keeps under every band, and the side bands FDK was holding only through that
rescue fall out together. How big the step is depends on how short of bits the frames are.
At a comfortable bitrate it is moderate and the narrowing goes on gradually below it. In
frames that are already starved most side bands exist only thanks to the rescue, so the
step takes nearly the whole side, and -1, -4 and -12 give the same stream. For gentle
narrowing stay between 0 and -0.95, or use a negative `--side-knee`, which has no step.

Examples (acoustic stereo music, 96 kbps; stock: mid -20.3 dB, side -11.4 dB, 7.0 side
bands cut per frame):
- Mild, more to the side: `--side-bias 3`. Side 0.8 dB cleaner, mid 0.4 dB worse, 3.4
  side bands cut.
- Strong, more to the side: `--side-bias 6`. Side 1.3 dB cleaner, mid 1.3 dB worse, 1.2
  side bands cut. The usual working value.
- Exaggerated, more to the side: `--side-bias 24`. The side is worse again than at +12 and
  the mid has lost 3.3 dB: bits poured into a channel that can no longer use them.
- Mild, narrower: `--side-bias -0.5`. A few marginal side bands given up; mid and side
  within 0.15 dB of stock.
- Strong, narrower: `--side-bias -3`. Past the step: 13.8 side bands cut, side 0.9 dB
  worse, mid 0.2 dB cleaner.
- Exaggerated, narrower: `--side-bias -12`. 26 of about 37 side bands cut, the side
  error 5.7 dB higher. On `dense48` at 40 kbps already `-6` leaves the side 6 bits per
  frame, practically mono.

Works with / against: `--side-knee` acts on the same decision and combines cleanly with
this one: the bias places the line, the knee grades the bands near it. `--side-cutoff`
removes the side above its frequency whatever the bias. `--msmask 0` removes MS and makes
this switch inert; `--msbands`, `--msbands-lo` and `--msbands-hi` limit the bands it can
reach.

Measured (acoustic stereo music; mid error / side error in dB / side bands cut per
frame):

| `--side-bias` | 48 kbps | 96 kbps |
|---|---|---|
| stock | -21.6 / -8.5 / 16.0 | -20.3 / -11.4 / 7.0 |
| `+3` | -19.1 / -7.9 / 11.6 | -19.8 / -12.3 / 3.4 |
| `+6` | -16.5 / -11.3 / 3.5 | -19.0 / -12.7 / 1.2 |
| `+9` | -15.4 / -12.3 / 1.8 | -18.6 / -13.1 / 0.6 |
| `-1` | -22.9 / -8.6 / 18.3 | -20.4 / -11.2 / 10.0 |
| `-3` | -24.2 / -8.0 / 20.7 | -20.5 / -10.5 / 13.8 |

At 48 kbps `+3` already codes more side bands, but so coarsely that the side error is
still 0.6 dB worse than stock; the gain arrives at `+6`. At 128 kbps `+6` takes the side from -14.2 to
-16.9 dB and the mid from -28.5 to -24.8 dB. The step: at 96 kbps 7.7 side bands are cut
at -0.96 and 9.9 at -0.97. On `dense48` at 40 kbps with `-w 18000` the side comes out
1.4 dB quieter than in the source at -0.95 and 10.8 dB quieter at -0.97, the same at -1,
-4 and -12.

#### `--side-knee <dB>`

**Grades the side bands near the kept/dropped line by their own level: finer or coarser
side detail, without a step.**

- Applies to: the same as `--side-bias`.
- Values: `-24.0..50.0` dB; `0` = off (default, stock behaviour)
- Input: decimal, up to 3 places, `.` or `,`

Where `--side-bias` moves the line, this switch works on the margin of each side band: how
far its energy lies above its threshold, which decides how finely the band is coded and
whether it survives. The value is added to that margin near the line and fades out above
it, so the loud, well coded part of the side is left alone, and every band keeps a
precision that follows its real level.

It exists because of one detail of FDK. When a frame runs out of bits, FDK does not drop a
weak band at once: it pins the band's threshold about 0.97 dB under its energy and codes
it at the coarsest step. On music around a tenth of the side bands sit on that one value,
whatever their real level. The knee reads the margin the rate control wanted before that
rescue, so those bands are graded again by their real level instead of moving as one
block. That is why a negative knee narrows smoothly where a negative bias jumps.

A positive value adds precision: bands up to N dB under their threshold are kept, graded
by how far under they were, and bands just above it get up to N dB finer. A negative value
takes precision away: the weakest bands near the line go first and their bits go to the
mid. In frames starved of bits most side bands exist only because of the rescue, so there
a small negative knee already removes most of them; at a comfortable bitrate it narrows
the image gradually all the way to -24.

Against `--side-bias` at the same value, a positive knee buys about the same side
improvement for less mid. At 96 kbps `+6` gives side -12.9 dB and mid -19.4 dB, the bias
-12.7 and -19.0; at 128 kbps the knee gives -16.8 and -25.7, the bias -16.9 and -24.8.
The knee suits material where the centre matters most, the bias material where the width
does, and the two together place the line and soften its edge.

Examples (acoustic stereo music, 96 kbps; stock as above):
- Mild, finer: `--side-knee 2`. Side bands cut from 7.0 to 3.4, side 0.5 dB cleaner, mid
  0.2 dB worse.
- Strong, finer: `--side-knee 6`. 1.2 side bands cut, side 1.5 dB cleaner, mid 0.8 dB
  worse.
- Exaggerated, finer: `--side-knee 24`. More bits go to the side than to the mid, and both
  channels end up worse than at +12.
- Mild, coarser: `--side-knee -1`. About five of the weakest side bands go (11.8 cut),
  mid 0.1 dB cleaner.
- Strong, coarser: `--side-knee -6`. 19 side bands cut, side 2.2 dB worse, mid 0.3 dB
  cleaner. Clearly narrower, still stereo.
- Exaggerated, coarser: `--side-knee -24`. 36 of 37 side bands gone, the side 8.9 dB
  quieter than in the source: a mono image with stereo bookkeeping.

Diagnostic companions, kept for measurements and possibly removed in a later version;
without `--side-knee` they do nothing:
- `--side-knee-zone <x>`, `1.0..20.0`, stock `3`: the offset is full up to |knee| dB above
  the threshold and fades out linearly to zero at x times |knee| dB.
- `--side-knee-spread <dB>`, `0.1..24.0`, stock `1`: for a negative knee, how many dB of
  knee it takes to spread the rescued bands fully by their real margin.

Works with / against: made to be used together with `--side-bias`. Inert under
`--msmask 0`.

Measured: at 96 kbps the coded side bands per frame fall evenly with a negative knee: 30.2
(stock), 29.4 (-0.25), 27.6 (-0.5), 25.4 (-1), 22.2 (-3), 18.1 (-6), 10.5 (-12), 1.1
(-24). On `dense48` at 40 kbps with `-w 18000`, where the frames are starved, -0.25 leaves
1.1 of 10.3 coded side bands, and from -4 on the stream no longer changes. The companions
move little: `--side-knee 6` with zone 1 and zone 10 differs by 0.06 dB of side error,
`--side-knee -3` with spread 0.1 and 10 by 0.15 dB.

#### `--side-cutoff <Hz>`

**From this frequency up, code every stereo band as pure mid: the top of the image for a
cleaner centre.**

- Applies to: stereo AAC core (LC, LD, ELD). Off by default.
- Values: `1000..24000` Hz, moved to the scalefactor band that starts at or above it
- Input: whole number, in Hz

Above the cutoff the side signal is dropped and the band is written as mid only, with the
cheap MS mask; pure L/R never happens there. Below the cutoff stereo is untouched. The
bits the side would have taken go to the mid, which matters at low bitrate, where the
side above a few kHz is mostly cut anyway and what is left of it is coarse.

The level stays. With the side gone both channels play the mid, which alone carries less
energy than the original channels whenever L and R differ (uncorrelated L/R would come out
3 dB quieter). So the mid is raised to the energy of the original, by at most 6 dB, and
only where the side is above its masking threshold; a side that stock would cut anyway is
not added. Two things the mid cannot carry at any level: content in anti-phase (L = -R,
its mid is zero) and the direction of a hard panned sound, which ends up in the centre.

Intensity stereo above the cutoff: FDK's own IS decision is cleared there, because pure
mid is cheaper. IS stays when it is asked for explicitly:
- `--is-force-lo/-hi`: IS on the given bands, always.
- loosened IS criteria (`--is-aggression`, `--is-corr-thresh`, `--is-lr-ratio`,
  `--is-min-sfbs`, `--is-admit` above 5): FDK's own IS decision with those criteria. Single
  panned regions above the cutoff then keep their direction (IS carries it per band), the
  rest stays mid, like the intensity stereo of MPEG-1 Layer II.

Bands that both channels code as PNS are left as they are. `--verbose` prints the band
where the cutoff starts, in the long and in the short table. Both round up to the next
band edge, so on the coarse short table the edge can only move up (9 kHz: long band 31 at
9.0 kHz, short band 9 at 10.5 kHz), which removes less side, never more.

Examples (acoustic stereo music, 48 kbps):
- Mild: `--side-cutoff 8000`. Side information from 238 to 235 bits per frame, the mid a
  little cleaner.
- Strong: `--side-cutoff 5000`. Side information 222 bits, mid error 0.7 dB lower, side
  above 5 kHz about 27..31 dB down.
- Exaggerated: `--side-cutoff 1000`. Practically mono with a stereo bass.

Works with / against: `--side-bias` and `--side-knee` still act below the cutoff. At
14 kbps the gain is small, because FDK already cuts almost all of the side there;
`--is-admit 0` is the bigger lever at that rate.

Measured: on `dense48` at 40 kbps, `8000` leaves the mid as it was and takes the side
1.2 dB down; `5000` makes the mid 1.3 dB cleaner and the side 3.4 dB quieter. With a
3..6 kHz source panned 4 dB left and right in turn over a mono mix (40 kbps,
`--side-cutoff 1000`) the cutoff alone leaves 0 dB of panning, with `--is-aggression 100`
3..4 dB, as in the source.

### MS laboratory

These switches reach into the MS decision and the MS thresholds directly. They answer
technical questions (what happens in pure L/R, with MS in a fixed range of bands, with the
decision tilted) and serve experiments. Measured on music, none of them makes the decoded
result clearly better than stock, and several make it worse. Three of them are Q8 numbers,
FDK's internal fixed point scale where `256` means 1.0 (see *Shared conventions*).

Under CBR the three threshold switches (`--ms-bias`, `--ms-precision`, `--mid-bias`) are
mostly undone by the rate loop, which still has to hit the same number of bits per frame.
Under VBR (`-m 1..5`) the bit count follows the thresholds, so there `--ms-precision` and
`--mid-bias` do have an audible effect, on quality and on file size.

#### `--msmask <n>`

**Forces the mid/side decision for every band instead of letting the encoder choose band
by band.**

- Applies to: stereo AAC core (LC, LD, ELD and the stereo core under HE-AAC). No effect in
  HE-AAC v2, where the core is mono.
- Values: `-1` auto (default, FDK decides per band), `0` off (pure L/R everywhere), `1` on
  (MS in every band)
- Input: whole number

The encoder normally tries each band both ways and keeps whichever is cheaper. On
correlated music that is MS in nearly every band, and because the rotation is lossless
the choice costs nothing in sound. Forcing it either way is a technical tool.

`--msmask 0` guarantees that the coder never mixes the channels. Quantisation noise then
stays in the channel that caused it instead of being shared between mid and side and
spread to both outputs. That matters if the file will later go through centre
cancellation, vocal removal or any other subtraction of one channel from the other. It
costs quality at the same bitrate, clearly on correlated music.

`--msmask 1` forces MS even where L/R would be cheaper. Useful as a reference point; on
material with genuinely independent channels, two unrelated sources panned hard, it
wastes bits.

Values:
- `-1`: stock, per band decision from the encoder's own cost comparison.
- `0`: pure L/R in every band. Nothing is mixed between channels.
- `1`: MS in every band.

Example in context:

    fdkaac -p 2 -b 128000 --msmask 0 -o lr.m4a in.wav

Works with / against: `--msmask 0` makes the whole mid/side family inert, including
`--side-bias`, `--side-knee`, `--msbands` and `--mid-bias`, because no band is in MS.
`--msbands`, `--msbands-lo` and `--msbands-hi` narrow where the mask may apply.

Measured (acoustic stereo music, 96 kbps): `--msmask 0` makes the mid error 1.3 dB and the
side error 3.9 dB higher than stock. `--msmask 1` stays within 0.4 dB of stock, because
stock already codes 39.5 of about 40 bands per frame in MS.

#### `--msbands <n>`

**Allows MS only in the N lowest scalefactor bands and leaves everything above in L/R.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` no limit (default), or `0..N` where N is the active SFB count; `n` means
  bands `0..n-1` may use MS
- Input: whole number

The "from the bottom up to" form of the band limit, counted from the bass upward:
`--msbands 6` lets bands 0 to 5 use MS and keeps bands 6 and above in L/R. `--msbands 0`
allows MS nowhere, the same as `--msmask 0`. The difference from `--msbands-lo` and
`--msbands-hi` is only the shape of the region: this switch always starts at band 0.

The reason to use it is technical, a later process that needs untouched L/R above some
frequency. There is no quality reason: every restriction measured so far made both
channels worse at the same bitrate.

Examples:
- Mild: `--msbands 40`. Only the top bands go back to L/R; on most material the stream
  barely moves, because those bands carry little.
- Strong: `--msbands 20`. MS confined to the lower part of the spectrum; at 96 kbps on
  acoustic music the mid error rises 1.3 dB and the side error 3.5 dB.
- Exaggerated: `--msbands 2`. MS on essentially nothing: nearly the full price of pure
  L/R, with MS still signalled in two bands.

Works with / against: equal to `--msbands-lo 0 --msbands-hi n-1` (`--msbands 6` and
`--msbands-lo 0 --msbands-hi 5` give the same stream). Inert under `--msmask 0`. A limit
above the active SFB count is the same as no limit.

Measured (acoustic stereo music, 96 kbps): `--msbands 30` raises the mid error by 0.9 dB
and the side error by 2.4 dB, `--msbands 20` by 1.3 and 3.5 dB.

#### `--msbands-lo <n>`

**Lowest scalefactor band of the range in which MS is allowed.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count; give it together
  with `--msbands-hi`
- Input: whole number

Together with `--msbands-hi` this defines a from..to window: MS may be used in bands
`lo..hi` inclusive and nowhere else. Unlike `--msbands` the window does not have to touch
the bottom of the spectrum, which is the only way to keep MS out of the bass. Give both
halves of the pair; `--msbands-lo` alone leaves the upper edge unset, which is not a
range.

Examples:
- Mild, window starting low: `--msbands-lo 2 --msbands-hi 48`. Almost everything may use
  MS; the two lowest bands are excluded.
- Strong, window in the middle: `--msbands-lo 10 --msbands-hi 30`. MS only across the
  mids, bass and top in L/R.
- Exaggerated, window at the very top: `--msbands-lo 44 --msbands-hi 48` with `-w 20000`.
  MS on five bands of air only. Without `-w 20000` these bands lie above the default
  cutoff of about 15.75 kHz and the setting becomes `--msmask 0` (see *Why a range above
  the coded bandwidth does nothing*).

Works with / against: read it against `active SFBs` in `--verbose` and against the coded
bandwidth. Inert under `--msmask 0`.

Measured (acoustic stereo music, 96 kbps): `--msbands-lo 20 --msbands-hi 48` raises the
mid error by 0.5 dB and the side error by 0.7 dB.

#### `--msbands-hi <n>`

**Highest scalefactor band, inclusive, of the range in which MS is allowed.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count; give it together
  with `--msbands-lo`
- Input: whole number

The upper edge of the window described above, inclusive: `--msbands-hi 5` includes band
5, bands above it go to L/R. In short blocks the range covers the same frequencies on the
short table (see *Short blocks* above), so `--msbands-lo 18 --msbands-hi 30` at 48 kHz is
short bands 3..7.

Examples:
- Mild: `--msbands-lo 0 --msbands-hi 44`. The top four bands excluded from MS.
- Strong: `--msbands-lo 0 --msbands-hi 20`. MS up to roughly the presence region, L/R
  above.
- Exaggerated: `--msbands-lo 0 --msbands-hi 0`. MS in exactly one band, the bass.

Works with / against: `--msbands-lo` is the other half; without it there is no range. A
value above the active SFB count is the same as no upper limit. Inert under `--msmask 0`.

#### `--ms-bias <n>`

**Tilts the encoder's L/R against MS cost comparison toward MS.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `0..255`, Q8 in FDK's logarithmic cost units; `-1` and `0` = stock (default)
- Input: whole number

For every band the encoder estimates what L/R and what MS would cost and takes the cheaper
one. This switch adds a bonus to the MS side of that comparison. Since MS is lossless, the
comparison decides only how the band is written, not how it sounds, and stock already
picks MS in nearly every band of correlated music.

The useful range is tiny. Values 1, 2 and 4 give different streams. From about 8 (16 on
dense material) the bonus is larger than any real difference between the two costs, every
band that may use MS takes it, and higher values change nothing: the result is close to
`--msmask 1`.

Values:
- `-1` or `0`: stock comparison.
- `1..8`: the decision tilted progressively toward MS.
- `8..255`: MS wherever it is allowed, the same stream for every value.

Example in context:

    fdkaac -p 2 -b 128000 --ms-bias 4 -o biased.m4a in.wav

Works with / against: pointless with `--msmask 0` or `--msmask 1`, which remove the
decision. For anything you can hear, use `--side-bias` and `--side-knee`.

Measured: acoustic stereo music at 96 kbps, `128` raises the mid error by 0.3 dB and
leaves the side as it was; on `dense48` at 40 kbps `255` changes nothing measurable.

#### `--ms-precision <n>`

**Lowers the thresholds of every MS band, mid and side together, so both are coded more
finely.**

- Applies to: stereo AAC core, MS bands only. No effect in HE-AAC v2.
- Values: Q8 from `256` (no change) upward, `384` is 1.5x, `512` is 2x; below 256 is
  rejected; `-1` off (default)
- Input: whole number

The thresholds of a band in MS are divided by the given factor, so the encoder treats
more of both channels as audible. It moves mid and side in the same direction by the same
amount, so it does not change their balance; the balance is what `--side-bias` and
`--side-knee` are for.

Under CBR it does almost nothing. The frame still gets the same number of bits, the rate
loop raises the thresholds back until the frame fits, and the decoded result stays within
a few tenths of a dB of stock. Under VBR the bit count follows the thresholds, and there
this switch works as a quality lift for the stereo pair, paid for in file size.

Examples (VBR `-m 3`, acoustic stereo music, stock about 108 kbps):
- Mild: `--ms-precision 384`. A slightly larger file, mid and side a little cleaner.
- Strong: `--ms-precision 512`. 5.3% larger, mid error 0.8 dB and side error 0.7 dB
  lower.
- Exaggerated: `--ms-precision 2048`. 15.2% larger, 2.2 and 1.9 dB lower: a bitrate
  increase in disguise, which `-m` does more evenly.

Works with / against: inert under `--msmask 0`. Under VBR a higher `-m` raises quality
across all bands, not only MS ones.

Measured: CBR 96 kbps, acoustic stereo music: `512` lowers the side error by 0.13 dB,
`4096` by 0.4 dB, the mid not at all; on `dense48` at 40 kbps `1024` changes nothing
measurable. VBR as in the examples.

Under the hood: the factor is applied as an offset to the band thresholds in FDK's log
domain, right after the MS rotation.

#### `--mid-bias <n>`

**Raises the mid channel threshold in MS bands, so the mid is coded more coarsely.**

- Applies to: stereo AAC core, MS bands. No effect in HE-AAC v2.
- Values: Q8, `256` = no change, above 256 raises the mid threshold; below 256 is
  rejected; `-1` off (default)
- Input: whole number

The idea is to free bits from the mid for the side. In measurement it does not do that
well. Under CBR the rate loop undoes most of it: at 96 kbps `384` moves mid and side by
about 0.1 dB, and only `4096` gives the side a full dB, for 0.65 dB of mid. Under VBR the
mid simply gets coarser and the file smaller, while the side gains nothing. For width
against centre use `--side-bias` or `--side-knee`.

Examples (CBR, acoustic stereo music, 96 kbps):
- Mild: `--mid-bias 288`. No measurable change.
- Strong: `--mid-bias 1024`. Under CBR a fraction of a dB; under VBR `-m 3` a 6.8% smaller
  file with the mid 1.7 dB worse and the side unchanged.
- Exaggerated: `--mid-bias 4096`. The side 1 dB cleaner, the mid 0.65 dB worse. For
  comparison `--side-knee 3` gives the side 0.8 dB for 0.3 dB of mid.

Works with / against: inert under `--msmask 0`. Combined with a positive `--side-bias` it
pulls on the same budget from the other end.

Measured: on `dense48` at 40 kbps `512` and `1024` change nothing measurable. VBR as in
the examples.

### Intensity stereo (IS)

Intensity stereo is the lossy one. Across a run of upper bands the encoder stops sending
two spectra and sends a single energy envelope plus a direction, and the decoder rebuilds
both channels from that one spectrum. The saving is large, the cost is stereo separation
in exactly those bands, and whether you can hear the cost depends entirely on what lives
up there: a correlated cymbal wash survives it well, a hard panned effect or a wide
reverb does not.

Stock FDK is very cautious with IS. There is not one gate but several, and a band is coded
as intensity only when all of them pass at once: a bitrate per band admission rule, a
correlation test between the channels, a minimum run of contiguous bands, and a stable
direction. This is why moving a single threshold so often appears to do nothing, another
gate is still blocking. Loosening `--is-corr-thresh` on its own, for instance, does not
open IS.

**Start with `--is-aggression` from group A.** It is the consumer slider for exactly this
problem: a single value from 0 to 100 that moves all of the gates together and coherently.
0 is stock, which in practice means IS barely activates at normal bitrates, and 100 is the
real maximum: the bitrate gate dropped, the correlation requirement loosened, the minimum
run reduced to a single band and the direction tolerance widened. Start around 30 to 50 and
listen, then go up if the saving is worth more to you than the width. Only once you know
what the slider does should you reach for the switches below, which override what it set.

#### `--is-admit <x>`

**Moves FDK's first gate, which decides whether intensity stereo may be used at all.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `0.0..20.0`, stock `5`; `0` = never; `-1` off (default)
- Input: decimal, up to 3 places, `.` or `,`

FDK allows intensity stereo only when the bitrate divided by the core bandwidth, in bits
per second per Hz, is below this value. At very low rates IS is expensive: it forces the
per band MS mask (about 22 bits per frame) and adds IS positions, so closing the gate can
free more bits than IS saves. `--is-aggression` opens the gate regardless of this value.

Measured: at 14 kbps stereo on dense material `--is-admit 0` lowers the share of zeroed
bands from 85% to 70%.

#### `--is <n>`

**Turn intensity stereo on or off globally instead of leaving it to the encoder.**

- Applies to: stereo AAC core (LC, LD, ELD, and the stereo core under HE-AAC). No effect
  in HE-AAC v2.
- Values: `-1` auto (default), `0` off, `1` on
- Input: whole number

This is the master switch, and it is a permission, not a command: `1` allows intensity
stereo, it does not place it. The gates described above still decide which bands actually
go to IS, so `--is 1` alone on material the encoder considers unsuitable, or at a bitrate
above the admission rule, can leave the stream unchanged. Pair it with `--is-aggression`
when you want IS to actually happen.

`--is 0` is the useful end of this switch on its own. It guarantees no intensity coding at
all, which is what you want when stereo separation in the top octaves matters more than
the bits, or when you are isolating the effect of some other switch and do not want IS
moving underneath the measurement.

Values:
- `-1`: stock. The encoder decides, cautiously.
- `0`: intensity stereo never used. Full L/R or MS coding everywhere.
- `1`: intensity stereo allowed, subject to the usual gates.

```
fdkaac -p 2 -b 64000 --is 1 --is-aggression 40 in.wav -o is.m4a
```

Works with / against: `--is-aggression` is what makes `--is 1` visible. `--is-force-lo`/
`--is-force-hi` bypass the gates entirely and work even without this switch. Completely
independent stereo is `--msmask 0 --is 0`.

#### `--isbands <n>`

**Cap how many scalefactor bands may be coded as intensity stereo.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` no limit (default), or a count within the active SFB count
- Input: whole number

A ceiling on the total extent of intensity coding, expressed as a number of bands rather
than as a position in the spectrum. It is the brake to use when IS is helping the bitrate
but you want to bound the damage: let the encoder pick where intensity goes, but refuse to
let it swallow half the spectrum.

Read the ceiling against `--verbose` `active SFBs`: at LC 48 kHz stereo there are 49
bands, so a cap of 40 is barely a cap at all and a cap of 4 is severe. Bands beyond the
cap are coded normally.

In short frames the cap holds for every window group on its own (each group is a slice of
time with its own copy of the band table) and is scaled to the coarser table: at 48 kHz
`--isbands 6` allows 2 short bands per group. Up to 1.4.0-beta.1 it counted across the
groups, so the first groups of a transient frame used it up and the rest of the frame got
no IS at all.

Examples:
- Mild cap: `--isbands 24`. Half the spectrum may still go intensity. On most material
  this changes little, because IS rarely reaches that far.
- Strong cap: `--isbands 8`. Intensity kept to a short run. Useful with a high
  `--is-aggression`, where it stops the slider from running away.
- Exaggerated cap: `--isbands 1`. A single band of intensity. A caricature: you keep the
  signalling and lose essentially all of the saving.

Works with / against: `--is-lo`/`--is-hi` restrict where IS may go, this one restricts how
much. They combine. Inert when IS never activates, so check with `--is-aggression` first.

#### `--is-min-sfbs <n>`

**Minimum run of contiguous bands before intensity stereo is allowed to start.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` default, which is 6, or a count of bands
- Input: whole number

One of the gates. The encoder refuses to start intensity coding on an isolated band or two;
it wants a continuous region of at least this many bands that all pass the other tests.
The reasoning is that a short intensity patch surrounded by normally coded bands is an
audible seam, a small region where the image suddenly collapses and then reopens.

Lower the number and IS catches short fragments as well, which means it triggers far more
often and on more material, with those seams as the risk. Raise it and IS becomes rarer:
only wide, consistently correlated regions qualify, which is the conservative setting.

This is an advanced knob. `--is-aggression` already moves it as part of its coordinated
sweep, so use this one only when you are tuning by hand with aggression at 0, and remember
that an explicit value overrides what the slider set.

Examples:
- Mild, more IS: `--is-min-sfbs 4`. Slightly shorter regions qualify. IS a little more
  frequent.
- Strong, more IS: `--is-min-sfbs 2`. Short fragments accepted. Clearly more intensity
  coding, and the first point where seams become plausible.
- Exaggerated, more IS: `--is-min-sfbs 1`. Any single band may become intensity. A
  caricature: intensity scattered through the spectrum one band at a time.
- Mild, less IS: `--is-min-sfbs 8`. Slightly rarer than stock.
- Strong, less IS: `--is-min-sfbs 16`. Only broad regions qualify; on most material IS
  almost never fires.
- Exaggerated, less IS: `--is-min-sfbs 40`. A run that long essentially never exists, so
  this is intensity stereo switched off by the back door. A caricature of a conservative
  setting, and `--is 0` says the same thing honestly.

Works with / against: one gate of several, so lowering it alone may change nothing if the
correlation or bitrate gate still blocks. Overridden in effect by `--is-force-lo`/`-hi`,
which bypass it. Prefer `--is-aggression`.

#### `--is-corr-thresh <n>`

**How similar the two channels must be in a band before intensity stereo is allowed.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: Q8, where 256 = 1.0 correlation; `-1` default, which is about 243, meaning
  roughly 0.95
- Input: whole number

The correlation gate, and the one people reach for first. The stock requirement is severe:
the two channels must be about 95 percent alike in a band before the encoder will consider
replacing them with one envelope and a direction. Very little real music is that correlated
in the top bands, which is most of the reason IS seems inactive by default.

The direction is counter intuitive, so read it twice: LOWER means MORE intensity. You are
lowering the bar that the material has to clear, so more bands clear it. 180 is about 0.70,
which is a lot more permissive than stock. Too low and you start coding genuinely different
channels as one steered source, which is heard as direction errors and as the top of the
image gluing together.

It is one gate among several, which has a practical consequence worth stating plainly:
changing this on its own does not open intensity stereo. If the bitrate admission rule or
the minimum run is still blocking, the stream will not move no matter what you set here.

Examples:
- Mild, more IS: `--is-corr-thresh 230`. About 0.90. Slightly more permissive than stock.
- Strong, more IS: `--is-corr-thresh 180`. About 0.70. Substantially more bands qualify;
  this is the usual hand tuning target.
- Exaggerated, more IS: `--is-corr-thresh 60`. About 0.23, which accepts channels that are
  barely related. A caricature: expect audible direction errors wherever it fires.
- Mild, less IS: `--is-corr-thresh 250`. Tighter than stock, IS slightly rarer.
- Strong, less IS: `--is-corr-thresh 254`. Only nearly identical channels qualify.
- Exaggerated, less IS: `--is-corr-thresh 256`. Perfect correlation demanded, which real
  material essentially never reaches. A caricature: intensity stereo disabled by an
  impossible test, where `--is 0` would be clearer.

Works with / against: alone it does not open IS, since the other gates still apply. Moved
automatically by `--is-aggression`, and an explicit value here overrides the slider.
Bypassed by `--is-force-lo`/`-hi`.

#### `--is-lr-ratio <n>`

**How even the left/right energy balance must be in a band before intensity stereo is
allowed.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: Q8, where 256 = 1.0; `-1` default, which is about 179, meaning roughly 0.7
- Input: whole number

The panning gate. Correlation says the two channels have the same shape; this says they
have comparable level. A band where one side carries almost everything is a badly chosen
place for intensity coding, because the single shared spectrum plus a direction has to
represent a strongly lopsided pair, and the direction parameter carries the whole burden.

Lower values accept more lopsided bands and therefore produce more intensity coding, in
the same counter intuitive direction as the correlation threshold. Higher values demand a
more even balance and make IS rarer.

This is the third of the three hand tuning thresholds and the one to set last. Work in the
order correlation, minimum run, then ratio: the first two decide whether IS fires at all,
this one mostly decides where.

Examples:
- Mild, more IS: `--is-lr-ratio 160`. Slightly more tolerant of uneven bands.
- Strong, more IS: `--is-lr-ratio 110`. Clearly lopsided bands accepted, more intensity
  placement across the spectrum.
- Exaggerated, more IS: `--is-lr-ratio 20`. Almost any imbalance accepted. A caricature:
  hard panned content gets reduced to one spectrum plus a pan value, which is precisely
  where IS sounds worst.
- Mild, less IS: `--is-lr-ratio 200`. A bit stricter than stock.
- Strong, less IS: `--is-lr-ratio 240`. Only well balanced bands qualify; IS becomes rare
  on anything with a wide mix.
- Exaggerated, less IS: `--is-lr-ratio 256`. Exact level equality demanded. A caricature
  that switches IS off through a test nothing passes.

Works with / against: one gate of several; alone it rarely changes the stream. Moved by
`--is-aggression`, overridden by an explicit value. Bypassed by `--is-force-lo`/`-hi`.

#### `--is-lo <sfb>`

**Allow intensity stereo only from this scalefactor band upward.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count
- Input: whole number

A placement restriction, not a trigger. Bands below the given number stay L/R or MS no
matter what the gates say; bands at or above it remain subject to the normal gates. This
switch can only take intensity coding away, never add it.

The obvious use is protecting the bottom of the spectrum. Intensity stereo high up is a
reasonable trade, intensity stereo in the mids or the bass is not, because that is where
the ear localises by level and timing and where a collapsed image is immediately heard.
There is a trap worth knowing: at low bitrate the encoder tends to place IS on relatively
LOW bands, lower than intuition suggests, so if you want to see this switch do anything,
scan small values rather than starting at 30.

Examples:
- Mild, push IS upward: `--is-lo 10`. Bass and low mids protected. Often enough to remove
  the worst placements.
- Strong, push IS upward: `--is-lo 25`. Intensity confined to the upper half of the
  spectrum. A sensible default for careful work at middling bitrate.
- Exaggerated, push IS upward: `--is-lo 46`. Intensity permitted only in the top few
  bands, which at the default coded bandwidth may be above the coded range entirely. A
  caricature, and the same trap as the MS high band example: check it against the coded
  bandwidth or it silently means "no IS at all".

Works with / against: pairs with `--is-hi` as a window. Restricts only, so it cannot make
IS happen; use `--is-aggression` for that. Has nothing to do with `--is-force-lo`, which
is the forcing switch with a similar name.

#### `--is-hi <sfb>`

**Allow intensity stereo only up to this scalefactor band, inclusive.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count
- Input: whole number

The upper edge of the permitted intensity window, inclusive, and the same kind of
restriction as `--is-lo`: it narrows where the encoder may place IS and never forces it.
Together the pair defines a from..to region, everything outside stays conventionally coded.

Capping from above is the less common direction, but it has a clear use. If the very top
of your spectrum carries the stereo information you care about most, air, cymbal placement,
room, you can let intensity take the busy upper mids and keep the top bands in real stereo.

Examples:
- Mild cap: `--is-hi 44`. Only the topmost bands excluded from intensity.
- Strong cap: `--is-hi 30`. Intensity confined below the top of the spectrum; the air band
  keeps genuine separation.
- Exaggerated cap: `--is-hi 3`. A window so low that the gates essentially never fire
  inside it. A caricature that amounts to disabling IS, where `--is 0` is clearer and
  cheaper to reason about.

Works with / against: `--is-lo` is the other half of the window. Combines with `--isbands`,
which limits the count rather than the position. Does not interact with the forced range.

#### `--is-force-lo <sfb>`

**Force intensity stereo from this band upward, bypassing every gate.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count; pair with
  `--is-force-hi`
- Input: whole number

This is the laboratory switch, and it behaves differently from everything above it. The
correlation test, the minimum run, the level balance rule and the bitrate admission rule
are all skipped: the bands in the forced range are coded as intensity because you said so.
It works with no other IS switch set, so it does not need `--is 1` and it ignores
`--is-aggression`.

Understand what that means for the sound. Intensity coding keeps one spectrum and a
direction, so within the forced range the channels can no longer differ in anything except
level and sign. On uncorrelated content the result is not subtle; it is a deliberate,
audible collapse of the image, and it can sound badly wrong. That is the point of the
switch: it lets you hear and measure the mechanism at full strength, and it lets you build
a specific effect. The stream stays standards compliant, so every decoder will play it.

Pair it with `--is-force-hi`. Set it high in the spectrum first to hear what intensity does
where it is meant to be used, then walk it downward to hear why the gates exist.

Examples:
- Mild force: `--is-force-lo 40 --is-force-hi 48`. Intensity on the top bands only, which
  is roughly where the encoder would put it anyway. Closest to a usable setting.
- Strong force: `--is-force-lo 20 --is-force-hi 48`. The whole upper half forced to
  intensity. Clear narrowing of the image above the mids, and a real bitrate saving.
- Exaggerated force: `--is-force-lo 0 --is-force-hi 48`. The entire spectrum coded as one
  steered channel. A caricature and a useful demonstration: this is what the stereo image
  costs you, all at once.

Works with / against: bypasses `--is-corr-thresh`, `--is-min-sfbs`, `--is-lr-ratio` and the
bitrate gate, so none of them limit it. Distinct from `--is-lo`, which only restricts.
Needs `--is-force-hi` to define the top of the range.

Measured: `--is-force-lo` and `--is-force-hi` change the stream with no other IS switch
given.

#### `--is-force-hi <sfb>`

**Upper band, inclusive, of the forced intensity range.**

- Applies to: stereo AAC core. No effect in HE-AAC v2.
- Values: `-1` off (default), or `0..N` within the active SFB count; pair with
  `--is-force-lo`
- Input: whole number

The other half of the forced range, inclusive of the band you name. Both halves together
define the region that is coded as intensity regardless of what the material looks like.

Keep the range inside the coded bandwidth. Forcing intensity on bands above the cutoff
forces it on silence, which changes nothing and will have you chasing a switch that is
working perfectly.

Examples:
- Mild: `--is-force-lo 44 --is-force-hi 46`. Three bands of forced intensity near the top.
  A narrow probe for A/B listening.
- Strong: `--is-force-lo 30 --is-force-hi 48`. A wide forced region through the upper
  spectrum.
- Exaggerated: `--is-force-lo 5 --is-force-hi 48`. Everything above the bass forced. A
  caricature: the bass keeps its stereo and everything else is steered mono, which sounds
  exactly as odd as it reads.

Works with / against: meaningless without `--is-force-lo`. Independent of the gate
switches and of `--is-lo`/`--is-hi`. Check the range against `--verbose` `active SFBs` and
against the coded bandwidth.

### Parametric stereo (PS)

Parametric stereo is what HE-AAC v2 (`-p 29`) uses, and it is a different proposition from
everything above. The waveform coder sees one channel: the two inputs are downmixed to
mono, the mono signal gets an AAC core plus SBR, and the stereo image travels separately
as a small set of parameters per frame. The decoder takes the mono signal, decorrelates it,
and rebuilds a left and a right from those parameters.

The parameters are few and specific. IID is the level difference between the channels per
parameter band, which is what places a level panned source. ICC is the coherence, how
alike the two channels are, which is what decides whether a band sounds like a point
source or like a wide diffuse field. Optionally IPD carries the phase difference between
the channels, and OPD says where that rotation sits relative to the downmix. That is the
whole stereo picture: a few dozen numbers per frame rather than a second spectrum.

This is why PS works at bitrates where nothing else does. At 24 or 32 kbps a real stereo
core would be starving both channels; a mono core at the same rate is comfortable, and the
parameters cost almost nothing. It is also why PS has its own failure modes, which have
nothing to do with quantisation noise: the image can be too coarsely sampled in frequency
(too few bands), too coarsely sampled in time (too few envelopes), or momentarily absent
(frames sent with no parameters at all). The switches below address exactly those three.

If you take one setting from this section, take `--ps-env 4 --ps-env-reduce 0`. Asking for
four envelopes alone often changes nothing, because an automatic reduction loop collapses
them again; switching that loop off is what delivers the improvement.

Everything here needs `-p 29`. Under LC, HE-AAC v1, LD or ELD there is no PS layer and
these switches do nothing.

#### `--ps <n>`

**Force the IID stereo parameter to be sent, or forbid it, instead of leaving it to the
encoder's heuristic.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `-1` auto (default), `0` off, `1` on
- Input: whole number

IID is the level difference parameter, the backbone of the PS image. The encoder decides
whether to send it from a loudness difference heuristic: if the two channels sit at
nearly the same level it may conclude there is nothing to describe and send nothing.

`--ps 0` suppresses IID entirely, which flattens the image toward the bare mono downmix
plus whatever ICC provides. It is a blunt instrument and mostly useful as a reference:
encode once with it to hear what the core alone sounds like, then compare.

`--ps 1` forces IID into the stream unconditionally. That is the setting that matters on
material the heuristic misjudges, near equal level content above all, and it is also what
carries the phase band count when you use IPD.

Values:
- `-1`: stock. The loudness difference heuristic decides.
- `0`: IID never sent. Flattened, near mono image.
- `1`: IID always sent.

```
fdkaac -p 29 -b 32000 --ps 1 in.wav -o ps.m4a
```

Works with / against: `--ps-ipd 1` makes the stream signal IID anyway, because that is the
only route by which the phase band count reaches the decoder. `--ps-iid-quant` chooses the
grid IID is quantised on. `--ps-icc` controls the other main parameter.

#### `--ps-iid-quant <n>`

**Choose the coarse or the fine quantisation grid for the IID level differences.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `-1` default, `0` coarse, `1` fine
- Input: whole number

IID is quantised onto a fixed grid of level differences. The fine grid resolves smaller
level differences and therefore places sources more precisely, especially near the centre
where small differences do most of the work. The coarse grid spends fewer bits per
parameter and, on material whose image is wandering slightly from frame to frame, can
actually sound steadier, because small analysis fluctuations quantise to the same step
instead of dithering between two.

At the bitrates where PS is used, the parameter bits are a small part of the budget, so
the fine grid is usually the right choice for music with real placement. Try coarse on
very low rate speech or on material where you can hear the image jitter.

Values:
- `-1`: stock grid for the configuration.
- `0`: coarse grid. Fewer parameter bits, blunter placement, steadier under jitter.
- `1`: fine grid. Finer placement, slightly more parameter bits.

```
fdkaac -p 29 -b 32000 --ps-iid-quant 1 in.wav -o fine.m4a
```

Works with / against: only meaningful when IID is actually sent, so pair with `--ps 1` if
the heuristic is suppressing it. Independent of `--ps-bands`, which decides how many IID
values exist rather than how precisely each is coded.

#### `--ps-icc <n>`

**Force the coherence parameter on or off.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `-1` auto (default), `0` off, `1` on
- Input: whole number

ICC tells the decoder how much of a band should come from the decorrelated copy of the
mono signal rather than from the mono signal itself. High coherence means a tight point
source, low coherence means a wide diffuse one. On reverberant material it is ICC, not
IID, that makes the difference between a room and a dry mono signal with a pan value.

`--ps-icc 0` removes that control, so bands fall back to the coherent case and the result
sounds drier and narrower, even when IID is still placing sources correctly. `--ps-icc 1`
forces coherence data into the stream where the encoder might have judged it unnecessary,
which is the setting to try on ambient or heavily reverberant material whose width sounds
collapsed.

Values:
- `-1`: stock. The encoder decides.
- `0`: no ICC. Drier, narrower, point source feel.
- `1`: ICC always sent. Diffuse content keeps its width.

```
fdkaac -p 29 -b 32000 --ps-icc 1 in.wav -o icc.m4a
```

Works with / against: `--ps-icc-mode` changes only how the same matrix is signalled and is
not a quality alternative to this switch. Complementary to `--ps`, which controls the level
parameter.

#### `--ps-ipd <0|1>`

**Transmit the inter-channel phase difference in the PS extension.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `0` off (default, stock, phase never sent), `1` on
- Input: whole number, on/off

Without phase, PS positions sound using level differences and coherence alone. That covers
anything panned by level, which is most of a modern mix, but it cannot represent a source
placed by TIMING. A real inter-channel delay, the way low frequency localisation actually
works in nature, is invisible to IID and ICC: both channels carry the same signal at the
same level and differ only in when it arrives. Turning on IPD sends that angle, quantised
onto the 8 defined steps of pi/4, in a length prefixed PS extension.

Where it helps is narrow and worth knowing exactly. IPD is coded for the lower 5 parameter
bands in the 10 band configuration and the lower 11 in the 20 band configuration, which in
frequency means roughly the bottom 690 Hz: those 11 bands live inside the first three QMF
bands. Listen or measure above 690 Hz and you will find nothing at all. The content that
benefits is low and low mid material, wide soft sources, reverb, and genuine inter-channel
micro delays.

There is also a ceiling, and it is a property of the representation rather than a defect.
A 45 degree grid can only track a delay while the phase it produces stays unambiguous
across the coded range. By the top of the IPD range a delay of about 0.7 ms has already
swung through nearly the full plus or minus 180 degrees, so the grid can no longer say
where it is. This is the classic phase ambiguity limit of ITD, and the same reason hearing
relies on timing cues mainly down low.

One practical coupling: the phase extension does not carry its own band count. The decoder
derives that from the IID signalling, so enabling `--ps-ipd` also makes the stream signal
IID. That is a cheap, always legal parameter and it is the only route by which the band
count reaches the decoder.

Compatibility is safe by construction. The payload sits in a length prefixed extension, so
a decoder without phase synthesis reads the byte count and skips it, which the standard
explicitly permits. Note which decoders use it: the Fraunhofer decoder IGNORES the IPD
extension, so decoding with it shows no change at all. ffmpeg does use it. Choose your
decoder deliberately before concluding the switch did nothing.

Values:
- `0`: stock. No phase in the stream.
- `1`: IPD transmitted for the lower 5 (10 band) or 11 (20 band) parameter bands, and IID
  signalled along with it.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 1 in.wav -o ipd.m4a
```

Works with / against: `--ps-opd` is the other half of the phase pair and is only meaningful
with this on. Forces IID signalling, so it overlaps `--ps 1`.

Measured, with ffmpeg as the decoder: probes built as identical signal in both channels,
equal level, offset only by an inter-channel delay, comparing the decoded phase profile
against the source inside the IPD range of 60 to 690 Hz.

| Inter-channel delay | phase error, IPD off | phase error, IPD on |
|---|---|---|
| 0.25 ms | 35.0 deg | 14.9 deg |
| 0.4 ms | 56.1 deg | 24.9 deg |
| 0.7 ms | 67.4 deg | 71.8 deg |

At 0.25 and 0.4 ms the phase error more than halves, with identical figures at 32 and
48 kbps. At 0.7 ms the benefit is gone, which is the ambiguity ceiling described above.

#### `--ps-opd <0|1>`

**Send the phase of the left channel relative to the mono downmix, the companion to IPD.**

- Applies to: HE-AAC v2 (`-p 29`) only, and only together with `--ps-ipd 1`
- Values: `0` pins OPD to zero, `1` computes it (default behaviour once IPD is on)
- Input: whole number, on/off

The distinction between the two phase parameters repays a careful read. IPD is the phase
of the left channel relative to the right. OPD is the phase of the left channel relative
to the mono downmix. The decoder needs both, because it rotates the paths fed from the
downmix by OPD and the others by OPD minus IPD. Subtract those two and OPD cancels, so the
inter-channel DIFFERENCE always comes out as IPD no matter what OPD says. What OPD decides
is where that rotation sits relative to the downmix.

That cancellation is exactly why the problem is easy to miss. With OPD pinned to zero the
left channel is glued to the downmix phase and the entire rotation is dumped onto the
right one. Any measurement of arg(L) minus arg(R) is blind to this, because the difference
is still correct. Measure each channel's absolute phase against the source instead, inside
the IPD range, and the asymmetry is plain, as is OPD removing it.

Deriving OPD costs essentially nothing, which is the neat part. The downmix is L+R, so
sum(L * conj(L+R)) equals sum(|L|^2) plus sum(L * conj(R)), whose real part is pwrL + pwrCr
and whose imaginary part is pwrCi: the same accumulators already feeding IID, ICC and IPD.
So OPD is atan2(pwrCi, pwrL + pwrCr), one more atan2 over numbers already in the loop, with
no downmix signal needing to be passed in from elsewhere. For the equal level delay case
this reduces analytically to IPD/2, since arg(1 + e^-i.phi) = -phi/2, which is a useful
sanity check; the general form is also correct when the channels differ in level, where
IPD/2 would not be.

Keep `0` for A/B comparison on identical material, and note that `--ps-opd 0` on its own,
without `--ps-ipd 1`, gives the stock stream, because there is no phase layer for it to
modify. Note also that OPD differences only appear on material with real inter-channel
delay; on level panned content there is nothing for it to correct.

Values:
- `0`: OPD pinned to zero. Phase error asymmetric between the channels.
- `1`: OPD computed from the existing accumulators. Symmetric phase error.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 0 in.wav -o opd0.m4a
```

Works with / against: meaningless without `--ps-ipd 1`. Does not change the inter-channel
difference, only the absolute phase of each channel, so pick your measurement accordingly.

Measured, with ffmpeg as the decoder: absolute phase error of each channel against the
source, inside the IPD range.

| Inter-channel delay | OPD = 0 (L / R) | OPD computed (L / R) |
|---|---|---|
| 0.25 ms | 14.7 / 20.1 deg | 14.4 / 17.3 deg |
| 0.4 ms | 23.0 / 28.0 deg | 15.5 / 17.8 deg |
| 0.7 ms | 48.1 / 56.2 deg | 40.9 / 36.3 deg |

At OPD = 0 the two channels never agree, 14.7 against 20.1 and later 48.1 against 56.2.
With OPD computed the pair converges, by +6.4 degrees on average and +13 at 0.7 ms. Note
that 0.7 ms is the case where the plain IPD measurement showed no benefit: the
inter-channel difference was already past what a 45 degree grid can resolve, yet the
absolute phase of each channel still improved substantially.

#### `--ps-icc-mode <n>`

**Select which of the two defined ICC rotation modes is signalled.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `-1` default, `0` ROT_A, `1` ROT_B
- Input: whole number

Set expectations before you spend time here: this is signalling only. Both modes describe
the same stereo matrix, the decoder simply derives it differently. Treat it as a
compatibility knob rather than a quality one, and if you are hunting for audible stereo
differences in PS, `--ps-bands` and `--ps-env` are where they actually live.

It is worth having because decoder implementations are not all equally exercised on both
paths, so if a particular player renders a PS stream oddly, switching the rotation mode is
a cheap thing to test.

Values:
- `-1`: stock choice for the configuration.
- `0`: ROT_A signalled.
- `1`: ROT_B signalled.

```
fdkaac -p 29 -b 32000 --ps-icc-mode 1 in.wav -o rotb.m4a
```

Works with / against: only relevant when ICC is being sent, so check `--ps-icc`. Audibly
far less significant than `--ps-bands`.

#### `--ps-bands <10|20>`

**Frequency resolution of the stereo parameters: how many PS bands describe the image.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `10` or `20`; `-1` picks from the bitrate (default), and from 22 kbps upward the
  table always gives 20
- Input: whole number

This is one of the two axes that decide how many stereo parameters exist at all, and it is
the frequency axis. Twenty bands describe the image in finer spectral detail, so a mix
whose placement varies across the spectrum, bass centred, guitars wide, hats offset, keeps
that structure. Ten bands average the same picture over twice the spectrum each.

The interesting case is the low end of the bitrate range. A coarse grid is not only
cheaper, it can also sound steadier, because each parameter is averaged over more spectrum
and therefore jitters less from frame to frame. A wandering image is more distracting than
a slightly blurred one. Stock FDK derives the count from the bitrate alone and always
picks 20 at 22 kbps and above, so without this switch you cannot audition the coarse grid
at all on normal material.

Values:
- `10`: coarse grid. Fewer parameter bits, blurrier placement across frequency, steadier
  under analysis jitter.
- `20`: fine grid. Finer placement, more parameter bits.
- `-1`: stock, picked from the bitrate.

```
fdkaac -p 29 -b 24000 --ps-bands 10 in.wav -o coarse.m4a
```

Works with / against: `--ps-env` is the time axis of the same idea. Both are audibly more
significant than `--ps-icc-mode`. `--ps-iid-quant` changes the precision of each value
rather than how many values there are.

#### `--ps-env <1|2|4>`

**Time resolution of the stereo parameters: how many parameter envelopes per frame.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `1`, `2` or `4`; `-1` picks from the bitrate (default), and from 36 kbps upward
  the table gives 4
- Input: whole number

The other resolution axis. One envelope means the whole frame gets a single stereo
snapshot, four means the image is sampled four times within the frame. This is what decides
whether a moving pan or a left/right transient keeps its position or smears across the
frame into an average.

There is a trap, and it is the most important thing in this section: asking for four
envelopes does not mean you get four. An automatic reduction loop keeps halving the count,
four to two to one, whenever neighbouring envelopes look similar by a built-in error
threshold, and on ordinary music that threshold is met very often. The request is undone
before it reaches the stream. `--ps-env-reduce 0` is what stops that, and the measurement
under that card shows the difference plainly.

Values:
- `1`: one stereo snapshot per frame. Cheapest, smears any movement.
- `2`: two envelopes. A reasonable middle setting with `--ps-env-reduce 0`.
- `4`: four envelopes. Tracks moving panorama and transients closely.
- `-1`: stock, picked from the bitrate.

```
fdkaac -p 29 -b 32000 --ps-env 4 --ps-env-reduce 0 in.wav -o env4.m4a
```

Works with / against: effectively requires `--ps-env-reduce 0` to be honoured on ordinary
material. `--ps-bands` is the frequency axis of the same tradeoff.

Measured: at HE-AAC v2 32 kbps `--ps-env 4` does change the stream, because the bitrate
table picks 2 there; at 48 kbps the table already gives 4, so the request matches stock and
only `--ps-env-reduce 0` makes a difference.

#### `--ps-env-reduce <0|1>`

**Switch off the automatic envelope halving loop so the requested envelope count is the
one transmitted.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `0` disables the reduction loop, `1` leaves it on; `-1` off, meaning stock, which
  has the loop on (default)
- Input: whole number, on/off

The encoder keeps a reduction loop that compares neighbouring parameter envelopes and
collapses four into two, and two into one, whenever they look similar enough by a built-in
error threshold. The intent is to save parameter bits on static material. The side effect
is that the envelope count you asked for is frequently not what gets transmitted, and
because the saving is small while the cost is temporal resolution of the image, the trade
is often a bad one on material with any movement at all.

`--ps-env-reduce 0` turns the loop off and makes `--ps-env` literal. This is the single
highest value setting in the PS group. The gain is in temporal accuracy of the stereo
picture, at essentially the same file size, because parameter bits are a small fraction of
the budget at these rates.

Values:
- `-1` or `1`: stock. The loop runs and may collapse the envelope count.
- `0`: loop disabled, `--ps-env` taken literally.

```
fdkaac -p 29 -b 48000 --ps-env 4 --ps-env-reduce 0 in.wav -o literal.m4a
```

Works with / against: pointless without `--ps-env`, since its whole function is to protect
that request. Combines freely with `--ps-bands`.

Measured: a 4 second stereo probe with a panorama swept at 0.25 Hz plus alternating left
and right transients, HE-AAC v2 at 48 kbps, comparing the decoded panorama trajectory
against the source.

| Setting | Panorama error (RMS) | Correlation with source |
|---|---|---|
| stock (20 bands / 4 envelopes) | 0.177 | 0.9655 |
| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
| `--ps-env 4 --ps-env-reduce 0` | 0.117 | 0.9944 |

Requesting 4 envelopes alone changes nothing at 48 kbps, the output is byte identical to
stock, because the reduction loop undoes the request immediately. The gain appears only
once the loop is stopped: a third less panorama error for roughly the same file size.

#### `--ps-noenv-skip <0|1>`

**Forbid parameter-less PS frames, so every frame carries stereo information.**

- Applies to: HE-AAC v2 (`-p 29`) only
- Values: `0` never emit parameter-less frames, `1` allow them; `-1` off, meaning stock,
  which allows them (default)
- Input: whole number, on/off

Beyond thinning out envelopes, the encoder can skip stereo parameters entirely for a
stretch of frames. When successive IID and ICC sets look similar it may send up to ten
consecutive frames carrying no stereo information at all, leaving the decoder to coast on
the last values it received.

On static material that is free. On material where the image drifts slowly it is audible
as the stereo picture flattening and then snapping back when parameters resume, because
the decoder held a stale snapshot through the drift and then jumped to the current one.
Once you know the artefact you tend to hear it, and it is more distracting than the bits
it saves are worth.

`--ps-noenv-skip 0` forbids the skipping and forces parameters into every frame. Use it
whenever the image seems to breathe or stutter. The cost is a modest number of parameter
bits per frame.

Values:
- `-1` or `1`: stock. Up to ten consecutive frames may carry no stereo parameters.
- `0`: parameters in every frame.

```
fdkaac -p 29 -b 32000 --ps-noenv-skip 0 --ps-env 4 --ps-env-reduce 0 in.wav -o steady.m4a
```

Works with / against: the same family of hidden thinning as `--ps-env-reduce`, and the two
are usually set together when the goal is a stable image. Neither changes the core audio,
only the stereo parameter stream.

## C. Bandwidth and SBR

Two different things decide how high your file sounds, and they are easy to confuse.

The first is the **core bandwidth**: the highest frequency the AAC core itself actually
codes, with real spectral lines and real bits. Above that line the core sends nothing.

The second is the **SBR crossover**, also called the SBR start frequency. SBR (spectral
band replication) does not code the high band. It copies lower, already coded spectrum
upward and then describes, with a handful of parameters per frame, how that copy should
be shaped. The crossover is where that copy begins. In a well formed HE-AAC stream the
core bandwidth and the crossover sit at the same place: the core stops, SBR takes over,
and the listener hears one continuous spectrum.

What SBR sends per frame is a short list:

- **envelopes**: the level of each SBR band over time. More envelopes per frame means
  finer time resolution up top and more bits.
- **noise floor**: how much synthetic noise to mix into each band. This is what turns a
  copied tonal region back into something that sounds like air, cymbals or breath.
- **inverse filtering level** (invf): per band, how hard to whiten the copy. The copied
  material carries the tonal structure of the band it came from. If that structure does
  not belong up there, you hear it as a metallic, ringing, chorused top. Whitening
  flattens that structure out.
- **missing harmonics**: flags that tell the decoder to insert a synthetic sine in a band
  where a real partial of the original got lost or landed in the wrong place.

The last two are decided by **estimators** inside the encoder, which compare the tonality
of the original high band with the tonality of the patched copy, band by band and frame
by frame. The estimators are where the character of the top end is set, and they trade
off against each other in one direction: too little whitening and too little fill noise
gives a metallic, ringing, phasey top; too much of both gives a hissy, washed out top
where cymbals turn into shaped noise and sustained partials disappear. Most of the knobs
in this group move you along exactly that axis. There is no universally correct point on
it; it depends on the material and on the bitrate.

SBR is active in these profiles:

- `-p 5` HE-AAC
- `-p 29` HE-AAC v2 (SBR plus parametric stereo)
- `-p 39 -L 1` AAC-ELD with SBR

In `-p 2` (AAC-LC), `-p 23` (AAC-LD) and `-p 39` without `-L 1` there is no SBR, and
every `--sbr-*` switch in this group does nothing. `--core-cutoff`, at the end of the
group, works everywhere, but without SBR it is the same as `-w`.

Two conventions used throughout this group:

- **x100 knobs** scale an internal constant: `100` means 1.00 and is exactly stock,
  below 100 makes the constant smaller, above 100 makes it larger. Both directions work.
  A few knobs are documented as **raw x100**, which means the value replaces the constant
  outright rather than scaling it (`--sbr-tran-peak 90` *is* the stock 0.90).
- `--verbose` prints the effective core bandwidth, crossover, final top edge and SBR
  header period. Under SBR it is the only reliable way to see what you actually got, and
  several switches in this group are validated by FDK only at initialisation time, so
  checking is not optional.

### SBR frequency layout: `--sbr-start` `--sbr-stop` `--sbr-freqscale` `--sbr-alterscale` `--sbr-noise-bands` `--sbr-amp-res` `--sbr-data-extra`

These seven switches define the shape of the SBR band before anything is measured in it:
where the replicated region starts, where it ends, how it is divided into bands, how
finely the noise floor is described, and how precisely envelope levels are quantised.

The two important ones are `--sbr-start` and `--sbr-stop`. Both take a table index, not a
frequency, and the frequency an index maps to depends on the sample rate, the profile and
the bitrate. Measured values are given in the cards below; read them as a guide to the
direction and the spacing, and confirm the actual numbers for your settings with
`--verbose`.

The rule that catches everyone: **legality depends on the pair, the bitrate and the sample
rate.** SBR requires a valid number of master bands between start and stop, and FDK
validates the combination when the encoder initialises. A bad pair does not degrade
gracefully; it reports "encoder initialization failed" and nothing is written. The legal
combinations form a diagonal band: as you raise the start you must usually raise the stop
with it. At HE-AAC 64 kbps stereo, 48 kHz input, every start from 0 to 4 is illegal with
every stop, and the documented looking pair `--sbr-start 5 --sbr-stop 9` fails there.
Pairs that do work at that setting: `8/9`, `8/14`, `14/12`.

So: change one index at a time, run the encode, and read `--verbose`. An encode that fails
to initialise costs you a second; a stream with a crossover you did not intend costs you
the listening session.

#### `--sbr-start <n>`

**Sets where the core stops and the replicated high band begins.**

- Applies to: SBR profiles only (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Values: table index `0..15`; default `-1` = the value from the SBR tuning table
- Input: whole number

This is the most consequential single number in an HE-AAC stream. Everything below the
crossover is real coded spectrum; everything above it is a described copy. Lowering the
start gives the core less to do, so what it does code it codes more cleanly, and a larger
share of the spectrum becomes parametric. Raising the start does the opposite: more real
treble, a smaller SBR region, and a core that has to pay for it.

At very low bitrate, lowering the crossover is often the only way to make the core sound
clean at all. At 64 kbps and above, raising it tends to win, because the core can afford
genuine 10..12 kHz detail and a copied band always carries some of the character of the
band it was copied from.

Measured core bandwidth per start index, as printed by `--verbose` at HE-AAC `-p 5`,
64 kbps, 48 kHz stereo input, with the default stop. "Illegal" means encoder
initialisation failed for every stop tried:

| `--sbr-start` | core bandwidth |
|---|---|
| 0..4 | illegal at this setting |
| 5 | 5062 Hz (only with `--sbr-stop 0`) |
| 6 | 5484 Hz |
| 7 | 5953 Hz |
| 8 | 6468 Hz |
| 9 | 6468 Hz |
| 10 | 7031 Hz |
| 11 | 7640 Hz |
| 12 | 8250 Hz |
| 13 | 9000 Hz |
| 14 | 10500 Hz (stock at this bitrate) |
| 15 | 12000 Hz |

The same sweep at 44.1 kHz input, HE-AAC 32 kbps: start 5 gives 5038 Hz, 8 gives 5943 Hz,
11 gives 7579 Hz, 14 gives 9646 Hz, 15 gives 11025 Hz. Note that index 8 and 9 land on the
same core bandwidth at 48 kHz: the mapping is a table, not a formula, and neighbouring
indices can coincide.

Examples, lowering the crossover (HE-AAC 64 kbps stereo, 48 kHz, stock 14):
- Mild: `--sbr-start 13`. Core to 9000 Hz. Marginally cleaner core, slightly wider patch.
- Strong: `--sbr-start 11`. Core to 7640 Hz. Noticeably more of the top is parametric.
  Worth trying when the core sounds grainy.
- Exaggerated: `--sbr-start 6 --sbr-stop 9`. Core to 5484 Hz. Almost everything above the
  presence region is a copy; cymbals and sibilants are entirely SBR's invention. This is
  the right shape at 24 kbps and a caricature at 64.

Examples, raising the crossover:
- Mild: `--sbr-start 15`. Core to 12000 Hz, the top of the index range at this rate. Real
  coded spectrum up to 12 kHz, a narrower SBR region above it.
- Strong: raising the crossover further is not available through this switch at this
  setting, because 15 is the highest index. The next step is to drop SBR and code the
  whole band with `-p 2` plus `-w`.
- Exaggerated: the same, taken to its conclusion: `-p 2 -b 64000 -w 16000`. No SBR, no
  copy, and at 64 kbps stereo a core that is clearly short of bits for a 16 kHz band.
  Instructive to compare against HE-AAC on the same material.

Works with / against: must form a legal pair with `--sbr-stop`. The core follows the
start by itself; `--core-cutoff` can pull it below the start, which opens a gap, and
cannot usefully push it above. Raise the start in single steps and listen: each step
moves real coded spectrum up and costs the core bits. Lowering the start widens the band that `--sbr-noise-bands`,
`--sbr-invf*` and the missing harmonics detector are responsible for, so their settings
matter more the lower you go.

#### `--sbr-stop <n>`

**Sets the top edge of the replicated band, which is the top edge of the file.**

- Applies to: SBR profiles only
- Values: table index `0..13`; default `-1` = the value from the SBR tuning table
- Input: whole number

Where `--sbr-start` splits the spectrum, this sets the ceiling. Raising it extends the
patched region higher, which costs almost nothing in bits (SBR describes bands, not lines)
but spreads the same parameter budget over a wider region, so each band is described more
coarsely. Lowering it closes the file off earlier, which sounds like a conventional
lowpass and sometimes reads as cleaner, because a mediocre 18..22 kHz patch contributes
more hiss than content.

Measured final top edge of the decoded output (core plus SBR) per stop index, `--verbose`
at HE-AAC `-p 5`, 64 kbps, 48 kHz stereo input, with `--sbr-start 14`:

| `--sbr-stop` | final top edge |
|---|---|
| 0..3 | illegal at this setting |
| 4 | about 10875 Hz |
| 5 | about 12000 Hz |
| 6 | about 13125 Hz |
| 7 | about 14250 Hz |
| 8 | about 15375 Hz |
| 9 | about 16875 Hz |
| 10 | about 18375 Hz |
| 11 | about 20250 Hz |
| 12 | about 22125 Hz (stock) |
| 13 | illegal at this setting |

Examples, lowering the top edge (from stock 12):
- Mild: `--sbr-start 14 --sbr-stop 11`. Top at about 20250 Hz. Hard to hear, slightly
  denser description of what remains.
- Strong: `--sbr-start 14 --sbr-stop 9`. Top at about 16875 Hz. A deliberate, audible
  ceiling, with the remaining bands described more finely. Often the better trade at
  32 kbps and below.
- Exaggerated: `--sbr-start 14 --sbr-stop 4`. Top at about 10875 Hz with a 10500 Hz core:
  SBR now covers a few hundred Hz and contributes nothing. You have effectively built an
  AAC-LC stream with SBR overhead attached.

Examples, raising the top edge:
- Mild: none upward at this setting. Stop 12 (about 22125 Hz) is already the stock top
  and stop 13 is not legal with start 14. The top can be held while the crossover moves
  up: `--sbr-start 15 --sbr-stop 12` keeps about 22125 Hz with a 12000 Hz core.
- Strong: `--sbr-start 8 --sbr-stop 14`. A wide patched region from 6468 Hz up. Cheap in
  bits, coarse in description: watch for a "shimmering" quality on dense material.
- Exaggerated: `--sbr-stop 13` at this setting, which fails to initialise. The index
  exists in the standard but not in every legal pair, and this is the fastest way to see
  what the diagonal legality band means in practice.

Works with / against: the pair constraint with `--sbr-start` governs both. Index 13 is
accepted by the parser but is not legal in every pair, which is why `--verbose` and a test
encode come before any listening.

#### `--sbr-freqscale <n>`

**Chooses how the replicated band is divided into SBR bands: linearly or on one of three
logarithmic grids.**

- Applies to: SBR profiles only
- Values: `0` = linear, `1`, `2`, `3` = logarithmic with 12, 10 and 8 bands per octave
  (progressively coarser); `-1` or leaving the switch out = the value from the SBR tuning table
- Input: whole number

SBR describes its region band by band, and this picks the ruler. A linear grid gives every
band the same width in Hz, so the top octave gets many bands and the bottom of the SBR
region gets few. A logarithmic grid spaces bands by ratio, which matches hearing: more
resolution where the ear resolves more, down near the crossover.

Values:
- `0` linear. Even spacing in Hz. Fine resolution high up, coarse just above the
  crossover, which is the opposite of what the ear wants. Occasionally useful on material
  whose high band is genuinely uniform, such as broadband noise beds.
- `1` finest logarithmic grouping, 12 bands per octave. Most bands, most parameters, best
  shaping of an uneven high band.
- `2` middle logarithmic grouping, 10 bands per octave.
- `3` coarsest logarithmic grouping, 8 bands per octave. Fewest bands, cheapest description,
  least control.

Measured at HE-AAC 64 kbps, SBR payload per frame: 220 bits at `0`, 188 at `1`, 170 at `2`,
152 at `3`.
- `-1` off: the tuning table decides, which is the right choice unless you have a reason.

Example in context: `fdkaac -p 5 -b 64000 --sbr-freqscale 1 -o out.m4a in.wav` (finest grid)

Works with / against: the number of bands this produces interacts with
`--sbr-noise-bands` (noise bands are derived per octave, not per SBR band) and with how
much the invf decision can vary across the region. A finer grid means a finer invf map.

#### `--sbr-alterscale <n>`

**Switches to the alternative band grouping resolution, a secondary adjustment to the grid
chosen by `--sbr-freqscale`.**

- Applies to: SBR profiles only
- Values: `0` = off, `1` = on; default `-1` = the value from the SBR tuning table
- Input: whole number

This is a standard SBR header flag that slightly changes how the master band table is
derived, which in practice moves a few band edges. It is a refinement, not a character
control: there is no listening result here worth promising in either position, and nothing
in our measurements isolates it.

Values:
- `0` the alternative resolution off.
- `1` the alternative resolution on.
- `-1` off: follow the tuning table. Recommended.

Example in context: `fdkaac -p 5 -b 64000 --sbr-freqscale 2 --sbr-alterscale 1 -o out.m4a in.wav`

Works with / against: only meaningful together with `--sbr-freqscale`, since it adjusts
that grid. If you are hunting for a tonal change up top, `--sbr-invf*` and
`--sbr-noise-bands` are the switches with real leverage; reach for this one last.

#### `--sbr-noise-bands <n>`

**Sets how finely the SBR noise floor is described across frequency.**

- Applies to: SBR profiles only
- Values: `0`, `1`, `2`, `3`; default `-1` = the value from the SBR tuning table
- Input: whole number

The noise floor is how SBR puts "air" back into a copied band. With one noise band for the
whole region, the encoder sends a single noise level and the decoder spreads it evenly:
cheap, and wrong whenever the high band is not uniform, for example a sustained string
note under a hi-hat. With more noise bands, the level can follow the material, so a quiet
tonal region does not get the same hiss as a busy one.

The field in the SBR header is two bits wide, so there are exactly four settings and no
more. The values are *not* a literal band count: they are a density, roughly bands per
octave.

Values:
- `0` one noise band for the entire SBR region. Cheapest, bluntest. At very low bitrate
  the saving is real; on anything with structure up top you hear noise in the wrong places.
- `1` about one noise band per octave.
- `2` about two per octave.
- `3` about three per octave. Densest description, best separation of tonal and noisy
  regions, most side information.
- `-1` off: the tuning table decides by bitrate.

Example in context: `fdkaac -p 5 -b 96000 --sbr-noise-bands 3 -o out.m4a in.wav`

Works with / against: this decides the *resolution* of the noise floor;
`--sbr-noise-max`, `--sbr-nf-weight` and `--sbr-noise-floor-offset` decide its *level*.
Raising the density is wasted if the level is clipped at a low ceiling, so if you are
chasing air, look at `--sbr-noise-max` first.

#### `--sbr-amp-res <n>`

**Chooses the quantisation step of the SBR envelope levels: 1.5 dB or 3.0 dB.**

- Applies to: SBR profiles only
- Values: `0` = 1.5 dB, `1` = 3.0 dB; default `-1` = the value from the SBR tuning table
- Input: whole number

Envelope levels are sent as quantised numbers, and this is the step size. A 1.5 dB step
tracks level changes in the high band more exactly and costs more bits per envelope. A
3.0 dB step is cheaper and coarser; on material with strong level movement up top, the
coarse step is audible as the high band stepping rather than gliding.

Values:
- `0` 1.5 dB steps. Finer, more expensive. Prefer at higher bitrate or on dynamic
  material.
- `1` 3.0 dB steps. Coarser, cheaper. The sensible trade when bits are scarce, which is
  why the table often picks it low down.
- `-1` off: the tuning table decides.

Example in context: `fdkaac -p 5 -b 80000 --sbr-amp-res 0 -o out.m4a in.wav`

Works with / against: interacts directly with `--sbr-num-env` and the transient detector,
because every extra envelope costs a full set of levels at whatever step size is in force.
Four envelopes at 1.5 dB resolution is the expensive corner of this group.

#### `--sbr-data-extra <n>`

**Writes the optional extra SBR header fields instead of omitting them.**

- Applies to: SBR profiles only
- Values: `0` = do not write, `1` = write; default `-1` = the value from the SBR tuning
  table
- Input: whole number

The SBR header has an optional block carrying additional configuration. Writing it makes
every header slightly larger and the configuration more explicit; omitting it leaves the
decoder on default assumptions for those fields. This is a bitstream formatting choice,
not a sound control, and it is only worth touching when you are chasing compatibility with
a specific decoder or inspecting headers.

Values:
- `0` do not write the extra header data.
- `1` write it.
- `-1` off: follow the tuning table.

Example in context: `fdkaac -p 5 -b 64000 --sbr-data-extra 1 -o out.m4a in.wav`

Works with / against: combined with a short `--sbr-header-period` the extra bytes repeat
often, so if you are optimising a low bitrate stream for fast SBR lock, keep this off.

### SBR time resolution and stereo: `--sbr-num-env` `--sbr-freqres-fixfix` `--sbr-stereo-mode` `--sbr-header-period` `--sbr-header-time`

Frequency layout decides where the SBR band is. This section decides how often it is
described, how it is split between channels, and how often the configuration itself is
repeated in the stream.

Time resolution is a direct bits versus smearing trade. One envelope per frame means one
level per band for the whole frame, so a hi-hat inside that frame is averaged with the
silence around it. Four envelopes track the shape but quadruple the envelope data. The
transient detector normally makes this decision per frame, which is why forcing a fixed
count is a bigger intervention than it looks.

The header period is a different kind of setting: it does not change how the file sounds
once playback is running, it changes how long a decoder that joins mid stream waits before
it has any high band at all. It matters for live streaming and for nothing else.

#### `--sbr-num-env <1|2|4>`

**Forces a fixed number of SBR envelopes per frame and switches off the transient
detector's framing decision.**

- Applies to: SBR profiles only
- Values: `1`, `2`, `4`; default `-1` = off, the transient detector decides per frame
- Input: whole number

Normally SBR uses a variable time grid: frames with a detected attack get more, shorter
envelopes so the attack is not smeared, and steady frames get one. This switch replaces
that with a static grid, every frame the same, regardless of content.

That makes it a good tool for stable material and a bad one for percussive material. On a
sustained pad, organ or string bed, forcing 2 or 4 envelopes buys you time resolution the
detector would not have asked for, and the cost is predictable. On drums, the static grid
means attacks no longer get their own envelope, and you get exactly the backwards smearing
the detector exists to prevent.

The value 8 would exceed the standard's grid and is rejected.

Values:
- `1` one envelope per frame. Cheapest, bluntest; the high band holds one level per band
  for the whole frame. Expect audible smearing on anything transient.
- `2` two envelopes per frame. A middle ground, and a reasonable forced setting on steady
  material at low bitrate.
- `4` four envelopes per frame. Finest static resolution, most envelope data. Good on
  sustained, ringing sounds; on sharp attacks it still loses to the dynamic grid, because
  the splits are in fixed places rather than at the attack.
- `-1` off. The transient detector picks per frame. This is the better default, and the
  knobs to shape it are in the transient detector section below.

Example in context: `fdkaac -p 5 -b 64000 --sbr-num-env 4 -o out.m4a organ.wav`

Works with / against: this overrides the framing part of what `--sbr-tran-thr` and
`--sbr-tran-split` influence, so combining them is contradictory: if you want better
behaviour on attacks, leave `--sbr-num-env` off and lower `--sbr-tran-thr` instead. The
bit cost per envelope is set by `--sbr-amp-res`.

#### `--sbr-freqres-fixfix <0|1>`

**Sets the frequency resolution of the FIXFIX envelope, the envelope type used in frames
with no detected transient.**

- Applies to: SBR profiles only
- Values: `0` = low, `1` = high; default `-1` = off
- Input: whole number

An envelope can describe the high band with few bands or with many. FIXFIX is the envelope
shape used for ordinary, transient free frames, which on most material is the large
majority of them, so this quietly affects most of the file.

Low resolution groups bands together: fewer levels per envelope, cheaper, and less able to
follow an uneven high band. High resolution sends more levels, tracks spectral shape
better, and costs bits that at low bitrate come out of the core.

Values:
- `0` low frequency resolution. Cheaper; the high band is described more coarsely across
  frequency. Sensible at low bitrate and on uniform high bands.
- `1` high frequency resolution. More levels per envelope, better shaping of structured
  high bands. Sensible when bits are available.
- `-1` off: the encoder's own choice, which follows the tuning table and the frame type.

Example in context: `fdkaac -p 5 -b 96000 --sbr-freqres-fixfix 1 -o out.m4a in.wav`

Works with / against: this is the frequency axis; `--sbr-num-env` is the time axis. Both
multiply into the same envelope data budget, and `--sbr-amp-res` sets the price per level.
It applies only to transient free frames, so on heavily percussive material its share of
the file is smaller.

#### `--sbr-stereo-mode <1..3>`

**Chooses how the SBR high band is shared between the two channels, independently of the
core's stereo coding.**

- Applies to: SBR profiles only, stereo input
- Values: `1` LR, `2` coupling, `3` switch-LRC; default `-1` = off (the tuning table
  decides: `3` below 60 kbps stereo, `1` from 60 kbps)
- Input: whole number

The core and SBR make separate stereo decisions. The core may be coding mid/side while SBR
is coupling, or the reverse. This switch addresses only the high band.

Full left/right separation sends an independent set of SBR parameters per channel: the
widest, most faithful top end, at roughly double the SBR side information. Coupling sends
one shared envelope plus level and balance information, which is much cheaper and pulls
the image up top towards the centre. On most material that is less audible than one would
expect, which is why it is the normal choice at low bitrate; on wide, hard panned cymbals
and on recordings with real room difference between channels it is clearly audible.

Values:
- `1` LR, full separation. Independent high band per channel. What FDK uses from 60 kbps
  stereo; at 32 kbps it is expensive.
- `2` coupling. Shared envelope plus level and balance, every frame. Cheap, narrower top.
- `3` switch-LRC. The encoder chooses between LR and coupling per frame. What FDK uses
  below 60 kbps stereo.
- `-1` off: the tuning table decides, as listed above.

Mono input needs no switch: FDK codes a single channel high band by itself. A mono mode
for stereo input does not exist; `0` is rejected.

Example in context: `fdkaac -p 5 -b 48000 --sbr-stereo-mode 1 -o out.m4a in.wav`, full
separation below the 60 kbps line.

Works with / against: in `-p 29` (HE-AAC v2) parametric stereo is describing the image
and the SBR part is mono, so this switch has nothing to separate there. Core stereo
switches in group B are a separate decision and do not override this one.

Measured: HE-AAC 48 kHz stereo, the stream without the switch is byte identical to `3` at
24, 32, 40, 48, 56 and 59 kbps and to `1` at 60, 64 and 96 kbps. Before beta.2 `0` was
accepted and crashed the encoder on stereo input, with no message and an empty file.

#### `--sbr-header-period <n>`

**Sets how many frames apart SBR headers are written, which decides how long a decoder
joining a live stream hears core only audio before the high band appears.**

- Applies to: SBR profiles only. In HE-AAC and HE-AAC v2 it controls the in-band
  `sbr_header`; in AAC-ELD with SBR there is no in-band SBR header, so the period applies
  to the LATM/LOAS StreamMuxConfig and therefore needs `-f 6`, `-f 7` or `-f 10`
- Values: frames, `1` and up; default `-1` = off, FDK's own period of about 10 frames
  (about 0.46 s at 44.1 kHz)
- Input: whole number

The entire SBR configuration lives in a periodic header, not in every frame. A listener
who tunes into an Icecast, Shoutcast or HLS stream mid stream decodes the core immediately
and hears a muffled, lowpassed version of the programme until the next SBR header arrives.
Then the top end appears. On a file this is invisible, since playback starts at the
beginning; on a live stream it is the first thing a new listener hears.

Writing the header in every frame removes that moment almost entirely, at the cost of the
header bytes in every frame. At low bitrate those bytes are not nothing. Stretching the
period saves them and lengthens the core only moment.

The value is taken literally, with no one second ceiling applied.

Examples, shortening the period:
- Mild: `--sbr-header-period 5`. Roughly halves the wait against the default.
- Strong: `--sbr-header-period 2`. A header every other frame; the core only moment is
  down to tens of milliseconds.
- Exaggerated: `--sbr-header-period 1`. A header in every single frame, near instant SBR
  lock, maximum overhead. On a 24 kbps stream that overhead is a real share of the budget,
  which is why it is a caricature there and a reasonable choice at 96 kbps.

Examples, lengthening the period:
- Mild: `--sbr-header-period 25`. About a second between headers at 44.1 kHz. Barely
  noticeable on a stream people stay tuned to.
- Strong: `--sbr-header-period 100`. Several seconds of core only audio for a new
  listener. Saves header bytes, sounds broken to anyone channel hopping.
- Exaggerated: `--sbr-header-period 5000`. Minutes between headers. For a file this is
  harmless; for a stream it means most listeners never hear the high band at all.

Works with / against: `--sbr-header-time` expresses the same thing in seconds and wins
when both are given. `--sbr-data-extra 1` makes each header bigger, so the two together
multiply the overhead. `--verbose` prints the effective period in frames and milliseconds.

#### `--sbr-header-time <s>`

**The same setting as `--sbr-header-period`, expressed in seconds instead of frames.**

- Applies to: SBR profiles only; in AAC-ELD with SBR it sets the LATM/LOAS
  StreamMuxConfig period, so use `-f 6`, `-f 7` or `-f 10`
- Values: `0.01..600` seconds; default `-1` = off
- Input: decimal, up to 3 places, `.` or `,`

Frames are an awkward unit when a stream operator specifies a requirement, because the
frame duration depends on the sample rate and the profile. This switch takes the seconds
directly and converts them for you, so the same command line behaves the same way at
44.1 and 48 kHz.

It wins over `--sbr-header-period` when both are given.

With AAC-ELD in ADTS or M4A there is nothing to repeat: the configuration sits only in the
container, and `--verbose` reports the SBR header period as the LATM/LOAS config period
rather than as an in-band SBR header.

Examples, shortening the time:
- Mild: `--sbr-header-time 0.25`. A quarter second worst case wait.
- Strong: `--sbr-header-time 0.05`. Effectively immediate lock for a listener joining at
  random.
- Exaggerated: `--sbr-header-time 0.01`, the minimum. At most sample rates this is shorter
  than a frame, so it amounts to a header in every frame with the maximum overhead that
  implies.

Examples, lengthening the time:
- Mild: `--sbr-header-time 1`. One second, close to a common operator requirement.
- Strong: `--sbr-header-time 3.5`. Three and a half seconds of core only audio for a new
  listener. Measured on the decoder side with this setting, the longest observed wait for
  the high band after joining mid stream was 3.41 s for HE-AAC, 3.46 s for HE-AAC v2 and
  3.52 s for AAC-ELD.
- Exaggerated: `--sbr-header-time 600`, the maximum, ten minutes. Legal, and for a live
  stream indistinguishable from shipping a core only stream.

Works with / against: overrides `--sbr-header-period`. Check the result in `--verbose`,
which prints the effective period both ways. Both `.` and `,` are accepted as the decimal
separator.

### SBR transient detector: `--sbr-tran-peak` `--sbr-tran-quiet` `--sbr-tran-dom` `--sbr-tran-thr` `--sbr-tran-split`

SBR divides each frame into time slots and decides, from the energy in those slots,
whether something in the high band attacked. If it says yes, the attack gets its own short
envelope and the level jump is coded where it happens. If it says no, one envelope covers
the whole stretch and the energy of the hit is averaged across it.

Averaging across an attack is audible, and it is audible *before* the attack, not after:
you hear a short burst of high frequency hiss just ahead of the hi-hat or the clap. That
is pre-echo, and in the replicated band it is one of the more recognisable coding
artefacts, because the material up there is already synthetic enough that a smear reads as
noise rather than as a soft attack.

Five knobs reach into this detector. One of them, `--sbr-tran-thr`, is far more effective
than the rest and is the one to try first. Two of them, `--sbr-tran-quiet` and
`--sbr-tran-dom`, live in a second, faster detector that only runs for AAC-ELD with SBR,
so they do nothing in HE-AAC and HE-AAC v2.

All five are x100 knobs and all of them work in both directions: `100` means the stock
constant exactly, below 100 makes the constant smaller, above 100 makes it larger.
`--sbr-tran-peak` is the exception in meaning, not in format: it replaces the constant
rather than scaling it.

#### `--sbr-tran-thr <n>`

**Scales the master threshold that decides how readily the SBR transient detector fires at
all, and it is the one knob in this section with a large, measured effect.**

- Applies to: SBR profiles (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

The detector compares slot energies against a threshold derived from the signal. Lower the
threshold and marginal attacks that previously went unnoticed now get their own envelope;
raise it and only the most obvious hits do. Because missing an attack is what produces
pre-echo, lowering this is the direct cure, and the cost is envelope data: more detected
transients means more envelopes means more bits, taken from the core.

This is the knob that justifies the group. On percussive material it removes high band
pre-echo almost entirely.

Measured, pre-echo in the SBR band. Test signal: 24 percussive attacks of hi-hat and clap
character plus four decaying tones between 8 and 13 kHz. Metric: energy above 9 kHz
appearing in the two 5.8 ms windows before each attack that is absent from the source, so
lower is better. Encoded as HE-AAC, decoded, lag aligned by cross correlation first:

| `--sbr-tran-thr` | pre-echo at 32 kbps | pre-echo at 64 kbps |
|---|---|---|
| stock | 1.610 | 1.186 |
| 20 / 40 / 60 | 0.041 | 0.059 |
| 80 | 0.661 | 0.502 |
| 100 and above | 1.610 (= stock) | 1.186 (= stock) |

The effect is monotonic, reproducible at both bitrates and saturated below 60. Values of
150 and 400 do change the stream, in the other direction.

Examples, lowering the threshold (detector fires more readily):
- Mild: `--sbr-tran-thr 80`. Pre-echo roughly cut in half on the measured signal, modest
  extra envelope cost.
- Strong: `--sbr-tran-thr 40`. Pre-echo essentially gone on percussive material. This is
  the recommended starting point when you hear smearing up top.
- Exaggerated: `--sbr-tran-thr 1`. The detector fires on almost everything. Below 60 there
  is nothing further to gain in pre-echo, so all you buy is envelope data spent on
  non events, which at low bitrate takes bits off the core and makes the whole file worse.

Examples, raising the threshold (detector fires less readily):
- Mild: `--sbr-tran-thr 150`. Marginal attacks are ignored, slightly fewer envelopes,
  slightly more smearing. Occasionally worth it on steady material at a very tight budget.
- Strong: `--sbr-tran-thr 400`. Only unmistakable attacks get their own envelope. Expect
  clearly more pre-echo on drums.
- Exaggerated: `--sbr-tran-thr 10000`. The detector effectively never fires; every frame is
  described with the static grid. A caricature, and a useful one if you want to hear the
  artefact this detector exists to prevent.

Works with / against: `--sbr-num-env` forces a static grid and so removes the framing
benefit this knob buys, which makes combining them self defeating.
`--sbr-tran-split` handles the separate question of splitting frames that have no detected
transient at all. For pre-echo in the *core* (LC, LD, ELD), the tools are in group E.

#### `--sbr-tran-split <n>`

**Scales the threshold for splitting a frame that has no detected transient into two
envelopes anyway.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

Not every level change in the high band is an attack. A crescendo, a swell, a chord change
or a cymbal decaying unevenly all move the high band without triggering the transient
detector, and one envelope for the whole frame then averages the movement away. This
threshold decides how readily such a frame is still split in two, giving the second half
its own levels.

Lowering it splits more often: finer time resolution on material that moves but does not
attack, at the cost of a second set of envelope levels in many frames. Raising it splits
less often: cheaper, and the high band follows slow movement less closely.

This is a smaller effect than `--sbr-tran-thr`, and it addresses a different symptom. If
the problem is hiss before a hit, use the threshold; if the problem is a high band that
sounds static while the music swells, use this.

Examples, lowering (split more often):
- Mild: `--sbr-tran-split 70`. Slightly more frames get two envelopes.
- Strong: `--sbr-tran-split 30`. Most frames with any movement get split. Noticeably more
  envelope data, better tracking of swells.
- Exaggerated: `--sbr-tran-split 1`. Nearly every frame split regardless of content; the
  bit cost is real and the benefit on steady material is zero.

Examples, raising (split less often):
- Mild: `--sbr-tran-split 150`. Marginally fewer splits, marginally cheaper.
- Strong: `--sbr-tran-split 300`. Splits become rare. Measured: this does change the
  HE-AAC stream.
- Exaggerated: `--sbr-tran-split 10000`. No frame is ever split on movement alone. One
  envelope per non transient frame, maximum smearing of slow high band changes, minimum
  envelope data.

Works with / against: ignored in effect when `--sbr-num-env` forces a static grid.
Complements `--sbr-tran-thr` rather than duplicating it: that one is about attacks, this
one about frames with no attack. Every extra envelope is priced by `--sbr-amp-res`.

#### `--sbr-tran-peak <n>`

**Sets how peaky a candidate must be before it counts as an attack: the signal must fall
below this fraction of the previous slot.**

- Applies to: SBR profiles
- Values: `1..200`, raw x100 where `90` = the stock 0.90; default `-1` = off
- Input: whole number

A real attack is not just loud, it is brief: the energy rises and then drops again. The
detector checks that drop, requiring the following slot to fall below a fraction of the
previous one. That fraction is this constant.

Note the unit carefully. This is a **raw** x100 value, not a scale factor: it replaces the
constant outright, so `90` is the stock 0.90 and is a no-op, while `100` is 1.0, not
"stock". And 1.0 has a special consequence: a fraction of 1.0 or more means the condition
can never be satisfied by a genuine drop in the way the detector tests it, so a slot never
counts as dropping. Every value from 100 to 200 therefore produces one identical stream.
The live range is 1..99.

Lowering the value demands a sharper drop, so fewer candidates qualify as attacks.
Raising it toward 99 accepts gentler decays as attacks, so more do.

Examples, lowering (stricter, fewer transients):
- Mild: `--sbr-tran-peak 80`. Slightly stricter than stock; soft attacks stop qualifying.
- Strong: `--sbr-tran-peak 50`. Only sharply decaying hits count. Expect more smearing on
  anything but dry percussion.
- Exaggerated: `--sbr-tran-peak 1`. The drop requirement is absurd; almost nothing
  qualifies as an attack and the peak test alone suppresses the detector.

Examples, raising (looser, more transients):
- Mild: `--sbr-tran-peak 95`. Gentler decays now count as attacks.
- Strong: `--sbr-tran-peak 99`. The loosest setting that still does anything. Attacks with
  long tails get their own envelopes.
- Exaggerated: `--sbr-tran-peak 200`. Beyond the useful range: 100 and above all mean 1.0,
  a slot never counts as dropping, and 100 through 200 are one and the same stream. Useful
  only as a demonstration that the knob has a hard edge.

Works with / against: this is one condition of several; `--sbr-tran-thr` scales the master
threshold and has far more leverage. If you want more or fewer transients overall, start
there and use this only to change the *character* of what qualifies.

#### `--sbr-tran-quiet <n>`

**Sets the quiet slot energy floor of the fast SBR transient detector, which decides how
readily quiet passages produce transients.**

- Applies to: AAC-ELD with SBR only (`-p 39 -L 1`). No effect in HE-AAC or HE-AAC v2,
  which use the other detector
- Values: `1..10000`, x100 where `100` = stock; default `-1` = off
- Input: whole number

AAC-ELD with SBR runs a second, lower latency transient detector, and that detector has an
energy floor below which a slot is treated as quiet. The floor exists so that noise in
silence does not register as attacks. Lower it and the detector looks into quieter
passages, so soft hits in a sparse arrangement can get their own envelope. Raise it and
quiet material is ignored.

Behaviour is strongly material dependent, which is worth knowing before you spend time on
it: on a purely tonal test signal the detector never reached a decision this knob could
change.

Measured at AAC-ELD with SBR, 24 kbps, across five test signals: values from 30 to 99
change the stream, `100` and above gave no change at all, and `1` and `10` give the same
stream as each other. So the live range here is roughly 11..99 downward, with a floor
effect below that and a ceiling at 100.

Examples, lowering (quiet passages produce transients more readily):
- Mild: `--sbr-tran-quiet 90`. A small extension into quieter slots.
- Strong: `--sbr-tran-quiet 50`. Clearly more sensitive in sparse material; measured to
  change the stream.
- Exaggerated: `--sbr-tran-quiet 1`. Floored: `1` and `10` give the same stream, so the
  detector has hit the bottom of what the floor can express.

Examples, raising (quiet passages ignored):
- Mild through exaggerated: `--sbr-tran-quiet 120`, `300`, `10000` all gave no stream
  change in our measurements at ELD plus SBR 24 kbps. The stock floor is already high
  enough that raising it further changes no decision on this material. Honest status:
  the direction is wired, the effect was not observed upward.

Works with / against: nothing in HE-AAC or HE-AAC v2, because the fast detector is not
used there. Within ELD plus SBR it sits alongside `--sbr-tran-dom`; `--sbr-tran-thr` and
`--sbr-tran-split` apply to the main detector.

#### `--sbr-tran-dom <n>`

**Sets how much louder a slot must be than the two before it when those are already marked
as transients.**

- Applies to: AAC-ELD with SBR only (`-p 39 -L 1`). No effect in HE-AAC or HE-AAC v2
- Values: `100..500`, x100 where `140` = the stock 1.4; default `-1` = off
- Input: whole number

A roll, a flam or a double hit produces several candidate slots in a row. This rule stops
the detector from marking all of them: once the two preceding slots are already transients,
the next one has to be clearly louder, by this factor, to be marked as well. It is a
dominance test, which is where the name comes from.

Lowering the factor toward 100 lets consecutive slots all count, so dense percussion
produces more transients. Raising it means only a decisive escalation counts, so a roll
registers once.

Honest status: this knob is wired to the constant it documents, and in our measurements it
produced no stream change at all. Values from 100 to 500 gave identical output on five test
signals at ELD plus SBR. The reason is most likely that the surrounding conditions are
rarely all satisfied on this material, so the dominance test is not what decides. Treat it
as available for experiments, not as a tool with a predictable result, and reach for
`--sbr-tran-thr` instead.

Examples, lowering (consecutive slots count more readily):
- Mild: `--sbr-tran-dom 130`. Slightly below stock.
- Strong: `--sbr-tran-dom 110`. Nearly no dominance requirement.
- Exaggerated: `--sbr-tran-dom 100`. The factor is 1.0, so a slot merely needs to match the
  two before it. No stream change observed.

Examples, raising (only decisive escalation counts):
- Mild: `--sbr-tran-dom 180`. Above stock.
- Strong: `--sbr-tran-dom 300`. A slot must be three times the level of its predecessors.
- Exaggerated: `--sbr-tran-dom 500`, the maximum. No stream change observed at any of these.

Works with / against: same ELD plus SBR fast detector as `--sbr-tran-quiet`, and the same
limitation: nothing in HE-AAC. If you want the transient behaviour in that detector to
change audibly, `--sbr-tran-quiet` in the 30..99 range is the one with measured effect.

### Missing harmonics detector: `--sbr-mh-tone` `--sbr-mh-diff` `--sbr-mh-decay-orig` `--sbr-mh-decay-diff` `--sbr-mh-sfm-sbr` `--sbr-mh-sfm-orig` `--sbr-mh-maxcomp` `--sbr-mh-deltatime`

The transient detector deals with time. This detector deals with a problem created by the
copying itself.

SBR builds the high band by patching lower spectrum upward. A tone that existed in the
original high band is not guaranteed to survive that: the patch brings up whatever happened
to be in the source region, so a real partial can land at the wrong frequency, arrive
accompanied by partials that were never there, or simply vanish. On a glockenspiel, a
triangle, a bell or a bright string harmonic this is exactly the content you notice. The
missing harmonics detector compares the tonality of the original band with the tonality of
the patched band, decides that a tone is missing, and signals the decoder to insert a
synthetic sine in that band.

Both failure directions are audible. Too eager and you get whistling: steady, pitched tones
up top that were never in the recording, often slightly off from the real harmonic series,
which is the classic "SBR warble". Too conservative and bells and cymbals go dull as their
partials quietly disappear.

Eight knobs reach into this detector. Be aware of their real status before spending time
here: on our test material only two of them moved the stream at all. `--sbr-mh-tone`
changed the output on a tonal signal at HE-AAC 24 kbps at values 1, 5 and 10, and
`--sbr-mh-diff` at value 10. The other six produced no stream change at HE-AAC 24 to
96 kbps on tonal and mixed material, in either direction. That is a property of this
detector on this material, not of the scaling: the x100 knobs here work in both directions
and `100` is exactly stock. The detector simply needs several conditions to coincide before
any of them decides the outcome, and on ordinary music it rarely gets there. Each card
below says plainly where it stands.

#### `--sbr-mh-tone <n>`

**Scales how tonal a band must be before SBR fabricates a synthetic harmonic in it, and it
is the main control in this section.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

The detector measures tonality in the original band and compares it against a threshold.
Scaling that threshold down makes more bands qualify as tonal, so more synthetic harmonics
get added. Scaling it up makes fewer qualify.

What you hear in each direction is specific. More added tones means more of a bell's or
cymbal's pitched content is restored, and more risk of whistling artefacts that were never
in the source. Fewer added tones means a safer, duller top, with bright percussion losing
its ring.

Measured: this is one of the two knobs in this section that moved the stream, on a tonal
test signal at HE-AAC 24 kbps, at values 1, 5 and 10. Those are all far below stock, which
tells you how low the threshold has to go before the detector changes its mind on that
material.

Examples, lowering the threshold (more synthetic harmonics):
- Mild: `--sbr-mh-tone 70`. A modest widening of what counts as tonal. On much material
  this will not change the stream at all.
- Strong: `--sbr-mh-tone 10`. Measured to change the stream on tonal material. Expect more
  added tones and a brighter, more pitched high band.
- Exaggerated: `--sbr-mh-tone 1`. Almost any band counts as tonal. Measured to change the
  stream, and the direction to use if you want to hear what whistling artefacts sound like.

Examples, raising the threshold (fewer synthetic harmonics):
- Mild: `--sbr-mh-tone 150`. Slightly stricter.
- Strong: `--sbr-mh-tone 400`. Only strongly tonal bands get a harmonic.
- Exaggerated: `--sbr-mh-tone 10000`. The detector effectively never adds a harmonic. No
  stream change was observed above 100 on our material, so if you want this result,
  verify it on yours before relying on it.

Works with / against: `--sbr-mh-diff` is the other trigger path into the same decision, and
`--sbr-mh-sfm-orig` / `--sbr-mh-sfm-sbr` move the tone versus noise line that feeds the
tonality measurement. `--sbr-mh-maxcomp` controls what happens around a harmonic once it is
added, not whether it is added.

#### `--sbr-mh-diff <n>`

**Scales the original versus patched tonality difference that triggers harmonic
compensation.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

`--sbr-mh-tone` asks "is the original band tonal enough". This asks a different question:
"did the patching change the tonality". It is the path that catches the case where the
original had one clean tone and the patched band has several, or the reverse, which is the
specific damage copying does.

Lowering the threshold makes a smaller difference count as damage, so compensation triggers
more often. Raising it means only a large mismatch counts.

Measured: this is the second of the two knobs in this section that moved the stream, at
value 10 on our material. Values nearer stock, and values above stock, produced no change.

Examples, lowering (compensate on smaller mismatches):
- Mild: `--sbr-mh-diff 70`. Modest; may well change nothing on your material.
- Strong: `--sbr-mh-diff 20`. Close to the value where we saw a change.
- Exaggerated: `--sbr-mh-diff 10`. Measured to change the stream. Almost any tonality
  mismatch between original and patch triggers compensation.

Examples, raising (compensate only on large mismatches):
- Mild: `--sbr-mh-diff 150`.
- Strong: `--sbr-mh-diff 400`.
- Exaggerated: `--sbr-mh-diff 10000`. The difference path effectively never fires, leaving
  `--sbr-mh-tone` as the only trigger. No stream change observed upward on our material.

Works with / against: the two trigger paths, this one and `--sbr-mh-tone`, feed the same
decision, so raising one while lowering the other partly cancels. The flatness knobs
`--sbr-mh-sfm-sbr` and `--sbr-mh-sfm-orig` decide what counts as tonal in the first place
on each side of the comparison.

#### `--sbr-mh-decay-orig <n>`

**Scales how long an already detected tone keeps being tracked in the original band as it
decays.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

A detector that only looked at the current frame would drop a tone as soon as it faded
below threshold, which on a bell or cymbal tail is exactly when you still want it. So the
detector keeps tracking a tone it has already found, with a decay. This scales the length
of that memory.

Longer tracking means synthetic harmonics persist into tails, so decays stay pitched
instead of turning into noise. Shorter tracking means tones are dropped sooner, so tails
lose their ring earlier.

Honest status: wired, no effect observed. On tonal and mixed material at HE-AAC 24 to
96 kbps this produced no stream change in either direction. The decay only matters once the
detector has found a tone to track, and the trigger conditions rarely coincided on our
material.

Examples, shortening the tracking:
- Mild: `--sbr-mh-decay-orig 70`.
- Strong: `--sbr-mh-decay-orig 30`.
- Exaggerated: `--sbr-mh-decay-orig 1`. Effectively no memory: a tone is forgotten as soon
  as it drops. No stream change observed.

Examples, lengthening the tracking:
- Mild: `--sbr-mh-decay-orig 150`.
- Strong: `--sbr-mh-decay-orig 400`.
- Exaggerated: `--sbr-mh-decay-orig 10000`. A tone, once found, is tracked almost
  indefinitely. No stream change observed.

Works with / against: only relevant where the detector fires, so lowering `--sbr-mh-tone`
into its measured active range is the prerequisite for this knob mattering at all.
`--sbr-mh-decay-diff` is the same mechanism on the difference guide.

#### `--sbr-mh-decay-diff <n>`

**The same decay as `--sbr-mh-decay-orig`, applied to the difference guide instead of the
original band.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

The detector keeps two tracking guides: one following tones in the original band, one
following the original versus patch tonality difference. This scales the memory of the
second, so it governs how long a detected *mismatch* keeps being treated as present after
it stops being obvious.

Longer means compensation decisions persist across frames and are more stable. Shorter
means they are re-evaluated nearly from scratch each frame, which reacts faster and can
flicker.

Honest status: wired, no effect observed. No stream change on tonal or mixed material at
HE-AAC 24 to 96 kbps, in either direction.

Examples, shortening:
- Mild: `--sbr-mh-decay-diff 70`.
- Strong: `--sbr-mh-decay-diff 30`.
- Exaggerated: `--sbr-mh-decay-diff 1`. No memory on the difference guide. No stream change
  observed.

Examples, lengthening:
- Mild: `--sbr-mh-decay-diff 150`.
- Strong: `--sbr-mh-decay-diff 400`.
- Exaggerated: `--sbr-mh-decay-diff 10000`. The guide never forgets. No stream change
  observed.

Works with / against: pairs with `--sbr-mh-decay-orig`, and like it depends on
`--sbr-mh-diff` actually triggering before the memory length can matter.

#### `--sbr-mh-sfm-sbr <n>`

**Scales the spectral flatness threshold above which the patched band is judged noise like
rather than tonal.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

Spectral flatness measures how evenly energy is spread across a band: near 0 for a single
clean tone, near 1 for noise. The detector uses it to classify each band, and this knob
moves the line for the *patched* side of the comparison.

Lowering the threshold means the patched band is called noise like sooner, so the detector
more often concludes that the copy lost the tone and compensation is needed. Raising it
means the patch is given the benefit of the doubt as tonal, so fewer harmonics are added.

Honest status: wired, no effect observed. No stream change on tonal or mixed material at
HE-AAC 24 to 96 kbps in either direction.

One practical note if you measure this yourself: flatness has to be measured in the real
SBR region, from the core bandwidth up to the final top edge as printed by `--verbose`. A
window that runs to Nyquist includes the empty part above the top edge and returns a
flatness of 0, which looks like perfect tonality and is an artefact of the window.

Examples, lowering (patch called noise like sooner, more compensation):
- Mild: `--sbr-mh-sfm-sbr 70`.
- Strong: `--sbr-mh-sfm-sbr 30`.
- Exaggerated: `--sbr-mh-sfm-sbr 1`. Nearly any patched band counts as noise like. No
  stream change observed.

Examples, raising (patch called tonal for longer, less compensation):
- Mild: `--sbr-mh-sfm-sbr 150`.
- Strong: `--sbr-mh-sfm-sbr 400`.
- Exaggerated: `--sbr-mh-sfm-sbr 10000`. The patched band is effectively always tonal. No
  stream change observed.

Works with / against: works as a pair with `--sbr-mh-sfm-orig`; between them they catch
"one tone in the original became several after patching". Moving only one side shifts the
comparison asymmetrically, which is sometimes what you want and is harder to reason about.

#### `--sbr-mh-sfm-orig <n>`

**The same flatness threshold as `--sbr-mh-sfm-sbr`, for the original band.**

- Applies to: SBR profiles
- Values: `1..10000`, x100 where `100` = stock x1.00; default `-1` = off
- Input: whole number

This moves the tone versus noise line on the source side. Lowering it means the original
band is called noise like sooner, so the detector concludes less often that there was a
tone worth restoring. Raising it means the original is treated as tonal more readily, so
more bands become candidates for a synthetic harmonic.

Note that the two sfm knobs push the final decision in opposite senses despite being the
same kind of threshold, because one describes what you had and the other what you got. If
you want "add more harmonics", lower `--sbr-mh-sfm-sbr` and raise this one.

Honest status: wired, no effect observed. No stream change on tonal or mixed material at
HE-AAC 24 to 96 kbps in either direction.

Examples, lowering (original called noise like sooner, fewer harmonics):
- Mild: `--sbr-mh-sfm-orig 70`.
- Strong: `--sbr-mh-sfm-orig 30`.
- Exaggerated: `--sbr-mh-sfm-orig 1`. The source is almost never tonal, so almost nothing
  is judged missing. No stream change observed.

Examples, raising (original called tonal more readily, more harmonics):
- Mild: `--sbr-mh-sfm-orig 150`.
- Strong: `--sbr-mh-sfm-orig 400`.
- Exaggerated: `--sbr-mh-sfm-orig 10000`. Nearly every original band counts as tonal. No
  stream change observed.

Works with / against: the mirror of `--sbr-mh-sfm-sbr`, and both feed the tonality
comparison that `--sbr-mh-diff` thresholds. Changing all three at once makes the result
very hard to attribute; move one at a time.

#### `--sbr-mh-maxcomp <n>`

**Caps how much envelope compensation is applied around a synthetic harmonic.**

- Applies to: SBR profiles
- Values: `0..200`, a plain integer where `50` = stock; default `-1` = off
- Input: whole number

Inserting a sine into a band changes the energy of that band, so the envelope around it has
to be adjusted or the band ends up too loud. This caps that adjustment. It is not an x100
scale: the number is the limit itself.

A higher cap allows larger corrections, so neighbouring bands are pulled further and less
noise is left sitting next to the added tone: the harmonic stands out more cleanly. A lower
cap limits the correction, so the added tone sits in more of its original surrounding noise
and blends in, or becomes audibly too loud if the correction needed was larger than the cap.

Honest status: wired, no effect observed. No stream change on tonal or mixed material at
HE-AAC 24 to 96 kbps in either direction, which follows from the detector rarely adding a
harmonic on this material in the first place.

Examples, lowering the cap:
- Mild: `--sbr-mh-maxcomp 35`.
- Strong: `--sbr-mh-maxcomp 10`. Very little compensation allowed.
- Exaggerated: `--sbr-mh-maxcomp 0`. No compensation at all around an added harmonic. No
  stream change observed.

Examples, raising the cap:
- Mild: `--sbr-mh-maxcomp 70`.
- Strong: `--sbr-mh-maxcomp 120`.
- Exaggerated: `--sbr-mh-maxcomp 200`, the maximum. Correction is effectively unlimited. No
  stream change observed.

Works with / against: only applies where a harmonic is actually added, so
`--sbr-mh-tone` has to be in its active range first. The noise that sits next to the added
tone is also governed by `--sbr-noise-max` and the noise floor knobs, which act on every
band rather than just the harmonic's neighbours.

#### `--sbr-mh-deltatime <n>`

**Sets the maximum transient distance for a frame to count as a transient frame inside the
missing harmonics detector.**

- Applies to: SBR profiles
- Values: `0..64`, a plain integer; default `-1` = off, which leaves 9 for AAC and 16 for
  AAC-LD
- Input: whole number

The missing harmonics detector behaves differently in frames it considers transient,
because adding a steady synthetic sine across an attack is a bad idea. This sets how far,
in detector time units, a transient may be from the frame for the frame still to count as
transient. It is a plain integer, not an x100 scale, and the stock value differs by
profile.

A larger distance means more frames count as transient, so the detector takes its cautious
path more often and adds fewer harmonics in the neighbourhood of hits. A smaller distance
means fewer frames count as transient, so the detector adds harmonics closer to attacks,
which is where a synthetic sine is most likely to sound wrong.

Honest status: wired, no effect observed. No stream change on tonal or mixed material at
HE-AAC 24 to 96 kbps in either direction.

Examples, lowering the distance:
- Mild: `--sbr-mh-deltatime 6`.
- Strong: `--sbr-mh-deltatime 2`.
- Exaggerated: `--sbr-mh-deltatime 0`. No frame counts as transient on distance, so the
  cautious path is never taken. No stream change observed.

Examples, raising the distance:
- Mild: `--sbr-mh-deltatime 16`, which is the AAC-LD stock value applied to AAC.
- Strong: `--sbr-mh-deltatime 32`.
- Exaggerated: `--sbr-mh-deltatime 64`, the maximum. Almost every frame near any hit counts
  as transient. No stream change observed.

Works with / against: this reads the transient decision, so it is downstream of
`--sbr-tran-thr` and `--sbr-tran-peak`: changing what counts as a transient changes what
this knob is measuring distance to. It has no influence on framing itself.

### Inverse filtering and noise floor: `--sbr-invf` `--sbr-softness` `--sbr-invf-min` `--sbr-invf-max` `--sbr-invf-bias` `--sbr-invf-table` `--sbr-invf-hyst` `--sbr-invf-sbr-thr` `--sbr-invf-orig-thr` `--sbr-invf-smooth` `--sbr-nf-weight` `--sbr-nf-weight-from` `--sbr-nf-smooth` `--sbr-noise-max` `--sbr-noise-floor-offset`

This is where the character of the high band is actually decided, and it is the part of
SBR worth understanding properly.

The copied high band inherits the fine structure of the band it was copied from. That
structure is tonal: peaks and valleys at the wrong frequencies for where it has landed. The
result sounds metallic, ringing, sometimes chorused, and once you have heard it you cannot
unhear it. Inverse filtering is the cure: the decoder applies a whitening filter to the
patch, flattening that inherited structure. There are four strengths, 0 off, 1 low, 2 mid,
3 high, chosen per band and per frame. The noise floor is the other half of the same cure:
synthetic noise mixed into each band to replace the structure that whitening removed. Too
little of both and the top is metallic; too much and it is hissy and washed out, with
cymbals reduced to shaped noise.

The level is normally chosen by a **tonality estimator** that compares the original band
with the patched band, band by band and frame by frame. That dynamic decision is the thing
to preserve: it is right much more often than any fixed value you could pick.

So this section has two kinds of switch, and the priority between them matters:

1. `--sbr-invf` **forces** one fixed level for the whole file, in every band and every
   moment. It wins over everything else here. Use it to diagnose, or when you know the
   material is uniform.
2. Everything else leaves the decision dynamic and changes the estimator's rules: bounds,
   bias, decision table, hysteresis, smoothing, border positions, noise weight. The
   decision is still made per band and per frame, just along different rules.

Within group 2, `--sbr-softness` is a macro that sets several of those rules at once, and
**every explicit knob wins over the macro**. So `--sbr-softness 6 --sbr-invf-max 2` gives
you the macro's softness with your own ceiling.

Two measurement facts shape how you should use this section.

First, the softness macro deliberately does *not* move the tonality borders. Shifting them
was measured to make the per band decision flicker, 0.2 to 0.29 changes per band per frame
against 0.004 to 0.04 for stock, which is audible as instability. `--sbr-invf-sbr-thr` and
`--sbr-invf-orig-thr` expose that mechanism anyway, because sometimes you want it, but that
is why the macro avoids them.

Second, the noise floor weight is clipped by the noise ceiling. With the tuning table's
ceiling (0.125 at 24 to 48 kbps) about 95 percent of bands already sit at the ceiling, so
`--sbr-nf-weight` on its own frequently changes nothing at all. It needs a raised ceiling
*and* high invf bands to do anything: measured, it moved the stream only with
`--sbr-noise-max 6` plus either `--sbr-invf 3` or `--sbr-nf-weight-from 1`. Alone, or with
only one of those, nothing changed. Levels 5 to 7 of `--sbr-softness` raise the ceiling for
you.

At their neutral values (`--sbr-invf-hyst 1.0`, `--sbr-invf-bias 0`, `--sbr-invf-smooth 2`,
`--sbr-nf-weight 0.25`, `--sbr-nf-weight-from 3`, `--sbr-invf-table 0`, `--sbr-invf-min 0`,
`--sbr-invf-max 3`) these knobs give a stream byte identical to stock, as do
`--sbr-softness 2` and `--speech --sbr-softness 4`.

#### `--sbr-invf <0..3>`

**Forces one fixed inverse filtering level for the whole file, overriding the tonality
estimator completely.**

- Applies to: SBR profiles (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Values: `0` off, `1` low, `2` mid, `3` high; default `-1` = auto, the tonality estimator
  decides per band and per frame
- Input: whole number

Higher means more whitening of the patched band: less metallic ringing, less detail, a
flatter and more noise like top. Lower means the inherited tonal structure is left in
place: more apparent detail, more metallic character.

The drawback of forcing is exactly what it says: one level applies everywhere, including in
the bands and moments where the estimator would have chosen differently. A file with a
quiet tonal intro and a dense chorus wants different levels in those places, and a forced
value gives them the same one.

That said, this is the most useful knob here for two jobs. The first is diagnosis: encode
at `0` and at `3` and listen to the extremes, and you will immediately know which end of
the axis your material needs. The second is uniform material, where there is nothing for
the estimator to adapt to anyway.

Values:
- `0` inverse filtering off. No whitening at all. The patch keeps its inherited structure:
  maximum apparent detail, maximum metallic ringing. On tonal material this is clearly an
  artefact; on noise like material it can be the cleanest option.
- `1` low. A light whitening.
- `2` mid. Moderate whitening, which is roughly where the estimator sits on average with
  the music table.
- `3` high. Strong whitening. The top becomes flat and noise like; metallic ringing
  disappears and so does a good deal of perceived detail.
- `-1` auto. The estimator decides. The recommended setting unless you are diagnosing.

Example in context: `fdkaac -p 5 -b 64000 --sbr-invf 3 -o out.m4a in.wav`

Works with / against: this **wins over every other switch in this section**, including
`--sbr-softness` and the min/max/bias knobs, because there is no dynamic decision left to
bound. If you want to shape the decision rather than replace it, leave this off. Pairing
`--sbr-invf 3` with `--sbr-noise-max 6` is one of the two combinations that makes
`--sbr-nf-weight` do anything.

#### `--sbr-softness <0..7>`

**One macro from a hard, metallic top to a very soft one, setting several estimator rules
at once while keeping the decision dynamic.**

- Applies to: SBR profiles
- Values: `0..7`; default `-1` = off (stock, which is equivalent to level `2` for music)
- Input: whole number

This is the knob to try first. It moves you along the metallic versus hissy axis without
freezing the decision, by setting the decision table, the bias, the noise weight, the
min/max bounds and the noise ceiling in combinations that were measured to be monotonic and
stable. It deliberately does not touch the tonality borders, because doing that was measured
to make the per band decision flicker.

Values:
- `0` one step less whitening than the music tuning: harder, more metallic than stock.
- `1` never HIGH whitening. Note that on music the estimator rarely picks HIGH anyway, so
  on music material this bound often never binds: measured at HE-AAC 32 kbps, levels `1`
  and `2` gave a stream identical to stock on tonal, voice and mixed signals.
- `2` stock music estimators. Byte identical to not using the switch at all.
- `3` at least LOW whitening: the estimator may no longer choose off. Measured equal to
  `--sbr-invf 1` on a tonal signal, because there the estimator wanted off everywhere and
  the floor lifted it all to LOW.
- `4` stock speech estimators: the speech decision table and noise weight, without the rest
  of `--speech`, so the crossover is untouched.
- `5` as `4` plus a raised noise ceiling (0.5) and the weight extended to MID bands.
- `6` as `5` plus one more step of whitening.
- `7` very soft: ceiling 1.0, maximum whitening and fill noise. The top becomes
  unambiguously noise like.
- `-1` off.

Measured at HE-AAC 32 kbps, invf levels read back from the bitstream, flatness of the
decoded SBR band 7 to 14.5 kHz where 0 is tonal and 1 is noise like:

| `--sbr-softness` | mean invf level (speech / tonal / music) | SBR band flatness (speech / tonal) |
|---|---|---|
| 0 | 0.15 / 0.00 / 0.00 | 0.366 / 0.396 |
| 2 (stock music) | 1.12 / 0.98 / 0.99 | 0.373 / 0.430 |
| 4 (stock speech) | 1.93 / 1.96 / 1.98 | 0.379 / 0.440 |
| 5 | 1.93 / 1.96 / 1.98 | 0.403 / 0.518 |
| 7 | 2.93 / 2.96 / 2.98 | 0.417 / 0.521 |

The scale is monotonic on every sample, and per band mode changes stay at the stock rate
(0.004 to 0.04 per band per frame). Also measured: level `0` equals `--sbr-invf 0` on tonal
and mixed material, and levels `4` through `7` are distinct from each other.

Example in context: `fdkaac -p 5 -b 32000 --sbr-softness 6 -o out.m4a in.wav`

Works with / against: **every explicit knob below wins over this macro**, which is how you
combine them: `--sbr-softness 6 --sbr-invf-max 2` keeps the macro's noise handling with your
own ceiling. `--sbr-invf` wins over this entirely. Neighbouring levels can give the same
stream when your material never reaches the bound they differ in; that is expected, not a
failure.

#### `--sbr-invf-min <0..3>`

**Sets a floor under the estimator: the lowest inverse filtering level it is allowed to
pick.**

- Applies to: SBR profiles
- Values: `0..3`; default `-1` = off (no floor, equivalent to `0`)
- Input: whole number

The estimator keeps deciding per band and per frame; this just removes the bottom of its
range. It is the tool for "the top is metallic in the quiet passages", where the estimator
correctly sees a tonal band and chooses little or no whitening, and you disagree.

Raising the floor guarantees a minimum amount of whitening everywhere, which costs detail
in the bands that genuinely did not need it.

Values:
- `0` floor at off. No constraint; identical to not using the switch.
- `1` the estimator may never choose off. The gentlest useful floor, and the one worth
  trying when metallic ringing appears only occasionally.
- `2` at least MID whitening everywhere. A strong intervention: bands that wanted none now
  get moderate whitening.
- `3` at least HIGH everywhere, which means the level is pinned to HIGH since 3 is the
  maximum. That makes it equivalent in effect to `--sbr-invf 3`, and if that is what you
  want, say so directly.
- `-1` off.

Example in context: `fdkaac -p 5 -b 48000 --sbr-invf-min 1 -o out.m4a in.wav`

Works with / against: applied after `--sbr-invf-bias`, together with `--sbr-invf-max` as
the other end of the clamp. Setting `--sbr-invf-min 3` or `--sbr-invf-min` equal to
`--sbr-invf-max` collapses the dynamic range to a single value, which is `--sbr-invf` the
long way round. Wins over `--sbr-softness`; loses to `--sbr-invf`.
`--sbr-softness 3` is the packaged version of `--sbr-invf-min 1`.

#### `--sbr-invf-max <0..3>`

**Sets a ceiling over the estimator: the highest inverse filtering level it is allowed to
pick.**

- Applies to: SBR profiles
- Values: `0..3`; default `-1` = off (no ceiling, equivalent to `3`)
- Input: whole number

The mirror of `--sbr-invf-min`, and the more frequently useful of the two. It is the tool
for "the top goes hissy and loses detail on the loud passages", where the estimator picks
strong whitening and takes the sparkle with it.

Lowering the ceiling preserves detail at the price of letting some metallic character
through in the bands that genuinely needed whitening.

Values:
- `3` ceiling at HIGH. No constraint; identical to not using the switch.
- `2` never HIGH. The usual choice: it removes only the most aggressive whitening and keeps
  the rest of the estimator's judgement.
- `1` never above LOW. A strong detail preserving setting; expect audible metallic
  character on tonal material.
- `0` the ceiling is off, so no whitening is permitted anywhere. Equivalent in effect to
  `--sbr-invf 0`, and better written that way.
- `-1` off.

Example in context: `fdkaac -p 5 -b 64000 --sbr-invf-max 2 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Works with / against: applied after `--sbr-invf-bias`, paired with `--sbr-invf-min`.
`--sbr-softness 1` is the packaged version of `--sbr-invf-max 2`, and note the measured
caveat there: on music the estimator rarely reaches HIGH, so this ceiling can be a no-op on
music material and still be exactly right on speech. Wins over `--sbr-softness`; loses to
`--sbr-invf`.

#### `--sbr-invf-bias <-3..3>`

**Shifts every dynamic inverse filtering decision up or down by a fixed number of steps.**

- Applies to: SBR profiles
- Values: `-3..3` steps; default `0` = off
- Input: whole number

Where min and max clip the range, this slides it. The estimator's per band, per frame
pattern of decisions is preserved exactly, and the whole pattern moves. That makes it the
cleanest way to say "the estimator has the right idea, it is just consistently too soft" or
"consistently too hard". Positive means more whitening.

Because the result is clamped to 0..3 afterwards, large biases flatten the pattern at
whichever end they run into: at `+3` almost everything lands on HIGH and the per band
variation you were trying to keep is gone.

Examples, more whitening:
- Mild: `--sbr-invf-bias 1`. Every band one step more whitened. The pattern is intact,
  the character is softer.
- Strong: `--sbr-invf-bias 2`. Two steps. Bands that wanted off now get MID; metallic
  character is largely gone and so is some detail.
- Exaggerated: `--sbr-invf-bias 3`. Everything the estimator did not already want at HIGH
  gets pushed there by the clamp. This is `--sbr-invf 3` with extra steps.

Examples, less whitening:
- Mild: `--sbr-invf-bias -1`. One step less everywhere. More detail, a hint more metallic.
- Strong: `--sbr-invf-bias -2`. Two steps less. Only the bands the estimator wanted at HIGH
  retain any whitening.
- Exaggerated: `--sbr-invf-bias -3`. Nearly everything clamps to off, which is
  `--sbr-invf 0` the long way round, with maximum metallic ringing on tonal material.

Works with / against: applied **before** `--sbr-invf-min` and `--sbr-invf-max`, so you can
bias and then clamp: `--sbr-invf-bias 2 --sbr-invf-max 2` shifts everything up but refuses
HIGH. Wins over `--sbr-softness`; loses to `--sbr-invf`.

#### `--sbr-invf-table <0|1>`

**Chooses which decision table the tonality estimator uses: the music table or the speech
table.**

- Applies to: SBR profiles
- Values: `0` music, `1` speech; default `-1` = follows `--speech`
- Input: whole number

The estimator's thresholds come from a table, and FDK ships two: one tuned for music, one
for speech. The speech table whitens considerably more readily, because speech above the
crossover is mostly fricatives and breath, where inherited tonal structure is pure artefact
and there is no detail worth protecting.

This switch picks the table and **only** the table. `--speech` also changes the crossover
and other tuning; if you want the speech estimator behaviour on music material without
rearranging the frequency layout, this is the switch for it. That combination is genuinely
useful: spoken word programme, audiobooks, podcast material at low bitrate, where you want
the whitening behaviour but your own crossover.

Values:
- `0` music table. Conservative whitening, protects tonal detail.
- `1` speech table. Whitens much more readily. Measured mean invf level with the speech
  table is near 2.0 against about 1.0 for the music table at HE-AAC 32 kbps.
- `-1` off: follow `--speech`, so music table unless `--speech` is given.

Example in context: `fdkaac -p 5 -b 32000 --sbr-invf-table 1 -o out.m4a speech.wav`

Works with / against: `--sbr-softness 4` is the packaged version of this plus the speech
noise weight. `--speech` sets this implicitly and also moves the crossover, which is the
difference worth remembering. Wins over `--sbr-softness`; loses to `--sbr-invf`.

#### `--sbr-invf-hyst <x>`

**Sets the hysteresis of the decision borders: how much the tonality has to change before
the estimator switches a band to a different level.**

- Applies to: SBR profiles
- Values: `0..2`, decimal where `1.0` = stock; default `-1` = off
- Input: decimal, up to 3 places, `.` or `,`

A band whose tonality sits right on a decision border would otherwise flip level every
frame, and flipping whitening levels frame to frame is audible as a kind of fluttering or
unsteadiness up top. Hysteresis widens the borders into a dead zone, so a band keeps its
current level until the tonality has moved meaningfully past the line.

Lowering it reacts faster to genuine changes and switches more often. Raising it is more
stable and slower to follow real changes, so a passage that turns tonal part way through
keeps the previous treatment for longer. Above 2 the widened borders would overlap each
other, which has no meaning, so that is rejected.

For reference, the stock per band switching rate is 0.004 to 0.04 changes per band per
frame, and a configuration measured at 0.2 to 0.29 was clearly unstable.

Examples, less hysteresis (faster, more switching):
- Mild: `--sbr-invf-hyst 0.8`. Slightly quicker to follow changes.
- Strong: `--sbr-invf-hyst 0.3`. Clearly more switching; use when the estimator feels
  sluggish on material that changes character quickly.
- Exaggerated: `--sbr-invf-hyst 0`. No dead zone at all. Bands sitting on a border flip
  every frame, which is the instability hysteresis exists to prevent.

Examples, more hysteresis (slower, more stable):
- Mild: `--sbr-invf-hyst 1.2`. A little stickier than stock.
- Strong: `--sbr-invf-hyst 1.5`. Levels hold through moderate tonality changes. A good
  pairing with `--sbr-invf-max 2` when you want a calm, predictable top.
- Exaggerated: `--sbr-invf-hyst 2`, the maximum. The dead zones are as wide as they can be;
  bands essentially keep whatever level they first landed on, which on material that
  changes character is simply the wrong level most of the time.

Works with / against: `--sbr-invf-smooth` attacks the same instability from the other side,
by averaging over frames instead of widening borders; using both at their extremes is
redundant. `--sbr-invf-sbr-thr` and `--sbr-invf-orig-thr` move the borders this knob is
widening. Wins over `--sbr-softness`; loses to `--sbr-invf`.

#### `--sbr-invf-sbr-thr <x>`

**Shifts the tonality decision borders for the patched band, roughly in dB.**

- Applies to: SBR profiles
- Values: `-20..20`, about dB, decimal; default `0` = off
- Input: decimal, up to 3 places, `.` or `,`

The estimator measures tonality on both sides of the comparison and compares each against
borders. This moves the borders on the patched side. Positive means the patched band counts
as tonal sooner, and a tonal patch is what whitening is for, so positive means more invf.

This is a sharper instrument than bias or min/max, and a riskier one. Moving borders changes
*which* bands cross them and in which frames, so it can make the decision flicker: this
mechanism measured 0.2 to 0.29 changes per band per frame against 0.004 to 0.04 for stock,
which is why `--sbr-softness` does not use it. If you reach for this, consider raising
`--sbr-invf-hyst` at the same time to buy the stability back.

Examples, positive (patched band tonal sooner, more invf):
- Mild: `--sbr-invf-sbr-thr 2`. A small shift; a few more bands cross into whitening.
- Strong: `--sbr-invf-sbr-thr 8`. Many more bands count as tonal. Expect noticeably more
  whitening and some flicker.
- Exaggerated: `--sbr-invf-sbr-thr 20`, the maximum. Essentially every patched band counts
  as tonal, so the measurement no longer discriminates and the estimator's judgement is
  replaced by a constant. `--sbr-invf 3` does the same thing without the flicker.

Examples, negative (patched band tonal later, less invf):
- Mild: `--sbr-invf-sbr-thr -2`. Slightly fewer bands whitened.
- Strong: `--sbr-invf-sbr-thr -8`. Only strongly tonal patches get whitened; more detail,
  more metallic character.
- Exaggerated: `--sbr-invf-sbr-thr -20`, the minimum. Nothing counts as tonal, so nothing
  is whitened. The caricature of "preserve all detail", and it sounds like it.

Works with / against: the mirror of `--sbr-invf-orig-thr`, and the two combine into the
original versus patch comparison. `--sbr-invf-hyst` is the stability counterweight. Wins
over `--sbr-softness`; loses to `--sbr-invf`.

#### `--sbr-invf-orig-thr <x>`

**Shifts the tonality decision borders for the original band, roughly in dB.**

- Applies to: SBR profiles
- Values: `-20..20`, about dB, decimal; default `0` = off
- Input: decimal, up to 3 places, `.` or `,`

The same mechanism on the source side, and the sign reads the other way round, which is the
thing to get right. The estimator whitens when the patch looks more tonal than the original
deserves. So making the *original* count as tonal **later** means the comparison finds a
bigger mismatch more often, and positive values therefore also mean more whitening. Both
threshold knobs point the same way in outcome despite describing opposite sides.

The flicker caveat applies here identically: this is a border shift, and border shifts
destabilise the per band decision more than bias or clamping do.

Examples, positive (original counts as tonal later, more invf):
- Mild: `--sbr-invf-orig-thr 2`.
- Strong: `--sbr-invf-orig-thr 8`. The original is credited with tonality rarely, so the
  patch looks wrong more often and gets whitened.
- Exaggerated: `--sbr-invf-orig-thr 20`, the maximum. The original never counts as tonal,
  so every patch is treated as damage. Maximum whitening by a route that also maximises
  flicker.

Examples, negative (original counts as tonal sooner, less invf):
- Mild: `--sbr-invf-orig-thr -2`.
- Strong: `--sbr-invf-orig-thr -8`. The original is generously credited as tonal, so the
  patch is more often judged acceptable and left alone.
- Exaggerated: `--sbr-invf-orig-thr -20`, the minimum. Everything in the original counts as
  tonal, the comparison finds no mismatch, and whitening effectively stops.

Works with / against: pairs with `--sbr-invf-sbr-thr`; moving both in the same direction
partly cancels, since one raises the mismatch and the other lowers it. The noise floor
reads the same original versus patch gap through `--sbr-nf-weight`, so shifting these
borders changes the fill noise as well, not just the whitening. Wins over `--sbr-softness`;
loses to `--sbr-invf`.

#### `--sbr-invf-smooth <0..2>`

**Sets how many frames the tonality estimator averages before deciding.**

- Applies to: SBR profiles
- Values: `0`, `1`, `2`; default `-1` = off (stock averages 2)
- Input: whole number

Averaging over frames is the other defence against an unstable decision. With averaging,
one anomalous frame cannot flip a band's whitening level; without it, each frame is judged
on its own. The trade is the usual one: stability against reaction speed.

Values:
- `0` this frame only. Fastest reaction to a genuine change of character, most frame to
  frame switching. Use on material that changes quickly, and expect some unsteadiness.
- `1` a shorter average than stock. The middle option.
- `2` stock. The balance FDK chose, and a good one on most material.
- `-1` off, which is the same as `2`.

Example in context: `fdkaac -p 5 -b 64000 --sbr-invf-smooth 0 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Works with / against: this and `--sbr-invf-hyst` both buy stability, by different means, so
pairing low smoothing with high hysteresis is a reasonable way to get fast reaction without
flutter. Wins over `--sbr-softness`; loses to `--sbr-invf`, which has no decision left to
smooth.

#### `--sbr-nf-weight <x>`

**Sets how strongly the original versus patch tonality gap raises the noise floor.**

- Applies to: SBR profiles, and only where the noise ceiling leaves room (see below)
- Values: `0.0..1.0`, decimal; default `-1` = follows `--speech` (music 0.25, speech 1.0)
- Input: decimal, up to 3 places, `.` or `,`

Whitening removes structure; fill noise replaces it. This weight decides how much of the
measured mismatch between the original and the patched band is converted into noise floor.
Higher means more fill noise wherever the patch is too tonal, which is why the speech tuning
uses 1.0: in speech the high band should be noise, so filling it is always right. Lower
means the gap is mostly handled by whitening alone.

**Read this before using it.** The weighted noise is clipped by the noise ceiling, and with
the tuning table's ceiling (0.125 at 24 to 48 kbps) about 95 percent of bands are already at
the ceiling, so raising the weight changes nothing because there is no headroom to use.
Measured, this knob moved the stream only with `--sbr-noise-max 6` **plus** either
`--sbr-invf 3` or `--sbr-nf-weight-from 1`. Alone, or with only one of those two, nothing
changed at all. It needs a raised ceiling and bands at a high invf level, since by default
only HIGH bands receive the weight.

So the working recipe is `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight <x>`, or
let `--sbr-softness 5`, `6` or `7` raise the ceiling for you.

Examples, more fill noise (with the ceiling raised):
- Mild: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.4`. A little more air
  where the patch is too tonal.
- Strong: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7`. Clearly more fill
  noise; the top reads as airier and less metallic, with less apparent detail.
- Exaggerated: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 1`, the maximum,
  which is the speech value applied to every band from LOW upward with the ceiling wide
  open. On music this is hiss where cymbals used to be.

Examples, less fill noise:
- Mild: `--sbr-nf-weight 0.15`. Below the music default; slightly drier top.
- Strong: `--sbr-nf-weight 0.05`. The tonality gap contributes almost no noise; whitening
  has to do the work alone.
- Exaggerated: `--sbr-nf-weight 0`, the minimum. The gap is ignored entirely. With the
  default ceiling this may well be bit identical to stock, since most bands are clipped
  anyway, which is itself the lesson about this knob.

Works with / against: needs `--sbr-noise-max 3` or `6` to have headroom, and
`--sbr-nf-weight-from` to reach bands below HIGH. `--sbr-noise-floor-offset` raises the
floor unconditionally and is the blunter alternative when this one refuses to move. Wins
over `--sbr-softness`; `--sbr-invf 3` does not block it, it is one of the two things that
enables it.

#### `--sbr-nf-weight-from <1..3>`

**Sets the lowest inverse filtering level whose bands receive the noise floor weight.**

- Applies to: SBR profiles
- Values: `1` LOW and above, `2` MID and above, `3` HIGH only; default `-1` = off
  (stock is `3`)
- Input: whole number

The tonality gap weight is not applied everywhere. By default only bands that got HIGH
whitening receive it, on the reasoning that those are the bands where structure was really
destroyed and needs replacing. This switch widens that set downward.

That makes it the enabling switch for `--sbr-nf-weight`: on music the estimator rarely picks
HIGH, so with the stock setting the weight has almost no bands to act on, and the knob looks
dead. Set this to `1` and every whitened band is in scope.

Values:
- `3` HIGH only. Stock. The most conservative, and the reason `--sbr-nf-weight` so often
  appears to do nothing on music.
- `2` MID and HIGH. A moderate widening, and what `--sbr-softness 5` and above do.
- `1` LOW, MID and HIGH. Every band with any whitening gets the weight. The widest scope,
  and the one measured to let `--sbr-nf-weight` change the stream when paired with
  `--sbr-noise-max 6`.
- `-1` off, same as `3`.

Example in context: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Works with / against: pointless without `--sbr-nf-weight` doing something, and
`--sbr-nf-weight` is pointless without headroom from `--sbr-noise-max`. All three move
together. Wins over `--sbr-softness`.

#### `--sbr-nf-smooth <0>`

**Turns off the time smoothing of the noise floor.**

- Applies to: SBR profiles
- Values: `0` = no smoothing; default `-1` = off, which leaves the stock smoothing over
  4 frames
- Input: whole number

The noise floor is normally averaged over four frames, so the amount of fill noise in a band
changes gradually. That is right on steady material, where a jumping noise level would be
audible as a pumping or breathing quality in the top end. It is wrong where the material
genuinely changes fast: the smoothed floor arrives late, so a sudden quiet passage still
carries the noise of the loud one for a few frames, and a sudden bright passage is
under filled at its start.

This switch is one directional. There is no value that increases the smoothing; `0` removes
it and anything else means leave it alone.

Values:
- `0` no time smoothing. The noise floor follows the frame by frame estimate exactly.
  Faster, and on steady material audibly less stable.
- `-1` off: stock smoothing over 4 frames. The right default.

Example in context: `fdkaac -p 5 -b 64000 --sbr-nf-smooth 0 -o out.m4a transients.wav`

Works with / against: the noise analogue of `--sbr-invf-smooth 0`, and the same reasoning
applies: use it on material with fast changes of character, and expect less stability on
sustained material. Its effect is bounded by `--sbr-noise-max`: if the ceiling is clipping
most bands, there is little variation left for smoothing to smooth. Wins over
`--sbr-softness`.

#### `--sbr-noise-max <6|3|-3>`

**Sets the ceiling on how loud the noise SBR injects may get: the air versus hiss limit up
top.**

- Applies to: SBR profiles
- Values: `6` = 1.0, `3` = 0.5, `-3` = 0.125; default `-1` = the value from the bitrate
  tuning table
- Input: whole number

This is the most consequential noise knob, not because it sets the level but because it sets
the limit everything else runs into. The three values are the three the FDK noise estimator
understands, and they correspond to amplitude ceilings of 1.0, 0.5 and 0.125. The tuning
table picks 0.125 at 24 to 48 kbps, and at that ceiling about 95 percent of bands are
already clipped, which is why fill noise feels fixed until you raise it.

Raising the ceiling does not by itself add noise; it allows the estimator and the weight to
add more where they already wanted to. Lowering it caps the top end into something drier and
more tonal, which on sparse material is cleaner and on cymbals and breath sounds thin.

Which of the three values is a no-op depends on the tuning table row you are on, so measure
rather than assume. Measured: on tonal and mixed signals at HE-AAC 32 kbps, `-3` equals
stock and `6` equals `3`; on a voice signal at the same setting, `6`, `3` and stock are all
the same.

Values:
- `-3` ceiling 0.125. The driest, and the table's choice at low bitrate. More tonal, more
  metallic, less hiss.
- `3` ceiling 0.5. Four times the headroom. Fill noise can actually vary band to band.
- `6` ceiling 1.0. Maximum headroom. Paired with `--sbr-nf-weight` and
  `--sbr-nf-weight-from 1` this is the configuration where the noise floor knobs finally
  have room to work; on its own it mostly lifts the bands the estimator was already pushing
  against the ceiling.
- `-1` off: the bitrate tuning table decides.

Example in context: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Works with / against: this is the gate in front of `--sbr-nf-weight`,
`--sbr-nf-weight-from` and `--sbr-nf-smooth`, and `--sbr-softness` levels 5 to 7 raise it
for you (5 and 6 to 0.5, 7 to 1.0). `--sbr-noise-bands` sets the frequency resolution of
the floor this ceiling caps, so a dense floor at a low ceiling is wasted side information.

#### `--sbr-noise-floor-offset <n>`

**Offsets the whole SBR noise floor, adding or removing fill noise everywhere at once.**

- Applies to: SBR profiles
- Values: `0..11`; default `-128` = off
- Input: whole number

Where `--sbr-nf-weight` adds noise conditionally, based on a measured mismatch, this simply
shifts the floor. Bigger means more fill noise in the reconstruction across the board, which
reads as more air and more hiss, and smothers whatever inherited tonal structure survived
whitening. Smaller means a drier, more tonal top.

That bluntness is sometimes exactly what you want. When `--sbr-nf-weight` refuses to move
because every band is clipped at the ceiling, this is the knob that still works, because it
acts on the floor itself rather than on the gap measurement.

The accepted range is `0..11` and nothing else. Outside it, FDK's own noise handling hits an
assertion and aborts, so the encoder rejects out of range values with a clean error rather
than letting that happen.

Examples, more fill noise:
- Mild: `--sbr-noise-floor-offset 3`. A touch more air up top.
- Strong: `--sbr-noise-floor-offset 7`. Clearly noisier top; metallic character is buried
  and so is fine detail.
- Exaggerated: `--sbr-noise-floor-offset 11`, the maximum. The high band is dominated by
  fill noise. Cymbals become shaped hiss, and the result is unambiguously a caricature,
  which makes it a useful reference point for how much of your top end is noise.

Examples, less fill noise:
- Mild: `--sbr-noise-floor-offset 2`. Slightly drier.
- Strong: `--sbr-noise-floor-offset 1`. Minimal fill; the patched band's own structure and
  whatever whitening leaves behind carry the top end.
- Exaggerated: `--sbr-noise-floor-offset 0`, the minimum. No offset contribution at all. On
  tonal material expect the metallic ringing that fill noise exists to mask, which pairs
  badly with low `--sbr-invf` settings and is worth hearing once for exactly that reason.

Works with / against: still subject to `--sbr-noise-max`, so a high offset against the
0.125 ceiling clips. Works in the same direction as `--sbr-nf-weight` but unconditionally,
which makes it the right choice when you want a global change and the wrong one when you
want the encoder to decide where noise belongs. Against low `--sbr-invf` levels it is
fighting a different artefact with the wrong tool: metallic ringing from un whitened
structure is better fixed with whitening than masked with noise.

### Core bandwidth: `--core-cutoff`

#### `--core-cutoff <hz>`

**Forces the upper edge of the band the AAC core codes, in Hz. Without SBR it does exactly
what `-w` does; under SBR it can only narrow the core, and that is an experiment.**

- Applies to: every profile
- Values: `0..48000` Hz (out of range is rejected); the encoder stops at the core Nyquist
  (half the core sample rate); default `0` = leave FDK's own decision
- Input: whole number

Without SBR (`-p 2`, `-p 23`, `-p 39` without `-L 1`, DAB+ LC) this switch and `-w` are the
same thing: in AAC-LC CBR and VBR, AAC-LD, AAC-ELD, DAB+ and 96 kHz material with
`--uncap-bandwidth`, `-w N` and `--core-cutoff N` gave bit identical streams. Use whichever
reads better in your scripts. The full band case at 96 kHz is a typical one:
`--uncap-bandwidth --core-cutoff 40000` or `--uncap-bandwidth -w 40000`.

Under SBR the situation is different, and it is the reason this card sits at the end of
the group. When SBR initialises, its tuning table sets the core bandwidth so that the core
stops where the SBR start begins, and `-w` is discarded. `--core-cutoff` is applied after
that and survives, but only one direction of it is useful:

- **Raising it above the SBR start does nothing audible.** The decoder rebuilds everything
  above the SBR start from its own SBR data and writes over whatever the core sent there.
  Measured at HE-AAC 64 kbps stereo, 48 kHz input: with `--core-cutoff 12000` the decoded
  signal above 10.5 kHz has a coherence of 0.03 with the original, the same as stock, with
  both the FDK decoder and ffmpeg. The core still pays for those lines, and it shows lower
  down: coherence at 7..9.5 kHz drops from 0.95 to 0.92.
- **Lowering it below the SBR start opens a hole.** Nothing is coded between the new core
  edge and the SBR start, so that band is missing rather than gently rolled off. At the
  same setting `--core-cutoff 9000` leaves 9..10 kHz at -15 to -33 dB.

To give the core more real spectrum under SBR, move the SBR start up with `--sbr-start`;
to give it less, move the start down. The core follows the start by itself and the two
stay joined. At HE-AAC 64 kbps `--sbr-start 15` takes the core from 10.5 to 12 kHz with
coherence 0.93 up to 11.5 kHz. What is left for `--core-cutoff` under SBR is the
deliberate experiment: hearing what a gap between core and SBR sounds like.

The cutoff snaps to a scalefactor band edge, so `--verbose` reports the real value: asking
for 17300 Hz and being told 17915 Hz is not an error. The ceiling is half the core sample
rate (12 kHz for 48 kHz input under dual rate SBR). Without SBR there is also a 20 kHz cap,
which `--uncap-bandwidth` (group A) lifts.

Examples, without SBR:
- Mild: `-p 2 -b 128000 --core-cutoff 16000`. Same stream as `-w 16000`.
- Strong: `-p 23 -b 64000 --core-cutoff 12000 -f 10`. AAC-LD picks a very narrow core on
  its own (about 5.8 kHz at 64 kbps); this widens it, as `-w 12000` would.
- Exaggerated: `-p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000` on 96 kHz input.
  Legal; at lower bitrates the same request drains the budget into inaudible treble.

Examples, under SBR (HE-AAC 64 kbps stereo, 48 kHz input, core 10.5 kHz):
- Raising: `--core-cutoff 11500` or `12000`. Bits spent on lines the decoder writes over;
  nothing new is heard above 10.5 kHz.
- Lowering: `--core-cutoff 9000`. The 9..10 kHz hole described above.
- Lowering, exaggerated: `--core-cutoff 2000`. Everything from 2 kHz to the SBR start is
  missing; only useful to hear what the core alone carries.

Works with / against: under SBR `--sbr-start` decides the core, and `--verbose` labels the
source of the core bandwidth, `[from --core-cutoff]` or `auto`. `--uncap-bandwidth` is
needed only to go past 20 kHz without SBR.

## D. Masking, noise shaping and detail

This is the group that decides how much noise the encoder is willing to leave in each
band, and where it fabricates signal instead of coding it. Four ideas carry the whole
group, so they are explained once here and the cards refer back.

**Masking threshold.** For every scalefactor band, in every frame, the psychoacoustic
model computes a level below which added noise should be inaudible, because louder
content in and around that band covers it. That level is the masking threshold. The
quantiser then spends only as many bits as it takes to push the quantisation noise down
to the threshold, and not one more. Raise the thresholds globally and the encoder
decides more noise is acceptable, so it spends fewer bits and the file gets coarser at
the same bitrate; lower them and it demands more precision everywhere and runs out of
bits sooner, which under CBR means something else has to give. `--ath-scale` is the
global scale on this threshold. `--spread-mask` changes how far masking leaks between
neighbouring bands, which is a different question: not how loud the mask is, but how
wide.

**SNR demand.** Separately from the threshold, FDK keeps a required signal to noise
ratio per band, the minimum quality the coder must deliver in that band whatever the
threshold says. This is the closest FDK gets to the tone masking noise and noise
masking tone constants that MusePack and LAME expose. It matters more than it sounds,
because the hole avoidance logic clamps thresholds back to this demand: if the rate
loop wants to starve a band, the SNR demand is the line it is not allowed to cross.
`--minsnr-scale` scales the demand, and `--minsnr-clamp-lo` and `--minsnr-clamp-hi`
move the finest and the coarsest end of the window the demand is held in.

**TNS, temporal noise shaping.** Inside one block, quantisation noise is spread evenly
across the whole block in time. That is wrong for a transient: a block containing a
drum hit has noise sitting in the quiet part before the hit, where nothing masks it,
which you hear as pre-echo. TNS applies a prediction filter across frequency, which has
the effect of shaping the noise envelope in time so it follows the signal envelope and
hides under the attack. `--tns-mask` chooses which of the filters are allowed to run
and `--tns-order` how long they may be. For LD and ELD this is the main defence against
pre-echo, since those profiles have no short blocks at all.

**PNS, perceptual noise substitution.** When a band is just noise, coding its exact
spectral lines is a waste: nobody can tell one noise realisation from another at that
level. PNS throws the lines away and stores a single number, the band's energy, with a
flag saying "noise here". The decoder regenerates random noise scaled to that energy.
The saving on noisy material is large. The risk is that a band which is not really
noise gets substituted, and then a genuine tone turns into hiss. Eight switches in this
group control PNS: whether it runs at all, from which frequency, whether the
low bitrate gate applies, how loud the fabricated noise is, and three detection
thresholds plus a minimum band width that together decide which bands qualify.

One convention before the cards: all the `--pns-*` scaling knobs take a plain decimal
multiplier on FDK's factory value. `1.0` means leave the factory value alone, `1.5`
means one and a half times it, `0.5` means half. `--pns-min-width` is the exception, it
is a raw count.

### `--tns-mask <n>`

**Chooses which of the temporal noise shaping filters the encoder may use.**

- Applies to: every profile with an AAC core. It matters most in LD and ELD, where TNS
  is the only defence against pre-echo, and on transient heavy material in LC.
- Values: `0..15`, a four bit mask; default: `-1`, stock `0xF`, all filters allowed
- Input: whole number

TNS shapes quantisation noise in time so it hides under an attack instead of sitting in
the quiet before it. FDK runs more than one TNS filter, differing by block type and by
the frequency region they cover, and the mask says which of them are enabled. Stock is
`0xF`, which is 15, meaning all four bits set.

This is a diagnostic and laboratory switch rather than a quality knob. The useful
settings are `0`, which disables TNS entirely and lets you hear what it was doing, and
`15`, which is stock. Intermediate masks let you isolate one filter, which is
interesting when you are trying to attribute an artifact, and rarely interesting
otherwise.

Disabling TNS has a second consequence worth knowing: FDK forces PNS off when TNS is
off. So `--tns-mask 0` silently takes perceptual noise substitution with it.

Values:
- `-1`: stock, all filters enabled (`0xF`).
- `0`: TNS fully off. Pre-echo on sharp attacks gets noticeably worse, especially in
  LD and ELD; PNS is forced off as well.
- `1` to `14`: individual filters enabled by bit. For isolating one filter's
  contribution.
- `15`: all filters enabled, the same as stock.

Example in context:

    fdkaac -p 2 -b 128000 --tns-mask 0 --verbose -o no_tns.m4a attacks.wav

Works with / against: `--tns-order` limits how long the enabled filters may be, so the
two are read together. `--tns-thresh` in group E decides how often TNS switches on at
all, which is the knob you usually want instead of the mask. Setting `0` here forces
PNS off regardless of `--pns`. `--verbose` prints the effective TNS mask as a hex
bitmask.

### `--tns-order <n>`

**Caps the length of the temporal noise shaping filter, trading how precisely the noise
envelope can follow the signal against the side information it costs.**

- Applies to: every profile with an AAC core. Short blocks are additionally capped at
  order 5 by the standard, so values above that only affect long blocks.
- Values: `1..12`; default: `-1`, FDK's own order
- Input: whole number

The TNS filter is a linear predictor running across frequency. Its order is how many
coefficients it has, and the order determines how detailed a time envelope it can
impose on the noise. A low order can only produce a crude shape, roughly "more noise
later, less earlier". A high order can follow a complicated envelope, at the cost of
sending more filter coefficients per frame as side information.

Lower values make TNS cheaper and blunter. At low bitrate that trade can be worth it,
because the coefficients compete with the spectrum for the same bits. Higher values let
the shaping track attacks more precisely, which is the direction to go when pre-echo is
the problem and you have bits, particularly in LD and ELD where no short blocks exist to
help.

Examples:
- Mild, longer filter: `--tns-order 8`. Slightly finer time shaping in long blocks, a
  small side information cost.
- Strong, longer filter: `--tns-order 12`. The maximum. Best time resolution TNS can
  give; worth trying in LD and ELD on percussive material.
- Exaggerated, longer filter: `--tns-order 12` is already the ceiling, so the
  caricature here is combining it with `--tns-thresh 0.5` from group E so the long
  filter also runs on nearly every frame. Expect side information to eat a visible
  share of a low bitrate budget and holes to appear in exchange.
- Mild, shorter filter: `--tns-order 5`. The short block limit applied everywhere.
  Cheaper, with slightly coarser shaping in long blocks.
- Strong, shorter filter: `--tns-order 3`. Crude envelope shaping only. Cheap; most of
  the pre-echo benefit is gone but the gross shaping remains.
- Exaggerated, shorter filter: `--tns-order 1`. A single coefficient, which can express
  almost nothing about the envelope. TNS is nominally on and effectively useless, which
  is a good way to confirm how much of the benefit comes from the filter length rather
  than from TNS being enabled.

Works with / against: `--tns-mask` decides which filters exist, this decides how long
they are; `0` on the mask makes this irrelevant. `--tns-thresh` in group E decides how
often they run. In LC, short blocks already address most transients, so the order
matters less there than in LD and ELD.

### `--pns <n>`

**Turns perceptual noise substitution on or off, overriding the encoder's own
decision.**

- Applies to: AAC-LC, AAC-LD and AAC-ELD cores under CBR. FDK forces PNS off when the
  bitrate mode is VBR and when TNS is off, and those rules are above this switch.
- Values: `0` off, `1` on; default: `-1`, the encoder decides
- Input: whole number, on/off

PNS replaces a band judged to be noise with a single energy value and a flag, and the
decoder regenerates noise at that energy. On noisy material the saving is large, and on
tonal material the risk is that a real tone becomes hiss.

The encoder's own decision comes from its tuning table and depends mostly on bitrate.
At 64 kbps PNS is normally already on, so `1` there is the same as stock and only `0`
changes anything. Below about 28 kbps the table disables PNS completely, and at that
point neither `1` here nor `--pns-start` does anything, because PNS is switched off
higher in the chain; `--force-pns` is the switch that bypasses that gate.

The honest use for this card is diagnosis and deliberate choice: `0` to hear what PNS
was contributing, `1` to assert it where you believe the table is being conservative.
`--verbose` reports the resulting PNS state, which is the quickest way to find out
whether your request survived the gates.

Values:
- `-1`: leave FDK's decision (stock, default).
- `0`: PNS off. On noisy material expect the noise bands to be coded properly and the
  bits to come from somewhere else.
- `1`: PNS on, where the gates permit. At 64 kbps and up this usually matches stock; at
  low bitrate it needs `--force-pns` to have any effect.

Example in context:

    fdkaac -p 2 -b 64000 --pns 0 --verbose -o no_pns.m4a in.wav

Works with / against: `--force-pns` removes the low bitrate gate. `--tns-mask 0`
forces PNS off whatever you set here, because FDK requires TNS for PNS. VBR mode
(`-m 1..5`) does the same. `--pns-start`, `--pns-tonality`, `--pns-refpower`,
`--pns-gapfill` and `--pns-min-width` all shape PNS once it is running and do nothing
when it is not.

Measured: at AAC-LC 64 kbps, `--pns 1` gives the stock stream, since stock already has
PNS on there, while `--pns 0` changes it.

### `--pns-start <hz>`

**Sets the lowest frequency at which noise substitution is allowed to act.**

- Applies to: the same places as `--pns`, and only when PNS is actually running.
- Values: frequency in Hz; default: `-1`, FDK's own start frequency
- Input: whole number

PNS is safe high up and dangerous low down. Low bands carry the fundamental tones that
define pitch and timbre, and substituting noise there is immediately audible. FDK
therefore has a start frequency below which PNS is never used, and it depends on
bitrate and sample rate.

Lower the start and more of the spectrum becomes eligible, so more bands get noise
coded and more bits are freed, with a growing risk of substituting something that was
not noise. Raise it and PNS retreats upward, which is safer and cheaper in savings.

One practical trap: the value is snapped to a scalefactor band boundary, so a change of
a few hundred Hz may land in the same band and produce an identical stream. Move in
steps of a kilohertz or more when you are probing.

Examples:
- Mild, lower start: `--pns-start 3000`. PNS reaches a little deeper. Slightly more
  bands noise coded on broadband material.
- Strong, lower start: `--pns-start 2000`. Clearly more of the spectrum eligible. A
  real saving on noisy material; on anything with upper midrange tonality, listen for
  the tone turning grainy.
- Exaggerated, lower start: `--pns-start 500`. PNS allowed almost everywhere. Sustained
  notes lose their identity where the detector misfires, which is exactly the artifact
  the start frequency exists to prevent.
- Mild, higher start: `--pns-start 6000`. PNS pushed up a bit, a small loss of saving
  and a small gain in safety.
- Strong, higher start: `--pns-start 12000`. Only the top of the spectrum may be
  substituted. Safe, with most of the benefit gone.
- Exaggerated, higher start: `--pns-start 20000`. Above the coded band in most
  configurations, so PNS has nowhere left to act and this is effectively `--pns 0` by
  another route.

Works with / against: does nothing when PNS is off, which at low bitrate means you need
`--force-pns` first. `--pns-min-width` restricts PNS by band width rather than by
frequency, and since bands get wider as frequency rises, the two restrictions overlap.
`--verbose` shows the resulting PNS state.

Measured: at AAC-LC 64 kbps and 48 kHz the stock start is 4100 Hz. `4000` maps to the
same scalefactor band and gives an identical stream; `2000` and `12000` both change it.

### `--force-pns`

**Bypasses the low bitrate gate that otherwise disables noise substitution entirely
below about 28 kbps.**

- Applies to: AAC-LC, AAC-LD and AAC-ELD under CBR, at bitrates below the gate. Above
  the gate it changes nothing, because PNS is already permitted.
- Values: flag; default: off
- Input: flag

FDK's tuning table has a bitrate row below roughly 28 kbps in which every PNS entry is
zero, for every sample rate. The consequence surprises people: at 24 kbps, exactly
where cheap noise coding would help most, PNS is completely off, and both `--pns 1` and
`--pns-start` are ignored. The audible result is the MDCT style artifact on noisy
content that you would otherwise associate with MP3.

This flag makes the encoder use the first active row of the table instead of the zero
row, so PNS also works at 24 kbps and below. The rest of FDK's requirements still
apply: PNS needs TNS enabled and a non VBR mode, and nothing in this build changes
that.

Values:
- omitted: the gate applies, PNS off below about 28 kbps (stock).
- `--force-pns`: gate bypassed, PNS available at low bitrate.

Example in context:

    fdkaac -p 2 -b 24000 --pns 1 --force-pns --pns-start 4000 -o out.m4a in.wav

Works with / against: usually given together with `--pns 1` and often with
`--pns-start`, since the point of forcing PNS is to then control it. Pointless with
`--tns-mask 0` or under VBR, both of which force PNS off anyway. At bitrates above the
gate it is a no-op.

### `--pns-shared <0|1>`

**In bands that both channels code as noise, write one shared noise or two independent ones, the same way in every band.**

- Applies to: stereo AAC-LC core with PNS on (`--pns 1`, `--force-pns` or `--pns-above`). Off by default.
- Values: `1` = one shared noise (the same noise in L and R), `0` = independent noise in each channel

PNS replaces a band by noise of the right level. In a channel pair the format can say,
band by band, that the decoder should use the same noise in both channels. That flag rides
on the MS mask. Stock decides it per band by measuring the correlation, so the mask usually
has to be written band by band: up to about 25 bits per frame at low rates. A uniform choice
lets the mask fall back to its 2-bit all-bands form when the rest of the frame allows it.
The noise level is still written for each channel: the format has no shared level.

Examples (stereo music, 24 kbps, PNS from 2.2 kHz):
- Typical: `--pns-shared 1`. MS mask 28 to 2 bits per frame on correlated material.
- Wide stereo: `--pns-shared 0`. Independent noise keeps the width, the mask stays as expensive as stock.

When the mask still stays per band (bands below the PNS region coded as L/R on wide
material) the switch saves little: the mask is only cheap when every band agrees.
ffmpeg 6.1.1 plays shared noise as independent noise; the FDK decoder plays it as shared.

### `--pns-hold <dB>`

**Let neighbouring noise bands share one level, so the noise envelope costs fewer bits.**

- Applies to: AAC core with PNS on. Off by default.
- Values: `0.1..12.0` dB

Each PNS band carries its noise level as a difference from the previous one: 1 bit when
it is the same, 3 to 9 bits otherwise. With this switch a band whose level is within the
given tolerance of the previous noise band takes that level. The noise envelope over
frequency gets coarser, in steps of about 1.5 dB.

Examples (stereo music, 24 kbps, PNS from 2.2 kHz):
- Mild: `--pns-hold 3`. Noise levels 94 to 87 bits per frame.
- Strong: `--pns-hold 6`. 70 bits per frame; the freed bits go to the coded spectrum.
- Exaggerated: `--pns-hold 12`. The noise is almost flat over its whole region.

### `--pns-short <0|1>`

**Use `--pns-above` also in short (transient) frames, where it pays off.**

- Applies to: AAC-LC core with `--pns-above`. Off by default.
- Values: `1` = on, `0` = stock

The AAC format allows noise substitution in short windows; FDK never does it, so above
`--pns-above` a transient frame codes the noise bands as spectrum. With this switch they
become noise too, but only in a frame coded as one group of all 8 windows
(`--short-groups 1`). Measured (1.4.0-beta.2) in smaller groups: the noise energies took
19% of the frame instead of 2%, and the attack came out 2.6..6 dB quieter, so those groups
keep the spectrum. With the stock grouping the switch therefore changes nothing (bit for
bit).

Measured with `--short-groups 1 --pns-above 4000`, a sample with an attack in every frame:
24 kbps file 9810 to 9226 B (-6%), the noise band at the source level (+1.1 dB instead of
-1.4 dB). The cost: one noise level covers all 8 windows, so the band is smeared over the
frame: pre-echo +1 dB, attack about 2 dB quieter. On music with few short frames it does
nearly nothing (-0.3% of the file). For low bitrates where the band would otherwise be
missing; listen first.

### `--pns-budget <0|1>`

**Count the bits the PNS noise levels take, so a frame does not run out of bits and cut its spectrum.**

- Applies to: AAC core with PNS on. Off by default.
- Values: `1` = on, `0` = stock

FDK treats a PNS band as free when it plans the bits of a frame, but every noise level costs
bits in the stream. At low bitrates with a lot of PNS the frame then overflows, and the
emergency cut removes bands from the top. Stock gives the cut no credit for the PNS bands,
so it goes on through the noise region into the coded spectrum below it: whole frames lose
everything above about 1.5 kHz, which sounds like a crackle in frame rhythm. With this switch
the bit target for the spectrum is lowered by the cost of the noise levels, and the cut gets
those bits back when it removes a noise band.

Measured on acoustic stereo music, `-b 24000 --block-bias 0 -w 5500 --pns-above 2200`:
frames hit by the emergency cut 42 of 766 to 1, frame-rhythm modulation of the noise band
19.7 to 11.0 dB (11.6 without PNS). Together with `--pns-shared 1`: 0 frames cut, 8.3 dB.

Recommended for any forced PNS at low bitrate: `--pns-budget 1 --pns-shared 1`, optionally `--pns-hold 3`.

### `--pns-gain <x>`

**Sets how loud the fabricated noise is relative to the energy of the band it
replaced.**

- Applies to: wherever PNS is actually running.
- Values: `0.0` and up, where `1.0` leaves the energy unchanged; default: `-1`, off
- Input: decimal, up to 3 places, `.` or `,`

When PNS substitutes a band, the one number it stores is that band's noise energy, and
the decoder regenerates random noise scaled to it. This switch scales the stored energy
directly, so it is the "how loud is the substituted noise" dial and the PNS knob you
will reach for most often.

At `1.0` the regenerated noise carries the original band's energy, which is what the
mechanism intends. Above `1.0` the fill is louder than the original, which helps when
the substituted noise sounds too timid and the mix turns dull: real noise floors often
feel quieter after substitution because the regenerated noise lacks the original's
structure. Below `1.0` it is quieter, which is the move when the fill hisses more than
the source did.

This is pure taste territory with no correct value, which is why both directions are
worth trying on your own material before settling.

Examples:
- Mild, louder: `--pns-gain 1.2`. Slightly fuller noise fill; useful when cymbal wash
  loses presence.
- Strong, louder: `--pns-gain 2.0`. Twice the energy. Clearly more hiss and air; on
  broadband noise this can sound closer to the source than the nominally correct value.
- Exaggerated, louder: `--pns-gain 8.0`. Eight times the energy. The substituted bands
  dominate the top of the mix with obvious artificial hiss, which is a quick way to
  hear exactly which bands PNS has taken.
- Mild, quieter: `--pns-gain 0.8`. A slightly more discreet fill.
- Strong, quieter: `--pns-gain 0.4`. Noise fill clearly below the original energy. The
  top sounds cleaner and also duller, and a noise floor can disappear.
- Exaggerated, quieter: `--pns-gain 0.0`. Substituted bands carry no energy at all,
  which means they are silent, so PNS becomes a hole generator. A caricature, and a
  useful demonstration of how much of the signal was being carried by noise bands.

Works with / against: does nothing when PNS is off. `--pns-tonality`,
`--pns-refpower`, `--pns-gapfill` and `--pns-min-width` decide which bands get
substituted; this one only changes how loud the result is. Using it at `0.0` is a
clumsier way to get what `--pns 0` gives properly.

### `--pns-tonality <x>`

**Scales the tonality threshold that decides whether a band counts as noise and may be
substituted.**

- Applies to: wherever PNS is actually running.
- Values: `0.0` and up, where `1.0` is the factory threshold; default: `-1`, off
- Input: decimal, up to 3 places, `.` or `,`

PNS only fires on bands the encoder judges noise like, which it measures as low
tonality. The threshold is the line between "tonal, code it properly" and "noise,
substitute it". This switch scales that line.

Higher than `1.0` lets more bands qualify, including somewhat tonal ones, so the noise
substitution gets wider: more of the spectrum replaced by a cheap description, more
saving, and more risk that a genuine tone gets smeared into hiss. Lower than `1.0`
makes the detector pickier, so only the clearly noisy bands are substituted, which is
safer and saves less.

This is the knob that controls the breadth of PNS, as opposed to `--pns-start`, which
controls where it is geographically allowed, and `--pns-min-width`, which controls it by
band size.

Examples:
- Mild, wider substitution: `--pns-tonality 1.3`. A few more borderline bands become
  PNS. Small saving, little risk.
- Strong, wider substitution: `--pns-tonality 2.0`. Twice the threshold. Clearly more
  bands substituted; on noisy material a real saving, on tonal material audible
  smearing of partials.
- Exaggerated, wider substitution: `--pns-tonality 20.0`. Almost everything above the
  start frequency counts as noise. Sustained tones turn to hiss and the top of the mix
  becomes a noise bed: a caricature that makes the mechanism unmistakable.
- Mild, narrower substitution: `--pns-tonality 0.8`. Slightly pickier detection.
- Strong, narrower substitution: `--pns-tonality 0.4`. Only unambiguous noise is
  substituted. Safe, with much of the saving gone.
- Exaggerated, narrower substitution: `--pns-tonality 0.01`. Effectively nothing
  qualifies, so this reaches the same place as `--pns 0` while leaving PNS nominally
  enabled.

Works with / against: `--pns-refpower` is the other half of the detection decision and
both must agree before a band becomes PNS, so moving only this one can be blocked by
the power gate. `--pns-gain` changes the loudness of the result, not the selection.
`--pns-min-width` can exclude a band that passed this test.

### `--pns-refpower <x>`

**Scales the reference power threshold a band's level must satisfy before it is
considered a noise candidate.**

- Applies to: wherever PNS is actually running.
- Values: `0.0` and up, where `1.0` is the factory threshold; default: `-1`, off
- Input: decimal, up to 3 places, `.` or `,`

Tonality is not the only test. Each band's power is also compared against a reference
before the band can be a PNS candidate, which keeps the mechanism away from bands whose
level makes substitution a bad trade. This switch moves that power gate.

The two detection tests are conjunctive: tonality and reference power must both agree
before a band becomes PNS. That is why moving `--pns-tonality` alone sometimes does
nothing, and why this card and that one are usually tried together.

The value multiplies the threshold itself. Higher than `1.0` raises it, so fewer bands
pass and PNS retreats; lower than `1.0` lowers it, so more bands pass. Measured on
acoustic stereo music at 64 kbps LC, PNS bands per frame: 5.5 at `0.01`, 5.1 at `0.4`,
4.8 at `0.8`, 4.6 at `1.0`, 4.3 at `1.3`, 3.6 at `2.0`, 0 at `20.0`.

Examples:
- Mild, looser gate: `--pns-refpower 0.8`. Slightly more candidates (4.6 to 4.8 bands).
- Strong, looser gate: `--pns-refpower 0.4`. 5.1 bands. Combined with a raised
  `--pns-tonality` this is where wide noise substitution takes hold.
- Exaggerated, looser gate: `--pns-refpower 0.01`. The power test stops discriminating
  and selection is left to tonality (5.5 bands), a good way to find out how much work
  this gate was doing.
- Mild, stricter gate: `--pns-refpower 1.3`. Slightly fewer candidates (4.3 bands).
- Strong, stricter gate: `--pns-refpower 2.0`. Twice the threshold, 3.6 bands. PNS
  retreats to the bands where it is least risky.
- Exaggerated, stricter gate: `--pns-refpower 20.0`. Nothing passes, so PNS is on paper
  enabled and in practice idle.

Works with / against: conjunctive with `--pns-tonality`, so test them as a pair.
`--pns-gapfill` can still add a band between two PNS bands that failed this test.
Nothing here matters when PNS is off.

### `--pns-gapfill <x>`

**Scales the threshold of the heuristic that closes a one band gap between two
substituted bands.**

- Applies to: wherever PNS is running, and only when two PNS bands already sit on
  either side of a coded band. Honest status: wired, no effect observed on our test
  material.
- Values: `0.0` and up, where `1.0` is the factory threshold; default: `-1`, off
- Input: decimal, up to 3 places, `.` or `,`

When PNS has marked bands on both sides of a narrow gap, a coded island stranded
between two noise bands is usually worse than extending the noise across it: the island
is the only place with real spectral structure and it stands out. The gap fill
heuristic can mark that band as PNS too, and this switch scales the threshold it uses.

Higher makes gap filling more willing, lower makes it reluctant. Both directions are
subtle at best, because the precondition is strict: for AAC-LC the rule needs both
neighbours to be PNS already, and on our material that configuration did not arise, so
no value of this switch changed the output. The switch is present and wired, and it may
act on material whose spectrum produces the required pattern, but do not expect it to
be a tool you reach for.

If you want a genuine lever on how much of the spectrum becomes noise, use
`--pns-tonality` or `--pns-start` instead.

Examples:
- Mild, more filling: `--pns-gapfill 1.3`. Slightly more willing to close a gap.
- Strong, more filling: `--pns-gapfill 3.0`. Clearly more willing, where the
  precondition occurs at all.
- Exaggerated, more filling: `--pns-gapfill 50.0`. Any gap between two PNS bands is
  closed. On material that triggers it, this produces contiguous noise regions where the
  encoder had kept an island.
- Mild, less filling: `--pns-gapfill 0.7`. Slightly more reluctant.
- Strong, less filling: `--pns-gapfill 0.3`. Gaps are left as coded islands.
- Exaggerated, less filling: `--pns-gapfill 0.0`. Gap filling never happens, which is
  the closest thing to switching the heuristic off.

Works with / against: depends entirely on `--pns-tonality` and `--pns-refpower`
producing two adjacent PNS bands with a gap, so widening those first is the only way to
give this switch something to act on.

Measured: no stream change across 4 test signals, 5 bitrates and 6 values of this
switch in AAC-LC. The precondition for the AAC-LC gap fill rule, both neighbours already
PNS, did not occur on that material.

### `--pns-min-width <n>`

**Sets the minimum scalefactor band width, in spectral lines, that a band must have
before noise substitution may act on it.**

- Applies to: wherever PNS is running. Only values above the built-in default have any
  effect, which for AAC-LC means above 16.
- Values: raw line count, `1` and up; default: `-1`, off, which means FDK's own
  minimum (16 for AAC-LC)
- Input: whole number

Scalefactor bands get wider as frequency rises. A narrow band at the bottom of the
spectrum holds few lines and usually carries tonal content, and substituting noise
there is conspicuous. A wide band high up holds many lines and is where PNS belongs.
This switch expresses that preference directly, as a minimum width.

Note the asymmetry, because the obvious reading of the name is wrong. Raising the
minimum restricts PNS to wider bands, so to the upper spectrum, and that works.
Lowering it below FDK's own minimum does not open PNS up on narrower bands: FDK's
default is already the effective floor, so values below it give the stock stream. There
is one useful direction here, upward.

Unlike the `--pns-*` switches above, this is a raw count of lines, not a multiplier.

Examples:
- Mild, wider bands only: `--pns-min-width 24`. PNS restricted a little further up the
  spectrum. Small reduction in substitution, small loss of saving.
- Strong, wider bands only: `--pns-min-width 32`. Only clearly wide bands may be
  substituted. Safer on tonal material, with a visible drop in the PNS saving.
- Exaggerated, wider bands only: `--pns-min-width 64`. At AAC-LC 64 kbps this leaves no
  qualifying band at all, so the result is identical to `--pns 0`: PNS is enabled and
  has nothing it may touch.
- Downward there is no useful example: `2` and `4` both give the stock stream, because
  the built-in minimum of 16 already binds.

Works with / against: overlaps with `--pns-start`, which restricts PNS by frequency
rather than by width; since width grows with frequency the two restrictions reinforce
each other and the tighter one wins. Does nothing when PNS is off.

Measured: at AAC-LC, values `2` and `4` give the stock stream, confirming that the
built-in default of 16 is the floor. `64` at AAC-LC 64 kbps produces exactly the stream
that `--pns 0` produces, so PNS is effectively off at that width.

### Masking thresholds: an advanced layer

The seven switches below move the psychoacoustic model itself: the threshold, the masking
spread, the required SNR and the limits around it. Most of them take Q8 numbers. Under CBR
the rate loop has to hit the same number of bits per frame whatever the model says, so it
takes most of their effect back: measured on music, each of them moves the decoded result
by a few hundredths to a few tenths of a dB. Under VBR (`-m 1..5`) the bit count follows
the model, and `--ath-scale` and `--minsnr-scale` become real quality settings, paid for in
file size. `--spread-mask`, the two clamps, `--reduce-clamp` and `--mask-slope` remain
laboratory switches either way.

#### `--ath-scale <n>`

**The global masking threshold scale: one knob for "be cleaner" or "be more
economical" across the whole spectrum.**

- Applies to: every profile with an AAC core, so all of them. Under SBR it affects the
  core only, since SBR has its own noise model.
- Values: Q8, `1..4096`, where `256` is x1.0; default: `256`, no change
- Input: whole number

The masking threshold is the noise level the model believes you cannot hear in a given
band. `--ath-scale` multiplies all of them at once. Above 256 the thresholds rise, the
encoder concludes more noise is inaudible, and it spends fewer bits per band: more
aggressive coding, which at a fixed bitrate means those bits go somewhere else or the
frame simply finishes early. Below 256 the thresholds fall, more content is deemed
worth coding accurately, and the demand for bits rises.

Two things keep this knob from being as strong as its range suggests. It operates on
the log domain threshold copy, and part of its effect is undone downstream by the
minimum SNR logic and the 29 dB threshold reduction ceiling in the CBR quantiser. And
under CBR a global threshold change mostly reshuffles bits rather than changing their
number, because the rate loop still has to hit the target. `--minsnr-scale` works
on the line the hole avoidance clamps back to, which `--ath-scale` cannot move.

Where it earns its keep is at high bitrate, where you have surplus bits and want the
encoder to find more things worth coding. Values a little below 256 convert that
surplus into preserved detail. At low bitrate the useful direction is usually upward, as
a way of accepting coarseness deliberately rather than having the rate loop decide where
to cut.

Examples:
- Mild, cleaner: `--ath-scale 230`. Thresholds about 10 percent lower. Slightly more
  bits to quiet detail; subtle at any bitrate, most visible above 192 kbps.
- Strong, cleaner: `--ath-scale 180`. A clear shift of effort towards low level
  content. Reverb tails and room noise survive better; at a tight bitrate this starves
  something else.
- Exaggerated, cleaner: `--ath-scale 64`. Thresholds a quarter of stock. The model is
  effectively told almost nothing is masked. Under CBR the rate loop fights back all
  frame long and the result is not better, just differently compromised; worth hearing
  once to understand where the limit sits.
- Mild, coarser: `--ath-scale 290`. A little more noise accepted per band, a small bit
  saving.
- Strong, coarser: `--ath-scale 512`. Thresholds doubled. Audibly grainier on
  broadband material, noticeably cheaper per band.
- Exaggerated, coarser: `--ath-scale 2048`. Thresholds eight times stock. The encoder
  believes almost everything is masked, and the result is a caricature of aggressive
  coding: hollow, hissy, with detail gone wherever anything louder sits nearby.

Works with / against: `--minsnr-scale` is the stronger tool for the same goal and
partly overrides the effect of this one, since the SNR demand is a floor on quality
that a raised threshold cannot push through. `--reduce-clamp 0` lets thresholds be
pushed deeper than the 29 dB ceiling, which gives low values here more room to act.
`--spread-mask` is the companion knob for the width rather than the height of masking.

Measured (acoustic stereo music): CBR 96 kbps, `128` lowers the overall error by 0.04 dB
and `384` raises it by 0.04 dB; at 256 kbps `190` lowers it by 0.2 dB with the file size
unchanged. VBR `-m 3` (about 108 kbps): `128` makes the file 5.5% larger and the error
0.8 dB lower, `192` 2.3% and 0.3 dB, `384` 3.3% smaller and 0.5 dB higher, `512` 5.6%
smaller and 0.8 dB higher.

#### `--minsnr-scale <n>`

**Scales the signal to noise ratio the coder is required to deliver in each band; the
most direct knob for detail in this group.**

- Applies to: every profile with an AAC core.
- Values: Q8, `1` and up with no documented ceiling, where `256` is x1.0; default:
  `-1`, off
- Input: whole number

Besides the masking threshold, each band carries a required coding SNR. It is the
guaranteed minimum: whatever the threshold computation says, the coder must reach this
quality in that band. FDK derives it from its tuning tables, and it is the nearest
equivalent to the tone masking noise and noise masking tone constants that MusePack
exposes directly.

Below 256 demands a higher SNR than stock, so more detail and more bits per band. Above
256 demands less and the result is coarser. Note the inversion against your intuition:
you are scaling the permitted noise, not the demanded quality, so smaller is cleaner.

The difference from `--ath-scale` is where the change lands. The hole avoidance logic
clamps thresholds back to the minimum SNR line, so the SNR demand is a floor the rate loop
may not breach, while a moved threshold can be pulled back. In measurement the two cost
about the same per dB gained under VBR; `--minsnr-scale` puts more of the gain into the
low band, `--ath-scale` spreads it more evenly.

Examples:
- Mild, more detail: `--minsnr-scale 220`. A modest lift in required quality per band.
  Visible at medium bitrate as fewer bands dropping out.
- Strong, more detail: `--minsnr-scale 150`. Clearly higher demand. Combined with a
  generous bitrate, this is the knob that turns surplus bits into preserved detail; at
  a tight bitrate it forces the rate loop to make the cut somewhere else.
- Exaggerated, more detail: `--minsnr-scale 16`. A sixteen fold demand. The frame can
  never satisfy it and the rate loop spends the whole frame in rescue mode, so the
  result is worse than stock; instructive once, not a setting.
- Mild, coarser: `--minsnr-scale 300`. A little less demanded per band, a small saving.
- Strong, coarser: `--minsnr-scale 512`. Half the demanded quality. Audibly grainier,
  with a real bit saving that can be redirected with other switches.
- Exaggerated, coarser: `--minsnr-scale 4096`. The demand is effectively removed, the
  floor under quality is gone, and bands collapse freely. A caricature of what the SNR
  demand was protecting.

Works with / against: the result is held inside the window set by
`--minsnr-clamp-lo` (the finest demand, about 25 dB in stock) and `--minsnr-clamp-hi`
(the coarsest, about 1 dB). Pushing far below 256 runs into the fine end, which
`--minsnr-clamp-lo` below 256 moves further. `--reduce-clamp 0` removes the 29 dB
reduction ceiling of the CBR rate loop.

Measured: under CBR the rate loop takes most of it back: at 96 kbps on acoustic music
`150` moves the low band 0.8 dB cleaner and the top 0.05 dB coarser, the total by
hundredths. Under VBR `-m 3` it works as a quality setting: `150` makes the file 1.3%
larger and the error 0.3 dB lower, `64` 7.2% larger and 1.1 dB lower, `512` 0.7% smaller
and 0.3 dB higher.

#### `--spread-mask <n>`

**Scales how far masking leaks from one band into its neighbours, which decides how
many quiet bands next to loud ones survive.**

- Applies to: every profile with an AAC core. In our measurements the effect was small at
  every bitrate tried, CBR and VBR alike.
- Values: Q8, `0` and up, where `256` is x1.0; default: `-1`, off
- Input: whole number

A loud tone does not only mask noise in its own band, it masks a skirt of bands around
it, more above than below. The model simulates that with a spreading function. The
spread is the reason a quiet band sitting next to a loud one gets almost no bits: the
model says its content is covered by the neighbour.

This switch scales the spreading. Below 256 means less masking spread, so the skirt
shrinks, fewer bands count as covered, and more of them get coded. That is the same idea
as relaxing the tone masks noise assumption in other encoders. Above 256 widens the
skirt and more bands are written off as covered by their neighbours, which is cheaper
and flatter.

In theory the spread decides most where bits are tight but not desperate: far below, the
whole frame is starved and the rate loop cuts everywhere; far above, there are bits for
everything. On our material, however, the spread decided very few bands at any bitrate,
see Measured.

Examples:
- Mild, less masking: `--spread-mask 230`. A slightly narrower skirt. A few more bands
  survive beside loud content; more detail around strong partials.
- Strong, less masking: `--spread-mask 180`. Clearly more bands coded next to loud
  ones. On dense material this is where detail around transient energy is regained, at
  real bit cost.
- Exaggerated, less masking: `--spread-mask 32`. Spreading almost removed. The model
  loses most of its ability to decide what is covered, so bits scatter across bands
  that genuinely are inaudible; a caricature that shows what the spreading function
  buys you.
- Mild, more masking: `--spread-mask 300`. A touch wider skirt, a small saving.
- Strong, more masking: `--spread-mask 512`. Spread doubled. Noticeably flatter
  spectrum beside loud content, cheap, with quiet detail written off.
- Exaggerated, more masking: `--spread-mask 1536`. Almost everything near anything loud
  counts as masked. The result is hollow and smeared, and useful mainly as the opposite
  end of the demonstration.

Works with / against: combine with `--ath-scale` below 256 when you want both a lower
and a narrower mask; the two are independent and the effects add. `--minsnr-scale`
still bounds the result from below, so a very low spread cannot push any band below its
SNR demand.

Measured (acoustic stereo music): CBR 96 kbps, `128` and `384` both within 0.01 dB of
stock; on `dense48` at 40 kbps within 0.05 dB. VBR `-m 3`: `128` makes the file 0.4% larger,
`64` 0.6%, with the error unchanged within 0.01 dB. On our material the spread decides
very little; the card describes the mechanism, the measurement its size.

#### `--minsnr-clamp-hi <n>`

**Moves the coarsest SNR any band is held to: a guarantee that even the least demanding
band keeps some detail.**

- Applies to: every profile with an AAC core. Matters most under VBR.
- Values: Q8, `1..320`, where `256` is x1.0 (stock); `-1` off (default). `320` is the
  loosest possible limit, 0 dB.
- Input: whole number

Every band carries a required SNR from the tuning tables (see *SNR demand* above), and FDK
holds that requirement inside a window. This switch moves the coarse end of the window,
which in stock means that no band is ever allowed more noise than about 1 dB below its own
energy. It matters for bands the tables consider unimportant: they would otherwise be
written off almost entirely.

Below 256 the limit tightens, so every band is guaranteed more SNR and the least demanding
bands get more bits. Above 256 it loosens, up to 0 dB at `320`, where a band may be coded
with as much noise as signal. Under CBR the rate loop undoes most of either direction; the
measurable effect is under VBR, where the bit count follows.

Examples (VBR `-m 3`, acoustic stereo music, stock about 108 kbps):
- Mild, tighter: `--minsnr-clamp-hi 192`. File 0.3% larger, overall error 0.06 dB lower.
- Strong, tighter: `--minsnr-clamp-hi 128`. File 1.4% larger, overall error 0.2 dB lower,
  mostly above 4 kHz.
- Exaggerated, tighter: `--minsnr-clamp-hi 64`. File 5.9% larger, error 0.5 dB lower: the
  quiet bands bought back at the price of a higher bitrate.
- Looser: `--minsnr-clamp-hi 320`. The loosest limit; on our material it changed nothing
  measurable, because the tables seldom demand so little.

Works with / against: `--minsnr-scale` moves the demand inside the window, this switch and
`--minsnr-clamp-lo` move the window's ends. Under CBR expect differences of hundredths of
a dB.

Measured: CBR 96 kbps, acoustic stereo music: `128` moves the overall error by 0.06 dB,
in the wrong direction, because the rate loop pays for the quiet bands elsewhere. Values
above 320 are rejected: past 0 dB the internal fixed point number overflowed and
the encoder produced garbage requirements.

#### `--minsnr-clamp-lo <n>`

**Moves the finest SNR a band may demand: the limit that `--minsnr-scale` below 256 runs
into.**

- Applies to: every profile with an AAC core, when `--minsnr-scale` asks for more SNR than
  stock or when the tables already demand the most.
- Values: Q8, `1` and up, where `256` is x1.0 (stock); `-1` off (default)
- Input: whole number

The fine end of the same window: in stock no band may demand more than about 25 dB of
SNR, however important the tables think it is. That keeps one band from taking the whole
frame.

Below 256 the limit moves further, so a band may demand more than 25 dB. On its own that
changes little, because few bands reach the limit; it becomes useful with
`--minsnr-scale` well below 256, which pushes the demanding bands against it. Above 256 the
limit comes closer, the most demanding bands are capped sooner and their bits go to the
rest. Very large values bring the two ends of the window together and every band demands
the same, which is no longer a psychoacoustic model.

Examples (VBR `-m 3`, acoustic stereo music):
- Mild, finer: `--minsnr-clamp-lo 64`. Without `--minsnr-scale` no measurable change.
- Strong, finer: `--minsnr-scale 64 --minsnr-clamp-lo 64`. The low band (100 Hz..1 kHz)
  0.4 dB cleaner than with `--minsnr-scale 64` alone.
- Mild, coarser: `--minsnr-clamp-lo 1024`. File 0.2% smaller, error in the low band
  0.3 dB higher.
- Exaggerated, coarser: `--minsnr-clamp-lo 4096`. File 0.8% smaller, low band 1.4 dB
  worse: the bands the tables care about most are the ones that lose.

Works with / against: the partner of `--minsnr-scale` below 256. Group E's hole switches
work on the same problem from the other side, by deciding which bands may be cut to zero.

Measured: CBR 96 kbps, acoustic stereo music: `4096` raises the low band error by 1.1 dB
and the overall error by 0.03 dB; `64` alone changes nothing measurable.

#### `--reduce-clamp <0|1>`

**Removes the 29 dB ceiling on how far the CBR quantiser may push thresholds down,
letting the other detail switches act without hitting a wall.**

- Applies to: CBR, that is the default `-m 0` path. VBR uses a different code path and
  is unaffected.
- Values: `0` or `1`; default: `1`, the ceiling applies, as in stock
- Input: whole number, on/off

When the CBR rate loop needs more quality in a band, it reduces that band's threshold.
There is a ceiling on the total reduction, about 29 dB, which stops the loop from
chasing one demanding band indefinitely. For normal material the ceiling never binds. If
you are deliberately demanding much more precision than stock, with `--minsnr-scale`
well below 256 or `--ath-scale` well below 256, the ceiling is exactly what stops your
setting from producing a further effect.

Setting `0` drops the ceiling, so thresholds may be pushed deeper and more bits flow
into the demanding bands. On its own this changes little, because stock rarely reaches
29 dB of reduction. It is a companion switch, not a standalone one.

Values:
- `1`: the 29 dB reduction ceiling applies (stock, default).
- `0`: ceiling removed, thresholds may be pushed deeper.

Example in context:

    fdkaac -p 2 -b 256000 --minsnr-scale 150 --reduce-clamp 0 -o out.m4a in.wav

Works with / against: pair it with `--minsnr-scale` below 256 for extreme detail; it is
near pointless alone. No effect under VBR (`-m 1..5`).

#### `--mask-slope <dB>`

**Shifts how far below the frame average a band must sit before the encoder relaxes its
SNR requirement.**

- Applies to: every profile with an AAC core, both channels. Off by default.
- Values: `-24.0..50.0` dB, `0` = off (default); distinct results only in about -9.9..+9
- Input: decimal, up to 3 places, `.` or `,`

FDK carries a heuristic that relaxes the required SNR of a band whose energy lies far
below the average energy of the frame, more than about 10 dB below in stock. In plain
words the encoder spends less on very quiet bands, on the grounds that the loud ones mask
them. This switch shifts that 10 dB line: a positive value raises it, so fewer bands
count as negligible; a negative value lowers it, so more do.

Measured on music the effect is small. The heuristic touches only the quietest bands, and
the rate loop redistributes whatever it saves, so under CBR the decoded error moves by
hundredths of a dB. Under VBR the file grows or shrinks by a fraction of a percent. It is
a laboratory switch for checking what that heuristic contributes, not a tool for detail.
For quiet detail use `--minsnr-scale` under VBR or the hole switches of group E.

The accepted range is wider than the live one. Distinct results appear for about -9.9 to
+9; at -10 and below the result stops changing, +10 to +40 give one stream and 45 to 50
another.

Examples:
- Mild, protect quiet bands: `--mask-slope 2`. A few more quiet bands keep their
  requirement.
- Strong, protect quiet bands: `--mask-slope 6`. Under VBR the file grows 0.1% on acoustic
  music, the overall error drops 0.03 dB.
- Exaggerated, protect quiet bands: `--mask-slope 9`. The top of the live range; beyond it
  nothing changes.
- Mild, starve quiet bands: `--mask-slope -2`. A few more quiet bands relaxed.
- Strong, starve quiet bands: `--mask-slope -6`. Under CBR within 0.1 dB of stock.
- Exaggerated, starve quiet bands: `--mask-slope -9.5`. The bottom of the live range.

Works with / against: applies to mid and side alike. It does not touch the side
threshold, which is the business of `--side-bias` and `--side-knee`.

Measured: mix48 at 40 kbps gives distinct streams only for about -9.9..+9. Acoustic stereo
music at 96 kbps CBR (`+6`, `-6`) and `dense48` at 40 kbps (`+9`, `-9`): mid and side
within 0.1 dB of stock. VBR `-m 3`: `+9` and `-9` change the file by +0.3% and +0.6%, the
mid error by under 0.2 dB.

## E. Core transients, pre-echo and spectral holes

These thirteen switches reach the AAC core itself, under SBR. They matter most where the
core is short of bits: AAC-LC stereo at 24..50 kbps with the bandwidth forced well above
what FDK would pick (`-w 16000..20000`), and LD or ELD with a widened `--core-cutoff`.
At comfortable bitrates most of them change little, because the problems they address
hardly occur there.

Two different failures show up under bit pressure, and it helps to keep them apart.

**Pre-echo.** A long AAC block covers 2048 samples, about 43 ms at 48 kHz. The
quantisation noise of a block is spread over the whole block in time. When a sharp attack
sits late in the block, that noise also lands before the attack, where the ear has almost
nothing to mask it: backward masking lasts only a few milliseconds. You hear it as a soft
smear or a "pre-shadow" in front of castanets, hand claps, plucked strings or a piano hit.
The encoder has four tools against it, and this group exposes all four:

1. the attack detector, which switches the frame to eight short blocks of 256 samples
   (`--attack-decay`, `--attack-floor`, together with `--block-bias` in group F);
2. the grouping of those eight short windows into scalefactor groups (`--short-groups`);
3. the window shape, which decides how much noise leaks away from where it belongs
   (`--window-shape`);
4. for the cases that stay in long blocks, TNS (temporal noise shaping, which shapes the
   noise in time inside a block: `--tns-thresh`, `--tns-start`) and the pre-echo control,
   which limits how fast a masking threshold may grow from one frame to the next
   (`--preecho-rise`, `--preecho-depth`).

AAC-LD and AAC-ELD have no short blocks and a fixed window, so for them only TNS and the
pre-echo control are left. That is why those switches matter more there than in LC.

**Spectral holes.** When a frame cannot afford all the bands it is asked to code, FDK
first relaxes the SNR it demands from bands that are peaks or valleys of the spectrum,
and if that is still not enough, it cuts whole scalefactor bands to zero. A zeroed band is
silence in that frequency range for that frame. Over time this sounds like a bubbling,
flickering or "underwater" high end: bands come and go from frame to frame. FDK calls the
whole mechanism "avoid holes", which is somewhat ironic: it decides which holes to make.
The order is fixed: bands are cut in up to eight energy steps, quietest first, and only
from a starting band upwards, so that the bottom of the spectrum is protected. The switches
`--avoid-holes`, `--hole-start`, `--hole-start-short`, `--hole-spread` and `--hole-ladder`
change that mechanism.

There is no free lunch in either case. Under CBR the frame budget is fixed. A band that is
not cut costs bits that some other band no longer gets, and a short block costs more side
information than a long one. Every switch here moves bits around; none adds any.

All measurements quoted in this group were taken after decoding, on two synthetic signals
built for the purpose. `attacks48.wav` holds castanet-like clicks over a very quiet bed;
pre-echo is the error energy in the 1024 samples before each attack, relative to the
energy of the attack itself. `dense48.wav` holds dense wideband chords, bright noise and a
hi-hat; a hole is a 1/3-octave band between 2 and 16 kHz whose original energy is above
-70 dBFS and which comes out of the decoder at least 15 dB too low. The tools that produce
these numbers are in `tools/core/` in the repository, so the measurements can be repeated
on any material.

A starting point for AAC-LC stereo at 24..50 kbps with a wide band:

    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

### `--hole-ladder <1..8>`

**Limits how far the encoder may go when it cuts bands to zero, so that a starved frame
gets coarser instead of emptier.**

- Applies to: the AAC core in every profile (LC, the core of HE-AAC and HE-AAC v2, LD, ELD).
  Matters only when bits run out.
- Values: 1..8; default: off, which is stock behaviour (8 steps)
- Input: whole number

When a frame is over budget, FDK sorts the candidate bands by energy into eight steps and
cuts them step by step, quietest first, rechecking the budget after each step. With
eight steps, a hard frame can end up cutting quite loud bands. `--hole-ladder N` stops
after N steps. Only the quietest bands may then be cut. If the frame is still over budget,
the rate loop solves the rest the other way, by raising the quantisation step in all
bands: the result is more noise spread evenly instead of more bands missing.

This is the strongest switch against the bubbling top at low bitrate. The trade is real
but usually favourable: a slightly grainier fill in exchange for bands that stay present
from frame to frame. The ear is far more tolerant of steady noise than of bands that blink.

Examples (only one direction exists: fewer steps than stock):
- Mild: `--hole-ladder 6`. Only the loudest cut steps disappear. Hard frames lose a few
  fewer bands; easy frames are untouched.
- Strong: `--hole-ladder 2`. Only the two quietest steps may be cut. On dense material at
  32..50 kbps this removes most holes (see Measured).
- Exaggerated: `--hole-ladder 1`. Only the very quietest step may be cut, so nearly every
  band survives and the whole budget problem turns into coarser quantisation. On very
  dense material at 24 kbps this can sound uniformly gritty: a caricature of the trade.

Works with / against: combines well with `--avoid-holes 1` and `--hole-start`. The rate
loop can only raise the step so far; at extreme budgets bands still vanish because they
quantise to zero on their own, which no switch here prevents.

Measured (dense48, LC stereo, holes after decoding): 32 kbps / 17 kHz: 37% to 8% with `2`;
40 kbps / 18 kHz: 37% to 1.7%; 50 kbps / 19 kHz: 35% to 0.2%. The error-to-signal ratio in
the bands that remain gets 0.3..0.5 dB worse.

Under the hood: the limit on `enIdx` in the "allow more holes" loop of `adj_thr.cpp`.

### `--avoid-holes <0|1>`

**Forces FDK's peak and valley protection on or off.**

- Applies to: the AAC core in every profile.
- Values: `0` off, `1` on; default: off, which is stock behaviour (on from 20 kbps per
  channel, off below)
- Input: whole number, 0 or 1

Before cutting bands, FDK can relax the SNR it demands in a smarter way: it lowers the
demand in valleys between spectral peaks and keeps it in the peaks, so that narrow loud
bands survive and the savings come from where the ear notices them least. Stock enables
this only from 20 kbps per channel upwards, which for stereo means from 40 kbps. Below
that, right inside the 24..50 kbps range this group is aimed at, the protection is off
and cutting starts earlier.

Values:
- `1`: protection on at every bitrate. Below 40 kbps stereo this is the meaningful
  setting; at or above it nothing changes, because stock already has it on.
- `0`: protection off at every bitrate. A diagnostic setting: it shows what the
  protection was buying you.

Example in context: `-p 2 -b 32000 -w 17000 --avoid-holes 1 --hole-ladder 2`.

Works with / against: on its own the effect is small (measured), because the protection
only reorders where the savings come from; it helps most together with `--hole-ladder`.

Measured: at LC 40 and 128 kbps stereo `--avoid-holes 1` gives a stream identical to stock
(already on there), `--avoid-holes 0` changes it.

### `--hole-start <sfb>`

**Protects the lower part of the spectrum in long blocks from being cut to zero.**

- Applies to: the AAC core, long blocks (LC, the HE-AAC core, LD, ELD).
- Values: scalefactor band 0..51; default: off, which is stock behaviour (band 15 when the
  protection is on, band 0 when it is off)
- Input: whole number

Bands below this number are never chosen as holes. Stock starts at band 15, which at
48 kHz is roughly 2 to 3 kHz: the bass and the low mids are safe, everything above may be
cut. Raising the number moves the protected region up; lowering it allows holes deeper
down. Remember that a protected band still costs bits: the savings then have to come
from the bands above, or from coarser quantisation everywhere.

How many bands exist depends on profile and sample rate; `--verbose` prints the count as
`active SFBs (long)`, 49 for LC stereo at 48 kHz.

Examples, protecting more:
- Mild: `--hole-start 20`. A few more bands in the low mids are safe.
- Strong: `--hole-start 25`. Protection up to roughly the presence region; measured to
  cut holes from 37% to 26% at 32 kbps.
- Exaggerated: `--hole-start 51`. No band in a long block may be cut by this mechanism at
  all. The whole budget problem lands on the quantiser; at low bitrate the top gets very
  coarse, and bands can still vanish by quantising to zero.

Examples, allowing deeper holes:
- Mild: `--hole-start 12`. A little more freedom just below the stock start.
- Strong: `--hole-start 6`. Holes allowed down into the low mids.
- Exaggerated: `--hole-start 0`. Even the bass bands may be cut. Useful only to hear what
  the stock protection prevents.

Short blocks: without `--hole-start-short` the same frequency is used there (the
nearest short band edge; `--hole-start 25`, 5 kHz at 48 kHz, becomes short band 6, and
the stock pair 15/3 maps onto itself). `--verbose` shows it.

Works with / against: `--hole-start-short` sets the short value explicitly and wins.

### `--hole-start-short <sfb>`

**The same protection for short blocks.**

- Applies to: LC and the HE-AAC core, frames coded as short blocks. LD and ELD have none.
- Values: short-block scalefactor band 0..15; default: off, which is stock behaviour (band 3),
  or the frequency of `--hole-start` when that one is given
- Input: whole number

A short block has about 14 bands at 44.1 or 48 kHz, so each band is wide and the numbers
are small. Stock protects bands 0..2.

Examples, protecting more:
- Mild: `--hole-start-short 5`.
- Strong: `--hole-start-short 8`. Most of the band in a transient frame is protected.
- Exaggerated: `--hole-start-short 15`. No short band may be cut; transient frames get
  coarse instead.

Examples, allowing deeper holes:
- Mild: `--hole-start-short 2`.
- Strong: `--hole-start-short 1`.
- Exaggerated: `--hole-start-short 0`. The attack frame may lose its bass.

### `--hole-spread <dB>`

**Decides how readily a band counts as covered by its neighbours, and therefore as a safe
candidate for a hole.**

- Applies to: the AAC core in every profile.
- Values: 0.0..24.0 dB; default: off, which is stock behaviour (3 dB for long blocks,
  2 dB for short blocks)
- Input: decimal, up to 3 places, `.` or `,`

A band may be cut more safely when its neighbours mask it anyway. To judge that, FDK
compares the band's energy with the energy spread into it from neighbouring bands, after
lowering that spread energy by a margin. A small margin makes neighbours look strong, so
many bands look covered and become candidates. A large margin makes neighbours look weak,
so fewer bands look covered and more of them are protected.

This is a fine knob. On our material it moved the hole count by one or two percent of
bands, far less than `--hole-ladder`.

Examples, protecting more:
- Mild: `--hole-spread 4.5`.
- Strong: `--hole-spread 9`.
- Exaggerated: `--hole-spread 24`. Neighbours practically never count as covering, so
  almost no band qualifies on that ground.

Examples, allowing more holes:
- Mild: `--hole-spread 2`.
- Strong: `--hole-spread 1`.
- Exaggerated: `--hole-spread 0`. The spread energy is taken at face value; any band
  sitting next to louder ones is a candidate.

The value is for long blocks; short blocks get two thirds of it, which keeps the stock
ratio (3 dB long, 2 dB short). `--hole-spread 3` is therefore exactly stock in both. Up to
1.4.0-beta.1 one value replaced both.

### `--preecho-rise <dB>`

**Limits how fast the masking threshold of a band may grow from one frame to the next,
so that quantisation noise cannot run ahead of an attack in a long block.**

- Applies to: long blocks in every profile. Most relevant in LD and ELD, and in LC with
  long blocks forced (`--block-bias 0`).
- Values: 0.0..24.0 dB; default: off, which is stock behaviour (3.01 dB, a factor of 2 in
  power)
- Input: decimal, up to 3 places, `.` or `,`

The psychoacoustic model computes a masking threshold per band and frame. In a frame that
contains an attack, the threshold jumps up, because the attack masks a lot. But the noise
of that frame also sits before the attack, where nothing masks it. Pre-echo control
therefore caps the rise: this frame's threshold may be at most N dB above the previous
frame's. Below 3.01 the cap is tighter (less pre-echo, more bits on onsets); above it the
cap is looser (fewer bits, more pre-echo).

In LC the effect is usually small, because real attacks are already moved into short
blocks before this cap matters. LD and ELD have no short blocks, so there this switch and
TNS are the main defence.

Examples, tighter:
- Mild: `--preecho-rise 2`.
- Strong: `--preecho-rise 1`. Onsets in long blocks get noticeably more bits.
- Exaggerated: `--preecho-rise 0`. A threshold may never rise at all from frame to frame.
  Every crescendo is coded as if it were still quiet, which wastes bits on loud passages
  and starves everything else under CBR.

Examples, looser:
- Mild: `--preecho-rise 4.5`.
- Strong: `--preecho-rise 9`.
- Exaggerated: `--preecho-rise 24`. The cap is practically gone: pre-echo control is off.

Works with / against: `--preecho-depth` limits the opposite side of the same rule.

Measured: tenths of a dB of pre-echo energy in LC on attacks48 (the attacks already go to
short blocks). The switch changes LD and ELD streams.

### `--preecho-depth <dB>`

**Limits how far pre-echo control may lower a threshold at most.**

- Applies to: the same as `--preecho-rise`.
- Values: 0.0..60.0 dB (the effect stops growing near 45 dB); default: off, which is stock
  behaviour (20 dB)
- Input: decimal, up to 3 places, `.` or `,`

When the rise cap pulls a threshold down, this switch sets the floor: the threshold may
not end up more than N dB below what the psychoacoustic model computed for the frame. It
keeps the cap from demanding absurd precision after a long silence, when the previous
frame's threshold was tiny.

Examples, allowing deeper lowering:
- Mild: `--preecho-depth 25`.
- Strong: `--preecho-depth 40`. After silence, the first attack is coded very cleanly
  and very expensively.
- Exaggerated: `--preecho-depth 60`. Practically no floor; under CBR a single onset after
  a pause can eat the reservoir. The internal factor bottoms out near 45 dB, so values
  between about 45 and 60 behave the same.

Examples, shallower:
- Mild: `--preecho-depth 15`.
- Strong: `--preecho-depth 6`.
- Exaggerated: `--preecho-depth 0`. A threshold may not be lowered at all, which switches
  pre-echo control off regardless of `--preecho-rise`.

### `--attack-decay <x>`

**Sets the memory of the attack detector that chooses between long and short blocks.**

- Applies to: LC and the HE-AAC core (the profiles with short blocks). In our test it also
  had no effect on LD or ELD.
- Values: 0.0..0.99; default: off, which is stock behaviour (0.70)
- Input: decimal, up to 3 places, `.` or `,`

The detector splits each frame into sub-windows, filters out the low frequencies, and
compares the energy of each sub-window with a running average of what came before. A
large enough ratio is an attack. The running average keeps a share of its value per
sub-window: that share is this switch. Higher values mean a long memory, so an attack is
compared with a long average; lower values mean the comparison is with the last moment.

A long memory is calmer and more selective on dense material, but after a loud passage
the average stays high and an attack that follows can be missed. A short memory catches
attacks right after loud passages, but on busy material it can fire too often and put
tonal frames into short blocks, which costs bits and resolution.

Examples, longer memory:
- Mild: `--attack-decay 0.8`.
- Strong: `--attack-decay 0.9`. Measured: worst case pre-echo 2.9 dB lower on attacks48.
- Exaggerated: `--attack-decay 0.99`. The average barely moves; attacks after loud
  passages are often missed.

Examples, shorter memory:
- Mild: `--attack-decay 0.6`.
- Strong: `--attack-decay 0.4`.
- Exaggerated: `--attack-decay 0`. Each sub-window is compared only with the one before;
  the detector becomes jumpy on noisy material.

### `--attack-floor <dB>`

**Shifts the minimum energy an attack must have before it counts.**

- Applies to: LC and the HE-AAC core. In our test it also changed an LD stream, not ELD.
- Values: -60.0..30.0 dB; default: off, which is stock behaviour (0 dB shift)
- Input: decimal, up to 3 places, `.` or `,`

Besides the ratio, the detector requires an absolute minimum energy, so that ticks in
near-silence do not trigger short blocks. Negative values lower that floor: quiet attacks
such as brushes, soft hi-hats or a quiet guitar pick also switch to short blocks.
Positive values raise it: only loud attacks count.

Examples, catching quieter attacks:
- Mild: `--attack-floor -6`.
- Strong: `--attack-floor -20`.
- Exaggerated: `--attack-floor -60`. Practically any rise in a quiet passage counts. Expect
  short blocks on room noise and on soft tonal passages, with the bit cost that brings.

Examples, only loud attacks:
- Mild: `--attack-floor 6`.
- Strong: `--attack-floor 15`.
- Exaggerated: `--attack-floor 30`. Soft percussion stays in long blocks and pre-echo
  appears on it.

### `--window-shape <0|1>`

**Chooses the window shape of every block: sine everywhere or KBD everywhere.**

- Applies to: AAC-LC and the LC core of HE-AAC and HE-AAC v2. LD and ELD use a fixed
  window and are not affected.
- Values: `0` sine, `1` KBD; default: off, which is stock behaviour (KBD for long blocks,
  sine for short blocks)
- Input: whole number, 0 or 1

The window decides how the energy of a block leaks into neighbouring frequencies. A sine
window has a narrow main lobe (good selectivity next to a peak) and slowly falling side
lobes (energy and noise leak far). A KBD window has a wider main lobe and side lobes that
fall much faster. In time the same property means that KBD keeps quantisation noise
closer to where it belongs. Stock uses sine for short blocks, which are exactly the
blocks around attacks.

The choice costs one bit per frame and is legal everywhere in LC; the stream carries the
shape actually used, also in short blocks, and every decoder applies it.

Values:
- `1`: KBD in every block. Measured: pre-echo lower by 3.4..6.1 dB on average and by
  1.2..6.6 dB in the worst case at 24, 40 and 128 kbps. The best single switch against
  pre-echo in LC.
- `0`: sine in every block. Better selectivity next to strong tonal peaks in long blocks,
  more leakage; mainly a comparison setting.

Example in context: `-p 2 -b 40000 -w 18000 --window-shape 1`.

### `--short-groups <0..3>`

**Chooses how the eight short windows of a transient frame are grouped.**

- Applies to: LC and the HE-AAC core, frames coded as short blocks.
- Values: `0`..`3`; default: off, which is stock behaviour (the table FDK picks by attack
  position)
- Input: whole number

A transient frame consists of eight short windows. Windows in the same group share one set
of scalefactors, so fewer groups means less side information and more bits for the
spectrum, but coarser control of the noise in time. More groups means the noise can be
shaped window by window, at a price.

Values:
- `0`: the stock table (the same as leaving the switch off).
- `1`: one group for all eight windows. The cheapest; the noise is spread over the whole
  frame, which partly undoes what the short blocks were for.
- `2`: four pairs. Even time resolution across the frame. Measured: the fewest holes in
  short frames on our material.
- `3`: tight around the attack: the attack window alone, one window on each side of it,
  the rest lumped. The most bits where the attack is.

Example in context: `-p 2 -b 32000 -w 17000 --short-groups 2`.

### `--tns-thresh <x>`

**Changes how readily TNS switches on.**

- Applies to: every profile with a core: LC, the HE-AAC core, LD, ELD.
- Values: 0.5..4.0, a multiplier; default: off, which is stock behaviour (1.0)
- Input: decimal, up to 3 places, `.` or `,`

TNS (temporal noise shaping) runs a prediction filter across frequency. Where the signal
has a strong time envelope, such as an attack inside a block, the filter makes the
quantisation noise follow that envelope in time, so the noise sits under the loud part
instead of in front of it. FDK enables TNS in a frame only when the prediction gain is
high enough to be worth the side information. This switch multiplies that required gain.
Below 1 TNS is used in more frames; above 1 in fewer.

In LC, short blocks already handle the sharpest attacks, so TNS covers the softer ones.
In LD and ELD, which have no short blocks, TNS is the main tool against pre-echo, and a
lower threshold there is often worth trying first.

Examples, more TNS:
- Mild: `--tns-thresh 0.85`.
- Strong: `--tns-thresh 0.7`. Measured: in LC at low bitrate the extra side information
  leaves more holes.
- Exaggerated: `--tns-thresh 0.5`. TNS on almost everywhere, also where there is nothing
  to shape; bits go to filter coefficients.

Examples, less TNS:
- Mild: `--tns-thresh 1.3`.
- Strong: `--tns-thresh 2`. Fewer bits on side information, slightly fewer holes, more
  pre-echo on soft attacks.
- Exaggerated: `--tns-thresh 4`. TNS practically off.

Works with / against: `--tns-mask` and `--tns-order` (group D) decide whether and how
large the filter may be; this switch decides how often it is worth it.

### `--tns-start <Hz>`

**Sets the lowest frequency the lower TNS filter covers.**

- Applies to: AAC-LC and the LC core of HE-AAC, long and short blocks. Measured no effect
  in LD and ELD.
- Values: 0..24000 Hz; default: off, which is stock behaviour (8 scalefactor bands, about
  1.4 kHz at 48 kHz)
- Input: whole number

Stock keeps the lowest bands out of TNS in long blocks and starts at 0 Hz in short ones.
The value is rounded to the nearest scalefactor band edge of each table, and it cannot go
above the top of the TNS range. Up to 1.4.0-beta.1 short blocks ignored it.

Examples, reaching lower:
- Mild: `--tns-start 1000`.
- Strong: `--tns-start 400`. TNS also shapes the noise of low mids, useful on piano and
  plucked bass notes.
- Exaggerated: `--tns-start 0`. TNS covers the spectrum from the bottom, including the
  bass bands, where its side benefit is small and its errors can show as low frequency
  "breathing".

Examples, starting higher:
- Mild: `--tns-start 2000`.
- Strong: `--tns-start 5000`.
- Exaggerated: `--tns-start 15000`. Only the very top is shaped; for most material TNS is
  then effectively off in long blocks.

### `--coverage <-5..5>`

**One axis at a fixed bitrate: code every band coarsely, or fewer bands cleanly.**

- Applies to: AAC core (LC, LD, ELD and the LC core of HE-AAC). Off by default.
- Values: whole numbers `-5..5`; `0` = stock
- Input: whole number

At a fixed bitrate the encoder can spend its bits on covering the whole spectrum with a
coarse description, or on describing fewer bands well and leaving holes. This macro moves
that balance with one number. It sets the six switches below (`--band-knee`,
`--quant-round`, `--thr-hold`, `--scf-range` and, at `+4` and `+5`, `--pns-above`); any of
them given explicitly wins over the macro. `--verbose` prints the values in effect.

Measured at LC 32 kbps, 17 kHz, after decoding (zeroed bands, and noise of the bands that
were kept): `-5` 51% holes with the kept bands 4.75 dB cleaner than the signal; stock 11.7%
holes, 1.96 dB; `+4` 0.2% holes, with the kept bands about as noisy as the content.

Examples:
- Mild, more coverage: `--coverage 2`. Fewer holes, a little more grain.
- Strong, more coverage: `--coverage 4`. Practically no holes; adds noise substitution from
  11 kHz up.
- Exaggerated, more coverage: `--coverage 5`. Everything filled, noise from 7 kHz up.
- Mild, more precision: `--coverage -2`. The weakest bands leave first.
- Strong, more precision: `--coverage -4`. Many holes, what stays is clean.
- Exaggerated, more precision: `--coverage -5`. About half of the bands gone.

### `--band-knee <dB>`

**Shift every band's margin above its threshold near the coded/zeroed edge, in every channel.**

- Applies to: AAC core. Off by default.
- Values: `-24.0..24.0` dB, `0` = off
- Input: decimal, up to 3 places, `.` or `,`

A positive value keeps bands up to N dB below their threshold, at the coarsest step,
instead of zeroing them. A negative value zeroes bands that clear their threshold by less
than N dB after the first bit estimate, so the second estimate gives those bits to the
rest. PNS and IS bands are left alone; on side bands of an MS pair `--side-knee` wins.

Examples:
- Mild, keep more: `--band-knee 3`.
- Strong, keep more: `--band-knee 6`. The useful maximum: above about 9 dB the rescued
  coarse bands cost so many bits that the rate loop cuts elsewhere.
- Exaggerated, keep more: `--band-knee 24`. Worse than 6, measured.
- Mild, drop more: `--band-knee -2`.
- Strong, drop more: `--band-knee -6`.
- Exaggerated, drop more: `--band-knee -24`. Only clearly audible bands stay.

### `--quant-round <x>`

**Move the rounding point of the quantizer: fewer or more small spectral lines become zero.**

- Applies to: AAC core. Off by default (stock 0.405).
- Values: `0.0..0.5`
- Input: decimal, up to 3 places, `.` or `,`

The quantizer rounds each spectral line to a whole step. Stock rounds with an offset of
about 0.405, slightly towards zero, which saves bits. `0.5` rounds to nearest: fewer small
lines vanish, it costs bits. Lower values send more lines to zero and are cheaper.

Examples:
- Mild, finer: `--quant-round 0.45`.
- Strong, finer: `--quant-round 0.5`. Round to nearest.
- Mild, cheaper: `--quant-round 0.35`.
- Strong, cheaper: `--quant-round 0.25`. Quiet details disappear.
- Exaggerated, cheaper: `--quant-round 0.0`. Truncation: every line is rounded down.

### `--pns-above <Hz>`

**From this frequency up, replace every long-block band by noise of the right level, whatever its tonality.**

- Applies to: AAC-LC, long blocks. Off by default.
- Values: `1000..48000` Hz
- Input: whole number

A high band "like SBR, without SBR": the band keeps its level, its content becomes noise.
It turns PNS on even below its bitrate gate; `--pns 0` still wins. Silent bands stay
silent. At low bitrates use it with `--pns-budget 1` and `--pns-shared 1`, otherwise
frames can run out of bits and crackle (see `--pns-budget`).

Examples:
- Mild: `--pns-above 11000`. Only the very top becomes noise.
- Strong: `--pns-above 5000`.
- Exaggerated: `--pns-above 2200`. Most of the presence region is noise.

### `--thr-hold <n>`

**Re-measure the masking model only every n long frames.**

- Applies to: AAC core, long blocks. Off by default.
- Values: `1..32` frames, `1` = off
- Input: whole number

Between measurements each band keeps its last noise-to-signal offset, so the noise lags the
music. A short block resets the hold. The effect is weak: the stream changes, but measured
quality moves by less than 1 dB even at 32, because FDK already works on smoothed energies.

Examples:
- Mild: `--thr-hold 2`.
- Strong: `--thr-hold 8`.
- Exaggerated: `--thr-hold 32`.

### `--scf-range <n>`

**Limit how far apart the finest and the coarsest band of a channel may be.**

- Applies to: AAC core. Off by default (stock 60).
- Values: `0..60`, steps of 1.5 dB
- Input: whole number

Lower values make the quantization noise flatter across frequency, with less
psychoacoustic shaping. On ordinary material the effect is close to zero; it shows at
small values on material with a strong spectral tilt (6 to 14 kHz noise 12.4 to 7.1 dB
below the signal at `0`, 64 kbps without PNS). At 14 kbps it does not change the budget.

Examples:
- Mild: `--scf-range 40`.
- Strong: `--scf-range 20`.
- Exaggerated: `--scf-range 0`. One step for the whole channel.

### `--bit-stats`

**Count where the bits of the written frames go, and print the table at the end.**

- Applies to: every profile. Off by default.
- Values: none, a flag

The table splits the frames into transport header, element headers, MS mask, section data,
scalefactors, PNS noise levels, TNS, spectrum per channel (mid and side for MS), SBR/PS and
fill, plus coded, zeroed, PNS and IS bands per frame. It counts the bits really written; the
stream is identical with and without the switch, and the sum matches the ADTS file.

Example: `fdkaac -p 2 -b 14000 --bit-stats -f 2 -o out.aac in.wav`

### `--enfac-patch <0|1>`

**Switch the energy re-weighting of flat, noise-like long frames off, or on for every frame.**

- Applies to: AAC core, long blocks, CBR and VBR. Off by default (stock decides).
- Values: `0` = never, `1` = every long frame

When the audible spectrum of a frame is flat, stock FDK re-weights band energies (powers
3/4, 1/2 or 1/4) and so moves bits between bands. `0` treats every frame as tonal, `1`
applies the re-weighting to every long frame. In CBR this is a fine knob: zeroed bands
move by at most 1 point and noise by less than 0.1 dB at 14 to 32 kbps; `0` is often
identical to stock on tonal material. In VBR (`-m 3`) `1` made a tone plus noise file 29%
larger and `0` a dense noise file 3% smaller.

## F. Block switching and bitrate control

Six switches: one that biases the long/short block decision, and five that control how
freely the bitrate may move from frame to frame. The bitrate five only make sense once
you know how the engine underneath works, so that comes first.

**The CBR engine.** You select a target with `-b` and the encoder aims at it as an
average. Frames are not individually forced to that size. At 48 kHz an AAC-LC frame
covers 1024 samples, about 21.3 ms, so a 128 kbps stereo target works out to roughly
2730 bits per frame on average. That is the number the rate loop steers towards.

**The bit reservoir.** Even in CBR, a buffer sits between the frames and the target.
When a frame is easy, the encoder codes it sparingly and puts the unspent bits into the
reservoir. When a hard frame arrives, it borrows from the reservoir and spends more than
its share. The average is preserved while the instantaneous bitrate moves up and down,
which is why CBR AAC is never perfectly flat. The reservoir size decides how far the
encoder can stray and for how long.

**The hard ceiling.** An AAC frame holds at most 6144 bits per channel, so 12288 bits
for stereo. Nothing in this encoder lifts that; it is a limit of the format. Every
reservoir and peak setting is clamped against it, which is also why the swings available
here are moderate compared with a true variable bitrate encoder.

**Quasi-VBR.** The problem these switches address is specific and you have probably
heard it: a difficult passage arrives, the encoder releases a few frames well above the
average, and then it chokes the following frames to pay the debt back immediately. The
result is an audible dimming right after a loud moment. Widening the reservoir and
allowing a short term peak lets the encoder hold the elevated quality longer and come
back down gently instead. It is still the CBR engine with a bigger buffer, not content
based allocation like LAME's, so expect a lighter touch than the name VBR suggests.

A note on the two hard per frame limits: do not set `--max-bits-frame` below the average
or `--min-bits-frame` above it. Both of those fight the target the rate loop is trying
to hit, and the result is worse quality rather than a different tradeoff. `--verbose`
prints the reservoir budget block, including the average bits per frame and the
ceilings, which is where you get the numbers to set these switches from.

### `--block-bias <n>`

**Sets the share of short blocks, from long blocks only (0) to short blocks only (255).
The stream stays standards compliant at every value.**

- Applies to: AAC-LC and the LC core of HE-AAC, which have short blocks. LD and ELD have
  none (LD switches to a low-overlap window instead, also moved by this switch; ELD has
  no switching at all).
- Values: `0..255`, `128` is stock; default: `-1`, off (stock)
- Input: whole number

A long block gives good frequency resolution and bad time resolution; a short block the
reverse. The encoder's attack detector decides frame by frame: it compares the energy of
each eighth of the frame with the running energy before it and calls an attack above a
ratio of 10. On music that ratio is 1.2 to 1.5 in an ordinary frame and above 10 in only
about 2..3% of frames.

This switch moves that threshold along a table calibrated on the encoder itself (six
excerpts of acoustic stereo, eurodance, vocals and drums at 64 kbps, share of short frames
read from the bitstream), so that the share of short frames follows the slider evenly:
- `0`: long blocks only, no attack is ever detected.
- `1..127`: fewer short frames, down from stock; at `64` about half of the stock share.
- `128`: stock, bit for bit.
- `129..240`: more short frames, about 12% of the frames per 16 steps on music; around
  `192` about half the frames, at `240` about 80..99%.
- `241..254`: also quiet passages go short (the detector's minimum energy is lowered).
- `255`: short blocks only, every frame, silence included.

Material without transients reacts later than music: steady pink noise and a sine stay
long up to about 208 and go short between 216 and 236, because their ratio stays near 1.
Material with hard hits only (a drum stem) keeps its hits at every value, so its share
moves less. Up to 1.4.0-beta.1 the switch scaled the threshold linearly: above 128 it
only went from 10 to 5 (6.5% short at 255 on eurodance) and just below 128 it jumped past
most real attacks, which is why the middle seemed to do nothing.

Only the attack decision changes. The window sequence is still built by the encoder's
own transitions (long, start, short, stop, long) and both channels are synchronised as
stock, so a decoder always gets a legal sequence. Checked: 300 encodes (0 to 255, LC,
HE-AAC, HE-AAC v2, short grouping 1 and 3, silence, sine, music), 0 illegal window
transitions and 0 decoder errors. `--verbose` shows the value.

Examples (eurodance, 64 kbps; share of short frames):
- `--block-bias 64`: 0.1% instead of 1.6%. Steady material stays long, hard hits still go
  short.
- `--block-bias 160`: 23%. Every larger rise in level gets short blocks.
- `--block-bias 192`: 57%.
- `--block-bias 224`: 87%.
- `--block-bias 255`: 100%. Tonal material sounds rough, a demonstration of what long
  blocks are for.
- `--block-bias 0`: 0%. Every transient is carried by a long block, pre-echo becomes the
  main artefact on percussive material.

Works with / against: with `--block-bias 0`, `--preecho-rise`, `--preecho-depth` and
`--tns-thresh` from group E are the tools left against pre-echo, the situation LD and ELD
are always in. `--short-groups`, `--window-shape` and `--pns-short` only matter where
short blocks occur. `--attack-decay` and `--attack-floor` change the detector itself; they
combine with this switch.

### `--vbr-reservoir <bits>`

**Sets the size of the bit reservoir, which decides how far and how long a frame may
stray from the average.**

- Applies to: every profile, in CBR mode. This is the central quasi-VBR knob.
- Values: bits, from `1` up to `6144 * channels` minus the average bits per frame, and
  clamped there automatically; default: `-1`, off
- Input: whole number

The reservoir is the buffer the encoder borrows from and pays into. Its size is the
memory of the system: a small reservoir means every overspend has to be repaid almost
immediately, which produces the dimming right after a climax; a large one means the
encoder can stay above the average for several frames and then return gradually.

Bigger is therefore the direction for material that should keep living after a difficult
passage. Smaller holds the average more tightly from moment to moment, which is what you
want when a transport link has a narrow instantaneous budget. A good starting point is
two to three times whatever `--verbose` reports as the default.

Two behaviours to know. `0` is treated as unset and does nothing, so if you want tight
tracking of the average use a small positive value or `--bitres-mode 2`. And any value
at or above the ceiling, that is `6144 * channels` minus the average bits per frame,
collapses to the ceiling, so there is no way to overdo it and no benefit in trying;
asking for 100000 bits on a stereo file gets you the same stream as asking for the
ceiling exactly.

Read the numbers from `--verbose`, which prints the average bits per frame and the
ceilings in its reservoir budget block. Setting this switch from a guess is how people
end up at the ceiling by accident.

Examples:
- Mild, more breathing: `--vbr-reservoir 4000`. Slightly wider swings at 128 kbps
  stereo; the encoder recovers from a peak over a few frames rather than one.
- Strong, more breathing: `--vbr-reservoir 12000`. The practical quasi-VBR setting at
  128 kbps stereo. Climaxes hold their quality and the return to average is gradual.
- Exaggerated, more breathing: `--vbr-reservoir 100000`. Far above the ceiling, so it
  collapses to the ceiling. The encoder uses the maximum breathing the format allows,
  and the caricature is instructive precisely because it shows how modest that maximum
  is: the 6144 bits per channel limit, not the reservoir, is what bounds AAC's flight.
- Mild, tighter: `--vbr-reservoir 2000`. Below the usual default. Swings narrow, the
  average is tracked more closely.
- Strong, tighter: `--vbr-reservoir 500`. Very little buffer. Hard frames cannot borrow
  much, so difficult passages are coded coarsely rather than expensively.
- Exaggerated, tighter: `--vbr-reservoir 1`. Effectively no reservoir. Every frame must
  fit its own share, so transients and dense passages have nowhere to go, which sounds
  like a worse version of what `--bitres-mode 2` does on purpose.

Works with / against: pairs with `--peak-bitrate`, which raises the short term ceiling
the reservoir can push against; the reservoir without a raised peak has less room to
act. `--bitres-mode` can reduce or disable the reservoir regardless of the size set
here. `--max-bits-frame` caps individual frames after all of this, so a tight cap makes
a large reservoir pointless. With `--unlock-bitrate` the reservoir becomes more
important, since overshoot is the normal case there.

Measured: `0` is treated as unset and produces no effect. Values at or above
`6144 * channels` minus the average bits per frame collapse to the ceiling.
`--verbose` prints the reservoir budget block that gives you both numbers.

### `--peak-bitrate <bps>`

**Allows short term bitrate peaks up to the value you give while the average is still
held at the `-b` target.**

- Applies to: every profile, in CBR mode.
- Values: bits per second, above the `-b` target; default: `-1`, off
- Input: whole number

The reservoir decides how long a frame may stray; this decides how high it may go. Set
it above your target and the encoder is allowed to let difficult frames reach that rate
momentarily, while the rate loop continues to steer the average towards `-b`.

Below the target it is ignored, which is deliberate: a peak ceiling under the average
makes no sense and the encoder does not act on the contradiction. The useful range is
therefore above `-b` only, and the natural way to express it is as a multiple: half
again the target is cautious, double is generous.

The ceiling of 6144 bits per channel still applies, so asking for a peak beyond what the
frame can physically hold gives you the frame limit instead. At 48 kHz stereo that is
about 576 kbps worth of frame (6144 x 2 bits x 48000 / 1024), so for ordinary targets you have plenty of room before
the format limit binds.

Examples:
- Mild, higher peak: `-b 128000 --peak-bitrate 150000`. A small amount of headroom.
  Climaxes come out slightly less compressed.
- Strong, higher peak: `-b 128000 --peak-bitrate 200000`. The practical setting, usually
  paired with a wide reservoir. Difficult frames reach well above the target and the
  average is kept.
- Exaggerated, higher peak: `-b 128000 --peak-bitrate 1100000`. Close to the format
  frame limit. The encoder may spend almost a whole frame's physical capacity on one
  frame, after which the rate loop must claw all of it back, so you get a dramatic
  version of the very dimming the quasi-VBR settings exist to prevent.
- Downward there is no useful direction: any value at or below the `-b` target is
  ignored, so `--peak-bitrate 100000` with `-b 128000` behaves as if the switch were
  absent.

Works with / against: the natural companion of `--vbr-reservoir`; both are needed for
the "do not choke after a peak" behaviour, since one grants height and the other grants
time. `--max-bits-frame` is applied after this switch, so an explicit frame cap wins
over the peak. `--bitres-mode 2` makes this irrelevant by removing the reservoir that
the peak would be drawn from.

Measured: the tip configuration in `--help` for quasi-VBR at about 128 kbps stereo is
`-b 128000 --peak-bitrate 200000 --vbr-reservoir 12000`.

### `--max-bits-frame <bits>`

**A hard ceiling on the number of bits any single frame may use.**

- Applies to: every profile, in CBR mode.
- Values: bits, at least the average bits per frame and at most `6144 * channels`;
  default: `-1`, off
- Input: whole number

This is the blunt instrument of the group. Where `--peak-bitrate` expresses a rate that
the engine may reach, this expresses an absolute per frame cap that is applied
afterwards and wins. If you give both, the explicit cap is what binds.

It is a constraint, not a quality knob, and its main use is meeting an external
requirement: a transport that cannot carry a frame above a certain size, or a test that
needs the maximum frame bounded. A reasonable cap is around one and a half times the
average, which still allows real breathing while bounding the worst case.

Set it too low and loud frames are starved, which is audible as exactly the dimming you
were presumably trying to avoid. Below the average it fights the target outright, and
the encoder cannot both honour your cap and hit the bitrate you asked for.

Examples:
- Mild, higher cap: at 128 kbps stereo (about 2730 bits per frame average),
  `--max-bits-frame 8000`. Well above anything the engine would normally produce, so
  effectively a safety net.
- Strong, higher cap: `--max-bits-frame 12288`. The stereo format maximum. The cap is
  present but never binds, which is the right setting when you want the switch
  documented in a command line without changing behaviour.
- Exaggerated, higher cap: `--max-bits-frame 100000`. Above the format limit, so it is
  not stored as is in the frame: the encoder takes the smaller of it and its own reservoir
  maximum (`6144 * channels`), so the stream is identical to `12288` in stereo (measured:
  12288, 20000 and 100000 give the same ADTS stream). Harmless, and a useful reminder that
  no switch here lifts the frame limit.
- Mild, lower cap: `--max-bits-frame 4000` at 128 kbps stereo. Peaks trimmed, average
  untouched; close to the useful 1.5x guideline.
- Strong, lower cap: `--max-bits-frame 3000` at the same target. Loud frames are
  clearly starved, since the cap sits barely above the average.
- Exaggerated, lower cap: `--max-bits-frame 2000` at 128 kbps stereo, below the average.
  The cap and the target now contradict each other every frame; quality drops across the
  board and nothing is gained, which is why the rule is to stay at or above the average.

Works with / against: applied after `--peak-bitrate`, so the explicit cap wins when
both are given. A tight cap makes a large `--vbr-reservoir` pointless, since the
reservoir's bits have nowhere to be spent. Read the average bits per frame from the
`--verbose` reservoir budget block before choosing a value.

Measured: AAC-LC 128 kbps on attacks48. Stock largest frame is 3816 payload bits. With
`--max-bits-frame 3000` the largest becomes 2888; with `4000` it becomes 3440.

### `--min-bits-frame <bits>`

**A hard floor on the number of bits every frame must use.**

- Applies to: every profile, in CBR mode.
- Values: bits, from `0` up to the average bits per frame; default: `-1`, off
- Input: whole number

The mirror of the ceiling, and the less useful half. A floor forces the encoder to spend
at least this much on every frame, including frames that genuinely need less, such as
silence or a sustained quiet passage. Those bits are not saved for later, they are spent,
so the floor converts easy frames into wasted budget.

The reason it exists is constant rate transport and experiments: some downstream
equipment prefers a predictable minimum frame, and sometimes you want to see what the
quantiser does when it is told to spend more than it wants.

Keep it at `0` unless you have such a reason. Above the average it fights the target the
same way a too low ceiling does, and the encoder cannot honour both.

Examples:
- Mild, higher floor: `--min-bits-frame 1000` at 128 kbps stereo. Silent and very easy
  frames get padded; the average has to be recovered from the harder frames.
- Strong, higher floor: `--min-bits-frame 2000` at the same target, close to the
  average. Almost every frame is now floored, so the engine loses most of its ability
  to redistribute.
- Exaggerated, higher floor: `--min-bits-frame 2730` at 128 kbps stereo, that is the
  average itself. Combined with the average as a target, this is close to forcing a
  rigid per frame rate, except that `--bitres-mode 2` does that properly and without
  fighting the rate loop.
- Mild, lower floor: `--min-bits-frame 200`. Barely a constraint; easy frames stay
  cheap.
- Strong, lower floor: `--min-bits-frame 1`. Effectively no floor, the same intent as
  leaving the switch off.
- Exaggerated, lower floor: `--min-bits-frame 0`. The documented off value for the
  floor; identical in effect to not setting it, and the setting to return to when an
  experiment is over.

Works with / against: the opposite end of `--max-bits-frame`, and the two are usually
discussed together as constrained VBR. A high floor wastes the bits that
`--vbr-reservoir` exists to redistribute, so the two work against each other.
`--bitres-mode 2` is the correct way to get rigid per frame behaviour.

Measured: AAC-LC 128 kbps on attacks48. Stock output 97056 bytes; with
`--min-bits-frame 1000` 97111 bytes; with `2900` 97709 bytes.

### `--bitres-mode <n>`

**Selects how much of the bit reservoir mechanism is active, from full breathing to a
rigid per frame bitrate.**

- Applies to: every profile, in CBR mode.
- Values: `0` full, `1` reduced, `2` disabled; default: `-1`, leave it to the encoder
- Input: whole number

Where `--vbr-reservoir` sets the size of the buffer, this selects the policy. Full is
the normal behaviour and the same as stock. Reduced narrows the reservoir's operation.
Disabled removes it, so each frame has to fit its own share of the budget, which is the
closest this encoder gets to a rigid frame by frame constant bitrate.

The one clearly useful setting is `2`, and the use case is a transport or a test that
needs each frame to be about the same size. The cost is real: a transient or a dense
passage cannot borrow, so it is coded coarsely instead.

Values:
- `-1`: leave it to the encoder (default).
- `0`: full reservoir, the same as stock behaviour.
- `1`: reduced reservoir. On our material this gave the same stream as `2`, so treat
  the distinction between the two as untested rather than established.
- `2`: reservoir disabled. Frames become near constant in size and difficult passages
  lose their escape route.

Example in context:

    fdkaac -p 2 -b 128000 --bitres-mode 2 -o rigid.m4a in.wav

Works with / against: `2` makes `--vbr-reservoir` and `--peak-bitrate` irrelevant,
since there is no reservoir for them to act on. `0` is the mode in which the rest of
group F is meant to be used.

Measured: AAC-LC 128 kbps on attacks48. `0` gives the stock stream; `1` and `2` gave
the same stream as each other, so no difference between reduced and disabled was
observed on that material.

## G. DAB+ output

Two switches, and a mode rather than a tweak. Everywhere else in this reference the
encoder makes a file or a bare AAC stream; with `--dab` it makes a DAB+ superframe
stream for terrestrial digital radio.

### `--dab`

**Emits a DAB+ superframe bitstream per ETSI TS 102 563 instead of a file or a plain
AAC stream.**

- Applies to: AAC-LC, HE-AAC and HE-AAC v2, mono or stereo, at 32000 or 48000 Hz.
  Replaces the normal output path entirely, so `-f` and the container options do not
  apply.
- Values: flag; default: off (normal file or stream output)
- Input: flag

DAB+ does not simply carry AAC in a different container. Three things differ. The
transform is 960 samples rather than the usual 1024, so the frame length fits the radio
timing grid. The audio is packed into superframes of exactly 120 ms. And because the air
is hostile, with fading, tunnels and interference, the bitstream is protected twice: a
firecode, which is a Fire CRC, guards the header, and Reed-Solomon RS(120,110) over
GF(256) protects the payload, so a receiver can rebuild data that arrived damaged. The
encoder does all of that.

The output is a raw `.dabp` stream, one superframe after another and nothing else. It is
not a file for a media player; it is what a DAB+ multiplexer ingests. The usual chain is
`out.dabp` into `odr-dabmux`, which combines several stations into one ensemble and emits
ETI, and from there to a transmitter or to a software receiver such as welle.io or
dablin for testing.

Three constraints the standard imposes and the encoder enforces: the sample rate must be
32000 or 48000 Hz, nothing else is legal in DAB+; the bitrate must be a multiple of
8 kbps between 8 and 192; mono and stereo are both fine.

The bitrate convention here is its own. With `--dab`, `-b` is kilobits per second and
the encoder multiplies by 1000 itself, so `-b 96` means 96 kbps. `--unlock-bitrate` does
not change that: the DAB+ check and the x1000 run first, so `--dab --unlock-bitrate -b 96`
is the same stream as without the unlock, and `-b 96000` is rejected as not a multiple
of 8 kbps.

You normally do not choose the profile. DAB+ practice, following the reference tool
`odr-audioenc`, derives it from bitrate and channel count so that low bitrates
automatically get the more economical tools:

- stereo at 48 kbps or below, that is a subchannel size of 6 or less, gives HE-AAC v2
  with parametric stereo;
- mono up to 64 kbps, or stereo up to 80 kbps, gives HE-AAC with SBR;
- anything higher gives AAC-LC.

So `-b 96` on a stereo file lands on AAC-LC, `-b 64` on HE-AAC and `-b 32` on HE-AAC v2
without you thinking about it. You can still force the profile with `-p` (`2` for LC,
`5` for HE-AAC, `29` for HE-AAC v2) when you know what you want. A forced profile gives
the same stream as the automatic pick would for that profile, bit for bit. Other `-p`
values are refused, and so is `-p 29` on a mono input. A forced profile below the FDK
tuning tables, for example `-p 2` at 8 kbps or `-p 5` at 8 kbps stereo, stops with a
note: leave out `-p` or add `--unlock-bitrate`.

Values:
- omitted: normal output, and the encoder behaves exactly as stock.
- `--dab`: DAB+ superframe output.

Examples in context:

    fdkaac --dab -b 96 -o out.dabp input.wav     # auto AAC-LC
    fdkaac --dab -b 64 -o out.dabp input.wav     # auto HE-AAC
    fdkaac --dab -b 32 -o out.dabp input.wav     # auto HE-AAC v2

Works with / against: `--dab-label` only does something in this mode. `-f` and the MP4
container switches are inapplicable, since there is no container. `--unlock-bitrate`
changes how `-b` is read. The switches of groups B to F still act on the AAC core
inside the superframes, so the stereo, SBR, detail, transient, coverage and bitrate knobs
remain available. Frequencies given in Hz are converted on the 960 sample grid, so
`--verbose` shows slightly different band edges than in a 1024 sample stream. Measured
on music and on an attack test, 48 and 96 kbps LC, 64 kbps HE-AAC stereo, 48 kbps HE-AAC
mono and 32 kbps HE-AAC v2: 58 settings of the switches added since v1.3.2 and of the
band switches, each stream decoded by faad2 behind dablin without an error. A switch
whose tool is not in the chosen profile does nothing, as in a normal stream: SBR knobs
in AAC-LC, stereo knobs in mono and in HE-AAC v2.

Measured: the streams decode with the independent faad2 decoder behind dablin, the DLS
label reads back correctly, and the AAC-LC output is bit identical to what the reference
`odr-audioenc` produces from the same input. Nine combinations were tested, 48 and
32 kHz against mono and stereo across the three profiles, each independently decodable.

### `--dab-label <text>`

**Carries a static station label in the superframe, shown on a DAB+ receiver's
display.**

- Applies to: `--dab` mode only. Without `--dab` the label is ignored, because there is
  no superframe to put it in.
- Values: text, up to about 48 characters; default: no label
- Input: text string

A DAB+ receiver shows text on its display, usually the station name and often the title
of what is playing. That text travels in a channel called DLS, the Dynamic Label
Segment, tucked into the superframe as X-PAD data. This encoder carries a static label:
one fixed string for the whole file or stream.

Static is the operative word. The text does not change over the course of the encode. A
truly dynamic label that updates track by track, and the MOT slideshow that puts artwork
on the screen, are the job of `ODR-PadEnc` in the OpenDigitalRadio chain and are not
part of this encoder.

The length limit of about 48 characters comes from three DLS segments packed into one
PAD. Receivers vary in how much of a long label they display at once, so for a station
name the useful length is shorter than the limit.

Values:
- omitted: no label in the stream.
- `--dab-label <text>`: the given string is carried as DLS. Quote it if it contains
  spaces.

Example in context:

    fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

Works with / against: requires `--dab`. Independent of every other switch, since it
touches the PAD and not the audio.

# Appendices

## Reading `--verbose`

`--verbose` prints the values the encoder actually initialised, before encoding starts.
The block comes in sections. Most fields name themselves; the ones below need a word.

| Field | How to read it |
|---|---|
| `AOT (profile)` | 2 AAC-LC, 5 HE-AAC, 29 HE-AAC v2, 23 AAC-LD, 39 AAC-ELD. |
| `bitrate-mode` | 0 CBR, 1..5 VBR (higher = better). |
| `channel-mode` | 1 mono, 2 stereo. In HE-AAC v2 the core is mono and PS makes the stereo. |
| `core bandwidth` | Upper edge of the AAC core, snapped to the nearest scalefactor band edge, so it can differ from what you typed (`-w 17300` gives `17915 Hz`). The bracket names the source: `from -w`, `from --core-cutoff` or `auto`. Under SBR this is only the core; SBR continues above it. Under SBR `-w` is ignored, although the source may still read `from -w`. |
| `final BW (AAC+SBR)` | Only with SBR: the approximate top of the whole signal, core plus SBR, derived from the SBR stop index. |
| `signaling-mode` | How SBR and PS are signalled: 0 implicit, 1 explicit backward compatible, 2 explicit hierarchical. |
| `SBR ratio` | 1 downsampled (single rate), 2 dual rate (the core runs at half the sample rate). |
| `sbr amp res` | Envelope amplitude resolution: 0 = 1.5 dB, 1 = 3.0 dB. |
| `granule length`, `frame length` | Samples per frame: 1024 for LC, 512 or 480 for LD and ELD. |
| `codec delay` | Delay in samples per channel, total and core only. Matters for gapless playback. |
| `active SFBs (long)` | Number of scalefactor bands in a long block. The ceiling for `--msbands`, `--isbands` and every `<sfb>` argument. |
| `IS corr threshold`, `IS L/R ratio thresh` | Q8 values (256 = 1.0). A lower correlation threshold makes intensity stereo more eager. |
| `IS min contiguous SFBs` | How many adjacent bands must qualify before intensity stereo starts. |
| `TNS mask` | Bit mask 0x0..0xF of the active TNS filters. |
| `MS/IS bands: auto up to N` | Top band index up to which the tool may act. |
| `sbr header period` | Frames and milliseconds between SBR headers: how long a decoder that tunes into a stream waits for SBR. For ELD it reports the LATM/LOAS configuration period, as ELD has no in-band SBR header. |
| `bit reservoir budget` | Average bits per frame and the reservoir ceilings for mono and stereo. Use it to choose `--vbr-reservoir`. |
| `franken overrides applied` | The switches that deviate from stock in this run, with the effective value. If a switch you gave is missing here, it did not take effect. |

## Scalefactor bands and frequencies

Bands are numbered from 0 at the bottom. The table shows the approximate upper edge of
every fourth band in a long block, for each sample rate; the last row gives the number of
bands and the Nyquist frequency. Use it to translate `--msbands`, `--isbands`,
`--hole-start` and the other `<sfb>` arguments into hertz. The exact count for your run is
in `--verbose` (`active SFBs (long)`), and it also depends on the coded bandwidth.

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
| 44  | none    | 9647 | 13000| 17916 | 19500 | none     |
| 48  | none    | none    | 15000| 22050 | 24000 | none     |
| **number of bands / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### AAC-LC: automatic core bandwidth by bitrate

When you give no `-w`, FDK picks the bandwidth from a table indexed by bitrate per
channel, and then snaps it to a scalefactor band edge. Measured with `--verbose` at
48 kHz (stereo `-b` is the total for both channels):

| `-b` stereo | core bandwidth | `-b` mono | core bandwidth |
|---:|---:|---:|---:|
| 24000, 32000 | 6843 Hz | 12000, 16000 | 5062 Hz |
| 40000, 48000 | 9750 Hz | 20000, 24000 | 7500 Hz |
| 56000, 64000 | 13500 Hz | 28000, 32000 | 9750 Hz |
| 80000, 96000 | 14250 Hz | 40000, 48000 | 12750 Hz |
| 128000 | 15750 Hz | 64000 | 14250 Hz |
| 192000 | 17250 Hz | 96000 | 17250 Hz |

A higher `-w` than this only helps if there are bits for it: at 24..50 kbps a forced wide
band is exactly where group E (holes) comes in. Above 20 kHz the core needs
`--uncap-bandwidth` and a sample rate that has room for it.

## The M4A container: what is mandatory and what can go

An `.m4a` file is a tree of boxes. The playback skeleton is mandatory and has no switch:
`ftyp`, `mdat` (the audio data) and `moov` with `mvhd`, `trak`, `tkhd`, `mdia` (`mdhd`,
`hdlr`, `minf` with `smhd`, `dinf` and `stbl`, which holds `stsd`, `stts`, `stsc`, `stsz`,
`stco`). Without it no decoder can find the audio.

Optional is the whole `udta` / `meta` / `ilst` block: the encoder identification tag
(`©too`), `iTunSMPB` (encoder delay for gapless playback) and all text tags.

| Switch | Works on | What it does |
|---|---|---|
| `--no-tool-tag` | `.m4a` | Leaves out the encoder identification tag. Other tags and gapless data stay. |
| `--minimal-moov` | `.m4a` | The smallest legal file: leaves out the whole metadata block (encoder tag, `iTunSMPB`, all tags). The playback skeleton stays. |

Measured on 2 s of 128 kbps stereo: 34381 bytes by default, 34297 with `--no-tool-tag`
(84 less), 34048 with `--minimal-moov` (333 less). The overhead is mostly the mandatory
skeleton. For no container at all, write raw ADTS (`-f 2 -o out.aac`), which has no tags
and no gapless data.

`--minimal-moov` removes `iTunSMPB`, so joined tracks may get tiny gaps because the
encoder delay is no longer signalled. For albums meant to play without gaps, keep the
default.

## Default output against the original binary

Checked against the original binary supplied with dBpoweramp R17 (the same libfdk-aac
4.0.1, package 2.0.3). AAC-LC is bit identical (same md5). HE-AAC and HE-AAC v2 differ in
bytes but not in quality: the HE-AAC v2 spectrum matches to 0.000 dB, and the HE-AAC error
relative to the original is the same as the original's own (7.05e11 against 7.05e11). The
byte differences come from a different compiler (mingw/gcc against MSVC) and fixed point
rounding.

# Building and testing

## Building

The repository contains sources only. GitHub Actions builds both Windows binaries on
request (workflow "Build Franken FDK AAC (Windows x64 + x86)", button "Run workflow") and
for every release tag. To build locally on Linux or WSL with mingw-w64 and autotools:

    bash tools/build_win.sh both      # or x64 / x86

The script builds each architecture from a fresh copy of the sources, the same way CI
does, and leaves `fdkaac-franken-x64.exe` and `fdkaac-franken-x86.exe` in the project
directory. All changes to libfdk-aac go through one module, `libAACenc/src/franken.{h,cpp}`
(the global `g_franken` block, where every unset value means "stock"); the new library
parameters (`AACENC_PARAM`, range `0xF0xx`) are declared in `aacenc_lib.h`.

## Tests

    make check

runs `tests/check.sh` on the built binaries. It checks that every switch appears in
`--help`, that every switch produces a stream the decoders accept without errors, that
each switch really changes the stream on material where it should act, that values read
from the bitstream match what was requested (window shape, groups, frame sizes), that
holes and pre-echo move in the measured direction after decoding, and that without the new
switches the output is byte identical to stock. It needs ffmpeg and python3; exit code 0
means all passed, 1 failures, 77 a missing dependency.

The measuring tools used for the numbers in this document are in `tools/`: `tools/core/`
(holes and pre-echo after decoding), `tools/invf/` (SBR inverse filtering and low
bitrate floors), `tools/msis/` (MS and IS read from the bitstream, SBR headers, stream
tune-in), `tools/dab/` (DAB+ verification).
