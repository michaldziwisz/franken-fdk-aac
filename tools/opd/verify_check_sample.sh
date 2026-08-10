#!/bin/bash
# Czy probka WYGENEROWANA PRZEZ check.sh (dokladnie ten generator) dyskryminuje?
# Wyciagam generator z check.sh 1:1 przez uruchomienie fragmentu skryptu.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p disc

# wyciagnij blok python3 z check.sh (od PSMULTI do PY) i odpal go
awk '/^python3 - "\$PSMULTI"/{flag=1;next} /^PY$/{if(flag){flag=0;exit}} flag' \
    ../../tests/check.sh > disc/gen_psmulti.py
wc -l disc/gen_psmulti.py
python3 disc/gen_psmulti.py disc/check_psmulti.wav
echo "probka z check.sh: $(stat -c%s disc/check_psmulti.wav) B"
echo

# ta sama funkcja derr co w check.sh (z naprawionym wzorcem)
derr() {
  ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 \
      | grep -icE 'error|invalid|exceeds|overflow|illegal' || true
}

printf '%-34s %8s %8s  %s\n' "wariant" "PRZED" "PO" "dyskryminuje"
for v in "|dflt" "--ps-env 2 --ps-env-reduce 0|e2" "--ps-env 4 --ps-env-reduce 0|e4"; do
  args="${v%|*}"; tag="${v#*|}"
  # shellcheck disable=SC2086
  "$OLD" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $args -o "disc/co_$tag.aac" disc/check_psmulti.wav 2>/dev/null
  # shellcheck disable=SC2086
  "$NEW" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $args -o "disc/cn_$tag.aac" disc/check_psmulti.wav 2>/dev/null
  o=$(derr "disc/co_$tag.aac"); n=$(derr "disc/cn_$tag.aac")
  if [ "$o" -gt 0 ] && [ "$n" = "0" ]; then d="TAK - lapie regresje"
  elif [ "$o" = "0" ] && [ "$n" = "0" ]; then d="nie (oba czyste)"
  else d="PO naprawie nadal bledy"; fi
  printf '%-34s %8s %8s  %s\n' "${args:-domyslne}" "$o" "$n" "$d"
done
echo
echo "=== opt-in: bez flagi vs --ps-ipd 0 (musi byc bit-identycznie) ==="
"$NEW" -p29 -b48000 -f2 -o disc/ci_base.aac disc/check_psmulti.wav 2>/dev/null
"$NEW" -p29 -b48000 -f2 --ps-ipd 0 -o disc/ci_off.aac disc/check_psmulti.wav 2>/dev/null
cmp -s disc/ci_base.aac disc/ci_off.aac && echo "  bit-identyczne OK" || echo "  !!! ROZNI SIE"
