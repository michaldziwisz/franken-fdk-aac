#!/bin/bash
# Weryfikacja WYDANEGO artefaktu v1.3.2 (self-report CI nie wystarcza).
# Pobiera zip z release, sprawdza przelaczniki i realne zachowanie kazdej
# poprawki tego wydania na binarce Z RELEASE, nie z lokalnego buildu.
# Pulapka: po unzip .exe nie ma prawa wykonywania -> chmod +x.
set -u
TAG="${1:-v1.3.2}"
P=/mnt/d/projekty/aacfdk
W="$P/tools/msis/rel"
mkdir -p "$W" && cd "$W"
rm -rf ./* 2>/dev/null || true

for arch in x64 x86; do
  gh release download "$TAG" -R michaldziwisz/franken-fdk-aac -p "*${arch}*.zip" --clobber >/dev/null 2>&1
  z=$(ls ./*"${arch}"*.zip 2>/dev/null | head -1)
  [ -n "$z" ] || { echo "BRAK zipa $arch"; exit 1; }
  mkdir -p "$arch" && unzip -o -q "$z" -d "$arch"
  exe=$(find "$arch" -name '*.exe' | head -1)
  chmod +x "$exe"
  echo "== $arch: $z ($(stat -c%s "$z") B), exe $(stat -c%s "$exe") B, md5 $(md5sum < "$exe" | cut -c1-12)"
  cp "$exe" "./franken-$arch.exe"
done

cp "$P/tools/msis/short_ms.wav" ms.wav
cp "$P/tools/msis/is_alt.wav" isalt.wav
cp "$P/tools/msis/m3_60.wav" m3.wav
ok=0; bad=0
chk(){ if [ "$1" = "1" ]; then echo "  OK   $2"; ok=$((ok+1)); else echo "  FAIL $2"; bad=$((bad+1)); fi; }

for arch in x64 x86; do
  E="./franken-$arch.exe"
  echo "=== $arch ==="
  for s in --sbr-header-time --sbr-noise-bands --msbands-lo --is-force-lo; do
    "$E" --help 2>&1 | grep -q -- "$s"; chk $([ $? = 0 ] && echo 1 || echo 0) "help zawiera $s"
  done
  # MS short: MS w grupach >0
  rm -f a.aac; "$E" -f2 -b96000 --msbands-lo 0 --msbands-hi 8 -o a.aac ms.wav >/dev/null 2>&1
  g=$(python3 "$P/tools/msis/parse_ms.py" a.aac --lo 0 --hi 8 --json | python3 -c "import json,sys;print(json.load(sys.stdin)['used_in_range_gN'])")
  chk $([ "$g" -gt 0 ] && echo 1 || echo 0) "MS short: $g pasm MS w grupach >0"
  # force-IS bez asercji
  rm -f b.aac; "$E" -f2 -p2 -b48000 --is 1 --is-force-lo 0 --is-force-hi 48 -o b.aac isalt.wav >/dev/null 2>&1; rc=$?
  n=$(ffmpeg -v error -i b.aac -f null - 2>&1 | grep -ciE 'error|invalid|overflow|illegal')
  chk $([ "$rc" = 0 ] && [ "$n" = 0 ] && echo 1 || echo 0) "force-IS naprzemienna panorama: rc=$rc bledy=$n"
  # noise bands 5 odrzucone
  rm -f c.aac; "$E" -f2 -p5 -b48000 --sbr-noise-bands 5 -o c.aac ms.wav >/dev/null 2>&1; rc=$?
  chk $([ "$rc" != 0 ] && [ ! -s c.aac ] && echo 1 || echo 0) "--sbr-noise-bands 5 odrzucone (rc=$rc)"
  # naglowek SBR 3.5 s, HE i ELD
  rm -f d.aac; "$E" -f2 -p5 -b48000 --sbr-header-time 3.5 -o d.aac m3.wav >/dev/null 2>&1
  g=$(python3 "$P/tools/msis/sbr_headers.py" d.aac --sr 48000 --json | python3 -c "import json,sys;print(json.load(sys.stdin)['gap_max_s'])")
  chk $(python3 -c "print(1 if 3.3<=$g<=3.6 else 0)") "HE-AAC naglowek SBR co $g s"
  rm -f e.loas; "$E" -f10 -p39 -L1 -b48000 --sbr-header-time 3.5 -o e.loas m3.wav >/dev/null 2>&1
  g=$(python3 "$P/tools/msis/loas_smc.py" e.loas --sr 48000 --fl 512 --json | python3 -c "import json,sys;print(json.load(sys.stdin)['gap_max_s'])")
  chk $(python3 -c "print(1 if 3.3<=$g<=3.6 else 0)") "ELD+SBR LOAS konfiguracja co $g s"
done
echo "WYNIK: OK=$ok FAIL=$bad"
rm -f ./*.wav ./*.aac ./*.loas
