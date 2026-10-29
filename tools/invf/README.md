# tools/invf — pomiary estymatorów SBR i niskiego bitrate

Narzędzia, którymi zmierzono `--sbr-softness`, pokrętła `--sbr-invf-*`,
`--sbr-nf-*` oraz `--unlock-bitrate`. Wszystkie oceniają strumień po stronie
dekodera albo z rozmiaru pliku, nie z deklaracji enkodera.

- `gen_samples.py` — próbki 48 kHz: mix, mowa (formanty), tony, rzadkie tony.
- `invf_dump_decoder.patch` — łatka na `libSBRdec/src/env_extr.cpp` dekodera FDK
  (osobne drzewo, nie projekt): przy zmiennej `INVF_DUMP=plik` zapisuje tryby
  inverse filtering każdej ramki odczytane z bitstreamu.
- `invf_hist.py plik` — rozkład OFF/LOW/MID/HIGH, średnia i zmiany trybu na pasmo
  i ramkę z takiego zrzutu.
- `softness_sweep.sh ENC INVFDEC "-p5 -b32000" próbki...` — rozkład invf dla
  `--sbr-softness` 0..7.
- `hf_flatness.py dec.wav --fcut --fhi [--ref orig.wav]` — energia i płaskość
  widmowa zdekodowanego pasma SBR. Okno ustaw na realne pasmo SBR z `--verbose`.
- `floor_bisect.sh`, `floor_table.sh` — najniższe `-b`, przy którym kodowanie się
  kończy, dla profili, częstotliwości i liczby kanałów.
- `lowrate_check.sh`, `lowrate_suite.sh`, `lowrate_matrix.py` — realny bitrate z
  rozmiaru ADTS/LOAS oraz błędy dekodera FDK (`tools/msis/fdkdec.c`) i ffmpeg.
- `ffmpeg_bad_frame.py plik.aac` — bisekcja: po której ramce ADTS ffmpeg zgłasza
  pierwszy błąd.

`tools/build_win.sh [x64|x86|both]` buduje binarki Windows ze świeżych drzew, tak
jak CI, do `make check`.
