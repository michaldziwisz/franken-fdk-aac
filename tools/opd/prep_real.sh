#!/bin/bash
# Inspekcja plikow Michala + konwersja do WAV o parametrach, ktore przyjmuje
# nasz harness. Nazwy maja spacje i polskie znaki - wszedzie cudzyslowy.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
mkdir -p real
i=0
for f in *.flac; do
  i=$((i+1))
  info=$(ffprobe -v error -select_streams a:0 \
      -show_entries stream=sample_rate,channels,bits_per_raw_sample,duration \
      -of default=nw=1:nk=1 "$f" | tr '\n' ' ')
  printf '%-2d %-72s %s\n' "$i" "$(echo "$f" | cut -c1-72)" "$info"
  out="real/M$i.wav"
  ffmpeg -y -v error -i "$f" -ar 44100 -ac 2 -c:a pcm_s16le "$out"
  printf '     -> %s (%s B)\n' "$out" "$(stat -c%s "$out")"
  echo "$f" > "real/M$i.name"
done
