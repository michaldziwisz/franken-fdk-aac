#!/bin/bash
# Przebudowa crossbuildow Windows (x64 + x86) z aktualnymi zrodlami (lista FILES = zmienione pliki).
# PULAPKA (ze skilla): tests/check.sh uruchamia specs na .exe z katalogu
# projektu. Jesli .exe sa starsze niz zmiana w kodzie, testy mierza STARY kod
# i naprawa wyglada jak nieskuteczna (albo nowe testy failuja bez powodu).
# Struktura drzew: fdk/ front/ inst-x64|inst-x86 (NIE fdk-aac/).
set -eu
P=/mnt/d/projekty/aacfdk
FILES="libAACenc/src/ms_stereo.cpp libAACenc/src/intensity.cpp libSBRenc/src/sbr_encoder.cpp"

for pair in "$HOME/aacfdk_win:x86_64-w64-mingw32:x64" \
            "$HOME/aacfdk_win86:i686-w64-mingw32:x86"; do
  d="${pair%%:*}"; rest="${pair#*:}"; host="${rest%%:*}"; arch="${rest##*:}"
  echo "=== $arch ($d) ==="
  [ -d "$d" ] || { echo "  BRAK drzewa $d - pomijam"; continue; }
  # Synchronizuj CALE zrodla biblioteki (tylko pliki .c/.cpp/.h z lib*/), nie
  # wybrana liste: lista FILES gubila zmienione naglowki (nowe pole w
  # franken.h -> build lib padal, a .exe zostawal stary). rsync -c porownuje
  # tresc, wiec make przebuduje dokladnie to, co sie zmienilo.
  rsync -rc --include=*/ --include=*.c --include=*.cpp --include=*.h \
        --exclude=* "$P/src-fdk-aac/" "$d/fdk/"
  ( cd "$d/fdk" && make -j"$(nproc)" >/dev/null 2>&1 \
      && make install >/dev/null 2>&1 ) || { echo "  BLAD build lib"; continue; }
  # frontend (nu774): zawsze kopiuj zrodla, bo przelaczniki i walidacja zyja tutaj
  cp "$P"/src-fdkaac/src/*.c "$P"/src-fdkaac/src/*.h "$d/front/src/" 2>/dev/null || true
  ( cd "$d/front" && make -j"$(nproc)" >/dev/null 2>&1 ) \
      || { echo "  BLAD build front"; continue; }
  exe=$(find "$d/front" -maxdepth 1 -name '*.exe' | head -1)
  echo "  zbudowany: $exe ($(stat -c%s "$exe") B)"
  cp "$exe" "$P/fdkaac-franken-$arch.exe"
  echo "  skopiowany do $P/fdkaac-franken-$arch.exe"
done
ls -la --time-style=+%H:%M "$P"/fdkaac-franken-x64.exe "$P"/fdkaac-franken-x86.exe
