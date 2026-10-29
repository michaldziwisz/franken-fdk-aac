#!/bin/bash
# Rozklad invf (strona dekodera) dla --sbr-softness 0..7 na kilku probkach.
# Uzycie: softness_sweep.sh ENC INVFDEC "-p5 -b32000" plik1.wav [plik2.wav ...]
ENC=$1; DEC=$2; ARGS=$3; shift 3
H=$(dirname "$0")/invf_hist.py
for f in "$@"; do
  for s in stock 0 1 2 3 4 5 6 7; do
    if [ "$s" = stock ]; then x=""; else x="--sbr-softness $s"; fi
    "$ENC" $ARGS $x -f 2 -o _sw.aac "$f" >/dev/null 2>&1 || { echo "$f $s ENCFAIL"; continue; }
    INVF_DUMP=_sw.txt "$DEC" _sw.aac /dev/null >/dev/null 2>&1
    printf "%-12s %-6s %s\n" "$(basename "$f")" "$s" "$(python3 "$H" _sw.txt)"
  done
done
rm -f _sw.aac _sw.txt
