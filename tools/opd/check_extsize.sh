#!/bin/bash
# Czy deklarowany rozmiar rozszerzenia PS zgadza sie z realnie zapisanym?
# Interesuje nas probka eps0.50, ktora JAKO JEDYNA wywolala blad dekodera.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
for b in A_antiphase_eps0.50 A_antiphase_eps0.00 D1_wide_bass_in_phase; do
  OPD_BITS=1 "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 \
      -o "dumps/${b}_bits.m4a" "$b.wav" 2> "dumps/${b}.bits"
  tot=$(grep -c PSEXT "dumps/${b}.bits" || true)
  # rozjazd: sized != written
  bad=$(awk '/PSEXT/{split($2,a,"=");split($3,c,"=");if(a[2]!=c[2])n++}END{print n+0}' "dumps/${b}.bits")
  printf '%-26s ramek_PSEXT=%-6s rozjazd_sized_vs_written=%s\n' "$b" "$tot" "$bad"
  echo "   przyklady:"; grep PSEXT "dumps/${b}.bits" | head -3 | sed 's/^/     /'
  # rozklad extSize
  echo -n "   extSize wystepujace: "
  awk '/PSEXT/{split($4,e,"=");print e[2]}' "dumps/${b}.bits" | sort -n | uniq -c | tr '\n' ' '
  echo
done
