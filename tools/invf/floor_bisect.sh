#!/bin/bash
# Bisekcja najnizszego bitrate, przy ktorym enkoder KONCZY kodowanie (rc=0)
# na danym wejsciu. Uzycie: floor_bisect.sh ENC WAV "-p 39 -f 10 ..." [lo] [hi]
ENC=$1; WAV=$2; ARGS=$3; lo=${4:-500}; hi=${5:-40000}
ok(){ "$ENC" $ARGS -b "$1" --unlock-bitrate -o _fb.out "$WAV" >/dev/null 2>&1; }
ok "$hi" || { echo "FAIL nawet przy $hi"; exit 1; }
while [ $((hi - lo)) -gt 100 ]; do
  mid=$(( (lo + hi) / 2 ))
  if ok "$mid"; then hi=$mid; else lo=$mid; fi
done
echo "$hi"
rm -f _fb.out
