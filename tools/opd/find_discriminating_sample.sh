#!/bin/bash
# Szukam probki, ktora REALNIE lapie regresje delta-time (fail PRZED, pass PO).
# Kandydaci: probki, o ktorych WIEM z pomiaru, ze wywolywaly blad, oraz warianty
# mojego generatora. Kryterium: PRZED naprawa >0 bledow, PO naprawie 0.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
NEW="$HOME/aacfdk_native/front/fdkaac"
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p disc

errs() {
  local exe="$1" src="$2" tag="$3"; shift 3
  # WZORZEC jak w naprawionym derr() z check.sh - MUSI zawierac overflow|illegal,
  # bo ffmpeg tak zglasza uszkodzone PS ("ps extension overflow", "illegal icc").
  # shellcheck disable=SC2086
  "$exe" -p29 -b48000 -f2 "$@" -o "disc/${tag}.aac" "$src" 2>/dev/null
  ffmpeg -y -loglevel error -i "disc/${tag}.aac" -f null - 2>&1 \
      | grep -icE 'error|invalid|exceeds|overflow|illegal' || true
}

printf '%-34s %-30s %6s %6s  %s\n' "probka" "flagi" "PRZED" "PO" "dyskryminuje"
for src in A_antiphase_eps0.50.wav A_antiphase_eps0.20.wav E_phase_transition.wav \
           A_antiphase_eps0.05.wav real/M2.wav real/M6.wav; do
  b=$(basename "$src" .wav)
  for v in "|dflt" "--ps-env 4 --ps-env-reduce 0|e4"; do
    args="${v%|*}"; tag="${b}_${v#*|}"
    # shellcheck disable=SC2086
    o=$(errs "$OLD" "$src" "old_$tag" --ps-ipd 1 --ps-opd 1 $args)
    # shellcheck disable=SC2086
    n=$(errs "$NEW" "$src" "new_$tag" --ps-ipd 1 --ps-opd 1 $args)
    if [ "$o" -gt 0 ] && [ "$n" = "0" ]; then d="TAK <<<<"
    elif [ "$o" -gt 0 ] && [ "$n" -gt 0 ]; then d="oba zle (inny watek)"
    else d="-"; fi
    printf '%-34s %-30s %6s %6s  %s\n' "$b" "${args:-brak}" "$o" "$n" "$d"
  done
done
