#!/bin/bash
# TEST HIPOTEZY: czy wymuszenie enableIID=1 przy wlaczonym IPD usuwa bledy
# na sciezce --ps-env 4 --ps-env-reduce 0?
#
# Uzasadnienie ze zrodla dekodera (ffmpeg aacps_common.c:143-156): nr_ipdopd_par
# (liczba pasm IPD/OPD) jest ustawiana WYLACZNIE w bloku
#     if (header) { if (enable_iid) { ... nr_ipdopd_par = tab[iid_mode]; } }
# Jesli wysylamy rozszerzenie fazy w strumieniu, w ktorym enable_iid=0, dekoder
# nie zna liczby pasm fazy i czyta INNA liczbe bitow niz zapisalismy.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
mkdir -p hyp

derr() { ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 \
    | grep -icE 'error|invalid|exceeds|overflow|illegal' || true; }

printf '%-26s %-12s %10s %14s  %s\n' "probka" "wariant" "bez_force" "z_force_iid" "ocena"
for b in A_antiphase_eps0.00 A_antiphase_eps0.02 A_antiphase_eps0.05 \
         A_antiphase_eps0.10 B_lowband_antiphase real/M5 real/M3; do
  src="$b.wav"; tag=$(basename "$b")
  for v in "--ps-env 4 --ps-env-reduce 0|env4" "|dflt"; do
    args="${v%|*}"; lbl="${v#*|}"
    # shellcheck disable=SC2086
    "$ENC" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $args -o "hyp/${tag}_${lbl}_a.aac" "$src" 2>/dev/null
    a=$(derr "hyp/${tag}_${lbl}_a.aac")
    # shellcheck disable=SC2086
    OPD_FORCE_IID=1 "$ENC" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $args -o "hyp/${tag}_${lbl}_b.aac" "$src" 2>/dev/null
    c=$(derr "hyp/${tag}_${lbl}_b.aac")
    if [ "$a" -gt 0 ] && [ "$c" = "0" ]; then o="HIPOTEZA POTWIERDZONA"
    elif [ "$a" -gt 0 ] && [ "$c" -lt "$a" ]; then o="czesciowa poprawa"
    elif [ "$a" = "0" ] && [ "$c" = "0" ]; then o="oba czyste"
    else o="nie tlumaczy"; fi
    printf '%-26s %-12s %10s %14s  %s\n' "$tag" "$lbl" "$a" "$c" "$o"
  done
done
