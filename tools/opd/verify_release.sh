#!/bin/bash
# WERYFIKACJA WYDANEGO ARTEFAKTU (self-report CI nie wystarcza).
# PULAPKA ze skilla: po unzip .exe NIE MA prawa wykonywania -> --help daje
# "Permission denied", co wyglada jak katastrofa, a to tylko chmod.
set -u
cd /mnt/d/projekty/aacfdk
mkdir -p tools/opd/rel && cd tools/opd/rel
rm -f ./*.zip ./*.exe 2>/dev/null || true

gh release download v1.3.1 -p '*x64*.zip' --clobber
z=$(ls ./*x64*.zip | head -1)
echo "pobrany: $z ($(stat -c%s "$z") B)"
unzip -o -q "$z"
exe=$(find . -name '*.exe' | head -1)
chmod +x "$exe"          # KONIECZNE, inaczej falszywy alarm
echo "exe: $exe ($(stat -c%s "$exe") B)"
echo

echo "=== przelaczniki fazy obecne w wydanej binarce ==="
for s in --ps-ipd --ps-opd --ps-bands --ps-env --ps-env-reduce; do
  "$exe" --help 2>&1 | grep -q -- "$s" && echo "  $s OK" || echo "  $s BRAK"
done

echo
echo "=== realne kodowanie + dekodowanie WYDANA binarka ==="
cp ../real/M5.wav ./t.wav 2>/dev/null || cp ../A_antiphase_eps0.00.wav ./t.wav
for v in "" "--ps-env 4 --ps-env-reduce 0"; do
  # shellcheck disable=SC2086
  "$exe" -p29 -b48000 --ps-ipd 1 --ps-opd 1 $v -o out.m4a t.wav 2>/dev/null
  n=$(ffmpeg -y -loglevel error -i out.m4a -f null - 2>&1 \
      | grep -icE 'error|invalid|exceeds|overflow|illegal' || true)
  printf '  %-32s rozmiar=%s bledow_dekodera=%s\n' "${v:-domyslne}" \
      "$(stat -c%s out.m4a)" "$n"
done

echo
echo "=== dowod ze NAPRAWA jest w srodku: stara binarka vs wydana ==="
OLD="$HOME/aacfdk_native/front/fdkaac.prebugfix"
python3 - psn.wav <<'PY'
import wave, struct, math, random, sys
sr=44100; rnd=random.Random(5); c=lambda x:max(-32768,min(32767,int(x)))
st=0.0; fr=[]
for i in range(sr*3):
    t=i/sr
    v=(math.sin(2*math.pi*70*t)+0.8*math.sin(2*math.pi*150*t)
       +0.6*math.sin(2*math.pi*260*t)+0.4*math.sin(2*math.pi*480*t))
    st=0.97*st+0.03*rnd.uniform(-1,1)
    s=(v+5.0*st)*(0.6+0.4*math.sin(2*math.pi*0.3*t))
    fr.append(struct.pack('<hh', c(4200*s), c(-4200*s)))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY
"$OLD" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 -o o.aac psn.wav 2>/dev/null
"$exe" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 --ps-env 4 --ps-env-reduce 0 -o n.aac psn.wav 2>/dev/null
eo=$(ffmpeg -y -loglevel error -i o.aac -f null - 2>&1 | grep -icE 'overflow|illegal' || true)
en=$(ffmpeg -y -loglevel error -i n.aac -f null - 2>&1 | grep -icE 'overflow|illegal' || true)
echo "  binarka SPRZED naprawy: $eo bledow"
echo "  WYDANA binarka v1.3.1 : $en bledow"
[ "$eo" -gt 0 ] && [ "$en" = "0" ] && echo "  => NAPRAWA POTWIERDZONA W WYDANYM ARTEFAKCIE" \
    || echo "  => UWAGA: nie potwierdzono roznicy"
