#!/bin/bash
# Tabela realnych progow (najnizsze -b, przy ktorym kodowanie konczy sie rc=0).
# Uzycie: floor_table.sh ENC [STARY_ENC]  (w katalogu z lr_*.wav i wn_*.wav)
ENC=$1; OLD=${2:-}
FB=$(dirname "$0")/floor_bisect.sh
eff(){ "$1" $2 -b 500 --unlock-bitrate --verbose -o _ft.out "$3" 2>&1 | tr '\r' '\n' | sed -n 's/^ *bitrate *: *\([0-9]*\).*/\1/p' | head -1; }
for cfg in "-p 5 -f 2" "-p 29 -f 2" "-p 23 -f 10" "-p 39 -f 10" "-p 39 -L 1 -f 10" "-p 2 -f 2"; do
 for sr in 48000 32000 24000; do for ch in 1 2; do
  case "$cfg" in *"-p 29"*) [ $ch = 1 ] && continue;; esac
  a=$(bash "$FB" "$ENC" lr_${sr}_${ch}.wav "$cfg" 2>/dev/null)
  b=$(bash "$FB" "$ENC" wn_${sr}_${ch}.wav "$cfg" 2>/dev/null)
  o=""; [ -n "$OLD" ] && o=" | stary: $(eff "$OLD" "$cfg" lr_${sr}_${ch}.wav)"
  echo "$cfg sr$sr ch$ch: nowy ton+szum $a, bialy szum $b$o"
 done; done
done
rm -f _ft.out
