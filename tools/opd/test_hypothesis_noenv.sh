#!/bin/bash
# HIPOTEZA na drugi watek (sciezka --ps-env 4 --ps-env-reduce 0):
# rozjazd na granicy ramek BEZ parametrow (nEnvelopes=0).
#
# Dekoder (ffmpeg aacps_common.c): dla obwiedni 0 w delta-time
#     e_prev = num_env_old - 1, potem FFMAX(e_prev, 0)
# Gdy POPRZEDNIA ramka nie niosla parametrow, num_env_old = 0 => e_prev = -1
# => po FFMAX dekoder predykuje z PAR[0], czyli z PIERWSZEJ obwiedni.
# Nasz enkoder w takiej sytuacji NIE aktualizuje opdIdxLast wcale (caly
# roll-forward siedzi w bloku `if (nEnvelopes > 0)`), wiec trzyma wartosci
# z ostatniej ramki, ktora miala parametry - INNE niz to, co ma dekoder.
#
# TEST: --ps-noenv-skip 0 zabrania ramek bez parametrow. Jesli to usuwa bledy
# na sciezce env4, hipoteza jest potwierdzona.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
mkdir -p cmp

printf '%-24s %12s %14s  %s\n' "probka" "env4" "env4+noenv0" "wniosek"
for b in A_antiphase_eps0.00 A_antiphase_eps0.05 A_antiphase_eps0.10 B_lowband_antiphase; do
  "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      -o cmp/h1.m4a "$b.wav" 2>/dev/null
  a=$(ffmpeg -y -v warning -i cmp/h1.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      --ps-noenv-skip 0 -o cmp/h2.m4a "$b.wav" 2>/dev/null
  c=$(ffmpeg -y -v warning -i cmp/h2.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  if [ "$a" -gt 0 ] && [ "$c" = "0" ]; then w="HIPOTEZA POTWIERDZONA"
  elif [ "$c" -lt "$a" ]; then w="czesciowo (spadek)"
  else w="hipoteza NIE tlumaczy"; fi
  printf '%-24s %12s %14s  %s\n' "$b" "$a" "$c" "$w"
done
