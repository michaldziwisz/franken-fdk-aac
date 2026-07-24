#!/bin/bash
# Test batch: nowe switche (IS band/force, minsnr, side/mid, sbr-header-period, reservoir verbose)
# Uruchamiac z katalogu projektu. Uzywa sciezek WZGLEDNYCH (interop .exe nie lubi /mnt absolutnych).
cd "$(dirname "$0")/.." || exit 1
EXE=./fdkaac-franken-x64.exe
IN=test_stereo.wav
BR=128000
FMT="-f2"   # ADTS: deterministyczne (M4A ma zmienny tag czasu -> falszywe roznice)
PASS=0; FAIL=0
b(){ md5sum "$1" 2>/dev/null | cut -d' ' -f1; }
dec_ok(){ ffmpeg -v error -i "$1" -f null - 2>/tmp/dec_err; [ ! -s /tmp/dec_err ]; }

# baseline
$EXE -b $BR $FMT -o out_base.aac "$IN" 2>/dev/null
BASE=$(b out_base.aac)
echo "baseline md5=$BASE (ADTS, deterministyczne)"

test_switch(){
  local name="$1"; shift
  local out="out_${name//[^a-zA-Z0-9]/_}.aac"
  $EXE -b $BR $FMT "$@" -o "$out" "$IN" 2>/tmp/enc_err
  if [ ! -f "$out" ] || [ ! -s "$out" ]; then echo "FAIL $name: brak/pusty output"; cat /tmp/enc_err|head -3; FAIL=$((FAIL+1)); return; fi
  local m=$(b "$out")
  if [ "$m" == "$BASE" ]; then echo "FAIL $name: bitstream identyczny z baseline (switch nic nie zrobil)"; FAIL=$((FAIL+1)); return; fi
  if ! dec_ok "$out"; then echo "FAIL $name: dekoder zglasza bledy:"; head -2 /tmp/dec_err; FAIL=$((FAIL+1)); return; fi
  echo "PASS $name: md5=$m rozny, dekodowalny"
  PASS=$((PASS+1))
}

# --- IS band range + force. IS jest bramkowane przy wysokim bitrate (allowIS wymaga
#     niskiego bitrate/pasma), wiec testujemy przy 48k + is-aggression gdzie IS realnie dziala.
IS="-p2 -b 48000 -f2 --is 1 --is-aggression 80"
is_base(){ $EXE $IS -o out_isbase.aac "$IN" 2>/dev/null; b out_isbase.aac; }
ISB=$(is_base); echo "IS-active baseline (48k agg80): $ISB"
test_is(){
  local name="$1"; shift
  local out="out_${name//[^a-zA-Z0-9]/_}.aac"
  $EXE $IS "$@" -o "$out" "$IN" 2>/tmp/enc_err
  local m=$(b "$out")
  if [ "$m" == "$ISB" ]; then echo "FAIL $name: identyczny z IS-baseline"; FAIL=$((FAIL+1)); return; fi
  if ! dec_ok "$out"; then echo "FAIL $name: dekoder bledy"; head -2 /tmp/dec_err; FAIL=$((FAIL+1)); return; fi
  echo "PASS $name: md5=$m rozny, dekodowalny"; PASS=$((PASS+1))
}
test_is "is-lo"       --is-lo 15
test_is "is-hi"       --is-hi 2
test_is "is-lohi"     --is-lo 1 --is-hi 3
test_is "is-force"    --is-force-lo 25 --is-force-hi 45
# --- masking MusePack-style ---
test_switch "minsnr-scale-lo"   --minsnr-scale 128
test_switch "minsnr-scale-hi"   --minsnr-scale 512
test_switch "minsnr-clamp-hi"   --minsnr-clamp-hi 512
test_switch "minsnr-clamp-lo"   --minsnr-clamp-lo 64
test_switch "reduce-clamp-off"  --reduce-clamp 0
# --- stereo bit-split ---
test_switch "mid-bias"          --mid-bias 384

echo "=========================================="
echo "WYNIK: PASS=$PASS FAIL=$FAIL"
