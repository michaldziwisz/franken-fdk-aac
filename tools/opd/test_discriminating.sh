#!/bin/bash
# CZY NOWY TEST JEST DYSKRYMINUJACY? Test, ktory przechodzi TAKZE na zepsutym
# kodzie, jest bezwartosciowy. Odpalamy nowa sekcje testu na DWOCH binarkach:
# naprawionej i sprzed naprawy. Oczekiwanie: PRZED = bledy, PO = czysto.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p disc

# ta sama probka co w check.sh (skopiowana logika generatora)
python3 - disc/psmulti.wav <<'PY'
import wave, struct, math, sys
sr = 44100
c = lambda x: max(-32768, min(32767, int(x)))
fr = []
for i in range(sr * 3):
    t = i / sr
    s = (math.sin(2 * math.pi * 110 * t) + 0.7 * math.sin(2 * math.pi * 290 * t)
         + 0.5 * math.sin(2 * math.pi * 540 * t))
    p = 0.5 + 0.5 * math.sin(2 * math.pi * 0.7 * t)
    ph = (t * 16.0) % 1.0
    burst = math.exp(-ph * 30.0) * math.sin(2 * math.pi * 420 * t)
    bl = burst if int(t * 16.0) % 2 == 0 else 0.0
    br = burst if int(t * 16.0) % 2 == 1 else 0.0
    fr.append(struct.pack('<hh', c(9000*(s*p+0.8*bl)), c(9000*(s*(1-p)+0.8*br))))
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY
echo "probka: $(stat -c%s disc/psmulti.wav) B"
echo

run() {
  local exe="$1" tag="$2"; shift 2
  # shellcheck disable=SC2086
  "$exe" -p29 -b48000 -f2 "$@" -o "disc/${tag}.aac" disc/psmulti.wav 2>/dev/null
  local n
  n=$(ffmpeg -y -loglevel error -i "disc/${tag}.aac" -f null - 2>&1 \
      | grep -icE 'error|invalid|exceeds' || true)
  echo "$n"
}

printf '%-40s %14s %14s  %s\n' "wariant" "PRZED naprawa" "PO naprawie" "test dyskryminuje?"
for v in "--ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0|e4" \
         "--ps-ipd 1 --ps-opd 1 --ps-env 2 --ps-env-reduce 0|e2" \
         "--ps-ipd 1 --ps-opd 1|dflt"; do
  args="${v%|*}"; tag="${v#*|}"
  # shellcheck disable=SC2086
  o=$(run "$OLD" "old_$tag" $args)
  # shellcheck disable=SC2086
  n=$(run "$NEW" "new_$tag" $args)
  if [ "$o" -gt 0 ] && [ "$n" = "0" ]; then d="TAK - lapie regresje"
  elif [ "$o" = "0" ] && [ "$n" = "0" ]; then d="nie (oba czyste)"
  else d="uwaga: PO naprawie nadal bledy"; fi
  printf '%-40s %14s %14s  %s\n' "$args" "$o" "$n" "$d"
done
