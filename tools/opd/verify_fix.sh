#!/bin/bash
# WERYFIKACJA NAPRAWY delta-time OPD.
# 1. probka, ktora BYLA zepsuta (eps0.50) musi dekodowac sie czysto
# 2. probka E_phase_transition (drugi watek) tez
# 3. WSZYSTKIE probki syntetyczne + realne = 0 bledow dekodera
# 4. NO-REGRESSION: bez --ps-ipd wyjscie ADTS bit-identyczne (opt-in!)
# 5. --ps-opd 0 vs 1 nadal daje ROZNE strumienie (OPD trafia do bitstreamu)
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
PREV="$HOME/aacfdk_native/front/fdkaac.prebugfix"
mkdir -p fix

echo "=== (1-3) bledy dekodera po naprawie ==="
echo "UWAGA METODOLOGICZNA: faad pisze binarny pasek postepu na stderr, wiec
naiwny grep 'error|warning' daje setki FALSZYWYCH trafien - identycznie w
baseline BEZ IPD (zmierzone: 196 dla --ps-ipd 0). Dlatego dla faad porownujemy
z BASELINE tej samej probki, a nie z zerem, i czyscimy strumien z bajtow
niedrukowalnych."
printf '%-28s %8s %10s %8s\n' "probka" "opd1_ff" "faad_vs_base" "env4_ff"
fail=0
for f in *.wav real/M*.wav; do
  b=$(basename "$f" .wav)
  "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 -o "fix/$b.m4a" "$f" 2>/dev/null
  e1=$(ffmpeg -y -v warning -i "fix/$b.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  # baseline BEZ IPD ta sama probka - punkt odniesienia dla faad
  "$ENC" -p 29 -b 48000 --ps-ipd 0 -o "fix/${b}_base.m4a" "$f" 2>/dev/null
  fa_new=$(faad -o /dev/null "fix/$b.m4a" 2>&1 | tr -cd '[:print:]\n' \
      | grep -ciE 'error|illegal|invalid' || true)
  fa_base=$(faad -o /dev/null "fix/${b}_base.m4a" 2>&1 | tr -cd '[:print:]\n' \
      | grep -ciE 'error|illegal|invalid' || true)
  # wymus 4 obwiednie - sciezka, ktora wywolywala bug
  "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 \
      -o "fix/${b}_e4.m4a" "$f" 2>/dev/null
  e4=$(ffmpeg -y -v warning -i "fix/${b}_e4.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-28s %8s %10s %8s\n' "$b" "$e1" "${fa_new}/${fa_base}" "$e4"
  [ "$e1" = "0" ] && [ "$e4" = "0" ] && [ "$fa_new" -le "$fa_base" ] || fail=1
done
echo
echo "WYNIK: $([ $fail -eq 0 ] && echo 'WSZYSTKIE CZYSTE' || echo 'NADAL SA BLEDY')"

echo
echo "=== (4) NO-REGRESSION: bez --ps-ipd musi byc bit-identycznie (ADTS!) ==="
for f in A_antiphase_eps0.50.wav real/M2.wav; do
  b=$(basename "$f" .wav)
  "$ENC" -p 29 -b 48000 -f 2 -o "fix/${b}_noipd_new.aac" "$f" 2>/dev/null
  if [ -x "$PREV" ]; then
    "$PREV" -p 29 -b 48000 -f 2 -o "fix/${b}_noipd_old.aac" "$f" 2>/dev/null
    if cmp -s "fix/${b}_noipd_new.aac" "fix/${b}_noipd_old.aac"; then
      echo "  $b: BIT-IDENTYCZNE z binarka przed naprawa"
    else
      echo "  $b: !!! ROZNI SIE - naprawa NIE jest opt-in"
    fi
  else
    echo "  $b: brak binarki referencyjnej (md5 $(md5sum "fix/${b}_noipd_new.aac" | cut -c1-12))"
  fi
done

echo
echo "=== (5) --ps-opd 0 vs 1 musza dac ROZNE strumienie (ADTS) ==="
for f in real/M2.wav real/M6.wav; do
  b=$(basename "$f" .wav)
  "$ENC" -p 29 -b 48000 -f 2 --ps-ipd 1 --ps-opd 0 -o "fix/${b}_o0.aac" "$f" 2>/dev/null
  "$ENC" -p 29 -b 48000 -f 2 --ps-ipd 1 --ps-opd 1 -o "fix/${b}_o1.aac" "$f" 2>/dev/null
  if cmp -s "fix/${b}_o0.aac" "fix/${b}_o1.aac"; then
    echo "  $b: !!! IDENTYCZNE - OPD nie trafia do bitstreamu"
  else
    echo "  $b: roznia sie (OPD realnie kodowane) OK"
  fi
done
