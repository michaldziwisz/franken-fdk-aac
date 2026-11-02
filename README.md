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


# PL Wersja polska

# Franken FDK AAC

Franken FDK AAC to koder Fraunhofer FDK AAC (libfdk-aac 2.0.3, mstorsjo/fdk-aac) z
frontendem nu774/fdkaac 1.0.2, rozszerzony o przełączniki odsłaniające decyzje, które FDK
zwykle podejmuje sam: jak kodowane jest stereo, jak odtwarzane jest górne pasmo, gdzie wolno
wyzerować pasma, jak traktowane są transjenty, jak bity rozkładają się w czasie. Jest
przeznaczony dla realizatorów, którzy chcą usłyszeć i zmierzyć, co te decyzje robią,
zwłaszcza w skrajnościach: przy bardzo niskim bitrate'ie, wymuszonym szerokim paśmie, mowie
i streamingu.

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski. Zbudowany na otwartym
kodzie: libfdk-aac (Fraunhofer IIS) i frontend nu774/fdkaac.

Bez żadnego z nowych przełączników koder daje dokładnie to samo co fabryczny FDK, bajt w
bajt. Każdy przełącznik trzeba włączyć świadomie i każdy może też pogorszyć wynik: pozwalają
wejść tam, gdzie strojenie FDK celowo nie wchodzi.

## Pobieranie

Gotowe binarki dla Windows są publikowane jako GitHub Releases i pobierają się bez konta:

**https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Wersje beta (na przykład `v1.4.0-beta.1`) są publikowane jako wydania wstępne, więc powyższy
odnośnik nadal pokazuje ostatnią wersję stabilną; bety są na pełnej liście:
**https://github.com/michaldziwisz/franken-fdk-aac/releases**

`franken-fdk-aac-x64-vX.Y.Z.zip` to wersja 64-bitowa, `x86` to 32-bitowa. Każde archiwum
zawiera statyczny koder (bez bibliotek DLL) i pełną dokumentację. Samo repozytorium ma
tylko źródła; budowanie opisuje koniec tego dokumentu.

## Pierwsze kroki

Wszystkie oryginalne opcje frontendu działają jak dotąd (`-p`, `-b`, `-m`, `-w`, `-a`,
`-s`, `-f`, znaczniki); `fdkaac-franken-x64.exe --help` wypisuje je razem z nowymi.

1. Zakoduj raz z `--verbose` i przeczytaj, co koder faktycznie ustawił:

       fdkaac-franken-x64.exe -p 2 -b 128000 --verbose -o out.m4a in.wav

2. Zmieniaj jeden przełącznik naraz i sprawdzaj w `--verbose`, w sekcji
   `franken overrides applied`, czy zadziałał.
3. Słuchaj, a gdzie to ważne, mierz: zdekoduj obie wersje i porównaj. Narzędzia w
   katalogu `tools/` repozytorium robią to dla dziur, pre-echa i błędu stereo.

## Przepisy

Każdy z nich to sprawdzony punkt wyjścia, a nie preset do stosowania w ciemno.

    # LC stereo przy niskim bitrate'ie z szerokim pasmem: mniej dziur, mniej pre-echa
    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

    # To samo przy 40 kbps, ze spokojniejszym detektorem ataków dla perkusji
    fdkaac-franken-x64.exe -p 2 -b 40000 -w 18000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 --attack-decay 0,9 -o out.m4a in.wav

    # Szersze stereo kosztem środka
    fdkaac-franken-x64.exe -p 2 -b 96000 --side-bias 6 -o out.m4a in.wav

    # Quasi-VBR wokół 128 kbps: silnik CBR z miejscem na oddech
    fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

    # HE-AAC z łagodniejszym, mniej metalicznym górnym pasmem
    fdkaac-franken-x64.exe -p 5 -b 32000 --sbr-softness 4 -o out.m4a in.wav

    # HE-AAC v2 z szerszym rdzeniem, niż wybiera FDK (9 kHz -> 10,5 kHz)
    fdkaac-franken-x64.exe -p 29 -b 48000 --sbr-start 14 -o out.m4a in.wav

    # Strumień, który słuchacz dołączający w trakcie od razu dekoduje z SBR
    fdkaac-franken-x64.exe -p 5 -b 48000 --sbr-header-period 1 -f 2 -o out.aac in.wav

    # AAC-LD z szerszym rdzeniem i częstszym TNS przeciw pre-echu (LD wymaga LOAS, nie ADTS)
    fdkaac-franken-x64.exe -p 23 -b 64000 --core-cutoff 12000 --tns-thresh 0,7 -f 10 -o out.loas in.wav

    # Skrajnie niski HE-AAC v2: 8000 bps stereo
    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

    # Pełny rdzeń 42 kHz ze źródła 96 kHz
    fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

Ostatni przepis daje w `--verbose` `core bandwidth: 42000 Hz`: żądane 40000 Hz zostaje
wyrównane w górę do najbliższej granicy pasma współczynnika skali.

# Opis przełączników

To jest opis wszystkich przełączników. Każdy ma własną kartę, a każda karta jest zbudowana
tak samo, więc można czytać tylko tę kartę, która jest potrzebna.

## Jak czytać kartę

Nagłówek to przełącznik w takiej postaci, w jakiej się go wpisuje, razem z argumentem. Pod
nim:

- Jedno zdanie pogrubione: do czego służy przełącznik, pod kątem dźwięku albo
  zachowania.
- **Dotyczy**: profile i sytuacje, w których przełącznik faktycznie działa. Jeśli w
  jakimś profilu nie robi nic, jest to napisane właśnie tutaj.
- **Wartości**: dozwolony zakres, jednostka i wartość domyślna. Domyślna to zawsze ta,
  która oznacza „decyduje fabryczny FDK”.
- **Wpisywanie**: czy przełącznik przyjmuje liczbę całkowitą, liczbę dziesiętną, flagę
  czy tekst.
- Dalej wyjaśnienie: najpierw problem, potem mechanizm w prostych słowach, a na końcu
  co się dzieje przy zwiększaniu i zmniejszaniu wartości.
- **Przykłady**: dla przełącznika liczbowego z miejscem na manewr nastawa łagodna,
  mocna i celowo przesadzona, w każdym kierunku. Przełączniki wyliczeniowe mają w tym
  miejscu listę wszystkich wartości.
- **Współpraca i konflikty**: zależności oraz informacja, który inny przełącznik
  przesłania ten.
- **Zmierzone**: wyłącznie rzeczywiste pomiary, z materiałem i warunkami. Jeśli pomiaru
  nie ma, nie ma też tej linii.
- **Pod maską**: jedna albo dwie linie dla ciekawych. Można je spokojnie pominąć.

## Wspólne zasady

Obowiązują w każdej karcie i nie są w nich powtarzane.

**Wyłączony = ustawienie fabryczne.** Każdy przełącznik Frankensteina jest wyłączony,
dopóki się go nie poda. Jeśli żadnego z nich nie ma w linii poleceń, wynik jest
identyczny bajt w bajt z wynikiem fabrycznego FDK (zachowanie niezmienionego FDK).
„Wyłączony” zapisuje się jako `-1` dla niemal wszystkich (to wartość specjalna oznaczająca
„zostaw własną decyzję FDK”) oraz jako `0` dla `--core-cutoff`, `--side-bias`,
`--side-knee` i `--mask-slope`, gdzie `-1` byłoby poprawną, działającą wartością.

**Liczby dziesiętne.** Przełączniki dziesiętne przyjmują `.` albo `,`, do trzech miejsc
po przecinku. Wartość jest zaokrąglana w połowie od zera, a zakres jest sprawdzany już
na wartości zaokrąglonej. Wszystko inne to błąd: `1e1`, `nan`, `3x`, spacje w środku.
Przełączniki całkowitoliczbowe odrzucają części dziesiętne, więc `128,5` tam, gdzie
oczekiwana jest liczba całkowita, jest błędem, a nie cichym obcięciem.

**Jednostki.** W całym tekście pojawiają się cztery konwencje:

- `dB`: zwykłe przesunięcie w decybelach, dodatnie albo ujemne.
- `x100`: liczba całkowita będąca stukrotnością wartości rzeczywistej, więc `100`
  oznacza 1,00, `90` oznacza 0,90, a `400` oznacza 4,00.
- `Q8`: liczba całkowita w formacie stałoprzecinkowym, gdzie `256` oznacza 1,0. Czyli
  `128` to 0,5, `384` to 1,5, `512` to 2,0, a `4096` to 16,0. Q8 to wewnętrzna skala
  samego FDK i dlatego trafia ona do linii poleceń.
- `SFB`: numer pasma współczynnika skali (scale factor band, SFB), liczony od `0` od dołu
  widma w górę. Pasmo współczynnika skali to grupa linii widmowych, które dzielą jeden
  współczynnik skali (scale factor),
  w przybliżeniu zgodnie z pasmem krytycznym: wąskie na dole, szerokie na górze. Ile
  tych pasm istnieje, zależy od profilu, częstotliwości próbkowania i kodowanego pasma,
  więc numer SFB ma sens tylko wtedy, gdy znana jest ta liczba. Wypisuje ją `--verbose`,
  na przykład `active SFBs (long): 49` przy AAC-LC 48 kHz stereo.

**Jak sprawdzić, co weszło w życie.** `--verbose` wypisuje na stderr rzeczywiste,
zainicjalizowane parametry kodera, zanim zacznie się kodowanie, a do tego linię
`franken overrides applied` z listą przełączników, które w tym przebiegu naprawdę
odbiegają od ustawień fabrycznych. Jeśli ustawionej wartości tam nie ma, to nie weszła ona w życie i
żadne słuchanie nie powie o tym szybciej.

## Jak czytany jest `-b`

Obok siebie żyją trzy konwencje, a pomylenie ich to najczęstszy sposób na zaskakujący
rozmiar pliku.

- **Praca normalna**: `-b` to bity na sekundę, ale frontend przyjmuje też postać skróconą.
  Małe liczby są czytane jako kilobity, więc `-b 128` i `-b 128000` oznaczają to samo,
  czyli 128 kbps, a `-b 1152` oznacza 1152 kbps.
- **Z `--unlock-bitrate`**: `-b` jest czytane dosłownie jako bity na sekundę, bez skrótu
  kilobitowego. `-b 6000` to 6000 bps. `-b 1152` w tym trybie to 1152 bps, czyli
  praktycznie nic; program ostrzega, gdy wartość wygląda na taką właśnie pomyłkę.
- **Z `--dab`**: `-b` to kilobity na sekundę, w zakresie 8..192 i jako wielokrotność 8.
  Koder sam mnoży przez 1000, więc `-b 96` to 96 kbps, także razem z `--unlock-bitrate`.

Górnego pułapu nie ustawia żaden przełącznik i nie da się go podnieść: ramka AAC mieści
najwyżej 6144 bity na kanał. Przy 48 kHz to z grubsza 576 kbps na kanał.

## Uwaga o transporcie dla LD i ELD

AAC-LD (`-p 23`) i AAC-ELD (`-p 39`) działają w tym koderze, ale nie da się ich zapisać do
ADTS. Żądanie `-f 2` z tymi profilami kończy się komunikatem „encoder initialization
failed”, a winowajcą jest format transportu, nie profil. Trzeba użyć M4A (`-f 0`, wartość
domyślna), LATM (`-f 6` albo `-f 7`) albo LOAS (`-f 10`). LOAS jest wyborem praktycznym, gdy
potrzebny jest goły strumień do pomiaru albo do podania dalej, bo jest to samowystarczalny
strumień bajtów, tak jak ADTS dla LC i HE-AAC.

## Mapa grup

Przełączniki są pogrupowane w `--help` z grubsza od łatwych do dziwactw, a ten opis trzyma
się tej samej kolejności.

- **A. Na początek**: pokrętła dla użytkownika. Diagnostyka, przeliczanie częstotliwości
  próbkowania wejścia, strojenie pod mowę, intensity stereo na jednym suwaku i dwie flagi
  włączane świadomie, które zdejmują wbudowane ograniczenia.
- **B. Kodowanie stereo**: mid/side, intensity stereo w szczegółach, parametric stereo.
- **C. Pasmo i SBR**: gdzie kończy się rdzeń i zaczyna SBR, potem siatka, detektory i
  estymatory górnego pasma; grupę zamyka `--core-cutoff`.
- **D. Maskowanie, kształtowanie szumu i szczegół**: progi psychoakustyczne, SNR, którego
  koder wymaga w każdym paśmie, TNS i PNS.
- **E. Transjenty rdzenia, pre-echo i dziury w widmie**: rdzeń LC, LD i ELD pod presją
  bitów.
- **F. Przełączanie bloków i kontrola bitrate'u**: decyzja blok długi czy krótki,
  rezerwuar bitów i pokrętła quasi-VBR.
- **G. Wyjście DAB+**: superramki radia cyfrowego.

## A. Na początek

Sześć przełączników, w kolejności, w jakiej zwykle są potrzebne: `--verbose` pokazuje, co
koder naprawdę zrobił, `--resample` wybiera częstotliwość próbkowania, z którą pracuje cały
koder, `--speech` i `--is-aggression` go stroją, a `--unlock-bitrate` i `--uncap-bandwidth`
zdejmują ograniczenia fabryczne FDK. Na początek warto przeczytać o `--verbose` i używać go
po każdym kolejnym przełączniku. Towarzyszem `--verbose` jest `--bit-stats` (grupa E), gdy
pytanie brzmi, na co poszły bity.

### `--verbose`

**Wypisuje na stderr rzeczywiste, zainicjalizowane ustawienia kodera przed kodowaniem,
żeby było widać, co faktycznie weszło w życie.**

- Dotyczy: każdego profilu i każdego transportu. Nie zmienia nic w strumieniu wyjściowym.
- Wartości: flaga, bez argumentu; domyślnie: wyłączona
- Wpisywanie: flaga

Koder podejmuje na podstawie linii poleceń bardzo wiele decyzji i większość z nich nie jest
wpisanymi wartościami. Pasmo zostaje przyciągnięte do granicy pasma współczynnika skali.
Bitrate wybiera wiersz w tabeli strojenia, a ten z kolei ustawia PNS, intensity stereo i
siatkę SBR. Poprawnie zapisany przełącznik może i tak zostać zignorowany, bo profil nie ma
takiego mechanizmu. `--verbose` to jedyny sposób, by zobaczyć wynik tego wszystkiego przed
zmarnowaniem czasu na słuchanie.

Co wypisuje: profil (AOT), bitrate i tryb bitrate'u, częstotliwość próbkowania, tryb
kanałów, efektywne pasmo rdzenia w Hz wraz ze źródłem tej decyzji (`from -w`, `from
--core-cutoff` albo `auto`), stan afterburnera, transport i sygnalizację oraz stan
narzędzi kodowania, które koder wybrał sobie sam, czyli TNS, PNS, intensity stereo i MS
stereo włączone albo wyłączone. Gdy działa SBR, dochodzi do tego współczynnik SBR,
efektywne indeksy częstotliwości początkowej i końcowej, skala częstotliwości, pasma
szumu, rozdzielczość amplitudy oraz, dla całego pasma, oszacowanie `final BW (AAC+SBR)`.
Wypisuje blok budżetu rezerwuaru, który mówi, ile jest średnio bitów na ramkę i jakie są
pułapy, a także `active SFBs (long)`, czyli liczbę potrzebną przed ustawieniem
jakiegokolwiek przełącznika opartego na SFB. Na końcu pojawia się lista
`franken overrides applied`, wymieniająca tylko te przełączniki, które w tym przebiegu
naprawdę odbiegają od ustawień fabrycznych.

Trzy odczyty warto obejrzeć uważnie. Pasmo rdzenia jest wyrównane do granicy pasma SFB,
więc wpisane `-w 17300` może wrócić jako `17915 Hz`: koder nie potrafi przyciąć w środku
pasma. Pod SBR wartość `-w` jest ignorowana, ale nadal opisana jako `[from -w]`; tam
rdzeń idzie za `--sbr-start`. Natomiast `active SFBs (long)` to pułap dla `--msbands`,
`--isbands` i każdego argumentu `<sfb>`: poproszenie o pasmo 48, gdy kodowanych jest tylko
40, nie jest błędem, po prostu nic nie robi.

Wartości:
- `--verbose`: wypisz zrzut. Nie ma przełącznika wyłączającego, bo pominięcie go jest
  wyłączeniem.

Przykład w kontekście:

    fdkaac -p 5 -b 64000 --verbose -o out.m4a in.wav

Współpraca i konflikty: brak. To narzędzie diagnostyczne dla wszystkiego innego w tym opisie
i najszybsza odpowiedź na pytanie „czy mój przełącznik w ogóle coś zrobił”.

Zmierzone: przy AAC-LC 48 kHz stereo raportuje `active SFBs (long): 49`. Przy HE-AAC v2 48
kbps, wejście 48 kHz, rdzeń wychodzi na 9000 Hz przy indeksie startu SBR 13.

### `--resample <częstotliwość>`

**Przelicza wejście na inną częstotliwość próbkowania, zanim zobaczy je cokolwiek innego,
więc cały koder pracuje już w nowej częstotliwości.**

- Dotyczy: każdego profilu i transportu, także `--dab`. Tak samo działa na plikach, na
  potoku (`-` jako wejście) i na wejściu `--raw`.
- Wartości: częstotliwość próbkowania AAC w Hz (`44100`) albo w kHz (`44`, `44.1`, `44,1`,
  `22`, `22.05`, `32`, `48`, `96` ...). Od 8000 do 96000. `-1` = wyłączone; domyślnie:
  wyłączone.
- Wpisywanie: częstotliwość

Przelicznik stoi na samym początku łańcucha PCM, zaraz za czytnikiem WAV, CAF albo surowego
PCM, przed limiterem, ekstrapolatorem brzegów i koderem. Nic dalej nie zna pierwotnej
częstotliwości: koder dobiera tabele, pasmo, układ SBR i budżet bitów dla nowej, skala
czasu M4A i długość gapless są liczone w nowej, a `--verbose` pokazuje nową wartość
`samplerate`. Źródło, które już ma żądaną częstotliwość, idzie nietkniętym łańcuchem, bit
w bit jak bez przełącznika.

Jak przelicza: dokładny stosunek wymierny (48000 na 44100 to 147/160, więc przez godziny
dźwięku nic się nie rozjeżdża), jądro sinc z oknem Kaisera liczone na starcie, arytmetyka
zmiennoprzecinkowa. Pasmo przenoszenia jest płaskie do 95 procent niższej częstotliwości
Nyquista, tłumienie zaczyna się na tej częstotliwości i wynosi 120 dB, więc nic nie odbija
się w pasmo słyszalne. Jądro jest symetryczne, a chwile wyjściowe są wyrównane z wejściem,
więc przeliczenie nie dodaje opóźnienia: opóźnienie kodera i priming gapless zostają takie,
jakie są w nowej częstotliwości. Jądro potrzebuje kilku milisekund wyprzedzenia wewnątrz
przelicznika, czego z zewnątrz nie widać. Żadnej zewnętrznej biblioteki ani DLL.

Po co: poniżej mniej więcej 48 kbps stereo z wąskim `-w` rdzeń 48 kHz wydaje bity na pasma
współczynników skali i siatkę ramek pomyślaną pod dźwięk do 20 kHz. Niższa częstotliwość
daje drobniejsze pasma poniżej odcięcia i dłuższą ramkę. Dłuższa ramka to też cena: przy 24
kHz jeden blok długi trwa 85 ms, więc pre-echo przed ostrymi atakami rośnie. Wyższa
częstotliwość też się przydaje: `--dab` przyjmuje tylko 32 i 48 kHz, więc źródło 44,1 kHz
potrzebuje `--resample 48`.

Wartości:
- pominięta albo `-1`: bez przeliczania (jak w fabrycznym FDK).
- `--resample 32`: źródło 48 albo 44,1 kHz kodowane w 32 kHz. Przy `-w 9500` i 32 kbps
  stereo zwykle najlepszy kompromis (patrz Zmierzone).
- `--resample 24`: jeszcze dalej w tę stronę, z dłuższymi blokami; warto tylko dla
  materiału bez ostrych ataków.
- `--resample 48`: źródło 44,1 kHz dla `--dab` albo dla HE-AAC w 48 kHz.

Przykład w kontekście:

    fdkaac -p 2 -b 32000 -w 9500 --resample 32 -o out.m4a in48k.wav
    ffmpeg -i in.flac -f wav - | fdkaac --dab -b 64 --resample 48 -o out.dabp -

Współpraca i konflikty: `-w` i `--core-cutoff` są w Hz, więc w każdej częstotliwości znaczą
to samo, ale nie mogą przekroczyć nowej częstotliwości Nyquista. `--raw-rate` podaje
częstotliwość surowego wejścia, a `--resample` przelicza z niej. Przełączniki z numerem
pasma (`--msbands`, `--is-lo` ...) liczą pasma tabeli nowej częstotliwości, sprawdź je w
`--verbose`.

Zmierzone: sam przelicznik (harness `tools/resample/rs_test.c`,
`tools/resample/rs_quality.py`) dla 48 na 32, 48 na 24, 44,1 na 32, 48 na 44,1, 44,1 na
22,05, 32 na 48, 44,1 na 48 i 48 na 16 kHz: odchyłka pasma przenoszenia poniżej 0,0001 dB
do 95 procent Nyquista, aliasy tonów leżących między nową a starą częstotliwością Nyquista
od -120 do -132 dB, THD+N tonu 1 kHz od -135 do -141 dB, przesunięcie fazy 0, długość
wyjścia dokładnie `ceil(n * nowa / stara)`, te same próbki niezależnie od wielkości porcji
odczytu. Szybkość: od 37 do 106 razy szybciej niż czas rzeczywisty dla stereo na jednym
rdzeniu (przypadek 48 na 32 kHz: kodowanie 30 s trwało 0,94 s wobec 0,62 s bez
przełącznika). Po kodowaniu (AAC-LC 32 kbps stereo, `-w 9500`, błąd mierzony w 200..9000 Hz
względem oryginału 48 kHz): to samo z dokładnością do 0,03 dB co wcześniejsze przeliczenie
programem SoX w najwyższej precyzji. W porównaniu z kodowaniem w 48 kHz stosunek szumu do
sygnału na czterech próbkach muzyki 48 / 32 / 24 kHz: -7,3 / -8,2 / -9,0 dB,
-11,1 / -14,8 / -16,8 dB, -7,6 / -8,4 / -9,1 dB, -8,3 / -7,7 / -8,6 dB; dziury w widmie
5,6 / 0,1 / 0 procent na pierwszej próbce. Pre-echo na sygnale testowym z atakami:
-33,9 dB przy 48 kHz, -28,4 dB przy 32 kHz, -28,1 dB przy 24 kHz.

### `--speech`

**Przełącza warstwę SBR na strojenie pod mowę: inne filtrowanie odwrotne, inny poziom
szumu (noise floor) i brak parametrycznego kodowania górnego pasma.**

- Dotyczy: HE-AAC i HE-AAC v2, czyli wszędzie, gdzie działa SBR, także ELD z SBR. Bez
  efektu w AAC-LC, które nigdzie w FDK nie ma osobnego trybu mowy.
- Wartości: flaga; domyślnie: wyłączona (strojenie pod muzykę)
- Wpisywanie: flaga

SBR odbudowuje górne pasmo z zakodowanego pasma dolnego i musi wiedzieć, jak tonalne jest
źródło, żeby zdecydować, jak mocno wybielić pasmo odtworzone przez SBR i ile szumu dosypać.
Muzyka i mowa odpowiadają na to zupełnie inaczej. Mowa przy niskim bitrate cierpi przy
strojeniu pod muzykę na konkretny artefakt: odbudowana góra bulgocze, bo estymator wciąż
uznaje odtworzone pasmo za tonalne, a w rzeczywistości jest to ruchomy formant plus oddech.

Flaga zmienia kilka rzeczy naraz: tabela decyzyjna filtrowania odwrotnego przełącza się na
tabelę mowy, wagowanie poziomu szumu przechodzi na wartość dla mowy, a parametryczne
kodowanie górnego pasma zostaje wyłączone. Dla podcastu, audiobooka albo dialogu przy 24..48
kbps efektem jest naturalniejsza góra z mniejszym bulgotaniem.

Jeśli potrzebna jest tylko część tego, co robi flaga, poszczególne elementy są dostępne
osobno: `--sbr-invf-table 1` daje samą tabelę decyzyjną mowy, `--sbr-nf-weight 1.0` samą
wagę szumu dla mowy, a `--sbr-softness 4` oba te elementy, w każdym przypadku bez ruszania
częstotliwości podziału i bez wyłączania kodowania parametrycznego.

Dla mowy w zwykłym AAC-LC nie ma odpowiednika i ten opis nie będzie udawał inaczej.
Model psychoakustyczny LC jest ten sam dla mowy i dla muzyki. Pomaga tam natomiast
zwężenie pasma, bo mowa ma mało treści wysoko, za pomocą `--core-cutoff 12000` do
`14000`, a przy bardzo niskim bitrate włączenie substytucji szumem przez
`--pns 1 --force-pns`.

Wartości:
- pominięta: strojenie pod muzykę (ustawienie fabryczne).
- `--speech`: strojenie pod mowę.

Przykład w kontekście:

    fdkaac -p 5 -b 32000 --speech -o speech.m4a podcast.wav

Współpraca i konflikty: jawne przełączniki estymatorów SBR z grupy C wygrywają z tą flagą,
więc `--sbr-invf` wymusza ustalony poziom filtrowania odwrotnego bez względu na nią.
`--sbr-invf-table` i `--sbr-nf-weight` idą za `--speech`, dopóki zostają na `-1`, i właśnie
tak jedna flaga potrafi ruszyć oba.

Zmierzone: zerowy efekt w AAC-LC; zmienia strumień HE-AAC.

### `--is-aggression <0..100>`

**Jeden suwak decydujący, jak ochoczo koder zwija górne pasma do intensity stereo,
wymieniając separację stereo na górze na bity.**

- Dotyczy: profili z prawdziwym rdzeniem stereo, czyli AAC-LC, AAC-LD i AAC-ELD, oraz
  rdzenia HE-AAC. Bez efektu w mono. Pod HE-AAC v2 obraz stereo niesie parametric stereo
  na rdzeniu mono, więc intensity stereo nie ma na czym działać.
- Wartości: `0..100`, gdzie `0` to zachowanie fabrycznego FDK; domyślnie: `-1`, wyłączone
  (identycznie jak bez przełącznika)
- Wpisywanie: liczba całkowita

Intensity stereo (IS) zastępuje osobne widma lewego i prawego kanału w górnych pasmach jedną
wspólną obwiednią energii plus kierunkiem. Ucho słabo lokalizuje wysokie tony, więc jest to
tanie: odrzuca się połowę danych w tych pasmach, a płaci separacją stereo na górze. Na
materiale, który naprawdę ma różne góry w obu kanałach, na przykład z talerzami ustawionymi
mocno na jedną stronę albo z szerokimi efektami przestrzennymi, płaci się słyszalnie.

Fabryczny FDK jest wobec IS wyjątkowo nieśmiały i właśnie dlatego łatwo uznać, że
przełącznik nie robi nic. Warunków dopuszczenia (gate) jest cztery i pasmo musi spełnić
wszystkie naraz: warunek bitrate'u, który bierze IS pod uwagę tylko wtedy, gdy bitrate na
pasmo spada poniżej progu, warunek korelacji między kanałami na poziomie około 0,95,
minimalna seria 6 kolejnych pasm i stabilny kierunek. Poluzowanie jednego warunku zwykle nie
zmienia nic, bo blokuje kolejny. `--is-aggression` rusza nimi wszystkimi razem, spójnie:
usuwa warunek bitrate'u i warunek liczby pasm oraz, w miarę wzrostu liczby, luzuje wymagania
co do korelacji, kierunku i rozmiaru obszaru.

Wyżej oznacza więcej pasm kodowanych jako IS, mniej bitów wydanych na górę widma i słabszą
separację stereo właśnie tam. `100` to prawdziwe maksimum: warunek bitrate'u usunięty,
korelacja poluzowana do około 0,475, obszar jednego pasma już wystarcza, a tolerancja
kierunku jest szeroka. Niskie wartości nie nadają się na delikatną korektę, bo małe
poluzowanie często i tak zostawia inny warunek jako wiążący; `1` nie zmieniło nic na
materiale testowym. Użyteczne okno pracy zaczyna się około 30.

Jest tylko jeden kierunek do przejścia, bo `0` to już ustawienie fabryczne, a fabryczne
strojenie jest blisko „nigdy”.

Przykłady:
- Łagodnie, więcej IS: `--is-aggression 30`. Na skorelowanym materiale pasma zaczynają
  trafiać do IS. Obraz na górze zwęża się nieznacznie, kilka procent bitów zostaje
  uwolnionych na resztę.
- Mocno, więcej IS: `--is-aggression 50`. Wyraźnie więcej pasm w IS. Słyszalne zwężenie
  na talerzach i ogonach pogłosu, realna oszczędność bitów przy 48..96 kbps.
- Przesadnie, więcej IS: `--is-aggression 100`. Do IS idzie prawie wszystko, co może.
  Góra obrazu zlepia się ze środkiem, a spanoramowane góry zaczynają dryfować. Przydatne
  przy 32 kbps i niżej oraz jako ustawienie skrajne, żeby usłyszeć, jak brzmi IS puszczone
  luzem.

Współpraca i konflikty: jawne przełączniki `--is-*` z grupy B nadpisują to, co ustawił
suwak, więc najpierw warto użyć suwaka, a po `--is-corr-thresh`, `--is-min-sfbs` i
`--is-lr-ratio` sięgać tylko wtedy, gdy potrzebna jest precyzja. `--is 0` wyłącza
intensity stereo całkowicie i czyni ten przełącznik bez znaczenia. `--is-force-lo` i
`--is-force-hi` obchodzą każdy warunek dopuszczenia, więc ten suwak nie jest im w ogóle potrzebny.
`--verbose` wypisuje efektywny próg korelacji i minimalną liczbę pasm, czym potwierdza
się, że suwak zadziałał.

Zmierzone: na mix48 przy AAC-LC 64 kbps `1` nie dało żadnej zmiany strumienia, a `30`,
`50` i `100` za każdym razem strumień zmieniły.

### `--unlock-bitrate`

**Zdejmuje dolne progi bitrate'u, których pilnuje każdy profil, i sprawia, że `-b` to
dosłownie bity na sekundę.**

- Dotyczy: każdego profilu. Zdejmowane progi różnią się w zależności od profilu, patrz
  niżej.
- Wartości: flaga; domyślnie: wyłączona (wszystkie progi obowiązują)
- Wpisywanie: flaga

Fabryczny FDK odmawia kodowania poniżej pewnego progu, a próg ten jest inny w każdym profilu
i często bywa mylony z ograniczeniem formatu. Nie jest nim. Rozpowszechnione twierdzenie, że
HE-AAC nie potrafi zejść poniżej 16 kbps, opisuje najniższy wiersz tabeli strojenia SBR dla
48 kHz stereo i nic więcej. Gdy żądanie jest mniejsze, fabryczny FDK po cichu podnosi
bitrate do tego wiersza.

Flaga zdejmuje pięć osobnych progów:

- minimum rdzenia wynoszące 40 bitów na kanał;
- regułę LD i ELD o 8 kbps na kanał oraz rezerwę transportową 208 bitów, którą te profile
  zgadują (rzeczywisty narzut nagłówków jest nadal sprawdzany wobec rzeczywistego kodera
  transportu);
- początek tabeli pasma dla LD i ELD, żeby te profile mogły użyć pasma węższego niż
  pierwszy wiersz tabeli;
- próg tabeli strojenia SBR i PS: HE-AAC, HE-AAC v2 oraz ELD z SBR poniżej tabeli
  używają ponownie jej najniższego wiersza, utrzymując tę samą siatkę SBR, podczas gdy
  żądany bitrate jest respektowany, a rdzeń AAC dostaje po prostu mniej bitów;
- regułę, że ramka niemogąca pomieścić własnej informacji pobocznej (side information)
  przerywa kodowanie.
  Takie ramki zamiast tego przestrzeliwują, a rezerwuar wchłania to w kolejnych ramkach.

Zostaje nagłówek transportu, 56 bitów na ramkę dla ADTS, oraz górny pułap 6144 bitów na
kanał, którego nie rusza żaden przełącznik. Poniżej oszacowanej zawartości SBR, czyli około
2,5 kbps na kanał plus parametric stereo, rdzeń nie miałby już żadnych bitów i koder
zatrzymuje się z czystym „encoder initialization failed”, a nie z asercją.

Flaga zdejmuje progi. Nie gwarantuje żadnego konkretnego bitrate'u. Przy takich wartościach
gęste fragmenty rutynowo wychodzą powyżej tego, o co poproszono, bo same nagłówki i
informacja poboczna kosztują więcej niż budżet, więc wynik należy oceniać po rozmiarze
pliku, a nie po liczbie w `--verbose`.

Uwaga na zmianę znaczenia `-b`. W trybie unlock liczba to dosłownie bity na sekundę.
`-b 6000` to 6000 bps. `-b 6` to 6 bps i nie zakoduje nic sensownego, a `-b 1152` wpisane
w przekonaniu, że znaczy 1152 kbps, daje 1152 bps. Zawsze trzeba tu wpisywać pełną liczbę
bitów.

Wartości:
- pominięta: wszystkie progi obowiązują, `-b` czytane ze skrótem kilobitowym (ustawienie
  fabryczne).
- `--unlock-bitrate`: progi zdjęte, `-b` to dosłowne bps.

Przykład w kontekście:

    fdkaac -p 29 -b 6000 --unlock-bitrate -f 2 -o out.aac in48k.wav

Współpraca i konflikty: `--vbr-reservoir` i `--peak-bitrate` stają się tu ciekawsze, bo
przestrzelenie jest w tym trybie normą, a rezerwuar jest tym, co za nie płaci. Przy `--dab`
konwencja kilobitowa DAB+ dla `-b` zostaje zastąpiona dosłownymi bps. Nic w grupie D ani F
nie przywraca progów.

Zmierzone: rzeczywisty bitrate obliczony z rozmiaru strumienia ADTS albo LOAS, bez narzutu
kontenera:

| Tryb | `-b` | rzeczywiste bps | dekoder FDK | ffmpeg 6.1 |
|---|---|---|---|---|
| HE-AAC 48 kHz stereo | 8000 / 6000 | 9232 / 7290 | 0 błędów | 0 błędów |
| HE-AAC 48 kHz mono | 4000 / 3000 | 4736 / 4348 | 0 | 0 |
| HE-AAC v2 48 kHz | 6000 / 4000 | 6678 / 4894 | 0 | 1 (patrz niżej) |
| ELD+SBR 48 kHz stereo | 12000 | 12984 | 0 | patrz niżej |
| ELD / LD 32 kHz stereo | 8000 | 8154 | 0 | patrz niżej |
| AAC-LC 48 kHz stereo | 4000 / 3000 | 6012 / 7054 | 0 | 0 |

Tam, gdzie rzeczywisty bitrate przekracza `-b`, chodzi o gęste ramki przestrzeliwujące na
samych nagłówkach i informacji pobocznej. Kolumnę ffmpeg 6.1 trzeba czytać ostrożnie:
raportuje te same błędy na koderze fabrycznym w tych trybach, jeden
`noise_facs_q is invalid` na HE-AAC v2 przy 12..14 kbps, co mieści się w tabelach
fabrycznych, oraz wiele błędów na każdym strumieniu LOAS z ELD albo LD przy dowolnym
bitrate. ffmpeg nie implementuje LD i ELD w pełni, więc te strumienie należy oceniać
dekoderem Fraunhofera.

### `--uncap-bandwidth`

**Zdejmuje twardy limit 20 kHz, jaki FDK nakłada na pasmo rdzenia, żeby `-w` albo
`--core-cutoff` mogły sięgnąć aż do Nyquista.**

- Dotyczy: każdego profilu z rdzeniem, który da się rozszerzyć, w praktyce AAC-LC przy
  wysokim bitrate ze źródła o wysokiej częstotliwości próbkowania. Bez `-w` albo
  `--core-cutoff` nie ma sensu.
- Wartości: flaga; domyślnie: wyłączona (obowiązuje limit 20 kHz)
- Wpisywanie: flaga

FDK ogranicza pasmo rdzenia do `min(20000, sr/2)`. Rozumowanie jest słuszne dla normalnego
materiału: powyżej 20 kHz prawie nikt nie słyszy, a bity lepiej wydać niżej. Konsekwencja
jest taka, że źródło 96 kHz zakodowane przy hojnym bitrate nadal nie koduje nic powyżej 20
kHz, niezależnie od żądań, a dodatkowa częstotliwość próbkowania kupuje tylko łagodniejsze
zbocze filtra antyaliasingowego.

Ta flaga zdejmuje ograniczenie. Sama z siebie nie robi zupełnie nic: podnosi tylko pułap,
nie rusza samego pasma. Pasmem ruszają `-w` i `--core-cutoff <hz>` (bez SBR identyczne), a
ze zdjętym limitem mogą sięgnąć do połowy częstotliwości próbkowania.

Warto mieć jasność, co się za to dostaje. Treść powyżej 20 kHz kosztuje bity, które
schodzą z pasma słyszalnego, chyba że bitrate ma realny zapas, a strumienie z pasmem poza
typowym zakresem leżą poza zwykłą praktyką, więc niektóre dekodery mogą się na nie boczyć.
To świadome włączenie i właśnie dlatego jest to osobna flaga, a nie efekt uboczny
`-w`.

Wartości:
- pominięta: pasmo rdzenia ograniczone do 20 kHz (ustawienie fabryczne).
- `--uncap-bandwidth`: ograniczenie zdjęte, `-w` i `--core-cutoff` wolne aż do `sr/2`.

Przykład w kontekście:

    fdkaac -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 200 \
        -o out.m4a in96k.wav

Współpraca i konflikty: wymaga `-w` albo `--core-cutoff`; bez SBR oba dają ten sam strumień.
Pod SBR limit 20 kHz nie ma znaczenia, bo rdzeń kończy się na starcie SBR, dużo niżej.
Naturalnie łączy się z `--ath-scale` poniżej 256, gdy jest zapas bitrate'u, bo nie ma sensu
kodować 40 kHz pasma przy progach strojonych pod niedostatek.

Zmierzone: wejście 96 kHz, AAC-LC 400 kbps, szum szerokopasmowy. Bez flagi
`--core-cutoff 40000` daje `20000 Hz` w `--verbose` i praktycznie zerową energię powyżej
20 kHz. Z flagą `--verbose` raportuje `40000 Hz`, a zdekodowany plik niesie około 10
procent swojej energii w 20..24 kHz, około 20 procent w 24..32 kHz i około 20 procent w
32..44 kHz. Ten koder nie ma w repozytorium sygnału testowego 96 kHz, więc należy to
traktować jako pomiar wcześniejszy, a nie taki, który da się odtworzyć z dostarczonego
materiału.

## B. Kodowanie stereo

Dwa kanały na mniejszą liczbę bitów zamieniają w tym koderze trzy różne maszyny, które
pracują na różnych częściach sygnału i mają różny koszt. Przed sięgnięciem po którykolwiek
przełącznik z tej grupy trzeba jasno ustalić, o którą z tych trzech maszyn chodzi.

**MS (mid/side)** to bezstratny obrót pary kanałów. Zamiast lewego i prawego koder
zapisuje w danym paśmie współczynnika skali (scale factor band, SFB) kanał mid (suma,
L+R) i kanał side (różnica, L-R). Nic nie jest wyrzucane: dekoder obraca z powrotem i
dostaje dokładnie L/R. Przy skorelowanej muzyce sygnał side jest mały, więc kwantyzuje się
do znacznie mniejszej liczby bitów niż drugi pełny kanał. MS to decyzja o ekonomii bitów,
a nie o dźwięku, i podejmowana jest osobno dla każdego pasma i każdej ramki. Dostępne w
rdzeniu AAC, czyli w LC, LD, ELD oraz w rdzeniu pod HE-AAC, dopóki ten rdzeń jest jeszcze stereo.

**IS (intensity stereo)** jest stratne i kierunkowe. W zakresie górnych pasm koder
przestaje wysyłać dwa widma i wysyła jedną obwiednię energii plus kierunek, więc dekoder
maluje oba kanały z tego samego widma, używając współczynnika panoramy. To jest tanie, a
ucho toleruje to wysoko, gdzie słabo lokalizuje drobny szczegół widmowy. To też naprawdę
usuwa informację: wszystko, co różniło lewy kanał od prawego w tych pasmach, czyli
talerze ustawione mocno na jedną stronę, efekty przestrzenne, treść w przeciwfazie,
zapada się w stronę jednego sterowanego źródła. IS także działa w rdzeniu, a koder
domyślnie obchodzi się z nim ostrożnie.

**PS (parametric stereo)** to maszyna HE-AAC v2. Rdzeń staje się mono: jedno widmo AAC,
jedno górne pasmo SBR, a obraz stereo jedzie obok jako garść parametrów na ramkę
(różnice poziomów, koherencja, opcjonalnie faza). Poniżej mniej więcej 32 kbps kupuje to
używalny obraz stereo tam, gdzie prawdziwe kodowanie dwukanałowe byłoby „zagłodzone”,
czyli brakowałoby mu bitów, więc koder zerowałby słabe pasma albo kodował je najgrubszym krokiem.
PS nie jest ulepszeniem MS ani IS, lecz je zastępuje: przy `-p 29` nie ma kanału side,
któremu można by przesunąć budżet, ani pasm intensity, które można by rozmieścić, więc
każdy przełącznik `--ms*` i `--is*` z tej grupy jest bezczynny. I odwrotnie, `--ps*` nie
robi nic poza HE-AAC v2.

W praktyce: od 96 kbps w górę MS wykonuje prawie całą pracę i stroi się podział bitów między
mid i side (`--side-bias`, `--side-knee`). Między mniej więcej 40 a 80 kbps intensity stereo
zaczyna zarabiać na siebie w górnych pasmach (`--is-aggression`, a potem przełączniki
rozmieszczenia `--is*`). Poniżej około 32 kbps w stereo rozsądną drogą są HE-AAC v2 i
parametry PS, a najciekawsze przełączniki to tam te od rozdzielczości (`--ps-env-reduce`,
`--ps-bands`).

**Jak czytać numery SFB.** Każdy przełącznik pasmowy w tej grupie liczy pasma
współczynnika skali od `0` na DOLE widma. Pasmo 0 to bas, najwyższy numer to szczyt
kodowanego zakresu. Ile pasm istnieje, zależy od profilu i częstotliwości próbkowania:
przy LC 48 kHz stereo `--verbose` wypisuje `active SFBs (long): 49`, więc dozwolone
numery pasm to 0..48. Zawsze warto przeczytać tę linię przed ustawieniem zakresu, bo
liczba zmienia się wraz z konfiguracją, a numer zapamiętany z innego kodowania może
wypadać całkiem poza widmo.

**Bloki krótkie.** Podawane numery to zawsze pasma bloku DŁUGIEGO. Ramka krótka ma własną,
dużo grubszą tablicę (około 14 pasm przy 44,1 i 48 kHz, liczonych od `0` wewnątrz każdej
grupy okien), więc ten sam numer oznaczałby tam inną częstotliwość. Dlatego koder
przenosi każdy zakres na tablicę krótką według częstotliwości: każda krawędź trafia na
najbliższą w Hz krawędź pasma krótkiego, a przy dokładnym remisie na wyższą. Pasmo krótkie
należy do zakresu, gdy leży w nim co najmniej połowa jego szerokości. Wąski zakres, który
mieści się w jednym szerokim paśmie krótkim, dostaje to pasmo. W efekcie ramki długie
i krótkie obejmują ten sam obszar widma, przy każdej częstotliwości próbkowania, w DAB+
(960/120) i przy każdym ustawieniu `--short-groups`. `--verbose` wypisuje oba przekłady,
np. `long 18..30 (2531..9000 Hz) -> short 3..7 (2250..8250 Hz)`. Do 1.4.0-beta.1 numery były
porównywane wprost: `--msbands-lo 18` zostawiało ramki krótkie całkiem bez MS, a
`--msbands 18` (MS do 2,5 kHz) dawało w ramkach krótkich MS aż do 24 kHz.

**Dlaczego zakres powyżej kodowanego pasma nie robi nic.** Liczba SFB to pełna tabela dla
profilu, ale tylko pasma do kodowanego pasma cokolwiek niosą. Pasma powyżej częstotliwości
odcięcia są zerowe, a decyzja stereo o paśmie zer nie zmienia ani jednego bitu. To
prawdziwa pułapka przy przykładach z górnym pasmem: przy domyślnym paśmie LC około
15,75 kHz `--msbands-lo 44 --msbands-hi 48` daje dokładnie ten sam strumień co
`--msmask 0`, bo pasma 44..48 leżą powyżej kodowanego pasma, a MS jest w związku z tym
wyłączone wszędzie tam, gdzie ma to znaczenie. Po otwarciu pasma siłą przez `-w 20000` te
dwa ustawienia przestają być równe. Ustawiany zakres trzeba więc sprawdzać wobec
kodowanego pasma, a nie tylko wobec liczby SFB.

Wszystkie przełączniki tej grupy są wyłączone, dopóki się ich nie poda. Wyłączony znaczy
domyślne zachowanie FDK: wartość `-1`, a dla `--side-bias` i `--side-knee` wartość `0`.
`--side-bias` i `--side-knee` przyjmują liczby dziesiętne, z `.` albo `,`, do 3 miejsc po
przecinku; `--side-cutoff` przyjmuje herce; pozostałe to liczby całkowite i odrzucają
części dziesiętne.

### MS

Sam obrót MS nic nie kosztuje w dźwięku, bo jest bezstratny. O tym, jak obraz stereo
przechodzi przez koder, decyduje podział bitów między kanał mid i kanał side w pasmach
kodowanych jako MS, a przede wszystkim decyzja, które pasma side w ogóle zostaną zakodowane.
Każde pasmo side wyzerowane w ramce to kawałek obrazu przesunięty na środek.

Przełączniki tej części dzielą się na trzy poziomy:

- **Dwie dźwignie**: `--side-bias` przesuwa granicę, przy której pasmo side zostaje albo
  wypada, a `--side-knee` stopniuje pasma leżące przy tej granicy według ich własnego
  poziomu. Szerokość obrazu wobec jakości środka ustawia się tymi dwoma, osobno albo razem.
- **Wymiana na niskie bitrate'y**: `--side-cutoff` oddaje side powyżej zadanej
  częstotliwości, a jego bity przekazuje kanałowi mid.
- **Laboratorium**: `--msmask`, `--msbands`, `--msbands-lo`, `--msbands-hi`, `--ms-bias`,
  `--ms-precision`, `--mid-bias`. Sięgają wprost do decyzji MS i do progów pasm MS. Służą
  do odtworzenia warunku technicznego albo do sprawdzenia, co robi koder. Na muzyce żaden
  z nich nie wypadł w pomiarze lepiej niż dwie dźwignie, a kilka pogarsza wynik względem
  ustawień domyślnych.

Jak to mierzono w tej części: AAC-LC stereo, dekoder Fraunhofera, plik po dekodowaniu
wyrównany w czasie do źródła i porównany w dziedzinie mid/side od 100 Hz do górnej granicy
kodowanego pasma. *Błąd mid* i *błąd side* to energia błędu kodowania względem energii
danego kanału w źródle, w dB, więc niżej znaczy czyściej. *Wyzerowane pasma side* to liczba
pasm side na ramkę zakodowanych jako zero, odczytana z `--bit-stats`. Głównym materiałem
jest akustyczna muzyka stereo 44,1 kHz; `dense48` to syntetyczny gęsty sygnał z grupy E.

#### `--side-bias <dB>`

**Przesuwa granicę, przy której pasmo side zostaje albo wypada: główna dźwignia
szerokości stereo wobec jakości środka.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD i rdzeń stereo pod HE-AAC), pasm kodowanych
  jako MS. Bez działania w HE-AAC v2, a przy `--msmask 0` nie ma na czym działać.
- Wartości: `-24.0..50.0` dB; `0` = wyłączone (domyślnie, próg FDK)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

W każdym paśmie MS koder porównuje energię sygnału side z jego progiem maskowania. Poniżej
progu pasmo side jest zerowane, powyżej jest kodowane, i to tym dokładniej, im wyżej ponad
progiem leży. Sygnał side to wszystko, czym różnią się kanały lewy i prawy, więc od tego
jednego porównania zależy szerokość obrazu. `--side-bias` przesuwa próg side w każdym paśmie
MS o podaną liczbę decybeli.

Wartość dodatnia obniża próg. Mniej pasm side jest zerowanych, pozostałe są kwantyzowane
dokładniej, a w odsłuchu obraz robi się stabilniejszy: pogłos wybrzmiewa zamiast się urywać,
a dźwięk pomieszczenia zostaje na swoim miejscu. Budżet bitów nie rośnie, więc te bity
zabiera kanał mid i przy niskim bitrate'ie słychać, że płaci za to środek. Między +3 a +9 ta
wymiana zwykle się opłaca. Powyżej około +12 side przestaje zyskiwać, a mid dalej traci.

Wartość ujemna podnosi próg: pasma side wypadają, obraz zwęża się w stronę mono, a
uwolnione bity idą do mid. Małe wartości ujemne działają płynnie. Przy około -0,97 dB jest
jednak skok. Podniesiony próg przekracza tam minimalny SNR, który mechanizm unikania dziur
w FDK utrzymuje w każdym paśmie, i pasma side, które FDK trzymało przy życiu tylko dzięki
temu ratunkowi, wypadają razem. Wielkość skoku zależy od tego, jak bardzo ramkom brakuje
bitów. Przy wygodnym bitrate'ie skok jest umiarkowany, a dalej zwężanie postępuje
stopniowo. W ramkach już „zagłodzonych” większość pasm side istnieje tylko dzięki
ratunkowi, więc skok zabiera prawie cały side, a -1, -4 i -12 dają ten sam strumień. Do
łagodnego zwężania warto zostać w przedziale od 0 do -0,95 albo użyć ujemnego
`--side-knee`, który skoku nie ma.

Przykłady (akustyczna muzyka stereo, 96 kbps; bez przełącznika: mid -20,3 dB, side
-11,4 dB, 7,0 wyzerowanych pasm side na ramkę):
- Łagodnie, więcej dla side: `--side-bias 3`. Side czystszy o 0,8 dB, mid gorszy o 0,4 dB,
  3,4 wyzerowanych pasm side.
- Mocno, więcej dla side: `--side-bias 6`. Side czystszy o 1,3 dB, mid gorszy o 1,3 dB,
  1,2 wyzerowanego pasma. Zwykła wartość robocza.
- Przesadnie, więcej dla side: `--side-bias 24`. Side znów gorszy niż przy +12, a mid
  stracił 3,3 dB: bity wlane w kanał, który nie umie ich już wykorzystać.
- Łagodnie, węziej: `--side-bias -0,5`. Kilka brzegowych pasm side oddanych; mid i side
  w granicach 0,15 dB od ustawień domyślnych.
- Mocno, węziej: `--side-bias -3`. Za skokiem: 13,8 wyzerowanych pasm side, side gorszy o
  0,9 dB, mid czystszy o 0,2 dB.
- Przesadnie, węziej: `--side-bias -12`. Wyzerowanych 26 z około 37 pasm side, błąd side
  wyższy o 5,7 dB. Na `dense48` przy 40 kbps już `-6` zostawia side 6 bitów na ramkę,
  czyli praktycznie mono.

Współpraca i konflikty: `--side-knee` działa na tej samej decyzji i łączy się z tym
przełącznikiem bez kolizji: bias ustawia granicę, knee stopniuje pasma przy niej.
`--side-cutoff` usuwa side powyżej swojej częstotliwości niezależnie od biasu.
`--msmask 0` wyłącza MS i czyni ten przełącznik bezczynnym; `--msbands`, `--msbands-lo` i
`--msbands-hi` ograniczają pasma, do których sięga.

Zmierzone (akustyczna muzyka stereo; błąd mid / błąd side w dB / wyzerowane pasma side na
ramkę):

| `--side-bias` | 48 kbps | 96 kbps |
|---|---|---|
| bez przełącznika | -21,6 / -8,5 / 16,0 | -20,3 / -11,4 / 7,0 |
| `+3` | -19,1 / -7,9 / 11,6 | -19,8 / -12,3 / 3,4 |
| `+6` | -16,5 / -11,3 / 3,5 | -19,0 / -12,7 / 1,2 |
| `+9` | -15,4 / -12,3 / 1,8 | -18,6 / -13,1 / 0,6 |
| `-1` | -22,9 / -8,6 / 18,3 | -20,4 / -11,2 / 10,0 |
| `-3` | -24,2 / -8,0 / 20,7 | -20,5 / -10,5 / 13,8 |

Przy 48 kbps `+3` koduje już więcej pasm side, ale tak zgrubnie, że błąd side jest jeszcze
o 0,6 dB gorszy niż bez przełącznika; zysk przychodzi przy `+6`. Przy 128 kbps `+6`
poprawia side z -14,2 na -16,9 dB, a mid zmienia z -28,5 na -24,8 dB. Skok: przy 96 kbps
wartość -0,96 zeruje 7,7 pasma side, a -0,97 już 9,9. Na `dense48` przy 40 kbps z
`-w 18000` side po dekodowaniu jest o 1,4 dB cichszy niż w źródle przy -0,95 i o 10,8 dB
cichszy przy -0,97, tak samo przy -1, -4 i -12.

#### `--side-knee <dB>`

**Stopniuje pasma side leżące przy granicy kodowane/wyzerowane według ich własnego
poziomu: więcej albo mniej szczegółu w side, bez skoku.**

- Dotyczy: tak samo jak `--side-bias`.
- Wartości: `-24.0..50.0` dB; `0` = wyłączone (domyślnie, zachowanie FDK)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

`--side-bias` przesuwa granicę, a ten przełącznik działa na zapasie każdego pasma side,
czyli na tym, o ile jego energia leży ponad progiem. Od zapasu zależy, jak dokładnie
pasmo jest kodowane i czy w ogóle zostaje. Wartość dodaje się do zapasu przy granicy i
wygasa wyżej, więc głośna, dobrze zakodowana część side zostaje nietknięta, a każde pasmo
zachowuje dokładność zgodną ze swoim prawdziwym poziomem.

Przełącznik istnieje z powodu jednego szczegółu FDK. Gdy ramce brakuje bitów, FDK nie
wyrzuca słabego pasma od razu: ustawia jego próg około 0,97 dB pod energią i koduje je
najgrubszym krokiem. W muzyce mniej więcej co dziesiąte pasmo side ląduje na tej jednej
wartości, niezależnie od swojego rzeczywistego poziomu. Knee czyta zapas, którego chciała
pętla dopasowania bitów przed tym ratunkiem, więc takie pasma znów są stopniowane według
prawdziwego poziomu, zamiast ruszać się jednym blokiem. Dlatego ujemny knee zwęża płynnie
tam, gdzie ujemny bias skacze.

Wartość dodatnia dodaje dokładności: pasma do N dB pod progiem zostają, stopniowane według
tego, jak daleko pod nim leżały, a pasma tuż ponad progiem są kodowane do N dB dokładniej.
Wartość ujemna odbiera dokładność: najpierw odchodzą najsłabsze pasma przy granicy, a ich
bity idą do mid. W ramkach „zagłodzonych” większość pasm side istnieje tylko dzięki
ratunkowi, więc tam już mały ujemny knee usuwa większość z nich; przy wygodnym bitrate'ie
zwęża obraz stopniowo aż do -24.

W porównaniu z `--side-bias` o tej samej wartości dodatni knee daje podobny zysk w side
mniejszym kosztem mid. Przy 96 kbps `+6` daje side -12,9 dB i mid -19,4 dB, a bias -12,7 i
-19,0; przy 128 kbps knee daje -16,8 i -25,7, a bias -16,9 i -24,8. Knee pasuje do
materiału, w którym najważniejszy jest środek, bias do materiału, w którym liczy się
szerokość, a razem ustawiają granicę i łagodzą jej krawędź.

Przykłady (akustyczna muzyka stereo, 96 kbps; wartości odniesienia jak wyżej):
- Łagodnie, dokładniej: `--side-knee 2`. Wyzerowane pasma side z 7,0 do 3,4, side czystszy
  o 0,5 dB, mid gorszy o 0,2 dB.
- Mocno, dokładniej: `--side-knee 6`. 1,2 wyzerowanego pasma, side czystszy o 1,5 dB, mid
  gorszy o 0,8 dB.
- Przesadnie, dokładniej: `--side-knee 24`. Więcej bitów trafia do side niż do mid, a oba
  kanały wychodzą gorzej niż przy +12.
- Łagodnie, zgrubniej: `--side-knee -1`. Odchodzi około pięciu najsłabszych pasm side
  (11,8 wyzerowanych), mid czystszy o 0,1 dB.
- Mocno, zgrubniej: `--side-knee -6`. 19 wyzerowanych pasm side, side gorszy o 2,2 dB, mid
  czystszy o 0,3 dB. Wyraźnie węziej, ale wciąż stereo.
- Przesadnie, zgrubniej: `--side-knee -24`. Znika 36 z 37 pasm side, side jest o 8,9 dB
  cichszy niż w źródle: obraz mono z księgowością stereo.

Przełączniki diagnostyczne, zostawione do pomiarów i możliwe do usunięcia w kolejnej
wersji; bez `--side-knee` nie robią nic:
- `--side-knee-zone <x>`, `1.0..20.0`, domyślnie `3`: przesunięcie działa w pełni do
  |knee| dB ponad progiem i wygasa liniowo do zera przy x razy |knee| dB.
- `--side-knee-spread <dB>`, `0.1..24.0`, domyślnie `1`: przy ujemnym knee, ile decybeli
  knee trzeba, żeby uratowane pasma rozłożyć w pełni według ich prawdziwego zapasu.

Współpraca i konflikty: zaprojektowany do używania razem z `--side-bias`. Bezczynny przy
`--msmask 0`.

Zmierzone: przy 96 kbps liczba kodowanych pasm side na ramkę spada z ujemnym knee
równomiernie: 30,2 (bez przełącznika), 29,4 (-0,25), 27,6 (-0,5), 25,4 (-1), 22,2 (-3), 18,1
(-6), 10,5 (-12), 1,1 (-24). Na `dense48` przy 40 kbps z `-w 18000`, gdzie ramki są
„zagłodzone”, -0,25 zostawia 1,1 z 10,3 kodowanych pasm side, a od -4 strumień już się nie
zmienia. Przełączniki diagnostyczne ruszają niewiele: `--side-knee 6` ze strefą 1 i 10 różni
się o 0,06 dB błędu side, `--side-knee -3` z rozłożeniem 0,1 i 10 o 0,15 dB.

#### `--side-cutoff <Hz>`

**Od tej częstotliwości w górę koduje każde pasmo stereo jako czysty mid: górę obrazu
wymienia na czystszy środek.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD). Domyślnie wyłączone.
- Wartości: `1000..24000` Hz, przesuwane do pasma współczynnika skali, które zaczyna się
  na tej częstotliwości albo wyżej
- Wpisywanie: liczba całkowita, w Hz

Powyżej granicy sygnał side jest porzucany, a pasmo zapisywane tylko jako mid, z tanią maską
MS; czystego L/R tam nie ma. Poniżej granicy stereo jest nietknięte. Bity, które zabrałby
side, dostaje mid, a to ma znaczenie przy niskim bitrate'ie, gdy side powyżej kilku kHz i
tak jest w większości zerowany, a to, co z niego zostaje, jest zgrubne.

Poziom zostaje. Bez side oba kanały grają mid, który sam niesie mniej energii niż kanały
oryginału, gdy tylko L i R się różnią (nieskorelowane L/R wyszłyby o 3 dB ciszej). Dlatego
mid jest podnoszony do energii oryginału, najwyżej o 6 dB i tylko tam, gdzie side leży ponad
progiem maskowania (masker, czyli sygnał maskujący); side, który FDK i tak by wyzerowało,
nie jest dodawany. Dwóch rzeczy mid nie przeniesie przy żadnym poziomie: treści w
przeciwfazie (L = -R, jej mid to zero) i kierunku twardo spanoramowanego dźwięku, który
ląduje na środku.

Intensity stereo powyżej granicy: własna decyzja FDK o IS jest tam kasowana, bo czysty mid
jest tańszy. IS zostaje, gdy prosi się o nie jawnie:
- `--is-force-lo/-hi`: IS na podanych pasmach, zawsze.
- poluzowane kryteria IS (`--is-aggression`, `--is-corr-thresh`, `--is-lr-ratio`,
  `--is-min-sfbs`, `--is-admit` powyżej 5): własna decyzja FDK z tymi kryteriami.
  Pojedyncze spanoramowane obszary powyżej granicy zachowują wtedy kierunek (IS niesie go
  w każdym paśmie), a reszta zostaje mid, jak w intensity stereo z MPEG-1 Layer II.

Pasma, które oba kanały kodują jako PNS, zostają bez zmian. `--verbose` pokazuje pasmo, od
którego granica działa, w tablicy długiej i krótkiej. Obie zaokrąglają w górę do najbliższej
krawędzi pasma, więc w grubej tablicy krótkiej granica może przesunąć się tylko w górę (9
kHz: pasmo długie 31 na 9,0 kHz, krótkie 9 na 10,5 kHz), czyli usuwa mniej side, nigdy
więcej.

Przykłady (akustyczna muzyka stereo, 48 kbps):
- Łagodnie: `--side-cutoff 8000`. Informacja poboczna z 238 do 235 bitów na ramkę, mid
  odrobinę czystszy.
- Mocno: `--side-cutoff 5000`. Informacja poboczna 222 bity, błąd mid niższy o 0,7 dB, side
  powyżej 5 kHz niżej o około 27..31 dB.
- Przesadnie: `--side-cutoff 1000`. Praktycznie mono ze stereofonicznym basem.

Współpraca i konflikty: `--side-bias` i `--side-knee` działają dalej poniżej granicy. Przy
14 kbps zysk jest mały, bo FDK i tak zeruje tam prawie cały side; większą dźwignią jest
wtedy `--is-admit 0`.

Zmierzone: na `dense48` przy 40 kbps `8000` zostawia mid bez zmian, a side obniża o 1,2 dB;
`5000` daje mid czystszy o 1,3 dB i side cichszy o 3,4 dB. Ze źródłem 3..6 kHz
panoramowanym na przemian 4 dB w lewo i w prawo na tle miksu mono (40 kbps,
`--side-cutoff 1000`) sama granica zostawia 0 dB panoramy, a z `--is-aggression 100`
3..4 dB, tak jak w źródle.

### Laboratorium MS

Te przełączniki sięgają wprost do decyzji MS i do progów pasm MS. Odpowiadają na pytania
techniczne (co się dzieje w czystym L/R, przy MS w stałym zakresie pasm, przy decyzji
przechylonej w jedną stronę) i służą eksperymentom. Na muzyce żaden z nich nie daje po
dekodowaniu wyraźnie lepszego wyniku niż ustawienia domyślne, a kilka go pogarsza. Trzy z
nich przyjmują liczby Q8, czyli wewnętrzną skalę stałoprzecinkową FDK, w której `256`
oznacza 1,0 (patrz *Wspólne zasady*).

Przy CBR trzy przełączniki progów (`--ms-bias`, `--ms-precision`, `--mid-bias`) w
większości odkręca pętla dopasowania bitów, która i tak musi trafić w tę samą liczbę bitów
na ramkę. Przy VBR (`-m 1..5`) liczba bitów idzie za progami, więc tam `--ms-precision` i
`--mid-bias` słyszalnie działają, i na jakość, i na rozmiar pliku.

#### `--msmask <n>`

**Wymusza decyzję mid/side we wszystkich pasmach, zamiast zostawić koderowi wybór pasmo
po paśmie.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD i rdzeń stereo pod HE-AAC). Bez działania w
  HE-AAC v2, gdzie rdzeń jest mono.
- Wartości: `-1` automatycznie (domyślnie, FDK decyduje dla każdego pasma), `0` wyłączone
  (czyste L/R wszędzie), `1` włączone (MS w każdym paśmie)
- Wpisywanie: liczba całkowita

Koder zwykle sprawdza każde pasmo na oba sposoby i zostawia tańszy. W skorelowanej muzyce to
MS w niemal każdym paśmie, a ponieważ obrót jest bezstratny, wybór nic nie kosztuje w
dźwięku. Wymuszenie w którąkolwiek stronę to narzędzie techniczne.

`--msmask 0` gwarantuje, że koder nigdy nie zmiesza kanałów. Szum kwantyzacji zostaje
wtedy w kanale, który go wywołał, zamiast dzielić się między mid i side i trafiać do obu
wyjść. Ma to znaczenie, gdy plik pójdzie później przez usuwanie środka, wycinanie wokalu
albo inne odejmowanie jednego kanału od drugiego. Przy tym samym bitrate'ie kosztuje
jakość, wyraźnie w skorelowanej muzyce.

`--msmask 1` wymusza MS także tam, gdzie L/R byłoby tańsze. Przydaje się jako punkt
odniesienia; przy materiale o naprawdę niezależnych kanałach, na przykład dwóch
niezwiązanych źródłach rozstawionych na skraje, marnuje bity.

Wartości:
- `-1`: domyślnie, decyzja dla każdego pasma z porównania kosztów w koderze.
- `0`: czyste L/R w każdym paśmie. Nic nie jest mieszane między kanałami.
- `1`: MS w każdym paśmie.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --msmask 0 -o lr.m4a in.wav

Współpraca i konflikty: `--msmask 0` czyni bezczynną całą rodzinę mid/side, w tym
`--side-bias`, `--side-knee`, `--msbands` i `--mid-bias`, bo żadne pasmo nie jest w MS.
`--msbands`, `--msbands-lo` i `--msbands-hi` zawężają obszar, w którym maska obowiązuje.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msmask 0` podnosi błąd mid o 1,3 dB i błąd
side o 3,9 dB względem ustawień domyślnych. `--msmask 1` mieści się w 0,4 dB od nich, bo FDK
i tak koduje 39,5 z około 40 pasm na ramkę jako MS.

#### `--msbands <n>`

**Dopuszcza MS tylko w N najniższych pasmach współczynnika skali, a wszystko wyżej
zostawia w L/R.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo `0..N`, gdzie N to liczba aktywnych SFB; `n`
  oznacza, że pasma `0..n-1` mogą używać MS
- Wpisywanie: liczba całkowita

To ograniczenie w postaci „od dołu do”, liczone od basu w górę: `--msbands 6` pozwala pasmom
0..5 używać MS, a pasma 6 i wyższe trzyma w L/R. `--msbands 0` nie dopuszcza MS nigdzie, tak
samo jak `--msmask 0`. Od pary `--msbands-lo` i `--msbands-hi` różni się tylko kształtem
obszaru: ten przełącznik zawsze zaczyna od pasma 0.

Powód, żeby go użyć, jest techniczny: późniejszy proces, który wymaga nietkniętego L/R
powyżej jakiejś częstotliwości. Powodu jakościowego nie ma: każde zmierzone dotąd
ograniczenie pogarszało oba kanały przy tym samym bitrate'ie.

Przykłady:
- Łagodnie: `--msbands 40`. Do L/R wracają tylko najwyższe pasma; na większości
  materiału strumień prawie się nie zmienia, bo te pasma niosą niewiele.
- Mocno: `--msbands 20`. MS tylko w dolnej części widma; przy 96 kbps na muzyce
  akustycznej błąd mid rośnie o 1,3 dB, a błąd side o 3,5 dB.
- Przesadnie: `--msbands 2`. MS praktycznie nigdzie: niemal pełna cena czystego L/R, a MS
  wciąż sygnalizowane w dwóch pasmach.

Współpraca i konflikty: równoważne z `--msbands-lo 0 --msbands-hi n-1` (`--msbands 6` i
`--msbands-lo 0 --msbands-hi 5` dają ten sam strumień). Bezczynne przy `--msmask 0`. Limit
powyżej liczby aktywnych SFB działa jak brak limitu.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msbands 30` podnosi błąd mid o 0,9 dB i
błąd side o 2,4 dB, `--msbands 20` o 1,3 i 3,5 dB.

#### `--msbands-lo <n>`

**Najniższe pasmo współczynnika skali w zakresie, w którym wolno używać MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  podawaj razem z `--msbands-hi`
- Wpisywanie: liczba całkowita

Razem z `--msbands-hi` wyznacza okno od..do: MS wolno użyć w pasmach `lo..hi` włącznie i
nigdzie poza nimi. W odróżnieniu od `--msbands` okno nie musi zaczynać się od dołu widma,
więc tylko w ten sposób da się trzymać MS z dala od basu. Podawaj obie połówki pary; samo
`--msbands-lo` zostawia górną krawędź nieustawioną, a to nie jest zakres.

Przykłady:
- Łagodnie, okno od dołu: `--msbands-lo 2 --msbands-hi 48`. Prawie wszystko może używać MS,
  wykluczone są dwa najniższe pasma.
- Mocno, okno w środku: `--msbands-lo 10 --msbands-hi 30`. MS tylko w środku pasma, bas i
  góra w L/R.
- Przesadnie, okno na samej górze: `--msbands-lo 44 --msbands-hi 48` z `-w 20000`. MS
  tylko w pięciu pasmach samego powietrza. Bez `-w 20000` te pasma leżą powyżej domyślnej
  granicy około 15,75 kHz i ustawienie zamienia się w `--msmask 0` (patrz *Dlaczego zakres
  powyżej kodowanego pasma nie robi nic*).

Współpraca i konflikty: zakres trzeba czytać wobec `active SFBs` z `--verbose` i wobec
kodowanego pasma. Bezczynne przy `--msmask 0`.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msbands-lo 20 --msbands-hi 48` podnosi
błąd mid o 0,5 dB, a błąd side o 0,7 dB.

#### `--msbands-hi <n>`

**Najwyższe pasmo współczynnika skali, włącznie, w zakresie, w którym wolno używać MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  podawaj razem z `--msbands-lo`
- Wpisywanie: liczba całkowita

Górna krawędź opisanego wyżej okna, włącznie: `--msbands-hi 5` obejmuje pasmo 5, a pasma
powyżej idą do L/R. W blokach krótkich zakres obejmuje te same częstotliwości w tablicy
krótkiej (patrz *Bloki krótkie* wyżej), więc `--msbands-lo 18 --msbands-hi 30` przy 48 kHz
to pasma krótkie 3..7.

Przykłady:
- Łagodnie: `--msbands-lo 0 --msbands-hi 44`. Cztery najwyższe pasma wyłączone z MS.
- Mocno: `--msbands-lo 0 --msbands-hi 20`. MS mniej więcej do zakresu prezencji, wyżej
  L/R.
- Przesadnie: `--msbands-lo 0 --msbands-hi 0`. MS w dokładnie jednym paśmie, w basie.

Współpraca i konflikty: `--msbands-lo` to druga połówka; bez niej nie ma zakresu. Wartość
powyżej liczby aktywnych SFB działa jak brak górnego limitu. Bezczynne przy `--msmask 0`.

#### `--ms-bias <n>`

**Przechyla porównanie kosztów L/R i MS w stronę MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `0..255`, Q8 w logarytmicznych jednostkach kosztu FDK; `-1` i `0` = domyślnie
- Wpisywanie: liczba całkowita

Dla każdego pasma koder szacuje, ile kosztowałoby L/R, a ile MS, i wybiera taniej. Ten
przełącznik dodaje premię stronie MS. Ponieważ MS jest bezstratne, porównanie decyduje tylko
o sposobie zapisu pasma, a nie o jego brzmieniu, a FDK i tak wybiera MS w niemal każdym
paśmie skorelowanej muzyki.

Użyteczny zakres jest malutki. Wartości 1, 2 i 4 dają różne strumienie. Od około 8 (16 na
gęstym materiale) premia przewyższa każdą rzeczywistą różnicę kosztów, każde pasmo, któremu
wolno używać MS, je dostaje, a wyższe wartości nic już nie zmieniają: wynik jest bliski
`--msmask 1`.

Wartości:
- `-1` albo `0`: porównanie kosztów jak w FDK.
- `1..8`: decyzja coraz mocniej przechylona w stronę MS.
- `8..255`: MS wszędzie, gdzie wolno, ten sam strumień dla każdej wartości.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --ms-bias 4 -o biased.m4a in.wav

Współpraca i konflikty: bez sensu razem z `--msmask 0` albo `--msmask 1`, które usuwają
decyzję. Do wszystkiego, co słychać, służą `--side-bias` i `--side-knee`.

Zmierzone: akustyczna muzyka stereo przy 96 kbps, `128` podnosi błąd mid o 0,3 dB i nie
zmienia side; na `dense48` przy 40 kbps `255` nie zmienia niczego mierzalnego.

#### `--ms-precision <n>`

**Obniża progi każdego pasma MS, mid i side razem, więc oba kanały są kodowane
dokładniej.**

- Dotyczy: rdzenia AAC w stereo, tylko pasm MS. Bez działania w HE-AAC v2.
- Wartości: Q8 od `256` (bez zmian) w górę, `384` to 1,5x, `512` to 2x; poniżej 256 jest
  odrzucane; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

Progi pasma w MS są dzielone przez podany współczynnik, więc koder uznaje za słyszalną
większą część obu kanałów. Przesuwa mid i side w tę samą stronę o tyle samo, więc nie
zmienia równowagi między nimi; od równowagi są `--side-bias` i `--side-knee`.

Przy CBR prawie nic nie robi. Ramka dostaje tyle samo bitów, pętla dopasowania podnosi progi
z powrotem, aż ramka się zmieści, a wynik po dekodowaniu zostaje w granicach kilku
dziesiątych decybela od ustawień domyślnych. Przy VBR liczba bitów idzie za progami i tam
ten przełącznik podnosi jakość pary stereo, płacąc rozmiarem pliku.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo, bez przełącznika około 108 kbps):
- Łagodnie: `--ms-precision 384`. Plik większy o 3,1%, błąd mid niższy o 0,5 dB, side o
  0,4 dB.
- Mocno: `--ms-precision 512`. Plik większy o 5,3%, błąd mid niższy o 0,8 dB, side o
  0,7 dB.
- Przesadnie: `--ms-precision 2048`. Plik większy o 15,2%, błędy niższe o 2,2 i 1,9 dB:
  zakamuflowane podniesienie bitrate'u, które `-m` robi równiej.

Współpraca i konflikty: bezczynne przy `--msmask 0`. Przy VBR wyższe `-m` podnosi jakość we
wszystkich pasmach, nie tylko w MS.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `512` obniża błąd side o 0,13 dB,
`4096` o 0,4 dB, a mid nie zmienia się wcale; na `dense48` przy 40 kbps `1024` nie zmienia
niczego mierzalnego. VBR jak w przykładach.

Pod maską: współczynnik jest nakładany jako przesunięcie progów pasm w dziedzinie
logarytmicznej FDK, zaraz po obrocie MS.

#### `--mid-bias <n>`

**Podnosi próg kanału mid w pasmach MS, więc mid jest kodowany zgrubniej.**

- Dotyczy: rdzenia AAC w stereo, pasm MS. Bez działania w HE-AAC v2.
- Wartości: Q8, `256` = bez zmian, powyżej 256 podnosi próg mid; poniżej 256 jest
  odrzucane; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

Pomysł polega na oddaniu bitów z mid na rzecz side. W pomiarze nie wychodzi to dobrze. Przy
CBR pętla dopasowania odkręca większość efektu: przy 96 kbps `384` przesuwa mid i side o
około 0,1 dB, a dopiero `4096` daje side pełny decybel za 0,65 dB mid. Przy VBR mid po
prostu robi się zgrubniejszy, a plik mniejszy, i side nic nie zyskuje. Do szerokości wobec
środka służą `--side-bias` i `--side-knee`.

Przykłady (CBR, akustyczna muzyka stereo, 96 kbps):
- Łagodnie: `--mid-bias 288`. Bez mierzalnej zmiany.
- Mocno: `--mid-bias 1024`. Przy CBR side czystszy o 0,4 dB, mid gorszy o 0,2 dB; przy VBR
  `-m 3` plik mniejszy o 6,8%, mid gorszy o 1,7 dB, side bez zmian.
- Przesadnie: `--mid-bias 4096`. Side czystszy o 1 dB, mid gorszy o 0,65 dB. Dla
  porównania `--side-knee 3` daje side 0,8 dB za 0,3 dB mid.

Współpraca i konflikty: bezczynne przy `--msmask 0`. Razem z dodatnim `--side-bias` ciągnie
ten sam budżet z drugiego końca.

Zmierzone: na `dense48` przy 40 kbps `512` i `1024` nie zmieniają niczego mierzalnego. VBR
jak w przykładach.

### Intensity stereo (IS)

Intensity stereo to ta stratna maszyna. W ciągu górnych pasm koder przestaje wysyłać dwa
widma i wysyła jedną obwiednię energii plus kierunek, a dekoder odbudowuje oba kanały z tego
jednego widma. Oszczędność jest duża, kosztem jest separacja stereo dokładnie w tych
pasmach, a słyszalność tego kosztu zależy wyłącznie od tego, co tam żyje: skorelowana kąpiel
talerzy znosi to dobrze, mocno rozpanoramowany efekt albo szeroki pogłos nie.

Fabryczny FDK jest z IS bardzo ostrożny. Warunków dopuszczenia (gate) jest nie jeden, ale
kilka, a pasmo jest kodowane jako intensity tylko wtedy, gdy wszystkie są spełnione naraz:
warunek bitrate'u na pasmo, warunek korelacji między kanałami, minimalny ciąg przyległych
pasm oraz stabilny kierunek. Właśnie dlatego przesunięcie jednego progu tak często wygląda,
jakby nie robiło nic: kolejny warunek nadal blokuje. Samo rozluźnienie `--is-corr-thresh` na
przykład nie otwiera IS.

**Warto zacząć od `--is-aggression` z grupy A.** To prosty suwak dokładnie do tego
problemu: jedna wartość od 0 do 100, która rusza wszystkie warunki razem i spójnie. 0 to
ustawienie fabryczne, co w praktyce oznacza, że przy normalnych bitrate'ach IS prawie się
nie włącza, a 100 to prawdziwe maksimum: warunek bitrate'u porzucony, wymóg korelacji
rozluźniony, minimalny ciąg zredukowany do jednego pasma i tolerancja kierunku poszerzona.
Najpierw ustaw około 30 do 50 i posłuchaj, a potem idź wyżej, jeśli oszczędność jest warta
więcej niż szerokość. Dopiero po poznaniu działania suwaka warto sięgać po przełączniki
poniżej, które przesłaniają to, co on ustawił.

#### `--is-admit <x>`

**Przesuwa pierwszy warunek dopuszczenia w FDK, który decyduje, czy intensity stereo w
ogóle wolno użyć.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `0.0..20.0`, domyślnie w FDK `5`; `0` = nigdy; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

FDK dopuszcza intensity stereo tylko wtedy, gdy bitrate podzielony przez pasmo rdzenia, w
bitach na sekundę na herc, jest mniejszy od tej wartości. Przy bardzo niskich bitrate'ach IS
jest drogie: wymusza maskę MS pasmo po paśmie (około 22 bity na ramkę) i dokłada pozycje IS,
więc zamknięcie tego warunku potrafi uwolnić więcej bitów, niż IS oszczędza.
`--is-aggression` otwiera go niezależnie od tej wartości.

Zmierzone: przy 14 kbps stereo na gęstym materiale `--is-admit 0` zmniejsza udział
wyzerowanych pasm z 85% do 70%.

#### `--is <n>`

**Włącza albo wyłącza intensity stereo globalnie, zamiast zostawiać tę decyzję koderowi.**

- Dotyczy: rdzeń AAC w stereo (LC, LD, ELD oraz rdzeń stereo pod HE-AAC). Brak działania
  w HE-AAC v2.
- Wartości: `-1` auto (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

To przełącznik główny, i jest pozwoleniem, a nie rozkazem: `1` dopuszcza intensity stereo,
ale go nie rozmieszcza. Opisane wyżej warunki nadal decydują, które pasma naprawdę idą do
IS, więc samo `--is 1` przy materiale, który koder uznaje za nieodpowiedni, albo przy
bitrate'ie powyżej progu dopuszczenia, może zostawić strumień bez zmian. Aby IS naprawdę
zadziałało, trzeba połączyć ten przełącznik z `--is-aggression`.

`--is 0` to użyteczny koniec tego przełącznika samego w sobie. Gwarantuje brak
jakiegokolwiek kodowania intensity, czyli dokładnie to, co jest potrzebne, gdy separacja
stereo w górnych oktawach liczy się bardziej niż bity, albo gdy trzeba odizolować efekt
innego przełącznika i nie dopuścić, by IS ruszało się pod pomiarem.

Wartości:
- `-1`: ustawienie fabryczne. Koder decyduje, ostrożnie.
- `0`: intensity stereo nigdy nie użyte. Wszędzie pełne kodowanie L/R albo MS.
- `1`: intensity stereo dopuszczone, z zachowaniem zwykłych warunków.

```
fdkaac -p 2 -b 64000 --is 1 --is-aggression 40 in.wav -o is.m4a
```

Współpraca i konflikty: `--is-aggression` jest tym, co czyni `--is 1` widocznym.
`--is-force-lo`/`--is-force-hi` obchodzą te warunki w całości i działają nawet bez tego
przełącznika. W pełni niezależne stereo to `--msmask 0 --is 0`.

#### `--isbands <n>`

**Ogranicza liczbę pasm współczynnika skali, które mogą być kodowane jako intensity
stereo.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo liczba w granicach liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Pułap na całkowity zasięg kodowania intensity, wyrażony jako liczba pasm, a nie jako pozycja
w widmie. To hamulec na wypadek, gdy IS pomaga bitrate'owi, ale szkody mają zostać
ograniczone: koder sam wybiera, gdzie idzie intensity, lecz nie może pochłonąć połowy widma.

Ten pułap trzeba czytać wobec `active SFBs` z `--verbose`: przy LC 48 kHz stereo jest 49
pasm, więc pułap 40 jest ledwie pułapem, a pułap 4 jest surowy. Pasma za pułapem są kodowane
normalnie.

W ramkach krótkich pułap obowiązuje w każdej grupie okien osobno (każda grupa to odcinek
czasu z własną kopią tablicy pasm) i jest przeliczany na grubszą tablicę: przy 48 kHz
`--isbands 6` pozwala na 2 pasma krótkie w grupie. Do 1.4.0-beta.1 liczył przez grupy po
kolei, więc pierwsze grupy ramki transjentowej go zużywały, a reszta ramki zostawała
całkiem bez IS.

Przykłady:
- Łagodny pułap: `--isbands 24`. Połowa widma nadal może pójść w intensity. Przy
  większości materiału zmienia to niewiele, bo IS rzadko sięga tak daleko.
- Mocny pułap: `--isbands 8`. Intensity utrzymane w krótkim ciągu. Przydatne przy wysokim
  `--is-aggression`, gdzie powstrzymuje suwak od ucieczki.
- Przesadzony pułap: `--isbands 1`. Jedno jedyne pasmo intensity. Ustawienie skrajne:
  sygnalizacja zostaje, a oszczędność znika praktycznie w całości.

Współpraca i konflikty: `--is-lo`/`--is-hi` ograniczają, gdzie IS może pójść, a ten
ogranicza, ile. Łączą się. Bezczynne, gdy IS nigdy się nie włącza, więc najpierw warto
sprawdzić działanie z `--is-aggression`.

#### `--is-min-sfbs <n>`

**Minimalny ciąg przyległych pasm, zanim intensity stereo może się zacząć.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` domyślnie, co oznacza 6, albo liczba pasm
- Wpisywanie: liczba całkowita

Jeden z warunków dopuszczenia. Koder odmawia rozpoczęcia kodowania intensity na odosobnionym
paśmie czy dwóch; wymaga ciągłego obszaru o długości co najmniej tylu pasm, z których
wszystkie spełniają pozostałe warunki. Uzasadnienie jest takie, że krótki fragment intensity
otoczony pasmami kodowanymi normalnie to słyszalny szew, mały obszar, w którym obraz nagle
zapada się, a potem znów otwiera.

Po obniżeniu tej liczby IS łapie także krótkie fragmenty, czyli wyzwala się znacznie
częściej i na większej liczbie materiału, a ryzykiem są właśnie te szwy. Po podniesieniu IS
staje się rzadsze: kwalifikują się tylko szerokie, konsekwentnie skorelowane obszary, co
jest ustawieniem zachowawczym.

To przełącznik zaawansowany. `--is-aggression` już nim rusza w ramach swojego
skoordynowanego przebiegu, więc warto go używać tylko przy ręcznym strojeniu z agresją na 0,
pamiętając, że jawna wartość przesłania to, co ustawił suwak.

Przykłady:
- Łagodnie, więcej IS: `--is-min-sfbs 4`. Kwalifikują się nieco krótsze obszary. IS trochę
  częściej.
- Mocno, więcej IS: `--is-min-sfbs 2`. Krótkie fragmenty przyjmowane. Wyraźnie więcej
  kodowania intensity i pierwszy punkt, w którym szwy stają się prawdopodobne.
- Przesadnie, więcej IS: `--is-min-sfbs 1`. Każde pojedyncze pasmo może stać się
  intensity. Ustawienie skrajne: intensity rozsypane po widmie po jednym paśmie.
- Łagodnie, mniej IS: `--is-min-sfbs 8`. Nieco rzadziej niż przy ustawieniu fabrycznym.
- Mocno, mniej IS: `--is-min-sfbs 16`. Kwalifikują się tylko szerokie obszary; przy
  większości materiału IS prawie nigdy nie odpala.
- Przesadnie, mniej IS: `--is-min-sfbs 40`. Tak długi ciąg praktycznie nie istnieje, więc
  jest to intensity stereo wyłączone kuchennymi drzwiami. Skrajna postać ustawienia
  zachowawczego, a `--is 0` mówi to samo wprost.

Współpraca i konflikty: to jeden warunek z kilku, więc samo jego obniżenie może nic nie
zmienić, jeśli warunek korelacji albo bitrate'u nadal blokuje. Faktycznie przesłonięty przez
`--is-force-lo`/`-hi`, które go obchodzą. Lepszym wyborem jest `--is-aggression`.

#### `--is-corr-thresh <n>`

**Określa, jak podobne muszą być oba kanały w paśmie, żeby intensity stereo było w ogóle
dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = korelacja 1,0; `-1` domyślnie, czyli około 243, co oznacza
  mniej więcej 0,95
- Wpisywanie: liczba całkowita

Warunek korelacji, a przy tym ten, po który sięga się pierwszy. Fabryczne strojenie stawia
ostry wymóg: oba kanały muszą być w paśmie podobne w jakichś 95 procentach, zanim koder
rozważy zastąpienie ich jedną obwiednią i kierunkiem. Bardzo niewiele prawdziwej muzyki jest
tak skorelowane w górnych pasmach, i to właśnie jest główny powód, dla którego IS wydaje się
domyślnie nieaktywne.

Kierunek jest przeciwny intuicji, więc warto to przeczytać dwa razy: NIŻEJ znaczy WIĘCEJ
intensity. Poprzeczka, którą materiał ma przeskoczyć, zostaje obniżona, więc przeskakuje ją
więcej pasm. 180 to około 0,70, czyli znacznie luźniej niż fabrycznie. Za nisko i naprawdę
różne kanały zostają zakodowane jako jedno sterowane źródło, co słychać jako błędy kierunku
i jako zlepianie się góry obrazu.

To jeden z kilku warunków, a stąd praktyczny wniosek, który warto powiedzieć wprost: zmiana
samego tego przełącznika nie otwiera intensity stereo. Jeśli nadal blokuje warunek bitrate'u
albo minimalna seria, strumień nie ruszy się niezależnie od ustawionej tu wartości.

Przykłady:
- Łagodnie, więcej IS: `--is-corr-thresh 230`. Około 0,90. Nieco luźniej niż fabrycznie.
- Mocno, więcej IS: `--is-corr-thresh 180`. Około 0,70. Kwalifikuje się istotnie więcej
  pasm; to typowy cel ręcznego strojenia.
- Przesadnie, więcej IS: `--is-corr-thresh 60`. Około 0,23, czyli przyjmowane są kanały
  ledwo ze sobą związane. Ustawienie skrajne: słyszalne błędy kierunku wszędzie, gdzie
  to zadziała.
- Łagodnie, mniej IS: `--is-corr-thresh 250`. Ciaśniej niż fabrycznie, IS trochę rzadsze.
- Mocno, mniej IS: `--is-corr-thresh 254`. Kwalifikują się tylko kanały niemal identyczne.
- Przesadnie, mniej IS: `--is-corr-thresh 256`. Wymagana korelacja idealna, której
  prawdziwy materiał w praktyce nigdy nie osiąga. Ustawienie skrajne: intensity stereo
  wyłączone niemożliwym testem, podczas gdy `--is 0` byłoby jaśniejsze.

Współpraca i konflikty: samo z siebie nie otwiera IS, bo pozostałe warunki wciąż obowiązują.
Przesuwane automatycznie przez `--is-aggression`, a podana tu jawna wartość przesłania
suwak. Pomijane przez `--is-force-lo`/`-hi`.

#### `--is-lr-ratio <n>`

**Określa, jak równa musi być w paśmie równowaga energii lewo/prawo, żeby intensity
stereo było dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = 1,0; `-1` domyślnie, czyli około 179, co oznacza mniej więcej
  0,7
- Wpisywanie: liczba całkowita

Warunek panoramy. Korelacja mówi, że oba kanały mają ten sam kształt; ten warunek mówi, że
mają porównywalny poziom. Pasmo, w którym jedna strona nosi niemal wszystko, jest źle
wybranym miejscem na kodowanie intensity, bo jedno wspólne widmo plus kierunek musi
przedstawić mocno niesymetryczną parę, a cały ciężar spada na parametr kierunku.

Niższe wartości dopuszczają pasma bardziej niesymetryczne i dlatego dają więcej kodowania
intensity, w tym samym przeciwnym do intuicji kierunku co próg korelacji. Wyższe wartości
wymagają równiejszego balansu i czynią IS rzadszym.

To trzeci z trzech progów do ręcznego strojenia i ten, który ustawia się na końcu. Kolejność
pracy: korelacja, minimalna seria, potem proporcja. Dwa pierwsze decydują, czy IS w ogóle
zadziała, ten głównie decyduje, gdzie.

Przykłady:
- Łagodnie, więcej IS: `--is-lr-ratio 160`. Trochę większa tolerancja na pasma nierówne.
- Mocno, więcej IS: `--is-lr-ratio 110`. Przyjmowane są pasma wyraźnie niesymetryczne,
  więcej intensity w całym widmie.
- Przesadnie, więcej IS: `--is-lr-ratio 20`. Przyjmowana praktycznie dowolna asymetria.
  Ustawienie skrajne: materiał spanoramowany skrajnie zostaje sprowadzony do jednego
  widma plus wartości panoramy, a to dokładnie ta sytuacja, w której IS brzmi najgorzej.
- Łagodnie, mniej IS: `--is-lr-ratio 200`. Trochę surowiej niż fabrycznie.
- Mocno, mniej IS: `--is-lr-ratio 240`. Kwalifikują się tylko pasma dobrze zbalansowane;
  IS staje się rzadkie na czymkolwiek z szerokim miksem.
- Przesadnie, mniej IS: `--is-lr-ratio 256`. Wymagana dokładna równość poziomów.
  Ustawienie skrajne, które wyłącza IS warunkiem niespełnialnym przez żaden materiał.

Współpraca i konflikty: to jeden z kilku warunków; sam rzadko zmienia strumień. Przesuwany
przez `--is-aggression`, przesłaniana przez jawną wartość. Pomijana przez
`--is-force-lo`/`-hi`.

#### `--is-lo <sfb>`

**Dopuszcza intensity stereo tylko od tego pasma współczynnika skali w górę.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Ograniczenie miejsca, a nie wyzwalacz. Pasma poniżej podanego numeru zostają L/R albo MS bez
względu na warunki dopuszczenia; pasma od niego w górę podlegają zwykłym warunkom. Ten
przełącznik może kodowanie intensity tylko odebrać, nigdy dodać.

Oczywiste zastosowanie to ochrona dołu widma. Intensity stereo wysoko to rozsądny kompromis,
intensity stereo w środku pasma albo w basie już nie, bo tam ucho lokalizuje po poziomie i
czasie i tam zapadnięty obraz słychać natychmiast. Warto znać pułapkę: przy niskim
bitrate'cie koder lubi umieszczać IS na pasmach względnie NISKICH, niżej niż podpowiada
intuicja, więc żeby zobaczyć działanie tego przełącznika, trzeba przeszukiwać małe wartości,
a nie zaczynać od 30.

Przykłady:
- Łagodnie, wypchnij IS w górę: `--is-lo 10`. Bas i dolny środek chronione. Często
  wystarcza, żeby usunąć najgorsze lokalizacje.
- Mocno, wypchnij IS w górę: `--is-lo 25`. Intensity ograniczone do górnej połowy widma.
  Rozsądna wartość wyjściowa do starannej pracy przy średnim bitrate'cie.
- Przesadnie, wypchnij IS w górę: `--is-lo 46`. Intensity dozwolone tylko w kilku
  najwyższych pasmach, które przy domyślnym kodowanym paśmie mogą w ogóle leżeć ponad
  zakresem kodowania. Ustawienie skrajne i ta sama pułapka co w przykładzie z górnym
  pasmem MS: trzeba to sprawdzić wobec kodowanego pasma, bo inaczej oznacza po cichu
  „żadnego IS”.

Współpraca i konflikty: łączy się w okno z `--is-hi`. Tylko ogranicza, więc nie potrafi
sprawić, żeby IS wystąpiło; do tego służy `--is-aggression`. Nie ma nic wspólnego z
`--is-force-lo`, czyli przełącznikiem wymuszania o podobnej nazwie.

#### `--is-hi <sfb>`

**Dopuszcza intensity stereo tylko do tego pasma współczynnika skali włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Górna krawędź dozwolonego okna intensity, włącznie, i ten sam rodzaj ograniczenia co
`--is-lo`: zawęża miejsca, w których koder może umieścić IS, i nigdy tego nie wymusza.
Razem para definiuje obszar od..do, a wszystko poza nim zostaje kodowane konwencjonalnie.

Ograniczanie od góry to kierunek rzadszy, ale ma wyraźne zastosowanie. Gdy właśnie sama góra
widma nosi najważniejszą informację stereo, czyli powietrze, lokalizację blach,
pomieszczenie, można oddać intensity zatłoczony górny środek, a najwyższe pasma zatrzymać w
prawdziwym stereo.

Przykłady:
- Łagodne ograniczenie: `--is-hi 44`. Z intensity wyłączone są tylko pasma najwyższe.
- Mocne ograniczenie: `--is-hi 30`. Intensity ograniczone poniżej góry widma; pasmo
  powietrza zachowuje prawdziwą separację.
- Przesadne ograniczenie: `--is-hi 3`. Okno tak nisko, że warunki w jego wnętrzu
  praktycznie nigdy nie zostaną spełnione. Ustawienie skrajne, które sprowadza się do
  wyłączenia IS, a `--is 0` jest jaśniejsze i tańsze w myśleniu.

Współpraca i konflikty: `--is-lo` to druga połowa okna. Łączy się z `--isbands`, które
ogranicza liczbę, a nie pozycję. Nie wchodzi w interakcję z zakresem wymuszonym.

#### `--is-force-lo <sfb>`

**Wymusza intensity stereo od tego pasma w górę, pomijając wszystkie warunki
dopuszczenia.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-hi`
- Wpisywanie: liczba całkowita

To przełącznik laboratoryjny i zachowuje się inaczej niż wszystko powyżej. Warunek
korelacji, minimalna seria, warunek równowagi poziomów i warunek bitrate'u są pomijane:
pasma w wymuszonym zakresie są kodowane jako intensity, bo tak je ustawiono. Działa bez
żadnego innego ustawionego przełącznika IS, więc nie potrzebuje `--is 1` i ignoruje
`--is-aggression`.

Warto rozumieć, co to znaczy dla dźwięku. Kodowanie intensity zachowuje jedno widmo i
kierunek, więc w wymuszonym zakresie kanały nie mogą się już różnić niczym poza poziomem i
znakiem. Na materiale nieskorelowanym wynik nie jest subtelny; to celowe, słyszalne
zapadnięcie obrazu i może brzmieć fatalnie źle. I o to w tym przełączniku chodzi: pozwala
usłyszeć i zmierzyć mechanizm w pełnej sile, a także zbudować konkretny efekt. Strumień
pozostaje zgodny ze standardem, więc każdy dekoder go odtworzy.

Trzeba go używać w parze z `--is-force-hi`. Najpierw warto ustawić go wysoko w widmie, by
usłyszeć, co intensity robi tam, gdzie ma być używane, a potem schodzić w dół, by usłyszeć,
dlaczego te warunki istnieją.

Przykłady:
- Łagodne wymuszenie: `--is-force-lo 40 --is-force-hi 48`. Intensity tylko na najwyższych
  pasmach, czyli z grubsza tam, gdzie koder i tak by je umieścił. Najbliżej nastawy
  używalnej.
- Mocne wymuszenie: `--is-force-lo 20 --is-force-hi 48`. Cała górna połowa wymuszona na
  intensity. Wyraźne zwężenie obrazu powyżej środka pasma i realna oszczędność bitrate'u.
- Przesadne wymuszenie: `--is-force-lo 0 --is-force-hi 48`. Całe widmo kodowane jako jeden
  sterowany kanał. Ustawienie skrajne i przydatna demonstracja: tyle właśnie kosztuje
  obraz stereo, wszystko naraz.

Współpraca i konflikty: pomija `--is-corr-thresh`, `--is-min-sfbs`, `--is-lr-ratio` oraz
warunek bitrate'u, więc żadne z nich go nie ogranicza. To nie to samo co `--is-lo`, które
jedynie ogranicza. Potrzebuje `--is-force-hi`, żeby wyznaczyć górę zakresu.

Zmierzone: `--is-force-lo` i `--is-force-hi` zmieniają strumień bez żadnego innego podanego
przełącznika IS.

#### `--is-force-hi <sfb>`

**Górne pasmo wymuszonego zakresu intensity, włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-lo`
- Wpisywanie: liczba całkowita

Druga połowa wymuszonego zakresu, razem z podanym pasmem. Obie połowy razem wyznaczają
obszar kodowany jako intensity niezależnie od tego, jak wygląda materiał.

Zakres trzeba trzymać wewnątrz kodowanego pasma. Wymuszanie intensity na pasmach powyżej
częstotliwości odcięcia wymusza je na ciszy, co nic nie zmienia, więc poszukiwania winy w
przełączniku są bezcelowe: działa on bez zarzutu.

Przykłady:
- Łagodnie: `--is-force-lo 44 --is-force-hi 46`. Trzy pasma wymuszonego intensity blisko
  góry. Wąska sonda do słuchania A/B.
- Mocno: `--is-force-lo 30 --is-force-hi 48`. Szeroki wymuszony obszar przez górne widmo.
- Przesadnie: `--is-force-lo 5 --is-force-hi 48`. Wymuszone wszystko powyżej basu.
  Ustawienie skrajne: bas zachowuje swoje stereo, a cała reszta to sterowane mono, co
  brzmi dokładnie tak dziwnie, jak się czyta.

Współpraca i konflikty: bez `--is-force-lo` nie ma sensu. Niezależne od przełączników
ustawiających warunki dopuszczenia oraz od `--is-lo`/`--is-hi`. Zakres trzeba sprawdzić
wobec `active SFBs` z `--verbose` oraz wobec kodowanego pasma.

### Parametric stereo (PS)

Parametric stereo to mechanizm, z którego korzysta HE-AAC v2 (`-p 29`), i jest to zupełnie
inna propozycja niż wszystko powyżej. Koder przebiegu widzi jeden kanał: oba wejścia są
zdownmiksowane do mono, sygnał mono dostaje rdzeń AAC plus SBR, a obraz stereo podróżuje
osobno jako niewielki zestaw parametrów na ramkę. Dekoder bierze sygnał mono, dekoreluje go
i odbudowuje z tych parametrów lewy i prawy kanał.

Parametrów jest niedużo i są bardzo konkretne. IID to różnica poziomów między kanałami w
paśmie parametru, i to ona umieszcza źródło spanoramowane poziomem. ICC to koherencja, czyli
jak podobne są oba kanały, i to ona decyduje, czy pasmo brzmi jak źródło punktowe, czy jak
szerokie, rozmyte pole. Opcjonalnie IPD przenosi różnicę fazy między kanałami, a OPD mówi,
gdzie ta rotacja leży względem downmiksu. I to cały obraz stereo: kilkadziesiąt liczb na
ramkę zamiast drugiego widma.

Dlatego właśnie PS działa przy bitrate'ach, przy których nic innego nie działa. Przy 24 albo
32 kbps prawdziwy rdzeń stereo „zagłodziłby” oba kanały, czyli zabrakłoby im bitów; rdzeń
mono przy tej samej przepływności ma się wygodnie, a parametry kosztują niemal nic. Dlatego
też PS ma własne tryby awarii, które nie mają nic wspólnego z szumem kwantyzacji: obraz może
być zbyt zgrubnie próbkowany w częstotliwości (za mało pasm), zbyt zgrubnie próbkowany w
czasie (za mało obwiedni) albo chwilowo nieobecny (ramki wysłane zupełnie bez parametrów).
Przełączniki poniżej zajmują się dokładnie tymi trzema sprawami.

Jedna nastawa warta zapamiętania z tego rozdziału to `--ps-env 4 --ps-env-reduce 0`.
Poproszenie o cztery obwiednie samo w sobie często nic nie zmienia, bo automatyczna pętla
redukcji z powrotem je zwija; dopiero wyłączenie tej pętli przynosi poprawę.

Wszystko tutaj wymaga `-p 29`. Przy LC, HE-AAC v1, LD albo ELD nie ma warstwy PS, więc te
przełączniki nic nie robią.

#### `--ps <n>`

**Wymusza wysyłanie parametru stereo IID albo go zabrania, zamiast zostawiać tę decyzję
heurystyce kodera.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

IID to parametr różnicy poziomów, kręgosłup obrazu PS. Koder decyduje o jego wysyłaniu na
podstawie heurystyki różnicy głośności: jeśli oba kanały leżą niemal na tym samym poziomie,
może dojść do wniosku, że nie ma czego opisywać, i nie wysłać nic.

`--ps 0` całkowicie tłumi IID, co spłaszcza obraz w stronę samego downmiksu mono plus
tego, co daje ICC. To narzędzie tępe i przydatne głównie jako punkt odniesienia: wystarczy
zakodować raz z nim, posłuchać, jak brzmi sam rdzeń, i porównać.

`--ps 1` wymusza obecność IID w strumieniu bezwarunkowo. To ta nastawa, która ma znaczenie
na materiale źle ocenianym przez heurystykę, przede wszystkim o niemal równych poziomach,
a do tego właśnie ona przenosi liczbę pasm fazy przy włączonym IPD.

Wartości:
- `-1`: ustawienie fabryczne. Decyduje heurystyka różnicy głośności.
- `0`: IID nigdy nie wysyłane. Obraz spłaszczony, prawie mono.
- `1`: IID wysyłane zawsze.

```
fdkaac -p 29 -b 32000 --ps 1 in.wav -o ps.m4a
```

Współpraca i konflikty: `--ps-ipd 1` i tak sprawia, że strumień sygnalizuje IID, bo to
jedyna droga, którą liczba pasm fazy dochodzi do dekodera. `--ps-iid-quant` wybiera siatkę,
na której kwantowane jest IID. `--ps-icc` steruje drugim głównym parametrem.

#### `--ps-iid-quant <n>`

**Wybiera zgrubną albo dokładną siatkę kwantyzacji różnic poziomów IID.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` zgrubna, `1` dokładna
- Wpisywanie: liczba całkowita

IID jest kwantowane na stałej siatce różnic poziomów. Siatka dokładna rozdziela mniejsze
różnice poziomów i dlatego umieszcza źródła precyzyjniej, zwłaszcza blisko środka, gdzie
małe różnice robią większość roboty. Siatka zgrubna wydaje mniej bitów na parametr i na
materiale, którego obraz błądzi lekko z ramki na ramkę, potrafi brzmieć stabilniej, bo małe
fluktuacje analizy kwantują się do tego samego kroku, zamiast drgać między dwoma.

Przy bitrate'ach, w których używa się PS, bity parametrów to niewielka część budżetu, więc
dla muzyki z prawdziwą lokalizacją siatka dokładna jest zwykle właściwym wyborem. Zgrubną
warto wypróbować na mowie o bardzo niskiej przepływności albo na materiale, w którym słychać
drganie obrazu.

Wartości:
- `-1`: siatka fabryczna dla danej konfiguracji.
- `0`: siatka zgrubna. Mniej bitów parametrów, mniej precyzyjna lokalizacja, stabilniej
  przy drganiu.
- `1`: siatka dokładna. Dokładniejsza lokalizacja, trochę więcej bitów parametrów.

```
fdkaac -p 29 -b 32000 --ps-iid-quant 1 in.wav -o fine.m4a
```

Współpraca i konflikty: ma sens tylko wtedy, gdy IID jest faktycznie wysyłane, więc w parze
z `--ps 1`, gdy heurystyka je tłumi. Niezależne od `--ps-bands`, które decyduje, ile
wartości IID istnieje, a nie jak precyzyjnie każda z nich jest zakodowana.

#### `--ps-icc <n>`

**Wymusza włączenie albo wyłączenie parametru koherencji.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

ICC mówi dekoderowi, jaka część pasma ma pochodzić z dekorelowanej kopii sygnału mono, a nie
z samego sygnału mono. Wysoka koherencja to zwarte źródło punktowe, niska koherencja to
źródło szerokie i rozmyte. Na materiale z pogłosem to ICC, nie IID, decyduje o różnicy
między pomieszczeniem a suchym sygnałem mono z wartością panoramy.

`--ps-icc 0` odbiera tę kontrolę, więc pasma wracają do przypadku koherentnego, a wynik
brzmi bardziej sucho i węziej, nawet gdy IID nadal poprawnie umieszcza źródła. `--ps-icc 1`
wymusza obecność danych koherencji w strumieniu tam, gdzie koder mógł uznać je za
niepotrzebne, i to jest nastawa do wypróbowania na materiale ambientowym albo mocno
zapogłosowanym, którego szerokość brzmi zapadnięta.

Wartości:
- `-1`: ustawienie fabryczne. Decyduje koder.
- `0`: brak ICC. Bardziej sucho, węziej, wrażenie źródła punktowego.
- `1`: ICC wysyłane zawsze. Materiał rozmyty zachowuje swoją szerokość.

```
fdkaac -p 29 -b 32000 --ps-icc 1 in.wav -o icc.m4a
```

Współpraca i konflikty: `--ps-icc-mode` zmienia wyłącznie sposób sygnalizowania tej samej
macierzy i nie jest jakościową alternatywą dla tego przełącznika. Uzupełnia `--ps`, które
steruje parametrem poziomu.

#### `--ps-ipd <0|1>`

**Przesyła międzykanałową różnicę fazy w rozszerzeniu PS.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłączone (ustawienie fabryczne, faza nigdy nie wysyłana), `1` włączone
- Wpisywanie: liczba całkowita, włącz/wyłącz

Bez fazy PS pozycjonuje dźwięk wyłącznie różnicami poziomów i koherencją. To pokrywa
wszystko spanoramowane poziomem, czyli większość współczesnego miksu, ale nie potrafi
przedstawić źródła umieszczonego CZASEM. Prawdziwe opóźnienie międzykanałowe, czyli to, jak
lokalizacja niskich częstotliwości działa w naturze, jest dla IID i ICC niewidoczne: oba
kanały niosą ten sam sygnał na tym samym poziomie i różnią się tylko momentem dotarcia.
Włączenie IPD wysyła ten kąt, skwantowany na 8 zdefiniowanych krokach po pi/4, w
rozszerzeniu PS z prefiksem długości.

Zakres, w którym to pomaga, jest wąski i warto go znać dokładnie. IPD jest kodowane dla
dolnych 5 pasm parametru w konfiguracji 10-pasmowej i dolnych 11 w konfiguracji 20-pasmowej,
co w częstotliwości oznacza z grubsza dolne 690 Hz: te 11 pasm leży wewnątrz pierwszych
trzech pasm QMF. Powyżej 690 Hz ani odsłuch, ani pomiar nie pokażą zupełnie nic. Materiał,
który na tym korzysta, to niskie i nisko-średnie pasmo, szerokie, miękkie źródła, pogłos
oraz prawdziwe międzykanałowe mikroopóźnienia.

Jest też sufit, przy czym wynika on z samej reprezentacji, a nie z jakiejś wady. Siatka o
kroku 45 stopni może śledzić opóźnienie tylko dopóty, dopóki wytwarzana przez nie faza
pozostaje jednoznaczna w całym kodowanym zakresie. Na górze zakresu IPD opóźnienie około 0,7
ms obraca się już przez niemal całe plus minus 180 stopni, więc siatka nie potrafi już
powiedzieć, gdzie ono jest. To klasyczna granica niejednoznaczności fazy ITD i ten sam
powód, dla którego słuch opiera się na przesłankach czasowych głównie nisko.

Jedno praktyczne sprzężenie: rozszerzenie fazy nie nosi własnej liczby pasm. Dekoder
wyprowadza ją z sygnalizacji IID, więc włączenie `--ps-ipd` sprawia, że strumień sygnalizuje
też IID. To parametr tani i zawsze dozwolony, a przy tym jedyna droga, którą liczba pasm
dochodzi do dekodera.

Zgodność jest bezpieczna z samej konstrukcji. Ładunek siedzi w rozszerzeniu z prefiksem
długości, więc dekoder bez syntezy fazy czyta liczbę bajtów i pomija je, na co standard
wyraźnie pozwala. Trzeba jednak zwrócić uwagę, które dekodery z tego korzystają: dekoder
Fraunhofera IGNORUJE rozszerzenie IPD, więc dekodowanie nim nie pokaże żadnej zmiany. ffmpeg
z niego korzysta. Dekoder należy więc wybrać świadomie, przed orzeczeniem, że przełącznik
nic nie zrobił.

Wartości:
- `0`: ustawienie fabryczne. Brak fazy w strumieniu.
- `1`: IPD przesyłane dla dolnych 5 (10 pasm) albo 11 (20 pasm) pasm parametru, a razem z
  nim sygnalizowane IID.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 1 in.wav -o ipd.m4a
```

Współpraca i konflikty: `--ps-opd` to druga połowa pary fazowej i ma sens tylko przy tym
włączonym. Wymusza sygnalizację IID, więc pokrywa się z `--ps 1`.

Zmierzone, z ffmpegiem jako dekoderem: sondy zbudowane jako identyczny sygnał w obu
kanałach, równy poziom, przesunięty wyłącznie opóźnieniem międzykanałowym, z porównaniem
zdekodowanego profilu fazy wobec źródła wewnątrz zakresu IPD od 60 do 690 Hz.

| Opóźnienie międzykanałowe | błąd fazy, IPD wyłączone | błąd fazy, IPD włączone |
|---|---|---|
| 0.25 ms | 35.0 deg | 14.9 deg |
| 0.4 ms | 56.1 deg | 24.9 deg |
| 0.7 ms | 67.4 deg | 71.8 deg |

Przy 0,25 i 0,4 ms błąd fazy spada o ponad połowę, z identycznymi liczbami przy 32 i 48
kbps. Przy 0,7 ms korzyść znika, a to właśnie opisany wyżej sufit niejednoznaczności.

#### `--ps-opd <0|1>`

**Wysyła fazę kanału lewego względem downmiksu mono, towarzysza IPD.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`) i tylko razem z `--ps-ipd 1`
- Wartości: `0` przypina OPD do zera, `1` wylicza je (zachowanie domyślne, gdy IPD jest
  włączone)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Różnica między obydwoma parametrami fazy wynagradza uważną lekturę. IPD to faza kanału
lewego względem prawego. OPD to faza kanału lewego względem downmiksu mono. Dekoder
potrzebuje obu, bo ścieżki zasilane z downmiksu obraca o OPD, a pozostałe o OPD minus IPD.
Po odjęciu obu OPD się skraca, więc RÓŻNICA międzykanałowa zawsze wychodzi jako IPD, bez
względu na to, co mówi OPD. OPD decyduje o tym, gdzie ta rotacja leży względem downmiksu.

Właśnie to skracanie sprawia, że problem łatwo przeoczyć. Przy OPD przypiętym do zera lewy
kanał jest przyklejony do fazy downmiksu, a cała rotacja zostaje zrzucona na prawy. Każdy
pomiar arg(L) minus arg(R) jest na to ślepy, bo różnica nadal jest poprawna. Trzeba zamiast
tego zmierzyć fazę bezwzględną każdego kanału wobec źródła, wewnątrz zakresu IPD: wtedy
asymetria staje się oczywista, podobnie jak to, że OPD ją usuwa.

Wyprowadzenie OPD praktycznie nic nie kosztuje, i to jest w tym zgrabne. Downmiks to L+R,
więc sum(L * conj(L+R)) równa się sum(|L|^2) plus sum(L * conj(R)), której część rzeczywista
to pwrL + pwrCr, a część urojona to pwrCi: te same akumulatory, które już zasilają IID, ICC
i IPD. Zatem OPD to atan2(pwrCi, pwrL + pwrCr), czyli jeszcze jeden atan2 na liczbach już
obecnych w pętli, bez potrzeby podawania sygnału downmiksu skądkolwiek indziej. Dla
przypadku opóźnienia przy równych poziomach redukuje się to analitycznie do IPD/2, bo arg(1
+ e^-i.phi) = -phi/2, co jest przydatnym testem zdrowego rozsądku; postać ogólna jest
poprawna także wtedy, gdy kanały różnią się poziomem, a wtedy IPD/2 już nie byłoby.

Wartość `0` warto zostawić do porównań A/B na tym samym materiale, pamiętając, że
`--ps-opd 0` samo z siebie, bez `--ps-ipd 1`, daje strumień fabryczny, bo nie ma warstwy
fazy, którą mógłby zmodyfikować. Różnice OPD pojawiają się przy tym tylko na materiale z
prawdziwym opóźnieniem międzykanałowym; na treści spanoramowanej poziomem nie ma tu czego
poprawiać.

Wartości:
- `0`: OPD przypięte do zera. Błąd fazy asymetryczny między kanałami.
- `1`: OPD wyliczane z istniejących akumulatorów. Błąd fazy symetryczny.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 0 in.wav -o opd0.m4a
```

Współpraca i konflikty: bez `--ps-ipd 1` nie ma sensu. Nie zmienia różnicy międzykanałowej,
a tylko fazę bezwzględną każdego kanału, więc pomiar trzeba dobrać odpowiednio.

Zmierzone, z ffmpegiem jako dekoderem: bezwzględny błąd fazy każdego kanału wobec źródła,
wewnątrz zakresu IPD.

| Opóźnienie międzykanałowe | OPD = 0 (L / R) | OPD wyliczane (L / R) |
|---|---|---|
| 0.25 ms | 14.7 / 20.1 deg | 14.4 / 17.3 deg |
| 0.4 ms | 23.0 / 28.0 deg | 15.5 / 17.8 deg |
| 0.7 ms | 48.1 / 56.2 deg | 40.9 / 36.3 deg |

Przy OPD = 0 oba kanały nigdy się nie zgadzają, 14,7 przeciw 20,1, a dalej 48,1 przeciw
56,2. Przy wyliczanym OPD para się zbiega, średnio o +6,4 stopnia, a przy 0,7 ms o +13.
Warto zauważyć, że 0,7 ms to przypadek, w którym zwykły pomiar IPD nie pokazał żadnej
korzyści: różnica międzykanałowa była już poza tym, co potrafi rozdzielić siatka 45 stopni,
a bezwzględna faza każdego kanału i tak poprawiła się istotnie.

#### `--ps-icc-mode <n>`

**Wybiera, który z dwóch zdefiniowanych trybów rotacji ICC jest sygnalizowany.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` ROT_A, `1` ROT_B
- Wpisywanie: liczba całkowita

Zanim poświęci się na to czas, warto ustawić oczekiwania: to wyłącznie sygnalizacja. Oba
tryby opisują tę samą macierz stereo, dekoder po prostu wyprowadza ją inaczej. To
pokrętło zgodności, a nie jakości; słyszalne różnice stereo w PS robią `--ps-bands` i
`--ps-env`.

Warto go mieć, bo nie wszystkie implementacje dekoderów są równie dobrze przećwiczone na obu
ścieżkach, więc gdy jakiś konkretny odtwarzacz dziwnie renderuje strumień PS, przełączenie
trybu rotacji jest tanie do sprawdzenia.

Wartości:
- `-1`: wybór fabryczny dla danej konfiguracji.
- `0`: sygnalizowane ROT_A.
- `1`: sygnalizowane ROT_B.

```
fdkaac -p 29 -b 32000 --ps-icc-mode 1 in.wav -o rotb.m4a
```

Współpraca i konflikty: istotne tylko wtedy, gdy ICC jest wysyłane, więc warto sprawdzić
`--ps-icc`. Słuchowo znacznie mniej znaczące niż `--ps-bands`.

#### `--ps-bands <10|20>`

**Rozdzielczość częstotliwościowa parametrów stereo: ile pasm PS opisuje obraz.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `10` albo `20`; `-1` wybiera na podstawie bitrate'u (domyślnie), a od 22 kbps w
  górę tabela zawsze daje 20
- Wpisywanie: liczba całkowita

To jedna z dwóch osi decydujących o tym, ile parametrów stereo w ogóle istnieje, i jest to
oś częstotliwości. Dwadzieścia pasm opisuje obraz z dokładniejszym szczegółem widmowym, więc
miks, którego lokalizacja zmienia się w widmie (bas w środku, gitary szeroko, talerze
przesunięte), zachowuje tę strukturę. Dziesięć pasm uśrednia ten sam obraz po dwa razy
większym fragmencie widma każde.

Ciekawy jest dolny koniec zakresu bitrate'u. Zgrubna siatka nie tylko jest tańsza, ale może
też brzmieć stabilniej, bo każdy parametr jest uśredniany po większym fragmencie widma i
dlatego mniej drga z ramki na ramkę. Błądzący obraz bardziej rozprasza niż lekko rozmyty.
Fabryczny FDK wyprowadza tę liczbę wyłącznie z bitrate'u i od 22 kbps w górę zawsze wybiera
20, więc bez tego przełącznika nie da się na normalnym materiale w ogóle przesłuchać siatki
zgrubnej.

Wartości:
- `10`: siatka zgrubna. Mniej bitów parametrów, bardziej rozmyta lokalizacja w
  częstotliwości, stabilniej przy drganiu analizy.
- `20`: siatka dokładna. Dokładniejsza lokalizacja, więcej bitów parametrów.
- `-1`: ustawienie fabryczne, wybrane na podstawie bitrate'u.

```
fdkaac -p 29 -b 24000 --ps-bands 10 in.wav -o coarse.m4a
```

Współpraca i konflikty: `--ps-env` to oś czasu tego samego pomysłu. Oba są słuchowo
istotniejsze niż `--ps-icc-mode`. `--ps-iid-quant` zmienia precyzję każdej wartości, a nie
to, ile tych wartości jest.

#### `--ps-env <1|2|4>`

**Rozdzielczość czasowa parametrów stereo: ile obwiedni parametrów na ramkę.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `1`, `2` albo `4`; `-1` wybiera na podstawie bitrate'u (domyślnie), a od 36 kbps
  w górę tabela daje 4
- Wpisywanie: liczba całkowita

Druga oś rozdzielczości. Jedna obwiednia oznacza, że cała ramka dostaje jedną migawkę
stereo, cztery oznaczają, że obraz jest próbkowany cztery razy w ramce. To właśnie decyduje
o tym, czy ruchoma panorama albo transjent lewo/prawo zachowa swoje położenie, czy rozmaże
się po ramce w uśrednienie.

Jest tu pułapka i to najważniejsza rzecz w tym rozdziale: poproszenie o cztery obwiednie nie
znaczy, że zostaną przesłane. Automatyczna pętla redukcji wciąż dzieli tę liczbę na pół, z
czterech na dwie, z dwóch na jedną, kiedy tylko sąsiednie obwiednie wyglądają podobnie
według wbudowanego progu błędu, a na zwykłej muzyce ten próg jest spełniany bardzo często.
Żądanie jest odwoływane, zanim dotrze do strumienia. Zatrzymuje to `--ps-env-reduce 0`, a
pomiar w jego karcie pokazuje różnicę wprost.

Wartości:
- `1`: jedna migawka stereo na ramkę. Najtaniej, rozmazuje każdy ruch.
- `2`: dwie obwiednie. Rozsądna nastawa środkowa razem z `--ps-env-reduce 0`.
- `4`: cztery obwiednie. Dokładnie nadąża za ruchomą panoramą i transjentami.
- `-1`: ustawienie fabryczne, wybrane na podstawie bitrate'u.

```
fdkaac -p 29 -b 32000 --ps-env 4 --ps-env-reduce 0 in.wav -o env4.m4a
```

Współpraca i konflikty: żeby na zwykłym materiale zostało uszanowane, wymaga w praktyce
`--ps-env-reduce 0`. `--ps-bands` to częstotliwościowa oś tego samego kompromisu.

Zmierzone: przy HE-AAC v2 32 kbps `--ps-env 4` faktycznie zmienia strumień, bo tabela
bitrate'u wybiera tam 2; przy 48 kbps tabela daje już 4, więc żądanie zgadza się z
ustawieniem fabrycznym i różnicę robi tylko `--ps-env-reduce 0`.

#### `--ps-env-reduce <0|1>`

**Wyłącza automatyczną pętlę połowienia obwiedni, żeby przesłana została żądana liczba
obwiedni.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłącza pętlę redukcji, `1` zostawia ją włączoną; `-1` wyłączone, czyli
  zachowanie fabryczne, które ma pętlę włączoną (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Koder utrzymuje pętlę redukcji, która porównuje sąsiednie obwiednie parametrów i zwija
cztery w dwie, a dwie w jedną, kiedy tylko wyglądają dość podobnie według wbudowanego progu
błędu. Intencją jest oszczędzenie bitów parametrów na materiale statycznym. Skutkiem
ubocznym jest to, że zamówiona liczba obwiedni często nie jest tą, która zostaje przesłana,
a ponieważ oszczędność jest mała, a kosztem jest rozdzielczość czasowa obrazu, na materiale
z jakimkolwiek ruchem ten kompromis jest często zły.

`--ps-env-reduce 0` wyłącza pętlę i czyni `--ps-env` dosłownym. To najwartościowsza
pojedyncza nastawa w grupie PS. Zysk jest w dokładności czasowej obrazu stereo, przy
praktycznie tym samym rozmiarze pliku, bo bity parametrów to niewielka część budżetu przy
tych przepływnościach.

Wartości:
- `-1` albo `1`: ustawienie fabryczne. Pętla działa i może zwinąć liczbę obwiedni.
- `0`: pętla wyłączona, `--ps-env` brane dosłownie.

```
fdkaac -p 29 -b 48000 --ps-env 4 --ps-env-reduce 0 in.wav -o literal.m4a
```

Współpraca i konflikty: bez `--ps-env` nie ma sensu, bo cała jego funkcja to ochrona tego
żądania. Łączy się dowolnie z `--ps-bands`.

Zmierzone: 4-sekundowa sonda stereo z panoramą przemiataną z częstotliwością 0,25 Hz plus
naprzemienne transjenty z lewej i z prawej, HE-AAC v2 przy 48 kbps, z porównaniem
zdekodowanej trajektorii panoramy wobec źródła.

| Nastawa | Błąd panoramy (RMS) | Korelacja ze źródłem |
|---|---|---|
| fabryczne (20 bands / 4 envelopes) | 0.177 | 0.9655 |
| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
| `--ps-env 4 --ps-env-reduce 0` | 0.117 | 0.9944 |

Samo żądanie 4 obwiedni nie zmienia przy 48 kbps nic, wyjście jest identyczne bajt w bajt z
fabrycznym, bo pętla redukcji natychmiast odwołuje żądanie. Zysk pojawia się tylko wtedy,
gdy pętla zostanie zatrzymana: o jedną trzecią mniejszy błąd panoramy przy z grubsza tym
samym rozmiarze pliku.

#### `--ps-noenv-skip <0|1>`

**Zabrania ramek PS bez parametrów, żeby każda ramka nosiła informację stereo.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` bez ramek bez parametrów, `1` zezwolenie na nie; `-1` wyłączone, czyli
  zachowanie fabryczne, które na nie zezwala (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Poza przerzedzaniem obwiedni koder może też całkowicie pominąć parametry stereo na odcinku
ramek. Kiedy kolejne zestawy IID i ICC wyglądają podobnie, może wysłać do dziesięciu
następujących po sobie ramek, które nie niosą żadnej informacji stereo, pozostawiając
dekoder na biegu jałowym z ostatnimi wartościami, jakie otrzymał.

Na materiale statycznym jest to darmowe. Na materiale, w którym obraz dryfuje powoli,
słychać to jako spłaszczanie się obrazu stereo i jego powrót z trzaskiem, gdy parametry
wracają, bo dekoder trzymał przez cały dryf nieaktualną migawkę, a potem przeskoczył do
bieżącej. Raz rozpoznany artefakt zwykle słychać już za każdym razem, a rozprasza on
bardziej, niż są warte zaoszczędzone bity.

`--ps-noenv-skip 0` zabrania pomijania i wymusza parametry w każdej ramce. Warto go użyć,
gdy obraz wydaje się oddychać albo zacinać. Kosztem jest skromna liczba bitów parametrów na
ramkę.

Wartości:
- `-1` albo `1`: ustawienie fabryczne. Do dziesięciu następujących po sobie ramek może nie
  nieść żadnych parametrów stereo.
- `0`: parametry w każdej ramce.

```
fdkaac -p 29 -b 32000 --ps-noenv-skip 0 --ps-env 4 --ps-env-reduce 0 in.wav -o steady.m4a
```

Współpraca i konflikty: ta sama rodzina ukrytego przerzedzania co `--ps-env-reduce`, i te
dwa ustawia się zwykle razem, gdy celem jest stabilny obraz stereo. Żaden z nich nie zmienia audio
rdzenia, tylko strumień parametrów stereo.

## C. Pasmo i SBR

O tym, jak wysoko brzmi plik, decydują dwie różne rzeczy, które łatwo ze sobą pomylić.

Pierwsza to **pasmo rdzenia**: najwyższa częstotliwość, którą sam rdzeń AAC faktycznie
koduje, prawdziwymi liniami widmowymi i prawdziwymi bitami. Powyżej tej granicy rdzeń nie
wysyła nic.

Druga to **częstotliwość podziału SBR**, nazywana też częstotliwością startu SBR. SBR
(spectral band replication) nie koduje górnego pasma. Kopiuje w górę niższe, już zakodowane
widmo, a potem opisuje garścią parametrów na ramkę, jak ta kopia ma zostać ukształtowana.
Częstotliwość podziału to miejsce, w którym kopia się zaczyna. W poprawnie zbudowanym
strumieniu HE-AAC pasmo rdzenia i częstotliwość podziału leżą w tym samym miejscu: rdzeń się
kończy, SBR przejmuje pałeczkę, a słuchacz słyszy jedno ciągłe widmo.

To, co SBR wysyła na ramkę, jest krótką listą:

- **obwiednie**: poziom każdego pasma SBR w czasie. Więcej obwiedni na ramkę oznacza
  drobniejszą rozdzielczość czasową na górze i więcej bitów.
- **poziom szumu SBR (noise floor)**: ile syntetycznego szumu domieszać do każdego pasma. To
  właśnie on zamienia skopiowany obszar tonalny z powrotem w coś, co brzmi jak powietrze,
  talerze albo oddech.
- **poziom filtrowania odwrotnego** (invf): jak mocno wybielać kopię w każdym paśmie.
  Skopiowany materiał niesie strukturę tonalną pasma, z którego pochodzi. Jeśli ta
  struktura nie pasuje do góry, słychać ją jako metaliczną, dzwoniącą, chórowaną górę.
  Wybielanie tę strukturę spłaszcza.
- **brakujące harmoniczne**: flagi, które każą dekoderowi wstawić syntetyczną sinusoidę w
  paśmie, gdzie prawdziwa składowa oryginału zginęła albo wylądowała w złym miejscu.

O dwóch ostatnich decydują **estymatory** w koderze, które porównują tonalność oryginalnego
górnego pasma z tonalnością pasma odtworzonego przez SBR, pasmo po paśmie i ramka po ramce.
To w estymatorach ustala się charakter góry, a one wymieniają się między sobą wzdłuż jednej
osi: za mało wybielania i za mało szumu wypełniającego daje górę metaliczną, dzwoniącą,
pofazowaną; za dużo jednego i drugiego daje górę syczącą i rozmytą, w której talerze
zamieniają się w kształtowany szum, a utrzymujące się składowe znikają. Większość pokręteł w
tej grupie przesuwa punkt pracy dokładnie po tej osi. Nie ma na niej punktu uniwersalnie
poprawnego; zależy on od materiału i od bitrate'u.

SBR jest aktywne w tych profilach:

- `-p 5` HE-AAC
- `-p 29` HE-AAC v2 (SBR plus parametric stereo)
- `-p 39 -L 1` AAC-ELD z SBR

W `-p 2` (AAC-LC), `-p 23` (AAC-LD) i `-p 39` bez `-L 1` nie ma SBR, więc każdy przełącznik
`--sbr-*` z tej grupy nic nie robi. `--core-cutoff`, na końcu grupy, działa wszędzie, ale
bez SBR jest tym samym co `-w`.

Dwie konwencje używane w całej tej grupie:

- **pokrętła x100** skalują wewnętrzną stałą: `100` oznacza 1,00 i jest dokładnie
  ustawieniem fabrycznym, poniżej 100 stała maleje, powyżej 100 rośnie. Oba kierunki
  działają. Kilka pokręteł jest opisanych jako **surowe x100**, co znaczy, że wartość
  zastępuje stałą wprost, zamiast ją skalować (`--sbr-tran-peak 90` *jest* fabryczną
  wartością 0,90).
- `--verbose` wypisuje efektywne pasmo rdzenia, częstotliwość podziału, końcową górną
  krawędź i okres nagłówka SBR. Pod SBR to jedyny pewny sposób, żeby zobaczyć, co
  naprawdę ustawił koder, a kilka przełączników z tej grupy FDK sprawdza dopiero przy
  inicjalizacji, więc sprawdzanie nie jest opcjonalne.

### Układ częstotliwości SBR: `--sbr-start` `--sbr-stop` `--sbr-freqscale` `--sbr-alterscale` `--sbr-noise-bands` `--sbr-amp-res` `--sbr-data-extra`

Tych siedem przełączników definiuje kształt pasma SBR, zanim cokolwiek zostanie w nim
zmierzone: gdzie zaczyna się obszar replikowany, gdzie się kończy, jak jest podzielony na
pasma, jak drobno opisywany jest poziom szumu i jak dokładnie kwantowane są poziomy
obwiedni.

Dwa ważne to `--sbr-start` i `--sbr-stop`. Oba przyjmują indeks tabeli, a nie częstotliwość,
a to, na jaką częstotliwość mapuje się indeks, zależy od częstotliwości próbkowania, profilu
i bitrate'u. Wartości zmierzone podajemy w kartach poniżej. Warto czytać je jako wskazówkę
co do kierunku i odstępów, a rzeczywiste liczby dla konkretnych ustawień potwierdzić przez
`--verbose`.

Reguła, na której wykłada się każdy: **legalność zależy od pary, bitrate'u i częstotliwości
próbkowania.** SBR wymaga poprawnej liczby pasm nadrzędnych między startem a stopem, a FDK
sprawdza tę kombinację przy inicjalizacji kodera. Zła para nie degraduje się łagodnie;
zgłasza "encoder initialization failed" i nic nie zostaje zapisane. Legalne kombinacje
tworzą przekątny pas: podnosząc start, trzeba zwykle podnieść razem z nim stop. Przy HE-AAC
64 kbps stereo, wejście 48 kHz, każdy start od 0 do 4 jest nielegalny z każdym stopem, a
wyglądająca na udokumentowaną para `--sbr-start 5 --sbr-stop 9` tam zawodzi. Pary, które
przy tym ustawieniu działają: `8/9`, `8/14`, `14/12`.

Czyli: zmieniać po jednym indeksie naraz, uruchomić kodowanie i przeczytać `--verbose`.
Kodowanie, które nie zainicjalizuje się, kosztuje sekundę; strumień z niezamierzoną
częstotliwością podziału kosztuje całą sesję odsłuchową.

#### `--sbr-start <n>`

**Ustawia miejsce, w którym kończy się rdzeń, a zaczyna replikowane górne pasmo.**

- Dotyczy: tylko profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: indeks tabeli `0..15`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

To najbardziej brzemienna w skutki pojedyncza liczba w strumieniu HE-AAC. Wszystko poniżej
częstotliwości podziału to prawdziwe, zakodowane widmo; wszystko powyżej to opisana kopia.
Obniżenie startu daje rdzeniowi mniej roboty, więc to, co koduje, koduje czyściej, a większa
część widma staje się parametryczna. Podniesienie startu działa odwrotnie: więcej prawdziwej
góry, mniejszy obszar SBR i rdzeń, który musi za to zapłacić.

Przy bardzo niskim bitrate obniżenie częstotliwości podziału jest często jedynym sposobem,
żeby rdzeń w ogóle brzmiał czysto. Przy 64 kbps i wyżej wygrywa raczej podnoszenie, bo rdzeń
stać na autentyczny detal w zakresie 10..12 kHz, a skopiowane pasmo zawsze niesie część
charakteru pasma, z którego je skopiowano.

Zmierzone pasmo rdzenia dla kolejnych indeksów startu, tak jak wypisuje je `--verbose` przy
HE-AAC `-p 5`, 64 kbps, wejście stereo 48 kHz, z domyślnym stopem. "Nielegalny" znaczy, że
inicjalizacja kodera zawiodła dla każdego wypróbowanego stopu:

| `--sbr-start` | pasmo rdzenia |
|---|---|
| 0..4 | nielegalny przy tym ustawieniu |
| 5 | 5062 Hz (tylko z `--sbr-stop 0`) |
| 6 | 5484 Hz |
| 7 | 5953 Hz |
| 8 | 6468 Hz |
| 9 | 6468 Hz |
| 10 | 7031 Hz |
| 11 | 7640 Hz |
| 12 | 8250 Hz |
| 13 | 9000 Hz |
| 14 | 10500 Hz (fabryczne przy tym bitrate) |
| 15 | 12000 Hz |

Ten sam przegląd przy wejściu 44,1 kHz, HE-AAC 32 kbps: start 5 daje 5038 Hz, 8 daje 5943
Hz, 11 daje 7579 Hz, 14 daje 9646 Hz, 15 daje 11025 Hz. Warto zauważyć, że indeksy 8 i 9
przy 48 kHz trafiają w to samo pasmo rdzenia: to mapowanie jest tabelą, a nie wzorem, i
sąsiednie indeksy mogą się pokrywać.

Przykłady, obniżanie częstotliwości podziału (HE-AAC 64 kbps stereo, 48 kHz, fabryczny
start 14):
- Łagodnie: `--sbr-start 13`. Rdzeń do 9000 Hz. Minimalnie czystszy rdzeń, odrobinę
  szerszy obszar odtwarzany przez SBR.
- Mocno: `--sbr-start 11`. Rdzeń do 7640 Hz. Zauważalnie większa część góry jest
  parametryczna. Warto tego spróbować, gdy rdzeń brzmi ziarniście.
- Przesadnie: `--sbr-start 6 --sbr-stop 9`. Rdzeń do 5484 Hz. Prawie wszystko powyżej
  obszaru obecności jest kopią; talerze i syczące głoski to w całości wymysł SBR. To
  właściwy kształt przy 24 kbps i ustawienie skrajne przy 64.

Przykłady, podnoszenie częstotliwości podziału:
- Łagodnie: `--sbr-start 15`. Rdzeń do 12000 Hz, szczyt zakresu indeksów przy tej
  częstotliwości. Prawdziwie kodowane widmo do 12 kHz, węższy obszar SBR powyżej.
- Mocno: dalsze podnoszenie częstotliwości podziału nie jest tym przełącznikiem dostępne
  przy tym ustawieniu, bo 15 to najwyższy indeks. Kolejny krok to porzucić SBR i zakodować
  całe pasmo przez `-p 2` plus `-w`.
- Przesadnie: to samo doprowadzone do końca: `-p 2 -b 64000 -w 16000`. Bez SBR, bez kopii,
  a przy 64 kbps stereo rdzeń wyraźnie nie ma bitów na pasmo 16 kHz. Pouczające do
  porównania z HE-AAC na tym samym materiale.

Współpraca i konflikty: musi tworzyć legalną parę z `--sbr-stop`. Rdzeń sam idzie za
startem; `--core-cutoff` może go ściągnąć poniżej startu, co otwiera dziurę, a powyżej nie
da się go przesunąć z pożytkiem. Start warto podnosić o jeden indeks i słuchać: każdy krok
przesuwa w górę prawdziwie kodowane widmo i kosztuje rdzeń bity. Obniżenie startu poszerza
pasmo, za które odpowiadają `--sbr-noise-bands`, `--sbr-invf*` i detektor brakujących
harmonicznych, więc im niżej leży start, tym bardziej liczą się ich ustawienia.

#### `--sbr-stop <n>`

**Ustawia górną krawędź pasma replikowanego, czyli górną krawędź pliku.**

- Dotyczy: tylko profili z SBR
- Wartości: indeks tabeli `0..13`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Tam, gdzie `--sbr-start` dzieli widmo, ten przełącznik ustawia sufit. Podniesienie go
rozciąga obszar odtwarzany przez SBR wyżej, co prawie nic nie kosztuje w bitach (SBR opisuje
pasma, a nie linie), ale rozkłada ten sam budżet parametrów na szerszy obszar, więc każde
pasmo jest opisane grubiej. Obniżenie go zamyka plik wcześniej, co brzmi jak zwykła
dolnoprzepustowa i bywa odbierane jako czystsze, bo mierne odtworzenie zakresu 18..22 kHz
wnosi więcej syku niż treści.

Zmierzona końcowa górna krawędź zdekodowanego wyjścia (rdzeń plus SBR) dla kolejnych
indeksów stopu, `--verbose` przy HE-AAC `-p 5`, 64 kbps, wejście stereo 48 kHz, z
`--sbr-start 14`:

| `--sbr-stop` | końcowa górna krawędź |
|---|---|
| 0..3 | nielegalny przy tym ustawieniu |
| 4 | około 10875 Hz |
| 5 | około 12000 Hz |
| 6 | około 13125 Hz |
| 7 | około 14250 Hz |
| 8 | około 15375 Hz |
| 9 | około 16875 Hz |
| 10 | około 18375 Hz |
| 11 | około 20250 Hz |
| 12 | około 22125 Hz (fabryczne) |
| 13 | nielegalny przy tym ustawieniu |

Przykłady, obniżanie górnej krawędzi (z fabrycznego 12):
- Łagodnie: `--sbr-start 14 --sbr-stop 11`. Góra na około 20250 Hz. Trudne do usłyszenia,
  nieco gęstszy opis tego, co zostaje.
- Mocno: `--sbr-start 14 --sbr-stop 9`. Góra na około 16875 Hz. Świadomy, słyszalny sufit,
  a pozostałe pasma opisane drobniej. Często lepszy kompromis przy 32 kbps i niżej.
- Przesadnie: `--sbr-start 14 --sbr-stop 4`. Góra na około 10875 Hz przy rdzeniu 10500 Hz:
  SBR obejmuje teraz kilkaset Hz i nie wnosi nic. W praktyce powstaje strumień AAC-LC z
  doczepionym narzutem SBR.

Przykłady, podnoszenie górnej krawędzi:
- Łagodnie: w górę przy tym ustawieniu nic nie ma. Stop 12 (około 22125 Hz) to już
  domyślna góra, a stop 13 nie jest legalny ze startem 14. Górę można utrzymać, przesuwając
  częstotliwość podziału w górę: `--sbr-start 15 --sbr-stop 12` zachowuje około 22125 Hz
  przy rdzeniu 12000 Hz.
- Mocno: `--sbr-start 8 --sbr-stop 14`. Szeroki obszar odtwarzany przez SBR od 6468 Hz w
  górę. Tani w bitach, zgrubny w opisie: na gęstym materiale grozi "migotliwy" charakter.
- Przesadnie: `--sbr-stop 13` przy tym ustawieniu, co kończy się nieudaną inicjalizacją.
  Indeks istnieje w standardzie, ale nie w każdej legalnej parze, i to najszybszy sposób,
  żeby zobaczyć w praktyce, co oznacza ten przekątny pas legalności.

Współpraca i konflikty: ograniczenie pary z `--sbr-start` rządzi obydwoma. Indeks 13 jest
przyjmowany przez parser, ale nie jest legalny w każdej parze, i dlatego `--verbose` oraz
testowe kodowanie wyprzedzają jakikolwiek odsłuch.

#### `--sbr-freqscale <n>`

**Wybiera, jak replikowane pasmo jest dzielone na pasma SBR: liniowo albo na jednej z
trzech siatek logarytmicznych.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = liniowo, `1`, `2`, `3` = logarytmicznie, 12, 10 i 8 pasm na oktawę
  (coraz grubiej); `-1` albo pominięcie przełącznika = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

SBR opisuje swój obszar pasmo po paśmie, a to jest wybór linijki. Siatka liniowa daje
każdemu pasmu tę samą szerokość w Hz, więc górna oktawa dostaje wiele pasm, a dół obszaru
SBR niewiele. Siatka logarytmiczna rozstawia pasma według stosunku częstotliwości, co pasuje
do słuchu: więcej rozdzielczości tam, gdzie ucho rozróżnia więcej, czyli nisko, przy
częstotliwości podziału.

Wartości:
- `0` liniowo. Równe odstępy w Hz. Drobna rozdzielczość wysoko, zgrubna tuż powyżej
  częstotliwości podziału, czyli odwrotnie niż oczekuje ucho. Bywa użyteczne na materiale,
  którego górne pasmo jest naprawdę jednorodne, na przykład na szerokopasmowych podkładach
  szumowych.
- `1` najdrobniejsze grupowanie logarytmiczne, 12 pasm na oktawę. Najwięcej pasm, najwięcej
  parametrów, najlepsze kształtowanie nierównego górnego pasma.
- `2` środkowe grupowanie logarytmiczne, 10 pasm na oktawę.
- `3` najgrubsze grupowanie logarytmiczne, 8 pasm na oktawę. Najmniej pasm, najtańszy opis,
  najmniejsza kontrola.

Zmierzone przy HE-AAC 64 kbps, dane SBR na ramkę: 220 bitów przy `0`, 188 przy `1`, 170
przy `2`, 152 przy `3`.
- `-1` wyłączone: decyduje tabela strojenia, co jest właściwym wyborem, dopóki nie ma
  powodu, żeby zrobić inaczej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 1 -o out.m4a in.wav` (najdrobniejsza siatka)

Współpraca i konflikty: liczba pasm, którą to daje, wchodzi w interakcję z
`--sbr-noise-bands` (pasma szumu wyprowadza się na oktawę, a nie na pasmo SBR) oraz z tym,
jak bardzo decyzja filtrowania odwrotnego (invf) może się zmieniać w obrębie obszaru.
Drobniejsza siatka oznacza drobniejszą mapę invf.

#### `--sbr-alterscale <n>`

**Przełącza na alternatywną rozdzielczość grupowania pasm, czyli wtórną korektę siatki
wybranej przez `--sbr-freqscale`.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = wyłączone, `1` = włączone; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

To standardowa flaga nagłówka SBR, która nieco zmienia sposób wyprowadzania tabeli pasm
nadrzędnych, co w praktyce przesuwa kilka granic pasm. To dopracowanie, a nie kontrola
charakteru: nie ma tu wyniku odsłuchowego, który warto byłoby obiecywać w którejkolwiek
pozycji, i nic w naszych pomiarach go nie izoluje.

Wartości:
- `0` alternatywna rozdzielczość wyłączona.
- `1` alternatywna rozdzielczość włączona.
- `-1` wyłączone: decyduje tabela strojenia. Zalecane.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 2 --sbr-alterscale 1 -o out.m4a in.wav`

Współpraca i konflikty: ma sens tylko razem z `--sbr-freqscale`, bo koryguje właśnie tamtą
siatkę. Gdy celem jest zmiana barwy na górze, prawdziwą dźwignię mają `--sbr-invf*` i
`--sbr-noise-bands`; po ten warto sięgać na końcu.

#### `--sbr-noise-bands <n>`

**Ustawia, jak drobno poziom szumu SBR jest opisany wzdłuż częstotliwości.**

- Dotyczy: tylko profili z SBR
- Wartości: `0`, `1`, `2`, `3`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziom szumu to sposób, w jaki SBR wkłada z powrotem "powietrze" do skopiowanego pasma. Przy
jednym paśmie szumu na cały obszar koder wysyła jeden poziom szumu, a dekoder rozsmarowuje
go równo: tanio i błędnie zawsze wtedy, gdy górne pasmo nie jest jednorodne, na przykład
przy utrzymywanym dźwięku smyczków pod hi-hatem. Przy większej liczbie pasm szumu poziom
może podążać za materiałem, więc cichy obszar tonalny nie dostaje tyle samo syku, co obszar
ruchliwy.

Pole w nagłówku SBR ma szerokość dwóch bitów, więc ustawień jest dokładnie cztery i ani
jednego więcej. Wartości *nie* są dosłowną liczbą pasm: to gęstość, z grubsza pasma na
oktawę.

Wartości:
- `0` jedno pasmo szumu na cały obszar SBR. Najtaniej, najbardziej tępo. Przy bardzo niskim
  bitrate oszczędność jest realna; na czymkolwiek, co ma na górze strukturę, słychać szum w
  złych miejscach.
- `1` mniej więcej jedno pasmo szumu na oktawę.
- `2` mniej więcej dwa na oktawę.
- `3` mniej więcej trzy na oktawę. Najgęstszy opis, najlepsze rozdzielenie obszarów
  tonalnych od szumowych, najwięcej informacji pobocznej.
- `-1` wyłączone: decyduje tabela strojenia, zależnie od bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-noise-bands 3 -o out.m4a in.wav`

Współpraca i konflikty: to decyduje o *rozdzielczości* poziomu szumu; `--sbr-noise-max`,
`--sbr-nf-weight` i `--sbr-noise-floor-offset` decydują o jego *poziomie*. Podnoszenie
gęstości idzie na marne, jeśli poziom jest przycięty niskim sufitem, więc przy pogoni za
powietrzem warto najpierw zajrzeć do `--sbr-noise-max`.

#### `--sbr-amp-res <n>`

**Wybiera krok kwantyzacji poziomów obwiedni SBR: 1,5 dB albo 3,0 dB.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = 1,5 dB, `1` = 3,0 dB; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziomy obwiedni są wysyłane jako liczby skwantowane, a to jest rozmiar kroku. Krok 1,5 dB
śledzi zmiany poziomu w górnym paśmie dokładniej i kosztuje więcej bitów na obwiednię. Krok
3,0 dB jest tańszy i grubszy; na materiale z dużym ruchem poziomu na górze zgrubny krok
słychać jako skokowe, a nie płynne, zmiany górnego pasma.

Wartości:
- `0` kroki 1,5 dB. Drobniej, drożej. Lepszy wybór przy wyższym bitrate albo na materiale
  dynamicznym.
- `1` kroki 3,0 dB. Zgrubniej, taniej. Rozsądny kompromis, gdy bitów brakuje, i dlatego
  tabela często wybiera go nisko.
- `-1` wyłączone: decyduje tabela strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 80000 --sbr-amp-res 0 -o out.m4a in.wav`

Współpraca i konflikty: wchodzi w bezpośrednią interakcję z `--sbr-num-env` i detektorem
transjentów, bo każda dodatkowa obwiednia kosztuje pełny komplet poziomów przy obowiązującym
rozmiarze kroku. Cztery obwiednie przy rozdzielczości 1,5 dB to najdroższy róg tej grupy.

#### `--sbr-data-extra <n>`

**Zapisuje opcjonalne dodatkowe pola nagłówka SBR, zamiast je pomijać.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = nie zapisuj, `1` = zapisz; domyślnie `-1` = wartość z tabeli strojenia
  SBR
- Wpisywanie: liczba całkowita

Nagłówek SBR ma opcjonalny blok z dodatkową konfiguracją. Zapisywanie go powiększa
nieznacznie każdy nagłówek i czyni konfigurację bardziej jawną; pominięcie zostawia dekoder
z domyślnymi założeniami dla tych pól. To decyzja o formatowaniu strumienia, a nie kontrola
brzmienia, i warto jej dotykać tylko wtedy, gdy celem jest zgodność z konkretnym dekoderem
albo wgląd w nagłówki.

Wartości:
- `0` nie zapisuj dodatkowych danych nagłówka.
- `1` zapisz je.
- `-1` wyłączone: decyduje tabela strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-data-extra 1 -o out.m4a in.wav`

Współpraca i konflikty: w połączeniu z krótkim `--sbr-header-period` dodatkowe bajty
powtarzają się często, więc przy optymalizowaniu strumienia o niskim bitrate pod szybkie
zaskoczenie SBR lepiej trzymać to wyłączone.

### Rozdzielczość czasowa SBR i stereo: `--sbr-num-env` `--sbr-freqres-fixfix` `--sbr-stereo-mode` `--sbr-header-period` `--sbr-header-time`

Układ częstotliwości decyduje o tym, gdzie jest pasmo SBR. Ta sekcja decyduje, jak często
jest opisywane, jak dzieli się między kanały i jak często sama konfiguracja powtarza się w
strumieniu.

Rozdzielczość czasowa to bezpośredni kompromis między bitami a rozmazaniem. Jedna obwiednia
na ramkę oznacza jeden poziom na pasmo dla całej ramki, więc hi-hat wewnątrz tej ramki jest
uśredniany z otaczającą go ciszą. Cztery obwiednie śledzą kształt, ale czterokrotnie
zwiększają dane obwiedni. Zwykle tę decyzję podejmuje detektor transjentów dla każdej ramki,
i dlatego wymuszenie stałej liczby jest większą ingerencją, niż wygląda.

Okres nagłówka to ustawienie innego rodzaju: nie zmienia tego, jak plik brzmi, gdy
odtwarzanie już trwa, tylko to, jak długo dekoder dołączający się w środku strumienia czeka,
zanim w ogóle będzie miał górne pasmo. Ma znaczenie przy transmisji na żywo i poza tym nie
ma znaczenia nigdzie.

#### `--sbr-num-env <1|2|4>`

**Wymusza stałą liczbę obwiedni SBR na ramkę i wyłącza decyzję detektora transjentów o
podziale ramki.**

- Dotyczy: tylko profili z SBR
- Wartości: `1`, `2`, `4`; domyślnie `-1` = wyłączone, decyduje detektor transjentów dla
  każdej ramki
- Wpisywanie: liczba całkowita

Normalnie SBR używa zmiennej siatki czasowej: ramki z wykrytym atakiem dostają więcej
krótszych obwiedni, żeby atak nie został rozmazany, a ramki ustabilizowane dostają jedną.
Ten przełącznik zastępuje to siatką statyczną, taką samą w każdej ramce, niezależnie od
treści.

To czyni go dobrym narzędziem na materiał stabilny i złym na materiał perkusyjny. Na
utrzymywanym padzie, organach albo podkładzie smyczkowym wymuszenie 2 lub 4 obwiedni daje
rozdzielczość czasową, o którą detektor by nie poprosił, a koszt jest przewidywalny. Na
perkusji statyczna siatka oznacza, że ataki nie dostają już własnej obwiedni, a w zamian
pojawia się dokładnie to rozmazanie wstecz, któremu detektor ma zapobiegać.

Wartość 8 przekroczyłaby siatkę ze standardu i jest odrzucana.

Wartości:
- `1` jedna obwiednia na ramkę. Najtaniej, najbardziej tępo; górne pasmo trzyma jeden
  poziom na pasmo przez całą ramkę. Na czymkolwiek transjentowym słychać rozmazanie.
- `2` dwie obwiednie na ramkę. Złoty środek i sensowne wymuszone ustawienie na materiale
  stabilnym przy niskim bitrate.
- `4` cztery obwiednie na ramkę. Najdrobniejsza statyczna rozdzielczość, najwięcej danych
  obwiedni. Dobre na dźwięki utrzymywane i dzwoniące; na ostrych atakach nadal przegrywa z
  siatką dynamiczną, bo podziały są w stałych miejscach, a nie w miejscu ataku.
- `-1` wyłączone. Detektor transjentów wybiera dla każdej ramki. To lepsze ustawienie
  domyślne, a pokrętła do jego kształtowania są w sekcji o detektorze transjentów poniżej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-num-env 4 -o out.m4a organ.wav`

Współpraca i konflikty: to nadpisuje tę część podziału ramki, na którą wpływają
`--sbr-tran-thr` i `--sbr-tran-split`, więc łączenie ich jest sprzeczne: gdy celem jest
lepsze zachowanie na atakach, trzeba zostawić `--sbr-num-env` wyłączone i zamiast tego
obniżyć `--sbr-tran-thr`. Koszt bitowy obwiedni ustawia `--sbr-amp-res`.

#### `--sbr-freqres-fixfix <0|1>`

**Ustawia rozdzielczość częstotliwościową obwiedni FIXFIX, czyli typu obwiedni używanego w
ramkach bez wykrytego transjentu.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = niska, `1` = wysoka; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Obwiednia może opisać górne pasmo niewieloma pasmami albo wieloma. FIXFIX to kształt
obwiedni używany dla zwykłych ramek bez transjentu, a na większości materiału stanowią one
zdecydowaną większość, więc to ustawienie po cichu wpływa na większość pliku.

Niska rozdzielczość grupuje pasma razem: mniej poziomów na obwiednię, taniej i gorsza
zdolność podążania za nierównym górnym pasmem. Wysoka rozdzielczość wysyła więcej poziomów,
lepiej śledzi kształt widma i kosztuje bity, które przy niskim bitrate biorą się z rdzenia.

Wartości:
- `0` niska rozdzielczość częstotliwościowa. Taniej; górne pasmo jest opisane grubiej
  wzdłuż częstotliwości. Sensowne przy niskim bitrate i na jednorodnych górnych pasmach.
- `1` wysoka rozdzielczość częstotliwościowa. Więcej poziomów na obwiednię, lepsze
  kształtowanie górnych pasm o wyraźnej strukturze. Sensowne, gdy bity są dostępne.
- `-1` wyłączone: wybór samego kodera, zgodnie z tabelą strojenia i typem ramki.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-freqres-fixfix 1 -o out.m4a in.wav`

Współpraca i konflikty: to jest oś częstotliwości; `--sbr-num-env` to oś czasu. Oba mnożą
się w ten sam budżet danych obwiedni, a `--sbr-amp-res` ustala cenę za poziom. Dotyczy tylko
ramek bez transjentów, więc na mocno perkusyjnym materiale jego udział w pliku jest
mniejszy.

#### `--sbr-stereo-mode <1..3>`

**Wybiera, jak górne pasmo SBR jest dzielone między dwa kanały, niezależnie od kodowania
stereo w rdzeniu.**

- Dotyczy: tylko profili z SBR, wejście stereo
- Wartości: `1` LR, `2` coupling, `3` switch-LRC; domyślnie `-1` = wyłączone (decyduje
  tabela strojenia: `3` poniżej 60 kbps stereo, `1` od 60 kbps)
- Wpisywanie: liczba całkowita

Rdzeń i SBR podejmują osobne decyzje stereo. Rdzeń może kodować mid/side, podczas gdy SBR
stosuje coupling, albo odwrotnie. Ten przełącznik dotyczy wyłącznie górnego pasma.

Pełna separacja lewy/prawy wysyła niezależny komplet parametrów SBR dla każdego kanału:
najszersza, najwierniejsza góra, za cenę mniej więcej podwojonej informacji pobocznej SBR.
Coupling wysyła jedną wspólną obwiednię oraz informację o poziomie i balansie, co jest
znacznie tańsze i ściąga obraz stereo na górze w stronę środka. Na większości materiału
słychać to mniej, niż można by się spodziewać, i dlatego jest to normalny wybór przy niskim
bitrate'ie; na szeroko i ostro spanoramowanych talerzach oraz na nagraniach z prawdziwą
różnicą akustyki między kanałami słychać to wyraźnie.

Wartości:
- `1` LR, pełna separacja. Niezależne górne pasmo dla każdego kanału. Tego trybu FDK używa
  od 60 kbps stereo; przy 32 kbps jest kosztowny.
- `2` coupling. Wspólna obwiednia oraz poziom i balans w każdej ramce. Tanio, węższa góra.
- `3` switch-LRC. Koder w każdej ramce wybiera między LR a couplingiem. Tego trybu FDK
  używa poniżej 60 kbps stereo.
- `-1` wyłączone: decyduje tabela strojenia, jak wyżej.

Wejście mono nie wymaga przełącznika: FDK sam koduje jednokanałowe górne pasmo. Trybu mono
dla wejścia stereo nie ma; `0` jest odrzucane.

Przykład w kontekście: `fdkaac -p 5 -b 48000 --sbr-stereo-mode 1 -o out.m4a in.wav`, pełna
separacja poniżej progu 60 kbps.

Współpraca i konflikty: w `-p 29` (HE-AAC v2) obraz stereo opisuje parametric stereo, a
część SBR jest mono, więc ten przełącznik nie ma tam czego rozdzielać. Przełączniki stereo
rdzenia z grupy B to osobna decyzja i go nie nadpisują.

Zmierzone: HE-AAC, wejście 48 kHz stereo; strumień bez przełącznika jest identyczny co do
bajtu z `3` przy 24, 32, 40, 48, 56 i 59 kbps oraz z `1` przy 60, 64 i 96 kbps. Przed beta.2
wartość `0` była przyjmowana i przy wejściu stereo wywracała koder, bez komunikatu i z
pustym plikiem.

#### `--sbr-header-period <n>`

**Ustawia, co ile ramek zapisywane są nagłówki SBR, co decyduje, jak długo dekoder
dołączający się do strumienia na żywo słyszy sam rdzeń, zanim pojawi się górne pasmo.**

- Dotyczy: tylko profili z SBR. W HE-AAC i HE-AAC v2 steruje nagłówkiem `sbr_header` w
  strumieniu; w AAC-ELD z SBR nie ma nagłówka SBR w strumieniu, więc okres dotyczy
  StreamMuxConfig w LATM/LOAS i wymaga `-f 6`, `-f 7` albo `-f 10`
- Wartości: ramki, `1` wzwyż; domyślnie `-1` = wyłączone, własny okres FDK około 10 ramek
  (około 0,46 s przy 44,1 kHz)
- Wpisywanie: liczba całkowita

Cała konfiguracja SBR jest zapisana w okresowym nagłówku, a nie w każdej ramce. Słuchacz,
który podłącza się do strumienia Icecast, Shoutcast albo HLS w środku, od razu dekoduje
rdzeń i słyszy stłumioną, obciętą dolnoprzepustowo wersję programu aż do nadejścia kolejnego
nagłówka SBR. Wtedy pojawia się góra. W pliku jest to niewidoczne, bo odtwarzanie zaczyna
się od początku; w strumieniu na żywo to pierwsza rzecz, jaką słyszy nowy słuchacz.

Zapisywanie nagłówka w każdej ramce niemal całkowicie usuwa ten moment, kosztem bajtów
nagłówka w każdej ramce. Przy niskim bitrate te bajty to nie jest nic. Rozciągnięcie okresu
je oszczędza i wydłuża moment z samym rdzeniem.

Wartość jest brana dosłownie, bez nakładania sufitu jednej sekundy.

Przykłady, skracanie okresu:
- Łagodnie: `--sbr-header-period 5`. Z grubsza o połowę skraca czekanie względem wartości
  domyślnej.
- Mocno: `--sbr-header-period 2`. Nagłówek co drugą ramkę; moment z samym rdzeniem spada do
  dziesiątek milisekund.
- Przesadnie: `--sbr-header-period 1`. Nagłówek w każdej pojedynczej ramce, niemal
  natychmiastowe zaskoczenie SBR, maksymalny narzut. Przy strumieniu 24 kbps ten narzut to
  realna część budżetu, i dlatego tam jest ustawieniem skrajnym, a przy 96 kbps rozsądnym
  wyborem.

Przykłady, wydłużanie okresu:
- Łagodnie: `--sbr-header-period 25`. Około sekundy między nagłówkami przy 44,1 kHz. Ledwie
  zauważalne na strumieniu, przy którym ludzie zostają.
- Mocno: `--sbr-header-period 100`. Kilka sekund dźwięku z samego rdzenia dla nowego
  słuchacza. Oszczędza bajty nagłówków, brzmi jak zepsute dla każdego, kto przeskakuje
  między stacjami.
- Przesadnie: `--sbr-header-period 5000`. Minuty między nagłówkami. Dla pliku nieszkodliwe;
  dla strumienia oznacza, że większość słuchaczy nigdy nie usłyszy górnego pasma.

Współpraca i konflikty: `--sbr-header-time` wyraża to samo w sekundach i wygrywa, gdy podane
są oba. `--sbr-data-extra 1` powiększa każdy nagłówek, więc razem te dwa mnożą narzut.
`--verbose` wypisuje efektywny okres w ramkach i w milisekundach.

#### `--sbr-header-time <s>`

**To samo ustawienie co `--sbr-header-period`, wyrażone w sekundach zamiast w ramkach.**

- Dotyczy: tylko profili z SBR; w AAC-ELD z SBR ustawia okres StreamMuxConfig w LATM/LOAS,
  więc trzeba użyć `-f 6`, `-f 7` albo `-f 10`
- Wartości: `0.01..600` sekund; domyślnie `-1` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Ramki są niewygodną jednostką, gdy operator strumienia określa wymaganie, bo czas trwania
ramki zależy od częstotliwości próbkowania i profilu. Ten przełącznik bierze sekundy wprost
i przelicza je sam, więc ta sama linia poleceń zachowuje się tak samo przy 44,1 i 48 kHz.

Wygrywa z `--sbr-header-period`, gdy podane są oba.

Przy AAC-ELD w ADTS albo M4A nie ma czego powtarzać: konfiguracja siedzi tylko w kontenerze,
a `--verbose` raportuje okres nagłówka SBR jako okres konfiguracji LATM/LOAS, a nie jako
nagłówek SBR w strumieniu.

Przykłady, skracanie czasu:
- Łagodnie: `--sbr-header-time 0,25`. Ćwierć sekundy czekania w najgorszym przypadku.
- Mocno: `--sbr-header-time 0,05`. Praktycznie natychmiastowe zaskoczenie dla słuchacza,
  który dołącza w losowym momencie.
- Przesadnie: `--sbr-header-time 0.01`, czyli minimum. Przy większości częstotliwości
  próbkowania to mniej niż ramka, więc sprowadza się to do nagłówka w każdej ramce z
  maksymalnym narzutem, jaki z tego wynika.

Przykłady, wydłużanie czasu:
- Łagodnie: `--sbr-header-time 1`. Jedna sekunda, blisko typowego wymagania operatorów.
- Mocno: `--sbr-header-time 3.5`. Trzy i pół sekundy dźwięku z samego rdzenia dla nowego
  słuchacza. Zmierzone po stronie dekodera przy tym ustawieniu: najdłuższe zaobserwowane
  oczekiwanie na górne pasmo po dołączeniu w środku strumienia wyniosło 3,41 s dla HE-AAC,
  3,46 s dla HE-AAC v2 i 3,52 s dla AAC-ELD.
- Przesadnie: `--sbr-header-time 600`, czyli maksimum, dziesięć minut. Legalne, a dla
  strumienia na żywo nie do odróżnienia od wysyłania strumienia z samym rdzeniem.

Współpraca i konflikty: nadpisuje `--sbr-header-period`. Wynik widać w `--verbose`, który
wypisuje efektywny okres na oba sposoby. Jako separator dziesiętny przyjmowane są zarówno
`.`, jak i `,`.

### Detektor transjentów SBR: `--sbr-tran-peak` `--sbr-tran-quiet` `--sbr-tran-dom` `--sbr-tran-thr` `--sbr-tran-split`

SBR dzieli każdą ramkę na szczeliny czasowe i na podstawie energii w tych szczelinach
rozstrzyga, czy w górnym paśmie coś zaatakowało. Jeśli uzna, że tak, atak dostaje własną
krótką obwiednię, a skok poziomu jest zakodowany tam, gdzie faktycznie następuje. Jeśli
uzna, że nie, jedna obwiednia pokrywa cały odcinek, a energia uderzenia zostaje po nim
uśredniona.

Uśrednianie przez atak jest słyszalne i słychać je *przed* atakiem, a nie po nim: tuż przed
hi-hatem albo klaśnięciem pojawia się krótki wybuch wysokoczęstotliwościowego syku. To
pre-echo, a w paśmie odtworzonym przez SBR jest to jeden z łatwiej rozpoznawalnych
artefaktów kodowania, bo materiał na górze jest już i tak dość syntetyczny, więc rozmycie
odbiera się jako szum, a nie jako miękki atak.

Do tego detektora sięga pięć pokręteł. Jedno z nich, `--sbr-tran-thr`, jest znacznie
skuteczniejsze od pozostałych i od niego warto zacząć. Dwa z nich, `--sbr-tran-quiet` i
`--sbr-tran-dom`, siedzą w drugim, szybszym detektorze, który działa tylko w AAC-ELD z SBR,
więc w HE-AAC i HE-AAC v2 nie robią nic.

Wszystkie pięć to pokrętła x100 i wszystkie działają w obu kierunkach: `100` oznacza
dokładnie stałą fabryczną, poniżej 100 stała maleje, powyżej 100 rośnie. `--sbr-tran-peak`
jest wyjątkiem co do znaczenia, nie co do formatu: zastępuje stałą, zamiast ją skalować.

#### `--sbr-tran-thr <n>`

**Skaluje główny próg, który decyduje o tym, jak chętnie detektor transjentów SBR w ogóle
się odpala, i jest to jedyne pokrętło w tej sekcji o dużym, zmierzonym efekcie.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor porównuje energie szczelin z progiem wyznaczonym z sygnału. Po obniżeniu progu
ataki z pogranicza, które wcześniej przechodziły niezauważone, dostają własną obwiednię; po
jego podniesieniu własną obwiednię dostają tylko najbardziej oczywiste uderzenia. Skoro to
przeoczony atak produkuje pre-echo, obniżanie tej wartości jest lekarstwem wprost, a kosztem
są dane obwiedni: więcej wykrytych transjentów to więcej obwiedni, a więcej obwiedni to
więcej bitów, zabranych rdzeniowi.

To pokrętło, które uzasadnia istnienie całej grupy. Na materiale perkusyjnym usuwa pre-echo
w górnym paśmie niemal całkowicie.

Zmierzone, pre-echo w paśmie SBR. Sygnał testowy: 24 ataki perkusyjne o charakterze hi-hatu
i klaśnięcia plus cztery wybrzmiewające tony między 8 a 13 kHz. Metryka: energia powyżej 9
kHz pojawiająca się w dwóch okienkach 5,8 ms przed każdym atakiem, której nie ma w źródle,
więc mniej znaczy lepiej. Zakodowane jako HE-AAC, zdekodowane, wcześniej dopasowane w czasie
korelacją wzajemną:

| `--sbr-tran-thr` | pre-echo przy 32 kbps | pre-echo przy 64 kbps |
|---|---|---|
| fabryczne | 1.610 | 1.186 |
| 20 / 40 / 60 | 0.041 | 0.059 |
| 80 | 0.661 | 0.502 |
| 100 i więcej | 1.610 (= fabryczne) | 1.186 (= fabryczne) |

Efekt jest monotoniczny, odtwarzalny na obu bitrate'ach i poniżej 60 wchodzi w nasycenie.
Wartości 150 i 400 też zmieniają strumień, tylko w drugą stronę.

Przykłady, obniżanie progu (detektor odpala się chętniej):
- Łagodnie: `--sbr-tran-thr 80`. Pre-echo na zmierzonym sygnale zmniejszone z grubsza o
  połowę, umiarkowany dodatkowy koszt obwiedni.
- Mocno: `--sbr-tran-thr 40`. Na materiale perkusyjnym pre-echo praktycznie znika. To
  zalecany punkt startu, gdy na górze słychać rozmycie.
- Przesadnie: `--sbr-tran-thr 1`. Detektor odpala się na niemal wszystkim. Poniżej 60 nie
  ma już nic do ugrania w pre-echu, więc rosną tylko dane obwiedni wydane na
  niezdarzenia, co przy niskim bitrate'ie zabiera bity rdzeniowi i pogarsza cały plik.

Przykłady, podnoszenie progu (detektor odpala się rzadziej):
- Łagodnie: `--sbr-tran-thr 150`. Ataki z pogranicza są ignorowane, trochę mniej obwiedni,
  trochę więcej rozmycia. Czasem warto na materiale spokojnym przy bardzo ciasnym budżecie.
- Mocno: `--sbr-tran-thr 400`. Własną obwiednię dostają tylko ataki niebudzące
  wątpliwości. Na perkusji słychać wyraźnie więcej pre-echa.
- Przesadnie: `--sbr-tran-thr 10000`. Detektor praktycznie nigdy się nie odpala; każda
  ramka jest opisana statyczną siatką. Ustawienie skrajne, i to przydatne, gdy celem jest
  usłyszenie artefaktu, któremu ten detektor ma zapobiegać.

Współpraca i konflikty: `--sbr-num-env` wymusza statyczną siatkę, a więc usuwa korzyść z
podziału czasu, którą kupuje to pokrętło, co sprawia, że łączenie ich jest wzajemnym
sabotażem. `--sbr-tran-split` zajmuje się osobnym pytaniem, czyli dzieleniem ramek, w
których żaden transjent nie został wykryty. Na pre-echo w *rdzeniu* (LC, LD, ELD) narzędzia
są w grupie E.

#### `--sbr-tran-split <n>`

**Skaluje próg dzielenia na dwie obwiednie ramki, w której nie wykryto żadnego
transjentu.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Nie każda zmiana poziomu w górnym paśmie jest atakiem. Crescendo, narastanie, zmiana akordu
albo nierówno wybrzmiewający talerz poruszają górnym pasmem, nie wyzwalając detektora
transjentów, a jedna obwiednia na całą ramkę ten ruch uśrednia. Ten próg rozstrzyga, jak
chętnie taka ramka zostanie jednak podzielona na dwie, tak żeby druga połowa dostała własne
poziomy.

Obniżanie dzieli częściej: drobniejsza rozdzielczość czasowa na materiale, który się
porusza, ale nie atakuje, kosztem drugiego zestawu poziomów obwiedni w wielu ramkach.
Podnoszenie dzieli rzadziej: taniej, a górne pasmo mniej dokładnie podąża za powolnym
ruchem.

Efekt jest mniejszy niż przy `--sbr-tran-thr` i dotyczy innego objawu. Gdy problemem jest
syk przed uderzeniem, trzeba sięgnąć po próg; gdy problemem jest górne pasmo brzmiące
statycznie w czasie narastania muzyki, po to pokrętło.

Przykłady, obniżanie (dzielenie częściej):
- Łagodnie: `--sbr-tran-split 70`. Trochę więcej ramek dostaje dwie obwiednie.
- Mocno: `--sbr-tran-split 30`. Większość ramek z jakimkolwiek ruchem zostaje podzielona.
  Zauważalnie więcej danych obwiedni, lepsze podążanie za narastaniami.
- Przesadnie: `--sbr-tran-split 1`. Niemal każda ramka podzielona bez względu na treść;
  koszt bitowy jest realny, a korzyść na materiale spokojnym żadna.

Przykłady, podnoszenie (dzielenie rzadziej):
- Łagodnie: `--sbr-tran-split 150`. Marginalnie mniej podziałów, marginalnie taniej.
- Mocno: `--sbr-tran-split 300`. Podziały stają się rzadkie. Zmierzone: to zmienia
  strumień HE-AAC.
- Przesadnie: `--sbr-tran-split 10000`. Żadna ramka nie zostaje podzielona wyłącznie z
  powodu ruchu. Jedna obwiednia na ramkę bez transjentu, maksymalne rozmycie powolnych
  zmian w górnym paśmie, minimum danych obwiedni.

Współpraca i konflikty: w praktyce ignorowane, gdy `--sbr-num-env` wymusza statyczną siatkę.
Uzupełnia `--sbr-tran-thr`, a nie dubluje: tamto dotyczy ataków, to ramek bez ataku. Każda
dodatkowa obwiednia jest wyceniana przez `--sbr-amp-res`.

#### `--sbr-tran-peak <n>`

**Ustala, jak szczytowy musi być kandydat, żeby policzył się jako atak: sygnał musi spaść
poniżej tego ułamka poprzedniej szczeliny.**

- Dotyczy: profili z SBR
- Wartości: `1..200`, surowe x100, gdzie `90` = fabryczne 0,90; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Prawdziwy atak nie jest tylko głośny, jest też krótki: energia rośnie, a potem znów spada.
Detektor sprawdza ten spadek, wymagając, żeby kolejna szczelina spadła poniżej ułamka
poprzedniej. Tym ułamkiem jest właśnie ta stała.

Uwaga na jednostkę. To **surowa** wartość x100, a nie współczynnik skalujący: zastępuje
stałą wprost, więc `90` to fabryczne 0,90 i nic nie zmienia, a `100` to 1,0, a nie wartość
fabryczna. A 1,0 ma szczególną konsekwencję: ułamek równy 1,0 albo większy oznacza, że
warunek nigdy nie może zostać spełniony przez prawdziwy spadek w taki sposób, w jaki
detektor go sprawdza, więc żadna szczelina nigdy nie liczy się jako spadająca. Każda wartość
od 100 do 200 produkuje zatem jeden i ten sam strumień. Żywy zakres to 1..99.

Obniżanie wartości wymaga ostrzejszego spadku, więc jako ataki kwalifikuje się mniej
kandydatów. Podnoszenie w stronę 99 przyjmuje za ataki łagodniejsze wybrzmienia, więc
kwalifikuje się ich więcej.

Przykłady, obniżanie (surowiej, mniej transjentów):
- Łagodnie: `--sbr-tran-peak 80`. Trochę surowiej niż fabrycznie; miękkie ataki przestają
  się kwalifikować.
- Mocno: `--sbr-tran-peak 50`. Liczą się tylko ostro wybrzmiewające uderzenia. Na
  wszystkim poza suchą perkusją słychać więcej rozmycia.
- Przesadnie: `--sbr-tran-peak 1`. Wymóg spadku jest absurdalny; prawie nic nie kwalifikuje
  się jako atak i sam test szczytu tłumi detektor.

Przykłady, podnoszenie (luźniej, więcej transjentów):
- Łagodnie: `--sbr-tran-peak 95`. Łagodniejsze wybrzmienia liczą się już jako ataki.
- Mocno: `--sbr-tran-peak 99`. Najluźniejsze ustawienie, które jeszcze cokolwiek robi.
  Ataki z długimi ogonami dostają własne obwiednie.
- Przesadnie: `--sbr-tran-peak 200`. Poza użytecznym zakresem: 100 i więcej znaczy zawsze
  1,0, żadna szczelina nie liczy się jako spadająca, a wartości od 100 do 200 to jeden i
  ten sam strumień. Przydatne tylko jako pokaz, że to pokrętło ma twardą krawędź.

Współpraca i konflikty: to jeden z kilku warunków; `--sbr-tran-thr` skaluje główny próg i ma
znacznie większą dźwignię. Gdy celem jest w ogóle więcej albo mniej transjentów, warto
zacząć od tamtego, a tego użyć tylko do zmiany *charakteru* tego, co się kwalifikuje.

#### `--sbr-tran-quiet <n>`

**Ustala najniższy dozwolony poziom energii cichej szczeliny w szybkim detektorze
transjentów SBR, co rozstrzyga, jak chętnie ciche fragmenty produkują transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2, które
  używają drugiego detektora
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

AAC-ELD z SBR uruchamia drugi detektor transjentów, o niższym opóźnieniu, a ten detektor ma
dolną granicę energii, poniżej której szczelina jest traktowana jako cicha. Granica istnieje
po to, żeby szum w ciszy nie rejestrował się jako ataki. Po jej obniżeniu detektor zagląda w
cichsze fragmenty, więc miękkie uderzenia w rzadkiej aranżacji mogą dostać własną obwiednię.
Po jej podniesieniu cichy materiał jest ignorowany.

Zachowanie silnie zależy od materiału, co warto wiedzieć przed poświęceniem temu czasu: na
sygnale testowym czysto tonalnym detektor nigdy nie doszedł do decyzji, którą to pokrętło
mogłoby zmienić.

Zmierzone na AAC-ELD z SBR, 24 kbps, na pięciu sygnałach testowych: wartości od 30 do 99
zmieniają strumień, `100` i więcej nie dały żadnej zmiany, a `1` i `10` dają ten sam
strumień co siebie wzajemnie. Żywy zakres to więc w dół z grubsza 11..99, z nasyceniem
poniżej i sufitem przy 100.

Przykłady, obniżanie (ciche fragmenty chętniej produkują transjenty):
- Łagodnie: `--sbr-tran-quiet 90`. Niewielkie rozszerzenie na cichsze szczeliny.
- Mocno: `--sbr-tran-quiet 50`. Wyraźnie czulej na materiale rzadkim; zmierzone, że
  zmienia strumień.
- Przesadnie: `--sbr-tran-quiet 1`. Na samym dole: `1` i `10` dają ten sam strumień, więc
  detektor dobił do dna tego, co ta dolna granica potrafi wyrazić.

Przykłady, podnoszenie (ciche fragmenty ignorowane):
- Od łagodnie do przesadnie: `--sbr-tran-quiet 120`, `300`, `10000` nie dały w naszych
  pomiarach żadnej zmiany strumienia na ELD plus SBR 24 kbps. Fabryczna dolna granica jest
  już tak wysoka, że dalsze jej podnoszenie nie zmienia na tym materiale żadnej decyzji.
  Uczciwy status: kierunek jest podłączony, efektu w górę nie zaobserwowano.

Współpraca i konflikty: nic w HE-AAC ani HE-AAC v2, bo szybki detektor nie jest tam
używany. W obrębie ELD plus SBR stoi obok `--sbr-tran-dom`; `--sbr-tran-thr` i
`--sbr-tran-split` dotyczą głównego detektora.

#### `--sbr-tran-dom <n>`

**Ustala, o ile głośniejsza musi być szczelina od dwóch poprzedzających, kiedy te są już
oznaczone jako transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2
- Wartości: `100..500`, x100, gdzie `140` = fabryczne 1,4; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Werbel, flam albo podwójne uderzenie produkują kilka szczelin kandydujących z rzędu. Ta
reguła nie pozwala detektorowi oznaczyć wszystkich: gdy dwie poprzedzające szczeliny są już
transjentami, następna musi być wyraźnie głośniejsza, o ten właśnie współczynnik, żeby też
została oznaczona. To test dominacji, stąd nazwa.

Obniżanie współczynnika w stronę 100 pozwala liczyć się wszystkim kolejnym szczelinom, więc
gęsta perkusja produkuje więcej transjentów. Podnoszenie oznacza, że liczy się tylko
zdecydowana eskalacja, więc werbel rejestruje się raz.

Uczciwy status: to pokrętło jest podłączone do stałej, którą dokumentuje, i w naszych
pomiarach nie dało żadnej zmiany strumienia. Wartości od 100 do 500 dały identyczne wyjście
na pięciu sygnałach testowych na ELD plus SBR. Powód jest najprawdopodobniej taki, że
warunki wokół rzadko są na tym materiale spełnione jednocześnie, więc o wyniku nie decyduje
test dominacji. Warto traktować je jako dostępne do eksperymentów, a nie jako narzędzie o
przewidywalnym rezultacie, i sięgać raczej po `--sbr-tran-thr`.

Przykłady, obniżanie (kolejne szczeliny liczą się chętniej):
- Łagodnie: `--sbr-tran-dom 130`. Trochę poniżej ustawienia fabrycznego.
- Mocno: `--sbr-tran-dom 110`. Praktycznie żaden wymóg dominacji.
- Przesadnie: `--sbr-tran-dom 100`. Współczynnik wynosi 1,0, więc szczelina musi jedynie
  dorównać dwóm przed nią. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (liczy się tylko zdecydowana eskalacja):
- Łagodnie: `--sbr-tran-dom 180`. Powyżej ustawienia fabrycznego.
- Mocno: `--sbr-tran-dom 300`. Szczelina musi mieć trzykrotny poziom swoich poprzedników.
- Przesadnie: `--sbr-tran-dom 500`, maksimum. Przy żadnej z tych wartości nie
  zaobserwowano zmiany strumienia.

Współpraca i konflikty: ten sam szybki detektor ELD plus SBR co przy `--sbr-tran-quiet` i to
samo ograniczenie: nic w HE-AAC. Gdy celem jest słyszalna zmiana zachowania transjentowego
tego detektora, efekt zmierzony ma `--sbr-tran-quiet` w zakresie 30..99.

### Detektor brakujących harmonicznych: `--sbr-mh-tone` `--sbr-mh-diff` `--sbr-mh-decay-orig` `--sbr-mh-decay-diff` `--sbr-mh-sfm-sbr` `--sbr-mh-sfm-orig` `--sbr-mh-maxcomp` `--sbr-mh-deltatime`

Detektor transjentów zajmuje się czasem. Ten detektor zajmuje się problemem, który tworzy
samo kopiowanie.

SBR buduje górne pasmo, odtwarzając je z widma z dołu. Ton, który istniał w oryginalnym
górnym paśmie, nie ma gwarancji, że to zostanie: odtworzenie podnosi to, co akurat było w
obszarze źródłowym, więc prawdziwa składowa może wylądować na złej częstotliwości, przybyć w
towarzystwie składowych, których nigdy tam nie było, albo po prostu zniknąć. Na dzwonkach,
trójkącie, dzwonie albo jasnej harmonicznej smyczków to jest dokładnie ta treść, którą słuch
wychwytuje. Detektor brakujących harmonicznych porównuje tonalność oryginalnego pasma z
tonalnością pasma odtworzonego przez SBR, rozstrzyga, że tonu brakuje, i sygnalizuje
dekoderowi, żeby wstawił w tym paśmie syntetyczną sinusoidę.

Oba kierunki porażki są słyszalne. Zbyt gorliwa praca detektora daje świstanie: stałe,
dźwięczne tony na górze, których nigdy nie było w nagraniu, często trochę obok prawdziwego
szeregu harmonicznego, czyli klasyczne "falowanie SBR". Zbyt zachowawcza sprawia, że dzwony
oraz talerze matowieją, gdy ich składowe po cichu znikają.

Do tego detektora sięga osiem pokręteł. Przed poświęceniem im czasu warto poznać ich
prawdziwy status: na naszym materiale testowym tylko dwa z nich w ogóle poruszyły strumień.
`--sbr-mh-tone` zmieniło wyjście na sygnale tonalnym w HE-AAC 24 kbps przy wartościach 1, 5
i 10, a `--sbr-mh-diff` przy wartości 10. Pozostałych sześć nie dało żadnej zmiany
strumienia w HE-AAC od 24 do 96 kbps na materiale tonalnym i mieszanym, w żadnym kierunku.
To właściwość tego detektora na tym materiale, a nie skalowania: pokrętła x100 działają tu w
obu kierunkach, a `100` to dokładnie ustawienie fabryczne. Detektor po prostu potrzebuje
zbieżności kilku warunków, zanim którykolwiek z nich zdecyduje o wyniku, a na zwyczajnej
muzyce rzadko do tego dochodzi. Każda karta poniżej mówi wprost, jak z tym jest.

#### `--sbr-mh-tone <n>`

**Skaluje to, jak tonalne musi być pasmo, zanim SBR sfabrykuje w nim syntetyczną
harmoniczną, i jest to główna kontrolka tej sekcji.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor mierzy tonalność w oryginalnym paśmie i porównuje ją z progiem. Skalowanie tego
progu w dół sprawia, że więcej pasm kwalifikuje się jako tonalne, więc dodawanych jest
więcej syntetycznych harmonicznych. Skalowanie w górę sprawia, że kwalifikuje się ich mniej.

To, co słychać w każdym kierunku, jest konkretne. Więcej dodanych tonów oznacza, że więcej
dźwięcznej treści dzwonu albo talerza zostaje przywrócone, a zarazem rośnie ryzyko
artefaktów świstania, których nigdy nie było w źródle. Mniej dodanych tonów oznacza
bezpieczniejszą, bardziej matową górę, w której jasna perkusja traci dzwonienie.

Zmierzone: to jedno z dwóch pokręteł w tej sekcji, które poruszyły strumień, na tonalnym
sygnale testowym w HE-AAC 24 kbps, przy wartościach 1, 5 i 10. Wszystkie leżą daleko poniżej
ustawienia fabrycznego, co pokazuje, jak nisko musi zejść próg, żeby detektor zmienił zdanie
na tym materiale.

Przykłady, obniżanie progu (więcej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 70`. Umiarkowane poszerzenie tego, co liczy się jako tonalne. Na
  wielu materiałach w ogóle nie zmieni to strumienia.
- Mocno: `--sbr-mh-tone 10`. Zmierzone, że zmienia strumień na materiale tonalnym. Słychać
  więcej dodanych tonów i jaśniejsze, bardziej dźwięczne górne pasmo.
- Przesadnie: `--sbr-mh-tone 1`. Niemal każde pasmo liczy się jako tonalne. Zmierzone, że
  zmienia strumień, i to kierunek do wybrania, gdy celem jest usłyszenie, jak brzmią
  artefakty świstania.

Przykłady, podnoszenie progu (mniej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 150`. Trochę surowiej.
- Mocno: `--sbr-mh-tone 400`. Harmoniczną dostają tylko pasma silnie tonalne.
- Przesadnie: `--sbr-mh-tone 10000`. Detektor praktycznie nigdy nie dodaje harmonicznej.
  Powyżej 100 nie zaobserwowano na naszym materiale zmiany strumienia, więc przy takim celu
  trzeba to najpierw sprawdzić na własnym materiale.

Współpraca i konflikty: `--sbr-mh-diff` to druga ścieżka wyzwalania tej samej decyzji, a
`--sbr-mh-sfm-orig` / `--sbr-mh-sfm-sbr` przesuwają granicę między tonem a szumem, która
zasila pomiar tonalności. `--sbr-mh-maxcomp` rządzi tym, co dzieje się wokół harmonicznej
po jej dodaniu, a nie tym, czy zostaje dodana.

#### `--sbr-mh-diff <n>`

**Skaluje różnicę tonalności między oryginałem a pasmem odtworzonym przez SBR, która
wyzwala kompensację harmonicznych.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

`--sbr-mh-tone` pyta "czy oryginalne pasmo jest dość tonalne". To pyta o coś innego: "czy
odtworzenie zmieniło tonalność". To ścieżka, która wyłapuje przypadek, gdy oryginał miał
jeden czysty ton, a pasmo odtworzone przez SBR ma ich kilka, albo odwrotnie, czyli dokładnie
to, co psuje kopiowanie.

Obniżanie progu sprawia, że mniejsza różnica liczy się jako szkoda, więc kompensacja wyzwala
się częściej. Podnoszenie oznacza, że liczy się tylko duża niezgodność.

Zmierzone: to drugie z dwóch pokręteł w tej sekcji, które poruszyły strumień, przy wartości
10 na naszym materiale. Wartości bliższe ustawieniu fabrycznemu oraz wartości powyżej niego
nie dały żadnej zmiany.

Przykłady, obniżanie (kompensacja przy mniejszych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 70`. Umiarkowanie; na danym materiale może nie zmienić nic.
- Mocno: `--sbr-mh-diff 20`. Blisko wartości, przy której widzieliśmy zmianę.
- Przesadnie: `--sbr-mh-diff 10`. Zmierzone, że zmienia strumień. Niemal każda niezgodność
  tonalności między oryginałem a pasmem odtworzonym przez SBR wyzwala kompensację.

Przykłady, podnoszenie (kompensacja tylko przy dużych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 150`.
- Mocno: `--sbr-mh-diff 400`.
- Przesadnie: `--sbr-mh-diff 10000`. Ścieżka różnicowa praktycznie nigdy się nie odpala,
  zostawiając `--sbr-mh-tone` jako jedyny wyzwalacz. W górę nie zaobserwowano na naszym
  materiale zmiany strumienia.

Współpraca i konflikty: obie ścieżki wyzwalania, ta i `--sbr-mh-tone`, zasilają tę samą
decyzję, więc podnoszenie jednej przy obniżaniu drugiej częściowo się znosi. Pokrętła
płaskości `--sbr-mh-sfm-sbr` i `--sbr-mh-sfm-orig` rozstrzygają, co w ogóle liczy się jako
tonalne po każdej stronie porównania.

#### `--sbr-mh-decay-orig <n>`

**Skaluje to, jak długo już wykryty ton jest nadal śledzony w oryginalnym paśmie, gdy
wybrzmiewa.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor, który patrzyłby tylko na bieżącą ramkę, porzucałby ton, gdy tylko spadnie poniżej
progu, a na ogonie dzwonu albo talerza jest to dokładnie moment, w którym ton jest jeszcze
potrzebny. Dlatego detektor nadal śledzi ton, który już znalazł, z wybrzmiewaniem. To
pokrętło skaluje długość tej pamięci.

Dłuższe śledzenie oznacza, że syntetyczne harmoniczne utrzymują się w ogonach, więc
wybrzmienia pozostają dźwięczne, zamiast zamieniać się w szum. Krótsze śledzenie oznacza, że
tony są porzucane szybciej, więc ogony tracą dzwonienie wcześniej.

Uczciwy status: podłączone, efektu nie zaobserwowano. Na materiale tonalnym i mieszanym w
HE-AAC od 24 do 96 kbps nie dało to żadnej zmiany strumienia w żadnym kierunku.
Wybrzmiewanie ma znaczenie dopiero wtedy, gdy detektor znalazł ton do śledzenia, a warunki
wyzwalania rzadko zbiegały się na naszym materiale.

Przykłady, skracanie śledzenia:
- Łagodnie: `--sbr-mh-decay-orig 70`.
- Mocno: `--sbr-mh-decay-orig 30`.
- Przesadnie: `--sbr-mh-decay-orig 1`. Praktycznie brak pamięci: ton jest zapominany, gdy
  tylko spadnie. Nie zaobserwowano zmiany strumienia.

Przykłady, wydłużanie śledzenia:
- Łagodnie: `--sbr-mh-decay-orig 150`.
- Mocno: `--sbr-mh-decay-orig 400`.
- Przesadnie: `--sbr-mh-decay-orig 10000`. Raz znaleziony ton jest śledzony niemal
  bezterminowo. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: ma znaczenie tylko tam, gdzie detektor się odpala, więc obniżenie
`--sbr-mh-tone` do jego zmierzonego zakresu aktywnego jest warunkiem wstępnym, żeby to
pokrętło w ogóle coś znaczyło. `--sbr-mh-decay-diff` to ten sam mechanizm na prowadnicy
różnicowej.

#### `--sbr-mh-decay-diff <n>`

**To samo wybrzmiewanie co w `--sbr-mh-decay-orig`, zastosowane do prowadnicy różnicowej, a
nie do oryginalnego pasma.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor prowadzi dwie prowadnice śledzące: jedną podążającą za tonami w oryginalnym paśmie,
drugą podążającą za różnicą tonalności między oryginałem a pasmem odtworzonym przez SBR. To
pokrętło skaluje pamięć drugiej, więc rządzi tym, jak długo wykryta *niezgodność* jest nadal
traktowana jako obecna po tym, gdy przestanie być oczywista.

Dłużej oznacza, że decyzje o kompensacji utrzymują się między ramkami i są stabilniejsze.
Krócej oznacza, że są przeliczane niemal od zera w każdej ramce, co reaguje szybciej i może
migotać.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps, w żadnym kierunku.

Przykłady, skracanie:
- Łagodnie: `--sbr-mh-decay-diff 70`.
- Mocno: `--sbr-mh-decay-diff 30`.
- Przesadnie: `--sbr-mh-decay-diff 1`. Brak pamięci na prowadnicy różnicowej. Nie
  zaobserwowano zmiany strumienia.

Przykłady, wydłużanie:
- Łagodnie: `--sbr-mh-decay-diff 150`.
- Mocno: `--sbr-mh-decay-diff 400`.
- Przesadnie: `--sbr-mh-decay-diff 10000`. Prowadnica nigdy nie zapomina. Nie zaobserwowano
  zmiany strumienia.

Współpraca i konflikty: tworzy parę z `--sbr-mh-decay-orig` i podobnie jak ono zależy od
tego, czy `--sbr-mh-diff` faktycznie się wyzwala, zanim długość pamięci może mieć znaczenie.

#### `--sbr-mh-sfm-sbr <n>`

**Skaluje próg płaskości widmowej, powyżej którego pasmo odtworzone przez SBR jest uznawane
za szumopodobne, a nie tonalne.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Płaskość widmowa mierzy, jak równo energia rozkłada się w paśmie: blisko 0 dla jednego
czystego tonu, blisko 1 dla szumu. Detektor używa jej do klasyfikowania każdego pasma, a to
pokrętło przesuwa granicę dla strony *odtworzonej przez SBR* w tym porównaniu.

Obniżanie progu oznacza, że pasmo odtworzone przez SBR szybciej zostaje nazwane
szumopodobnym, więc detektor częściej dochodzi do wniosku, że kopia zgubiła ton i
kompensacja jest potrzebna. Podnoszenie oznacza, że odtworzone pasmo dostaje kredyt zaufania
jako tonalne, więc dodawanych jest mniej harmonicznych.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Jedna praktyczna uwaga do pomiarów własnych: płaskość trzeba mierzyć w
prawdziwym obszarze SBR, od pasma rdzenia aż do końcowej górnej krawędzi wypisanej przez
`--verbose`. Okno sięgające do Nyquista obejmuje puste miejsce powyżej górnej krawędzi i
zwraca płaskość równą 0, co wygląda jak doskonała tonalność, a jest artefaktem okna.

Przykłady, obniżanie (odtworzone pasmo szybciej nazywane szumopodobnym, więcej
kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 70`.
- Mocno: `--sbr-mh-sfm-sbr 30`.
- Przesadnie: `--sbr-mh-sfm-sbr 1`. Niemal każde pasmo odtworzone przez SBR liczy się jako
  szumopodobne. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (odtworzone pasmo dłużej nazywane tonalnym, mniej kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 150`.
- Mocno: `--sbr-mh-sfm-sbr 400`.
- Przesadnie: `--sbr-mh-sfm-sbr 10000`. Pasmo odtworzone przez SBR jest praktycznie zawsze
  tonalne. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: działa w parze z `--sbr-mh-sfm-orig`; razem wyłapują sytuację "z
jednego tonu w oryginale po odtworzeniu zrobiło się kilka". Przesunięcie tylko jednej strony
przechyla porównanie asymetrycznie, co bywa pożądane, ale jest trudniejsze do przemyślenia.

#### `--sbr-mh-sfm-orig <n>`

**Ten sam próg płaskości co `--sbr-mh-sfm-sbr`, dla oryginalnego pasma.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

To przesuwa granicę między tonem a szumem po stronie źródła. Obniżanie oznacza, że
oryginalne pasmo szybciej zostaje nazwane szumopodobnym, więc detektor rzadziej dochodzi do
wniosku, że był tam ton wart przywrócenia. Podnoszenie oznacza, że oryginał jest chętniej
traktowany jako tonalny, więc więcej pasm staje się kandydatami na syntetyczną harmoniczną.

Warto zauważyć, że oba pokrętła sfm pchają końcową decyzję w przeciwnych sensach, choć są
tym samym rodzajem progu, bo jedno opisuje stan wejściowy, a drugie wynik. Gdy celem jest
"dodawać więcej harmonicznych", trzeba obniżyć `--sbr-mh-sfm-sbr` i podnieść to.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Przykłady, obniżanie (oryginał szybciej nazywany szumopodobnym, mniej harmonicznych):
- Łagodnie: `--sbr-mh-sfm-orig 70`.
- Mocno: `--sbr-mh-sfm-orig 30`.
- Przesadnie: `--sbr-mh-sfm-orig 1`. Źródło niemal nigdy nie jest tonalne, więc niemal nic
  nie jest uznawane za brakujące. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (oryginał chętniej nazywany tonalnym, więcej harmonicznych):
- Łagodnie: `--sbr-mh-sfm-orig 150`.
- Mocno: `--sbr-mh-sfm-orig 400`.
- Przesadnie: `--sbr-mh-sfm-orig 10000`. Niemal każde oryginalne pasmo liczy się jako
  tonalne. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: zwierciadlane odbicie `--sbr-mh-sfm-sbr`, a oba zasilają porównanie
tonalności, dla którego `--sbr-mh-diff` ustawia próg. Zmiana wszystkich trzech naraz
sprawia, że wynik jest bardzo trudny do przypisania; lepiej ruszać po jednym.

#### `--sbr-mh-maxcomp <n>`

**Ogranicza, ile kompensacji obwiedni jest stosowane wokół syntetycznej harmonicznej.**

- Dotyczy: profili z SBR
- Wartości: `0..200`, zwykła liczba całkowita, gdzie `50` = fabryczne; domyślnie `-1` =
  wyłączone
- Wpisywanie: liczba całkowita

Wstawienie sinusoidy w pasmo zmienia energię tego pasma, więc obwiednia wokół niej musi
zostać skorygowana, inaczej pasmo wyjdzie za głośne. To pokrętło ogranicza tę korektę. Nie
jest to skala x100: liczba jest samym limitem.

Wyższy limit dopuszcza większe korekty, więc pasma sąsiednie są ściągane dalej i mniej szumu
zostaje obok dodanego tonu: harmoniczna wybija się czyściej. Niższy limit ogranicza korektę,
więc dodany ton siedzi w większej części swojego pierwotnego szumu otoczenia i się w nim
zlewa, albo staje się słyszalnie za głośny, gdy potrzebna korekta była większa od limitu.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku, co wynika z tego, że
detektor i tak rzadko dodaje na tym materiale harmoniczną.

Przykłady, obniżanie limitu:
- Łagodnie: `--sbr-mh-maxcomp 35`.
- Mocno: `--sbr-mh-maxcomp 10`. Dopuszczona bardzo niewielka kompensacja.
- Przesadnie: `--sbr-mh-maxcomp 0`. Żadnej kompensacji wokół dodanej harmonicznej. Nie
  zaobserwowano zmiany strumienia.

Przykłady, podnoszenie limitu:
- Łagodnie: `--sbr-mh-maxcomp 70`.
- Mocno: `--sbr-mh-maxcomp 120`.
- Przesadnie: `--sbr-mh-maxcomp 200`, maksimum. Korekta jest praktycznie nieograniczona.
  Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: działa tylko tam, gdzie harmoniczna faktycznie zostaje dodana, więc
`--sbr-mh-tone` musi najpierw być w swoim zakresie aktywnym. Szumem, który siedzi obok
dodanego tonu, rządzą także `--sbr-noise-max` i pokrętła poziomu szumu, działające na każde
pasmo, a nie tylko na sąsiadów harmonicznej.

#### `--sbr-mh-deltatime <n>`

**Ustala maksymalną odległość transjentu, przy której ramka liczy się jako ramka
transjentowa wewnątrz detektora brakujących harmonicznych.**

- Dotyczy: profili z SBR
- Wartości: `0..64`, zwykła liczba całkowita; domyślnie `-1` = wyłączone, co zostawia 9 dla
  AAC i 16 dla AAC-LD
- Wpisywanie: liczba całkowita

Detektor brakujących harmonicznych zachowuje się inaczej w ramkach, które uznaje za
transjentowe, bo dodawanie stałej syntetycznej sinusoidy przez atak to zły pomysł. To
pokrętło ustala, jak daleko, w jednostkach czasu detektora, transjent może być od ramki,
żeby ramka wciąż liczyła się jako transjentowa. To zwykła liczba całkowita, a nie skala
x100, a wartość fabryczna różni się zależnie od profilu.

Większa odległość oznacza, że więcej ramek liczy się jako transjentowe, więc detektor
częściej wybiera swoją ostrożną ścieżkę i dodaje mniej harmonicznych w sąsiedztwie uderzeń.
Mniejsza odległość oznacza, że mniej ramek liczy się jako transjentowe, więc detektor dodaje
harmoniczne bliżej ataków, czyli tam, gdzie syntetyczna sinusoida najpewniej zabrzmi źle.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Przykłady, obniżanie odległości:
- Łagodnie: `--sbr-mh-deltatime 6`.
- Mocno: `--sbr-mh-deltatime 2`.
- Przesadnie: `--sbr-mh-deltatime 0`. Żadna ramka nie liczy się jako transjentowa z racji
  odległości, więc ostrożna ścieżka nigdy nie jest wybierana. Nie zaobserwowano zmiany
  strumienia.

Przykłady, podnoszenie odległości:
- Łagodnie: `--sbr-mh-deltatime 16`, czyli fabryczna wartość AAC-LD zastosowana do AAC.
- Mocno: `--sbr-mh-deltatime 32`.
- Przesadnie: `--sbr-mh-deltatime 64`, maksimum. Niemal każda ramka blisko jakiegokolwiek
  uderzenia liczy się jako transjentowa. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: to pokrętło czyta decyzję o transjencie, więc leży poniżej
`--sbr-tran-thr` i `--sbr-tran-peak` w strumieniu przetwarzania: zmiana tego, co liczy się
jako transjent, zmienia to, do czego to pokrętło mierzy odległość. Na samo dzielenie ramek
nie ma wpływu.

### Filtrowanie odwrotne i poziom szumu: `--sbr-invf` `--sbr-softness` `--sbr-invf-min` `--sbr-invf-max` `--sbr-invf-bias` `--sbr-invf-table` `--sbr-invf-hyst` `--sbr-invf-sbr-thr` `--sbr-invf-orig-thr` `--sbr-invf-smooth` `--sbr-nf-weight` `--sbr-nf-weight-from` `--sbr-nf-smooth` `--sbr-noise-max` `--sbr-noise-floor-offset`

Tutaj faktycznie rozstrzyga się charakter górnego pasma i to jest ta część SBR, którą warto
zrozumieć porządnie.

Skopiowane górne pasmo dziedziczy drobną strukturę pasma, z którego zostało skopiowane. Ta
struktura jest tonalna: szczyty i doliny wypadają na częstotliwościach nieodpowiednich dla
miejsca, w którym wylądowały. Efekt brzmi metalicznie, dzwoni, czasem jest chórowany, a kto
raz to usłyszał, nie potrafi już tego odsłyszeć. Lekarstwem jest filtrowanie odwrotne:
dekoder nakłada na odtworzone pasmo filtr wybielający, który tę odziedziczoną strukturę
spłaszcza. Są cztery poziomy mocy, 0 wyłączone, 1 niski, 2 średni, 3 wysoki, wybierane na
pasmo i na ramkę. Poziom szumu to druga połowa tego samego lekarstwa: syntetyczny szum
domieszany do każdego pasma, który zastępuje strukturę usuniętą przez wybielanie. Za mało
jednego i drugiego daje metaliczną górę; za dużo daje górę syczącą i rozmytą, w której
talerze redukują się do kształtowanego szumu.

Poziom wybiera normalnie **estymator tonalności**, który porównuje pasmo oryginału z pasmem
odtworzonym przez SBR, pasmo po paśmie i ramka po ramce. To tę dynamiczną decyzję warto
zachować: ma rację znacznie częściej niż jakakolwiek stała wartość wybrana z ręki.

Ta sekcja zawiera więc dwa rodzaje przełączników, a pierwszeństwo między nimi ma znaczenie:

1. `--sbr-invf` **wymusza** jeden ustalony poziom na cały plik, w każdym paśmie i w każdej
   chwili. Wygrywa ze wszystkim innym w tym miejscu. Warto go używać do diagnozy albo
   wtedy, gdy materiał jest na pewno jednorodny.
2. Wszystko pozostałe zostawia decyzję dynamiczną i zmienia reguły estymatora: granice,
   przesunięcie, tabelę decyzyjną, histerezę, wygładzanie, położenie granic, wagę szumu.
   Decyzja nadal jest podejmowana na pasmo i na ramkę, tylko według innych reguł.

W grupie 2 `--sbr-softness` jest makrem, które ustawia kilka z tych reguł naraz, a **każde
jawne pokrętło wygrywa z makrem**. Czyli `--sbr-softness 6 --sbr-invf-max 2` daje miękkość z
makra z własnym sufitem.

Sposób korzystania z tej sekcji kształtują dwa fakty pomiarowe.

Pierwszy: makro miękkości celowo *nie* rusza granic tonalności. Zmierzyliśmy, że ich
przesuwanie powoduje migotanie decyzji w obrębie pasma, od 0,2 do 0,29 zmiany na pasmo na
ramkę wobec 0,004 do 0,04 w ustawieniu fabrycznym, co jest słyszalne jako niestabilność.
`--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` i tak udostępniają ten mechanizm, bo czasem
jest to właśnie pożądane, ale to dlatego makro ich unika.

Drugi: waga poziomu szumu jest obcinana przez sufit szumu. Przy sufitach z tabeli strojenia
(0,125 przy 24 do 48 kbps) około 95 procent pasm i tak już siedzi na sufice, więc
`--sbr-nf-weight` samo w sobie często nie zmienia zupełnie nic. Żeby cokolwiek zrobiło,
potrzebuje podniesionego sufitu *oraz* pasm z wysokim invf: zmierzone, strumień ruszył
dopiero przy `--sbr-noise-max 6` w połączeniu z `--sbr-invf 3` albo
`--sbr-nf-weight-from 1`. Samo albo tylko z jednym z nich nie zmieniło nic. Poziomy od 5 do
7 w `--sbr-softness` podnoszą sufit samoczynnie.

Przy wartościach neutralnych (`--sbr-invf-hyst 1.0`, `--sbr-invf-bias 0`,
`--sbr-invf-smooth 2`, `--sbr-nf-weight 0.25`, `--sbr-nf-weight-from 3`,
`--sbr-invf-table 0`, `--sbr-invf-min 0`, `--sbr-invf-max 3`) te pokrętła dają strumień
identyczny bajt w bajt z ustawieniem fabrycznym, podobnie jak `--sbr-softness 2` i
`--speech --sbr-softness 4`.

#### `--sbr-invf <0..3>`

**Wymusza jeden ustalony poziom filtrowania odwrotnego na cały plik, całkowicie pomijając
estymator tonalności.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `0` wyłączone, `1` niski, `2` średni, `3` wysoki; domyślnie `-1` = automat,
  czyli estymator tonalności decyduje na pasmo i na ramkę
- Wpisywanie: liczba całkowita

Wyżej znaczy mocniejsze wybielanie odtworzonego pasma: mniej metalicznego dzwonienia, mniej
szczegółu, góra bardziej płaska i bardziej szumopodobna. Niżej znaczy, że odziedziczona
struktura tonalna zostaje na miejscu: więcej pozornego szczegółu, więcej metalicznego
charakteru.

Wada wymuszania jest dokładnie taka, jak brzmi: jeden poziom obowiązuje wszędzie, także w
tych pasmach i momentach, w których estymator wybrałby inaczej. Plik z cichym, tonalnym
wstępem i gęstym refrenem potrzebuje w tych miejscach różnych poziomów, a wymuszona wartość
daje im ten sam.

Mimo tego jest to najbardziej przydatne pokrętło w tej sekcji, do dwóch zadań. Pierwsze to
diagnoza: zakodować na `0` i na `3`, posłuchać skrajności i od razu wiadomo, którego końca
tej osi potrzebuje dany materiał. Drugie to materiał jednorodny, gdzie estymator i tak nie
ma do czego się dostrajać.

Wartości:
- `0` filtrowanie odwrotne wyłączone. Żadnego wybielania. Odtworzone pasmo zachowuje
  odziedziczoną strukturę: maksimum pozornego szczegółu, maksimum metalicznego dzwonienia.
  Na materiale tonalnym to wyraźny artefakt; na materiale szumopodobnym może być najczystszą
  opcją.
- `1` niski. Lekkie wybielanie.
- `2` średni. Umiarkowane wybielanie, z grubsza tam, gdzie estymator siedzi średnio przy
  tabeli muzycznej.
- `3` wysoki. Mocne wybielanie. Góra staje się płaska i szumopodobna; metaliczne dzwonienie
  znika, a razem z nim spora część odczuwanego szczegółu.
- `-1` automat. Decyduje estymator. Ustawienie zalecane poza diagnostyką.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf 3 -o out.m4a in.wav`

Współpraca i konflikty: ten przełącznik **wygrywa z każdym innym w tej sekcji**, w tym z
`--sbr-softness` i z pokrętłami min/max/bias, bo nie zostaje już żadna dynamiczna decyzja
do ograniczania. Gdy celem jest kształtowanie decyzji, a nie jej zastąpienie, trzeba
zostawić go wyłączonym. Zestawienie `--sbr-invf 3` z `--sbr-noise-max 6` to jedna z dwóch
kombinacji, które sprawiają, że `--sbr-nf-weight` w ogóle coś robi.

#### `--sbr-softness <0..7>`

**Jedno makro od góry twardej i metalicznej do bardzo miękkiej, ustawiające kilka reguł
estymatora naraz i zachowujące dynamiczną decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0..7`; domyślnie `-1` = wyłączone (fabrycznie odpowiada poziomowi `2` dla
  muzyki)
- Wpisywanie: liczba całkowita

To pokrętło warto wypróbować pierwsze. Przesuwa punkt pracy po osi metaliczne kontra syczące
bez zamrażania decyzji, ustawiając tabelę decyzyjną, przesunięcie, wagę szumu, granice
min/max i sufit szumu w kombinacjach, które zmierzyliśmy jako monotoniczne i stabilne.
Celowo nie rusza granic tonalności, bo zmierzyliśmy, że to powoduje migotanie decyzji w
obrębie pasma.

Wartości:
- `0` o jeden stopień mniej wybielania niż strojenie muzyczne: twardziej, bardziej
  metalicznie niż fabrycznie.
- `1` nigdy wybielanie HIGH. Na muzyce estymator i tak rzadko wybiera HIGH, więc na
  materiale muzycznym ta granica często wcale nie działa: zmierzone na HE-AAC
  32 kbps, poziomy `1` i `2` dały strumień identyczny z fabrycznym na sygnałach tonalnym,
  głosowym i mieszanym.
- `2` fabryczne estymatory muzyczne. Identyczny bajt w bajt z niekorzystaniem z
  przełącznika w ogóle.
- `3` co najmniej wybielanie LOW: estymator nie może już wybrać wyłączenia. Zmierzone jako
  równe `--sbr-invf 1` na sygnale tonalnym, bo tam estymator dążył do wyłączenia wszędzie, a
  dolna granica podniosła wszystko do LOW.
- `4` fabryczne estymatory mowy: tabela decyzyjna mowy i waga szumu, bez pozostałej części
  `--speech`, więc częstotliwość podziału zostaje nietknięta.
- `5` jak `4` plus podniesiony sufit szumu (0,5) i waga rozszerzona na pasma MID.
- `6` jak `5` plus jeszcze jeden stopień wybielania.
- `7` bardzo miękko: sufit 1,0, maksimum wybielania i szumu wypełniającego. Góra staje się
  jednoznacznie szumopodobna.
- `-1` wyłączone.

Zmierzone na HE-AAC 32 kbps, poziomy invf odczytane ze strumienia, płaskość zdekodowanego
pasma SBR od 7 do 14,5 kHz, gdzie 0 jest tonalne, a 1 szumopodobne:

| `--sbr-softness` | średni poziom invf (mowa / tonalny / muzyka) | płaskość pasma SBR (mowa / tonalny) |
|---|---|---|
| 0 | 0.15 / 0.00 / 0.00 | 0.366 / 0.396 |
| 2 (fabryczna muzyka) | 1.12 / 0.98 / 0.99 | 0.373 / 0.430 |
| 4 (fabryczna mowa) | 1.93 / 1.96 / 1.98 | 0.379 / 0.440 |
| 5 | 1.93 / 1.96 / 1.98 | 0.403 / 0.518 |
| 7 | 2.93 / 2.96 / 2.98 | 0.417 / 0.521 |

Skala jest monotoniczna na każdej próbce, a zmiany trybu w obrębie pasma zostają na poziomie
fabrycznym (0,004 do 0,04 na pasmo na ramkę). Zmierzone również: poziom `0` równa się
`--sbr-invf 0` na materiale tonalnym i mieszanym, a poziomy od `4` do `7` różnią się między
sobą.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-softness 6 -o out.m4a in.wav`

Współpraca i konflikty: **każde jawne pokrętło poniżej wygrywa z tym makrem**, i właśnie tak
się je łączy: `--sbr-softness 6 --sbr-invf-max 2` zachowuje obsługę szumu z makra przy
własnym sufice. `--sbr-invf` wygrywa z tym makrem całkowicie. Sąsiednie poziomy mogą dać ten
sam strumień, gdy materiał nigdy nie dociera do granicy, którą się różnią; to oczekiwane, a
nie awaria.

#### `--sbr-invf-min <0..3>`

**Ustawia dolną granicę dla estymatora: najniższy poziom filtrowania odwrotnego, jaki wolno
mu wybrać.**

- Dotyczy: profili z SBR
- Wartości: `0..3`; domyślnie `-1` = wyłączone (bez dolnej granicy, odpowiednik `0`)
- Wpisywanie: liczba całkowita

Estymator nadal decyduje na pasmo i na ramkę; to tylko usuwa dolną część jego zakresu. To
narzędzie na „góra jest metaliczna w cichych fragmentach”, gdy estymator poprawnie widzi
pasmo tonalne i wybiera mało wybielania albo wcale, a odsłuch temu przeczy.

Podniesienie dolnej granicy gwarantuje minimalną ilość wybielania wszędzie, co kosztuje
szczegół w tych pasmach, które naprawdę go nie potrzebowały.

Wartości:
- `0` dolna granica na wyłączeniu. Bez ograniczenia; identyczne z niekorzystaniem z
  przełącznika.
- `1` estymator nie może nigdy wybrać wyłączenia. Najdelikatniejszy użyteczny poziom i ten,
  który warto wypróbować, gdy metaliczne dzwonienie pojawia się tylko sporadycznie.
- `2` co najmniej wybielanie MID wszędzie. Mocna interwencja: pasma, które nie chciały
  żadnego, dostają teraz umiarkowane wybielanie.
- `3` co najmniej HIGH wszędzie, co oznacza, że poziom jest przypięty do HIGH, bo 3 to
  maksimum. W efekcie jest to odpowiednik `--sbr-invf 3`, więc przy takim celu lepiej
  zapisać to wprost.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 48000 --sbr-invf-min 1 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, razem z `--sbr-invf-max` jako
drugim końcem klamry. Ustawienie `--sbr-invf-min 3` albo `--sbr-invf-min` równego
`--sbr-invf-max` zwija dynamiczny zakres do jednej wartości, czyli jest to `--sbr-invf`
drogą okrężną. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.
`--sbr-softness 3` to zapakowana wersja `--sbr-invf-min 1`.

#### `--sbr-invf-max <0..3>`

**Ustawia sufit nad estymatorem: najwyższy poziom filtrowania odwrotnego, jaki wolno mu
wybrać.**

- Dotyczy: profili z SBR
- Wartości: `0..3`; domyślnie `-1` = wyłączone (bez sufitu, odpowiednik `3`)
- Wpisywanie: liczba całkowita

Odbicie `--sbr-invf-min` i częściej użyteczne z tych dwóch. To narzędzie na „góra robi się
sycząca i traci szczegół w głośnych fragmentach”, gdy estymator wybiera mocne wybielanie i
zabiera razem z nim blask.

Obniżenie sufitu zachowuje szczegół za cenę przepuszczenia pewnej dawki metalicznego
charakteru w pasmach, które naprawdę potrzebowały wybielania.

Wartości:
- `3` sufit na HIGH. Bez ograniczenia; identyczne z niekorzystaniem z przełącznika.
- `2` nigdy HIGH. Zwykły wybór: usuwa tylko najbardziej agresywne wybielanie i zachowuje
  resztę oceny estymatora.
- `1` nigdy powyżej LOW. Mocne ustawienie chroniące szczegół; na materiale tonalnym słychać
  metaliczny charakter.
- `0` sufit jest na wyłączeniu, więc nigdzie nie wolno wybielać. W efekcie odpowiednik
  `--sbr-invf 0`, który lepiej zapisać właśnie tak.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-max 2 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, w parze z `--sbr-invf-min`.
`--sbr-softness 1` to zapakowana wersja `--sbr-invf-max 2`, ze zmierzonym zastrzeżeniem z
tamtego miejsca: na muzyce estymator rzadko dociera do HIGH, więc ten sufit może nie robić
nic na materiale muzycznym i jednocześnie być dokładnie tym, czego trzeba na mowie. Wygrywa
z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-bias <-3..3>`

**Przesuwa każdą dynamiczną decyzję filtrowania odwrotnego w górę albo w dół o ustaloną
liczbę stopni.**

- Dotyczy: profili z SBR
- Wartości: `-3..3` stopnie; domyślnie `0` = wyłączone
- Wpisywanie: liczba całkowita

Tam gdzie min i max obcinają zakres, to pokrętło (przesunięcie progu, bias) go przesuwa.
Wzór decyzji estymatora na pasmo i na ramkę jest zachowany dokładnie, a cały wzór się
przesuwa. To czyni je najczystszym sposobem powiedzenia „estymator ma dobry pomysł, tylko
konsekwentnie jest za miękki” albo „konsekwentnie za twardy”. Wartości dodatnie oznaczają
więcej wybielania.

Ponieważ wynik jest potem obcinany do 0..3, duże przesunięcia spłaszczają wzór na tym końcu,
w który uderzą: przy `+3` niemal wszystko ląduje na HIGH i zróżnicowanie między pasmami,
które miało zostać, znika.

Przykłady, więcej wybielania:
- Łagodnie: `--sbr-invf-bias 1`. Każde pasmo o jeden stopień mocniej wybielone. Wzór
  pozostaje nietknięty, charakter jest łagodniejszy.
- Mocno: `--sbr-invf-bias 2`. Dwa stopnie. Pasma, które chciały wyłączenia, dostają teraz
  MID; metaliczny charakter w dużej mierze zniknął, a razem z nim część szczegółu.
- Przesadnie: `--sbr-invf-bias 3`. Wszystko, czego estymator i tak nie chciał na HIGH,
  zostaje tam zepchnięte przez obcięcie. To `--sbr-invf 3` okrężną drogą.

Przykłady, mniej wybielania:
- Łagodnie: `--sbr-invf-bias -1`. O jeden stopień mniej wszędzie. Więcej szczegółu, cień
  więcej metaliczności.
- Mocno: `--sbr-invf-bias -2`. Dwa stopnie mniej. Jakiekolwiek wybielanie zachowują tylko
  te pasma, które estymator chciał na HIGH.
- Przesadnie: `--sbr-invf-bias -3`. Niemal wszystko obcina się do wyłączenia, czyli jest to
  `--sbr-invf 0` drogą okrężną, z maksymalnym metalicznym dzwonieniem na materiale tonalnym.

Współpraca i konflikty: nakładane **przed** `--sbr-invf-min` i `--sbr-invf-max`, więc można
najpierw przesunąć, a potem obciąć: `--sbr-invf-bias 2 --sbr-invf-max 2` przesuwa wszystko w
górę, ale odmawia HIGH. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-table <0|1>`

**Wybiera, której tabeli decyzyjnej używa estymator tonalności: tabeli muzycznej czy tabeli
mowy.**

- Dotyczy: profili z SBR
- Wartości: `0` muzyka, `1` mowa; domyślnie `-1` = zgodnie z `--speech`
- Wpisywanie: liczba całkowita

Progi estymatora pochodzą z tabeli, a FDK dostarcza dwie: jedną strojoną pod muzykę, drugą
pod mowę. Tabela mowy wybiela znacznie chętniej, bo mowa powyżej częstotliwości podziału to
w większości szczelinowe i oddech, gdzie odziedziczona struktura tonalna jest czystym
artefaktem i nie ma tam szczegółu wartego ochrony.

Ten przełącznik wybiera tabelę i **tylko** tabelę. `--speech` zmienia również częstotliwość
podziału i inne elementy strojenia; gdy celem jest zachowanie estymatora mowy na materiale
muzycznym bez przestawiania układu częstotliwości, to jest przełącznik do tego. Taka
kombinacja jest naprawdę przydatna: audycja słowna, audiobooki, materiał podcastowy przy
niskim bitrate'ie, gdzie potrzebne jest zachowanie wybielania przy własnej częstotliwości
podziału.

Wartości:
- `0` tabela muzyczna. Zachowawcze wybielanie, chroni szczegół tonalny.
- `1` tabela mowy. Wybiela znacznie chętniej. Zmierzony średni poziom invf przy tabeli mowy
  jest blisko 2,0 wobec około 1,0 przy tabeli muzycznej na HE-AAC 32 kbps.
- `-1` wyłączone: zgodnie z `--speech`, czyli tabela muzyczna, chyba że podano `--speech`.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-invf-table 1 -o out.m4a speech.wav`

Współpraca i konflikty: `--sbr-softness 4` to zapakowana wersja tego plus waga szumu dla
mowy. `--speech` ustawia to niejawnie i przesuwa również częstotliwość podziału, i to jest
różnica warta zapamiętania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-hyst <x>`

**Ustawia histerezę granic decyzyjnych: jak bardzo musi zmienić się tonalność, zanim
estymator przełączy pasmo na inny poziom.**

- Dotyczy: profili z SBR
- Wartości: `0..2`, liczba dziesiętna, gdzie `1.0` = fabryczne; domyślnie `-1` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Pasmo, którego tonalność siedzi dokładnie na granicy decyzyjnej, przeskakiwałoby w
przeciwnym razie na inny poziom w każdej ramce, a przeskakiwanie poziomów wybielania z ramki
na ramkę jest słyszalne jako rodzaj trzepotania albo niestabilności na górze. Histereza
rozszerza granice w strefę martwą, więc pasmo trzyma bieżący poziom, dopóki tonalność nie
przesunie się znacząco za linię.

Obniżanie oznacza szybszą reakcję na prawdziwe zmiany i częstsze przełączanie. Podnoszenie
oznacza większą stabilność i wolniejsze podążanie za rzeczywistymi zmianami, więc fragment,
który w połowie staje się tonalny, trzyma dłużej poprzednie traktowanie. Powyżej 2
rozszerzone granice zachodziłyby na siebie, co nie ma sensu, więc jest odrzucane.

Dla odniesienia: fabryczne tempo przełączania w obrębie pasma to od 0,004 do 0,04 zmiany na
pasmo na ramkę, a konfiguracja zmierzona na 0,2 do 0,29 była wyraźnie niestabilna.

Przykłady, mniej histerezy (szybciej, więcej przełączania):
- Łagodnie: `--sbr-invf-hyst 0,8`. Nieco szybsze podążanie za zmianami.
- Mocno: `--sbr-invf-hyst 0,3`. Wyraźnie więcej przełączania; warto stosować, gdy estymator
  wydaje się ospały na materiale, który szybko zmienia charakter.
- Przesadnie: `--sbr-invf-hyst 0`. Zupełny brak strefy martwej. Pasma siedzące na granicy
  przeskakują w każdej ramce, czyli pojawia się dokładnie ta niestabilność, której histereza
  ma zapobiegać.

Przykłady, więcej histerezy (wolniej, stabilniej):
- Łagodnie: `--sbr-invf-hyst 1,2`. Odrobinę bardziej lepkie niż fabrycznie.
- Mocno: `--sbr-invf-hyst 1,5`. Poziomy trzymają się mimo umiarkowanych zmian tonalności.
  Dobra para dla `--sbr-invf-max 2`, gdy celem jest spokojna, przewidywalna góra.
- Przesadnie: `--sbr-invf-hyst 2`, czyli maksimum. Strefy martwe są tak szerokie, jak tylko
  mogą być; pasma zasadniczo trzymają ten poziom, na którym wylądowały na początku, co na
  materiale zmieniającym charakter jest po prostu złym poziomem przez większość czasu.

Współpraca i konflikty: `--sbr-invf-smooth` atakuje tę samą niestabilność z drugiej strony,
uśredniając po ramkach zamiast poszerzać granice; używanie obu w skrajnych ustawieniach jest
nadmiarowe. `--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` przesuwają te granice, które to
pokrętło poszerza. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-sbr-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma odtworzonego przez SBR, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Estymator mierzy tonalność po obu stronach porównania i każdą z nich zestawia z granicami.
To pokrętło przesuwa granice po stronie odtworzonej przez SBR. Wartości dodatnie oznaczają,
że odtworzone pasmo szybciej liczy się jako tonalne, a tonalne pasmo odtworzone jest właśnie
tym, po co jest wybielanie, więc dodatnie oznaczają więcej invf.

To instrument ostrzejszy niż przesunięcie progu czy min/max, i bardziej ryzykowny.
Przesuwanie granic zmienia to, *które* pasma je przekraczają i w których ramkach, więc może
wprowadzić migotanie decyzji: dla tego mechanizmu zmierzyliśmy od 0,2 do 0,29 zmiany na
pasmo na ramkę wobec 0,004 do 0,04 w ustawieniu fabrycznym, i to dlatego `--sbr-softness` z
niego nie korzysta. Przy sięganiu po to pokrętło warto jednocześnie podnieść
`--sbr-invf-hyst`, żeby odkupić stabilność.

Przykłady, wartości dodatnie (odtworzone pasmo szybciej tonalne, więcej invf):
- Łagodnie: `--sbr-invf-sbr-thr 2`. Niewielkie przesunięcie; kilka pasm więcej przechodzi
  do wybielania.
- Mocno: `--sbr-invf-sbr-thr 8`. Dużo więcej pasm liczy się jako tonalne. Słychać
  zauważalnie większe wybielanie i pewne migotanie.
- Przesadnie: `--sbr-invf-sbr-thr 20`, czyli maksimum. Zasadniczo każde pasmo odtworzone
  przez SBR liczy się jako tonalne, więc pomiar przestaje rozróżniać, a ocena estymatora
  zostaje zastąpiona stałą. `--sbr-invf 3` robi to samo bez migotania.

Przykłady, wartości ujemne (odtworzone pasmo tonalne później, mniej invf):
- Łagodnie: `--sbr-invf-sbr-thr -2`. Odrobinę mniej wybielanych pasm.
- Mocno: `--sbr-invf-sbr-thr -8`. Wybielane są tylko mocno tonalne pasma odtworzone; więcej
  szczegółu, więcej metalicznego charakteru.
- Przesadnie: `--sbr-invf-sbr-thr -20`, czyli minimum. Nic nie liczy się jako tonalne, więc
  nic nie jest wybielane. Ustawienie skrajne „zachowaj cały szczegół”, i tak właśnie brzmi.

Współpraca i konflikty: odbicie `--sbr-invf-orig-thr`, a obydwa składają się na porównanie
oryginału z pasmem odtworzonym przez SBR. `--sbr-invf-hyst` jest przeciwwagą po stronie
stabilności. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-orig-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma oryginalnego, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Ten sam mechanizm po stronie źródła, przy czym znak czyta się odwrotnie, i to jest rzecz,
którą trzeba dobrze zrozumieć. Estymator wybiela, gdy pasmo odtworzone wygląda na bardziej
tonalne, niż na to zasługuje oryginał. Zatem sprawienie, żeby *oryginał* liczył się jako
tonalny **później**, oznacza, że porównanie częściej znajduje większą niezgodność, a więc
wartości dodatnie także tutaj oznaczają więcej wybielania. Oba pokrętła progowe prowadzą do
tego samego skutku, mimo że opisują przeciwne strony.

Zastrzeżenie o migotaniu obowiązuje tu identycznie: to jest przesunięcie granicy, a
przesunięcia granic destabilizują decyzję w obrębie pasma mocniej niż przesunięcie progu czy
obcinanie.

Przykłady, wartości dodatnie (oryginał liczy się jako tonalny później, więcej invf):
- Łagodnie: `--sbr-invf-orig-thr 2`.
- Mocno: `--sbr-invf-orig-thr 8`. Oryginałowi rzadko przypisuje się tonalność, więc pasmo
  odtworzone częściej wygląda na błędne i zostaje wybielone.
- Przesadnie: `--sbr-invf-orig-thr 20`, czyli maksimum. Oryginał nigdy nie liczy się jako
  tonalny, więc każde odtworzone pasmo jest traktowane jak uszkodzenie. Maksymalne
  wybielanie drogą, która maksymalizuje też migotanie.

Przykłady, wartości ujemne (oryginał liczy się jako tonalny wcześniej, mniej invf):
- Łagodnie: `--sbr-invf-orig-thr -2`.
- Mocno: `--sbr-invf-orig-thr -8`. Oryginałowi hojnie przypisuje się tonalność, więc pasmo
  odtworzone częściej jest uznawane za akceptowalne i zostawiane w spokoju.
- Przesadnie: `--sbr-invf-orig-thr -20`, czyli minimum. Wszystko w oryginale liczy się jako
  tonalne, porównanie nie znajduje niezgodności, a wybielanie praktycznie ustaje.

Współpraca i konflikty: para dla `--sbr-invf-sbr-thr`; przesuwanie obu w tę samą stronę
częściowo się znosi, bo jedno podnosi niezgodność, a drugie ją obniża. Poziom szumu
odczytuje tę samą różnicę między oryginałem a pasmem odtworzonym przez `--sbr-nf-weight`,
więc przesuwanie tych granic zmienia także szum wypełniający, nie tylko wybielanie. Wygrywa
z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-smooth <0..2>`

**Ustawia, po ilu ramkach estymator tonalności uśrednia, zanim podejmie decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0`, `1`, `2`; domyślnie `-1` = wyłączone (fabrycznie uśrednianie po 2)
- Wpisywanie: liczba całkowita

Uśrednianie po ramkach to druga obrona przed niestabilną decyzją. Z uśrednianiem jedna
odbiegająca ramka nie przestawi poziomu wybielania pasma; bez niego każda ramka jest
oceniana sama. Wymiana jest typowa: stabilność za szybkość reakcji.

Wartości:
- `0` tylko bieżąca ramka. Najszybsza reakcja na prawdziwą zmianę charakteru, najwięcej
  przełączania z ramki na ramkę. Warto stosować na materiale, który zmienia się szybko,
  licząc się z pewną chwiejnością.
- `1` uśrednianie krótsze niż fabryczne. Opcja pośrednia.
- `2` ustawienie fabryczne. Równowaga wybrana przez FDK, i to dobra na większości materiału.
- `-1` wyłączone, czyli to samo co `2`.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-smooth 0 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: to pokrętło i `--sbr-invf-hyst` kupują stabilność, każde innym
sposobem, więc zestawienie niskiego wygładzania z wysoką histerezą to rozsądna droga do
szybkiej reakcji bez trzepotania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`,
któremu nie zostaje już żadna decyzja do wygładzania.

#### `--sbr-nf-weight <x>`

**Ustawia, jak mocno różnica tonalności między oryginałem a pasmem odtworzonym przez SBR
podnosi poziom szumu.**

- Dotyczy: profili z SBR, i tylko tam, gdzie sufit szumu zostawia zapas (patrz niżej)
- Wartości: `0.0..1.0`, liczba dziesiętna; domyślnie `-1` = zgodnie z `--speech`
  (muzyka 0,25, mowa 1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Wybielanie usuwa strukturę; szum wypełniający ją zastępuje. Ta waga rozstrzyga, jaka część
zmierzonej niezgodności między pasmem oryginału a pasmem odtworzonym przez SBR zamienia się
w poziom szumu. Wyżej oznacza więcej szumu wypełniającego wszędzie tam, gdzie odtworzone
pasmo jest zbyt tonalne, i to dlatego strojenie mowy używa 1,0: w mowie górne pasmo powinno
być szumem, więc wypełnianie go jest zawsze słuszne. Niżej oznacza, że różnicą zajmuje się
głównie samo wybielanie.

**Warto to przeczytać przed użyciem.** Ważony szum jest obcinany przez sufit szumu, a przy
sufice z tabeli strojenia (0,125 przy 24 do 48 kbps) około 95 procent pasm już na nim
siedzi, więc podnoszenie wagi nic nie zmienia, bo nie ma zapasu do wykorzystania.
Zmierzone, to pokrętło ruszyło strumień tylko przy `--sbr-noise-max 6` **plus**
`--sbr-invf 3` albo `--sbr-nf-weight-from 1`. Samo, albo tylko z jednym z tych dwóch, nie
zmieniło zupełnie nic. Potrzebuje podniesionego sufitu i pasm na wysokim poziomie invf, bo
domyślnie wagę dostają tylko pasma HIGH.

Działający przepis to zatem `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight <x>`,
albo pozostawienie podniesienia sufitu poziomom `--sbr-softness 5`, `6` czy `7`.

Przykłady, więcej szumu wypełniającego (przy podniesionym sufice):
- Łagodnie: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.4`. Trochę więcej
  powietrza tam, gdzie odtworzone pasmo jest zbyt tonalne.
- Mocno: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7`. Wyraźnie więcej
  szumu wypełniającego; góra czyta się jako bardziej powietrzna i mniej metaliczna, przy
  mniejszym pozornym szczególe.
- Przesadnie: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 1`, czyli maksimum,
  a więc wartość dla mowy nałożona na każde pasmo od LOW w górę przy szeroko otwartym
  sufice. Na muzyce to syk w miejscu, gdzie kiedyś były talerze.

Przykłady, mniej szumu wypełniającego:
- Łagodnie: `--sbr-nf-weight 0,15`. Poniżej domyślnej wartości dla muzyki; nieco bardziej
  sucha góra.
- Mocno: `--sbr-nf-weight 0,05`. Różnica tonalności nie wnosi prawie żadnego szumu;
  wybielanie musi odwalić robotę samo.
- Przesadnie: `--sbr-nf-weight 0`, czyli minimum. Różnica jest całkowicie ignorowana. Przy
  domyślnym sufice może to spokojnie dać strumień identyczny bitowo z fabrycznym, bo
  większość pasm i tak jest obcięta, co samo w sobie jest lekcją o tym pokrętle.

Współpraca i konflikty: potrzebuje `--sbr-noise-max 3` albo `6`, żeby mieć zapas, i
`--sbr-nf-weight-from`, żeby sięgnąć pasm poniżej HIGH. `--sbr-noise-floor-offset` podnosi
poziom szumu bezwarunkowo i jest bardziej topornym wyjściem awaryjnym, gdy to pokrętło
odmawia ruchu. Wygrywa z `--sbr-softness`; `--sbr-invf 3` go nie blokuje, tylko jest jedną z
dwóch rzeczy, które go odblokowują.

#### `--sbr-nf-weight-from <1..3>`

**Ustawia najniższy poziom filtrowania odwrotnego, którego pasma dostają wagę poziomu
szumu.**

- Dotyczy: profili z SBR
- Wartości: `1` LOW i wyżej, `2` MID i wyżej, `3` tylko HIGH; domyślnie `-1` = wyłączone
  (fabrycznie `3`)
- Wpisywanie: liczba całkowita

Waga różnicy tonalności nie jest nakładana wszędzie. Domyślnie dostają ją tylko pasma, które
dostały wybielanie HIGH, z rozumowania, że to w nich struktura została naprawdę zniszczona i
wymaga zastąpienia. Ten przełącznik poszerza ten zbiór w dół.

To czyni go przełącznikiem odblokowującym `--sbr-nf-weight`: na muzyce estymator rzadko
wybiera HIGH, więc przy ustawieniu fabrycznym waga nie ma prawie na czym działać, a pokrętło
wygląda na martwe. Po ustawieniu na `1` w grze jest każde wybielone pasmo.

Wartości:
- `3` tylko HIGH. Ustawienie fabryczne. Najbardziej zachowawcze i powód, dla którego
  `--sbr-nf-weight` tak często sprawia na muzyce wrażenie, że nic nie robi.
- `2` MID i HIGH. Umiarkowane poszerzenie, i to właśnie robi `--sbr-softness 5` i wyżej.
- `1` LOW, MID i HIGH. Wagę dostaje każde pasmo z jakimkolwiek wybielaniem. Najszerszy
  zasięg, i ten, przy którym zmierzyliśmy, że `--sbr-nf-weight` zmienia strumień, w parze z
  `--sbr-noise-max 6`.
- `-1` wyłączone, to samo co `3`.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Współpraca i konflikty: bez sensu, dopóki `--sbr-nf-weight` czegoś nie robi, a
`--sbr-nf-weight` jest bez sensu bez zapasu od `--sbr-noise-max`. Wszystkie trzy ruszają
razem. Wygrywa z `--sbr-softness`.

#### `--sbr-nf-smooth <0>`

**Wyłącza wygładzanie poziomu szumu w czasie.**

- Dotyczy: profili z SBR
- Wartości: `0` = bez wygładzania; domyślnie `-1` = wyłączone, co zostawia fabryczne
  wygładzanie po 4 ramkach
- Wpisywanie: liczba całkowita

Poziom szumu jest normalnie uśredniany po czterech ramkach, więc ilość szumu wypełniającego
w paśmie zmienia się stopniowo. To słuszne na materiale ustabilizowanym, gdzie skaczący
poziom szumu byłby słyszalny jako pompowanie albo oddychanie na górze. Jest niesłuszne tam,
gdzie materiał naprawdę zmienia się szybko: wygładzony poziom przychodzi z opóźnieniem, więc
nagle cichy fragment jeszcze przez kilka ramek niesie szum z fragmentu głośnego, a nagle
jasny fragment jest na starcie niedopełniony.

Ten przełącznik działa jednokierunkowo. Nie ma wartości, która zwiększa wygładzanie; `0` je
usuwa, a cokolwiek innego oznacza zostawienie go w spokoju.

Wartości:
- `0` bez wygładzania w czasie. Poziom szumu podąża dokładnie za oszacowaniem z ramki na
  ramkę. Szybciej, i na materiale ustabilizowanym słyszalnie mniej stabilnie.
- `-1` wyłączone: fabryczne wygładzanie po 4 ramkach. Właściwa wartość domyślna.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-nf-smooth 0 -o out.m4a transients.wav`

Współpraca i konflikty: szumowy odpowiednik `--sbr-invf-smooth 0`, i obowiązuje to samo
rozumowanie: warto stosować na materiale z szybkimi zmianami charakteru, licząc się z
mniejszą stabilnością na materiale utrzymującym się. Jego efekt jest ograniczony przez
`--sbr-noise-max`: gdy sufit obcina większość pasm, zostaje niewiele zróżnicowania, które
wygładzanie mogłoby wygładzić. Wygrywa z `--sbr-softness`.

#### `--sbr-noise-max <6|3|-3>`

**Ustawia sufit tego, jak głośny może być szum wstrzykiwany przez SBR: limit między
powietrzem a sykiem na górze.**

- Dotyczy: profili z SBR
- Wartości: `6` = 1,0, `3` = 0,5, `-3` = 0,125; domyślnie `-1` = wartość z tabeli strojenia
  dla bitrate'u
- Wpisywanie: liczba całkowita

To najbardziej doniosłe pokrętło szumu, nie dlatego, że ustawia poziom, ale dlatego, że
ustawia limit, w który uderza wszystko pozostałe. Te trzy wartości to trzy, które rozumie
estymator szumu w FDK, i odpowiadają sufitom amplitudy 1,0, 0,5 oraz 0,125. Tabela strojenia
wybiera 0,125 przy 24 do 48 kbps, a przy tym sufice około 95 procent pasm jest już
obciętych, i to dlatego szum wypełniający sprawia wrażenie ustalonego aż do jego
podniesienia.

Podniesienie sufitu samo w sobie nie dodaje szumu; pozwala estymatorowi i wadze dodać go
więcej tam, gdzie i tak już chciały. Obniżenie go przycina górę do czegoś bardziej suchego i
tonalnego, co na materiale rzadkim brzmi czyściej, a na talerzach i oddechu chudo.

Która z tych trzech wartości nic nie robi, zależy od obowiązującego wiersza tabeli
strojenia, więc lepiej mierzyć niż zakładać. Zmierzone: na sygnałach tonalnym i mieszanym
przy HE-AAC 32 kbps `-3` równa się ustawieniu fabrycznemu, a `6` równa się `3`; na sygnale
głosowym przy tym samym ustawieniu `6`, `3` i ustawienie fabryczne są identyczne.

Wartości:
- `-3` sufit 0,125. Najbardziej sucho i taki wybiera tabela przy niskim bitrate'ie.
  Bardziej tonalnie, bardziej metalicznie, mniej syku.
- `3` sufit 0,5. Czterokrotnie większy zapas. Szum wypełniający może faktycznie zmieniać
  się z pasma na pasmo.
- `6` sufit 1,0. Maksymalny zapas. W parze z `--sbr-nf-weight` i `--sbr-nf-weight-from 1` to
  konfiguracja, w której pokrętła poziomu szumu wreszcie mają pole do pracy; samo z siebie
  podnosi głównie te pasma, które estymator już i tak dopychał do sufitu.
- `-1` wyłączone: decyduje tabela strojenia dla bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Współpraca i konflikty: to jest warunek dopuszczenia (gate) przed `--sbr-nf-weight`,
`--sbr-nf-weight-from` i `--sbr-nf-smooth`, a poziomy od 5 do 7 w `--sbr-softness` podnoszą
go samoczynnie (5 i 6 do 0,5, 7 do 1,0). `--sbr-noise-bands` ustawia rozdzielczość
częstotliwościową poziomu szumu, który ten sufit przycina, więc gęsty poziom szumu przy
niskim sufice to zmarnowana informacja poboczna.

#### `--sbr-noise-floor-offset <n>`

**Przesuwa cały poziom szumu SBR, dodając albo odejmując szum wypełniający wszędzie naraz.**

- Dotyczy: profili z SBR
- Wartości: `0..11`; domyślnie `-128` = wyłączone
- Wpisywanie: liczba całkowita

Tam gdzie `--sbr-nf-weight` dodaje szum warunkowo, na podstawie zmierzonej niezgodności, to
pokrętło po prostu przesuwa poziom szumu. Więcej oznacza więcej szumu wypełniającego w
rekonstrukcji na całej długości, co czyta się jako więcej powietrza i więcej syku, i co
przytłacza tę odziedziczoną strukturę tonalną, która przetrwała wybielanie. Mniej oznacza
górę bardziej suchą i bardziej tonalną.

Ta toporność bywa dokładnie tym, co jest potrzebne. Gdy `--sbr-nf-weight` odmawia ruchu, bo
każde pasmo jest obcięte na sufice, to pokrętło nadal działa, bo działa na samym poziomie
szumu, a nie na pomiarze różnicy.

Akceptowany zakres to `0..11` i nic poza tym. Poza nim własna obsługa szumu w FDK trafia na
asercję i przerywa pracę, więc koder odrzuca wartości spoza zakresu czystym błędem, zamiast
do tego dopuścić.

Przykłady, więcej szumu wypełniającego:
- Łagodnie: `--sbr-noise-floor-offset 3`. Odrobinę więcej powietrza na górze.
- Mocno: `--sbr-noise-floor-offset 7`. Wyraźnie głośniejsza szumowo góra; metaliczny
  charakter zostaje zasypany, a razem z nim drobny szczegół.
- Przesadnie: `--sbr-noise-floor-offset 11`, czyli maksimum. W górnym paśmie dominuje szum
  wypełniający. Talerze zamieniają się w kształtowany syk, a wynik jest jednoznacznym
  ustawieniem skrajnym, co czyni go przydatnym punktem odniesienia dla oceny, jak dużą część
  góry stanowi szum.

Przykłady, mniej szumu wypełniającego:
- Łagodnie: `--sbr-noise-floor-offset 2`. Odrobinę bardziej sucho.
- Mocno: `--sbr-noise-floor-offset 1`. Minimalne wypełnienie; górę niosą własna struktura
  pasma odtworzonego przez SBR i to, co zostawia po sobie wybielanie.
- Przesadnie: `--sbr-noise-floor-offset 0`, czyli minimum. Żadnego wkładu z przesunięcia.
  Na materiale tonalnym słychać metaliczne dzwonienie, które szum wypełniający ma maskować,
  co źle łączy się z niskimi ustawieniami `--sbr-invf` i co właśnie dlatego warto raz
  usłyszeć.

Współpraca i konflikty: nadal podlega `--sbr-noise-max`, więc wysokie przesunięcie wobec
sufitu 0,125 zostaje obcięte. Działa w tym samym kierunku co `--sbr-nf-weight`, ale
bezwarunkowo, co czyni je właściwym wyborem przy zmianie globalnej, a niewłaściwym, gdy o
miejscu dla szumu ma decydować koder. Wobec niskich poziomów
`--sbr-invf` walczy z innym artefaktem przy użyciu złego narzędzia: metaliczne dzwonienie od
niewybielonej struktury lepiej naprawić wybielaniem niż maskować szumem.

### Pasmo rdzenia: `--core-cutoff`

#### `--core-cutoff <hz>`

**Wymusza górną granicę pasma kodowanego przez rdzeń AAC, w Hz. Bez SBR robi dokładnie to
samo co `-w`; pod SBR może tylko zwęzić rdzeń, a to już eksperyment.**

- Dotyczy: każdego profilu
- Wartości: `0..48000` Hz (spoza zakresu jest odrzucane); koder zatrzymuje się na
  częstotliwości Nyquista rdzenia (połowa częstotliwości próbkowania rdzenia); domyślnie
  `0` = decyzja FDK
- Wpisywanie: liczba całkowita

Bez SBR (`-p 2`, `-p 23`, `-p 39` bez `-L 1`, DAB+ LC) ten przełącznik i `-w` to jedno i to
samo: w AAC-LC CBR i VBR, AAC-LD, AAC-ELD, DAB+ i na materiale 96 kHz z `--uncap-bandwidth`
`-w N` i `--core-cutoff N` dały strumienie identyczne co do bitu. Można używać tego, który
lepiej czyta się w skrypcie. Typowy przypadek to pełne pasmo przy 96 kHz:
`--uncap-bandwidth --core-cutoff 40000` albo `--uncap-bandwidth -w 40000`.

Pod SBR jest inaczej i dlatego ta karta stoi na końcu grupy. Przy inicjalizacji SBR tabela
strojenia ustawia pasmo rdzenia tak, żeby rdzeń kończył się tam, gdzie zaczyna się SBR, a
wartość z `-w` jest odrzucana. `--core-cutoff` jest stosowany później i zostaje, ale
użyteczny jest tylko w jedną stronę:

- **Podniesienie powyżej startu SBR nie daje nic słyszalnego.** Dekoder odtwarza wszystko
  powyżej startu SBR z własnych danych SBR i nadpisuje to, co wysłał tam rdzeń. Pomiar przy
  HE-AAC 64 kbps stereo, wejście 48 kHz: z `--core-cutoff 12000` sygnał po dekodowaniu
  powyżej 10,5 kHz ma koherencję z oryginałem 0,03, tyle samo co bez przełącznika, zarówno
  w dekoderze FDK, jak i w ffmpeg. Rdzeń i tak płaci za te prążki, co widać niżej: koherencja
  w paśmie 7..9,5 kHz spada z 0,95 do 0,92.
- **Obniżenie poniżej startu SBR otwiera dziurę.** Między nową górną granicą rdzenia a
  startem SBR nic nie jest kodowane, więc tego pasma brakuje, zamiast łagodnie opadać. Przy
  tych samych ustawieniach `--core-cutoff 9000` zostawia pasmo 9..10 kHz na poziomie od -15
  do -33 dB.

Szerszy rdzeń pod SBR daje podniesienie startu SBR przez `--sbr-start`, węższy jego
obniżenie. Rdzeń sam idzie za startem i oba pasma pozostają złączone. Przy HE-AAC 64 kbps
`--sbr-start 15` poszerza rdzeń z 10,5 do 12 kHz, z koherencją 0,93 aż do 11,5 kHz. Dla
`--core-cutoff` pod SBR zostaje świadomy eksperyment: posłuchanie, jak brzmi szczelina
między rdzeniem a SBR.

Granica odcięcia jest wyrównywana do granicy pasma SFB, więc `--verbose` podaje wartość
rzeczywistą: prośba o 17300 Hz i odpowiedź 17915 Hz to nie błąd. Górny limit to połowa
częstotliwości próbkowania rdzenia (12 kHz dla wejścia 48 kHz przy SBR w trybie dual rate).
Bez SBR obowiązuje też limit 20 kHz, który zdejmuje `--uncap-bandwidth` (grupa A).

Przykłady, bez SBR:
- Łagodnie: `-p 2 -b 128000 --core-cutoff 16000`. Ten sam strumień co z `-w 16000`.
- Mocno: `-p 23 -b 64000 --core-cutoff 12000 -f 10`. AAC-LD sam wybiera bardzo wąski rdzeń
  (około 5,8 kHz przy 64 kbps); ten przełącznik go poszerza, tak jak zrobiłoby `-w 12000`.
- Przesadnie: `-p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000` na wejściu 96 kHz.
  Dozwolone; przy niższym bitrate'ie to samo żądanie przepala budżet na niesłyszalną górę.

Przykłady, pod SBR (HE-AAC 64 kbps stereo, wejście 48 kHz, rdzeń 10,5 kHz):
- Podniesienie: `--core-cutoff 11500` albo `12000`. Bity wydane na prążki, które dekoder
  nadpisuje; powyżej 10,5 kHz nie słychać nic nowego.
- Obniżenie: `--core-cutoff 9000`. Opisana wyżej dziura 9..10 kHz.
- Obniżenie, przesadnie: `--core-cutoff 2000`. Brakuje wszystkiego od 2 kHz do startu SBR;
  przydaje się tylko po to, żeby usłyszeć, co niesie sam rdzeń.

Współpraca i konflikty: pod SBR o rdzeniu decyduje `--sbr-start`, a `--verbose` podaje
źródło pasma rdzenia: `[from --core-cutoff]` albo `auto`. `--uncap-bandwidth` jest potrzebny
tylko po to, żeby bez SBR wyjść powyżej 20 kHz.

## D. Maskowanie, kształtowanie szumu i szczegół

To grupa, która decyduje o tym, ile szumu koder zgadza się zostawić w każdym paśmie i gdzie
zamiast kodować sygnał po prostu go fabrykuje. Całą grupę niosą cztery pomysły, więc
wyjaśniamy je raz tutaj, a karty będą się do nich odwoływać.

**Próg maskowania.** Dla każdego pasma współczynnika skali (scale factor band, SFB),
w każdej ramce, model psychoakustyczny wylicza poziom, poniżej którego dodany szum
powinien być niesłyszalny, bo głośniejsza treść w tym paśmie i wokół niego go przykrywa.
Ten poziom to próg maskowania. Kwantyzator wydaje potem dokładnie tyle bitów, ile trzeba,
żeby zepchnąć szum kwantyzacji do progu, i ani jednego więcej. Po globalnym podniesieniu
progów koder uznaje, że więcej szumu jest dopuszczalne, więc wydaje mniej bitów, a wynik
robi się grubszy przy tym samym bitrate. Po ich obniżeniu koder żąda większej precyzji
wszędzie i bity kończą się szybciej, co przy CBR oznacza, że coś innego musi ustąpić.
`--ath-scale` to globalna skala tego progu. `--spread-mask` zmienia, jak daleko
maskowanie rozlewa się na sąsiednie pasma, a to już inne pytanie: nie jak głośna jest
maska, lecz jak szeroka.

**Wymagany SNR.** Niezależnie od progu FDK trzyma wymagany stosunek sygnału do szumu dla
każdego pasma, czyli minimalną jakość, jaką koder musi w tym paśmie dostarczyć bez względu
na to, co mówi próg. To najbliższy odpowiednik stałych tone masking noise i noise masking
tone, które udostępniają MusePack i LAME. Znaczy więcej, niż się wydaje, bo logika
unikania dziur przyciąga progi z powrotem do tego żądania: jeśli pętla dopasowania
bitrate'u chce „zagłodzić” pasmo, czyli zabrać mu bity tak, że koder je zeruje albo koduje
najgrubszym krokiem, żądanie SNR jest linią, której nie wolno jej przekroczyć.
`--minsnr-scale` skaluje to wymaganie, a `--minsnr-clamp-lo` i `--minsnr-clamp-hi`
przesuwają najwyższy i najniższy koniec okna, w którym wymaganie jest trzymane.

**TNS, czyli kształtowanie szumu w czasie.** Wewnątrz jednego bloku szum kwantyzacji
rozkłada się równo na cały blok w czasie. Dla transjentu to zły rozkład: blok z uderzeniem
perkusji ma szum także w cichym fragmencie przed uderzeniem, gdzie nic go nie maskuje, co
słychać jako pre-echo. TNS stosuje filtr predykcyjny w poprzek częstotliwości, co w
efekcie kształtuje obwiednię szumu w czasie tak, by szła za obwiednią sygnału i chowała
się pod atakiem. `--tns-mask` wybiera, które z filtrów mogą działać, a `--tns-order`, jak
długie mogą być. Dla LD i ELD to główna obrona przed pre-echem, bo te profile nie mają
bloków krótkich wcale.

**PNS, czyli percepcyjne podstawianie szumu.** Kiedy pasmo jest po prostu szumem,
kodowanie jego dokładnych linii widmowych jest marnotrawstwem: nikt na tym poziomie nie
odróżni jednej realizacji szumu od drugiej. PNS wyrzuca linie i zapisuje jedną liczbę,
energię pasma, plus flagę mówiącą "tutaj szum". Dekoder odtwarza losowy szum wyskalowany
do tej energii. Na materiale szumowym oszczędność jest duża. Ryzyko polega na tym, że
podstawione zostanie pasmo, które szumem naprawdę nie jest, i wtedy prawdziwy ton zamienia
się w syk. PNS steruje osiem przełączników z tej grupy: czy w ogóle działa, od jakiej
częstotliwości, czy obowiązuje warunek dopuszczenia (gate) przy niskim bitrate, jak głośny
jest fabrykowany szum, oraz trzy progi detekcji i minimalna szerokość pasma, które razem
decydują, które pasma się kwalifikują.

Jedna konwencja przed kartami: wszystkie pokrętła skalujące `--pns-*` przyjmują zwykły
mnożnik dziesiętny na fabryczną wartość FDK. `1.0` oznacza "zostaw wartość fabryczną",
`1.5` oznacza półtora raza tyle, `0.5` oznacza połowę. Wyjątkiem jest `--pns-min-width`,
które jest surową liczbą.

### `--tns-mask <n>`

**Wybiera, których filtrów kształtowania szumu w czasie koder może używać.**

- Dotyczy: każdego profilu z rdzeniem AAC. Najważniejszy w LD i ELD, gdzie TNS jest jedyną
  obroną przed pre-echem, oraz na materiale bogatym w transjenty w LC.
- Wartości: `0..15`, maska czterobitowa; domyślnie: `-1`, fabryczne `0xF`, wszystkie filtry
  dozwolone
- Wpisywanie: liczba całkowita

TNS kształtuje szum kwantyzacji w czasie, żeby chował się pod atakiem, a nie siedział w
ciszy przed nim. FDK ma więcej niż jeden filtr TNS, różniące się typem bloku i obszarem
częstotliwości, który obejmują, a maska mówi, które z nich są włączone. Fabrycznie jest to
`0xF`, czyli 15, czyli wszystkie cztery bity ustawione.

To raczej przełącznik diagnostyczny i laboratoryjny niż pokrętło jakości. Użyteczne
ustawienia to `0`, które wyłącza TNS zupełnie i pozwala usłyszeć, co robił, oraz `15`, czyli
wartość domyślna. Maski pośrednie pozwalają wyizolować jeden filtr, co bywa przydatne przy
przypisywaniu artefaktu do źródła, a poza tym rzadko.

Wyłączenie TNS ma drugą konsekwencję, o której warto wiedzieć: FDK wymusza wyłączenie PNS,
gdy TNS jest wyłączony. Zatem `--tns-mask 0` po cichu zabiera ze sobą percepcyjne
podstawianie szumu.

Wartości:
- `-1`: ustawienie fabryczne, wszystkie filtry włączone (`0xF`).
- `0`: TNS całkowicie wyłączony. Pre-echo na ostrych atakach robi się zauważalnie gorsze,
  szczególnie w LD i ELD; PNS też zostaje wymuszenie wyłączony.
- `1` do `14`: poszczególne filtry włączane bitami. Do izolowania wkładu jednego filtru.
- `15`: wszystkie filtry włączone, tak jak domyślnie.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --tns-mask 0 --verbose -o no_tns.m4a attacks.wav

Współpraca i konflikty: `--tns-order` ogranicza, jak długie mogą być włączone filtry, więc
te dwa czyta się razem. `--tns-thresh` z grupy E decyduje, jak często TNS w ogóle się
włącza, i zwykle to on jest właściwym pokrętłem zamiast maski. Ustawienie `0` tutaj wymusza
wyłączenie PNS bez względu na `--pns`. `--verbose` wypisuje efektywną maskę TNS jako maskę
bitową w zapisie szesnastkowym.

### `--tns-order <n>`

**Ogranicza długość filtru kształtowania szumu w czasie, wymieniając precyzję, z jaką
obwiednia szumu może iść za sygnałem, na informację poboczną, która to kosztuje.**

- Dotyczy: każdego profilu z rdzeniem AAC. Bloki krótkie są dodatkowo ograniczone przez
  standard do rzędu 5, więc wartości powyżej tej liczby dotyczą tylko bloków długich.
- Wartości: `1..12`; domyślnie: `-1`, własny rząd FDK
- Wpisywanie: liczba całkowita

Filtr TNS to predyktor liniowy działający w poprzek częstotliwości. Jego rząd to liczba
współczynników, a rząd decyduje, jak szczegółową obwiednię czasową może narzucić szumowi.
Niski rząd potrafi wytworzyć tylko zgrubny kształt, z grubsza "więcej szumu później, mniej
wcześniej". Wysoki rząd potrafi iść za skomplikowaną obwiednią, kosztem wysyłania większej
liczby współczynników filtru na ramkę jako informacja poboczna.

Niższe wartości robią TNS tańszym i tępszym. Przy niskim bitrate ta wymiana może być tego
warta, bo współczynniki konkurują z widmem o te same bity. Wyższe wartości pozwalają
kształtowaniu śledzić ataki precyzyjniej, i to jest kierunek, gdy pre-echo jest problemem, a
bity są dostępne, zwłaszcza w LD i ELD, gdzie nie istnieją bloki krótkie, które by pomogły.

Przykłady:
- Łagodnie, dłuższy filtr: `--tns-order 8`. Nieco dokładniejsze kształtowanie w czasie w
  blokach długich, mały koszt informacji pobocznej.
- Mocno, dłuższy filtr: `--tns-order 12`. Maksimum. Najlepsza rozdzielczość czasowa, jaką
  TNS może dać; warto spróbować w LD i ELD na materiale perkusyjnym.
- Przesadnie, dłuższy filtr: `--tns-order 12` jest już sufitem, więc ustawieniem skrajnym
  jest tutaj połączenie go z `--tns-thresh 0,5` z grupy E, żeby długi filtr działał też na
  prawie każdej ramce. Informacja poboczna zjada wtedy widoczną część niskiego budżetu
  bitowego, a w zamian pojawiają się dziury.
- Łagodnie, krótszy filtr: `--tns-order 5`. Limit bloku krótkiego zastosowany wszędzie.
  Taniej, z nieco grubszym kształtowaniem w blokach długich.
- Mocno, krótszy filtr: `--tns-order 3`. Tylko zgrubne kształtowanie obwiedni. Tanio;
  większość korzyści przeciw pre-echu zniknęła, ale grube kształtowanie zostaje.
- Przesadnie, krótszy filtr: `--tns-order 1`. Jeden współczynnik, który o obwiedni nie
  potrafi wyrazić prawie nic. TNS jest formalnie włączony i praktycznie bezużyteczny, co jest
  dobrym sposobem potwierdzenia, ile korzyści pochodzi z długości filtru, a nie z samego
  włączenia TNS.

Współpraca i konflikty: `--tns-mask` decyduje, które filtry istnieją, a to, jak długie są;
`0` na masce czyni ten przełącznik bez znaczenia. `--tns-thresh` z grupy E decyduje, jak
często działają. W LC bloki krótkie i tak załatwiają większość transjentów, więc rząd ma tam
mniejsze znaczenie niż w LD i ELD.

### `--pns <n>`

**Włącza lub wyłącza percepcyjne podstawianie szumu, nadpisując własną decyzję kodera.**

- Dotyczy: rdzeni AAC-LC, AAC-LD i AAC-ELD przy CBR. FDK wymusza wyłączenie PNS, gdy tryb
  bitrate'u to VBR i gdy TNS jest wyłączony, a te reguły stoją nad tym przełącznikiem.
- Wartości: `0` wyłączone, `1` włączone; domyślnie: `-1`, decyduje koder
- Wpisywanie: liczba całkowita, włącz/wyłącz

PNS zastępuje pasmo uznane za szum jedną wartością energii i flagą, a dekoder odtwarza szum
o tej energii. Na materiale szumowym oszczędność jest duża, a na materiale tonalnym ryzyko
polega na tym, że prawdziwy ton zamieni się w syk.

Własna decyzja kodera pochodzi z jego tabeli strojenia i zależy głównie od bitrate'u. Przy
64 kbps PNS jest normalnie już włączony, więc `1` to tam to samo co ustawienie fabryczne
i tylko `0` coś zmienia. Poniżej około 28 kbps tabela wyłącza PNS kompletnie, a w tym punkcie
ani `1` tutaj, ani `--pns-start` nie robią nic, bo PNS jest wyłączony wyżej w łańcuchu;
`--force-pns` to przełącznik, który ten warunek dopuszczenia (gate) obchodzi.

Szczere zastosowanie tej karty to diagnoza i świadomy wybór: `0`, żeby usłyszeć, co PNS
wnosił, `1`, żeby go narzucić tam, gdzie tabela wydaje się zachowawcza. `--verbose`
raportuje wynikowy stan PNS, co jest najszybszym sposobem sprawdzenia, czy żądanie przeszło
przez warunki dopuszczenia.

Wartości:
- `-1`: decyzja należy do FDK (ustawienie fabryczne, domyślne).
- `0`: PNS wyłączony. Na materiale szumowym pasma szumowe zostają porządnie zakodowane,
  a bity biorą się skądinąd.
- `1`: PNS włączony tam, gdzie pozwalają warunki dopuszczenia. Przy 64 kbps i wyżej zwykle
  pokrywa się to z zachowaniem fabrycznym; przy niskim bitrate wymaga `--force-pns`, żeby
  mieć jakikolwiek efekt.

Przykład w kontekście:

    fdkaac -p 2 -b 64000 --pns 0 --verbose -o no_pns.m4a in.wav

Współpraca i konflikty: `--force-pns` usuwa warunek niskiego bitrate'u. `--tns-mask 0`
wymusza wyłączenie PNS niezależnie od wartości ustawionej tutaj, bo FDK wymaga TNS dla PNS. Tryb
VBR (`-m 1..5`) robi to samo. `--pns-start`, `--pns-tonality`, `--pns-refpower`,
`--pns-gapfill` i `--pns-min-width` kształtują PNS, gdy już działa, a nie robią nic, gdy nie
działa.

Zmierzone: przy AAC-LC 64 kbps `--pns 1` daje strumień fabryczny, bo ustawienie domyślne ma
tam PNS już włączony, natomiast `--pns 0` go zmienia.

### `--pns-start <hz>`

**Ustawia najniższą częstotliwość, na której podstawianie szumu wolno działać.**

- Dotyczy: tych samych miejsc co `--pns`, i tylko wtedy, gdy PNS faktycznie działa.
- Wartości: częstotliwość w Hz; domyślnie: `-1`, własna częstotliwość startowa FDK
- Wpisywanie: liczba całkowita

PNS jest bezpieczny wysoko i groźny nisko. Niskie pasma noszą tony podstawowe, które
definiują wysokość i barwę, a podstawienie tam szumu jest natychmiast słyszalne. FDK ma więc
częstotliwość startową, poniżej której PNS nigdy nie jest używany, i zależy ona od bitrate'u
oraz częstotliwości próbkowania.

Przy niższym starcie kwalifikuje się więcej widma, więc więcej pasm dostaje kodowanie
szumowe i zwalnia się więcej bitów, przy rosnącym ryzyku podstawienia czegoś, co szumem nie
było. Przy wyższym PNS wycofuje się w górę, co jest bezpieczniejsze i tańsze w
oszczędnościach.

Jedna praktyczna pułapka: wartość jest wyrównywana do granicy pasma SFB, więc zmiana o
kilkaset Hz może wylądować w tym samym paśmie i dać identyczny strumień. Przy sondowaniu
warto przesuwać ją w krokach kilohercowych albo większych.

Przykłady:
- Łagodnie, niższy start: `--pns-start 3000`. PNS sięga trochę głębiej. Nieco więcej pasm
  kodowanych szumowo na materiale szerokopasmowym.
- Mocno, niższy start: `--pns-start 2000`. Wyraźnie więcej widma się kwalifikuje. Realna
  oszczędność na materiale szumowym; przy czymkolwiek z tonalnością w górnym średnim paśmie
  warto nasłuchiwać, czy ton nie robi się ziarnisty.
- Przesadnie, niższy start: `--pns-start 500`. PNS dozwolony prawie wszędzie. Dźwięki
  trzymane tracą swoją tożsamość tam, gdzie detektor się myli, a dokładnie temu artefaktowi
  częstotliwość startowa ma zapobiegać.
- Łagodnie, wyższy start: `--pns-start 6000`. PNS popchnięty trochę w górę, mała strata
  oszczędności i mały zysk bezpieczeństwa.
- Mocno, wyższy start: `--pns-start 12000`. Podstawiana może być tylko góra widma.
  Bezpiecznie, z większością korzyści utraconą.
- Przesadnie, wyższy start: `--pns-start 20000`. W większości konfiguracji powyżej
  kodowanego pasma, więc PNS nie ma już gdzie działać i jest to w praktyce `--pns 0` inną
  drogą.

Współpraca i konflikty: nie robi nic, gdy PNS jest wyłączony, co przy niskim bitrate
oznacza, że najpierw trzeba podać `--force-pns`. `--pns-min-width` ogranicza PNS przez
szerokość pasma, a nie przez częstotliwość, a skoro pasma rosną wraz z częstotliwością, oba
ograniczenia się nakładają. `--verbose` pokazuje wynikowy stan PNS.

Zmierzone: przy AAC-LC 64 kbps i 48 kHz fabryczny start to 4100 Hz. `4000` trafia do tego
samego pasma SFB i daje identyczny strumień; `2000` i `12000` oba go zmieniają.

### `--force-pns`

**Obchodzi warunek niskiego bitrate'u, który inaczej wyłącza podstawianie szumu całkowicie
poniżej około 28 kbps.**

- Dotyczy: AAC-LC, AAC-LD i AAC-ELD przy CBR, przy bitrate'ach poniżej tego warunku. Powyżej
  niego nie zmienia nic, bo PNS jest już dozwolony.
- Wartości: flaga; domyślnie: wyłączone
- Wpisywanie: flaga

Tabela strojenia FDK ma wiersz bitrate'u poniżej około 28 kbps, w którym każdy wpis PNS jest
zerem, dla każdej częstotliwości próbkowania. Konsekwencja zaskakuje ludzi: przy 24 kbps,
dokładnie tam, gdzie tanie kodowanie szumowe pomogłoby najbardziej, PNS jest całkowicie
wyłączony, a zarówno `--pns 1`, jak i `--pns-start` są ignorowane. Słyszalny skutek to
artefakt w stylu MDCT na treści szumowej, kojarzony raczej z MP3.

Ta flaga każe koderowi użyć pierwszego aktywnego wiersza tabeli zamiast wiersza zerowego,
więc PNS działa też przy 24 kbps i niżej. Reszta wymagań FDK nadal obowiązuje: PNS
potrzebuje włączonego TNS i trybu innego niż VBR, i nic w tej kompilacji tego nie zmienia.

Wartości:
- pominięte: warunek obowiązuje, PNS wyłączony poniżej około 28 kbps (zachowanie fabryczne).
- `--force-pns`: warunek obchodzony, PNS dostępny przy niskim bitrate.

Przykład w kontekście:

    fdkaac -p 2 -b 24000 --pns 1 --force-pns --pns-start 4000 -o out.m4a in.wav

Współpraca i konflikty: zwykle podaje się go razem z `--pns 1` i często z `--pns-start`, bo
sensem wymuszania PNS jest potem nim sterować. Bez sensu z `--tns-mask 0` albo przy VBR,
które i tak wymuszają wyłączenie PNS. Przy bitrate'ach powyżej tego warunku nie robi nic.

### `--pns-shared <0|1>`

**W pasmach, które oba kanały kodują jako szum, zapisuje jeden wspólny szum albo dwa niezależne, wszędzie tak samo.**

- Dotyczy: rdzeń AAC-LC w stereo z włączonym PNS (`--pns 1`, `--force-pns` albo `--pns-above`). Domyślnie wyłączone.
- Wartości: `1` = jeden wspólny szum (ten sam w L i R), `0` = niezależny szum w każdym kanale

PNS zastępuje pasmo szumem o właściwym poziomie. W parze kanałów format pozwala zaznaczyć,
pasmo po paśmie, że dekoder ma użyć tego samego szumu w obu kanałach. Ta flaga jedzie na
masce MS. Fabryczny FDK rozstrzyga ją dla każdego pasma osobno, mierząc korelację, więc
maskę zwykle trzeba zapisać pasmo po paśmie: przy niskim bitrate do około 25 bitów na ramkę.
Jednolity wybór pozwala masce wrócić do 2-bitowej postaci dla wszystkich pasm, jeśli reszta
ramki na to pozwala. Poziom szumu nadal jest zapisywany dla każdego kanału: format nie ma
wspólnego poziomu.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Typowo: `--pns-shared 1`. Maska MS z 28 do 2 bitów na ramkę na materiale skorelowanym.
- Szerokie stereo: `--pns-shared 0`. Niezależny szum zachowuje szerokość, maska zostaje tak
  droga jak przy ustawieniu fabrycznym.

Gdy maska i tak zostaje pasmo po paśmie (pasma pod obszarem PNS kodowane jako L/R na
szerokim materiale), przełącznik oszczędza niewiele: maska jest tania tylko wtedy, gdy
zgadzają się wszystkie pasma. ffmpeg 6.1.1 gra wspólny szum jako niezależny; dekoder FDK
jako wspólny.

### `--pns-hold <dB>`

**Pozwól sąsiednim pasmom szumu dzielić jeden poziom, żeby obwiednia szumu kosztowała mniej bitów.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `0.1..12.0` dB

Każde pasmo PNS zapisuje swój poziom szumu jako różnicę względem poprzedniego: 1 bit, gdy
jest taki sam, 3 do 9 bitów w innym wypadku. Z tym przełącznikiem pasmo, którego poziom
mieści się w podanej tolerancji względem poprzedniego pasma szumu, przejmuje jego poziom.
Obwiednia szumu w częstotliwości robi się grubsza, w krokach około 1,5 dB.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Łagodnie: `--pns-hold 3`. Poziomy szumu z 94 do 87 bitów na ramkę.
- Mocno: `--pns-hold 6`. 70 bitów na ramkę; uwolnione bity idą do kodowanego widma.
- Przesadnie: `--pns-hold 12`. Szum jest prawie płaski w całym swoim obszarze.

### `--pns-short <0|1>`

**Stosuj `--pns-above` także w ramkach krótkich (transjentach), tam gdzie to się opłaca.**

- Dotyczy: rdzeń AAC-LC z `--pns-above`. Domyślnie wyłączone.
- Wartości: `1` = włączone, `0` = fabryczne

Format AAC pozwala na podstawianie szumu w oknach krótkich; FDK nigdy tego nie robi, więc
powyżej `--pns-above` ramka transjentowa koduje pasma szumowe widmem. Z tym przełącznikiem
one też stają się szumem, ale tylko w ramce zakodowanej jako jedna grupa wszystkich 8 okien
(`--short-groups 1`). Zmierzone (1.4.0-beta.2) w mniejszych grupach: energie szumu zajmowały
19% ramki zamiast 2%, a atak wychodził o 2,6 do 6 dB ciszej, więc te grupy zostają przy
widmie. Przy grupowaniu fabrycznym przełącznik nie zmienia więc niczego (bit w bit).

Zmierzone z `--short-groups 1 --pns-above 4000`, próbka z atakiem w każdej ramce: plik przy
24 kbps z 9810 do 9226 B (-6%), pasmo szumu na poziomie źródła (+1,1 dB zamiast -1,4 dB).
Koszt: jeden poziom szumu obejmuje 8 okien, więc pasmo rozmazuje się na całą ramkę: pre-echo
+1 dB, atak o około 2 dB ciszej. Na muzyce z nielicznymi ramkami krótkimi nie robi prawie
nic (-0,3% pliku). Dla niskich bitrate'ów, przy których pasma inaczej by brakowało; najpierw
odsłuch.

### `--pns-budget <0|1>`

**Licz bity, które zajmują poziomy szumu PNS, żeby ramce nie zabrakło bitów i nie obcięła widma.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `1` = włączone, `0` = fabryczne

FDK planując bity ramki traktuje pasmo PNS jako darmowe, a każdy poziom szumu kosztuje bity
w strumieniu. Przy niskim bitrate i dużym obszarze PNS ramka się wtedy przepełnia, a
awaryjne cięcie zdejmuje pasma od góry. Domyślnie koder nie zalicza mu pasm PNS, więc tnie
dalej przez obszar szumu w kodowane widmo pod nim: całe ramki tracą wszystko powyżej około
1,5 kHz, co słychać jako trzask w rytmie ramek. Z tym przełącznikiem cel bitowy dla widma
jest obniżony o koszt poziomów szumu, a cięcie odzyskuje te bity, gdy zdejmuje pasmo szumu.

Zmierzone na akustycznej muzyce stereo, `-b 24000 --block-bias 0 -w 5500 --pns-above 2200`:
ramki z awaryjnym cięciem z 42 na 766 do 1, modulacja pasma szumu w rytmie ramek z 19,7 do
11,0 dB (11,6 bez PNS). Razem z `--pns-shared 1`: 0 obciętych ramek, 8,3 dB.

Zalecane przy każdym wymuszonym PNS przy niskim bitrate: `--pns-budget 1 --pns-shared 1`, opcjonalnie `--pns-hold 3`.

### `--pns-gain <x>`

**Ustawia, jak głośny jest fabrykowany szum względem energii pasma, które zastąpił.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` zostawia energię bez zmian; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy PNS podstawia pasmo, jedyną liczbą, którą zapisuje, jest energia szumu tego pasma, a
dekoder odtwarza losowy szum wyskalowany do niej. Ten przełącznik skaluje zapisaną energię
bezpośrednio, więc jest pokrętłem "jak głośny jest podstawiony szum" i tym pokrętłem PNS, po
które sięga się najczęściej.

Przy `1.0` odtworzony szum nosi energię pierwotnego pasma, czyli to, co mechanizm zakłada.
Powyżej `1.0` wypełnienie jest głośniejsze od pierwotnego, co pomaga, gdy podstawiony szum
brzmi zbyt nieśmiało i miks robi się matowy: prawdziwe poziomy szumu po podstawieniu często
wydają się cichsze, bo odtworzony szum nie ma struktury oryginału. Poniżej `1.0` jest
cichsze, i to jest ruch na sytuację, gdy wypełnienie syczy bardziej niż źródło.

To czyste terytorium gustu, bez poprawnej wartości, i właśnie dlatego oba kierunki warto
wypróbować na konkretnym materiale przed podjęciem decyzji.

Przykłady:
- Łagodnie, głośniej: `--pns-gain 1,2`. Nieco pełniejsze wypełnienie szumem; przydatne, gdy
  szum blach traci obecność.
- Mocno, głośniej: `--pns-gain 2.0`. Dwa razy więcej energii. Wyraźnie więcej syku i
  powietrza; na szumie szerokopasmowym może to brzmieć bliżej źródła niż wartość nominalnie
  poprawna.
- Przesadnie, głośniej: `--pns-gain 8.0`. Osiem razy więcej energii. Podstawione pasma
  dominują górę miksu oczywistym sztucznym sykiem, co jest szybkim sposobem usłyszenia
  dokładnie tego, które pasma zabrał PNS.
- Łagodnie, ciszej: `--pns-gain 0,8`. Nieco dyskretniejsze wypełnienie.
- Mocno, ciszej: `--pns-gain 0.4`. Wypełnienie szumem wyraźnie poniżej pierwotnej energii.
  Góra brzmi czyściej, a także bardziej matowo, i poziom szumu może zniknąć.
- Przesadnie, ciszej: `--pns-gain 0.0`. Podstawione pasma nie noszą w ogóle energii, czyli
  są ciche, więc PNS staje się generatorem dziur. Ustawienie skrajne i przydatny pokaz, ile
  sygnału nosiły pasma szumowe.

Współpraca i konflikty: nie robi nic, gdy PNS jest wyłączony. `--pns-tonality`,
`--pns-refpower`, `--pns-gapfill` i `--pns-min-width` decydują, które pasma są podstawiane;
ten zmienia tylko, jak głośny jest wynik. Użycie go na `0.0` to niezdarniejszy sposób
osiągnięcia tego, co `--pns 0` daje porządnie.

### `--pns-tonality <x>`

**Skaluje próg tonalności, który decyduje, czy pasmo liczy się jako szum i może być
podstawione.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

PNS odpala tylko na pasmach, które koder uznaje za szumopodobne, co mierzy jako niską
tonalność. Próg to linia między "tonalne, zakoduj porządnie" a "szum, podstaw je". Ten
przełącznik skaluje tę linię.

Wyżej niż `1.0` pozwala kwalifikować się większej liczbie pasm, w tym nieco tonalnym, więc
podstawianie szumu rozszerza się: więcej widma zastąpionego tanim opisem, więcej
oszczędności i więcej ryzyka, że prawdziwy ton rozmaże się w syk. Niżej niż `1.0` czyni
detektor wybredniejszym, więc podstawiane są tylko pasma wyraźnie szumowe, co jest
bezpieczniejsze i oszczędza mniej.

To pokrętło od szerokości PNS, w przeciwieństwie do `--pns-start`, które kontroluje, gdzie
PNS jest geograficznie dozwolony, i do `--pns-min-width`, które kontroluje go rozmiarem
pasma.

Przykłady:
- Łagodnie, szersze podstawianie: `--pns-tonality 1,3`. Kilka granicznych pasm więcej
  staje się PNS. Mała oszczędność, małe ryzyko.
- Mocno, szersze podstawianie: `--pns-tonality 2.0`. Dwa razy wyższy próg. Wyraźnie więcej
  pasm podstawionych; na materiale szumowym realna oszczędność, na materiale tonalnym
  słyszalne rozmazanie składowych.
- Przesadnie, szersze podstawianie: `--pns-tonality 20.0`. Prawie wszystko powyżej
  częstotliwości startowej liczy się jako szum. Tony trzymane zamieniają się w syk, a góra
  miksu staje się posłaniem szumu: ustawienie skrajne, przy którym mechanizmu nie da się
  pomylić z niczym innym.
- Łagodnie, węższe podstawianie: `--pns-tonality 0,8`. Nieco wybredniejsza detekcja.
- Mocno, węższe podstawianie: `--pns-tonality 0.4`. Podstawiany jest tylko jednoznaczny
  szum. Bezpiecznie, z dużą częścią oszczędności utraconą.
- Przesadnie, węższe podstawianie: `--pns-tonality 0.01`. W praktyce nic się nie kwalifikuje,
  więc trafia to tam, gdzie `--pns 0`, zostawiając PNS formalnie włączony.

Współpraca i konflikty: `--pns-refpower` jest drugą połową decyzji detekcyjnej i oba muszą
się zgodzić, zanim pasmo zostanie PNS, więc ruszanie tylko tym może być zablokowane przez
warunek mocy. `--pns-gain` zmienia głośność wyniku, nie wybór. `--pns-min-width` może
wykluczyć pasmo, które ten test przeszło.

### `--pns-refpower <x>`

**Skaluje próg mocy odniesienia, który poziom pasma musi spełnić, zanim zostanie uznane za
kandydata na szum.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Tonalność nie jest jedynym testem. Moc każdego pasma jest też porównywana z odniesieniem,
zanim pasmo może zostać kandydatem PNS, co trzyma mechanizm z dala od pasm, których poziom
czyni podstawianie złą wymianą. Ten przełącznik przesuwa ten warunek mocy.

Oba testy detekcji są koniunkcyjne: tonalność i moc odniesienia muszą się obie zgodzić,
zanim pasmo zostanie PNS. To dlatego ruszanie samym `--pns-tonality` czasem nie robi nic i
dlatego tę kartę i tamtą próbuje się zwykle razem.

Wartość mnoży sam próg. Powyżej `1.0` go podnosi, więc przechodzi mniej pasm i PNS się
wycofuje; poniżej `1.0` go obniża, więc przechodzi więcej pasm. Zmierzone na akustycznej
muzyce stereo przy 64 kbps LC, pasm PNS na ramkę: 5,5 przy `0.01`, 5,1 przy `0.4`, 4,8 przy
`0.8`, 4,6 przy `1.0`, 4,3 przy `1.3`, 3,6 przy `2.0`, 0 przy `20.0`.

Przykłady:
- Łagodnie, luźniejszy warunek: `--pns-refpower 0,8`. Nieco więcej kandydatów (z 4,6 do 4,8 pasma).
- Mocno, luźniejszy warunek: `--pns-refpower 0.4`. 5,1 pasma. W połączeniu z podniesionym
  `--pns-tonality` to miejsce, w którym szerokie podstawianie szumu naprawdę się zaczyna.
- Przesadnie, luźniejszy warunek: `--pns-refpower 0.01`. Test mocy przestaje rozróżniać i wybór
  zostaje po stronie tonalności (5,5 pasma), co dobrze pokazuje, ile pracy ten warunek wykonywał.
- Łagodnie, surowszy warunek: `--pns-refpower 1,3`. Nieco mniej kandydatów (4,3 pasma).
- Mocno, surowszy warunek: `--pns-refpower 2.0`. Dwa razy wyższy próg, 3,6 pasma. PNS wycofuje
  się do pasm, gdzie jest najmniej ryzykowny.
- Przesadnie, surowszy warunek: `--pns-refpower 20.0`. Nic nie przechodzi, więc PNS jest na
  papierze włączony, a w praktyce bezczynny.

Współpraca i konflikty: koniunkcyjny z `--pns-tonality`, więc warto testować je parą.
`--pns-gapfill` może i tak dodać pasmo między dwoma pasmami PNS, które tego testu nie
przeszło. Nic tutaj nie ma znaczenia, gdy PNS jest wyłączony.

### `--pns-gapfill <x>`

**Skaluje próg heurystyki, która zamyka jednopasmową lukę między dwoma podstawionymi
pasmami.**

- Dotyczy: wszędzie tam, gdzie PNS działa, i tylko wtedy, gdy dwa pasma PNS już siedzą po
  obu stronach zakodowanego pasma. Stan faktyczny: podłączone, bez zaobserwowanego efektu
  na materiale testowym.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy PNS oznaczył pasma po obu stronach wąskiej luki, zakodowana wyspa uwięziona między
dwoma pasmami szumu jest zwykle gorsza niż rozciągnięcie szumu przez nią: wyspa to jedyne
miejsce z prawdziwą strukturą widmową i odstaje. Heurystyka wypełniania luk może oznaczyć to
pasmo też jako PNS, a ten przełącznik skaluje próg, którego ona używa.

Wyżej czyni wypełnianie luk chętniejszym, niżej niechętnym. Oba kierunki są co najwyżej
subtelne, bo warunek wstępny jest surowy: dla AAC-LC reguła wymaga, żeby oba sąsiednie pasma
były już PNS, a na naszym materiale taka konfiguracja nie wystąpiła, więc żadna wartość tego
przełącznika nie zmieniła wyjścia. Przełącznik jest obecny i podłączony i może działać na
materiale, którego widmo wytworzy wymagany wzorzec, ale nie należy go traktować jako
codziennego narzędzia.

Prawdziwą dźwignię na to, jak duża część widma staje się szumem, dają zamiast tego
`--pns-tonality` albo `--pns-start`.

Przykłady:
- Łagodnie, więcej wypełniania: `--pns-gapfill 1,3`. Nieco chętniej zamyka lukę.
- Mocno, więcej wypełniania: `--pns-gapfill 3.0`. Wyraźnie chętniej, tam gdzie warunek
  wstępny w ogóle występuje.
- Przesadnie, więcej wypełniania: `--pns-gapfill 50.0`. Każda luka między dwoma pasmami PNS
  zostaje zamknięta. Na materiale, który to wyzwala, daje ciągłe obszary szumu tam, gdzie
  koder zachował wyspę.
- Łagodnie, mniej wypełniania: `--pns-gapfill 0,7`. Nieco bardziej niechętnie.
- Mocno, mniej wypełniania: `--pns-gapfill 0.3`. Luki zostają zakodowanymi wyspami.
- Przesadnie, mniej wypełniania: `--pns-gapfill 0.0`. Wypełnianie luk nie zdarza się nigdy,
  co jest najbliższą rzeczą do wyłączenia tej heurystyki.

Współpraca i konflikty: zależy całkowicie od tego, czy `--pns-tonality` i `--pns-refpower`
wytworzą dwa sąsiadujące pasma PNS z luką, więc rozszerzenie tamtych najpierw to jedyny
sposób, żeby dać temu przełącznikowi na czym działać.

Zmierzone: brak zmiany strumienia na 4 sygnałach testowych, 5 bitrate'ach i 6 wartościach
tego przełącznika w AAC-LC. Warunek wstępny reguły wypełniania luk w AAC-LC, czyli oba
sąsiednie pasma już jako PNS, na tym materiale nie wystąpił.

### `--pns-min-width <n>`

**Ustawia minimalną szerokość pasma SFB, w liniach widmowych, jaką pasmo musi mieć, zanim
podstawianie szumu będzie mogło na nim zadziałać.**

- Dotyczy: wszędzie tam, gdzie PNS działa. Jakikolwiek efekt mają tylko wartości powyżej
  wbudowanej wartości domyślnej, co dla AAC-LC oznacza powyżej 16.
- Wartości: surowa liczba linii, `1` i wyżej; domyślnie: `-1`, wyłączone, czyli własne
  minimum FDK (16 dla AAC-LC)
- Wpisywanie: liczba całkowita

Pasma SFB robią się szersze wraz ze wzrostem częstotliwości. Wąskie pasmo na dole widma
mieści niewiele linii i zwykle niesie treść tonalną, a podstawienie tam szumu rzuca się w
uszy. Szerokie pasmo wysoko mieści wiele linii i tam właśnie PNS jest na miejscu. Ten
przełącznik wyraża tę preferencję wprost, jako minimalną szerokość.

Warto zwrócić uwagę na asymetrię, bo oczywisty odczyt nazwy jest błędny. Podniesienie
minimum ogranicza PNS do pasm szerszych, czyli do górnego widma, i to działa. Obniżenie go
poniżej własnego minimum FDK nie otwiera PNS na węższe pasma: wartość domyślna FDK jest już
efektywną dolną granicą, więc wartości poniżej niej dają strumień fabryczny. Jest tu jeden
użyteczny kierunek, w górę.

W odróżnieniu od przełączników `--pns-*` powyżej, to surowa liczba linii, nie mnożnik.

Przykłady:
- Łagodnie, tylko szersze pasma: `--pns-min-width 24`. PNS ograniczony trochę wyżej w widmie.
  Małe zmniejszenie podstawiania, mała strata oszczędności.
- Mocno, tylko szersze pasma: `--pns-min-width 32`. Podstawiane mogą być tylko wyraźnie
  szerokie pasma. Bezpieczniej na materiale tonalnym, z widocznym spadkiem oszczędności z PNS.
- Przesadnie, tylko szersze pasma: `--pns-min-width 64`. Przy AAC-LC 64 kbps nie zostaje żadne
  kwalifikujące się pasmo, więc wynik jest identyczny jak `--pns 0`: PNS jest włączony i nie
  ma niczego, czego wolno mu dotknąć.
- W dół nie ma użytecznego przykładu: `2` i `4` oba dają strumień fabryczny, bo wbudowane
  minimum 16 już wiąże.

Współpraca i konflikty: nakłada się z `--pns-start`, który ogranicza PNS częstotliwością, a
nie szerokością; skoro szerokość rośnie wraz z częstotliwością, oba ograniczenia wzmacniają
się nawzajem i wygrywa ciaśniejsze. Nie robi nic, gdy PNS jest wyłączony.

Zmierzone: przy AAC-LC wartości `2` i `4` dają strumień fabryczny, co potwierdza, że
wbudowana wartość domyślna 16 jest najniższym dozwolonym poziomem. `64` przy AAC-LC 64 kbps
daje dokładnie ten strumień, który daje `--pns 0`, więc przy tej szerokości PNS jest
praktycznie wyłączony.

### Progi maskowania: warstwa zaawansowana

Siedem przełączników poniżej przesuwa sam model psychoakustyczny: próg, rozlewanie
maskowania, wymagany SNR i granice wokół niego. Większość przyjmuje liczby Q8. Przy CBR
pętla dopasowania musi trafić w tę samą liczbę bitów na ramkę bez względu na to, co mówi
model, więc odbiera większość ich efektu: zmierzone na muzyce, każdy z nich zmienia wynik po
dekodowaniu o setne albo dziesiąte części decybela. Przy VBR (`-m 1..5`) liczba bitów idzie
za modelem, a `--ath-scale` i `--minsnr-scale` stają się prawdziwymi ustawieniami jakości,
opłaconymi rozmiarem pliku. `--spread-mask`, obie granice okna, `--reduce-clamp` i
`--mask-slope` zostają przełącznikami laboratoryjnymi w obu trybach.

#### `--ath-scale <n>`

**Globalna skala progu maskowania: jedno pokrętło na "bądź czystszy" albo "bądź
oszczędniejszy" w całym widmie.**

- Dotyczy: każdego profilu z rdzeniem AAC, czyli wszystkich. Pod SBR działa tylko na
  rdzeń, bo SBR ma własny model szumu.
- Wartości: Q8, `1..4096`, gdzie `256` to x1,0; domyślnie: `256`, bez zmiany
- Wpisywanie: liczba całkowita

Próg maskowania to poziom szumu, o którym model sądzi, że w danym paśmie jest niesłyszalny.
`--ath-scale` mnoży wszystkie progi naraz. Powyżej 256 progi rosną, koder dochodzi do
wniosku, że więcej szumu jest niesłyszalne, i wydaje mniej bitów na pasmo: kodowanie
bardziej agresywne, co przy stałym bitrate oznacza, że te bity idą gdzie indziej albo
ramka po prostu kończy się wcześniej. Poniżej 256 progi spadają, więcej treści uznaje się
za warte dokładnego zakodowania, a zapotrzebowanie na bity rośnie.

Dwie rzeczy nie pozwalają temu pokrętłu być tak mocnym, jak sugeruje jego zakres. Działa na
logarytmicznej kopii progu, a część jego efektu znosi dalej w łańcuchu logika minimalnego
SNR oraz sufit redukcji progu 29 dB w kwantyzatorze CBR. Poza tym przy CBR globalna zmiana
progu raczej przetasowuje bity, niż zmienia ich liczbę, bo pętla dopasowania bitrate'u i tak
musi trafić w cel. `--minsnr-scale` działa na linii, do której unikanie dziur przyciąga
progi z powrotem, a tej `--ath-scale` przesunąć nie może.

Na swoje zarabia przy wysokim bitrate, gdzie jest nadwyżka bitów, a celem jest znalezienie
przez koder większej liczby rzeczy wartych zakodowania. Wartości trochę poniżej 256
zamieniają tę nadwyżkę w zachowany szczegół. Przy niskim bitrate użyteczny kierunek jest
zwykle w górę, jako sposób na świadome przyjęcie szorstkości zamiast pozostawienia pętli
dopasowania bitrate'u decyzji, gdzie ciąć.

Przykłady:
- Łagodnie, czyściej: `--ath-scale 230`. Progi niższe o jakieś 10 procent. Trochę więcej
  bitów na cichy szczegół; subtelne przy każdym bitrate, najlepiej widoczne powyżej
  192 kbps.
- Mocno, czyściej: `--ath-scale 180`. Wyraźne przesunięcie wysiłku w stronę treści o
  niskim poziomie. Ogony pogłosu i szum pomieszczenia zostają w lepszym stanie; przy napiętym
  bitrate coś innego jest wtedy „głodzone”.
- Przesadnie, czyściej: `--ath-scale 64`. Progi na jednej czwartej wartości fabrycznej.
  Modelowi mówi się praktycznie, że nic nie jest maskowane. Przy CBR pętla dopasowania
  bitrate'u walczy przez całą ramkę, a wynik nie jest lepszy, tylko inaczej skompromitowany; warto raz
  posłuchać, żeby zrozumieć, gdzie leży granica.
- Łagodnie, grubiej: `--ath-scale 290`. Trochę więcej szumu przyjętego na pasmo, mała
  oszczędność bitów.
- Mocno, grubiej: `--ath-scale 512`. Progi podwojone. Słyszalnie bardziej ziarniste na
  materiale szerokopasmowym, zauważalnie tańsze na pasmo.
- Przesadnie, grubiej: `--ath-scale 2048`. Progi osiem razy wyższe niż fabryczne. Koder
  wierzy, że prawie wszystko jest zamaskowane, a wynik to skrajna wersja agresywnego kodowania:
  wydrążony, syczący, z utraconym szczegółem wszędzie tam, gdzie obok siedzi coś
  głośniejszego.

Współpraca i konflikty: `--minsnr-scale` jest mocniejszym narzędziem do tego samego celu i
częściowo nadpisuje efekt tego przełącznika, bo żądanie SNR jest dolną granicą jakości,
przez którą podniesiony próg nie przepchnie. `--reduce-clamp 0` pozwala pchać progi głębiej
niż sufit 29 dB, co daje niskim wartościom tutaj więcej miejsca na działanie.
`--spread-mask` to pokrętło towarzyszące, od szerokości maskowania, a nie od jego wysokości.

Zmierzone (akustyczna muzyka stereo): CBR 96 kbps, `128` obniża łączny błąd o 0,04 dB, a
`384` podnosi go o 0,04 dB; przy 256 kbps `190` obniża go o 0,2 dB przy niezmienionym
rozmiarze pliku. VBR `-m 3` (około 108 kbps): `128` daje plik większy o 5,5% i błąd niższy
o 0,8 dB, `192` odpowiednio 2,3% i 0,3 dB, `384` plik mniejszy o 3,3% i błąd wyższy o
0,5 dB, `512` mniejszy o 5,6% i błąd wyższy o 0,8 dB.

#### `--minsnr-scale <n>`

**Skaluje stosunek sygnału do szumu, jaki koder musi dostarczyć w każdym paśmie; najbardziej
bezpośrednie pokrętło od szczegółu w tej grupie.**

- Dotyczy: każdego profilu z rdzeniem AAC.
- Wartości: Q8, `1` i wyżej bez udokumentowanego sufitu, gdzie `256` to x1,0; domyślnie:
  `-1`, wyłączone
- Wpisywanie: liczba całkowita

Oprócz progu maskowania każde pasmo nosi wymagany SNR kodowania. To gwarantowane minimum:
cokolwiek mówi wyliczenie progu, koder musi w tym paśmie osiągnąć taką jakość. FDK
wyprowadza je ze swoich tabel strojenia i jest to najbliższy odpowiednik stałych tone
masking noise i noise masking tone, które MusePack udostępnia bezpośrednio.

Poniżej 256 żąda wyższego SNR niż ustawienie fabryczne, czyli więcej szczegółu i więcej
bitów na pasmo. Powyżej 256 żąda mniej i wynik jest grubszy. Warto zwrócić uwagę na
odwrócenie wbrew intuicji: skalowany jest dozwolony szum, nie żądana jakość, więc mniej
znaczy czyściej.

Od `--ath-scale` różni się miejscem, w które trafia zmiana. Logika unikania dziur przyciąga
progi z powrotem do linii minimalnego SNR, więc wymagany SNR jest dolną granicą, której
pętla dopasowania nie może przebić, a przesunięty próg może zostać cofnięty. W pomiarze przy
VBR oba kosztują mniej więcej tyle samo za każdy zyskany decybel; `--minsnr-scale` kieruje
więcej zysku w dolne pasmo, `--ath-scale` rozkłada go równiej.

Przykłady:
- Łagodnie, więcej szczegółu: `--minsnr-scale 220`. Skromne podniesienie wymaganej jakości
  na pasmo. Widoczne przy średnim bitrate jako mniej wypadających pasm.
- Mocno, więcej szczegółu: `--minsnr-scale 150`. Wyraźnie wyższe żądanie. W połączeniu z
  hojnym bitrate to właśnie to pokrętło zamienia nadwyżkę bitów w zachowany szczegół; przy
  napiętym bitrate zmusza pętlę dopasowania bitrate'u, żeby cięła w innym miejscu.
- Przesadnie, więcej szczegółu: `--minsnr-scale 16`. Żądanie szesnastokrotne. Ramka nigdy go
  nie zaspokoi, a pętla dopasowania bitrate'u spędza całą ramkę w trybie ratunkowym, więc
  wynik jest gorszy niż fabryczny; pouczające raz, ale to nie ustawienie do pracy.
- Łagodnie, grubiej: `--minsnr-scale 300`. Trochę mniej wymagane na pasmo, mała oszczędność.
- Mocno, grubiej: `--minsnr-scale 512`. Połowa żądanej jakości. Słyszalnie bardziej
  ziarniste, z realną oszczędnością bitów, którą można przekierować innymi przełącznikami.
- Przesadnie, grubiej: `--minsnr-scale 4096`. Żądanie jest praktycznie usunięte, dolna granica
  jakości znika, a pasma swobodnie się zapadają. Ustawienie skrajne, które pokazuje, czego to
  żądanie SNR broniło.

Współpraca i konflikty: wynik jest trzymany w oknie, które wyznaczają `--minsnr-clamp-lo`
(najwyższe wymaganie, w FDK około 25 dB) i `--minsnr-clamp-hi` (najniższe, około 1 dB).
Daleko poniżej 256 wymaganie dochodzi do górnej granicy, którą `--minsnr-clamp-lo` poniżej
256 odsuwa dalej. `--reduce-clamp 0` zdejmuje sufit redukcji 29 dB w pętli CBR.

Zmierzone: przy CBR pętla dopasowania odbiera większość efektu: przy 96 kbps na muzyce
akustycznej `150` daje dolne pasmo czystsze o 0,8 dB i górę zgrubniejszą o 0,05 dB, a łączny
wynik zmienia się o setne części decybela. Przy VBR `-m 3` działa jak ustawienie jakości:
`150` daje plik większy o 1,3% i błąd niższy o 0,3 dB, `64` plik większy o 7,2% i błąd
niższy o 1,1 dB, `512` plik mniejszy o 0,7% i błąd wyższy o 0,3 dB.

#### `--spread-mask <n>`

**Skaluje, jak daleko maskowanie rozlewa się z jednego pasma na sąsiednie, co decyduje, ile
cichych pasm obok głośnych zostanie zakodowanych.**

- Dotyczy: każdego profilu z rdzeniem AAC. W naszych pomiarach efekt był mały przy każdym
  sprawdzonym bitrate'ie, przy CBR i przy VBR.
- Wartości: Q8, `0` i wyżej, gdzie `256` to x1,0; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Głośny ton, czyli sygnał maskujący (masker), maskuje szum nie tylko we własnym paśmie:
maskuje też skraj pasm wokół siebie, bardziej w górę niż w dół. Model symuluje to funkcją
rozmycia. To właśnie rozmycie jest powodem, dla którego ciche pasmo stojące obok głośnego
dostaje prawie zero bitów: model uznaje jego treść za dźwięk maskowany, przykryty przez
sąsiada.

Ten przełącznik skaluje rozmycie. Poniżej 256 oznacza mniej rozmycia maskowania, więc skraj
się zwęża, mniej pasm liczy się jako przykryte i więcej z nich zostaje zakodowanych. To ta
sama idea, co rozluźnienie założenia "ton maskuje szum" w innych koderach. Powyżej 256 skraj
się rozszerza i więcej pasm zostaje spisanych na straty jako przykryte przez sąsiadów, co
jest tańsze i bardziej płaskie.

W teorii rozlewanie decyduje najwięcej tam, gdzie bitów jest mało, ale jeszcze nie
rozpaczliwie: znacznie niżej „głodzona” jest cała ramka i pętla dopasowania tnie wszędzie,
znacznie wyżej bitów starcza na wszystko. Na materiale testowym rozlewanie decydowało jednak
o bardzo niewielu pasmach przy każdym bitrate'ie, zobacz Zmierzone.

Przykłady:
- Łagodnie, mniej maskowania: `--spread-mask 230`. Nieco węższy skraj. Kilka pasm więcej
  zostaje obok głośnej treści; więcej szczegółu wokół mocnych składowych.
- Mocno, mniej maskowania: `--spread-mask 180`. Wyraźnie więcej pasm zakodowanych obok
  głośnych. Na gęstym materiale to właśnie tutaj odzyskuje się szczegół wokół energii
  transjentów, realnym kosztem bitów.
- Przesadnie, mniej maskowania: `--spread-mask 32`. Rozmycie prawie usunięte. Model traci
  większość zdolności do decydowania, co jest przykryte, więc bity rozsypują się po pasmach,
  które naprawdę są niesłyszalne; ustawienie skrajne, pokazujące, co daje funkcja rozmycia.
- Łagodnie, więcej maskowania: `--spread-mask 300`. Odrobinę szerszy skraj, mała oszczędność.
- Mocno, więcej maskowania: `--spread-mask 512`. Rozmycie podwojone. Zauważalnie bardziej
  płaskie widmo obok głośnej treści, tanio, z cichym szczegółem spisanym na straty.
- Przesadnie, więcej maskowania: `--spread-mask 1536`. Prawie wszystko blisko czegokolwiek
  głośnego liczy się jako zamaskowane. Wynik jest wydrążony i rozmazany, przydatny głównie
  jako przeciwny koniec pokazu.

Współpraca i konflikty: warto łączyć z `--ath-scale` poniżej 256, gdy maska ma być zarazem
niższa i węższa; oba są niezależne i efekty się dodają. `--minsnr-scale` nadal ogranicza
wynik od dołu, więc bardzo niskie rozmycie nie zepchnie żadnego pasma poniżej jego żądania
SNR.

Zmierzone (akustyczna muzyka stereo): CBR 96 kbps, `128` i `384` w granicach 0,01 dB od
ustawień domyślnych; na `dense48` przy 40 kbps w granicach 0,05 dB. VBR `-m 3`: `128` daje
plik większy o 0,4%, `64` o 0,6%, a błąd zmienia się o mniej niż 0,01 dB. Na materiale
testowym rozlewanie maskowania decyduje o bardzo niewielu pasmach; karta opisuje mechanizm,
pomiar jego skalę.

#### `--minsnr-clamp-hi <n>`

**Przesuwa najgrubszy SNR, jaki koder musi zachować w każdym paśmie: gwarancję, że nawet
najmniej ważne pasmo zachowa trochę szczegółu.**

- Dotyczy: każdego profilu z rdzeniem AAC. Najwięcej znaczy przy VBR.
- Wartości: Q8, `1..320`, gdzie `256` to x1,0 (domyślnie w FDK); `-1` wyłączone (domyślnie).
  `320` to najluźniejsza możliwa granica, 0 dB.
- Wpisywanie: liczba całkowita

Każde pasmo ma wymagany SNR z tabel strojenia (zobacz *Wymagany SNR* wyżej), a FDK trzyma to
wymaganie w oknie. Ten przełącznik przesuwa zgrubny koniec okna, który w FDK oznacza, że
żadne pasmo nie może mieć więcej szumu niż około 1 dB pod własną energią. Ma to znaczenie
dla pasm, które tabele uznają za mało ważne: bez tej granicy zostałyby niemal całkiem
odpuszczone.

Poniżej 256 granica się zacieśnia, więc każde pasmo ma zagwarantowany wyższy SNR, a pasma
najmniej wymagające dostają więcej bitów. Powyżej 256 granica luzuje się, aż do 0 dB przy
`320`, gdzie pasmo może mieć tyle szumu, ile sygnału. Przy CBR pętla dopasowania odkręca
większość efektu w obu kierunkach; mierzalny skutek jest przy VBR, gdzie liczba bitów idzie
za wymaganiem.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo, bez przełącznika około 108 kbps):
- Łagodnie, ciaśniej: `--minsnr-clamp-hi 192`. Plik większy o 0,3%, łączny błąd niższy o
  0,06 dB.
- Mocno, ciaśniej: `--minsnr-clamp-hi 128`. Plik większy o 1,4%, łączny błąd niższy o
  0,2 dB, głównie powyżej 4 kHz.
- Przesadnie, ciaśniej: `--minsnr-clamp-hi 64`. Plik większy o 5,9%, błąd niższy o 0,5 dB:
  ciche pasma odkupione wyższym bitrate'em.
- Luźniej: `--minsnr-clamp-hi 320`. Najluźniejsza granica; na materiale testowym nie zmieniła
  niczego mierzalnego, bo tabele rzadko żądają tak mało.

Współpraca i konflikty: `--minsnr-scale` przesuwa wymaganie wewnątrz okna, a ten przełącznik
i `--minsnr-clamp-lo` przesuwają końce okna. Przy CBR różnice są rzędu setnych części
decybela.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `128` zmienia łączny błąd o 0,06 dB w złą
stronę, bo pętla dopasowania płaci za ciche pasma gdzie indziej. Wartości powyżej 320 są
odrzucane: za 0 dB wewnętrzna liczba stałoprzecinkowa się przepełniała i koder wyliczał
bezsensowne wymagania.

#### `--minsnr-clamp-lo <n>`

**Przesuwa najwyższy SNR, jakiego pasmo może zażądać: granicę, w którą uderza
`--minsnr-scale` poniżej 256.**

- Dotyczy: każdego profilu z rdzeniem AAC, gdy `--minsnr-scale` żąda więcej SNR niż FDK albo
  gdy tabele już żądają najwięcej.
- Wartości: Q8, `1` i wyżej, gdzie `256` to x1,0 (domyślnie w FDK); `-1` wyłączone
  (domyślnie)
- Wpisywanie: liczba całkowita

To dokładny koniec tego samego okna: w FDK żadne pasmo nie może żądać więcej niż około 25 dB
SNR, choćby tabele uważały je za bardzo ważne. Dzięki temu jedno pasmo nie zabiera całej
ramki.

Poniżej 256 granica odsuwa się, więc pasmo może żądać więcej niż 25 dB. Samo z siebie
zmienia to niewiele, bo do granicy dochodzi mało pasm; przydaje się razem z `--minsnr-scale`
dobrze poniżej 256, które dopycha wymagające pasma do tej granicy. Powyżej 256 granica się
przybliża, najbardziej wymagające pasma są ograniczane wcześniej, a ich bity idą do
pozostałych. Bardzo duże wartości zsuwają oba końce okna i każde pasmo żąda tego samego, a
to już nie jest model psychoakustyczny.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo):
- Łagodnie, dokładniej: `--minsnr-clamp-lo 64`. Bez `--minsnr-scale` bez mierzalnej zmiany.
- Mocno, dokładniej: `--minsnr-scale 64 --minsnr-clamp-lo 64`. Dolne pasmo (100 Hz..1 kHz)
  czystsze o 0,4 dB niż z samym `--minsnr-scale 64`.
- Łagodnie, zgrubniej: `--minsnr-clamp-lo 1024`. Plik mniejszy o 0,2%, błąd w dolnym
  paśmie wyższy o 0,3 dB.
- Przesadnie, zgrubniej: `--minsnr-clamp-lo 4096`. Plik mniejszy o 0,8%, dolne pasmo gorsze o
  1,4 dB: tracą właśnie pasma, na których tabelom najbardziej zależy.

Współpraca i konflikty: partner `--minsnr-scale` poniżej 256. Przełączniki dziur z grupy E
zajmują się tym samym problemem z drugiej strony, decydując, które pasma wolno wyzerować.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `4096` podnosi błąd w dolnym paśmie o 1,1
dB, a łączny błąd o 0,03 dB; samo `64` nie zmienia niczego mierzalnego.

#### `--reduce-clamp <0|1>`

**Usuwa sufit 29 dB na to, jak daleko kwantyzator CBR może pchać progi w dół, pozwalając
innym przełącznikom od szczegółu działać bez uderzania w ścianę.**

- Dotyczy: CBR, czyli domyślnej ścieżki `-m 0`. VBR używa innej ścieżki kodu i nie jest tym
  dotknięte.
- Wartości: `0` albo `1`; domyślnie: `1`, sufit obowiązuje, jak w fabrycznym FDK
- Wpisywanie: liczba całkowita, włącz/wyłącz

Kiedy pętla dopasowania bitrate'u CBR potrzebuje w paśmie więcej jakości, redukuje próg tego
pasma. Na łączną redukcję jest sufit, około 29 dB, który nie pozwala pętli gonić jednego
wymagającego pasma w nieskończoność. Przy normalnym materiale sufit nigdy nie wiąże. Przy
celowym żądaniu znacznie większej precyzji niż fabryczna, przez `--minsnr-scale` dobrze
poniżej 256 albo `--ath-scale` dobrze poniżej 256, to właśnie ten sufit zatrzymuje dalszy
efekt ustawienia.

Ustawienie `0` zdejmuje sufit, więc progi mogą być pchane głębiej i do wymagających pasm
płynie więcej bitów. Samo z siebie zmienia niewiele, bo fabryczny koder rzadko dochodzi do
29 dB redukcji. To przełącznik towarzyszący, nie samodzielny.

Wartości:
- `1`: sufit redukcji 29 dB obowiązuje (ustawienie fabryczne, domyślne).
- `0`: sufit usunięty, progi mogą być pchane głębiej.

Przykład w kontekście:

    fdkaac -p 2 -b 256000 --minsnr-scale 150 --reduce-clamp 0 -o out.m4a in.wav

Współpraca i konflikty: warto łączyć w parę z `--minsnr-scale` poniżej 256 dla skrajnego
szczegółu; sam jest niemal bez sensu. Bez efektu przy VBR (`-m 1..5`).

#### `--mask-slope <dB>`

**Przesuwa, jak głęboko pod średnią energią ramki musi leżeć pasmo, zanim koder rozluźni
jego wymagany SNR.**

- Dotyczy: każdego profilu z rdzeniem AAC, obu kanałów. Domyślnie wyłączone.
- Wartości: `-24.0..50.0` dB, `0` = wyłączone (domyślnie); odmienne wyniki tylko w
  przybliżeniu w zakresie -9,9..+9
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

FDK ma heurystykę, która rozluźnia wymagany SNR pasma, gdy jego energia leży daleko pod
średnią energią ramki, w FDK więcej niż około 10 dB pod nią. Prościej: koder wydaje mniej na
bardzo ciche pasma, zakładając, że głośne je zamaskują. Ten przełącznik przesuwa tę granicę
10 dB: wartość dodatnia ją podnosi, więc mniej pasm uchodzi za pomijalne, a wartość ujemna
ją obniża, więc pomijalnych jest więcej. Mowa tu o dźwięku maskowanym, czyli o tym, co
sygnał maskujący (masker) przykrywa.

Zmierzony na muzyce efekt jest mały. Heurystyka dotyka tylko najcichszych pasm, a pętla
dopasowania rozdziela to, co się zaoszczędzi, więc przy CBR błąd po dekodowaniu zmienia się
o setne części decybela. Przy VBR plik rośnie albo maleje o ułamek procenta. To przełącznik
laboratoryjny do sprawdzenia, co ta heurystyka wnosi, a nie narzędzie do szczegółu. Do
cichego szczegółu służą `--minsnr-scale` przy VBR i przełączniki dziur z grupy E.

Przyjmowany zakres jest szerszy niż żywy. Odmienne wyniki pojawiają się mniej więcej od -9,9
do +9; przy -10 i niżej wynik przestaje się zmieniać, +10 do +40 daje jeden strumień, a 45
do 50 inny.

Przykłady:
- Łagodnie, ochrona cichych pasm: `--mask-slope 2`. Kilka cichych pasm więcej zachowuje
  wymaganie.
- Mocno, ochrona cichych pasm: `--mask-slope 6`. Przy VBR plik większy o 0,1% na muzyce
  akustycznej, łączny błąd niższy o 0,03 dB.
- Przesadnie, ochrona cichych pasm: `--mask-slope 9`. Szczyt żywego zakresu; dalej nic się
  nie zmienia.
- Łagodnie, mniej dla cichych pasm: `--mask-slope -2`. Kilka cichych pasm więcej
  rozluźnionych.
- Mocno, mniej dla cichych pasm: `--mask-slope -6`. Przy CBR w granicach 0,1 dB od ustawień
  domyślnych.
- Przesadnie, mniej dla cichych pasm: `--mask-slope -9,5`. Dół żywego zakresu.

Współpraca i konflikty: działa jednakowo na mid i side. Nie rusza progu side, który należy
do `--side-bias` i `--side-knee`.

Zmierzone: mix48 przy 40 kbps daje odmienne strumienie tylko dla około -9,9..+9. Akustyczna
muzyka stereo przy 96 kbps CBR (`+6`, `-6`) i `dense48` przy 40 kbps (`+9`, `-9`): mid i
side w granicach 0,1 dB od ustawień domyślnych. VBR `-m 3`: `+9` i `-9` zmieniają plik o
+0,3% i +0,6%, błąd mid o mniej niż 0,2 dB.

## E. Transjenty rdzenia, pre-echo i dziury w widmie

Te trzynaście przełączników sięga samego rdzenia AAC, pod SBR. Znaczą najwięcej tam, gdzie
rdzeniowi brakuje bitów: AAC-LC stereo przy 24..50 kbps z pasmem wymuszonym dobrze powyżej
tego, co wybrałby FDK (`-w 16000..20000`), oraz LD albo ELD z rozszerzonym `--core-cutoff`.
Przy wygodnych bitrate'ach większość z nich zmienia niewiele, bo problemy, którymi się
zajmują, prawie tam nie występują.

Pod presją bitową pojawiają się dwie różne awarie i warto je trzymać osobno.

**Pre-echo.** Długi blok AAC obejmuje 2048 próbek, czyli około 43 ms przy 48 kHz. Szum
kwantyzacji bloku rozkłada się na cały blok w czasie. Kiedy ostry atak siedzi późno w bloku,
ten szum trafia też przed atakiem, gdzie ucho ma prawie nic, co by go maskowało: maskowanie
wstecz trwa tylko kilka milisekund. Słychać to jako delikatne rozmycie albo "cień" przed
kastanietami, klaśnięciem, szarpniętą struną czy uderzeniem fortepianu. Koder ma przeciw
temu cztery narzędzia i ta grupa udostępnia wszystkie cztery:

1. detektor ataku, który przełącza ramkę na osiem bloków krótkich po 256 próbek
   (`--attack-decay`, `--attack-floor`, razem z `--block-bias` z grupy F);
2. grupowanie tych ośmiu krótkich okien w grupy współczynników skali (`--short-groups`);
3. kształt okna, który decyduje, ile szumu wycieka z miejsca, gdzie powinien być
   (`--window-shape`);
4. dla przypadków zostających w blokach długich, TNS (temporal noise shaping, które
   układa szum w czasie wewnątrz bloku: `--tns-thresh`, `--tns-start`) oraz kontrola
   pre-echa, która ogranicza, jak szybko próg maskowania może rosnąć z ramki na ramkę
   (`--preecho-rise`, `--preecho-depth`).

AAC-LD i AAC-ELD nie mają bloków krótkich i mają ustalone okno, więc dla nich zostaje tylko
TNS i kontrola pre-echa. Dlatego te przełączniki znaczą tam więcej niż w LC.

**Dziury w widmie.** Kiedy ramki nie stać na wszystkie pasma, które ma zakodować, FDK
najpierw rozluźnia SNR, jakiego żąda od pasm będących szczytami albo dolinami widma, a jeśli
to nadal nie wystarcza, ścina całe pasma współczynnika skali (scale factor band, SFB) do
zera. Wyzerowane pasmo to cisza w tym zakresie częstotliwości w tej ramce. W czasie brzmi to jak bulgoczące, migające
albo "podwodne" góry: pasma przychodzą i odchodzą z ramki na ramkę. FDK nazywa cały ten
mechanizm "avoid holes", co jest trochę ironiczne: to on decyduje, które dziury zrobić.
Kolejność jest ustalona: pasma są ścinane w maksymalnie ośmiu krokach energetycznych, od
najcichszych, i tylko od pasma początkowego w górę, żeby dół widma był chroniony.
Przełączniki `--avoid-holes`, `--hole-start`, `--hole-start-short`, `--hole-spread` i
`--hole-ladder` zmieniają ten mechanizm.

W żadnym z tych przypadków nie ma nic za darmo. Przy CBR budżet ramki jest ustalony. Pasmo,
którego nie ścięto, kosztuje bity, których nie dostanie inne pasmo, a blok krótki kosztuje
więcej informacji pobocznej niż długi. Każdy przełącznik tutaj przesuwa bity; żaden ich nie
dodaje.

Wszystkie pomiary cytowane w tej grupie zrobiliśmy po dekodowaniu, na dwóch sygnałach
syntetycznych zbudowanych do tego celu. `attacks48.wav` zawiera kliknięcia przypominające
kastaniety na bardzo cichym tle; pre-echo to energia błędu w 1024 próbkach przed każdym
atakiem, względem energii samego ataku. `dense48.wav` zawiera gęste akordy szerokopasmowe,
jasny szum i hi-hat; dziurą jest pasmo 1/3 oktawy między 2 a 16 kHz, którego energia
oryginalna jest powyżej -70 dBFS i które wychodzi z dekodera co najmniej 15 dB za cicho.
Narzędzia, które produkują te liczby, są w `tools/core/` w repozytorium, więc pomiary można
powtórzyć na dowolnym materiale.

Punkt wyjścia dla AAC-LC stereo przy 24..50 kbps z szerokim pasmem:

    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

### `--hole-ladder <1..8>`

**Ogranicza, jak daleko koder może pójść, ścinając pasma do zera, tak by „zagłodzona” ramka
dostawała grubszą kwantyzację zamiast większej liczby pustych pasm.**

- Dotyczy: rdzenia AAC w każdym profilu (LC, rdzeń HE-AAC i HE-AAC v2, LD, ELD). Znaczy coś
  tylko wtedy, gdy kończą się bity.
- Wartości: 1..8; domyślnie: wyłączone, czyli zachowanie fabryczne (8 kroków)
- Wpisywanie: liczba całkowita

Ramka „zagłodzona” to taka, której brakuje bitów, więc koder zeruje słabe pasma albo koduje
je najgrubszym krokiem.

Kiedy ramka przekracza budżet, FDK sortuje pasma kandydujące według energii na osiem kroków
i ścina je krok po kroku, od najcichszych, sprawdzając budżet po każdym kroku. Przy ośmiu
krokach trudna ramka może skończyć na ścinaniu całkiem głośnych pasm. `--hole-ladder N`
zatrzymuje się po N krokach. Wtedy wolno ścinać tylko najcichsze pasma. Jeśli ramka nadal
przekracza budżet, pętla dopasowania bitrate'u rozwiązuje resztę inaczej, podnosząc krok
kwantyzacji we wszystkich pasmach: wynikiem jest więcej szumu rozłożonego równomiernie
zamiast większej liczby brakujących pasm.

To najmocniejszy przełącznik przeciw bulgoczącej górze przy niskim bitrate. Wymiana jest
realna, ale zwykle korzystna: nieco bardziej ziarniste wypełnienie w zamian za pasma, które
zostają obecne z ramki na ramkę. Ucho jest znacznie bardziej tolerancyjne na stały szum niż
na pasma, które mrugają.

Przykłady (istnieje tylko jeden kierunek: mniej kroków niż fabrycznie):
- Łagodnie: `--hole-ladder 6`. Znikają tylko najgłośniejsze kroki ścinania. Trudne ramki
  tracą kilka pasm mniej; łatwe ramki są nietknięte.
- Mocno: `--hole-ladder 2`. Ścinać wolno tylko dwa najcichsze kroki. Na gęstym materiale
  przy 32..50 kbps usuwa to większość dziur (patrz Zmierzone).
- Przesadnie: `--hole-ladder 1`. Ścinać wolno tylko najcichszy krok, więc prawie każde pasmo
  zostaje, a cały problem budżetowy zamienia się w grubszą kwantyzację. Na bardzo
  gęstym materiale przy 24 kbps może to brzmieć jednolicie chropowato: ustawienie skrajne
  dla tej wymiany.

Współpraca i konflikty: dobrze łączy się z `--avoid-holes 1` i `--hole-start`. Pętla
dopasowania bitrate'u może podnieść krok tylko do pewnej granicy; przy skrajnych budżetach
pasma i tak znikają, bo same kwantyzują się do zera, czemu żaden przełącznik tutaj nie
zapobiega.

Zmierzone (dense48, LC stereo, dziury po dekodowaniu): 32 kbps / 17 kHz: z 37% do 8% przy
`2`; 40 kbps / 18 kHz: z 37% do 1,7%; 50 kbps / 19 kHz: z 35% do 0,2%. Stosunek błędu do
sygnału w pasmach, które zostają, pogarsza się o 0,3..0,5 dB.

Pod maską: ograniczenie `enIdx` w pętli "allow more holes" w `adj_thr.cpp`.

### `--avoid-holes <0|1>`

**Wymusza włączenie albo wyłączenie ochrony szczytów i dolin w FDK.**

- Dotyczy: rdzenia AAC w każdym profilu.
- Wartości: `0` wyłączone, `1` włączone; domyślnie: wyłączone, czyli zachowanie fabryczne
  (włączona od 20 kbps na kanał, wyłączona poniżej)
- Wpisywanie: liczba całkowita, 0 albo 1

Przed ścinaniem pasm FDK może rozluźnić SNR, jakiego żąda, w mądrzejszy sposób: obniża
żądanie w dolinach między szczytami widma, a utrzymuje je w szczytach, tak by wąskie głośne
pasma zostały, a oszczędności przyszły stamtąd, gdzie ucho najmniej je zauważa. Fabryczny
FDK włącza to dopiero od 20 kbps na kanał, co dla stereo oznacza od 40 kbps. Poniżej,
dokładnie w zakresie 24..50 kbps, w który ta grupa jest wycelowana, ochrona jest wyłączona i
ścinanie zaczyna się wcześniej.

Wartości:
- `1`: ochrona włączona przy każdym bitrate. Poniżej 40 kbps stereo to jest sensowne
  ustawienie; na tej wartości i powyżej nic się nie zmienia, bo domyślnie jest już włączona.
- `0`: ochrona wyłączona przy każdym bitrate. Ustawienie diagnostyczne: pokazuje, co ta
  ochrona dawała.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --avoid-holes 1 --hole-ladder 2`.

Współpraca i konflikty: sam z siebie daje mały efekt (zmierzone), bo ochrona tylko
przestawia, skąd biorą się oszczędności; najbardziej pomaga razem z `--hole-ladder`.

Zmierzone: przy LC 40 i 128 kbps stereo `--avoid-holes 1` daje strumień identyczny z
fabrycznym (tam już jest włączona), a `--avoid-holes 0` go zmienia.

### `--hole-start <sfb>`

**Chroni dolną część widma w blokach długich przed ścięciem do zera.**

- Dotyczy: rdzenia AAC, bloków długich (LC, rdzeń HE-AAC, LD, ELD).
- Wartości: pasmo SFB 0..51; domyślnie: wyłączone, czyli zachowanie fabryczne
  (pasmo 15, gdy ochrona jest włączona, pasmo 0, gdy jest wyłączona)
- Wpisywanie: liczba całkowita

Pasma poniżej tej liczby nigdy nie są wybierane jako dziury. Fabrycznie start wypada na
paśmie 15, co przy 48 kHz jest z grubsza 2 do 3 kHz: bas i dolna średnica są bezpieczne,
wszystko powyżej wolno ściąć. Podniesienie liczby przesuwa chroniony obszar w górę;
obniżenie dopuszcza dziury głębiej. Warto pamiętać, że chronione pasmo nadal kosztuje bity:
oszczędności muszą wtedy przyjść z pasm powyżej albo z grubszej kwantyzacji wszędzie.

Ile pasm istnieje, zależy od profilu i częstotliwości próbkowania; `--verbose` wypisuje tę
liczbę jako `active SFBs (long)`, 49 dla LC stereo przy 48 kHz.

Przykłady, chronić więcej:
- Łagodnie: `--hole-start 20`. Kilka pasm więcej w dolnej średnicy jest bezpiecznych.
- Mocno: `--hole-start 25`. Ochrona z grubsza do obszaru obecności; zmierzone jako
  zmniejszenie dziur z 37% do 26% przy 32 kbps.
- Przesadnie: `--hole-start 51`. Żadnego pasma w bloku długim nie wolno temu mechanizmowi
  ściąć. Cały problem budżetowy spada na kwantyzator; przy niskim bitrate góra robi się
  bardzo zgrubna, a pasma i tak mogą zniknąć, kwantyzując się do zera.

Przykłady, dopuszczać głębsze dziury:
- Łagodnie: `--hole-start 12`. Trochę więcej swobody zaraz poniżej startu fabrycznego.
- Mocno: `--hole-start 6`. Dziury dopuszczone w dół, w dolną średnicę.
- Przesadnie: `--hole-start 0`. Nawet pasma basu wolno ściąć. Przydatne tylko do usłyszenia,
  czemu zapobiega ochrona fabryczna.

Bloki krótkie: bez `--hole-start-short` używana jest tam ta sama częstotliwość (najbliższa
krawędź pasma krótkiego; `--hole-start 25`, czyli 5 kHz przy 48 kHz, staje się pasmem
krótkim 6, a para fabryczna 15/3 przechodzi sama na siebie). `--verbose` to pokazuje.

Współpraca i konflikty: `--hole-start-short` ustawia wartość krótką jawnie i wygrywa.

### `--hole-start-short <sfb>`

**Ta sama ochrona dla bloków krótkich.**

- Dotyczy: LC i rdzenia HE-AAC, ramek kodowanych jako bloki krótkie. LD i ELD nie mają
  żadnych.
- Wartości: pasmo SFB bloku krótkiego 0..15; domyślnie: wyłączone, czyli
  zachowanie fabryczne (pasmo 3) albo częstotliwość z `--hole-start`, gdy ten jest podany
- Wpisywanie: liczba całkowita

Blok krótki ma około 14 pasm przy 44,1 albo 48 kHz, więc każde pasmo jest szerokie, a liczby
są małe. Fabrycznie chronione są pasma 0..2.

Przykłady, chronić więcej:
- Łagodnie: `--hole-start-short 5`.
- Mocno: `--hole-start-short 8`. Większość pasma w ramce transjentowej jest chroniona.
- Przesadnie: `--hole-start-short 15`. Żadnego krótkiego pasma nie wolno ściąć; ramki
  transjentowe stają się zamiast tego zgrubne.

Przykłady, dopuszczać głębsze dziury:
- Łagodnie: `--hole-start-short 2`.
- Mocno: `--hole-start-short 1`.
- Przesadnie: `--hole-start-short 0`. Ramka ataku może stracić swój bas.

### `--hole-spread <dB>`

**Decyduje, jak chętnie pasmo liczy się jako przykryte przez sąsiadów, a zatem jako
bezpieczny kandydat na dziurę.**

- Dotyczy: rdzenia AAC w każdym profilu.
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (3 dB dla bloków
  długich, 2 dB dla bloków krótkich)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Pasmo można ściąć bezpieczniej, kiedy sąsiedzi i tak je maskują. Żeby to ocenić, FDK
porównuje energię pasma z energią rozmytą do niego z pasm sąsiednich, po obniżeniu tej
rozmytej energii o margines. Mały margines sprawia, że sąsiedzi wyglądają mocno, więc wiele
pasm wygląda na przykryte i staje się kandydatami. Duży margines sprawia, że sąsiedzi
wyglądają słabo, więc mniej pasm wygląda na przykryte i więcej z nich jest chronionych.

To delikatne pokrętło. Na materiale testowym przesunęło liczbę dziur o jeden lub dwa procent
pasm, znacznie mniej niż `--hole-ladder`.

Przykłady, chronić więcej:
- Łagodnie: `--hole-spread 4,5`.
- Mocno: `--hole-spread 9`.
- Przesadnie: `--hole-spread 24`. Sąsiedzi praktycznie nigdy nie liczą się jako
  przykrywający, więc prawie żadne pasmo nie kwalifikuje się z tego powodu.

Przykłady, dopuszczać więcej dziur:
- Łagodnie: `--hole-spread 2`.
- Mocno: `--hole-spread 1`.
- Przesadnie: `--hole-spread 0`. Rozmyta energia jest brana za dobrą monetę; każde pasmo
  stojące obok głośniejszych jest kandydatem.

Wartość dotyczy bloków długich; bloki krótkie dostają dwie trzecie, co zachowuje proporcję
fabryczną (3 dB długie, 2 dB krótkie). `--hole-spread 3` to więc dokładnie ustawienie
domyślne w obu. Do 1.4.0-beta.1 jedna wartość zastępowała obie.

### `--preecho-rise <dB>`

**Ogranicza, jak szybko próg maskowania pasma może rosnąć z ramki na ramkę, tak by szum
kwantyzacji nie mógł wybiegać przed atak w bloku długim.**

- Dotyczy: bloków długich w każdym profilu. Najistotniejszy w LD i ELD oraz w LC z
  wymuszonymi blokami długimi (`--block-bias 0`).
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (3,01 dB, czyli
  dwukrotność w mocy)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Model psychoakustyczny wylicza próg maskowania dla pasma i ramki. W ramce zawierającej atak
próg skacze w górę, bo atak maskuje dużo. Ale szum tej ramki siedzi także przed atakiem,
gdzie nic go nie maskuje. Kontrola pre-echa ogranicza więc wzrost: próg tej ramki może być
najwyżej N dB powyżej progu ramki poprzedniej. Poniżej 3,01 ograniczenie jest ciaśniejsze
(mniej pre-echa, więcej bitów na nagłosy); powyżej jest luźniejsze (mniej bitów, więcej
pre-echa).

W LC efekt jest zwykle mały, bo prawdziwe ataki są już przenoszone do bloków krótkich, zanim
to ograniczenie zaczyna mieć znaczenie. LD i ELD nie mają bloków krótkich, więc tam ten
przełącznik i TNS są główną obroną.

Przykłady, ciaśniej:
- Łagodnie: `--preecho-rise 2`.
- Mocno: `--preecho-rise 1`. Nagłosy w blokach długich dostają zauważalnie więcej bitów.
- Przesadnie: `--preecho-rise 0`. Próg nie może w ogóle rosnąć z ramki na ramkę. Każde
  crescendo jest kodowane tak, jakby nadal było cicho, co marnuje bity na głośnych
  fragmentach i „głodzi” wszystko inne przy CBR.

Przykłady, luźniej:
- Łagodnie: `--preecho-rise 4,5`.
- Mocno: `--preecho-rise 9`.
- Przesadnie: `--preecho-rise 24`. Ograniczenia praktycznie nie ma: kontrola pre-echa jest
  wyłączona.

Współpraca i konflikty: `--preecho-depth` ogranicza przeciwną stronę tej samej reguły.

Zmierzone: dziesiąte części dB energii pre-echa w LC na attacks48 (ataki i tak idą do bloków
krótkich). Przełącznik zmienia strumienie LD i ELD.

### `--preecho-depth <dB>`

**Ogranicza, jak nisko kontrola pre-echa może najwyżej obniżyć próg.**

- Dotyczy: tego samego co `--preecho-rise`.
- Wartości: 0.0..60.0 dB (efekt przestaje rosnąć około 45 dB); domyślnie: wyłączone, czyli
  zachowanie fabryczne (20 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy ograniczenie wzrostu ciągnie próg w dół, ten przełącznik ustawia dolną granicę: próg
nie może skończyć niżej niż N dB poniżej tego, co model psychoakustyczny wyliczył dla ramki.
Nie pozwala ograniczeniu żądać absurdalnej precyzji po długiej ciszy, kiedy próg poprzedniej
ramki był znikomy.

Przykłady, dopuszczać głębsze obniżanie:
- Łagodnie: `--preecho-depth 25`.
- Mocno: `--preecho-depth 40`. Po ciszy pierwszy atak jest kodowany bardzo czysto i bardzo
  drogo.
- Przesadnie: `--preecho-depth 60`. Praktycznie brak dolnej granicy; przy CBR jeden nagłos po
  pauzie może zjeść rezerwuar. Wewnętrzny współczynnik osiąga dno około 45 dB, więc wartości
  między około 45 a 60 zachowują się tak samo.

Przykłady, płycej:
- Łagodnie: `--preecho-depth 15`.
- Mocno: `--preecho-depth 6`.
- Przesadnie: `--preecho-depth 0`. Progu nie wolno w ogóle obniżyć, co wyłącza kontrolę
  pre-echa niezależnie od `--preecho-rise`.

### `--attack-decay <x>`

**Ustawia pamięć detektora ataku, który wybiera między blokami długimi a krótkimi.**

- Dotyczy: LC i rdzenia HE-AAC (profili z blokami krótkimi). W naszym teście nie miał też
  efektu na LD ani ELD.
- Wartości: 0.0..0.99; domyślnie: wyłączone, czyli zachowanie fabryczne (0,70)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Detektor dzieli każdą ramkę na podokna, odfiltrowuje niskie częstotliwości i porównuje
energię każdego podokna z bieżącą średnią tego, co było wcześniej. Dostatecznie duży
stosunek to atak. Bieżąca średnia zachowuje część swojej wartości na podokno: ta część to
właśnie ten przełącznik. Wyższe wartości oznaczają długą pamięć, więc atak jest porównywany
z długą średnią; niższe oznaczają porównanie z ostatnią chwilą.

Długa pamięć jest spokojniejsza i bardziej selektywna na gęstym materiale, ale po głośnym
fragmencie średnia zostaje wysoko i atak, który po nim następuje, może zostać przeoczony.
Krótka pamięć łapie ataki zaraz po głośnych fragmentach, ale na ruchliwym materiale może
odpalać zbyt często i wrzucać ramki tonalne do bloków krótkich, co kosztuje bity i
rozdzielczość.

Przykłady, dłuższa pamięć:
- Łagodnie: `--attack-decay 0,8`.
- Mocno: `--attack-decay 0.9`. Zmierzone: pre-echo w najgorszym przypadku niższe o 2,9 dB na
  attacks48.
- Przesadnie: `--attack-decay 0.99`. Średnia prawie się nie rusza; ataki po głośnych
  fragmentach są często przeoczane.

Przykłady, krótsza pamięć:
- Łagodnie: `--attack-decay 0,6`.
- Mocno: `--attack-decay 0.4`.
- Przesadnie: `--attack-decay 0`. Każde podokno jest porównywane tylko z poprzednim;
  detektor robi się nerwowy na materiale szumowym.

### `--attack-floor <dB>`

**Przesuwa minimalną energię, jaką atak musi mieć, zanim się zaliczy.**

- Dotyczy: LC i rdzenia HE-AAC. W naszym teście zmienił też strumień LD, ale nie ELD.
- Wartości: -60.0..30.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (przesunięcie 0 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Oprócz stosunku detektor wymaga bezwzględnego minimum energii, żeby stuknięcia w niemal
ciszy nie wyzwalały bloków krótkich. Wartości ujemne obniżają ten najniższy dozwolony
poziom: ciche ataki, takie jak miotełki, miękkie hi-haty czy ciche szarpnięcie gitary, też
przełączają na bloki krótkie. Wartości dodatnie go podnoszą: liczą się tylko głośne ataki.

Przykłady, łapanie cichszych ataków:
- Łagodnie: `--attack-floor -6`.
- Mocno: `--attack-floor -20`.
- Przesadnie: `--attack-floor -60`. Praktycznie każdy wzrost w cichym fragmencie się zalicza.
  Bloki krótkie pojawiają się wtedy na szumie pomieszczenia i na miękkich fragmentach
  tonalnych, wraz z kosztem bitowym, który to niesie.

Przykłady, tylko głośne ataki:
- Łagodnie: `--attack-floor 6`.
- Mocno: `--attack-floor 15`.
- Przesadnie: `--attack-floor 30`. Miękka perkusja zostaje w blokach długich i pojawia się
  na niej pre-echo.

### `--window-shape <0|1>`

**Wybiera kształt okna każdego bloku: sinusowe wszędzie albo KBD wszędzie.**

- Dotyczy: AAC-LC oraz rdzenia LC w HE-AAC i HE-AAC v2. LD i ELD używają ustalonego okna i
  nie są tym objęte.
- Wartości: `0` sinusowe, `1` KBD; domyślnie: wyłączone, czyli zachowanie fabryczne (KBD dla
  bloków długich, sinusowe dla bloków krótkich)
- Wpisywanie: liczba całkowita, 0 albo 1

Okno decyduje o tym, jak energia bloku wycieka w sąsiednie częstotliwości. Okno sinusowe ma
wąski listek główny (dobra selektywność obok szczytu) i wolno opadające listki boczne
(energia i szum wyciekają daleko). Okno KBD ma szerszy listek główny i listki boczne, które
opadają znacznie szybciej. W czasie ta sama właściwość oznacza, że KBD trzyma szum
kwantyzacji bliżej miejsca, gdzie powinien być. Fabryczny FDK używa okna sinusowego dla
bloków krótkich, czyli dokładnie dla bloków wokół ataków.

Ten wybór kosztuje jeden bit na ramkę i jest legalny wszędzie w LC; strumień niesie kształt
faktycznie użyty, także w blokach krótkich, i każdy dekoder go stosuje.

Wartości:
- `1`: KBD w każdym bloku. Zmierzone: pre-echo niższe o 3,4..6,1 dB średnio i o 1,2..6,6 dB
  w najgorszym przypadku przy 24, 40 i 128 kbps. Najlepszy pojedynczy przełącznik przeciw
  pre-echu w LC.
- `0`: sinusowe w każdym bloku. Lepsza selektywność obok mocnych szczytów tonalnych w blokach
  długich, więcej wycieku; głównie ustawienie do porównania.

Przykład w kontekście: `-p 2 -b 40000 -w 18000 --window-shape 1`.

### `--short-groups <0..3>`

**Wybiera, jak grupowanych jest osiem krótkich okien ramki transjentowej.**

- Dotyczy: LC i rdzenia HE-AAC, ramek kodowanych jako bloki krótkie.
- Wartości: `0`..`3`; domyślnie: wyłączone, czyli zachowanie fabryczne (tabela, którą FDK
  wybiera według pozycji ataku)
- Wpisywanie: liczba całkowita

Ramka transjentowa składa się z ośmiu krótkich okien. Okna w tej samej grupie dzielą jeden
zestaw współczynników skali, więc mniej grup oznacza mniej informacji pobocznej i więcej
bitów na widmo, ale grubszą kontrolę szumu w czasie. Więcej grup oznacza, że szum można
kształtować okno po oknie, za odpowiednią cenę.

Wartości:
- `0`: tabela fabryczna (to samo co pozostawienie przełącznika wyłączonego).
- `1`: jedna grupa na wszystkie osiem okien. Najtaniej; szum rozkłada się na całą ramkę, co
  częściowo odkręca to, po co były bloki krótkie.
- `2`: cztery pary. Równa rozdzielczość czasowa w całej ramce. Zmierzone: najmniej dziur w
  ramkach krótkich na materiale testowym.
- `3`: ciasno wokół ataku: samo okno ataku, po jednym oknie z każdej jego strony, reszta
  zbita razem. Najwięcej bitów tam, gdzie jest atak.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --short-groups 2`.

### `--tns-thresh <x>`

**Zmienia, jak chętnie włącza się TNS.**

- Dotyczy: każdego profilu z rdzeniem: LC, rdzenia HE-AAC, LD, ELD.
- Wartości: 0.5..4.0, mnożnik; domyślnie: wyłączone, czyli zachowanie fabryczne (1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

TNS (kształtowanie szumu w czasie) prowadzi filtr predykcyjny w poprzek częstotliwości. Tam,
gdzie sygnał ma mocną obwiednię czasową, na przykład atak wewnątrz bloku, filtr sprawia, że
szum kwantyzacji idzie za tą obwiednią w czasie, więc szum siedzi pod głośną częścią zamiast
przed nią. FDK włącza TNS w ramce tylko wtedy, gdy zysk predykcji jest dość wysoki, żeby
opłacić informację poboczną. Ten przełącznik mnoży wymagany zysk. Poniżej 1 TNS jest używany
w większej liczbie ramek; powyżej 1 w mniejszej.

W LC bloki krótkie obsługują już najostrzejsze ataki, więc TNS zajmuje się tymi
łagodniejszymi. W LD i ELD, które nie mają bloków krótkich, TNS jest głównym narzędziem
przeciw pre-echu, a niższy próg warto tam często wypróbować w pierwszej kolejności.

Przykłady, więcej TNS:
- Łagodnie: `--tns-thresh 0,85`.
- Mocno: `--tns-thresh 0.7`. Zmierzone: w LC przy niskim bitrate dodatkowa informacja
  poboczna zostawia więcej dziur.
- Przesadnie: `--tns-thresh 0.5`. TNS włączony prawie wszędzie, także tam, gdzie nie ma czego
  kształtować; bity idą na współczynniki filtru.

Przykłady, mniej TNS:
- Łagodnie: `--tns-thresh 1,3`.
- Mocno: `--tns-thresh 2`. Mniej bitów na informację poboczną, nieco mniej dziur, więcej
  pre-echa na miękkich atakach.
- Przesadnie: `--tns-thresh 4`. TNS praktycznie wyłączony.

Współpraca i konflikty: `--tns-mask` i `--tns-order` (grupa D) decydują, czy i jak duży może
być filtr; ten przełącznik decyduje, jak często się to opłaca.

### `--tns-start <Hz>`

**Ustawia najniższą częstotliwość, którą obejmuje dolny filtr TNS.**

- Dotyczy: AAC-LC i rdzenia LC w HE-AAC, bloków długich i krótkich. Zmierzony brak efektu
  w LD i ELD.
- Wartości: 0..24000 Hz; domyślnie: wyłączone, czyli zachowanie fabryczne (8 pasm SFB,
  około 1,4 kHz przy 48 kHz)
- Wpisywanie: liczba całkowita

Fabrycznie najniższe pasma zostają poza TNS w blokach długich, a w krótkich zakres zaczyna
się od 0 Hz. Wartość jest wyrównywana do najbliższej krawędzi pasma w każdej z tablic i nie
może przekroczyć górnej granicy zakresu TNS. Do 1.4.0-beta.1 bloki krótkie ją pomijały.

Przykłady, sięganie niżej:
- Łagodnie: `--tns-start 1000`.
- Mocno: `--tns-start 400`. TNS kształtuje też szum dolnej średnicy, co przydaje się na
  fortepianie i szarpanych dźwiękach basu.
- Przesadnie: `--tns-start 0`. TNS obejmuje widmo od samego dołu, łącznie z pasmami basu,
  gdzie jego korzyść uboczna jest mała, a jego błędy mogą objawiać się jako
  niskoczęstotliwościowe "oddychanie".

Przykłady, start wyżej:
- Łagodnie: `--tns-start 2000`.
- Mocno: `--tns-start 5000`.
- Przesadnie: `--tns-start 15000`. Kształtowana jest tylko sama góra; dla większości materiału
  TNS jest wtedy w blokach długich praktycznie wyłączony.

### `--coverage <-5..5>`

**Jedna oś przy stałym bitrate: koduj każde pasmo grubo albo mniej pasm czysto.**

- Dotyczy: rdzeń AAC (LC, LD, ELD i rdzeń LC w HE-AAC). Domyślnie wyłączone.
- Wartości: liczby całkowite `-5..5`; `0` = fabryczne
- Wpisywanie: liczba całkowita

Przy stałym bitrate koder może wydać bity na pokrycie całego widma grubym opisem albo na
dobry opis mniejszej liczby pasm, zostawiając dziury. To makro przesuwa tę równowagę jedną
liczbą. Ustawia sześć przełączników poniżej (`--band-knee`, `--quant-round`, `--thr-hold`,
`--scf-range`, a przy `+4` i `+5` także `--pns-above`); każdy z nich podany jawnie wygrywa
z makrem. `--verbose` pokazuje wartości, które faktycznie działają.

Zmierzone przy LC 32 kbps, 17 kHz, po dekodowaniu (wyzerowane pasma i szum pasm, które
zostały): `-5` 51% dziur, a zachowane pasma 4,75 dB czystsze od sygnału; fabrycznie 11,7%
dziur, 1,96 dB; `+4` 0,2% dziur, a zachowane pasma mniej więcej tak szumne jak treść.

Przykłady:
- Łagodnie, więcej pokrycia: `--coverage 2`. Mniej dziur, trochę więcej ziarna.
- Mocno, więcej pokrycia: `--coverage 4`. Praktycznie bez dziur; dokłada podstawianie szumu od 11 kHz.
- Przesadnie, więcej pokrycia: `--coverage 5`. Wszystko wypełnione, szum od 7 kHz.
- Łagodnie, więcej precyzji: `--coverage -2`. Najpierw odchodzą najsłabsze pasma.
- Mocno, więcej precyzji: `--coverage -4`. Dużo dziur, to, co zostaje, jest czyste.
- Przesadnie, więcej precyzji: `--coverage -5`. Znika mniej więcej połowa pasm.

### `--band-knee <dB>`

**Przesuwa zapas każdego pasma nad progiem przy krawędzi kodowane/wyzerowane, we wszystkich kanałach.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone.
- Wartości: `-24.0..24.0` dB, `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Wartość dodatnia zatrzymuje pasma do N dB pod progiem, na najgrubszym kroku, zamiast je
zerować. Wartość ujemna zeruje pasma, które przechodzą próg o mniej niż N dB po pierwszym
oszacowaniu bitów, więc drugie oszacowanie oddaje te bity reszcie. Pasm PNS i IS nie rusza;
w pasmach side pary MS wygrywa `--side-knee`.

Przykłady:
- Łagodnie, więcej zatrzymanych: `--band-knee 3`.
- Mocno, więcej zatrzymanych: `--band-knee 6`. Użyteczne maksimum: powyżej około 9 dB uratowane
  grube pasma kosztują tyle bitów, że pętla tnie gdzie indziej.
- Przesadnie, więcej zatrzymanych: `--band-knee 24`. Zmierzone: gorzej niż 6.
- Łagodnie, więcej wyrzuconych: `--band-knee -2`.
- Mocno, więcej wyrzuconych: `--band-knee -6`.
- Przesadnie, więcej wyrzuconych: `--band-knee -24`. Zostają tylko wyraźnie słyszalne pasma.

### `--quant-round <x>`

**Przesuwa punkt zaokrąglania kwantyzatora: mniej albo więcej małych linii widma staje się zerem.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (fabrycznie 0,405).
- Wartości: `0.0..0.5`
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kwantyzator zaokrągla każdą linię widma do całego kroku. Fabrycznie zaokrągla z
przesunięciem około 0,405, czyli lekko w stronę zera, co oszczędza bity. `0.5` zaokrągla do
najbliższej: mniej małych linii znika, kosztuje to bity. Niższe wartości wysyłają więcej
linii do zera i są tańsze.

Przykłady:
- Łagodnie, dokładniej: `--quant-round 0.45`.
- Mocno, dokładniej: `--quant-round 0.5`. Zaokrąglanie do najbliższej.
- Łagodnie, taniej: `--quant-round 0.35`.
- Mocno, taniej: `--quant-round 0.25`. Ciche szczegóły znikają.
- Przesadnie, taniej: `--quant-round 0.0`. Obcinanie: każda linia zaokrąglona w dół.

### `--pns-above <Hz>`

**Od tej częstotliwości w górę zastępuje każde pasmo bloku długiego szumem o właściwym poziomie, bez względu na tonalność.**

- Dotyczy: AAC-LC, bloki długie. Domyślnie wyłączone.
- Wartości: `1000..48000` Hz
- Wpisywanie: liczba całkowita

Górne pasmo „jak SBR, bez SBR”: pasmo zachowuje poziom, a jego treść staje się szumem.
Włącza PNS nawet poniżej jego warunku bitrate'u; `--pns 0` nadal wygrywa. Ciche pasma
zostają ciche. Przy niskim bitrate należy używać razem z `--pns-budget 1` i
`--pns-shared 1`, inaczej ramkom może zabraknąć bitów i pojawi się trzask (patrz
`--pns-budget`).

Przykłady:
- Łagodnie: `--pns-above 11000`. Szumem staje się tylko sama góra.
- Mocno: `--pns-above 5000`.
- Przesadnie: `--pns-above 2200`. Większość obszaru prezencji to szum.

### `--thr-hold <n>`

**Mierzy model maskowania tylko co n ramek długich.**

- Dotyczy: rdzeń AAC, bloki długie. Domyślnie wyłączone.
- Wartości: `1..32` ramki, `1` = wyłączone
- Wpisywanie: liczba całkowita

Pomiędzy pomiarami każde pasmo zachowuje ostatni stosunek szumu do sygnału, więc szum
spóźnia się za muzyką. Blok krótki zeruje to trzymanie. Efekt jest słaby: strumień się
zmienia, ale zmierzona jakość rusza się o mniej niż 1 dB nawet przy 32, bo FDK i tak pracuje
na wygładzonych energiach.

Przykłady:
- Łagodnie: `--thr-hold 2`.
- Mocno: `--thr-hold 8`.
- Przesadnie: `--thr-hold 32`.

### `--scf-range <n>`

**Ogranicza, jak daleko od siebie mogą być najdrobniej i najgrubiej kodowane pasmo kanału.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (fabrycznie 60).
- Wartości: `0..60`, kroki po 1,5 dB
- Wpisywanie: liczba całkowita

Niższe wartości robią szum kwantyzacji bardziej płaskim w częstotliwości, z mniejszym
kształtowaniem psychoakustycznym. Na zwykłym materiale efekt jest bliski zera; widać go przy
małych wartościach na materiale z silnym nachyleniem widma (szum 6 do 14 kHz z 12,4 do 7,1
dB pod sygnałem przy `0`, 64 kbps bez PNS). Przy 14 kbps nie zmienia budżetu.

Przykłady:
- Łagodnie: `--scf-range 40`.
- Mocno: `--scf-range 20`.
- Przesadnie: `--scf-range 0`. Jeden krok na cały kanał.

### `--bit-stats`

**Liczy, na co idą bity zapisanych ramek, i wypisuje tabelę na końcu.**

- Dotyczy: każdy profil. Domyślnie wyłączone.
- Wartości: brak, to flaga

Tabela dzieli ramki na nagłówek transportu, nagłówki elementów, maskę MS, dane sekcji,
współczynniki skali, poziomy szumu PNS, TNS, widmo każdego kanału (mid i side dla MS),
SBR/PS i wypełnienie, a do tego kodowane, wyzerowane, PNS i IS pasma na ramkę. Liczy bity
naprawdę zapisane; strumień jest identyczny z przełącznikiem i bez niego, a suma zgadza się
z plikiem ADTS.

Przykład: `fdkaac -p 2 -b 14000 --bit-stats -f 2 -o out.aac in.wav`

### `--enfac-patch <0|1>`

**Wyłącza przeważanie energii płaskich, szumowych ramek długich albo włącza je dla każdej ramki.**

- Dotyczy: rdzeń AAC, bloki długie, CBR i VBR. Domyślnie wyłączone (decyduje fabryczny FDK).
- Wartości: `0` = nigdy, `1` = każda ramka długa

Gdy słyszalne widmo ramki jest płaskie, fabryczny FDK przeważa energie pasm (potęgi 3/4, 1/2
albo 1/4) i w ten sposób przesuwa bity między pasmami. `0` traktuje każdą ramkę jak tonalną,
`1` stosuje przeważanie do każdej ramki długiej. W CBR to drobne pokrętło: wyzerowane pasma
zmieniają się najwyżej o 1 punkt, a szum o mniej niż 0,1 dB przy 14 do 32 kbps; `0` na
materiale tonalnym często daje to samo co ustawienie domyślne. W VBR (`-m 3`) `1`
powiększyło plik z tonem i szumem o 29%, a `0` zmniejszyło plik z gęstym szumem o 3%.

## F. Przełączanie bloków i kontrola bitrate'u

Sześć przełączników: jeden, który przechyla decyzję o bloku długim albo krótkim, i pięć,
które kontrolują, jak swobodnie bitrate może się ruszać z ramki na ramkę. Te pięć od
bitrate'u ma sens tylko przy znajomości silnika pod spodem, więc od tego zaczynamy.

**Silnik CBR.** Cel wybiera się przez `-b`, a koder mierzy w niego jako w średnią. Poszczególne
ramki nie są wymuszane do tego rozmiaru. Przy 48 kHz ramka AAC-LC obejmuje 1024 próbki, czyli
około 21,3 ms, więc cel 128 kbps stereo wychodzi na z grubsza 2730 bitów na ramkę średnio.
To ta liczba, do której steruje pętla dopasowania bitrate'u.

**Rezerwuar bitów.** Nawet przy CBR między ramkami a celem siedzi bufor. Kiedy ramka jest
łatwa, koder koduje ją oszczędnie i wkłada niewydane bity do rezerwuaru. Kiedy przychodzi
trudna ramka, pożycza z rezerwuaru i wydaje więcej, niż mu się należy. Średnia zostaje
zachowana, a chwilowy bitrate rusza się w górę i w dół, i dlatego CBR w AAC nigdy nie jest
idealnie płaski. Rozmiar rezerwuaru decyduje, jak daleko i jak długo koder może zbaczać.

**Twardy sufit.** Ramka AAC pomieści najwyżej 6144 bity na kanał, czyli 12288 bitów dla
stereo. Nic w tym koderze tego nie podnosi; to ograniczenie formatu. Każde ustawienie
rezerwuaru i szczytu jest o nie przycinane, i to też dlatego dostępne tutaj wahania są
umiarkowane w porównaniu z prawdziwym koderem o zmiennym bitrate.

**Quasi-VBR.** Problem, którym zajmują się te przełączniki, jest konkretny i prawdopodobnie
go słychać: przychodzi trudny fragment, koder wypuszcza kilka ramek dobrze powyżej średniej,
a potem dusi następne ramki, żeby natychmiast spłacić dług. Wynikiem jest słyszalne
przyciemnienie zaraz po głośnym momencie. Rozszerzenie rezerwuaru i dopuszczenie
krótkoterminowego szczytu pozwala koderowi trzymać podniesioną jakość dłużej i zamiast tego
zejść łagodnie. To nadal silnik CBR z większym buforem, a nie przydział na podstawie treści
jak w LAME, więc efekt jest lżejszy, niż sugeruje nazwa VBR.

Uwaga o dwóch twardych limitach na ramkę: nie należy ustawiać `--max-bits-frame` poniżej
średniej ani `--min-bits-frame` powyżej niej. Oba walczą wtedy z celem, w który pętla
dopasowania bitrate'u próbuje trafić, a wynikiem jest gorsza jakość, nie inny kompromis.
`--verbose` wypisuje blok budżetu rezerwuaru, w tym średnią liczbę bitów na ramkę i sufity,
i stamtąd pochodzą liczby do ustawienia tych przełączników.

### `--block-bias <n>`

**Ustawia udział bloków krótkich, od samych długich (0) do samych krótkich (255). Strumień
jest zgodny ze standardem przy każdej wartości.**

- Dotyczy: AAC-LC i rdzenia LC w HE-AAC, które mają bloki krótkie. LD i ELD ich nie mają
  (LD przełącza się zamiast tego na okno o małym nakładaniu, także przesuwane tym
  przełącznikiem; ELD w ogóle nie przełącza okien).
- Wartości: `0..255`, `128` to wartość fabryczna; domyślnie: `-1`, wyłączone (ustawienie fabryczne)
- Wpisywanie: liczba całkowita

Blok długi daje dobrą rozdzielczość częstotliwościową i złą czasową; blok krótki odwrotnie.
Detektor ataku decyduje ramka po ramce: porównuje energię każdej ósmej części ramki z
bieżącą energią przed nią i uznaje atak powyżej stosunku 10. Na muzyce ten stosunek wynosi
1,2 do 1,5 w zwykłej ramce i przekracza 10 tylko w około 2..3% ramek.

Ten przełącznik przesuwa ten próg według tablicy skalibrowanej na samym koderze (sześć
fragmentów: akustyczne stereo, eurodance, wokal, perkusja, 64 kbps, udział ramek krótkich
odczytany ze strumienia), tak żeby udział ramek krótkich szedł za suwakiem równo:
- `0`: tylko bloki długie, atak nie jest wykrywany nigdy.
- `1..127`: mniej ramek krótkich niż fabrycznie; przy `64` około połowy udziału fabrycznego.
- `128`: zachowanie fabryczne, bit w bit.
- `129..240`: więcej ramek krótkich, na muzyce około 12% ramek na 16 kroków; w okolicy `192`
  mniej więcej połowa ramek, przy `240` około 80..99%.
- `241..254`: na krótkie idą też ciche fragmenty (obniżona minimalna energia detektora).
- `255`: tylko bloki krótkie, każda ramka, łącznie z ciszą.

Materiał bez transjentów reaguje później niż muzyka: stały szum różowy i sinus zostają
długie mniej więcej do 208 i przechodzą na krótkie między 216 a 236, bo ich stosunek zostaje
blisko 1. Materiał z samymi mocnymi uderzeniami (ścieżka perkusji) ma swoje uderzenia przy
każdej wartości, więc jego udział rusza się mniej. Do 1.4.0-beta.1 przełącznik skalował próg
liniowo: powyżej 128 próg szedł tylko z 10 do 5 (6,5% ramek krótkich przy 255 na eurodance),
a tuż poniżej 128 przeskakiwał większość prawdziwych ataków, dlatego środek wydawał się nic
nie robić.

Zmienia się tylko decyzja o ataku. Sekwencję okien dalej budują własne przejścia kodera
(długie, start, krótkie, stop, długie), a oba kanały są synchronizowane jak w fabrycznym
FDK, więc dekoder zawsze dostaje legalną sekwencję. Sprawdzone: 300 kodowań (od 0 do 255,
LC, HE-AAC, HE-AAC v2, grupowanie krótkich 1 i 3, cisza, sinus, muzyka), 0 nielegalnych
przejść okien i 0 błędów dekodera. `--verbose` pokazuje wartość.

Przykłady (eurodance, 64 kbps; udział ramek krótkich):
- `--block-bias 64`: 0,1% zamiast 1,6%. Materiał stały zostaje długi, mocne uderzenia nadal
  idą na krótkie.
- `--block-bias 160`: 23%. Każdy większy wzrost poziomu dostaje bloki krótkie.
- `--block-bias 192`: 57%.
- `--block-bias 224`: 87%.
- `--block-bias 255`: 100%. Materiał tonalny brzmi szorstko, pokaz tego, po co są bloki
  długie.
- `--block-bias 0`: 0%. Każdy transjent niesie blok długi, pre-echo staje się głównym
  artefaktem na materiale perkusyjnym.

Współpraca i konflikty: przy `--block-bias 0` `--preecho-rise`, `--preecho-depth` i
`--tns-thresh` z grupy E zostają jedynymi narzędziami przeciw pre-echu, czyli sytuacja, w
jakiej LD i ELD są zawsze. `--short-groups`, `--window-shape` i `--pns-short` mają znaczenie
tylko tam, gdzie występują bloki krótkie. `--attack-decay` i `--attack-floor` zmieniają sam
detektor; łączą się z tym przełącznikiem.

### `--vbr-reservoir <bits>`

**Ustawia rozmiar rezerwuaru bitów, który decyduje, jak daleko i jak długo ramka może zbaczać
od średniej.**

- Dotyczy: każdego profilu, w trybie CBR. To główne pokrętło quasi-VBR.
- Wartości: bity, od `1` do `6144 * liczba kanałów` minus średnia liczba bitów na ramkę, i
  tam automatycznie przycinane; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Rezerwuar to bufor, z którego koder pożycza i do którego wpłaca. Jego rozmiar jest pamięcią
systemu: mały rezerwuar oznacza, że każde przekroczenie musi być spłacone niemal
natychmiast, co daje przyciemnienie zaraz po kulminacji; duży oznacza, że koder może trzymać
się powyżej średniej przez kilka ramek, a potem wracać stopniowo.

Większy jest zatem kierunkiem dla materiału, który powinien żyć dalej po trudnym fragmencie.
Mniejszy trzyma średnią ciaśniej z chwili na chwilę i jest właściwy, gdy łącze transportowe
ma wąski budżet chwilowy. Dobry punkt wyjścia to dwa do trzech razy tyle, ile `--verbose`
raportuje jako wartość domyślną.

Dwa zachowania, o których warto wiedzieć. `0` jest traktowane jako nieustawione i nie robi
nic, więc do ciasnego trzymania średniej służy mała wartość dodatnia albo
`--bitres-mode 2`. A każda wartość równa sufitowi albo większa, czyli `6144 * liczba kanałów`
minus średnia liczba bitów na ramkę, zapada się do sufitu, więc nie ma sposobu, by przesadzić,
ani korzyści z próbowania; proszenie o 100000 bitów na pliku stereo daje ten sam strumień, co
proszenie dokładnie o sufit.

Liczby pochodzą z `--verbose`, który wypisuje średnią liczbę bitów na ramkę i sufity w swoim
bloku budżetu rezerwuaru. Ustawianie tego przełącznika na zgadywanie to sposób, w jaki
ludzie przypadkiem kończą na sufice.

Przykłady:
- Łagodnie, więcej oddechu: `--vbr-reservoir 4000`. Nieco szersze wahania przy 128 kbps
  stereo; koder dochodzi do siebie po szczycie przez kilka ramek, a nie przez jedną.
- Mocno, więcej oddechu: `--vbr-reservoir 12000`. Praktyczne ustawienie quasi-VBR przy
  128 kbps stereo. Kulminacje trzymają swoją jakość, a powrót do średniej jest stopniowy.
- Przesadnie, więcej oddechu: `--vbr-reservoir 100000`. Dobrze powyżej sufitu, więc zapada
  się do sufitu. Koder używa maksymalnego oddechu, jaki format pozwala, a to ustawienie
  skrajne jest pouczające właśnie dlatego, że pokazuje, jak skromne jest to maksimum: lot AAC
  ogranicza limit 6144 bitów na kanał, a nie rezerwuar.
- Łagodnie, ciaśniej: `--vbr-reservoir 2000`. Poniżej zwykłej wartości domyślnej. Wahania
  się zwężają, średnia jest trzymana bliżej.
- Mocno, ciaśniej: `--vbr-reservoir 500`. Bardzo mało buforu. Trudne ramki nie mogą dużo
  pożyczyć, więc trudne fragmenty są kodowane zgrubnie, a nie drogo.
- Przesadnie, ciaśniej: `--vbr-reservoir 1`. Praktycznie brak rezerwuaru. Każda ramka musi
  zmieścić się we własnym przydziale, więc transjenty i gęste fragmenty nie mają gdzie się
  podziać, co brzmi jak gorsza wersja tego, co `--bitres-mode 2` robi celowo.

Współpraca i konflikty: łączy się w parę z `--peak-bitrate`, który podnosi krótkoterminowy
sufit, o jaki rezerwuar może się odbijać; rezerwuar bez podniesionego szczytu ma mniej
miejsca na działanie. `--bitres-mode` może zmniejszyć albo wyłączyć rezerwuar niezależnie od
rozmiaru ustawionego tutaj. `--max-bits-frame` ogranicza poszczególne ramki po wszystkim
tym, więc ciasne ograniczenie czyni duży rezerwuar bezsensownym. Przy `--unlock-bitrate`
rezerwuar staje się ważniejszy, bo przekraczanie jest tam normalnym przypadkiem.

Zmierzone: `0` jest traktowane jako nieustawione i nie daje żadnego efektu. Wartości równe
`6144 * liczba kanałów` minus średnia liczba bitów na ramkę albo większe zapadają się do
sufitu. `--verbose` wypisuje blok budżetu rezerwuaru, który podaje obie liczby.

### `--peak-bitrate <bps>`

**Dopuszcza krótkoterminowe szczyty bitrate'u do podanej wartości, przy czym średnia nadal
jest trzymana na celu z `-b`.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity na sekundę, powyżej celu z `-b`; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Rezerwuar decyduje, jak długo ramka może zbaczać; ten przełącznik decyduje, jak wysoko może
pójść. Ustawiony powyżej celu pozwala koderowi na chwilowe osiągnięcie tego bitrate'u przez
trudne ramki, podczas gdy pętla dopasowania bitrate'u dalej steruje średnią do `-b`.

Poniżej celu jest ignorowany, i to celowo: sufit szczytowy pod średnią nie ma sensu i koder
nie działa na tej sprzeczności. Użyteczny zakres jest więc tylko powyżej `-b`, a naturalnym
sposobem wyrażenia tego jest krotność: półtora raza cel jest ostrożne, dwa razy jest hojne.

Sufit 6144 bitów na kanał nadal obowiązuje, więc proszenie o szczyt poza to, co ramka
fizycznie pomieści, daje limit ramki. Przy 48 kHz stereo to ramka warta około 576 kbps (6144
x 2 bity x 48000 / 1024), więc przy zwykłych celach jest dużo miejsca, zanim limit formatu
zacznie wiązać.

Przykłady:
- Łagodnie, wyższy szczyt: `-b 128000 --peak-bitrate 150000`. Niewielki zapas. Kulminacje
  wychodzą nieco mniej sprasowane.
- Mocno, wyższy szczyt: `-b 128000 --peak-bitrate 200000`. Praktyczne ustawienie, zwykle w
  parze z szerokim rezerwuarem. Trudne ramki sięgają dobrze powyżej celu, a średnia jest
  utrzymana.
- Przesadnie, wyższy szczyt: `-b 128000 --peak-bitrate 1100000`. Blisko limitu ramki w
  formacie. Koder może wydać na jedną ramkę prawie całą fizyczną pojemność ramki, po czym pętla
  dopasowania bitrate'u musi wszystko to odebrać, a wynikiem jest dramatyczna wersja dokładnie
  tego przyciemnienia, któremu ustawienia quasi-VBR mają zapobiegać.
- W dół nie ma użytecznego kierunku: każda wartość równa celowi z `-b` albo mniejsza jest
  ignorowana, więc `--peak-bitrate 100000` przy `-b 128000` zachowuje się tak, jakby
  przełącznika nie było.

Współpraca i konflikty: naturalny towarzysz `--vbr-reservoir`; oba są potrzebne do zachowania
"nie duś się po szczycie", bo jeden daje wysokość, a drugi czas. `--max-bits-frame` jest
stosowany po tym przełączniku, więc wyraźne ograniczenie ramki wygrywa nad szczytem.
`--bitres-mode 2` czyni go bez znaczenia, usuwając rezerwuar, z którego szczyt miałby być
czerpany.

Zmierzone: konfiguracja z podpowiedzi w `--help` dla quasi-VBR przy około 128 kbps stereo to
`-b 128000 --peak-bitrate 200000 --vbr-reservoir 12000`.

### `--max-bits-frame <bits>`

**Twardy sufit na liczbę bitów, jakich może użyć pojedyncza ramka.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity, co najmniej średnia liczba bitów na ramkę i najwyżej `6144 * liczba
  kanałów`; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

To tępe narzędzie tej grupy. Tam gdzie `--peak-bitrate` wyraża bitrate, który silnik może
osiągnąć, ten wyraża bezwzględne ograniczenie na ramkę, stosowane później i wygrywające. Gdy
podane są oba, wiąże wyraźne ograniczenie.

To ograniczenie, a nie pokrętło jakości, i jego głównym zastosowaniem jest spełnienie
zewnętrznego wymagania: transportu, który nie uniesie ramki powyżej pewnego rozmiaru, albo
testu, który potrzebuje ograniczonej ramki maksymalnej. Sensowne ograniczenie to około
półtora raza średnia, co nadal dopuszcza prawdziwy oddech, ograniczając najgorszy przypadek.

Przy zbyt niskim ustawieniu głośne ramki zostają „zagłodzone”, co jest słyszalne dokładnie
jako to przyciemnienie, którego zwykle chce się uniknąć. Poniżej średniej walczy z celem
wprost i koder nie może jednocześnie uszanować ograniczenia i trafić w żądany bitrate.

Przykłady:
- Łagodnie, wyższe ograniczenie: przy 128 kbps stereo (średnio około 2730 bitów na ramkę),
  `--max-bits-frame 8000`. Dobrze powyżej czegokolwiek, co silnik normalnie wyprodukuje, więc
  w praktyce siatka bezpieczeństwa.
- Mocno, wyższe ograniczenie: `--max-bits-frame 12288`. Maksimum formatu dla stereo.
  Ograniczenie jest obecne, ale nigdy nie wiąże, co jest właściwym ustawieniem, gdy celem jest
  udokumentowanie przełącznika w wierszu poleceń bez zmiany zachowania.
- Przesadnie, wyższe ograniczenie: `--max-bits-frame 100000`. Powyżej limitu formatu, więc
  koder bierze mniejszą z tej wartości i własnego maksimum rezerwuaru (`6144 * liczba kanałów`),
  więc w stereo strumień jest identyczny jak przy `12288` (zmierzone: 12288, 20000 i 100000 dają
  ten sam strumień ADTS). Nieszkodliwe i przydatne jako przypomnienie, że żaden przełącznik
  tutaj nie podnosi limitu ramki.
- Łagodnie, niższe ograniczenie: `--max-bits-frame 4000` przy 128 kbps stereo. Szczyty
  przycięte, średnia nietknięta; blisko użytecznej wskazówki 1,5x.
- Mocno, niższe ograniczenie: `--max-bits-frame 3000` przy tym samym celu. Głośne ramki są
  wyraźnie „zagłodzone”, bo ograniczenie siedzi tylko trochę powyżej średniej.
- Przesadnie, niższe ograniczenie: `--max-bits-frame 2000` przy 128 kbps stereo, czyli poniżej
  średniej. Ograniczenie i cel przeczą sobie teraz w każdej ramce; jakość spada na całej linii
  i nic się nie zyskuje, i dlatego reguła mówi, żeby zostać na średniej albo powyżej.

Współpraca i konflikty: stosowany po `--peak-bitrate`, więc gdy podane są oba, wygrywa
wyraźne ograniczenie. Ciasne ograniczenie czyni duży `--vbr-reservoir` bezsensownym, bo bity
rezerwuaru nie mają gdzie zostać wydane. Przed wyborem wartości warto odczytać średnią
liczbę bitów na ramkę z bloku budżetu rezerwuaru w `--verbose`.

Zmierzone: AAC-LC 128 kbps na attacks48. Największa ramka fabrycznie to 3816 bitów ładunku. Przy
`--max-bits-frame 3000` największa staje się 2888; przy `4000` staje się 3440.

### `--min-bits-frame <bits>`

**Twarda dolna granica liczby bitów, jakich musi użyć każda ramka.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity, od `0` do średniej liczby bitów na ramkę; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Lustrzane odbicie sufitu i mniej użyteczna połowa. Dolna granica zmusza koder do wydania co
najmniej tyle na każdą ramkę, w tym na ramki, które naprawdę potrzebują mniej, takie jak
cisza albo trzymany cichy fragment. Te bity nie są oszczędzane na później, są wydawane, więc
dolna granica zamienia łatwe ramki w zmarnowany budżet.

Powodem jej istnienia jest transport o stałym bitrate i eksperymenty: część sprzętu dalej w
łańcuchu woli przewidywalną minimalną ramkę, a czasem warto zobaczyć, co robi kwantyzator,
gdy mu się każe wydać więcej, niż chce.

Bez takiego powodu wartość powinna zostać na `0`. Powyżej średniej walczy z celem tak samo,
jak zbyt niski sufit, i koder nie może uszanować obu.

Przykłady:
- Łagodnie, wyższa granica: `--min-bits-frame 1000` przy 128 kbps stereo. Ciche i bardzo łatwe
  ramki zostają dopełnione; średnią trzeba odrobić na trudniejszych ramkach.
- Mocno, wyższa granica: `--min-bits-frame 2000` przy tym samym celu, blisko średniej. Prawie
  każda ramka siedzi teraz na tej granicy, więc silnik traci większość zdolności do redystrybucji.
- Przesadnie, wyższa granica: `--min-bits-frame 2730` przy 128 kbps stereo, czyli dokładnie
  średnia. W połączeniu ze średnią jako celem jest to bliskie wymuszeniu sztywnego bitrate'u na
  ramkę, z tą różnicą, że `--bitres-mode 2` robi to porządnie i bez walki z pętlą dopasowania
  bitrate'u.
- Łagodnie, niższa granica: `--min-bits-frame 200`. Z trudem ograniczenie; łatwe ramki
  zostają tanie.
- Mocno, niższa granica: `--min-bits-frame 1`. Praktycznie brak dolnej granicy, ta sama intencja
  co pozostawienie przełącznika wyłączonego.
- Przesadnie, niższa granica: `--min-bits-frame 0`. Udokumentowana wartość wyłączająca dolną
  granicę; identyczna w efekcie z nieustawieniem jej i ustawienie, do którego wraca się po
  zakończeniu eksperymentu.

Współpraca i konflikty: przeciwny koniec `--max-bits-frame`, a oba omawia się zwykle razem
jako VBR z ograniczeniami. Wysoka dolna granica marnuje bity, które `--vbr-reservoir` ma
redystrybuować, więc te dwa działają przeciw sobie. `--bitres-mode 2` to właściwy sposób
uzyskania sztywnego zachowania na ramkę.

Zmierzone: AAC-LC 128 kbps na attacks48. Wyjście fabryczne 97056 bajtów; przy
`--min-bits-frame 1000` 97111 bajtów; przy `2900` 97709 bajtów.

### `--bitres-mode <n>`

**Wybiera, jak duża część mechanizmu rezerwuaru bitów jest aktywna, od pełnego oddechu do
sztywnego bitrate'u na ramkę.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: `0` pełny, `1` zredukowany, `2` wyłączony; domyślnie: `-1`, decyzja kodera
- Wpisywanie: liczba całkowita

Tam gdzie `--vbr-reservoir` ustawia rozmiar bufora, ten wybiera politykę. Pełny to normalne
zachowanie, takie samo jak fabryczne. Zredukowany zwęża działanie rezerwuaru. Wyłączony go
usuwa, więc każda ramka musi zmieścić się we własnym przydziale budżetu, a to najbliższa
rzecz do sztywnego, stałego bitrate'u ramka po ramce, jaką ten koder osiąga.

Jedynym wyraźnie użytecznym ustawieniem jest `2`, a przypadkiem użycia jest transport albo
test, który potrzebuje, by każda ramka miała około tego samego rozmiaru. Koszt jest realny:
transjent albo gęsty fragment nie może pożyczyć, więc zostaje zakodowany zgrubnie.

Wartości:
- `-1`: decyzja należy do kodera (domyślnie).
- `0`: pełny rezerwuar, to samo co zachowanie fabryczne.
- `1`: zredukowany rezerwuar. Na materiale testowym dało to ten sam strumień co `2`, więc różnicę
  między tymi dwoma należy traktować jako nieprzetestowaną, a nie ustaloną.
- `2`: rezerwuar wyłączony. Ramki stają się niemal stałe w rozmiarze, a trudne fragmenty tracą
  swoją drogę ucieczki.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --bitres-mode 2 -o rigid.m4a in.wav

Współpraca i konflikty: `2` czyni `--vbr-reservoir` i `--peak-bitrate` bez znaczenia, bo nie
ma rezerwuaru, na którym mogłyby działać. `0` to tryb, w którym należy używać reszty grupy
F.

Zmierzone: AAC-LC 128 kbps na attacks48. `0` daje strumień fabryczny; `1` i `2` dały ten sam
strumień, więc na tym materiale nie zaobserwowaliśmy różnicy między zredukowanym a
wyłączonym.

## G. Wyjście DAB+

Dwa przełączniki i raczej tryb niż poprawka. Wszędzie indziej w tym opisie koder robi plik
albo goły strumień AAC; przy `--dab` robi strumień superramek DAB+ dla naziemnego radia
cyfrowego.

### `--dab`

**Wypuszcza strumień superramek DAB+ zgodny z ETSI TS 102 563 zamiast pliku albo zwykłego
strumienia AAC.**

- Dotyczy: AAC-LC, HE-AAC i HE-AAC v2, mono albo stereo, przy 32000 albo 48000 Hz. Zastępuje
  normalną ścieżkę wyjścia całkowicie, więc `-f` i opcje kontenera nie mają zastosowania.
- Wartości: flaga; domyślnie: wyłączone (normalne wyjście do pliku albo strumienia)
- Wpisywanie: flaga

DAB+ nie niesie po prostu AAC w innym kontenerze. Różnią się trzy rzeczy. Transformata ma
960 próbek, a nie zwykłe 1024, żeby długość ramki pasowała do siatki czasowej radia. Dźwięk
jest pakowany w superramki o dokładnie 120 ms. A skoro eter jest nieprzyjazny, z zanikami,
tunelami i zakłóceniami, strumień jest chroniony podwójnie: firecode, czyli Fire CRC,
pilnuje nagłówka, a Reed-Solomon RS(120,110) nad GF(256) chroni ładunek, więc odbiornik może
odbudować dane, które przyszły uszkodzone. Koder robi to wszystko.

Wyjściem jest surowy strumień `.dabp`, jedna superramka po drugiej i nic więcej. To nie jest
plik do odtwarzacza multimedialnego; to coś, co pobiera multiplekser DAB+. Zwykły łańcuch to
`out.dabp` do `odr-dabmux`, który łączy kilka stacji w jeden zespół i wypuszcza ETI, a stamtąd
do nadajnika albo do odbiornika programowego, takiego jak welle.io albo dablin, do testów.

Trzy ograniczenia, które narzuca standard, a koder ich pilnuje: częstotliwość próbkowania
musi być 32000 albo 48000 Hz, nic innego nie jest w DAB+ legalne; bitrate musi być
wielokrotnością 8 kbps między 8 a 192; mono i stereo oba są w porządku.

Konwencja bitrate'u jest tu własna. Przy `--dab` `-b` to kilobity na sekundę, a koder sam
mnoży przez 1000, więc `-b 96` oznacza 96 kbps. `--unlock-bitrate` tego nie zmienia:
kontrola DAB+ i mnożenie przez 1000 dzieją się wcześniej, więc
`--dab --unlock-bitrate -b 96` daje ten sam strumień co bez odblokowania, a `-b 96000`
zostaje odrzucone jako niewielokrotność 8 kbps.

Normalnie profilu nie wybiera się ręcznie. Praktyka DAB+, idąc za narzędziem referencyjnym
`odr-audioenc`, wyprowadza go z bitrate'u i liczby kanałów, tak by niskie bitrate'y
automatycznie dostawały oszczędniejsze narzędzia:

- stereo przy 48 kbps albo niżej, czyli rozmiar podkanału 6 lub mniej, daje HE-AAC v2 z
  parametric stereo;
- mono do 64 kbps albo stereo do 80 kbps daje HE-AAC z SBR;
- cokolwiek wyżej daje AAC-LC.

Zatem `-b 96` na pliku stereo trafia na AAC-LC, `-b 64` na HE-AAC, a `-b 32` na HE-AAC v2
automatycznie. Profil nadal można wymusić przez `-p` (`2` dla LC, `5` dla HE-AAC, `29` dla
HE-AAC v2), gdy wybór jest świadomy. Wymuszony profil daje bit w bit ten sam strumień, który
dobór automatyczny daje dla tego profilu. Inne wartości `-p` są odrzucane, tak samo `-p 29` na
wejściu mono. Wymuszony profil poniżej tablic strojenia FDK, na przykład `-p 2` przy 8 kbps albo
`-p 5` przy 8 kbps stereo, kończy się komunikatem: pomiń `-p` albo dodaj `--unlock-bitrate`.

Wartości:
- pominięte: normalne wyjście, a koder zachowuje się dokładnie jak fabryczny.
- `--dab`: wyjście superramek DAB+.

Przykłady w kontekście:

    fdkaac --dab -b 96 -o out.dabp input.wav     # auto AAC-LC
    fdkaac --dab -b 64 -o out.dabp input.wav     # auto HE-AAC
    fdkaac --dab -b 32 -o out.dabp input.wav     # auto HE-AAC v2

Współpraca i konflikty: `--dab-label` robi coś tylko w tym trybie. `-f` i przełączniki
kontenera MP4 nie mają zastosowania, bo nie ma kontenera. `--unlock-bitrate` zmienia sposób
odczytu `-b`. Przełączniki z grup od B do F nadal działają na rdzeń AAC wewnątrz superramek,
więc pokrętła stereo, SBR, szczegółu, transjentów, pokrycia i bitrate'u pozostają dostępne.
Częstotliwości podane w Hz są przeliczane na siatce 960 próbek, dlatego `--verbose` pokazuje
nieco inne granice pasm niż w strumieniu 1024 próbek. Zmierzone na muzyce i na teście
ataków, LC 48 i 96 kbps, HE-AAC stereo 64 kbps, HE-AAC mono 48 kbps i HE-AAC v2 32 kbps: 58
ustawień przełączników dodanych od v1.3.2 oraz przełączników pasmowych, każdy strumień
zdekodowany przez faad2 za dablinem bez błędu. Przełącznik, którego narzędzia nie ma w
wybranym profilu, nic nie robi, tak samo jak w zwykłym strumieniu: pokrętła SBR w AAC-LC,
pokrętła stereo w mono i w HE-AAC v2.

Zmierzone: strumienie dekodują się niezależnym dekoderem faad2 za dablinem, etykieta DLS
odczytuje się poprawnie, a wyjście AAC-LC jest identyczne bit w bit z tym, co z tego samego
wejścia produkuje referencyjny `odr-audioenc`. Przetestowaliśmy dziewięć kombinacji, 48 i 32
kHz przeciw mono i stereo w trzech profilach, każda niezależnie dekodowalna.

### `--dab-label <text>`

**Niesie statyczną etykietę stacji w superramce, pokazywaną na wyświetlaczu odbiornika DAB+.**

- Dotyczy: wyłącznie trybu `--dab`. Bez `--dab` etykieta jest ignorowana, bo nie ma superramki,
  w której można by ją umieścić.
- Wartości: tekst, do około 48 znaków; domyślnie: brak etykiety
- Wpisywanie: łańcuch tekstowy

Odbiornik DAB+ pokazuje tekst na swoim wyświetlaczu, zwykle nazwę stacji i często tytuł
tego, co leci. Ten tekst podróżuje w kanale zwanym DLS, czyli Dynamic Label Segment,
upchniętym w superramce jako dane X-PAD. Ten koder niesie etykietę statyczną: jeden ustalony
łańcuch dla całego pliku albo strumienia.

Statyczna to słowo kluczowe. Tekst nie zmienia się w trakcie kodowania. Prawdziwie
dynamiczna etykieta, która aktualizuje się utwór po utworze, oraz pokaz slajdów MOT, który
wyświetla okładki, to zadanie `ODR-PadEnc` w łańcuchu OpenDigitalRadio i nie są częścią tego
kodera.

Limit długości około 48 znaków wynika z trzech segmentów DLS upakowanych w jeden PAD.
Odbiorniki różnią się tym, ile z długiej etykiety pokazują naraz, więc dla nazwy stacji
użyteczna długość jest krótsza niż limit.

Wartości:
- pominięte: brak etykiety w strumieniu.
- `--dab-label <text>`: podany łańcuch jest niesiony jako DLS. Łańcuch ze spacjami należy
  wziąć w cudzysłowy.

Przykład w kontekście:

    fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

Współpraca i konflikty: wymaga `--dab`. Niezależny od każdego innego przełącznika, bo dotyka
PAD, a nie dźwięku.

# Dodatki

## Jak czytać `--verbose`

`--verbose` wypisuje wartości, które koder faktycznie zainicjalizował, zanim zacznie
kodować. Blok ma kilka sekcji. Większość pól tłumaczy się sama; te poniżej wymagają słowa
komentarza.

| Pole | Jak je czytać |
|---|---|
| `AOT (profile)` | 2 AAC-LC, 5 HE-AAC, 29 HE-AAC v2, 23 AAC-LD, 39 AAC-ELD. |
| `bitrate-mode` | 0 CBR, 1..5 VBR (wyżej = lepiej). |
| `channel-mode` | 1 mono, 2 stereo. W HE-AAC v2 rdzeń jest mono, a stereo tworzy PS. |
| `core bandwidth` | Górna krawędź rdzenia AAC, wyrównana do najbliższej granicy pasma współczynnika skali, więc może się różnić od wpisanej wartości (`-w 17300` daje `17915 Hz`). W nawiasie źródło: `from -w`, `from --core-cutoff` albo `auto`. Pod SBR to tylko rdzeń; SBR ciągnie pasmo wyżej. Pod SBR `-w` jest ignorowane, choć źródło może nadal brzmieć `from -w`. |
| `final BW (AAC+SBR)` | Tylko z SBR: przybliżona górna granica całego sygnału, rdzeń razem z SBR, wyliczona z indeksu stop SBR. |
| `signaling-mode` | Sposób sygnalizacji SBR i PS: 0 niejawna, 1 jawna zgodna wstecz, 2 jawna hierarchiczna. |
| `SBR ratio` | 1 downsampled (single rate), 2 dual rate (rdzeń pracuje z połową częstotliwości próbkowania). |
| `sbr amp res` | Rozdzielczość amplitudy obwiedni: 0 = 1,5 dB, 1 = 3,0 dB. |
| `granule length`, `frame length` | Próbki na ramkę: 1024 dla LC, 512 albo 480 dla LD i ELD. |
| `codec delay` | Opóźnienie w próbkach na kanał, całkowite i samego rdzenia. Ważne przy odtwarzaniu bez przerw. |
| `active SFBs (long)` | Liczba pasm współczynnika skali w bloku długim. Górna granica dla `--msbands`, `--isbands` i każdego argumentu `<sfb>`. |
| `IS corr threshold`, `IS L/R ratio thresh` | Wartości Q8 (256 = 1,0). Niższy próg korelacji sprawia, że intensity stereo włącza się chętniej. |
| `IS min contiguous SFBs` | Ile sąsiednich pasm musi się kwalifikować, zanim ruszy intensity stereo. |
| `TNS mask` | Maska bitowa 0x0..0xF aktywnych filtrów TNS. |
| `MS/IS bands: auto up to N` | Najwyższy indeks pasma, do którego narzędzie może działać. |
| `sbr header period` | Ramki i milisekundy między nagłówkami SBR: tyle czeka dekoder, który dołącza do strumienia, zanim usłyszy SBR. Dla ELD podawany jest okres konfiguracji LATM/LOAS, bo ELD nie ma nagłówka SBR w strumieniu. |
| `bit reservoir budget` | Średnia liczba bitów na ramkę i sufity rezerwuaru dla mono i stereo. Na tej podstawie dobiera się `--vbr-reservoir`. |
| `franken overrides applied` | Przełączniki, które w tym uruchomieniu odbiegają od ustawień fabrycznych, z wartością efektywną. Jeśli podanego przełącznika tu nie ma, nie zadziałał. |

## Pasma współczynnika skali i częstotliwości

Pasma numerujemy od 0 na dole. Tabela pokazuje przybliżoną górną krawędź co czwartego
pasma w bloku długim dla każdej częstotliwości próbkowania; ostatni wiersz podaje liczbę
pasm i częstotliwość Nyquista. Służy do przeliczania `--msbands`, `--isbands`,
`--hole-start` i innych argumentów `<sfb>` na herce. Dokładną liczbę dla danego
uruchomienia podaje `--verbose` (`active SFBs (long)`); zależy ona też od kodowanego pasma.

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
| 44  | brak    | 9647 | 13000| 17916 | 19500 | brak     |
| 48  | brak    | brak    | 15000| 22050 | 24000 | brak     |
| **liczba pasm / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### AAC-LC: automatyczne pasmo rdzenia zależnie od bitrate'u

Bez `-w` FDK wybiera pasmo z tabeli indeksowanej bitrate'em na kanał, a potem wyrównuje je
do granicy pasma SFB. Zmierzone przez `--verbose` przy 48 kHz (dla stereo `-b` to suma obu
kanałów):

| `-b` stereo | pasmo rdzenia | `-b` mono | pasmo rdzenia |
|---:|---:|---:|---:|
| 24000, 32000 | 6843 Hz | 12000, 16000 | 5062 Hz |
| 40000, 48000 | 9750 Hz | 20000, 24000 | 7500 Hz |
| 56000, 64000 | 13500 Hz | 28000, 32000 | 9750 Hz |
| 80000, 96000 | 14250 Hz | 40000, 48000 | 12750 Hz |
| 128000 | 15750 Hz | 64000 | 14250 Hz |
| 192000 | 17250 Hz | 96000 | 17250 Hz |

Wyższe `-w` pomaga tylko wtedy, gdy są na nie bity: przy 24..50 kbps wymuszone szerokie
pasmo to dokładnie obszar grupy E (dziury). Powyżej 20 kHz rdzeń wymaga
`--uncap-bandwidth` i częstotliwości próbkowania, która ma na to miejsce.

## Kontener M4A: co jest konieczne, a co można usunąć

Plik `.m4a` to drzewo pudełek (boxów). Szkielet odtwarzania jest obowiązkowy i nie ma do
niego przełącznika: `ftyp`, `mdat` (dane audio) i `moov` z `mvhd`, `trak`, `tkhd`, `mdia`
(`mdhd`, `hdlr`, `minf` z `smhd`, `dinf` i `stbl`, w którym są `stsd`, `stts`, `stsc`,
`stsz`, `stco`). Bez niego żaden dekoder nie znajdzie dźwięku.

Opcjonalny jest cały blok `udta` / `meta` / `ilst`: znacznik identyfikujący koder (`©too`),
`iTunSMPB` (opóźnienie kodera dla odtwarzania bez przerw) i wszystkie znaczniki tekstowe.

| Przełącznik | Działa na | Co robi |
|---|---|---|
| `--no-tool-tag` | `.m4a` | Pomija znacznik identyfikujący koder. Pozostałe znaczniki i dane gapless zostają. |
| `--minimal-moov` | `.m4a` | Najmniejszy legalny plik: pomija cały blok metadanych (znacznik kodera, `iTunSMPB`, wszystkie znaczniki). Szkielet odtwarzania zostaje. |

Zmierzone na 2 s stereo 128 kbps: 34381 bajtów domyślnie, 34297 z `--no-tool-tag` (o 84
mniej), 34048 z `--minimal-moov` (o 333 mniej). Narzut to głównie obowiązkowy szkielet.
Całkowitą rezygnację z kontenera daje zapis surowego ADTS (`-f 2 -o out.aac`), który nie ma
znaczników ani danych gapless.

`--minimal-moov` usuwa `iTunSMPB`, więc przy łączeniu utworów mogą pojawić się drobne
przerwy, bo opóźnienie kodera nie jest wtedy sygnalizowane. Dla albumów odtwarzanych
bez przerw lepiej zostawić ustawienie domyślne.

## Domyślne wyjście a oryginalna binarka

Sprawdzone względem oryginalnej binarki dostarczanej z dBpoweramp R17 (ta sama libfdk-aac
4.0.1, pakiet 2.0.3). AAC-LC jest identyczny bit w bit (ta sama suma md5). HE-AAC i HE-AAC
v2 różnią się bajtami, ale nie jakością: widmo HE-AAC v2 zgadza się co do 0,000 dB, a błąd
HE-AAC względem oryginału jest taki sam jak błąd samego oryginału (7,05e11 wobec 7,05e11).
Różnice bajtów wynikają z innego kompilatora (mingw/gcc zamiast MSVC) i zaokrągleń
arytmetyki stałoprzecinkowej.

# Budowanie i testy

## Budowanie

Repozytorium zawiera tylko źródła. GitHub Actions buduje obie binarki dla Windows na żądanie
(workflow „Build Franken FDK AAC (Windows x64 + x86)”, przycisk „Run workflow”) oraz przy
każdym tagu wydania. Lokalnie, na Linuksie albo w WSL z mingw-w64 i autotools:

    bash tools/build_win.sh both      # albo x64 / x86

Skrypt buduje każdą architekturę ze świeżej kopii źródeł, tak samo jak CI, i zostawia
`fdkaac-franken-x64.exe` oraz `fdkaac-franken-x86.exe` w katalogu projektu. Wszystkie
zmiany w libfdk-aac przechodzą przez jeden moduł, `libAACenc/src/franken.{h,cpp}` (globalny
blok `g_franken`, w którym każda nieustawiona wartość oznacza ustawienie fabryczne); nowe parametry
biblioteki (`AACENC_PARAM`, zakres `0xF0xx`) są zadeklarowane w `aacenc_lib.h`.

## Testy

    make check

uruchamia `tests/check.sh` na zbudowanych binarkach. Sprawdza, że każdy przełącznik jest w
`--help`, że każdy daje strumień przyjmowany przez dekodery bez błędów, że każdy faktycznie
zmienia strumień na materiale, na który powinien działać, że wartości odczytane ze
strumienia zgadzają się z żądanymi (kształt okna, grupy, rozmiary ramek), że dziury i
pre-echo po dekodowaniu zmieniają się w zmierzonym kierunku, oraz że bez nowych
przełączników wynik jest identyczny bajt w bajt z fabrycznym. Wymaga ffmpeg i python3; kod
wyjścia 0 oznacza sukces, 1 błędy, 77 brak zależności.

Narzędzia pomiarowe, z których pochodzą liczby w tym dokumencie, są w `tools/`:
`tools/core/` (dziury i pre-echo po dekodowaniu), `tools/invf/` (filtrowanie odwrotne SBR i
progi niskiego bitrate'u), `tools/msis/` (MS i IS odczytane ze strumienia, nagłówki SBR,
dołączanie do strumienia), `tools/dab/` (weryfikacja DAB+).
