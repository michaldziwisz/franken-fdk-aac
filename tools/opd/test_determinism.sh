#!/bin/bash
# Czy blad jest DETERMINISTYCZNY? (lekcja z flaky make check - zanim uznam
# cokolwiek za bug, sprawdzam czy narzedzie daje ten sam wynik N razy)
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"

for s in A_antiphase_eps0.50 E_phase_transition; do
  echo "=== $s ==="
  for i in 1 2 3; do
    "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 -o "dumps/det_$i.m4a" "$s.wav" 2>/dev/null
    md5=$(md5sum "dumps/det_$i.m4a" | cut -c1-12)
    e=$(ffmpeg -y -v warning -i "dumps/det_$i.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
    printf '  przebieg %d: md5=%s bledow=%s\n' "$i" "$md5" "$e"
  done
  echo "  --- z OPD_FREQ_ONLY ---"
  for i in 1 2; do
    OPD_FREQ_ONLY=1 "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 -o "dumps/detf_$i.m4a" "$s.wav" 2>/dev/null
    md5=$(md5sum "dumps/detf_$i.m4a" | cut -c1-12)
    e=$(ffmpeg -y -v warning -i "dumps/detf_$i.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
    printf '  przebieg %d: md5=%s bledow=%s\n' "$i" "$md5" "$e"
  done
done
