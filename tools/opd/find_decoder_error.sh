#!/bin/bash
# Ktora probka wywoluje blad dekodera ffmpeg? Osobno OPD=0 i OPD=1,
# zeby ustalic czy winowajca to OPD czy samo IPD.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
mkdir -p dec
for f in *.wav; do
  b="${f%.wav}"
  for mode in "0" "1"; do
    m="dec/${b}_opd${mode}.m4a"
    "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd "$mode" -o "$m" "$f" 2>/dev/null
    err=$(ffmpeg -y -v warning -i "$m" -f null - 2>&1 | grep -cE 'overflow|illegal|Error|error' || true)
    printf '%-30s opd=%s  bledow_dekodera=%s\n' "$b" "$mode" "$err"
  done
done
echo "--- kontrola: probki z poprzednich sesji (regresja?) ---"
for f in ../../ps_test.wav ../../ipd_test.wav; do
  [ -f "$f" ] || continue
  b=$(basename "$f" .wav)
  for mode in "0" "1"; do
    m="dec/${b}_opd${mode}.m4a"
    "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd "$mode" -o "$m" "$f" 2>/dev/null
    err=$(ffmpeg -y -v warning -i "$m" -f null - 2>&1 | grep -cE 'overflow|illegal|Error|error' || true)
    printf '%-30s opd=%s  bledow_dekodera=%s\n' "$b" "$mode" "$err"
  done
done
