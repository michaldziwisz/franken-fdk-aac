#!/usr/bin/env bash
# ============================================================================
# Setup narzedzi weryfikacyjnych DAB+ w WSL BEZ ROOTA (apt-get download + dpkg -x)
# ----------------------------------------------------------------------------
# Instaluje lokalnie (do $DABTOOLS, domyslnie /tmp/dabtools/root):
#   - dablin       (DAB/DAB+ receiver client; dekoder AAC = faad2, NIEZALEZNY od FDK)
#   - odr-dabmux   (mux super-ramek -> ETI)
# Zaleznosci runtime (libfaad2 itd.) sa zwykle juz w systemie; jesli nie,
# dolozy je ten sam mechanizm.
#
# NIEZALEZNOSC: dablin uzywa faad2, nie FDK - to celowe. Gdyby weryfikator
# uzywal tego samego FDK co nasz enkoder, symetryczny blad (np. zla siatka 960)
# przeszedlby niezauwazony. faad2 to druga, niezalezna implementacja AAC.
# ============================================================================
set -euo pipefail
DABTOOLS_BASE="${DABTOOLS_BASE:-/tmp/dabtools}"
mkdir -p "$DABTOOLS_BASE" && cd "$DABTOOLS_BASE"
echo "== pobieram pakiety (bez roota) =="
apt-get download dablin odr-dabmux 2>&1 | grep -E 'Get:|Fetched' || true
echo "== rozpakowuje lokalnie =="
mkdir -p root
for d in *.deb; do dpkg -x "$d" root/; done
echo "== weryfikacja =="
export LD_LIBRARY_PATH="$DABTOOLS_BASE/root/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"
MISS=0
for b in dablin odr-dabmux; do
  P="$DABTOOLS_BASE/root/usr/bin/$b"
  if [ -x "$P" ] && ! ldd "$P" 2>/dev/null | grep -q 'not found'; then
    echo "  OK: $b"
  else
    echo "  BRAK/zaleznosci: $b"; ldd "$P" 2>/dev/null | grep 'not found' || true; MISS=1
  fi
done
echo "== faad2 (dekoder) =="; (command -v faad && faad 2>&1 | head -1) || echo "  faad brak - apt-get download faad libfaad2"
[ $MISS -eq 0 ] && echo "SETUP_DAB_TOOLS_OK" || echo "SETUP_DAB_TOOLS_PARTIAL (dolozyc brakujace .deb)"
echo "Ustaw: export DABTOOLS=$DABTOOLS_BASE/root/usr"
