#!/usr/bin/env bash
# ============================================================================
# Buduje ZLOTY WZORZEC referencyjny DAB+ przy uzyciu ODR-AudioEnc
# (fork Opendigitalradio/fdk-aac-dabplus, ktory zawiera odr-audioenc).
# ----------------------------------------------------------------------------
# Sluzy DWOM celom:
#  1. Dostarcza poprawny .dabp do walidacji naszego harnessu (verify_dabplus.sh).
#  2. Daje referencyjna binarke do porownan A/B, gdy zbudujemy WLASNY --dab.
#
# Buduj na NATYWNYM FS (~/dab_ref), NIE na /mnt/d (DrvFs psuje symlinki libtoola).
# ZeroMQ: fork wymaga -lzmq w configure; jesli brak libzmq3-dev (root), uzywamy
# symlinku do systemowej libzmq.so.5 (patrz ponizej) - link sie udaje.
# ============================================================================
set -euo pipefail
REF="${REF:-$HOME/dab_ref}"
mkdir -p "$REF" && cd "$REF"

if [ ! -x "$REF/inst/bin/odr-audioenc" ]; then
  echo "== klon fork =="
  [ -d fdk-aac-dabplus ] || git clone --depth 1 https://github.com/Opendigitalradio/fdk-aac-dabplus.git
  # zmq: symlink do systemowej shared lib + naglowki z .deb (bez roota)
  mkdir -p zmqlink && cd zmqlink
  ln -sf /usr/lib/x86_64-linux-gnu/libzmq.so.5 libzmq.so
  if [ ! -f zmq.h ]; then
    (cd /tmp && apt-get download libzmq3-dev 2>/dev/null && dpkg -x libzmq3-dev*.deb /tmp/zmqhdr) || true
    cp /tmp/zmqhdr/usr/include/zmq*.h . 2>/dev/null || echo "UWAGA: brak zmq.h - dostarcz recznie"
  fi
  cd "$REF/fdk-aac-dabplus"
  ./bootstrap
  Z="$REF/zmqlink"
  ./configure --prefix="$REF/inst" --enable-static --disable-shared --disable-example \
      CFLAGS="-O2 -I$Z" CXXFLAGS="-O2 -I$Z" LDFLAGS="-L$Z"
  make -j"$(nproc)"
  make install
fi
echo "== odr-audioenc gotowy: $REF/inst/bin/odr-audioenc =="

# --- wygeneruj wzorcowy sygnal i strumienie ---
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
cd "$REF"
# tony w obu kanalach (do dowodu tresci per-sample); pink noise osobno (do stereo/energii)
ffmpeg -hide_banner -loglevel error -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=5" \
  -f lavfi -i "sine=frequency=1000:sample_rate=48000:duration=5" \
  -filter_complex "[0:a][1:a]join=inputs=2:channel_layout=stereo" -c:a pcm_s16le -y ref_tones.wav
BR="${1:-96}"
"$REF/inst/bin/odr-audioenc" -i ref_tones.wav -f wav -b "$BR" -r 48000 -c 2 \
   -o ref_tones.dabp --decode=ref_tones_loop.wav 2>&1 | grep -iE 'framelen|bitrate|type'
echo "Wzorzec: $REF/ref_tones.dabp (bitrate $BR)  + loopback ref_tones_loop.wav"
echo "GOLDEN_REF_DONE"
