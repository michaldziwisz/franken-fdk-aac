#!/bin/bash
# NO-REGRESSION na finalnych .exe: bez flag IPD wyjscie musi byc bit-identyczne
# z binarka SPRZED naprawy. Dowodzi, ze poprawka jest czysto w sciezce OPT-IN.
# ADTS (-f2), nie M4A - kontener M4A ma zmienny tag czasu (falszywa regresja).
set -u
cd /mnt/d/projekty/aacfdk
mkdir -p tools/opd/reg
NEWX=./fdkaac-franken-x64.exe
OLDX=./tools/opd/reg/old-x64.exe

# binarka referencyjna: zbuduj ze zrodel .orig w drzewie win, jesli nie ma kopii
if [ ! -f "$OLDX" ]; then
  if [ -f ./tools/opd/reg/prev-x64.exe ]; then cp ./tools/opd/reg/prev-x64.exe "$OLDX"; fi
fi

python3 - tools/opd/reg/nr.wav <<'PY'
import wave, struct, math, random, sys
sr=44100; random.seed(3); c=lambda x:max(-32768,min(32767,int(x)))
fr=[]
for i in range(sr*2):
    t=i/sr
    l=9000*(math.sin(2*math.pi*440*t)+0.3*random.uniform(-1,1))
    r=9000*(math.sin(2*math.pi*554*t)+0.3*random.uniform(-1,1))
    fr.append(struct.pack('<hh',c(l),c(r)))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY

echo "=== warianty BEZ IPD (musza byc identyczne z wersja przed naprawa) ==="
for args in "-p2 -b128000" "-p29 -b48000" "-p29 -b48000 --ps-env 4 --ps-env-reduce 0" \
            "-p5 -b64000"; do
  tag=$(echo "$args" | tr -d ' -')
  # shellcheck disable=SC2086
  $NEWX $args -f2 -o "tools/opd/reg/new_$tag.aac" tools/opd/reg/nr.wav 2>/dev/null
  m=$(md5sum "tools/opd/reg/new_$tag.aac" 2>/dev/null | cut -c1-12)
  printf '  %-46s md5=%s\n' "$args" "$m"
done

echo
echo "=== to samo z natywnych binarek (dokladne porownanie PRZED/PO) ==="
NEWN="$HOME/aacfdk_native/front/fdkaac"
OLDN="$HOME/aacfdk_native/front/fdkaac.prebugfix"
ok=1
for args in "-p2 -b128000" "-p29 -b48000" "-p29 -b48000 --ps-env 4 --ps-env-reduce 0" \
            "-p5 -b64000" "-p29 -b24000" "-p23 -b64000"; do
  tag=$(echo "$args" | tr -d ' -')
  # shellcheck disable=SC2086
  "$OLDN" $args -f2 -o "tools/opd/reg/o_$tag.aac" tools/opd/reg/nr.wav 2>/dev/null
  # shellcheck disable=SC2086
  "$NEWN" $args -f2 -o "tools/opd/reg/n_$tag.aac" tools/opd/reg/nr.wav 2>/dev/null
  # WAZNE: brak pliku != regresja. Czesc kombinacji profil/bitrate/format FDK
  # odrzuca ("encoder initialization failed") - i robi to w OBU binarkach.
  # cmp na dwoch nieistniejacych plikach zglasza roznice i daje FALSZYWY ALARM
  # "zmiana widoczna bez flag" (tak wlasnie zachowal sie -p39 -b64000 -f2).
  if [ ! -s "tools/opd/reg/o_$tag.aac" ] && [ ! -s "tools/opd/reg/n_$tag.aac" ]; then
    printf '  %-46s POMINIETE (nieobslugiwana kombinacja w obu)\n' "$args"
    continue
  fi
  if [ ! -s "tools/opd/reg/o_$tag.aac" ] || [ ! -s "tools/opd/reg/n_$tag.aac" ]; then
    printf '  %-46s !!! plik powstal tylko w JEDNEJ wersji\n' "$args"; ok=0
    continue
  fi
  if cmp -s "tools/opd/reg/o_$tag.aac" "tools/opd/reg/n_$tag.aac"; then
    printf '  %-46s BIT-IDENTYCZNE\n' "$args"
  else
    printf '  %-46s !!! ROZNI SIE\n' "$args"; ok=0
  fi
done
echo
echo "WYNIK: $([ $ok -eq 1 ] && echo 'naprawa jest czysto OPT-IN' || echo 'UWAGA: zmiana widoczna bez flag')"
