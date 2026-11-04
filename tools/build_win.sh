#!/bin/bash
# Lokalny cross-build Windows (x64 + x86) ze swiezych drzew, jak w CI,
# z pelnym zestawem optymalizacji: -O3 -DNDEBUG -flto + PGO.
#
# Fazy na kazda architekture:
#   1. build instrumentowany (-fprofile-generate), lib i front
#   2. trening: EXE przez interop WSL, CWD na D:, material treningowy
#      z D: (fallback C:); profile .gcda same wpadaja do /mnt/d/home
#      (sciezki absolutne buildu rozwiązane wzgledem dysku D:)
#   3. build docelowy (-fprofile-use -fprofile-correction)
#
# Wyjscie enkodera jest przy tym BIT-IDENTYCZNE z buildem -O2 (pomiary
# 03.11.2026; szczegoly: references/szybkosc-flagi-pgo.md w skillu projektu).
# stdin/stdout kodera nie sa dotykane: zadnych zmian buforow potokow.
#
# Wynik: fdkaac-franken-x64.exe i fdkaac-franken-x86.exe w katalogu projektu.
# Uzycie: bash tools/build_win.sh [x64|x86|both]
# Pominiecie PGO (tylko -O3 -DNDEBUG -flto): FRANKEN_NO_PGO=1
set -euo pipefail
P="$(cd "$(dirname "$0")/.." && pwd)"
WANT=${1:-both}
B=$HOME/.hermes/cache/scratch/franken-winbuild
FLAGS_BASE="-O3 -DNDEBUG -flto"
FLAGS_USE="-fprofile-use -fprofile-correction"
TRAIN=/mnt/d/.franken-pgo-train
[ -d /mnt/d ] || TRAIN=/mnt/c/.franken-pgo-train

train_prepare() {
  rm -rf "$TRAIN"; mkdir -p "$TRAIN"
  local SRC=$P/_bs/bbias
  if [ -f "$SRC/dance.wav" ] && [ -f "$SRC/ld.wav" ]; then
    cp "$SRC/dance.wav" "$SRC/ld.wav" "$TRAIN/"
    # dlugi plik ok. 5 min: 10x dance, zeby profile ustabilizowaly galezie
    python3 - "$TRAIN" <<'PYEOF'
import sys, wave
t = sys.argv[1]
w = wave.open(t + '/dance.wav'); p = w.getparams(); d = w.readframes(w.getnframes())
o = wave.open(t + '/long.wav', 'wb'); o.setparams(p)
for _ in range(10): o.writeframes(d)
o.close()
PYEOF
  else
    # material syntetyczny (deterministyczny), gdy brak plikow z _bs
    python3 - "$TRAIN" <<'PYEOF'
import sys, wave, struct, math, random
t = sys.argv[1]
random.seed(20261103)
for fn, secs in [('dance.wav', 30), ('ld.wav', 30), ('long.wav', 300)]:
    o = wave.open(t + '/' + fn, 'wb'); o.setnchannels(2); o.setsampwidth(2); o.setframerate(48000)
    for i in range(secs * 48000):
        env = 0.5 + 0.5 * math.sin(2 * math.pi * 0.5 * i / 48000)
        l = int(12000 * env * math.sin(2 * math.pi * (220 + 880 * (i % 48000) / 48000) * i / 48000))
        r = int(10000 * (2 * random.random() - 1))
        o.writeframes(struct.pack('<hh', l, r))
    o.close()
PYEOF
  fi
}

train_run() {
  local R=$1 EXE="$1/front/fdkaac.exe"
  # Profile zbieramy BEZ GCOV_PREFIX: mingw libgcov rozwiazuje absolutne
  # sciezki z czasu kompilacji wzgledem DYSKU katalogu roboczego. CWD na D:
  # (NTFS) -> profile laduja w /mnt/d/home/<drzewo-budowy>, a mkdir na
  # zwyklym dysku dziala (po UNC \\wsl.localhost libgcov dostaje odmowe
  # tworzenia katalogow i calymi plikami Skip; WSLENV z losowa skutecznoscia
  # przekazuje zmienne, wiec w ogole go nie dotykamy).
  rm -rf /mnt/d/home
  local TW; TW=$(wslpath -w "$TRAIN"); local O="$TW\\out"
  mkdir -p "$TRAIN/out"
  cd /mnt/d
  "$EXE" -S -f 2 -b 128000          -o "$O\\t1.aac" "$TW\\dance.wav"
  "$EXE" -S -f 2 -p 5 -b 48000      -o "$O\\t2.aac" "$TW\\dance.wav"
  "$EXE" -S -f 2 -p 29 -b 32000     -o "$O\\t3.aac" "$TW\\dance.wav"
  "$EXE" -S -f 2 -b 64000           -o "$O\\t4.aac" "$TW\\ld.wav"
  "$EXE" -S -f 2 -b 128000          -o "$O\\t6.aac" "$TW\\long.wav"
  "$EXE" -S -f 2 --resample 44100 -b 96000 -o "$O\\t7.aac" "$TW\\dance.wav"
  "$EXE" -S --dab -b 96             -o "$O\\t8.dab" "$TW\\dance.wav"
  cd "$R"
  # profile z powrotem na miejsca w drzewie budowy (identyczny ksztalt
  # sciezek, minus korzen dysku)
  rsync -a "/mnt/d$R/" "$R/"
  rm -rf /mnt/d/home
  echo "  gcda w drzewie: $(find "$R" -name '*.gcda' | wc -l)"
}

build(){
  local arch=$1 host R="$B/$1" GAR
  case $arch in x64) host=x86_64-w64-mingw32 ;; x86) host=i686-w64-mingw32 ;; esac
  GAR="$host-gcc-ar"
  rm -rf "$R"; mkdir -p "$R"
  cp -r "$P/src-fdk-aac" "$R/lib"; cp -r "$P/src-fdkaac" "$R/front"

  local GEN="$FLAGS_BASE -fprofile-generate"
  echo "== $arch: faza 1/3 (instrumentacja)"
  ( cd "$R/lib" && autoreconf -fi >/dev/null 2>&1 &&
    ./configure --host="$host" --prefix="$R/inst" --enable-static --disable-shared \
      CFLAGS="$GEN" CXXFLAGS="$GEN" >/dev/null &&
    make -j"$(nproc)" AR="$GAR" RANLIB="$host-gcc-ranlib" >"$R/lib1.log" 2>&1 &&
    make install AR="$GAR" RANLIB="$host-gcc-ranlib" >/dev/null 2>&1 ) || { echo "BLAD lib1 $arch"; tail -20 "$R/lib1.log"; return 1; }
  ( cd "$R/front" && (autoreconf -fi >/dev/null 2>&1 || true) &&
    PKG_CONFIG_PATH="$R/inst/lib/pkgconfig" ./configure --host="$host" \
      CFLAGS="$GEN -I$R/inst/include" \
      LDFLAGS="-static -static-libgcc -flto -O3 -fprofile-generate -L$R/inst/lib" >/dev/null &&
    make -j"$(nproc)" >"$R/front1.log" 2>&1 ) || { echo "BLAD front1 $arch"; tail -20 "$R/front1.log"; return 1; }

  if [ "${FRANKEN_NO_PGO:-}" != 1 ]; then
    echo "== $arch: faza 2/3 (trening PGO przez interop)"
    train_run "$R"
    local USE="$FLAGS_BASE $FLAGS_USE"
  else
    local USE="$FLAGS_BASE"
  fi

  echo "== $arch: faza 3/3 (build docelowy)"
  ( cd "$R/lib" && make clean >/dev/null 2>&1 &&
    make -j"$(nproc)" AR="$GAR" RANLIB="$host-gcc-ranlib" \
      CFLAGS="$USE" CXXFLAGS="$USE" >"$R/lib2.log" 2>&1 &&
    make install AR="$GAR" RANLIB="$host-gcc-ranlib" >/dev/null 2>&1 ) || { echo "BLAD lib2 $arch"; tail -20 "$R/lib2.log"; return 1; }
  ( cd "$R/front" && make clean >/dev/null 2>&1 &&
    make -j"$(nproc)" \
      CFLAGS="$USE -I$R/inst/include" \
      LDFLAGS="-static -static-libgcc -flto -O3 $([ "${FRANKEN_NO_PGO:-}" = 1 ] || echo "$FLAGS_USE") -L$R/inst/lib" >"$R/front2.log" 2>&1 ) || { echo "BLAD front2 $arch"; tail -20 "$R/front2.log"; return 1; }

  cp "$R/front/fdkaac.exe" "$P/fdkaac-franken-$arch.exe"
  "$host-strip" "$P/fdkaac-franken-$arch.exe" || true
  echo "OK $arch $(stat -c%s "$P/fdkaac-franken-$arch.exe") B md5 $(md5sum "$P/fdkaac-franken-$arch.exe" | cut -c1-12)"
}

case "$WANT" in
  x64)  [ "${FRANKEN_NO_PGO:-}" = 1 ] || train_prepare; build x64 ;;
  x86)  [ "${FRANKEN_NO_PGO:-}" = 1 ] || train_prepare; build x86 ;;
  both) [ "${FRANKEN_NO_PGO:-}" = 1 ] || train_prepare; build x64; build x86 ;;
esac
