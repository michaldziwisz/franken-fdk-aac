#!/bin/bash
# SZEROKI SKAN: czy zostala JAKAKOLWIEK kombinacja, ktora psuje strumien?
# Domykamy watek na serio - nie tylko na tych sciezkach, ktore wczesniej
# przypadkiem sprawdzilem. Krzyzujemy: probki x bitrate x pasma PS x obwiednie
# x noenv-skip. Kazda kombinacja przez ffmpeg; wybrane takze przez faad.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
mkdir -p sweep

derr() { ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 \
    | grep -icE 'error|invalid|exceeds|overflow|illegal' || true; }

total=0; bad=0
echo "kombinacje z bledami (pusto = wszystko czyste):"
for f in A_antiphase_eps0.00 A_antiphase_eps0.20 A_antiphase_eps0.50 \
         B_lowband_antiphase C_quiet_fade D2_reverb_decorrelated \
         E_phase_transition real/M2 real/M5 real/M6; do
  src="$f.wav"
  for br in 24000 32000 48000 64000; do
    for bands in "" "--ps-bands 10" "--ps-bands 20"; do
      for envv in "" "--ps-env 1" "--ps-env 2 --ps-env-reduce 0" \
                  "--ps-env 4 --ps-env-reduce 0" "--ps-env 4"; do
        for ns in "" "--ps-noenv-skip 0"; do
          total=$((total+1))
          # shellcheck disable=SC2086
          "$ENC" -p29 -b$br -f2 --ps-ipd 1 --ps-opd 1 $bands $envv $ns \
              -o sweep/s.aac "$src" 2>/dev/null
          if [ ! -s sweep/s.aac ]; then continue; fi
          n=$(derr sweep/s.aac)
          if [ "$n" != "0" ]; then
            bad=$((bad+1))
            echo "  BLAD($n): $(basename $f) -b$br $bands $envv $ns"
          fi
        done
      done
    done
  done
done
echo
echo "przebadanych kombinacji: $total, z bledami: $bad"
