#!/bin/bash
# TEST ROZSTRZYGAJACY: czy blad dekodera jest zwiazany z ramkami o 4 obwiedniach?
# eps0.50 mial 4 ramki nEnv=4 i 2 bledy dekodera; D1 nie mial ani jednej nEnv=4
# i zero bledow. Jesli wymuszenie 1 obwiedni usuwa blad - to sciezka nEnv>1.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
S=A_antiphase_eps0.50

run() {
  desc="$1"; shift
  "$ENC" -p 29 -b 48000 -o dumps/t.m4a "$@" "$S.wav" 2>/dev/null
  n=$(ffmpeg -y -v warning -i dumps/t.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-46s bledow=%s\n' "$desc" "$n"
}

run "IPD+OPD, domyslne obwiednie"      --ps-ipd 1 --ps-opd 1
run "IPD+OPD, --ps-env 1 (bez nEnv>1)" --ps-ipd 1 --ps-opd 1 --ps-env 1
run "IPD+OPD, --ps-env 4 --ps-env-reduce 0" --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0
run "tylko IPD (OPD=0)"                --ps-ipd 1 --ps-opd 0
run "tylko IPD, --ps-env 4 --ps-env-reduce 0" --ps-ipd 1 --ps-opd 0 --ps-env 4 --ps-env-reduce 0
run "stock (bez IPD/OPD)"              --ps-ipd 0
echo
echo "--- czy nEnv=4 wystepuje takze gdzie indziej? (kontrola na innych probkach) ---"
for b in A_antiphase_eps0.20 E_phase_transition D2_reverb_decorrelated C_quiet_fade; do
  "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      -o dumps/t2.m4a "$b.wav" 2>/dev/null
  n=$(ffmpeg -y -v warning -i dumps/t2.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-46s bledow=%s\n' "$b (env4, reduce off)" "$n"
done
