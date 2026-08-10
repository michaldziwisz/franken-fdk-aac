#!/bin/bash
# Zbuduj binarke SPRZED naprawy (z zachowanych .orig) jako referencje
# no-regression, potem przywroc naprawiona wersje.
set -eu
N="$HOME/aacfdk_native"
cd "$N/fdk/libSBRenc/src"
cp ps_encode.cpp ps_encode.cpp.fixed
cp ps_bitenc.cpp ps_bitenc.cpp.fixed
cp ps_encode.cpp.orig ps_encode.cpp
cp ps_bitenc.cpp.orig ps_bitenc.cpp
cd "$N/fdk" && touch libSBRenc/src/ps_encode.cpp libSBRenc/src/ps_bitenc.cpp
make -j"$(nproc)" >/dev/null 2>&1 && make install >/dev/null 2>&1
cd "$N/front" && make -j"$(nproc)" >/dev/null 2>&1
cp fdkaac fdkaac.prebugfix
echo "zbudowana binarka referencyjna: $N/front/fdkaac.prebugfix"

# przywroc naprawione
cd "$N/fdk/libSBRenc/src"
cp ps_encode.cpp.fixed ps_encode.cpp
cp ps_bitenc.cpp.fixed ps_bitenc.cpp
cd "$N/fdk" && touch libSBRenc/src/ps_encode.cpp libSBRenc/src/ps_bitenc.cpp
make -j"$(nproc)" >/dev/null 2>&1 && make install >/dev/null 2>&1
cd "$N/front" && make -j"$(nproc)" >/dev/null 2>&1
echo "przywrocona binarka NAPRAWIONA"
# kontrola: zrodla musza byc identyczne z projektem
for f in ps_encode.cpp ps_bitenc.cpp; do
  if diff -q "$N/fdk/libSBRenc/src/$f" \
      "/mnt/d/projekty/aacfdk/src-fdk-aac/libSBRenc/src/$f" >/dev/null; then
    echo "  $f zgodny z projektem"
  else
    echo "  $f !!! ROZNI SIE od projektu"
  fi
done
