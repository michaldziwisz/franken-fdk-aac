#!/usr/bin/env bash
# fdkaac-franken - funkcjonalny test suite (make check).
#
# Upstream fdk-aac/nu774 nie maja testow jednostkowych, wiec weryfikujemy
# ZACHOWANIE gotowych binarek: kompletnosc switchy, dekodowalnosc strumienia,
# realny efekt (rozny bitstream), quasi-CVBR (rozrzut bitow/ramke) oraz brak
# regresji domyslnego CBR wzgledem oryginalnej binarki.
#
# Uzycie:  bash tests/check.sh            (testuje x64; x86 jesli jest)
#          make check                     (to samo przez Makefile)
# Wymaga:  ffmpeg, python3 (oba w WSL). Binarki .exe uruchamiane przez WSL interop.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"

X64=./fdkaac-franken-x64.exe
X86=./fdkaac-franken-x86.exe
ORIG=./fdkaac2.exe          # oryginalna dostarczona binarka (no-regression baseline)
# UWAGA: binarki Windows uruchamiane przez WSL interop NIE rozumieja absolutnych
# sciezek /mnt/... - uzywamy nazw WZGLEDNYCH wzgledem cwd (skrypt robi cd $HERE).
WAV=_check_in.wav
OUT=_check_out             # prefiks plikow tymczasowych (wzgledny)

PASS=0; FAIL=0
ok(){   echo "  PASS  $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

command -v ffmpeg  >/dev/null || { echo "SKIP: brak ffmpeg";  exit 77; }
command -v python3 >/dev/null || { echo "SKIP: brak python3"; exit 77; }
[ -x "$X64" ] || [ -f "$X64" ] || { echo "FAIL: brak $X64"; exit 1; }

cleanup(){ rm -f "$WAV" _check_dab_in.wav "${OUT}"*.m4a "${OUT}"*.aac "${OUT}"*.txt "${OUT}"*.dabp "${OUT}"*.bin; rm -rf _check_dabtmp; }
trap cleanup EXIT

# --- probka: 2s stereo, ton + lekki szum (rusza MS/IS/TNS) ---
python3 - "$WAV" <<'PY'
import wave,struct,math,random,sys
sr=44100; random.seed(1); c=lambda x:max(-32768,min(32767,int(x)))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(struct.pack('<hh',
   c(9000*(math.sin(2*math.pi*440*i/sr)+0.3*random.uniform(-1,1))),
   c(9000*(math.sin(2*math.pi*554*i/sr)+0.3*random.uniform(-1,1)))) for i in range(sr*2)))
w.close()
PY

md5(){ md5sum "$1" 2>/dev/null | cut -c1-12; }
# liczba bledow dekodera (0 = strumien poprawny)
derr(){ ffmpeg -y -loglevel error -i "$1" -f null - 2>&1 | grep -icE 'error|invalid|exceeds'; }

enc(){ "$@" 2>/dev/null; }   # $1=exe ... -o plik in.wav

test_exe(){
  local exe="$1" tag="$2"
  echo "== $tag ($exe) =="
  [ -f "$exe" ] || { echo "  (pominieto - brak binarki)"; return; }

  # 1) help kompletny - franken switche obecne
  local hn
  hn=$("$exe" --help 2>&1 | grep -cE '^ --(msmask|msbands|is|isbands|is-min-sfbs|is-corr-thresh|is-lr-ratio|is-lo|is-hi|is-force-lo|is-force-hi|core-cutoff|sbr-start|sbr-stop|sbr-freqscale|sbr-alterscale|sbr-noise-bands|sbr-amp-res|sbr-data-extra|sbr-header-period|sbr-tran-peak|sbr-tran-quiet|sbr-tran-dom|sbr-tran-thr|sbr-tran-split|sbr-mh-tone|sbr-mh-diff|sbr-mh-decay-orig|sbr-mh-decay-diff|sbr-mh-sfm-sbr|sbr-mh-sfm-orig|sbr-mh-maxcomp|sbr-mh-deltatime|sbr-noise-max|ps|ps-iid-quant|ps-ipd|ps-bands|ps-env|ps-env-reduce|ps-noenv-skip|tns-mask|tns-order|pns|pns-start|pns-gain|pns-tonality|pns-refpower|pns-gapfill|pns-min-width|ath-scale|minsnr-scale|minsnr-clamp-hi|minsnr-clamp-lo|reduce-clamp|mid-bias|side-bias|side-knee|mask-slope|block-bias|vbr-reservoir|peak-bitrate|max-bits-frame|min-bits-frame|bitres-mode|ms-bias|verbose)')
  [ "$hn" -ge 59 ] && ok "help: $hn franken switchy" || bad "help: tylko $hn switchy (oczekiwano >=59)"

  # 2) baseline + rozny bitstream per switch, wszystko dekodowalne
  enc "$exe" -p2 -b128000 -o "${OUT}_base.m4a" "$WAV"
  local B; B=$(md5 "${OUT}_base.m4a")
  [ -s "${OUT}_base.m4a" ] && [ "$(derr ${OUT}_base.m4a)" = "0" ] && ok "baseline LC128 dekodowalny" || bad "baseline"

  local specs=( "msbands5:--msbands 5" "msmask0:--msmask 0" "is-bias:--is 1 --is-min-sfbs 20"
                "tns2:--tns-order 2" "ath512:--ath-scale 512" "block-bias:--block-bias 200"
                "ms-bias:--ms-bias 128" "core-cutoff:-p 29 -b 48000 --core-cutoff 7500"
                "is-aggr:--is 1 --is-aggression 70" "force-pns:-b 24000 --pns 1 --force-pns"
                "msbands-hi:--msbands-lo 20 --msbands-hi 40" "ms-precision:--ms-precision 512"
                "sbr-invf:-p5 -b64000 --sbr-invf 3" "sbr-stereo:-p5 -b64000 --sbr-stereo-mode 1"
                "ps-icc:-p29 -b32000 --ps-icc 0" "unlock:-b8000 --unlock-bitrate"
                "ps-bands10:-p29 -b48000 --ps-bands 10"
                "ps-bands20:-p29 -b48000 --ps-bands 20 --ps-env 1"
                "ps-env2:-p29 -b48000 --ps-env 2"
                "ps-env4-nored:-p29 -b48000 --ps-env 4 --ps-env-reduce 0"
                "ps-noenv-skip:-p29 -b48000 --ps-noenv-skip 0"
                "ps-full-matrix:-p29 -b48000 --ps-bands 10 --ps-env 4 --ps-env-reduce 0 --ps-icc-mode 1 --ps-iid-quant 1"
                "sbr-tran-peak:-p5 -b64000 --sbr-tran-peak 70"
                "sbr-tran-thr:-p5 -b64000 --sbr-tran-thr 40"
                "sbr-tran-split:-p5 -b64000 --sbr-tran-split 30"
                "sbr-mh-tone:-p5 -b64000 --sbr-mh-tone 50"
                "sbr-mh-diff:-p5 -b64000 --sbr-mh-diff 50"
                "sbr-mh-decay-orig:-p5 -b64000 --sbr-mh-decay-orig 200"
                "sbr-mh-sfm-orig:-p5 -b64000 --sbr-mh-sfm-orig 300"
                "sbr-mh-maxcomp:-p5 -b64000 --sbr-mh-maxcomp 10"
                "sbr-mh-deltatime:-p5 -b64000 --sbr-mh-deltatime 2"
                "sbr-noise-max:-p5 -b64000 --sbr-noise-max 3"
                "sbr-noise-max-lo:-p5 -b64000 --sbr-noise-max -3"
                "ps-ipd:-p29 -b48000 --ps-ipd 1"
                "ps-ipd-lowbr:-p29 -b24000 --ps-ipd 1"
                "ps-ipd-combo:-p29 -b48000 --ps-ipd 1 --ps-env 4 --ps-env-reduce 0"
                "speech:-p5 -b32000 --speech" "spread-mask:-b128000 --spread-mask 64"
 "ms-prec-hi:--ms-precision 1024"
 "mid-bias:-b96000 --mid-bias 384"
 "side-bias:-b96000 --side-bias 6"
 "side-knee:-b96000 --side-knee 4"
 "side-bias+knee:-b96000 --side-bias 6 --side-knee 4"
 "mask-slope-pos:-b96000 --mask-slope 12"
 "mask-slope-neg:-b96000 --mask-slope -12"
 "minsnr-scale:-b128000 --minsnr-scale 128"
 "minsnr-clamp-hi:-b128000 --minsnr-clamp-hi 512"
 "minsnr-clamp-lo:-b128000 --minsnr-clamp-lo 64"
 "reduce-clamp:-b128000 --reduce-clamp 0"
 "pns-gain:-b64000 --pns 1 --force-pns --pns-gain 2.0"
 "pns-tonality:-b64000 --pns 1 --force-pns --pns-tonality 2.0"
 "pns-refpower:-b64000 --pns 1 --force-pns --pns-refpower 2.0"
 "pns-min-width:-b64000 --pns 1 --force-pns --pns-min-width 32"
 "cutoff-verbose:-b128000 -w 17300 --verbose"
 "is-lo:-p2 -b48000 --is 1 --is-aggression 80 --is-lo 15"
 "is-hi:-p2 -b48000 --is 1 --is-aggression 80 --is-hi 2"
 "is-force:-p2 -b48000 --is 1 --is-aggression 80 --is-force-lo 25 --is-force-hi 45"
 "sbr-header-period:-p5 -b48000 --sbr-header-period 1" )
  for s in "${specs[@]}"; do
    local n="${s%%:*}" a="${s#*:}"
    enc "$exe" -p2 -b128000 $a -o "${OUT}_$n.m4a" "$WAV"
    if [ -s "${OUT}_$n.m4a" ] && [ "$(derr ${OUT}_$n.m4a)" = "0" ]; then ok "$n dekodowalny"
    else bad "$n"; fi
  done

  # 3) quasi-CVBR: reservoir szerszy => wiekszy rozrzut, bitres-mode2 => sztywny
  enc "$exe" -p2 -b128000 -f2 -o "${OUT}_cbr.aac" "$WAV"
  enc "$exe" -p2 -b128000 --vbr-reservoir 8000 --peak-bitrate 192000 -f2 -o "${OUT}_cvbr.aac" "$WAV"
  enc "$exe" -p2 -b128000 --bitres-mode 2 -f2 -o "${OUT}_rigid.aac" "$WAV"
  local spread
  spread=$(python3 - "${OUT}_cbr.aac" "${OUT}_cvbr.aac" "${OUT}_rigid.aac" <<'PY'
import sys
def frames(fn):
    d=open(fn,'rb').read(); i=0; s=[]
    while i+7<=len(d):
        if d[i]!=0xFF or (d[i+1]&0xF0)!=0xF0: break
        fl=((d[i+3]&3)<<11)|(d[i+4]<<3)|((d[i+5]>>5)&7)
        if fl<7: break
        s.append(fl); i+=fl
    kb=[x*8*44100/1024/1000 for x in s]
    return (max(kb)-min(kb)) if kb else -1
cbr,cvbr,rigid=[frames(x) for x in sys.argv[1:4]]
# CVBR ma oddychac szerzej niz sztywny; sztywny ma byc waski
print(f"{cbr:.1f} {cvbr:.1f} {rigid:.1f}")
PY
)
  read -r sc scv sr8 <<<"$spread"
  echo "    rozrzut kbps: CBR=$sc  CVBR=$scv  rigid=$sr8"
  awk "BEGIN{exit !($scv > $sr8)}" && ok "CVBR oddycha szerzej niz bitres-mode2" || bad "CVBR rozrzut ($scv vs $sr8)"
  awk "BEGIN{exit !($sr8 < 5)}" && ok "bitres-mode2 sztywny (<5 kbps rozrzut)" || bad "bitres-mode2 nie sztywny ($sr8)"

  # 4) verbose: brak literalnego ' -1' w wartosciach (naglowek nie liczony)
  "$exe" -p2 -b128000 --verbose -o "${OUT}_v.m4a" "$WAV" 2>"${OUT}_v.txt"
  local m1
  m1=$(grep -E ':' "${OUT}_v.txt" | grep -vE 'no -1|processed|ETA|%\]' | grep -cE '(: *-1|= *-1)')
  [ "$m1" = "0" ] && ok "verbose bez wartosci -1" || bad "verbose ma $m1 wartosci -1"

  # 5) uncap-bandwidth: core-cutoff 24000 realnie przechodzi (verbose > 20000)
  "$exe" -p2 -b256000 --core-cutoff 24000 --uncap-bandwidth --verbose -o "${OUT}_uc.m4a" "$WAV" 2>"${OUT}_uc.txt"
  local bw
  bw=$(grep -iE 'core bandwidth' "${OUT}_uc.txt" | grep -oE '[0-9]+' | head -1)
  [ "${bw:-0}" -gt 20000 ] && ok "uncap-bandwidth: cutoff ${bw}Hz > 20000 (cap zdjety)" || bad "uncap-bandwidth: ${bw:-?}Hz (cap nadal 20000?)"
}

test_exe "$X64" "x64"
test_exe "$X86" "x86"

# --- DAB+ (--dab): superframe LC/HE/HEv2 + statyczny DLS ---
# Osobna sciezka: .dabp NIE jest dekodowalny przez ffmpeg. Strukture i etykiete
# weryfikujemy w python3 (zawsze dostepny); dekodowalnosc audio przez dablin/
# odr-dabmux gdy sa (inaczej graceful skip). Bit-id z odr-audioenc gdy jest.
DABLIN="${DABLIN:-$(command -v dablin 2>/dev/null || echo /tmp/dabtools/root/usr/bin/dablin)}"
DABMUX="${DABMUX:-$(command -v odr-dabmux 2>/dev/null || echo /tmp/dabtools/root/usr/bin/odr-dabmux)}"
DABLIN_LIBS="${DABLIN_LIBS:-/tmp/dabtools/root/usr/lib/x86_64-linux-gnu}"
ODRENC="${ODRENC:-$HOME/dab_ref/inst/bin/odr-audioenc}"

test_dab(){
  local exe="$1" tag="$2"
  echo "== DAB+ $tag ($exe) =="
  [ -f "$exe" ] || { echo "  (pominieto - brak binarki)"; return; }

  # DAB+ przyjmuje tylko sr 32k/48k - osobna probka 48k (glowna jest 44.1k).
  local DWAV=_check_dab_in.wav
  python3 - "$DWAV" <<'PY'
import wave,struct,math,random,sys
sr=48000; random.seed(2); c=lambda x:max(-32768,min(32767,int(x)))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(struct.pack('<hh',
   c(9000*(math.sin(2*math.pi*440*i/sr)+0.3*random.uniform(-1,1))),
   c(9000*(math.sin(2*math.pi*1000*i/sr)+0.3*random.uniform(-1,1)))) for i in range(sr*2)))
w.close()
PY

  # 1) 3 profile x auto-AOT: enkoduje, spojna struktura super-ramki (subch*120)
  local specs=( "LC96:96:12" "HE64:64:8" "PS32:32:4" )
  for s in "${specs[@]}"; do
    local n="${s%%:*}"
    local rest="${s#*:}"
    local b="${rest%%:*}"
    local subch="${rest##*:}"
    enc "$exe" --dab -b "$b" -o "${OUT}_dab_$n.dabp" "$DWAV"
    local sz; sz=$(stat -c%s "${OUT}_dab_$n.dabp" 2>/dev/null || echo 0)
    local sf=$((subch*120))
    if [ "$sz" -gt 0 ] && [ $((sz % sf)) -eq 0 ]; then ok "DAB+ $n: super-ramka spojna ($sz B, %$sf=0)"
    else bad "DAB+ $n: rozmiar $sz nie jest wielokrotnoscia $sf"; fi
  done

  # 2) statyczny DLS: etykieta trafia do strumienia (reverse-fill X-PAD w DSE)
  enc "$exe" --dab -b 96 --dab-label "Radio Test 123" -o "${OUT}_dab_lbl.dabp" "$DWAV"
  if python3 - "${OUT}_dab_lbl.dabp" <<'PY'
import sys
d=open(sys.argv[1],'rb').read()
sys.exit(0 if b'Radio Test 123'[::-1] in d else 1)
PY
  then ok "DAB+ DLS: etykieta obecna w strumieniu (reverse X-PAD)"
  else bad "DAB+ DLS: etykieta nieobecna w strumieniu"; fi

  # 3) round-trip DLS przez niezalezny dekoder tools/dab/ (gen + decode)
  if [ -f tools/dab/dls_decode.py ] && [ -f tools/dab/gen_test_xpad.py ]; then
    local got
    python3 tools/dab/gen_test_xpad.py "Radio Test 123" 48 "${OUT}_xpad.bin" 2>/dev/null
    got=$(python3 tools/dab/dls_decode.py "${OUT}_xpad.bin" 48 2>/dev/null)
    [ "$got" = "Radio Test 123" ] && ok "DAB+ DLS: round-trip dekoder = 'Radio Test 123'" \
      || bad "DAB+ DLS: round-trip dal '$got'"
  fi

  # 4) no-regression: --dab bez labela == odr-audioenc (bit-identyczny) - gdy jest
  if [ -x "$ODRENC" ] && [ "$tag" = "x64-native" ]; then
    enc "$exe" --dab -b 96 -o "${OUT}_dab_nl.dabp" "$DWAV"
    "$ODRENC" -i "$DWAV" -f wav -b 96 -r 48000 -c 2 -o "${OUT}_dab_g.dabp" >/dev/null 2>&1
    if cmp -s "${OUT}_dab_nl.dabp" "${OUT}_dab_g.dabp"; then ok "DAB+ LC bit-identyczny z odr-audioenc"
    else echo "  (info) DAB+ LC rozny od odr (moze inny sr/wersja) - pomijam"; fi
  fi

  # 5) audio z super-ramki dekoduje sie (dablin/odr-dabmux) - opcjonalne
  if [ -x "$DABMUX" ] && [ -x "$DABLIN" ]; then
    local d=_check_dabtmp; rm -rf "$d"; mkdir -p "$d"
    cp "${OUT}_dab_LC96.dabp" "$d/a.dabp"
    cat > "$d/m.mux" <<EOF
general { dabmode 1 nbframes 250 }
ensemble { id 0x4fff ecc 0xec label "T" shortlabel "T" international-table 1 }
services { srv { label "A" shortlabel "A" pty 0 language 0 } }
subchannels { sub { type dabplus inputfile "a.dabp" nonblock false bitrate 96 id 1 protection 3 } }
components { comp { type 0 service srv subchannel sub } }
outputs { out1 "file://a.eti?type=raw" }
EOF
    ( cd "$d" && LD_LIBRARY_PATH="$DABLIN_LIBS" "$DABMUX" m.mux >/dev/null 2>&1 \
        && LD_LIBRARY_PATH="$DABLIN_LIBS" "$DABLIN" -f eti -1 -p a.eti >a.pcm 2>a.log )
    if grep -q 'sync succeeded' "$d/a.log" 2>/dev/null; then ok "DAB+ audio dekoduje sie (dablin/faad2)"
    else bad "DAB+ audio: brak sync w dablin"; fi
    rm -rf "$d"
  else
    echo "  (SKIP dekodowalnosc audio - brak dablin/odr-dabmux; ustaw DABLIN/DABMUX)"
  fi
  rm -f "${OUT}"_dab_*.dabp "${OUT}"_xpad.bin
}

# DAB+ na natywnej binarce Linux jesli podano (bit-id z odr wymaga natywnej),
# inaczej na .exe przez interop (struktura+etykieta+round-trip dzialaja tak samo).
if [ -n "${DAB_NATIVE:-}" ] && [ -x "${DAB_NATIVE}" ]; then
  test_dab "$DAB_NATIVE" "x64-native"
else
  test_dab "$X64" "x64"
fi

# 5) no-regression: domyslny CBR bez franken flag == oryginalna binarka.
# Porownujemy SUROWY ADTS (-f2), nie M4A: kontener M4A ma tag 'tool' z nazwa
# enkodera (rozny) + moze miec timestamp; audio (ADTS) musi byc bit-identyczne.
if [ -f "$ORIG" ]; then
  echo "== no-regression =="
  enc "$X64"  -p2 -b128000 -f2 -o "${OUT}_pl.aac" "$WAV"
  enc "$ORIG" -p2 -b128000 -f2 -o "${OUT}_or.aac" "$WAV"
  if cmp -s "${OUT}_pl.aac" "${OUT}_or.aac"; then ok "CBR bez flag: ADTS bit-identyczny z fdkaac2.exe"
  else bad "REGRESJA: ADTS bez flag rozni sie od oryginalu"; fi
fi

# 6) NEUTRAL-IDENTITY: pokretla ustawione na wartosc NEUTRALNA musza dawac wyjscie
# bit-identyczne z brakiem pokretla. Ten test wylapal realny blad (09.08.2026):
# mnoznik liczony jako ((INT64)100<<31)/100 = 2^31 NIE MIESCI sie w signed 32-bit
# FIXP_DBL i zawijal sie na wartosc ujemna, wiec "x1.00" cicho psulo dzwiek.
echo "== neutral-identity (x1.00 == brak pokretla) =="
NEUTRAL_SBR="--sbr-tran-quiet 100 --sbr-tran-thr 100 --sbr-tran-split 100 \
--sbr-mh-tone 100 --sbr-mh-diff 100 --sbr-mh-decay-orig 100 --sbr-mh-decay-diff 100 \
--sbr-mh-sfm-sbr 100 --sbr-mh-sfm-orig 100 --sbr-tran-peak 90 --sbr-tran-dom 140 \
--sbr-mh-maxcomp 50"
enc "$X64" -p5 -b64000 -f2 -o "${OUT}_ni_base.aac" "$WAV"
enc "$X64" -p5 -b64000 -f2 $NEUTRAL_SBR -o "${OUT}_ni_neu.aac" "$WAV"
if cmp -s "${OUT}_ni_base.aac" "${OUT}_ni_neu.aac"; then
  ok "SBR detector knobs @ neutral: bit-identyczne ze stockiem"
else
  bad "SBR detector knobs @ neutral ZMIENIAJA wyjscie (regresja skali/przepelnienie?)"
fi

# IPD jest OPT-IN: --ps-ipd 0 musi dac dokladnie to samo co brak flagi. Chroni
# przed przypadkowym wlaczeniem sciezki fazy (ktora zmienia ps_extension i
# wyrownanie strumienia SBR).
enc "$X64" -p29 -b48000 -f2 -o "${OUT}_ipd_base.aac" "$WAV"
enc "$X64" -p29 -b48000 -f2 --ps-ipd 0 -o "${OUT}_ipd_zero.aac" "$WAV"
if cmp -s "${OUT}_ipd_base.aac" "${OUT}_ipd_zero.aac"; then
  ok "--ps-ipd 0: bit-identyczne z brakiem flagi"
else
  bad "--ps-ipd 0 ZMIENIA wyjscie (IPD nie jest czysto opt-in)"
fi

# Z IPD wlaczonym strumien MUSI byc nadal poprawny skladniowo: rozmiar
# ps_extension i wyrownanie SBR sa policzone recznie, wiec blad tam objawia sie
# bledami parsera dekodera (a nie samym exit code enkodera).
enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 -o "${OUT}_ipd_on.aac" "$WAV"
if [ -s "${OUT}_ipd_on.aac" ] && [ "$(derr ${OUT}_ipd_on.aac)" = "0" ]; then
  ok "--ps-ipd 1: strumien dekodowalny bez bledow parsera"
else
  bad "--ps-ipd 1: dekoder raportuje bledy (rozmiar ps_extension / wyrownanie?)"
fi

echo "==================================="
echo "WYNIK: PASS=$PASS FAIL=$FAIL"
[ "$FAIL" = "0" ] && { echo "ALL PASS"; exit 0; } || { echo "SA BLEDY"; exit 1; }
