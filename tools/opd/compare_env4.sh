#!/bin/bash
# Czy bledy na sciezce --ps-env 4 --ps-env-reduce 0 to REGRESJA po mojej
# naprawie, czy watek istniejacy JUZ WCZESNIEJ? Porownanie z binarka
# referencyjna sprzed naprawy - to jedyny uczciwy sposob rozstrzygniecia.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p cmp

printf '%-28s %14s %14s  %s\n' "probka" "PRZED naprawa" "PO naprawie" "ocena"
for f in A_antiphase_eps0.00 A_antiphase_eps0.02 A_antiphase_eps0.05 \
         A_antiphase_eps0.10 B_lowband_antiphase real/M5; do
  b=$(basename "$f")
  src="$f.wav"; [ -f "$src" ] || src="$f.wav"
  "$OLD" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      -o "cmp/${b}_old.m4a" "$src" 2>/dev/null
  "$NEW" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      -o "cmp/${b}_new.m4a" "$src" 2>/dev/null
  eo=$(ffmpeg -y -v warning -i "cmp/${b}_old.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  en=$(ffmpeg -y -v warning -i "cmp/${b}_new.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  if [ "$en" -lt "$eo" ]; then ocena="POPRAWA"
  elif [ "$en" -gt "$eo" ]; then ocena="!!! REGRESJA"
  elif [ "$en" = "0" ]; then ocena="oba czyste"
  else ocena="bez zmian (osobny watek)"; fi
  printf '%-28s %14s %14s  %s\n' "$b" "$eo" "$en" "$ocena"
done
