#!/bin/bash
# Pomiar przed/po dla poprawek MS (short) / promocji MS_MASK_ALL / force-IS.
# Uzycie: measure.sh <binarka_przed> <binarka_po>
# Mierzy na probkach z gen_samples.py (strona dekodera: parse_ms.py).
set -u
OLD="$1"; NEW="$2"
cd "$(dirname "$0")"
[ -f short_ms.wav ] || python3 gen_samples.py . >/dev/null
TESTWAV=../../testms.wav

row(){ # tag bin args... ; zakres przekazywany jako LO HI MAX w zmiennych
  local tag="$1" bin="$2"; shift 2
  "$bin" -f2 "$@" -o _m.aac "$IN" >/dev/null 2>&1; local rc=$?
  local j; j=$(python3 parse_ms.py _m.aac $PARGS --json 2>/dev/null)
  local de; de=$(ffmpeg -v error -i _m.aac -f null - 2>&1 | grep -ciE 'error|invalid|exceeds|overflow|illegal')
  echo "  $tag rc=$rc dec_err=$de $j"
}

echo "### MS short: zakres w obrebie grupy (short_ms.wav, 96k)"
IN=short_ms.wav
for c in "--msbands-lo 0 --msbands-hi 8|--lo 0 --hi 8" "--msbands 9|--max 9" "--msbands-lo 3 --msbands-hi 10|--lo 3 --hi 10"; do
  A="${c%%|*}"; PARGS="${c##*|}"
  echo " [$A]"
  row PRZED "$OLD" -b96000 $A
  row PO    "$NEW" -b96000 $A
done

echo "### Promocja MS_MASK_ALL przy zakresie wylaczajacym <9 pasm (short_ms.wav, 96k, --pns 0)"
for hi in 32 34 36 38; do
  A="--pns 0 --msbands-hi $hi"; PARGS="--hi $hi"
  echo " [$A]"
  row PRZED "$OLD" -b96000 $A
  row PO    "$NEW" -b96000 $A
done
echo " [bez --pns 0, --msbands-hi 36]"; A="--msbands-hi 36"; PARGS="--hi 36"
row PRZED "$OLD" -b96000 $A
row PO    "$NEW" -b96000 $A

echo "### Force-IS: naprzemienna panorama (is_alt.wav)"
IN=is_alt.wav; PARGS=""
for b in 48000 96000; do
  row "PRZED b=$b" "$OLD" -p2 -b$b --is 1 --is-force-lo 0 --is-force-hi 48
  row "PO    b=$b" "$NEW" -p2 -b$b --is 1 --is-force-lo 0 --is-force-hi 48
done

echo "### Brak regresji bez flag i dla long-only (md5 ADTS)"
for IN in short_ms.wav is_alt.wav; do
  for A in "" "--msbands 9" "--msbands-lo 3 --msbands-hi 10" "--is 1 --is-force-lo 25 --is-force-hi 45" "--msmask 1" "--msmask 0"; do
    for p in "-b96000" "-b48000 --noshort" ; do :; done
    "$OLD" -f2 -b96000 $A -o _a.aac "$IN" >/dev/null 2>&1
    "$NEW" -f2 -b96000 $A -o _b.aac "$IN" >/dev/null 2>&1
    if cmp -s _a.aac _b.aac; then r=IDENT; else r=ROZNE; fi
    echo "  $IN [$A] $r"
  done
done
rm -f _m.aac _a.aac _b.aac
