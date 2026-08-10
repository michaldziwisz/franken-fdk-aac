#!/bin/bash
# -p39 (AAC-ELD) rozni sie BEZ flag IPD - czy to naprawa, czy sam enkoder
# jest tam niedeterministyczny? ELD to inna sciezka (bez PS), wiec naprawa
# ps_bitenc/ps_encode NIE POWINNA go dotyczyc. Test: powtarzalnosc TEJ SAMEJ
# binarki N razy. Jesli ta sama binarka daje rozne md5, to niedeterminizm
# srodowiska/kodera, nie nasza zmiana.
set -u
cd /mnt/d/projekty/aacfdk
NEWN="$HOME/aacfdk_native/front/fdkaac"
OLDN="$HOME/aacfdk_native/front/fdkaac.prebugfix"
W=tools/opd/reg/nr.wav

echo "=== ta sama binarka (NOWA), 4 przebiegi -p39 -b64000 ==="
for i in 1 2 3 4; do
  "$NEWN" -p39 -b64000 -f2 -o "tools/opd/reg/d_new_$i.aac" "$W" 2>/dev/null
  printf '  %d: md5=%s rozmiar=%s\n' "$i" \
      "$(md5sum tools/opd/reg/d_new_$i.aac | cut -c1-12)" \
      "$(stat -c%s tools/opd/reg/d_new_$i.aac)"
done
echo "=== ta sama binarka (STARA), 4 przebiegi ==="
for i in 1 2 3 4; do
  "$OLDN" -p39 -b64000 -f2 -o "tools/opd/reg/d_old_$i.aac" "$W" 2>/dev/null
  printf '  %d: md5=%s rozmiar=%s\n' "$i" \
      "$(md5sum tools/opd/reg/d_old_$i.aac | cut -c1-12)" \
      "$(stat -c%s tools/opd/reg/d_old_$i.aac)"
done
echo
echo "=== kontrola: czy -p39 w ogole uzywa Parametric Stereo? ==="
"$NEWN" -p39 -b64000 --verbose -o tools/opd/reg/eld.m4a "$W" 2>&1 | grep -iE "parametric|^PS|ps " | head -5
echo "  (jesli PS jest OFF, naprawa PS nie moze wplywac na -p39)"
