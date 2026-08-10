#!/bin/bash
# ADTS (-f2) czy M4A? Wczesniej blad wychodzil na M4A, a moj test check.sh
# uzywa -f2. Jesli ADTS MASKUJE blad, to caly nowy test jest bezuzyteczny
# i musi isc na M4A.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p disc

printf '%-24s %-8s %8s %8s\n' "probka" "format" "PRZED" "PO"
for src in A_antiphase_eps0.50.wav; do
  b=$(basename "$src" .wav)
  for fmt in "m4a|" "adts|-f2"; do
    ext="${fmt%|*}"; flag="${fmt#*|}"
    # shellcheck disable=SC2086
    "$OLD" -p29 -b48000 $flag --ps-ipd 1 --ps-opd 1 -o "disc/o.$ext" "$src" 2>/dev/null
    # shellcheck disable=SC2086
    "$NEW" -p29 -b48000 $flag --ps-ipd 1 --ps-opd 1 -o "disc/n.$ext" "$src" 2>/dev/null
    o=$(ffmpeg -y -loglevel error -i "disc/o.$ext" -f null - 2>&1 | grep -icE 'error|invalid|exceeds|overflow|illegal' || true)
    n=$(ffmpeg -y -loglevel error -i "disc/n.$ext" -f null - 2>&1 | grep -icE 'error|invalid|exceeds|overflow|illegal' || true)
    printf '%-24s %-8s %8s %8s\n' "$b" "$ext" "$o" "$n"
  done
done
echo
echo "=== czy -v warning vs -loglevel error zmienia obraz? (poziom logowania) ==="
"$OLD" -p29 -b48000 --ps-ipd 1 --ps-opd 1 -o disc/lvl.m4a A_antiphase_eps0.50.wav 2>/dev/null
echo -n "  -v warning: "; ffmpeg -y -v warning -i disc/lvl.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true
echo -n "  -loglevel error: "; ffmpeg -y -loglevel error -i disc/lvl.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true
