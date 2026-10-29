#!/bin/bash
# Kodowanie skrajnie niskim bitrate + realny bitrate i dekodowalnosc (FDK, ffmpeg).
# Uzycie: lowrate_check.sh ENC FDKDEC "profil i flagi" plik.wav sekundy
ENC=$1; DEC=$2; ARGS=$3; WAV=$4; SECS=$5
o=_lc.aac; case "$ARGS" in *"-f 10"*) o=_lc.loas;; esac
rm -f "$o"
timeout 60 "$ENC" $ARGS --unlock-bitrate -o "$o" "$WAV" >/dev/null 2>_lc.err; rc=$?
if [ $rc -ne 0 ] || [ ! -s "$o" ]; then echo "$ARGS $WAV: rc=$rc $(grep -a -m1 -i 'error\|assert' _lc.err)"; exit 1; fi
real=$(( $(stat -c%s "$o") * 8 / SECS ))
fdk=$("$DEC" "$o" /dev/null | sed -n 's/.*errors=\([0-9]*\).*/\1/p')
ff=$(ffmpeg -v error -i "$o" -f null - 2>&1 | grep -ci 'error\|invalid\|overflow\|illegal')
echo "$ARGS $WAV: rc=0 real=${real} bps  bledy FDK=$fdk ffmpeg=$ff"
rm -f "$o" _lc.err
