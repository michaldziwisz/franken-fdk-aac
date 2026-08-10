#!/bin/bash
# Czy nowy test "IPD bez IID" jest DYSKRYMINUJACY? Odtwarzamy probke
# z check.sh 1:1 i puszczamy na starej oraz nowej binarce.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p disc2

# generator wyciagniety z check.sh (blok PSNOIID)
awk '/^python3 - "\$PSNOIID"/{flag=1;next} /^PY$/{if(flag){flag=0;exit}} flag' \
    ../../tests/check.sh > disc2/gen.py
wc -l < disc2/gen.py | tr -d '\n'; echo " linii generatora"
python3 disc2/gen.py disc2/psnoiid.wav
echo "probka: $(stat -c%s disc2/psnoiid.wav) B"
echo

derr() { ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 \
    | grep -icE 'error|invalid|exceeds|overflow|illegal' || true; }

printf '%-34s %8s %8s  %s\n' "wariant" "PRZED" "PO" "dyskryminuje"
for v in "--ps-env 4 --ps-env-reduce 0" "--ps-env 2 --ps-env-reduce 0" ""; do
  tag=$(echo "${v:-dflt}" | tr -d ' -')
  # shellcheck disable=SC2086
  "$OLD" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $v -o "disc2/o_$tag.aac" disc2/psnoiid.wav 2>/dev/null
  # shellcheck disable=SC2086
  "$NEW" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $v -o "disc2/n_$tag.aac" disc2/psnoiid.wav 2>/dev/null
  o=$(derr "disc2/o_$tag.aac"); n=$(derr "disc2/n_$tag.aac")
  if [ "$o" -gt 0 ] && [ "$n" = "0" ]; then d="TAK - lapie regresje"
  elif [ "$o" = "0" ] && [ "$n" = "0" ]; then d="nie (oba czyste)"
  else d="PO naprawie nadal bledy"; fi
  printf '%-34s %8s %8s  %s\n' "${v:-domyslne}" "$o" "$n" "$d"
done
