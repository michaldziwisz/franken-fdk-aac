#!/bin/bash
# Czy blad siedzi w kodowaniu DELTA-TIME (stan miedzy ramkami)?
# Jesli wymuszenie delta-freq usuwa blad, winowajca jest roll-forward
# opdIdxLast/ipdIdxLast wobec ramek o zmiennej liczbie obwiedni.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
S=A_antiphase_eps0.50

probe() {
  local desc="$1"; shift
  env "$@" "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 -o dumps/dt.m4a "$S.wav" 2>/dev/null
  local n
  n=$(ffmpeg -y -v warning -i dumps/dt.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-44s bledow=%s\n' "$desc" "$n"
}

probe "baseline (delta-time dozwolone)"        DUMMY=1
probe "OPD tylko delta-freq"                   OPD_FREQ_ONLY=1
probe "IPD tylko delta-freq"                   IPD_FREQ_ONLY=1
probe "OBA tylko delta-freq"                   OPD_FREQ_ONLY=1 IPD_FREQ_ONLY=1
