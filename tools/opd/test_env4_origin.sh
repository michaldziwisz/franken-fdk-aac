#!/bin/bash
# CZY blad na sciezce --ps-env 4 --ps-env-reduce 0 zalezy od IPD/OPD W OGOLE?
# Jesli wystepuje TAKZE przy --ps-ipd 0, to jest to bug w pokretlach --ps-env
# z 09.08 (commit 4af633f), a NIE w parze fazowej - inny temat, inny fix.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p cmp

printf '%-24s %10s %10s %10s %10s\n' "probka" "ipd0_e4" "ipd1_e4" "ipd0_dflt" "ipd1_dflt"
for b in A_antiphase_eps0.00 A_antiphase_eps0.05 B_lowband_antiphase; do
  r=()
  for args in "--ps-ipd 0 --ps-env 4 --ps-env-reduce 0" \
              "--ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0" \
              "--ps-ipd 0" \
              "--ps-ipd 1 --ps-opd 1"; do
    # shellcheck disable=SC2086
    "$NEW" -p 29 -b 48000 $args -o cmp/q.m4a "$b.wav" 2>/dev/null
    n=$(ffmpeg -y -v warning -i cmp/q.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
    r+=("$n")
  done
  printf '%-24s %10s %10s %10s %10s\n' "$b" "${r[0]}" "${r[1]}" "${r[2]}" "${r[3]}"
done
echo
echo "Jesli kolumna ipd0_e4 > 0, to bug siedzi w --ps-env/--ps-env-reduce"
echo "(commit 4af633f z 09.08), NIEZALEZNIE od pary fazowej IPD/OPD."
echo
echo "=== kontrola: czy to samo widzi STARA binarka (przed dzisiejsza naprawa) ==="
for b in A_antiphase_eps0.00; do
  "$OLD" -p 29 -b 48000 --ps-ipd 0 --ps-env 4 --ps-env-reduce 0 -o cmp/qo.m4a "$b.wav" 2>/dev/null
  n=$(ffmpeg -y -v warning -i cmp/qo.m4a -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  echo "  $b stara binarka, --ps-ipd 0 --ps-env 4: $n bledow"
done
