#!/bin/bash
# Lokalny cross-build Windows (x64 + x86) ze swiezych drzew, jak w CI.
# Wynik: fdkaac-franken-x64.exe i fdkaac-franken-x86.exe w katalogu projektu.
# Uzycie: bash tools/build_win.sh [x64|x86|both]
set -eu
P="$(cd "$(dirname "$0")/.." && pwd)"
WANT=${1:-both}
B=$HOME/.hermes/cache/scratch/franken-winbuild
build(){
  local arch=$1 host=$2 R="$B/$1"
  rm -rf "$R"; mkdir -p "$R"
  cp -r "$P/src-fdk-aac" "$R/lib"; cp -r "$P/src-fdkaac" "$R/front"
  ( cd "$R/lib" && autoreconf -fi >/dev/null 2>&1 &&
    ./configure --host="$host" --prefix="$R/inst" --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2 >/dev/null &&
    make -j"$(nproc)" >"$R/lib.log" 2>&1 && make install >/dev/null 2>&1 ) || { echo "BLAD lib $arch"; tail -20 "$R/lib.log"; return 1; }
  ( cd "$R/front" && (autoreconf -fi >/dev/null 2>&1 || true) &&
    PKG_CONFIG_PATH="$R/inst/lib/pkgconfig" ./configure --host="$host" CFLAGS="-O2 -I$R/inst/include" LDFLAGS="-static -static-libgcc -L$R/inst/lib" >/dev/null &&
    make -j"$(nproc)" >"$R/front.log" 2>&1 ) || { echo "BLAD front $arch"; tail -20 "$R/front.log"; return 1; }
  cp "$R/front/fdkaac.exe" "$P/fdkaac-franken-$arch.exe"
  "$host-strip" "$P/fdkaac-franken-$arch.exe" || true
  echo "OK $arch $(stat -c%s "$P/fdkaac-franken-$arch.exe") B md5 $(md5sum "$P/fdkaac-franken-$arch.exe" | cut -c1-12)"
}
case "$WANT" in
  x64) build x64 x86_64-w64-mingw32 ;;
  x86) build x86 i686-w64-mingw32 ;;
  both) build x64 x86_64-w64-mingw32; build x86 i686-w64-mingw32 ;;
esac
