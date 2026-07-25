#!/usr/bin/env bash
# ============================================================================
# DAB+ verification harness for Franken FDK AAC
# ----------------------------------------------------------------------------
# Cel: NIEZALEZNY dowod, ze wygenerowana super-ramka DAB+ jest poprawna.
# Dekoder = faad2 (przez dablin), czyli INNA implementacja AAC niz nasz FDK -
# symetryczny blad 960/super-ramki sie NIE ukryje.
#
# Dwie sciezki dekodowania (obie faad2, ale rozne wejscia):
#   A) super-ramka .dabp -> odr-dabmux -> ETI -> dablin -> PCM/WAV   (glowna)
#   B) (tylko dla odr-audioenc) --decode loopback do WAV             (pomocnicza)
#
# Wywolanie:
#   tools/dab/verify_dabplus.sh <superframe.dabp> <bitrate_kbps> <ref_in.wav>
# np.
#   tools/dab/verify_dabplus.sh out.dabp 96 ref_tones.wav
#
# Wymaga (rozpakowane bez roota w $DABTOOLS, patrz setup_dab_tools.sh):
#   - odr-dabmux, dablin  (apt-get download + dpkg -x)
#   - ffmpeg, python3+numpy
# ============================================================================
set -euo pipefail

DABP="${1:?podaj plik .dabp}"
BR="${2:?podaj bitrate kbps (wielokrotnosc 8)}"
REFIN="${3:-}"

# Katalog z rozpakowanymi narzedziami ODR (edytuj jesli inny):
DABTOOLS="${DABTOOLS:-/tmp/dabtools/root/usr}"
DABMUX="$DABTOOLS/bin/odr-dabmux"
DABLIN="$DABTOOLS/bin/dablin"
export LD_LIBRARY_PATH="$DABTOOLS/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

SUBCH=$((BR/8))
SFSIZE=$((SUBCH*120))
FSIZE=$(stat -c%s "$DABP")
echo "== struktura super-ramki =="
echo "  bitrate=$BR kbps  subchannel_index=$SUBCH  rozmiar SF=$SFSIZE B"
echo "  plik=$FSIZE B  reszta mod SF=$((FSIZE % SFSIZE)) (0 = spojne)"
if [ $((FSIZE % SFSIZE)) -ne 0 ]; then
  echo "  BLAD: rozmiar pliku nie jest wielokrotnoscia rozmiaru super-ramki!"
  exit 2
fi

# --- mux do ETI ---
cat > "$WORK/mux.mux" <<EOF
general { dabmode 1 nbframes 250 }
remotecontrol { telnetport 0 }
ensemble { id 0x4fff ecc 0xec label "TEST" shortlabel "TEST" international-table 1 }
services { srv-audio { label "AUD" shortlabel "AUD" pty 0 language 0 } }
subchannels { sub-audio {
    type dabplus
    inputfile "$(readlink -f "$DABP")"
    nonblock false
    bitrate $BR
    id 1
    protection 3
} }
components { comp-audio { type 0 service srv-audio subchannel sub-audio } }
outputs { out1 "file://$WORK/out.eti?type=raw" }
EOF
"$DABMUX" "$WORK/mux.mux" >"$WORK/mux.log" 2>&1 || { echo "odr-dabmux FAIL"; tail "$WORK/mux.log"; exit 3; }

# --- dablin (faad2) dekoduje ETI -> PCM ---
"$DABLIN" -f eti -1 -p "$WORK/out.eti" > "$WORK/out.pcm" 2>"$WORK/dablin.log" || { echo "dablin FAIL"; tail "$WORK/dablin.log"; exit 4; }
grep -E "Superframe sync succeeded|AACDecoder|format:" "$WORK/dablin.log" | sed 's/^/  dablin: /'

ffmpeg -hide_banner -loglevel error -f f32le -ar 48000 -ac 2 -i "$WORK/out.pcm" -c:a pcm_s16le "$WORK/out.wav"
echo "== dekodowanie OK; WAV: $(stat -c%s "$WORK/out.wav") B =="

# --- pomiar tresci vs oryginal (jesli podano ref_in.wav) ---
if [ -n "$REFIN" ] && [ -f "$REFIN" ]; then
  HERE="$(cd "$(dirname "$0")" && pwd)"
  python3 "$HERE/measure_content.py" "$REFIN" "$WORK/out.wav"
else
  echo "(pomiar tresci pominiety - nie podano ref_in.wav)"
fi
echo "VERIFY_DABPLUS_DONE"
