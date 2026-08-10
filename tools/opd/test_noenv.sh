#!/bin/bash
# Czy winowajca to ramki BEZ parametrow stereo (nEnvelopes=0, sciezka noEnv)?
# W dumpie eps0.50 bylo 228 takich ramek. Jesli --ps-noenv-skip 0 (zakaz ramek
# bez parametrow) usuwa blad, to rozjazd siedzi w roll-forward opdIdxLast
# przy ramkach, ktore nie wysylaja obwiedni.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"

probe() {
  local s="$1"; shift
  "$ENC" -p 29 -b 48000 -o dumps/t3.m4a "$@" "$s.wav" 2>/dev/null
  local n
  n=$(ffmpeg -y -v warning -i dumps/t3.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-34s %-40s bledow=%s\n' "$s" "$*" "$n"
}

for s in A_antiphase_eps0.50 A_antiphase_eps0.20 A_antiphase_eps0.10 D2_reverb_decorrelated E_phase_transition; do
  probe "$s" --ps-ipd 1 --ps-opd 1
  probe "$s" --ps-ipd 1 --ps-opd 1 --ps-noenv-skip 0
done
