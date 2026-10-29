#!/bin/bash
# Przeglad pokretel rdzenia: pre-echo (attacks48) i dziury (dense48) po dekodowaniu.
# Uzycie: core_sweep.sh ENC DEC "-p2 -b40 -w 18000" "opcje1" "opcje2" ...
# Pusty string "" = przebieg bez nowych flag (punkt odniesienia).
set -u
ENC="$1"; DEC="$2"; BASE="$3"; shift 3
HERE="$(cd "$(dirname "$0")" && pwd)"
M="$HERE/core_measure.py"
[ -f attacks48.wav ] || python3 "$HERE/gen_core.py" . >/dev/null
# LD/ELD (-p23/-p39) nie maja ADTS: wtedy LOAS (-f10), dekoder rozpoznaje po rozszerzeniu.
case " $BASE " in *" -p23 "*|*" -p39 "*|*" -p 23 "*|*" -p 39 "*) TF="-f10"; EXT=loas;; *) TF="-f2"; EXT=aac;; esac
for OPT in "$@"; do
  r="[${OPT:-stock}]"
  for S in attacks48 dense48; do
    "$ENC" $BASE $OPT $TF -o _c.$EXT $S.wav >/dev/null 2>&1 || { r="$r $S=ENC_FAIL"; continue; }
    "$DEC" _c.$EXT _c.wav >/dev/null 2>&1 || { r="$r $S=DEC_FAIL"; continue; }
    bps=$(python3 -c "import os;print(int(os.path.getsize('_c.'+'$EXT')*8/6))")
    if [ $S = attacks48 ]; then
      m=$(python3 "$M" $S.wav _c.wav --attacks attacks48.txt | sed 's/lag=[0-9]* corr=[0-9.]* //;s/ n=[0-9]*//')
    else
      m=$(python3 "$M" $S.wav _c.wav | sed 's/lag=[0-9]* corr=[0-9.]* //')
    fi
    r="$r | $S ${bps}bps $m"
  done
  echo "$r"
done
rm -f _c.aac _c.loas _c.wav
