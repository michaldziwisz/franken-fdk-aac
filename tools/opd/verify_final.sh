#!/bin/bash
# WERYFIKACJA KONCOWA po naprawie enableIID: wszystkie probki, wszystkie
# sciezki obwiedni, DWA dekodery. Plus no-regression i dowod ze OPD nadal dziala.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p final

derr() { ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 \
    | grep -icE 'error|invalid|exceeds|overflow|illegal' || true; }

echo "=== (1) bledy dekodera: wszystkie probki x 4 sciezki obwiedni ==="
printf '%-26s %8s %8s %8s %8s\n' "probka" "dflt" "env1" "env2" "env4"
fail=0
for f in *.wav real/M*.wav; do
  b=$(basename "$f" .wav); r=()
  for v in "" "--ps-env 1" "--ps-env 2 --ps-env-reduce 0" "--ps-env 4 --ps-env-reduce 0"; do
    # shellcheck disable=SC2086
    "$ENC" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $v -o "final/$b.aac" "$f" 2>/dev/null
    n=$(derr "final/$b.aac"); r+=("$n")
    [ "$n" = "0" ] || fail=1
  done
  printf '%-26s %8s %8s %8s %8s\n' "$b" "${r[0]}" "${r[1]}" "${r[2]}" "${r[3]}"
done
echo "WYNIK: $([ $fail -eq 0 ] && echo 'WSZYSTKO CZYSTE' || echo 'SA BLEDY')"

echo
echo "=== (2) drugi dekoder (faad) - porownanie z baseline bez IPD ==="
for f in A_antiphase_eps0.00.wav real/M5.wav real/M2.wav; do
  b=$(basename "$f" .wav)
  "$ENC" -p29 -b48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 -o "final/${b}_f.m4a" "$f" 2>/dev/null
  "$ENC" -p29 -b48000 --ps-ipd 0 -o "final/${b}_fb.m4a" "$f" 2>/dev/null
  a=$(faad -o /dev/null "final/${b}_f.m4a" 2>&1 | tr -cd '[:print:]\n' | grep -ciE 'error|illegal|invalid' || true)
  c=$(faad -o /dev/null "final/${b}_fb.m4a" 2>&1 | tr -cd '[:print:]\n' | grep -ciE 'error|illegal|invalid' || true)
  printf '  %-24s z_IPD=%s baseline=%s  %s\n' "$b" "$a" "$c" \
      "$([ "$a" -le "$c" ] && echo OK || echo '!!! GORZEJ NIZ BASELINE')"
done

echo
echo "=== (3) NO-REGRESSION: bez --ps-ipd bit-identycznie z wersja przed naprawami ==="
python3 - final/nr.wav <<'PY'
import wave, struct, math, random, sys
sr=44100; random.seed(3); c=lambda x:max(-32768,min(32767,int(x)))
fr=[]
for i in range(sr*2):
    t=i/sr
    fr.append(struct.pack('<hh',
        c(9000*(math.sin(2*math.pi*440*t)+0.3*random.uniform(-1,1))),
        c(9000*(math.sin(2*math.pi*554*t)+0.3*random.uniform(-1,1)))))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY
ok=1
for args in "-p2 -b128000" "-p29 -b48000" "-p5 -b64000" "-p29 -b24000" \
            "-p29 -b48000 --ps-env 4 --ps-env-reduce 0"; do
  tag=$(echo "$args" | tr -d ' -')
  # shellcheck disable=SC2086
  "$OLD" $args -f2 -o "final/o_$tag.aac" final/nr.wav 2>/dev/null
  # shellcheck disable=SC2086
  "$ENC" $args -f2 -o "final/n_$tag.aac" final/nr.wav 2>/dev/null
  if [ ! -s "final/o_$tag.aac" ] && [ ! -s "final/n_$tag.aac" ]; then
    printf '  %-44s POMINIETE (nieobslugiwane w obu)\n' "$args"; continue
  fi
  if cmp -s "final/o_$tag.aac" "final/n_$tag.aac"; then
    printf '  %-44s BIT-IDENTYCZNE\n' "$args"
  else
    printf '  %-44s !!! ROZNI SIE\n' "$args"; ok=0
  fi
done
echo "  => $([ $ok -eq 1 ] && echo 'naprawy sa czysto OPT-IN' || echo 'UWAGA')"

echo
echo "=== (4) OPD nadal realnie kodowane (--ps-opd 0 vs 1 musza sie roznic) ==="
for f in real/M2.wav real/M6.wav; do
  b=$(basename "$f" .wav)
  "$ENC" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 0 -o "final/${b}_o0.aac" "$f" 2>/dev/null
  "$ENC" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 -o "final/${b}_o1.aac" "$f" 2>/dev/null
  cmp -s "final/${b}_o0.aac" "final/${b}_o1.aac" \
      && echo "  $b: !!! IDENTYCZNE" || echo "  $b: roznia sie OK"
done
