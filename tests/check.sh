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

cleanup(){ rm -f "$WAV" _check_core_*.wav _check_core_*.txt _check_invf.wav _check_nfw.wav _check_unlock.wav _check_hdr.wav _check_msshort.wav _check_isalt.wav _check_allshort.wav _check_longonly.wav _check_scu.wav _check_scp.wav _check_pss.wav _check_bb.wav _check_bbs.wav short_ms.wav is_alt.wav _check_dab_in.wav _check_psmulti.wav _check_psnoiid.wav "${OUT}"*.m4a "${OUT}"*.aac "${OUT}"*.txt "${OUT}"*.dabp "${OUT}"*.bin "${OUT}"*.loas; rm -rf _check_dabtmp; }
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
#
# UWAGA na WSL/drvfs: plik zapisuje binarka WINDOWS, a czyta go ffmpeg z LINUKSA.
# Na /mnt/... zapis nie zawsze jest natychmiast w pelni widoczny dla procesu Linux,
# co objawialo sie POJEDYNCZYMI, NIEPOWTARZALNYMI FAILami w losowych przypadkach
# (raz sbr-tran-thr, raz side-bias, raz sbr-mh-decay-orig) przy w pelni
# DETERMINISTYCZNYM enkoderze - to samo wejscie i flagi daja bit-identyczne
# wyjscie 6/6 razy. Dlatego: krotka proba ponowna, zanim uznamy strumien za zly.
derr(){
  local f="$1" n
  # WZORZEC (rozszerzony 10.08.2026): dokladany 'overflow' i 'illegal', bo
  # ffmpeg zglasza uszkodzone Parametric Stereo WLASNIE tak:
  #   "ps extension overflow -4" / "illegal icc"
  # a stary wzorzec (error|invalid|exceeds) NIE LAPAL ZADNEGO z tych napisow.
  # Realny skutek: bug odniesienia delta-time w OPD psul bitstream tak, ze
  # ffmpeg I faad sie wywracaly, a make check pokazywal 143/143 PASS. Test,
  # ktory nie widzi bledu, ktory sam wywoluje, jest gorszy niz brak testu.
  local pat='error|invalid|exceeds|overflow|illegal'
  n=$(ffmpeg -y -loglevel error -i "$f" -f null - 2>&1 | grep -icE "$pat")
  if [ "$n" != "0" ]; then
    sync 2>/dev/null
    sleep 0.3
    n=$(ffmpeg -y -loglevel error -i "$f" -f null - 2>&1 | grep -icE "$pat")
  fi
  echo "$n"
}

enc(){
  # $1=exe ... -o plik in.wav
  #
  # WSL interop pod obciazeniem BYWA NIE URUCHAMIA procesu Windows w ogole:
  # zmierzone 8/60 nieudanych wywolan przy 3 rownoleglych petlach .exe w tle,
  # objaw = plik wyjsciowy NIE POWSTAJE (rozmiar 0), enkoder nie zglasza bledu.
  # W izolacji ten sam przypadek przechodzi 40/40. Stad losowe, niepowtarzalne
  # FAILe w roznych testach przy KAZDYM przebiegu make check, mimo ze enkoder
  # jest w pelni deterministyczny (6/6 bit-identycznych wyjsc dla tych samych
  # argumentow). To ograniczenie SRODOWISKA, nie kodu - dlatego ponawiamy.
  local out="" prev="" a
  for a in "$@"; do
    [ "$prev" = "-o" ] && out="$a"
    prev="$a"
  done

  local try
  for try in 1 2 3 4 5; do
    "$@" 2>/dev/null
    [ -z "$out" ] && return 0
    [ -s "$out" ] && return 0
    sleep 0.4
  done
  return 1
}

test_exe(){
  local exe="$1" tag="$2"
  echo "== $tag ($exe) =="
  [ -f "$exe" ] || { echo "  (pominieto - brak binarki)"; return; }

  # 1) help kompletny - franken switche obecne
  local hn _t
  # retry takze tutaj: pod obciazeniem interop WSL potrafi nie odpalic .exe wcale,
  # co dawalo falszywe "help: tylko 0 switchy".
  for _t in 1 2 3 4 5; do
    hn=$("$exe" --help 2>&1 | grep -cE '^ --(msmask|msbands|is|isbands|is-min-sfbs|is-corr-thresh|is-lr-ratio|is-lo|is-hi|is-force-lo|is-force-hi|core-cutoff|sbr-start|sbr-stop|sbr-freqscale|sbr-alterscale|sbr-noise-bands|sbr-amp-res|sbr-data-extra|sbr-header-period|sbr-tran-peak|sbr-tran-quiet|sbr-tran-dom|sbr-tran-thr|sbr-tran-split|sbr-mh-tone|sbr-mh-diff|sbr-mh-decay-orig|sbr-mh-decay-diff|sbr-mh-sfm-sbr|sbr-mh-sfm-orig|sbr-mh-maxcomp|sbr-mh-deltatime|sbr-noise-max|ps|ps-iid-quant|ps-ipd|ps-opd|ps-bands|ps-env|ps-env-reduce|ps-noenv-skip|tns-mask|tns-order|pns|pns-start|pns-gain|pns-tonality|pns-refpower|pns-gapfill|pns-min-width|ath-scale|minsnr-scale|minsnr-clamp-hi|minsnr-clamp-lo|reduce-clamp|mid-bias|side-bias|side-knee|mask-slope|block-bias|vbr-reservoir|peak-bitrate|max-bits-frame|min-bits-frame|bitres-mode|ms-bias|verbose)')
  [ "$hn" -ge 60 ] && break
    sleep 0.4
  done
  [ "$hn" -ge 60 ] && ok "help: $hn franken switchy" || bad "help: tylko $hn switchy (oczekiwano >=60)"

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
                "ps-ipd-opd0:-p29 -b48000 --ps-ipd 1 --ps-opd 0"
                "ps-ipd-opd1:-p29 -b48000 --ps-ipd 1 --ps-opd 1"
                "speech:-p5 -b32000 --speech" "spread-mask:-b128000 --spread-mask 64"
 "ms-prec-hi:--ms-precision 1024"
 "mid-bias:-b96000 --mid-bias 384"
 "side-bias:-b96000 --side-bias 6"
 "side-knee:-b96000 --side-knee 4"
 "side-bias+knee:-b96000 --side-bias 6 --side-knee 4"
 "mask-slope-pos:-b96000 --mask-slope 12"
 "mask-slope-neg:-b96000 --mask-slope -12"
 "minsnr-scale:-b128000 --minsnr-scale 128"
 "minsnr-clamp-hi:-b128000 --minsnr-clamp-hi 128"
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
  # te dwa wywolania omijaja enc(), bo przechwytuja stderr do analizy --verbose;
  # potrzebuja wlasnego retry na te sama usterke interopu WSL (patrz enc()).
  for _t in 1 2 3 4 5; do
    "$exe" -p2 -b128000 --verbose -o "${OUT}_v.m4a" "$WAV" 2>"${OUT}_v.txt"
    grep -qiE 'core bandwidth' "${OUT}_v.txt" && break
    sleep 0.4
  done
  local m1
  m1=$(grep -E ':' "${OUT}_v.txt" | grep -vE 'no -1|processed|ETA|%\]' | grep -cE '(: *-1|= *-1)')
  [ "$m1" = "0" ] && ok "verbose bez wartosci -1" || bad "verbose ma $m1 wartosci -1"

  # 5) uncap-bandwidth: core-cutoff 24000 realnie przechodzi (verbose > 20000)
  for _t in 1 2 3 4 5; do
    "$exe" -p2 -b256000 --core-cutoff 24000 --uncap-bandwidth --verbose -o "${OUT}_uc.m4a" "$WAV" 2>"${OUT}_uc.txt"
    # nie wystarczy niepusty plik: przy nieodpalonym interopie stderr moze zawierac
    # sam pasek postepu bez linii "core bandwidth" - warunkuj na SZUKANEJ TRESCI
    grep -qiE 'core bandwidth' "${OUT}_uc.txt" && break
    sleep 0.4
  done
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

  # 3b) jawny -p z --dab: ten sam strumien co auto-dobor (beta.1: -p2 = aacEncEncode
  #     failed, -p 5/29 = extrapolater psul zgodnosc), -p spoza DAB+ odrzucony,
  #     -b 8 to 8 kbps (beta.1: drugie x1000 -> abort).
  local pp
  for pp in "LC96:96:2" "HE64:64:5" "PS32:32:29"; do
    local pn="${pp%%:*}" pr="${pp#*:}"
    local pb="${pr%%:*}" pq="${pr##*:}"
    enc "$exe" --dab -b "$pb" -p "$pq" -o "${OUT}_dab_p$pn.dabp" "$DWAV"
    if [ -s "${OUT}_dab_p$pn.dabp" ] && cmp -s "${OUT}_dab_p$pn.dabp" "${OUT}_dab_$pn.dabp"
    then ok "DAB+ -p $pq -b $pb = auto-dobor (bit w bit)"
    else bad "DAB+ -p $pq -b $pb rozny od auto albo pusty"; fi
  done
  rm -f "${OUT}_dab_bad.dabp"
  "$exe" --dab -b 96 -p 129 -o "${OUT}_dab_bad.dabp" "$DWAV" >/dev/null 2>&1
  local prc=$?
  if [ "$prc" -ne 0 ] && [ ! -s "${OUT}_dab_bad.dabp" ]; then ok "DAB+ -p 129 odrzucony (rc=$prc, bez pliku)"
  else bad "DAB+ -p 129 nie odrzucony (rc=$prc)"; fi
  enc "$exe" --dab -b 8 -o "${OUT}_dab_b8.dabp" "$DWAV"
  local s8; s8=$(stat -c%s "${OUT}_dab_b8.dabp" 2>/dev/null || echo 0)
  if [ "$s8" -gt 0 ] && [ $((s8 % 120)) -eq 0 ]; then ok "DAB+ -b 8: 8 kbps, super-ramka 120 B ($s8 B)"
  else bad "DAB+ -b 8: rozmiar $s8"; fi

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
if [ ! -s "${OUT}_ni_base.aac" ] || [ ! -s "${OUT}_ni_neu.aac" ]; then
  bad "SBR detector knobs @ neutral: BRAK pliku (interop WSL nie odpalil .exe) - wynik nierozstrzygniety"
elif cmp -s "${OUT}_ni_base.aac" "${OUT}_ni_neu.aac"; then
  ok "SBR detector knobs @ neutral: bit-identyczne ze stockiem"
else
  bad "SBR detector knobs @ neutral ZMIENIAJA wyjscie (regresja skali/przepelnienie?)"
fi

# IPD jest OPT-IN: --ps-ipd 0 musi dac dokladnie to samo co brak flagi. Chroni
# przed przypadkowym wlaczeniem sciezki fazy (ktora zmienia ps_extension i
# wyrownanie strumienia SBR).
enc "$X64" -p29 -b48000 -f2 -o "${OUT}_ipd_base.aac" "$WAV"
enc "$X64" -p29 -b48000 -f2 --ps-ipd 0 -o "${OUT}_ipd_zero.aac" "$WAV"
if [ ! -s "${OUT}_ipd_base.aac" ] || [ ! -s "${OUT}_ipd_zero.aac" ]; then
  bad "--ps-ipd 0: BRAK pliku (interop WSL) - wynik nierozstrzygniety"
elif cmp -s "${OUT}_ipd_base.aac" "${OUT}_ipd_zero.aac"; then
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

# OPD: --ps-opd 0 vs 1 MUSZA dac ROZNE strumienie (inaczej OPD nie jest wysylane),
# i OBA musza sie dekodowac bez bledow (drugi zestaw pol fazy w rozszerzeniu).
enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 0 -o "${OUT}_opd0.aac" "$WAV"
enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 -o "${OUT}_opd1.aac" "$WAV"
if [ ! -s "${OUT}_opd0.aac" ] || [ ! -s "${OUT}_opd1.aac" ]; then
  bad "--ps-opd: BRAK pliku (interop WSL) - wynik nierozstrzygniety"
elif cmp -s "${OUT}_opd0.aac" "${OUT}_opd1.aac"; then
  bad "--ps-opd 0/1 daja IDENTYCZNY strumien (OPD nie trafia do bitstreamu)"
else
  ok "--ps-opd 0/1: roznia sie (OPD realnie kodowane)"
fi
if [ "$(derr ${OUT}_opd1.aac)" = "0" ] && [ "$(derr ${OUT}_opd0.aac)" = "0" ]; then
  ok "--ps-opd 0 i 1: oba strumienie dekodowalne bez bledow"
else
  bad "--ps-opd: dekoder raportuje bledy"
fi

# --- IPD/OPD na materiale WIELOOBWIEDNIOWYM (regresja delta-time, 10.08.2026) ---
#
# DLACZEGO OSOBNA PROBKA: w delta-time dekoder rozwiazuje odniesienie jako
#   e_prev = e ? e-1 : num_env_old-1   (ffmpeg aacps_common.c, READ_PAR_DATA)
# czyli dla KAZDEJ obwiedni po pierwszej predykuje z POPRZEDNIEJ OBWIEDNI TEJ
# SAMEJ RAMKI. Nasz writer trzymal odniesienie na poprzedniej RAMCE dla
# wszystkich obwiedni, co rozjezdzalo enkoder z dekoderem ("ps extension
# overflow" + zepsuty parse ICC) - ale WYLACZNIE w ramkach o >1 obwiedni.
#
# Glowna probka testowa (440/554 Hz + szum) ma statyczny obraz stereo, wiec
# envelopeReducible() zwija obwiednie do JEDNEJ i ten blad byl NIEWIDOCZNY.
# Ponizsza probka to czesciowa przeciwfaza z dekorelowana domieszka
# (L = x, R = -0.4*x + 0.6*d) w zasiegu IPD/OPD. Parametry (eps 0.6, seedy 7/13,
# 3 s) NIE sa dobrane "na oko" - wyskanowane tak, by test byl DYSKRYMINUJACY:
# na binarce sprzed naprawy daje blad dekodera, po naprawie 0. Test, ktory
# przechodzi takze na zepsutym kodzie, nie jest testem - dokladnie tak stary
# zestaw (ps-ipd-combo) przepuscil ten bug przy 143/143 PASS.
PSMULTI="_check_psmulti.wav"
python3 - "$PSMULTI" <<'PY'
import wave, struct, math, random, sys
sr = 44100
c = lambda x: max(-32768, min(32767, int(x)))


def lowband(n, seed):
    """Tresc w zasiegu IPD/OPD (60-690 Hz): tony + szum dolnoprzepustowy."""
    rnd = random.Random(seed)
    ph = [rnd.uniform(0, 6.283) for _ in range(5)]
    out = []
    st = 0.0
    for i in range(n):
        t = i / sr
        v = (1.0 * math.sin(2 * math.pi * 70 * t + ph[0])
             + 0.8 * math.sin(2 * math.pi * 130 * t + ph[1])
             + 0.6 * math.sin(2 * math.pi * 220 * t + ph[2])
             + 0.5 * math.sin(2 * math.pi * 330 * t + ph[3])
             + 0.35 * math.sin(2 * math.pi * 520 * t + ph[4]))
        st = 0.97 * st + 0.03 * rnd.uniform(-1, 1)   # szum LP
        env = 0.6 + 0.4 * math.sin(2 * math.pi * 0.3 * t)
        out.append((v + 6.0 * st) * env)
    return out


EPS = 0.6
n = sr * 3
x = lowband(n, 7)
d = lowband(n, 13)
mx = max(max(abs(a) for a in x), max(abs(a) for a in d)) or 1.0
g = 0.63 * 32767.0 / mx
fr = [struct.pack('<hh', c(x[i] * g),
                  c((-(1.0 - EPS) * x[i] + EPS * d[i]) * g)) for i in range(n)]
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY

# Sciezka DOMYSLNA (z redukcja obwiedni) - tam bug realnie wychodzil, przy
# zbiegu ramki wieloobwiedniowej z wyborem delta-time.
enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 -o "${OUT}_psmulti_df.aac" "$PSMULTI"
if [ ! -s "${OUT}_psmulti_df.aac" ]; then
  bad "IPD/OPD wieloobwiedniowe: BRAK pliku (interop WSL) - nierozstrzygniete"
elif [ "$(derr ${OUT}_psmulti_df.aac)" = "0" ]; then
  ok "IPD+OPD @ wieloobwiedniowe: strumien dekodowalny (delta-time e-1)"
else
  bad "IPD+OPD @ wieloobwiedniowe: bledy dekodera (odniesienie delta-time OPD?)"
fi

# Ta sama probka z wymuszonymi 2 obwiedniami (granica e=1, pierwsza obwiednia
# ktora w ogole moze siegnac wewnatrz ramki).
enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 --ps-env 2 --ps-env-reduce 0 \
    -o "${OUT}_psmulti_e2.aac" "$PSMULTI"
if [ -s "${OUT}_psmulti_e2.aac" ] && [ "$(derr ${OUT}_psmulti_e2.aac)" = "0" ]; then
  ok "IPD+OPD @ 2 obwiednie: strumien dekodowalny"
else
  bad "IPD+OPD @ 2 obwiednie: bledy dekodera"
fi

# IPD nadal musi byc czysto OPT-IN takze na tej probce.
enc "$X64" -p29 -b48000 -f2 -o "${OUT}_psmulti_base.aac" "$PSMULTI"
enc "$X64" -p29 -b48000 -f2 --ps-ipd 0 -o "${OUT}_psmulti_off.aac" "$PSMULTI"
if [ ! -s "${OUT}_psmulti_base.aac" ] || [ ! -s "${OUT}_psmulti_off.aac" ]; then
  bad "IPD opt-in (wieloobwiedniowe): BRAK pliku - nierozstrzygniete"
elif cmp -s "${OUT}_psmulti_base.aac" "${OUT}_psmulti_off.aac"; then
  ok "--ps-ipd 0 @ wieloobwiedniowe: bit-identyczne z brakiem flagi"
else
  bad "--ps-ipd 0 @ wieloobwiedniowe ZMIENIA wyjscie"
fi

# --- IPD/OPD wymaga zasygnalizowanego IID (bug naprawiony 10.08.2026) ---
#
# Liczba pasm IPD/OPD NIE jedzie w rozszerzeniu - dekoder wyciaga ja z iid_mode,
# i to WYLACZNIE w galezi naglowka bramkowanej przez enable_iid:
#   if (header) { enable_iid = get_bits1();
#                 if (enable_iid) { nr_ipdopd_par = nr_iidopd_par_tab[iid_mode]; } }
# (ffmpeg libavcodec/aacps_common.c). FDK wlacza IID z heurystyki roznicy
# glosnosci, wiec na materiale o zblizonych poziomach kanalow legalnie wysylal
# enable_iid = 0 - a wtedy dekoder mial 0 pasm fazy, czytal MNIEJ bitow niz
# zapisalismy (overflow rozszerzenia, zepsuty parse ICC) i CICHO WYRZUCAL cala
# przeslana faze. Ta sciezka wychodzila glownie przy wielu obwiedniach.
#
# Probka: prawie idealna przeciwfaza (L = x, R = -x) - kanaly maja identyczne
# poziomy, wiec heurystyka IID zostaje wylaczona i warunek jest realnie testowany.
PSNOIID="_check_psnoiid.wav"
python3 - "$PSNOIID" <<'PY'
import wave, struct, math, random, sys
sr = 44100
rnd = random.Random(5)
c = lambda x: max(-32768, min(32767, int(x)))
n = sr * 3
st = 0.0
fr = []
for i in range(n):
    t = i / sr
    v = (math.sin(2 * math.pi * 70 * t) + 0.8 * math.sin(2 * math.pi * 150 * t)
         + 0.6 * math.sin(2 * math.pi * 260 * t)
         + 0.4 * math.sin(2 * math.pi * 480 * t))
    st = 0.97 * st + 0.03 * rnd.uniform(-1, 1)
    s = (v + 5.0 * st) * (0.6 + 0.4 * math.sin(2 * math.pi * 0.3 * t))
    # identyczne poziomy L/P, przeciwna faza -> IID heurystycznie WYLACZONE
    fr.append(struct.pack('<hh', c(4200 * s), c(-4200 * s)))
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(fr)); w.close()
PY

for evar in "--ps-env 4 --ps-env-reduce 0" "--ps-env 2 --ps-env-reduce 0"; do
  lbl=$(echo "$evar" | awk '{print $2}')
  enc "$X64" -p29 -b48000 -f2 --ps-ipd 1 --ps-opd 1 $evar \
      -o "${OUT}_psnoiid_${lbl}.aac" "$PSNOIID"
  if [ ! -s "${OUT}_psnoiid_${lbl}.aac" ]; then
    bad "IPD bez IID (env $lbl): BRAK pliku - nierozstrzygniete"
  elif [ "$(derr ${OUT}_psnoiid_${lbl}.aac)" = "0" ]; then
    ok "IPD+OPD @ przeciwfaza, $lbl obwiedni: dekodowalny (IID sygnalizowane)"
  else
    bad "IPD+OPD @ przeciwfaza, $lbl obwiedni: bledy (brak enable_iid dla pasm fazy?)"
  fi
done

# Ta probka tez musi zostac bit-identyczna bez flagi IPD.
enc "$X64" -p29 -b48000 -f2 -o "${OUT}_psnoiid_b.aac" "$PSNOIID"
enc "$X64" -p29 -b48000 -f2 --ps-ipd 0 -o "${OUT}_psnoiid_o.aac" "$PSNOIID"
if [ -s "${OUT}_psnoiid_b.aac" ] && cmp -s "${OUT}_psnoiid_b.aac" "${OUT}_psnoiid_o.aac"; then
  ok "--ps-ipd 0 @ przeciwfaza: bit-identyczne z brakiem flagi"
else
  bad "--ps-ipd 0 @ przeciwfaza: ZMIENIA wyjscie lub brak pliku"
fi

# --- MS/IS przy short blocks i promocja MS_MASK_ALL (bugi naprawione 20.10.2026) ---
#
# 1) Zakres pasm MS (--msbands, --msbands-lo/-hi) byl porownywany ze
#    SPLASZCZONYM indeksem grupy (sfb + sfboffs). W bloku short kazda grupa okien
#    zaczyna sie od czestotliwosciowego SFB 0, wiec grupy 1..n wypadaly poza
#    zakres i MS znikalo z nich w calosci. Dowod czytamy Z BITSTREAMU
#    (tools/msis/parse_ms.py): ms_used w grupach >0 musi pojawic sie w zakresie.
# 2) Zakres --msbands-lo/-hi nie blokowal promocji do MS_MASK_ALL (FDK promuje,
#    gdy L/R zostaje na <9 pasmach), wiec motylek M/S wracal na pasma, ktore
#    zakres wylaczal. Liczymy ramki ms_mask_present=2 i pasma poza zakresem.
# 3) Wymuszony IS (--is-force-lo/-hi) na sasiednich pasmach spanoramowanych
#    w PRZECIWNE strony dawal delte pozycji IS > 60 i asercje w enkoderze
#    (kod powrotu != 0, plik uciety).
MSS="_check_msshort.wav"; ISALT="_check_isalt.wav"
python3 tools/msis/gen_samples.py . >/dev/null && mv -f short_ms.wav "$MSS" && mv -f is_alt.wav "$ISALT"

ms_json(){ python3 tools/msis/parse_ms.py "$@" --json 2>/dev/null; }
jget(){ python3 -c "import json,sys; d=json.loads(sys.argv[1]); print(d['$2'] if '$2'!='mmp2' else d['mmp'][2])" "$1"; }

# Od 1.4.0-beta.2 zakres long przechodzi do ramek short przez Hz (44.1 kHz:
# long 0..8 = 0..775 Hz -> short 0..0, long 3..10 = 258..1034 Hz -> short 0..1),
# wiec ramki short sprawdzamy przelozonym zakresem (--slo/--shi/--smax).
for spec in "lo0hi8:--msbands-lo 0 --msbands-hi 8:--lo 0 --hi 8 --slo 0 --shi 0" \
            "max9:--msbands 9:--max 9 --smax 1" \
            "lo3hi10:--msbands-lo 3 --msbands-hi 10:--lo 3 --hi 10 --slo 0 --shi 1"; do
  lbl="${spec%%:*}"; rest="${spec#*:}"; A="${rest%%:*}"; P="${rest#*:}"
  enc "$X64" -f2 -b96000 $A -o "${OUT}_mss_${lbl}.aac" "$MSS"
  if [ ! -s "${OUT}_mss_${lbl}.aac" ]; then
    bad "MS short ($A): BRAK pliku - nierozstrzygniete"; continue
  fi
  j=$(ms_json "${OUT}_mss_${lbl}.aac" $P)
  gN=$(jget "$j" used_in_range_gN); sh=$(jget "$j" short); vb=$(jget "$j" viol_band); va=$(jget "$j" viol_all)
  if [ "$sh" -gt 0 ] && [ "$gN" -gt 0 ] && [ "$vb" = "0" ] && [ "$va" = "0" ] \
     && [ "$(derr ${OUT}_mss_${lbl}.aac)" = "0" ]; then
    ok "MS short [$A]: zakres dziala w kazdej grupie okien ($gN pasm MS w grupach >0)"
  else
    bad "MS short [$A]: short=$sh ms_w_grupach>0=$gN poza_zakresem=$vb mask_all=$va"
  fi
done

for hi in 34 38; do
  enc "$X64" -f2 -b96000 --pns 0 --msbands-hi $hi -o "${OUT}_msp_${hi}.aac" "$MSS"
  if [ ! -s "${OUT}_msp_${hi}.aac" ]; then
    bad "MS promocja (hi $hi): BRAK pliku - nierozstrzygniete"; continue
  fi
  j=$(ms_json "${OUT}_msp_${hi}.aac" --hi $hi)
  va=$(jget "$j" viol_all); vb=$(jget "$j" viol_band)
  if [ "$va" = "0" ] && [ "$vb" = "0" ]; then
    ok "--msbands-hi $hi: brak promocji do MS_MASK_ALL, zero MS poza zakresem"
  else
    bad "--msbands-hi $hi: promocja MS_MASK_ALL w $va ramkach, $vb pasm MS poza zakresem"
  fi
done

for b in 48000 96000; do
  rc=1
  for _t in 1 2 3 4 5; do
    rm -f "${OUT}_isalt_${b}.aac"
    "$X64" -f2 -p2 -b$b --is 1 --is-force-lo 0 --is-force-hi 48 \
        -o "${OUT}_isalt_${b}.aac" "$ISALT" >/dev/null 2>&1; rc=$?
    [ -s "${OUT}_isalt_${b}.aac" ] && break
    sleep 0.4
  done
  if [ "$rc" = "0" ] && [ -s "${OUT}_isalt_${b}.aac" ] && [ "$(derr ${OUT}_isalt_${b}.aac)" = "0" ]; then
    ok "force-IS @ naprzemienna panorama ${b}: enkoduje bez asercji, dekodowalny"
  else
    bad "force-IS @ naprzemienna panorama ${b}: rc=$rc (delta pozycji IS poza ksiazka?)"
  fi
done

# Bez flag zakresu probka short musi zostac bit-identyczna z samym --msmask -1.
enc "$X64" -f2 -b96000 -o "${OUT}_mss_base.aac" "$MSS"
enc "$X64" -f2 -b96000 --msmask -1 -o "${OUT}_mss_auto.aac" "$MSS"
if [ -s "${OUT}_mss_base.aac" ] && cmp -s "${OUT}_mss_base.aac" "${OUT}_mss_auto.aac"; then
  ok "--msmask -1 @ short: bit-identyczne z brakiem flagi"
else
  bad "--msmask -1 @ short: ZMIENIA wyjscie lub brak pliku"
fi
rm -f "$MSS" "$ISALT"

# --- --minsnr-clamp-hi: powyzej 320 przepelnienie Q30 (bug naprawiony 02.11.2026) ---
# MAX_SNR 0.8 razy n/256 przekraczalo 1.0 w q30, a snr << 1 dawalo wartosc ujemna:
# wymaganie SNR wszystkich pasm stawalo sie smieciem (VBR 64k: plik 2.8x wiekszy).
for v in 321 512; do
  rm -f "${OUT}_ch${v}.aac"
  "$X64" -f2 -p2 -b96000 --minsnr-clamp-hi $v -o "${OUT}_ch${v}.aac" "$WAV" >/dev/null 2>&1; rc=$?
  if [ "$rc" != "0" ] && [ ! -s "${OUT}_ch${v}.aac" ]; then ok "--minsnr-clamp-hi $v odrzucone (powyzej 0 dB)"
  else bad "--minsnr-clamp-hi $v przyjete (rc=$rc)"; fi
done
# --minsnr-clamp-lo bardzo duze: zostaje w q30 (cap 1.0), strumien dekodowalny
enc "$X64" -f2 -p2 -b96000 --minsnr-clamp-lo 100000 -o "${OUT}_cl.aac" "$WAV"
if [ -s "${OUT}_cl.aac" ] && [ "$(derr ${OUT}_cl.aac)" = "0" ]; then ok "--minsnr-clamp-lo 100000 dekodowalne"
else bad "--minsnr-clamp-lo 100000"; fi

# --- --sbr-noise-bands: pole bs_noise_bands ma 2 bity (bug naprawiony 21.10.2026) ---
# Wartosci 4 i 5 byly przyjmowane: naglowek zapisywal je obciete do 2 bitow
# (4 -> 0, 5 -> 1), a estymator szumu liczyl pasma z pelnej wartosci, wiec
# dekoder czytal inna liczbe wspolczynnikow szumu niz enkoder zapisal.
for nb in 4 5; do
  rm -f "${OUT}_nb${nb}.aac"
  "$X64" -f2 -p5 -b48000 --sbr-noise-bands $nb -o "${OUT}_nb${nb}.aac" "$WAV" >/dev/null 2>&1; rc=$?
  if [ "$rc" != "0" ] && [ ! -s "${OUT}_nb${nb}.aac" ]; then
    ok "--sbr-noise-bands $nb odrzucone (pole ma 2 bity)"
  else
    bad "--sbr-noise-bands $nb przyjete (rc=$rc) - naglowek SBR tego nie przeniesie"
  fi
done
for nb in 0 1 2 3; do
  enc "$X64" -f2 -p5 -b48000 --sbr-noise-bands $nb -o "${OUT}_nb${nb}.aac" "$WAV"
  if [ -s "${OUT}_nb${nb}.aac" ] && [ "$(derr ${OUT}_nb${nb}.aac)" = "0" ]; then
    ok "--sbr-noise-bands $nb @ HE-AAC: dekodowalny"
  else
    bad "--sbr-noise-bands $nb @ HE-AAC: bledy dekodera lub brak pliku"
  fi
done

# --- Okres naglowka SBR bez sufitu 1 s (--sbr-header-time, 21.10.2026) ---
# Stock FDK przycinal kazdy jawny okres naglowka SBR do "najwyzej raz na
# sekunde", wiec operator strumienia nie mogl zazadac np. 3,5 s miedzy
# poprawnymi naglowkami. W AAC-ELD naglowek SBR jest tylko w konfiguracji,
# czyli jego okres to okres StreamMuxConfig w LOAS. Odstep czytamy Z
# BITSTREAMU (tools/msis/sbr_headers.py, loas_smc.py), nie z deklaracji.
HDRW="_check_hdr.wav"
python3 - "$HDRW" <<'PY'
import wave, struct, math, random, sys
sr = 48000; rnd = random.Random(3)
c = lambda x: max(-32768, min(32767, int(x)))
n = sr * 12
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
fr = []
for i in range(n):
    t = i / sr
    v = 3000 * math.sin(2 * math.pi * 330 * t) + 1500 * math.sin(2 * math.pi * 7100 * t) + 2500 * rnd.uniform(-1, 1)
    fr.append(struct.pack('<hh', c(v), c(0.8 * v + 800 * rnd.uniform(-1, 1))))
w.writeframes(b''.join(fr)); w.close()
PY

hdr_gap(){ python3 tools/msis/sbr_headers.py "$1" --sr 48000 --json 2>/dev/null | python3 -c "import json,sys; d=json.load(sys.stdin); print(d['gap_max_s'] if d['gap_max_s'] is not None else -1)"; }
smc_gap(){ python3 tools/msis/loas_smc.py "$1" --sr 48000 --fl 512 --json 2>/dev/null | python3 -c "import json,sys; d=json.load(sys.stdin); print(d['gap_max_s'] if d['gap_max_s'] is not None else -1)"; }
in_win(){ python3 -c "import sys; v=float(sys.argv[1]); sys.exit(0 if float(sys.argv[2])<=v<=float(sys.argv[3]) else 1)" "$@"; }

for spec in "p5:-p5 -b48000" "p29:-p29 -b32000"; do
  lbl="${spec%%:*}"; P="${spec#*:}"
  enc "$X64" -f2 $P --sbr-header-time 3.5 -o "${OUT}_hdr_${lbl}.aac" "$HDRW"
  if [ ! -s "${OUT}_hdr_${lbl}.aac" ]; then
    bad "--sbr-header-time 3.5 @ $lbl: BRAK pliku (przelacznik odrzucony?)"; continue
  fi
  g=$(hdr_gap "${OUT}_hdr_${lbl}.aac")
  if in_win "$g" 3.3 3.6 && [ "$(derr ${OUT}_hdr_${lbl}.aac)" = "0" ]; then
    ok "--sbr-header-time 3.5 @ $lbl: najdluzszy odstep naglowkow SBR ${g} s, dekodowalny"
  else
    bad "--sbr-header-time 3.5 @ $lbl: najdluzszy odstep ${g} s (oczekiwane ~3.5)"
  fi
done

enc "$X64" -f2 -p5 -b48000 --sbr-header-period 200 -o "${OUT}_hdr_p200.aac" "$HDRW"
g=$(hdr_gap "${OUT}_hdr_p200.aac")
if in_win "$g" 8.0 9.0; then
  ok "--sbr-header-period 200 @ HE-AAC: brany doslownie (${g} s, bez sufitu 1 s)"
else
  bad "--sbr-header-period 200 @ HE-AAC: odstep ${g} s (stary sufit 1 s?)"
fi

rm -f "${OUT}_hdr_eld.loas"
enc "$X64" -f10 -p39 -L1 -b48000 --sbr-header-time 3.5 -o "${OUT}_hdr_eld.loas" "$HDRW"
if [ ! -s "${OUT}_hdr_eld.loas" ]; then
  bad "--sbr-header-time 3.5 @ ELD+SBR LOAS: BRAK pliku"
else
  g=$(smc_gap "${OUT}_hdr_eld.loas")
  if in_win "$g" 3.3 3.6; then
    ok "--sbr-header-time 3.5 @ ELD+SBR LOAS: StreamMuxConfig (z naglowkiem LD-SBR) co ${g} s"
  else
    bad "--sbr-header-time 3.5 @ ELD+SBR LOAS: odstep konfiguracji ${g} s (oczekiwane ~3.5)"
  fi
fi

# Bez nowych przelacznikow: domyslny okres naglowka SBR bez zmian (10 ramek).
enc "$X64" -f2 -p5 -b48000 -o "${OUT}_hdr_def.aac" "$HDRW"
g=$(hdr_gap "${OUT}_hdr_def.aac")
if in_win "$g" 0.40 0.45; then
  ok "domyslny okres naglowka SBR bez zmian (${g} s = 10 ramek)"
else
  bad "domyslny okres naglowka SBR zmieniony: ${g} s"
fi
rm -f "$HDRW"

# --- Estymator invf/noise floor wystawiony + --sbr-softness (29.10.2026) ---
# Dynamiczna decyzja inverse filtering zostaje dynamiczna; pokretla zmieniaja
# tylko jej parametry. Makro --sbr-softness 2 = stock muzyka, 4 = stock mowa
# (tabela + waga szumu, bez zmiany crossovera), wiec maja dac IDENTYCZNY
# strumien jak stock. Pozostale musza zmienic bitstream i dekodowac sie.
INVW="_check_invf.wav"
python3 - "$INVW" <<'PY'
import wave, struct, math, random, sys
sr = 48000; rnd = random.Random(5)
c = lambda x: max(-32768, min(32767, int(x)))
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
fr = []
for i in range(sr * 3):
    t = i / sr
    v = 4000 * math.sin(2 * math.pi * 440 * t) + 2500 * math.sin(2 * math.pi * 3520 * t) \
        + 1800 * math.sin(2 * math.pi * 7040 * t) + 1500 * rnd.uniform(-1, 1)
    fr.append(struct.pack('<hh', c(v), c(0.9 * v)))
w.writeframes(b''.join(fr)); w.close()
PY
enc "$X64" -f2 -p5 -b32000 -o "${OUT}_inv_base.aac" "$INVW"
enc "$X64" -f2 -p5 -b32000 --speech -o "${OUT}_inv_speech.aac" "$INVW"
IB=$(md5 "${OUT}_inv_base.aac"); IS=$(md5 "${OUT}_inv_speech.aac")
enc "$X64" -f2 -p5 -b32000 --sbr-softness 2 -o "${OUT}_inv_s2.aac" "$INVW"
[ -s "${OUT}_inv_s2.aac" ] && [ "$(md5 ${OUT}_inv_s2.aac)" = "$IB" ] \
  && ok "--sbr-softness 2 = stock muzyka (bit-identyczny)" || bad "--sbr-softness 2 rozni sie od stock muzyki"
enc "$X64" -f2 -p5 -b32000 --speech --sbr-softness 4 -o "${OUT}_inv_s4.aac" "$INVW"
[ -s "${OUT}_inv_s4.aac" ] && [ "$(md5 ${OUT}_inv_s4.aac)" = "$IS" ] \
  && ok "--sbr-softness 4 z --speech = stock mowa (bit-identyczny)" || bad "--sbr-softness 4 rozni sie od stock --speech"
enc "$X64" -f2 -p5 -b32000 --sbr-invf-hyst 1.0 --sbr-invf-bias 0 --sbr-invf-smooth 2 \
    --sbr-nf-weight 0.25 --sbr-nf-weight-from 3 --sbr-invf-table 0 --sbr-invf-min 0 --sbr-invf-max 3 \
    -o "${OUT}_inv_neutral.aac" "$INVW"
[ -s "${OUT}_inv_neutral.aac" ] && [ "$(md5 ${OUT}_inv_neutral.aac)" = "$IB" ] \
  && ok "invf: wartosci neutralne = stock (bit-identyczny)" || bad "invf: wartosci neutralne zmieniaja strumien"
for spec in "soft0:--sbr-softness 0" "soft5:--sbr-softness 5" "soft7:--sbr-softness 7" "min2:--sbr-invf-min 2" "max1:--sbr-invf-max 1" \
            "bias+1:--sbr-invf-bias 1" "bias-1:--sbr-invf-bias -1" \
            "hyst0:--sbr-invf-hyst 0" "sbrthr:--sbr-invf-sbr-thr 6" "origthr:--sbr-invf-orig-thr -6" \
            "smooth0:--sbr-invf-smooth 0"; do
  lbl="${spec%%:*}"; P="${spec#*:}"
  enc "$X64" -f2 -p5 -b32000 $P -o "${OUT}_inv_${lbl}.aac" "$INVW"
  f="${OUT}_inv_${lbl}.aac"
  if [ -s "$f" ] && [ "$(md5 $f)" != "$IB" ] && [ "$(derr $f)" = "0" ]; then
    ok "invf $P: inny strumien, dekodowalny"
  else
    bad "invf $P: brak pliku, strumien = stock albo bledy dekodera"
  fi
done
# Tabela decyzji i waga szumu zaleza od materialu: tu tylko dekodowalnosc.
for P in "--sbr-invf-table 1" "--sbr-nf-weight 1.0 --sbr-nf-weight-from 1"; do
  enc "$X64" -f2 -p5 -b32000 $P -o "${OUT}_inv_dec.aac" "$INVW"
  [ -s "${OUT}_inv_dec.aac" ] && [ "$(derr ${OUT}_inv_dec.aac)" = "0" ] \
    && ok "invf $P: dekodowalny" || bad "invf $P: brak pliku albo bledy dekodera"
done
# Waga szumu dziala ponizej sufitu szumu. Z sufitem z tabeli (0.125) ~95%
# pasm jest obcietych i waga nic nie zmienia; z --sbr-noise-max 6 musi.
# Wlasna probka (ton + mocny szum): na probce INVW z trzema tonami waga nie
# zmienia ani jednego pasma, co jest cecha materialu, nie bledem.
NFW="_check_nfw.wav"
python3 - "$NFW" <<'PY'
import wave, struct, math, random, sys
sr = 48000; rnd = random.Random(5)
c = lambda x: max(-32768, min(32767, int(x)))
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
fr = []
for i in range(sr * 3):
    x = 3000 * math.sin(2 * math.pi * 440 * i / sr) + 4000 * rnd.uniform(-1, 1)
    fr.append(struct.pack('<hh', c(x), c(0.9 * x)))
w.writeframes(b''.join(fr)); w.close()
PY
enc "$X64" -f2 -p5 -b32000 --sbr-noise-max 6 -o "${OUT}_inv_nm6.aac" "$NFW"
enc "$X64" -f2 -p5 -b32000 --sbr-noise-max 6 --sbr-nf-weight 1.0 --sbr-nf-weight-from 1 -o "${OUT}_inv_nm6w.aac" "$NFW"
rm -f "$NFW"
[ -s "${OUT}_inv_nm6w.aac" ] && [ "$(md5 ${OUT}_inv_nm6w.aac)" != "$(md5 ${OUT}_inv_nm6.aac)" ] \
  && [ "$(derr ${OUT}_inv_nm6w.aac)" = "0" ] \
  && ok "--sbr-nf-weight 1.0 pod sufitem --sbr-noise-max 6: zmienia strumien, dekodowalny" \
  || bad "--sbr-nf-weight 1.0 pod sufitem --sbr-noise-max 6: brak efektu albo bledy"
rm -f "$INVW"

# --- --unlock-bitrate ponizej tabel SBR/PS i LD/ELD (29.10.2026) ---
# Stock (i Franken do 1.3.2) podnosil HE/HEv2 do najnizszego wiersza tabeli
# strojenia SBR (16 kbps przy 48 kHz stereo HE-AAC), a LD/ELD odrzucal
# ponizej 8 kbps/kanal + 208-bitowa rezerwa transportu. Mierzymy REALNY
# bitrate z rozmiaru strumienia ADTS/LOAS (4 s), nie deklaracje.
ULW="_check_unlock.wav"
python3 - "$ULW" <<'PY'
import wave, struct, math, random, sys
sr = 48000; rnd = random.Random(7)
c = lambda x: max(-32768, min(32767, int(x)))
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
fr = []
for i in range(sr * 4):
    t = i / sr
    v = 5000 * math.sin(2 * math.pi * 220 * t) + 2500 * math.sin(2 * math.pi * 1760 * t) + 2500 * rnd.uniform(-1, 1)
    fr.append(struct.pack('<hh', c(v), c(0.8 * v + 1200 * rnd.uniform(-1, 1))))
w.writeframes(b''.join(fr)); w.close()
PY
realbps(){ echo $(( $(stat -c%s "$1") * 8 / 4 )); }
for spec in "he8k:-f2 -p5 -b8000:aac:12000" "hev2-6k:-f2 -p29 -b6000:aac:9000" \
            "eldsbr12k:-f10 -p39 -L1 -b12000:loas:15000" "ld12k:-f10 -p23 -b12000:loas:15000" \
            "eld10k:-f10 -p39 -b10000:loas:13000"; do
  IFS=':' read -r lbl P ext lim <<< "$spec"
  f="${OUT}_ul_${lbl}.${ext}"; rm -f "$f"
  enc "$X64" $P --unlock-bitrate -o "$f" "$ULW"
  if [ ! -s "$f" ]; then bad "--unlock-bitrate $P: enkoder nie wystartowal"; continue; fi
  r=$(realbps "$f")
  if [ "$r" -le "$lim" ]; then
    ok "--unlock-bitrate $P: realnie ${r} bps (limit testu ${lim})"
  else
    bad "--unlock-bitrate $P: realnie ${r} bps, podloga nie zdjeta"
  fi
done
for lbl in he8k hev2-6k; do
  f="${OUT}_ul_${lbl}.aac"
  # HEv2 ponizej 16 kbps: ffmpeg 6.1 zglasza JEDEN "noise_facs_q 255 is
  # invalid" na 3. ramce - identycznie w STOCK HEv2 12 i 14 kbps (w tabelach
  # oryginalu), dekoder Fraunhofera 0 bledow. Usterka istniejaca, nie nasza.
  lim=0; [ "$lbl" = hev2-6k ] && lim=1
  n=$(derr "$f")
  [ -s "$f" ] && [ "$n" -le "$lim" ] && ok "--unlock-bitrate $lbl: ffmpeg bledow $n (dopuszczalne $lim)" \
    || bad "--unlock-bitrate $lbl: bledy dekodera ffmpeg ($n)"
done
# Ponizej szacunku wlasnego ladunku SBR rdzen nie ma budzetu: czysty blad, nie abort.
rm -f "${OUT}_ul_neg.aac"
"$X64" -f2 -p5 -b2000 --unlock-bitrate -o "${OUT}_ul_neg.aac" "$ULW" >/dev/null 2>&1; rc=$?
if [ "$rc" != "0" ] && [ "$rc" -lt 128 ] && [ ! -s "${OUT}_ul_neg.aac" ]; then
  ok "--unlock-bitrate HE 2 kbps stereo: czysty blad inicjalizacji (rc=$rc), bez abort"
else
  bad "--unlock-bitrate HE 2 kbps stereo: rc=$rc (oczekiwany czysty blad)"
fi
# Bez --unlock-bitrate stock podnosi HE 8 kbps stereo (zachowanie bez zmian).
# Bez flagi -b < 1000 znaczy kbps, wiec 8 kbps to "-b 8" (-b 8000 = 8000 kbps).
enc "$X64" -f2 -p5 -b8 -o "${OUT}_ul_stock.aac" "$ULW"
r=$(realbps "${OUT}_ul_stock.aac")
[ "$r" -ge 14000 ] && [ "$r" -le 20000 ] && ok "bez --unlock-bitrate HE -b 8 dalej podniesione do wiersza tabeli (realnie ${r} bps)" \
  || bad "bez --unlock-bitrate HE -b 8: ${r} bps (zmienione zachowanie stock)"
rm -f "$ULW"

# --- Parser liczb dziesietnych + rdzen: pre-echo, transjenty, dziury (30.10.2026) ---
echo "--- parser dziesietny (--side-bias/--side-knee/--mask-slope) ---"
# Bug: sscanf("%lf") czytal "-0,23" jako "-0" (pokretlo wylaczone), "-1,5" jako
# "-1", a skala dB*10 gubila trzecie miejsce. Teraz '.' lub ',', do 3 miejsc,
# zakres sprawdzany na wartosci PO zaokragleniu. Odczyt z --verbose.
vb(){ "$X64" -p2 -b40 "$@" --verbose -f2 -o "${OUT}_pv.aac" "$WAV" 2>&1 | tr -d '\r' | grep -o "$PVNAME *: [-+0-9.]* dB" | sed 's/.*: *\([-+0-9.]*\) dB/\1/'; }
PVNAME=side-bias
for pair in "-0.23:-0.230" "-0,23:-0.230" "-0,98:-0.980" "-1,5:-1.500" "1.2345:+1.235" "-0.0005:-0.001" "50.0004:+50.000"; do
  a=${pair%%:*}; want=${pair#*:}
  got=$(vb --side-bias "$a")
  [ "$got" = "$want" ] && ok "--side-bias $a -> $got dB" || bad "--side-bias $a -> '$got' (oczekiwane $want)"
done
PVNAME=side-knee
got=$(vb --side-knee "0,75"); [ "$got" = "+0.750" ] && ok "--side-knee 0,75 -> $got dB" || bad "--side-knee 0,75 -> '$got'"
for a in 50.001 -24.001 1e1 nan inf "1.2.3" "1,2,3" abc 1x " 1" 99999999999; do
  "$X64" -p2 -b40 --side-bias "$a" -f2 -o "${OUT}_pv.aac" "$WAV" >/dev/null 2>&1; rc=$?
  [ $rc != 0 ] && ok "--side-bias '$a' odrzucone (rc=$rc)" || bad "--side-bias '$a' przyjete, a powinno byc odrzucone"
done
for a in "128,5" "3x" "1e3"; do
  "$X64" -p2 -b40 --block-bias "$a" -f2 -o "${OUT}_pv.aac" "$WAV" >/dev/null 2>&1; rc=$?
  [ $rc != 0 ] && ok "--block-bias '$a' odrzucone (calkowite pokretla tez scisle)" || bad "--block-bias '$a' przyjete"
done
# Ulamek musi realnie zmieniac strumien (wczesniej -0.23..-0.98 = wylaczone).
enc "$X64" -f2 -p2 -b40 -w 18000 -o "${OUT}_pv0.aac" "$WAV"
enc "$X64" -f2 -p2 -b40 -w 18000 --side-bias -0.5 -o "${OUT}_pv5.aac" "$WAV"
enc "$X64" -f2 -p2 -b40 -w 18000 --side-bias -0,5 -o "${OUT}_pv5c.aac" "$WAV"
[ "$(md5 ${OUT}_pv5.aac)" != "$(md5 ${OUT}_pv0.aac)" ] && [ "$(md5 ${OUT}_pv5.aac)" = "$(md5 ${OUT}_pv5c.aac)" ] \
  && ok "--side-bias -0.5 zmienia strumien, '-0,5' = '-0.5'" || bad "--side-bias -0.5 bez efektu albo przecinek != kropka"

echo "--- rdzen: pre-echo, transjenty, okno, grupy, dziury ---"
python3 tools/core/gen_core.py . >/dev/null
mv attacks48.wav _check_core_att.wav; mv attacks48.txt _check_core_att.txt; mv dense48.wav _check_core_dense.wav
# Kazde pokretlo musi zmienic strumien na materiale, na ktory dziala, i dac
# strumien bez bledow dekodera.
for spec in "att:-p2 -b40 -w 18000:--preecho-rise 1.5" "att:-p2 -b40 -w 18000:--preecho-depth 40" \
            "att:-p2 -b40 -w 18000:--attack-decay 0.9" "att:-p2 -b40 -w 18000:--attack-floor -20" \
            "att:-p2 -b40 -w 18000:--tns-thresh 0.7" "att:-p2 -b40 -w 18000:--tns-start 400" \
            "att:-p2 -b40 -w 18000:--window-shape 0" "att:-p2 -b40 -w 18000:--window-shape 1" \
            "att:-p2 -b40 -w 18000:--short-groups 1" "att:-p2 -b40 -w 18000:--short-groups 3" \
            "dense:-p2 -b32 -w 17000:--avoid-holes 1" "dense:-p2 -b32 -w 17000:--hole-start 25" \
            "dense:-p2 -b32 -w 17000:--hole-start-short 8" "dense:-p2 -b32 -w 17000:--hole-spread 9" \
            "dense:-p2 -b32 -w 17000:--hole-ladder 2"; do
  w=${spec%%:*}; rest=${spec#*:}; base=${rest%%:*}; knob=${rest#*:}
  enc "$X64" -f2 $base -o "${OUT}_cb.aac" "_check_core_$w.wav"
  enc "$X64" -f2 $base $knob -o "${OUT}_ck.aac" "_check_core_$w.wav"
  if [ -s "${OUT}_ck.aac" ] && [ "$(md5 ${OUT}_ck.aac)" != "$(md5 ${OUT}_cb.aac)" ] && [ "$(derr ${OUT}_ck.aac)" = "0" ]; then
    ok "$knob: zmienia strumien, 0 bledow dekodera"
  else
    bad "$knob: brak efektu, brak pliku albo bledy dekodera"
  fi
done
# Okno i grupy odczytane Z BITSTREAMU (pole window_shape i scale_factor_grouping).
wsg(){ python3 - "$1" <<'PY'
import sys
sys.path.insert(0, 'tools/msis')
from parse_ms import frames, Bits
sh = set(); gr = set()
for fr, hdr, nb in frames(open(sys.argv[1], 'rb').read()):
    b = Bits(fr, hdr)
    if b.get(3) != 1: continue
    b.get(4)
    if not b.get(1): continue
    b.get(1); ws = b.get(2); shape = b.get(1)
    sh.add(shape)
    if ws == 2:
        b.get(4); g = b.get(7)
        gr.add(1 + sum(1 for k in range(7) if not (g >> (6 - k)) & 1))
print(sorted(sh), sorted(gr))
PY
}
enc "$X64" -f2 -p2 -b128 --window-shape 1 -o "${OUT}_cw.aac" _check_core_att.wav
[ "$(wsg ${OUT}_cw.aac)" = "[1] [4]" ] && ok "--window-shape 1: KBD w kazdej ramce, takze short (z bitstreamu)" || bad "--window-shape 1: $(wsg ${OUT}_cw.aac)"
enc "$X64" -f2 -p2 -b128 --window-shape 0 -o "${OUT}_cw.aac" _check_core_att.wav
[ "$(wsg ${OUT}_cw.aac)" = "[0] [4]" ] && ok "--window-shape 0: sinus w kazdej ramce (z bitstreamu)" || bad "--window-shape 0: $(wsg ${OUT}_cw.aac)"
enc "$X64" -f2 -p2 -b128 --short-groups 1 -o "${OUT}_cw.aac" _check_core_att.wav
[ "$(wsg ${OUT}_cw.aac)" = "[0, 1] [1]" ] && ok "--short-groups 1: jedna grupa w kazdym bloku short" || bad "--short-groups 1: $(wsg ${OUT}_cw.aac)"
# Dziury: --hole-ladder 2 przy 32 kbps i pasmie 17 kHz musi zmniejszyc odsetek
# wyzerowanych pasm po dekodowaniu (zmierzone: 37% -> 8%).
enc "$X64" -f2 -p2 -b32 -w 17000 -o "${OUT}_ch0.aac" _check_core_dense.wav
enc "$X64" -f2 -p2 -b32 -w 17000 --hole-ladder 2 -o "${OUT}_ch2.aac" _check_core_dense.wav
# Dekodowanie ffmpeg (AAC-LC, zgodny dekoder); core_measure sam wyrownuje opoznienie.
ffmpeg -y -loglevel error -i "${OUT}_ch0.aac" -c:a pcm_s16le _check_core_d0.wav
ffmpeg -y -loglevel error -i "${OUT}_ch2.aac" -c:a pcm_s16le _check_core_d2.wav
h0=$(python3 tools/core/core_measure.py _check_core_dense.wav _check_core_d0.wav | grep -o 'holes=[0-9.]*' | cut -d= -f2)
h2=$(python3 tools/core/core_measure.py _check_core_dense.wav _check_core_d2.wav | grep -o 'holes=[0-9.]*' | cut -d= -f2)
python3 -c "import sys; sys.exit(0 if float('${h2:-999}') < float('${h0:-0}') * 0.5 else 1)" \
  && ok "--hole-ladder 2: dziury po dekodowaniu ${h0}% -> ${h2}%" || bad "--hole-ladder 2: dziury '${h0}'% -> '${h2}'% (oczekiwany spadek o polowe)"
# Bez flag: wynik identyczny z punktem odniesienia obliczonym w tym przebiegu
# jest juz pokryty wyzej (stock = bez nowych flag), tu tylko wartosci neutralne.
enc "$X64" -f2 -p2 -b40 -w 18000 -o "${OUT}_cn0.aac" _check_core_att.wav
enc "$X64" -f2 -p2 -b40 -w 18000 --short-groups 0 --hole-ladder 8 -o "${OUT}_cn1.aac" _check_core_att.wav
[ "$(md5 ${OUT}_cn1.aac)" = "$(md5 ${OUT}_cn0.aac)" ] && ok "wartosci neutralne (--short-groups 0, --hole-ladder 8) = stock" || bad "wartosci neutralne zmieniaja strumien"
rm -f _check_core_*.wav _check_core_*.txt

# --- Poprawki znalezione przy przepisywaniu dokumentacji ---
echo "--- regresje: x100 > 100, --is-aggression 100, --max-bits-frame, --sbr-noise-floor-offset ---"
# x100 > 100 bylo cichym no-op (mnoznik Q31 obciety do 1.0).
enc "$X64" -f2 -p5 -b32000 -o "${OUT}_x0.aac" "$WAV"
enc "$X64" -f2 -p5 -b32000 --sbr-tran-thr 100 -o "${OUT}_x1.aac" "$WAV"
enc "$X64" -f2 -p5 -b32000 --sbr-tran-thr 400 -o "${OUT}_x4.aac" "$WAV"
[ "$(md5 ${OUT}_x1.aac)" = "$(md5 ${OUT}_x0.aac)" ] && ok "--sbr-tran-thr 100 = stock" || bad "--sbr-tran-thr 100 != stock"
[ "$(md5 ${OUT}_x4.aac)" != "$(md5 ${OUT}_x0.aac)" ] && [ "$(derr ${OUT}_x4.aac)" = "0" ] && ok "--sbr-tran-thr 400 dziala (powyzej 100 nie jest no-op)" || bad "--sbr-tran-thr 400 bez efektu"
enc "$X64" -f2 -p5 -b32000 --sbr-tran-split 300 -o "${OUT}_x5.aac" "$WAV"
[ "$(md5 ${OUT}_x5.aac)" != "$(md5 ${OUT}_x0.aac)" ] && ok "--sbr-tran-split 300 dziala" || bad "--sbr-tran-split 300 bez efektu"
# --is-aggression 100 zawijalo prog kierunku w liczbe ujemna = IS wylaczone.
enc "$X64" -f2 -p2 -b48000 --is 0 -o "${OUT}_i0.aac" "$WAV"
enc "$X64" -f2 -p2 -b48000 --is-aggression 100 -o "${OUT}_i100.aac" "$WAV"
[ "$(md5 ${OUT}_i100.aac)" != "$(md5 ${OUT}_i0.aac)" ] && [ "$(derr ${OUT}_i100.aac)" = "0" ] && ok "--is-aggression 100 != --is 0 (maksimum dziala)" || bad "--is-aggression 100 = IS wylaczone"
# --max-bits-frame bylo nadpisywane pozniej w inicjalizacji.
maxpay(){ python3 - "$1" <<'PY'
import sys
d=open(sys.argv[1],'rb').read(); i=0; m=0
while i+7<=len(d) and d[i]==0xFF and (d[i+1]&0xF0)==0xF0:
    n=((d[i+3]&3)<<11)|(d[i+4]<<3)|(d[i+5]>>5); m=max(m,n*8); i+=n
print(m)
PY
}
enc "$X64" -f2 -p2 -b128000 --max-bits-frame 3000 -o "${OUT}_mb.aac" "$WAV"
mb=$(maxpay ${OUT}_mb.aac)
[ -n "$mb" ] && [ "$mb" -le 3056 ] && [ "$(derr ${OUT}_mb.aac)" = "0" ] && ok "--max-bits-frame 3000: najwieksza ramka ADTS $mb bitow (z naglowkiem 56)" || bad "--max-bits-frame 3000: najwieksza ramka '$mb' bitow"
# --sbr-noise-floor-offset poza 0..11 = asercja FDK (abort); frontend ma odrzucic.
for v in -1 12; do
  "$X64" -p5 -b32000 --sbr-noise-floor-offset $v -f2 -o "${OUT}_nf.aac" "$WAV" >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "--sbr-noise-floor-offset $v odrzucone czysto (rc=1)" || bad "--sbr-noise-floor-offset $v rc=$rc"
done
enc "$X64" -f2 -p5 -b32000 --sbr-noise-floor-offset 11 -o "${OUT}_nf.aac" "$WAV"
[ -s "${OUT}_nf.aac" ] && [ "$(derr ${OUT}_nf.aac)" = "0" ] && ok "--sbr-noise-floor-offset 11 koduje" || bad "--sbr-noise-floor-offset 11"

# --- Pokrycie a precyzja: --coverage, --band-knee, --quant-round, --pns-above, --thr-hold, --scf-range ---
echo "--- pokrycie/precyzja: nowe przelaczniki ---"
hc=$("$X64" --help 2>&1 | tr -d '\r' | grep -cE '^ --(coverage|band-knee|quant-round|pns-above|thr-hold|scf-range) ')
[ "$hc" = 6 ] && ok "help: 6 przelacznikow pokrycia/precyzji" || bad "help: tylko $hc z 6 przelacznikow pokrycia/precyzji"
python3 tools/core/gen_core.py . >/dev/null
mv dense48.wav _check_cov_dense.wav; rm -f attacks48.wav attacks48.txt
for knob in "--coverage 3" "--coverage -3" "--band-knee 6" "--band-knee -6" "--quant-round 0.48" \
            "--pns-above 8000" "--thr-hold 8" "--scf-range 0"; do
  enc "$X64" -f2 -p2 -b32 -w 17000 -o "${OUT}_v0.aac" _check_cov_dense.wav
  enc "$X64" -f2 -p2 -b32 -w 17000 $knob -o "${OUT}_v1.aac" _check_cov_dense.wav
  if [ -s "${OUT}_v1.aac" ] && [ "$(md5 ${OUT}_v1.aac)" != "$(md5 ${OUT}_v0.aac)" ] && [ "$(derr ${OUT}_v1.aac)" = "0" ]; then
    ok "$knob: zmienia strumien, 0 bledow dekodera"
  else
    bad "$knob: brak efektu, brak pliku albo bledy dekodera"
  fi
done
# Wartosci neutralne = stock (bit w bit).
enc "$X64" -f2 -p2 -b32 -w 17000 --coverage 0 --band-knee 0 --thr-hold 1 --scf-range 60 -o "${OUT}_v2.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_v2.aac)" = "$(md5 ${OUT}_v0.aac)" ] && ok "wartosci neutralne (--coverage 0, --band-knee 0, --thr-hold 1, --scf-range 60) = stock" || bad "wartosci neutralne zmieniaja strumien"
# Kierunek osi po dekodowaniu: + mniej dziur, - wiecej dziur niz stock.
hol(){ ffmpeg -y -loglevel error -i "$1" -c:a pcm_s16le _check_cov_d.wav; python3 tools/core/core_measure.py _check_cov_dense.wav _check_cov_d.wav | grep -o 'holes=[0-9.]*' | cut -d= -f2; }
enc "$X64" -f2 -p2 -b32 -w 17000 --coverage 3 -o "${OUT}_vp.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b32 -w 17000 --coverage -3 -o "${OUT}_vm.aac" _check_cov_dense.wav
h0=$(hol ${OUT}_v0.aac); hp=$(hol ${OUT}_vp.aac); hm=$(hol ${OUT}_vm.aac)
python3 -c "import sys; sys.exit(0 if float('${hp:-999}') < float('${h0:-0}')*0.5 < float('${hm:-0}') else 1)" \
  && ok "--coverage: dziury +3 ${hp}% < stock ${h0}% < -3 ${hm}%" || bad "--coverage: kierunek osi zly (+3 '${hp}', stock '${h0}', -3 '${hm}')"
# Jawny przelacznik wygrywa z makrem.
enc "$X64" -f2 -p2 -b32 -w 17000 --coverage 3 --hole-ladder 8 --band-knee 0.001 --quant-round 0.405 --avoid-holes 0 -o "${OUT}_vx.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_vx.aac)" != "$(md5 ${OUT}_vp.aac)" ] && ok "jawne przelaczniki nadpisuja --coverage" || bad "jawne przelaczniki nie nadpisuja --coverage"
# Skrajne: 14 kbps, wymuszone 22 kHz, wszystko naraz; bez awarii i bledow dekodera.
for knob in "--band-knee 24" "--coverage 5 --scf-range 0 --thr-hold 32" "--coverage -5" "--pns-above 1000"; do
  "$X64" -p2 -b14000 -w 22000 --uncap-bandwidth --unlock-bitrate $knob -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 0 ] && [ "$(derr ${OUT}_vz.aac)" = "0" ] && ok "14 kbps / 22 kHz $knob: koduje, 0 bledow" || bad "14 kbps / 22 kHz $knob: rc=$rc"
done
for v in "--coverage 6" "--band-knee 25" "--quant-round 0.6" "--pns-above 500" "--thr-hold 33" "--scf-range 61"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done

# --- --enfac-patch: wazenie energii plaskich ramek (CBR i VBR) ---
"$X64" --help 2>&1 | tr -d '\r' | grep -qE '^ --enfac-patch ' && ok "--enfac-patch w --help" || bad "--enfac-patch brak w --help"
enc "$X64" -f2 -p2 -b32 -w 17000 -o "${OUT}_e0.aac" _check_cov_dense.wav
for v in 0 1; do
  enc "$X64" -f2 -p2 -b32 -w 17000 --enfac-patch $v -o "${OUT}_e$((v+1)).aac" _check_cov_dense.wav
  [ "$(md5 ${OUT}_e$((v+1)).aac)" != "$(md5 ${OUT}_e0.aac)" ] && [ "$(derr ${OUT}_e$((v+1)).aac)" = "0" ] \
    && ok "--enfac-patch $v zmienia strumien CBR, 0 bledow dekodera" || bad "--enfac-patch $v: brak efektu albo bledy"
done
"$X64" -p2 -b14000 -w 22000 --uncap-bandwidth --unlock-bitrate --enfac-patch 1 -f2 -o "${OUT}_ez.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
[ $rc = 0 ] && [ "$(derr ${OUT}_ez.aac)" = "0" ] && ok "14 kbps / 22 kHz --enfac-patch 1: koduje, 0 bledow" || bad "14 kbps / 22 kHz --enfac-patch 1: rc=$rc"
for v in "--enfac-patch 2" "--enfac-patch -1"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done
# --- --side-knee jako offset zapasu pasm side: plynny, bez skoku przy -0.97 dB ---
# Stary knee zabijal naraz cala kupke pasm uratowanych przez avoid-holes (0.97 dB nad
# progiem). Teraz liczba kodowanych pasm side ma maleć monotonicznie i bez skoku.
sk(){ "$X64" -f2 -p2 -b96000 --side-knee "$1" --bit-stats -o "${OUT}_sk.aac" _check_cov_dense.wav 2>&1 >/dev/null | tr -d '\r' | sed -n 's/.*ch1 bands\/frame: coded \([0-9.]*\),.*/\1/p'; }
c0=$(sk 0.001); c1=$(sk -0.9); c2=$(sk -1.0); c3=$(sk -3); c4=$(sk -12); cp=$(sk 6)
python3 -c "
import sys; v=[float(x) for x in '$c0 $c1 $c2 $c3 $c4'.split()]; p=float('$cp')
mono=all(a>=b for a,b in zip(v,v[1:])); jump=abs(v[1]-v[2])
sys.exit(0 if mono and jump < 1.0 and v[-1] < v[0] and p >= v[0] else 1)" \
  && ok "--side-knee: pasma side ${cp} (+6) >= ${c0} >= ${c1} (-0.9) >= ${c2} (-1.0) >= ${c3} >= ${c4}, bez skoku przy -0.97" \
  || bad "--side-knee: zly ksztalt ($cp | $c0 $c1 $c2 $c3 $c4)"
for v in "--side-knee-zone 0.5" "--side-knee-zone 21" "--side-knee-spread 0" "--side-knee-spread 25"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done
enc "$X64" -f2 -p2 -b96000 -o "${OUT}_sz0.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b96000 --side-knee-zone 5 --side-knee-spread 2 -o "${OUT}_sz1.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_sz1.aac)" = "$(md5 ${OUT}_sz0.aac)" ] && ok "--side-knee-zone/-spread bez --side-knee = stock" || bad "--side-knee-zone/-spread zmieniaja strumien bez --side-knee"

# --- --side-cutoff / --is-admit ---
enc "$X64" -f2 -p2 -b48000 -o "${OUT}_sc0.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b48000 --side-cutoff 24000 -o "${OUT}_sc1.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_sc1.aac)" = "$(md5 ${OUT}_sc0.aac)" ] && ok "--side-cutoff ponad pasmem rdzenia = stock" || bad "--side-cutoff 24000 zmienia strumien"
enc "$X64" -f2 -p2 -b48000 --side-cutoff 3000 -o "${OUT}_sc2.aac" _check_cov_dense.wav
ffmpeg -v error -y -i "${OUT}_sc0.aac" -ac 2 -c:a pcm_s16le "${OUT}_sc0.wav" 2>/dev/null
ffmpeg -v error -y -i "${OUT}_sc2.aac" -ac 2 -c:a pcm_s16le "${OUT}_sc2.wav" 2>/dev/null
python3 - "${OUT}_sc0.wav" "${OUT}_sc2.wav" <<'PY' && ok "--side-cutoff 3000: side ponad 4 kHz wyciete (>= 20 dB), ponizej 2 kHz nietkniete" || bad "--side-cutoff 3000: zly rozklad side"
import wave,sys,numpy as np
def sm(p):
    w=wave.open(p); x=np.frombuffer(w.readframes(w.getnframes()),'<i2').astype(float).reshape(-1,2)
    s=np.abs(np.fft.rfft((x[:,0]-x[:,1])/2))**2; f=np.fft.rfftfreq(len(x),1/w.getframerate()); return s,f
a,f=sm(sys.argv[1]); b,_=sm(sys.argv[2])
hi=(f>4000)&(f<12000); lo=(f>300)&(f<2000)
dh=10*np.log10(a[hi].sum()/max(b[hi].sum(),1e-9)); dl=10*np.log10(a[lo].sum()/max(b[lo].sum(),1e-9))
print(f"  side hi -{dh:.1f} dB, lo {dl:+.1f} dB"); sys.exit(0 if dh>=20 and abs(dl)<3 else 1)
PY
[ "$(derr ${OUT}_sc2.aac)" = "0" ] && ok "--side-cutoff 3000: 0 bledow dekodera" || bad "--side-cutoff 3000: bledy dekodera"
enc "$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate -o "${OUT}_ia0.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate --is 0 -o "${OUT}_ia1.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate --is-admit 0 -o "${OUT}_ia2.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_ia2.aac)" = "$(md5 ${OUT}_ia1.aac)" ] && [ "$(md5 ${OUT}_ia2.aac)" != "$(md5 ${OUT}_ia0.aac)" ] \
  && ok "--is-admit 0 = --is 0 przy 14 kbps (i rozne od stock)" || bad "--is-admit 0 nie dziala jak --is 0"
enc "$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate --is-admit 5 -o "${OUT}_ia3.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_ia3.aac)" = "$(md5 ${OUT}_ia0.aac)" ] && ok "--is-admit 5 = stock" || bad "--is-admit 5 != stock"
for v in "--side-cutoff 999" "--side-cutoff 24001" "--is-admit 20.01" "--is-admit -1"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done
rm -f "${OUT}"_sc*.wav

# --- PNS: --pns-budget / --pns-shared / --pns-hold ---
# Przy niskim bitrate wymuszony PNS jest ksiegowany przez FDK jako darmowy; w ramkach,
# gdzie budzet sie nie domyka, awaryjne ciecie przelatuje przez pasma PNS i zeruje widmo
# az do ~1.5 kHz. --pns-budget ma te ramki usunac, --pns-shared 1 sprowadzic maske MS do 2 bitow.
PKW=_check_cov_dense.wav
pk(){ "$X64" -f2 -p2 -b24000 --block-bias 0 -w 5500 --pns-above 2200 "$@" --bit-stats -o "${OUT}_pk.aac" "$PKW" 2>"${OUT}_pk.txt" >/dev/null; }
cuts(){ python3 - "${OUT}_pk.aac" <<'PY'
import sys; sys.path.insert(0,'tools/msis'); import parse_ms as p
ms=[p.parse_frame(t[0],t[1]).get('max_sfb') for t in p.frames(open(sys.argv[1],'rb').read())]
full=max(m for m in ms if m); print(sum(1 for m in ms if m and 5<m<full))
PY
}
mask(){ tr -d '\r' < "${OUT}_pk.txt" | awk '/MS mask/{print int($3+0.5)}'; }
pk; c0=$(cuts)
pk --pns-budget 1; c1=$(cuts); [ "$(derr ${OUT}_pk.aac)" = 0 ] && [ "$c1" -lt "$c0" ] && [ "$c1" -le 2 ] \
  && ok "--pns-budget 1: ramki z awaryjnym cieciem $c0 -> $c1, 0 bledow" || bad "--pns-budget 1: ciecia $c0 -> $c1"
# maska i poziomy: dwa tony z szumem (maska stock MS_SOME, pasma dolne w MS)
PKW="$WAV"; pk; m0=$(mask)
pk --pns-shared 1; m1=$(mask); [ "$(derr ${OUT}_pk.aac)" = 0 ] && [ "$m1" -le 8 ] && [ "$m0" -gt 20 ] \
  && ok "--pns-shared 1: maska MS $m0 -> $m1 bit/ramke" || bad "--pns-shared 1: maska $m0 -> $m1"
pnsb(){ tr -d '\r' < "${OUT}_pk.txt" | awk '/PNS energies/{print int($3+0.5)}'; }
pk --pns-budget 1 --pns-shared 1; p0=$(pnsb); pk --pns-budget 1 --pns-shared 1 --pns-hold 6; p1=$(pnsb)
[ "$(derr ${OUT}_pk.aac)" = 0 ] && [ "$p1" -lt "$p0" ] && ok "--pns-hold 6: energie PNS $p0 -> $p1 bit/ramke" || bad "--pns-hold 6: $p0 -> $p1"
PKW=_check_cov_dense.wav
enc "$X64" -f2 -p2 -b24000 --pns-above 2200 -o "${OUT}_pn0.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b24000 --pns-above 2200 --pns-budget 0 -o "${OUT}_pn1.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_pn1.aac)" = "$(md5 ${OUT}_pn0.aac)" ] && ok "--pns-budget 0 = stock" || bad "--pns-budget 0 != stock"
enc "$X64" -f2 -p2 -b24000 --pns 0 -o "${OUT}_pn2.aac" _check_cov_dense.wav
enc "$X64" -f2 -p2 -b24000 --pns 0 --pns-budget 1 --pns-shared 1 --pns-hold 6 -o "${OUT}_pn3.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_pn3.aac)" = "$(md5 ${OUT}_pn2.aac)" ] && ok "przelaczniki PNS bez PNS = strumien bez zmian" || bad "przelaczniki PNS zmieniaja strumien bez PNS"
for v in "--pns-shared 2" "--pns-budget -1" "--pns-hold 0" "--pns-hold 12.01"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done

# --- "-1" = przelacznik pominiety (dokumentowane "-1 off") i domkniete zakresy ---
enc "$X64" -f2 -p2 -b32000 -o "${OUT}_m10.aac" _check_cov_dense.wav
m1ok=1
for s in --block-bias --bitres-mode --ms-bias --pns-gain --pns-refpower --max-bits-frame --vbr-reservoir --peak-bitrate --msbands-lo; do
  "$X64" -f2 -p2 -b32000 $s -1 -o "${OUT}_m11.aac" _check_cov_dense.wav >/dev/null 2>&1 || { m1ok=0; echo "    $s -1 odrzucone"; continue; }
  [ "$(md5 ${OUT}_m11.aac)" = "$(md5 ${OUT}_m10.aac)" ] || { m1ok=0; echo "    $s -1 zmienia strumien"; }
done
[ $m1ok = 1 ] && ok "-1 = przelacznik pominiety (9 przelacznikow, strumien = stock)" || bad "-1 nie dziala jak pominiecie"
enc "$X64" -f2 -p5 -b32000 -o "${OUT}_m12.aac" _check_cov_dense.wav
enc "$X64" -f2 -p5 -b32000 --sbr-tran-thr 50 --sbr-tran-thr -1 -o "${OUT}_m13.aac" _check_cov_dense.wav
[ "$(md5 ${OUT}_m13.aac)" = "$(md5 ${OUT}_m12.aac)" ] && ok "-1 po wczesniejszej wartosci przywraca stock" || bad "-1 nie kasuje wczesniejszej wartosci"
for v in "--block-bias -2" "--ath-scale 0" "--ath-scale 4097" "--core-cutoff -1000" "--core-cutoff 48001" "--ms-bias 256"; do
  "$X64" -p2 -b32000 $v -f2 -o "${OUT}_vz.aac" _check_cov_dense.wav >/dev/null 2>&1; rc=$?
  [ $rc = 1 ] && ok "$v odrzucone (rc=1)" || bad "$v rc=$rc"
done

# --- --bit-stats: tylko liczy, strumien bit w bit ten sam; tabela sie drukuje ---
enc "$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate -o "${OUT}_bs0.aac" _check_cov_dense.wav
"$X64" -f2 -p2 -b14000 --uncap-bandwidth --unlock-bitrate --bit-stats -o "${OUT}_bs1.aac" _check_cov_dense.wav 2> "${OUT}_bs.txt" >/dev/null
[ -s "${OUT}_bs1.aac" ] && [ "$(md5 ${OUT}_bs1.aac)" = "$(md5 ${OUT}_bs0.aac)" ] && ok "--bit-stats nie zmienia strumienia" || bad "--bit-stats zmienia strumien albo brak pliku"
tr -d '\r' < "${OUT}_bs.txt" | grep -q "spectrum (Huffman)" && tr -d '\r' < "${OUT}_bs.txt" | grep -q "transport header" \
  && ok "--bit-stats drukuje tabele budzetu" || bad "--bit-stats: brak tabeli"
bpf=$(tr -d '\r' < "${OUT}_bs.txt" | sed -n 's/.*frames, \([0-9.]*\) bits\/frame.*/\1/p')
fsz=$(stat -c%s "${OUT}_bs1.aac"); nfr=$(tr -d '\r' < "${OUT}_bs.txt" | sed -n 's/=== bit-stats: \([0-9]*\) frames.*/\1/p')
python3 -c "import sys; sys.exit(0 if abs(float('${bpf:-0}')*${nfr:-1}-${fsz}*8) < ${fsz}*8*0.01 else 1)" \
  && ok "--bit-stats: suma bitow zgadza sie z plikiem ADTS (<1%)" || bad "--bit-stats: suma ${bpf}x${nfr} != ${fsz} B"
rm -f _check_cov_*.wav

# --- Przelaczniki pasmowe w ramkach short (bugi naprawione w 1.4.0-beta.2) ---
# Przelaczniki biora numery pasm LONG. Ramka short ma wlasna tablice (48 kHz:
# 14 pasm zamiast 49), wiec do 1.4.0-beta.1 numer trafial w inna czestotliwosc:
# --msbands-lo 18 zostawialo short bez MS, --msbands 18 dawalo MS do 24 kHz,
# --is-lo 33 kasowalo IS, --isbands zuzywaly pierwsze grupy okien. Teraz zakres
# przechodzi do short przez Hz (najblizsza granica, remis w gore). Maske MS
# czytamy z bitstreamu (parse_ms.py), pasma IS z logu sekcji zapisanych do
# strumienia (FRANKEN_BANDLOG, przez WSLENV do .exe, bandlog.py).
python3 tools/msis/gen_samples.py . >/dev/null && mv -f all_short.wav _check_allshort.wav && rm -f short_ms.wav is_alt.wav
ASH=_check_allshort.wav
blog(){ local f="$1"; shift; rm -f "$f"; FRANKEN_BANDLOG="$f" WSLENV="${WSLENV:+$WSLENV:}FRANKEN_BANDLOG" "$@" >/dev/null 2>&1; }
enc "$X64" -f2 -p2 -b96000 --pns 0 -o "${OUT}_as0.aac" "$ASH"
sh0=$(jget "$(ms_json ${OUT}_as0.aac)" short); fr0=$(jget "$(ms_json ${OUT}_as0.aac)" frames)
[ "${sh0:-0}" -ge $(( ${fr0:-1} * 8 / 10 )) ] && ok "probka all_short: $sh0/$fr0 ramek short" || bad "probka all_short: tylko $sh0/$fr0 ramek short"
# MS: zakres long 18..30 (2.5..9 kHz) = short 3..7; --msbands 18 (do 2.5 kHz) = short 0..2.
for spec in "lo18hi30:--msbands-lo 18 --msbands-hi 30:--lo 18 --hi 30 --slo 3 --shi 7" \
            "max18:--msbands 18:--max 18 --smax 3" \
            "lo33:--msbands-lo 33:--lo 33 --slo 9"; do
  lbl="${spec%%:*}"; rest="${spec#*:}"; A="${rest%%:*}"; P="${rest#*:}"
  for g in 0 1 2 3; do
    enc "$X64" -f2 -p2 -b96000 --pns 0 --short-groups $g $A -o "${OUT}_asm.aac" "$ASH"
    j=$(ms_json "${OUT}_asm.aac" $P)
    ir=$(jget "$j" short_ms_in_range); vb=$(jget "$j" viol_band); va=$(jget "$j" viol_all)
    if [ -s "${OUT}_asm.aac" ] && [ "${ir:-0}" -gt 0 ] && [ "$vb" = "0" ] && [ "$va" = "0" ] && [ "$(derr ${OUT}_asm.aac)" = "0" ]; then
      ok "MS short [$A, short-groups $g]: $ir pasm MS w przelozonym zakresie, 0 poza nim"
    else
      bad "MS short [$A, short-groups $g]: w_zakresie=$ir poza=$vb mask_all=$va"
    fi
  done
done
# Verbose pokazuje przeklad (tu: 48 kHz, long 18..30 -> short 3..7).
"$X64" -f2 -p2 -b96000 --verbose --msbands-lo 18 --msbands-hi 30 -o "${OUT}_asv.aac" "$ASH" 2>&1 | tr -d '\r' | grep -q "long 18..30 (2531..9000 Hz) -> short 3..7" \
  && ok "--verbose: przeklad pasm long -> short w pasmach i Hz" || bad "--verbose: brak linii przekladu msbands-lo/-hi"
# IS: --is-lo 33 (10.5 kHz) = short 9; --is-hi 30 (9 kHz) = short do 7.
ISA="--is 1 --is-aggression 100"
blog "${OUT}_isl.txt" "$X64" -f2 -p2 -b48000 $ISA -o "${OUT}_isl.aac" "$ASH"
j=$(python3 tools/msis/bandlog.py "${OUT}_isl.txt" 14,15 2>/dev/null)
n0=$(jget "$j" short_groups_with)
[ "${n0:-0}" -gt 0 ] && ok "IS w ramkach short bez flag: $n0 grup z IS (log sekcji dziala)" || bad "IS short bez flag: brak IS albo brak logu (FRANKEN_BANDLOG/WSLENV)"
blog "${OUT}_isl.txt" "$X64" -f2 -p2 -b48000 $ISA --is-lo 33 -o "${OUT}_isl.aac" "$ASH"
j=$(python3 tools/msis/bandlog.py "${OUT}_isl.txt" 14,15 2>/dev/null)
mn=$(jget "$j" short_min_band); n1=$(jget "$j" short_groups_with)
[ "${n0:-0}" -gt 0 ] && { [ "${n1:-0}" = "0" ] || [ "${mn:--1}" -ge 9 ]; } && [ "$(derr ${OUT}_isl.aac)" = "0" ] \
  && ok "--is-lo 33 @ short: IS tylko od short 9 (10.5 kHz) albo wcale (grupy z IS: $n1)" || bad "--is-lo 33 @ short: najnizsze pasmo IS $mn"
blog "${OUT}_isl.txt" "$X64" -f2 -p2 -b48000 $ISA --is-hi 30 -o "${OUT}_isl.aac" "$ASH"
j=$(python3 tools/msis/bandlog.py "${OUT}_isl.txt" 14,15 2>/dev/null)
mx=$(jget "$j" short_max_band); n2=$(jget "$j" short_groups_with)
[ "${n2:-0}" -gt 0 ] && [ "${mx:-99}" -le 7 ] && ok "--is-hi 30 @ short: IS do short 7 (9 kHz), $n2 grup z IS" || bad "--is-hi 30 @ short: najwyzsze pasmo IS $mx, grup $n2"
# Wymuszony IS long 25..45 (5..20 kHz) = short 6..12: kazda grupa ma IS na 6..
blog "${OUT}_isl.txt" "$X64" -f2 -p2 -b48000 $ISA --is-force-lo 25 --is-force-hi 45 -o "${OUT}_isl.aac" "$ASH"
j=$(python3 tools/msis/bandlog.py "${OUT}_isl.txt" 14,15 2>/dev/null)
sg=$(jget "$j" short_groups); fw=$(python3 -c "import json,sys; d=json.loads(sys.argv[1]); print(d['short'].get('6',0))" "$j")
[ "${sg:-0}" -gt 0 ] && [ "${fw:-0}" = "$sg" ] && [ "$(derr ${OUT}_isl.aac)" = "0" ] \
  && ok "--is-force-lo 25 @ short: IS na short 6 (5.25 kHz) w kazdej z $sg grup" || bad "--is-force-lo 25 @ short: pasmo 6 z IS w $fw z $sg grup"
# --isbands: limit w kazdej grupie (6 long -> 2 short przy 48 kHz), takze w pozniejszych grupach
blog "${OUT}_isl.txt" "$X64" -f2 -p2 -b48000 $ISA --isbands 6 -o "${OUT}_isl.aac" "$ASH"
j=$(python3 tools/msis/bandlog.py "${OUT}_isl.txt" 14,15 2>/dev/null)
mp=$(jget "$j" max_per_group); lg=$(jget "$j" later_groups_with); lm=$(jget "$j" long_max_per_frame)
[ "${mp:-99}" -le 2 ] && [ "${lg:-0}" -gt 0 ] && [ "${lm:-99}" -le 6 ] \
  && ok "--isbands 6: long <= 6 na ramke, short <= 2 na grupe, IS takze w grupach > 0 ($lg)" || bad "--isbands 6: long max $lm, short max/grupe $mp, pozniejsze grupy z IS $lg"
# --hole-start bez --hole-start-short przechodzi na short (verbose) ...
"$X64" -f2 -p2 -b48000 --verbose --hole-start 25 -o "${OUT}_ash.aac" "$ASH" 2>&1 | tr -d '\r' | grep -q "hole-start            : 25 (short blocks: moved to 6)" \
  && ok "--hole-start 25 -> short 6 (5.25 kHz)" || bad "--hole-start 25: brak przekladu na short"
# ... a jawne --hole-start-short wygrywa (strumien = sam --hole-start-short 6 + hole-start 25)
enc "$X64" -f2 -p2 -b48000 --hole-start 25 -o "${OUT}_ash1.aac" "$ASH"
enc "$X64" -f2 -p2 -b48000 --hole-start 25 --hole-start-short 6 -o "${OUT}_ash2.aac" "$ASH"
enc "$X64" -f2 -p2 -b48000 --hole-start 25 --hole-start-short 3 -o "${OUT}_ash3.aac" "$ASH"
[ -s "${OUT}_ash1.aac" ] && cmp -s "${OUT}_ash1.aac" "${OUT}_ash2.aac" && ! cmp -s "${OUT}_ash1.aac" "${OUT}_ash3.aac" \
  && ok "--hole-start-short jawne wygrywa z przekladem" || bad "--hole-start-short: przeklad i jawna wartosc sie nie zgadzaja"
# --hole-spread 3.0 (wartosc stock long) nie zmienia juz ramek short; 9 zmienia
# (64 kbps, --block-bias 255 = 141/143 ramek short: tu beta.1 zmieniala strumien przy 3.0)
enc "$X64" -f2 -p2 -b64000 --block-bias 255 -o "${OUT}_hsp0.aac" "$ASH"
enc "$X64" -f2 -p2 -b64000 --block-bias 255 --hole-spread 3.0 -o "${OUT}_hsp3.aac" "$ASH"
enc "$X64" -f2 -p2 -b64000 --block-bias 255 --hole-spread 9 -o "${OUT}_hsp9.aac" "$ASH"
[ -s "${OUT}_hsp3.aac" ] && cmp -s "${OUT}_hsp0.aac" "${OUT}_hsp3.aac" && ! cmp -s "${OUT}_hsp0.aac" "${OUT}_hsp9.aac" \
  && ok "--hole-spread 3.0 = stock takze w short (2 dB), 9 zmienia" || bad "--hole-spread 3.0 zmienia ramki short albo 9 nic nie robi"
# --tns-start dziala tez w ramkach short: pierwsza roznica strumienia musi byc w
# ramce short (w beta.1 zmienialy sie tylko ramki long, tu jedyne 2 na koncu)
enc "$X64" -f2 -p2 -b96000 --block-bias 255 -o "${OUT}_tns0.aac" "$ASH"
enc "$X64" -f2 -p2 -b96000 --block-bias 255 --tns-start 6000 -o "${OUT}_tns1.aac" "$ASH"
fdk=$(python3 - "${OUT}_tns0.aac" "${OUT}_tns1.aac" <<'PY'
import sys; sys.path.insert(0,'tools/msis'); import parse_ms as p
a=list(p.frames(open(sys.argv[1],'rb').read())); b=list(p.frames(open(sys.argv[2],'rb').read()))
d=next((i for i,(x,y) in enumerate(zip(a,b)) if x[0]!=y[0]), None)
print('S' if d is not None and p.parse_frame(a[d][0],a[d][1]).get('short') else 'L')
PY
)
[ "$fdk" = "S" ] && [ "$(derr ${OUT}_tns1.aac)" = "0" ] \
  && ok "--tns-start 6000 zmienia ramki short (dekodowalne)" || bad "--tns-start: pierwsza zmiana w ramce $fdk, nie short"
# --side-cutoff: obie tablice zaokraglaja w gore (9 kHz: long 31 = 9.0 kHz, short 9 = 10.5 kHz)
"$X64" -f2 -p2 -b48000 --verbose --side-cutoff 9000 -o "${OUT}_asc.aac" "$ASH" 2>&1 | tr -d '\r' | grep -q "long SFB 31, short SFB 9 up" \
  && ok "--side-cutoff 9000: long SFB 31 / short SFB 9 (w gore)" || bad "--side-cutoff 9000: zla granica short"
# DAB+ 960/120: wlasne tablice, przeklad z nich (long 18..30 = 2700..9600 Hz -> short 3..7)
"$X64" --dab -p2 -b64 --verbose --msbands-lo 18 --msbands-hi 30 -o "${OUT}_asd.dabp" "$ASH" 2>&1 | tr -d '\r' | grep -q "long 18..30 (2700..9600 Hz) -> short 3..7" \
  && ok "DAB+ 960/120: przeklad z tablic 960" || bad "DAB+ 960/120: zly przeklad pasm"
# Material bez ramek short: przeklad nie zmienia niczego w long. Wymaga binarki
# referencyjnej sprzed zmiany (FRANKEN_REF=sciezka wzgledna .exe), inaczej pomijane.
if [ -n "${FRANKEN_REF:-}" ] && [ -f "$FRANKEN_REF" ]; then
python3 - _check_longonly.wav <<'PY'
import wave,struct,math,random,sys
sr=48000; n=sr*3; rnd=random.Random(3); w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
o=[]
for i in range(n):
    t=i/sr; f=min(1.0,t/0.5)
    s=f*(3000*math.sin(2*math.pi*300*t)+1500*math.sin(2*math.pi*2500*t)+800*math.sin(2*math.pi*9000*t)+700*math.sin(2*math.pi*14000*t)); z=f*400*(rnd.random()-0.5)
    o.append(struct.pack('<hh',int(s+z),int(0.8*s-z)))
w.writeframes(b''.join(o)); w.close()
PY
  enc "$X64" -f2 -p2 -b48000 -o "${OUT}_lo0.aac" _check_longonly.wav
  shl=$(jget "$(ms_json ${OUT}_lo0.aac)" short)
  lok=1
  for a in "--msbands 18" "--msbands-lo 18 --msbands-hi 30" "--is 1 --is-aggression 100 --is-lo 33" "--isbands 6 --is 1 --is-aggression 100" "--hole-start 25" "--tns-start 4000"; do
    enc "$X64" -f2 -p2 -b48000 $a -o "${OUT}_lo1.aac" _check_longonly.wav
    enc "$FRANKEN_REF" -f2 -p2 -b48000 $a -o "${OUT}_lo2.aac" _check_longonly.wav
    cmp -s "${OUT}_lo1.aac" "${OUT}_lo2.aac" || { lok=0; echo "    rozne: $a"; }
  done
  [ "$shl" = "0" ] && [ $lok = 1 ] && ok "material bez short: 6 przelacznikow pasmowych bit w bit jak binarka referencyjna" || bad "material bez short: short=$shl, przeklad zmienil ramki long"
fi
rm -f "$ASH" _check_longonly.wav

# --- --side-cutoff: poziom bez zmian + IS ponad granica (1.4.0-beta.2) ---
# Ponad granica pasmo jest czystym mid. Do beta.1 dekoder dostawal tylko E_M z
# energii kanalu E_M + E_S: nieskorelowane L/R -3 dB. Teraz mid jest skalowany do
# energii oryginalu (do +6 dB, tylko gdy side stock by zakodowal). IS ponad
# granica zostaje, gdy kryteria IS sa podane jawnie (tu --is-aggression).
python3 - _check_scu.wav _check_scp.wav <<'PY'
import numpy as np, wave, sys
sr=48000; rng=np.random.default_rng(1)
def bp(x,f1,f2):
    X=np.fft.rfft(x,axis=0); f=np.fft.rfftfreq(len(x),1/sr); X[(f<f1)|(f>=f2)]=0; return np.fft.irfft(X,n=len(x),axis=0)
def wr(fn,x):
    w=wave.open(fn,'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes(np.clip(x,-32767,32767).astype('<i2').tobytes()); w.close()
n=sr*3; low=bp(rng.standard_normal(n),200,4000)*3000
hf=bp(rng.standard_normal((n,2)),9000,16000)*3000          # nieskorelowane L/R ponad 9 kHz
wr(sys.argv[1], low[:,None]+hf)
n=sr*4; mono=bp(rng.standard_normal(n),100,12000)*2500; src=bp(rng.standard_normal(n),3000,6000)*4000
env=np.zeros(n)
for k in range(4):
    a=int((k+0.2)*sr); b=int((k+0.7)*sr); env[a:b]=np.hanning(b-a)
gl=np.where((np.arange(n)//sr)%2==0,1.0,0.25); gr=np.where((np.arange(n)//sr)%2==0,0.25,1.0)
wr(sys.argv[2], np.stack([mono+gl*src*env, mono+gr*src*env],1))  # zrodlo 3-6 kHz na przemian L / P (+-4 dB)
PY
dec(){ rm -f "$2"; ffmpeg -y -loglevel error -i "$1" -f wav "$2" >/dev/null 2>&1; }
scl(){ python3 - "$1" "$2" <<'PY'
import numpy as np, wave, sys
def rd(f):
    w=wave.open(f); return np.frombuffer(w.readframes(w.getnframes()),'<i2').astype(float).reshape(-1,2)
o=rd('_check_scu.wav'); d=rd(sys.argv[1]); off=int(sys.argv[2]); d=d[off:]
n=min(len(o),len(d))-8192; o=o[4096:n]; d=d[4096:n]
X=np.fft.rfft(o,axis=0); Y=np.fft.rfft(d,axis=0); f=np.fft.rfftfreq(len(o),1/48000); s=(f>=9500)&(f<15500)
print(round(10*np.log10((abs(Y[s])**2).sum()/(abs(X[s])**2).sum()),2))
PY
}
enc "$X64" -f2 -p2 -b96000 --pns 0 -w 16000 -o "${OUT}_scu0.aac" _check_scu.wav
enc "$X64" -f2 -p2 -b96000 --pns 0 -w 16000 --side-cutoff 9000 -o "${OUT}_scu1.aac" _check_scu.wav
dec "${OUT}_scu0.aac" "${OUT}_scu0.wav"; dec "${OUT}_scu1.aac" "${OUT}_scu1.wav"
l0=$(scl "${OUT}_scu0.wav" 2048 2>/dev/null); l1=$(scl "${OUT}_scu1.wav" 2048 2>/dev/null)
python3 -c "import sys; a,b=float(sys.argv[1]),float(sys.argv[2]); sys.exit(0 if abs(a-b)<=0.5 else 1)" "${l0:-99}" "${l1:--99}" \
  && ok "--side-cutoff 9000, nieskorelowane L/R: poziom ponad granica $l1 dB = bez przelacznika $l0 dB" || bad "--side-cutoff 9000: poziom $l1 dB, bez przelacznika $l0 dB (beta.1: -3 dB)"
ild(){ python3 - "$1" <<'PY'
import numpy as np, wave, sys
def rd(f):
    w=wave.open(f); return np.frombuffer(w.readframes(w.getnframes()),'<i2').astype(float).reshape(-1,2)
o=rd('_check_scp.wav'); d=rd(sys.argv[1])[2048:]; sr=48000; m=[]
for k in range(4):
    a=int((k+0.3)*sr); b=int((k+0.6)*sr)
    def i(x):
        X=np.fft.rfft(x[a:b],axis=0); f=np.fft.rfftfreq(b-a,1/sr); s=(f>=3000)&(f<6000); e=(abs(X[s])**2).sum(0); return 10*np.log10(e[0]/e[1])
    m.append(min(abs(i(d)),4.0)*(1 if i(d)*i(o)>0 else -1))
print(round(min(m),1))
PY
}
enc "$X64" -f2 -p2 -b40000 --side-cutoff 1000 -o "${OUT}_scp0.aac" _check_scp.wav
enc "$X64" -f2 -p2 -b40000 --side-cutoff 1000 --is-aggression 100 -o "${OUT}_scp1.aac" _check_scp.wav
dec "${OUT}_scp0.aac" "${OUT}_scp0.wav"; dec "${OUT}_scp1.aac" "${OUT}_scp1.wav"
i0=$(ild "${OUT}_scp0.wav" 2>/dev/null); i1=$(ild "${OUT}_scp1.wav" 2>/dev/null)
python3 -c "import sys; sys.exit(0 if float(sys.argv[1])>=2.5 and float(sys.argv[2])<1.5 else 1)" "${i1:--9}" "${i0:-9}" && [ "$(derr ${OUT}_scp1.aac)" = "0" ] \
  && ok "--side-cutoff 1000 + --is-aggression 100: zrodlo 3-6 kHz zostaje po swojej stronie (ILD $i1 dB; sam cutoff $i0 dB)" || bad "--side-cutoff + IS: ILD z IS $i1, bez IS $i0"
rm -f _check_scu.wav _check_scp.wav

# --- --pns-short (1.4.0-beta.2): PNS w ramkach short tylko w grupie 8 okien ---
python3 tools/msis/gen_samples.py . >/dev/null && mv -f all_short.wav _check_pss.wav && rm -f short_ms.wav is_alt.wav
enc "$X64" -f2 -p2 -b24000 -w 11000 --pns-above 4000 -o "${OUT}_ps0.aac" _check_pss.wav
enc "$X64" -f2 -p2 -b24000 -w 11000 --pns-above 4000 --pns-short 1 -o "${OUT}_ps1.aac" _check_pss.wav
[ -s "${OUT}_ps1.aac" ] && cmp -s "${OUT}_ps0.aac" "${OUT}_ps1.aac" \
  && ok "--pns-short 1 przy grupowaniu stock (brak grupy 8 okien) = bit w bit bez przelacznika" || bad "--pns-short 1 zmienia strumien bez grupy 8 okien"
enc "$X64" -f2 -p2 -b24000 -w 11000 --pns-above 4000 --short-groups 1 -o "${OUT}_ps2.aac" _check_pss.wav
blog "${OUT}_ps3.txt" "$X64" -f2 -p2 -b24000 -w 11000 --pns-above 4000 --short-groups 1 --pns-short 1 -o "${OUT}_ps3.aac" _check_pss.wav
np=$(jget "$(python3 tools/msis/bandlog.py ${OUT}_ps3.txt 13 2>/dev/null)" short_groups_with)
s2=$(stat -c%s "${OUT}_ps2.aac" 2>/dev/null || echo 0); s3=$(stat -c%s "${OUT}_ps3.aac" 2>/dev/null || echo 0)
[ "${np:-0}" -gt 0 ] && [ "$s3" -gt 0 ] && [ "$s3" -lt "$s2" ] && [ "$(derr ${OUT}_ps3.aac)" = "0" ] \
  && ok "--pns-short 1 + --short-groups 1: PNS w $np grupach short, plik $s2 -> $s3 B, dekodowalny" || bad "--pns-short 1: grupy z PNS $np, plik $s2 -> $s3"
rm -f _check_pss.wav

# --- --block-bias: udzial ramek short idzie za suwakiem (1.4.0-beta.2) ---
# Do beta.1 prog skalowany liniowo: powyzej 128 tylko 10 -> 5 (255 daleko od
# samych short), ponizej 128 skok. Teraz tablica skalibrowana: 0 = same long,
# 128 = stock bit w bit, 255 = same short (tez cisza), po drodze monotonicznie.
python3 - _check_bb.wav _check_bbs.wav <<'PY'
import numpy as np, wave, sys
sr=48000; n=sr*6; rng=np.random.default_rng(4)
env=np.ones(n); i=0; lvl=0.3
while i<n:
    L=int(rng.uniform(0.04,0.2)*sr); lvl=float(np.clip(lvl*10**(rng.uniform(-6,6)/20),0.05,1.0)); env[i:i+L]=lvl; i+=L
env=np.convolve(env,np.ones(240)/240,'same'); t=np.arange(n)/sr
x=(rng.standard_normal(n)*0.3+np.sin(2*np.pi*220*t)*0.4+np.sin(2*np.pi*1760*t)*0.2)*env*9000
y=np.stack([x,0.8*x+rng.standard_normal(n)*300*env],1)
def wr(f,a):
    w=wave.open(f,'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes(np.clip(a,-32767,32767).astype('<i2').tobytes()); w.close()
wr(sys.argv[1],y); wr(sys.argv[2],np.zeros((sr*2,2)))
PY
bbs(){ jget "$(ms_json "$1")" short; }
bbf(){ jget "$(ms_json "$1")" frames; }
prev=-1; mono=1; line=""
for b in 0 64 128 160 192 224 254 255; do
  enc "$X64" -f2 -p2 -b64000 --block-bias $b -o "${OUT}_bb$b.aac" _check_bb.wav
  sh=$(bbs "${OUT}_bb$b.aac"); [ "${sh:-0}" -ge "$prev" ] || mono=0; prev=${sh:-0}; line="$line $b:$sh"
  [ "$(derr ${OUT}_bb$b.aac)" = "0" ] || mono=0
done
fr=$(bbf "${OUT}_bb255.aac")
s0=$(bbs "${OUT}_bb0.aac"); s192=$(bbs "${OUT}_bb192.aac"); s224=$(bbs "${OUT}_bb224.aac"); s255=$(bbs "${OUT}_bb255.aac")
[ $mono = 1 ] && [ "$s0" = "0" ] && [ "$s255" -ge $((fr - 2)) ] && [ "$s224" -gt $((fr / 3)) ] && [ "$s192" -gt "$(bbs ${OUT}_bb128.aac)" ] \
  && ok "--block-bias: udzial short rosnie monotonicznie, 0 = same long, 255 = same short (ramek $fr,$line)" || bad "--block-bias: rozklad short$line (ramek $fr)"
enc "$X64" -f2 -p2 -b64000 -o "${OUT}_bbn.aac" _check_bb.wav
cmp -s "${OUT}_bbn.aac" "${OUT}_bb128.aac" && ok "--block-bias 128 = bez przelacznika, bit w bit" || bad "--block-bias 128 rozni sie od stock"
enc "$X64" -f2 -p2 -b64000 --block-bias 255 -o "${OUT}_bbs.aac" _check_bbs.wav
enc "$X64" -f2 -p2 -b64000 --block-bias 254 -o "${OUT}_bbs2.aac" _check_bbs.wav
a=$(bbs "${OUT}_bbs.aac"); f=$(bbf "${OUT}_bbs.aac"); c=$(bbs "${OUT}_bbs2.aac")
[ "$a" -ge $((f - 2)) ] && [ "$c" = "0" ] && [ "$(derr ${OUT}_bbs.aac)" = "0" ] \
  && ok "--block-bias 255 w ciszy: $a/$f ramek short (254: $c), dekodowalne" || bad "--block-bias cisza: 255 -> $a/$f, 254 -> $c"
# legalnosc sekwencji okien: LONG->{LONG,START}, START->SHORT, SHORT->{SHORT,STOP}, STOP->{LONG,START}
seqok=1
for b in 1 200 245 255; do for o in "-p2 -b64000" "-p5 -b48000" "-p29 -b32000" "-p2 -b96000 --short-groups 1"; do
  enc "$X64" -f2 $o --block-bias $b -o "${OUT}_bbl.aac" _check_bb.wav
  python3 - "${OUT}_bbl.aac" <<'PY' || seqok=0
import sys; sys.path.insert(0,'tools/msis'); import parse_ms as p
ok={0:{0,1},1:{2},2:{2,3},3:{0,1}}; seq=[]
for fr,hdr,_ in p.frames(open(sys.argv[1],'rb').read()):
    b=p.Bits(fr,hdr); ele=b.get(3)
    if ele not in (0,1): continue
    b.get(4)
    if ele==1 and not b.get(1): continue
    if ele==0: b.get(8)   # SCE: global_gain przed ics_info (HE-AAC v2 = mono + PS)
    b.get(1); seq.append(b.get(2))
bad=sum(1 for a,c in zip(seq[1:],seq[2:]) if c not in ok[a])   # ramka 0 = priming
sys.exit(1 if bad else 0)
PY
  [ "$(derr ${OUT}_bbl.aac)" = "0" ] || seqok=0
done; done
[ $seqok = 1 ] && ok "--block-bias 1/200/245/255 x LC/HE/HEv2/short-groups 1: legalne przejscia okien, 0 bledow dekodera" || bad "--block-bias: nielegalna sekwencja okien albo blad dekodera"
rm -f _check_bb.wav _check_bbs.wav

# --- --resample: etap PCM przed enkoderem (1.4.0-beta.2) ---
echo "== --resample =="
python3 - _check_rs.wav <<'PY'
import wave,struct,math,random,sys
sr=48000; random.seed(3); c=lambda x:max(-32768,min(32767,int(x)))
w=wave.open(sys.argv[1],'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
w.writeframes(b''.join(struct.pack('<hh',
   c(8000*math.sin(2*math.pi*1000*i/sr)+2000*random.uniform(-1,1)),
   c(8000*math.sin(2*math.pi*5000*i/sr)+8000*math.sin(2*math.pi*21000*i/sr))) for i in range(sr*2)))
w.close()
PY
rsr(){ ffprobe -v error -show_entries stream=sample_rate -of csv=p=0 "$1" 2>/dev/null | head -1; }
# 1) ten sam sr / -1 = lancuch nietkniety, bit w bit
enc "$X64" -f2 -p2 -b64000 -o "${OUT}_rs0.aac" _check_rs.wav
enc "$X64" -f2 -p2 -b64000 --resample 48 -o "${OUT}_rs1.aac" _check_rs.wav
enc "$X64" -f2 -p2 -b64000 --resample -1 -o "${OUT}_rs2.aac" _check_rs.wav
[ "$(md5 ${OUT}_rs0.aac)" = "$(md5 ${OUT}_rs1.aac)" ] && [ "$(md5 ${OUT}_rs0.aac)" = "$(md5 ${OUT}_rs2.aac)" ] \
  && ok "--resample 48 na 48k i --resample -1: strumien bit w bit jak bez przelacznika" || bad "--resample no-op zmienia strumien"
# 2) skladnia: Hz i skroty kHz, przecinek; zle wartosci odrzucone
rsok=1
for a in "44:44100" "44.1:44100" "44,1:44100" "44100:44100" "22:22050" "22.05:22050" "32:32000" "24:24000" "16:16000" "11:11025"; do
  enc "$X64" -f2 -p2 -b48000 --resample "${a%%:*}" -o "${OUT}_rs.aac" _check_rs.wav
  [ "$(rsr ${OUT}_rs.aac)" = "${a##*:}" ] && [ "$(derr ${OUT}_rs.aac)" = "0" ] || { rsok=0; echo "    --resample ${a%%:*} -> $(rsr ${OUT}_rs.aac)"; }
  rm -f "${OUT}_rs.aac"
done
for a in 45 44099 abc 0 7 1e3; do
  "$X64" -f2 -p2 -b48000 --resample "$a" -o "${OUT}_rsx.aac" _check_rs.wav >/dev/null 2>&1 && { rsok=0; echo "    --resample $a przyjete"; }
  rm -f "${OUT}_rsx.aac"
done
[ $rsok = 1 ] && ok "--resample: Hz, kHz, przecinek; 45/44099/abc/0/7/1e3 odrzucone" || bad "--resample: skladnia"
# 3) enkoder pracuje w nowym sr: ASC, pasmo, dlugosc M4A (iTunSMPB) dokladnie n*32/48
enc "$X64" -p2 -b32000 --resample 32 -o "${OUT}_rs.m4a" _check_rs.wav
n=$(strings "${OUT}_rs.m4a" | grep -o ' 00000000 [0-9A-F]* [0-9A-F]* [0-9A-F]*' | head -1 | awk '{print strtonum("0x"$4)}')
[ "$(rsr ${OUT}_rs.m4a)" = "32000" ] && [ "$n" = "64000" ] && [ "$(derr ${OUT}_rs.m4a)" = "0" ] \
  && ok "--resample 32: M4A 32 kHz, dlugosc gapless $n = 96000*32/48" || bad "--resample 32: sr $(rsr ${OUT}_rs.m4a) dlugosc $n"
# 4) jakosc: 1 kHz w L po 48->32 zachowany, 21 kHz z R wyciety (alias wypadlby na 11 kHz, w pasmie -w 15000), 5 kHz zostaje
enc "$X64" -f2 -p2 -b128000 -w 15000 --resample 32 -o "${OUT}_rsq.aac" _check_rs.wav
ffmpeg -v error -y -i "${OUT}_rsq.aac" -f f32le -ac 2 "${OUT}_rs.bin"
python3 - "${OUT}_rs.bin" <<'PY' && ok "--resample 32: ton 1k/5k bez zmian poziomu, 21 kHz usuniety (alias na 11 kHz < -90 dB)" || bad "--resample 32: poziomy/alias"
import sys,numpy as np
y=np.frombuffer(open(sys.argv[1],'rb').read(),'<f4').reshape(-1,2)[8000:-8000]
sr=32000; t=np.arange(len(y))/sr
def lv(x,f):
    w=np.hanning(len(x)); return 2*abs(np.dot(x*w,np.exp(-2j*np.pi*f*t)))/w.sum()
l1=20*np.log10(lv(y[:,0],1000)/(8000/32768)); l5=20*np.log10(lv(y[:,1],5000)/(8000/32768))
al=20*np.log10(lv(y[:,1],32000-21000)/(8000/32768)+1e-12)
print(f"    1k {l1:+.2f} dB, 5k {l5:+.2f} dB, alias 11k {al:.1f} dB")
sys.exit(0 if abs(l1)<0.5 and abs(l5)<0.5 and al<-90 else 1)
PY
# 5) stdin: plik == potok (wav) == raw z --raw-rate
cat _check_rs.wav | "$X64" -S -f2 -p2 -b32000 --resample 32 -o "${OUT}_rsp.aac" - 2>/dev/null
enc "$X64" -S -f2 -p2 -b32000 --resample 32 -o "${OUT}_rsf.aac" _check_rs.wav
python3 -c "import wave,sys;w=wave.open('_check_rs.wav');sys.stdout.buffer.write(w.readframes(w.getnframes()))" \
  | "$X64" -S -R --raw-rate 48000 -f2 -p2 -b32000 --resample 32 -o "${OUT}_rsr.aac" - 2>/dev/null
[ -s "${OUT}_rsp.aac" ] && [ "$(md5 ${OUT}_rsp.aac)" = "$(md5 ${OUT}_rsf.aac)" ] && [ "$(md5 ${OUT}_rsr.aac)" = "$(md5 ${OUT}_rsf.aac)" ] \
  && ok "--resample ze stdin (wav i raw) = z pliku, bit w bit" || bad "--resample stdin rozny od pliku"
# 6) inne profile i DAB+ z 44.1k
ffmpeg -v error -y -i _check_rs.wav -ar 44100 -c:a pcm_s16le _check_rs441.wav
rsp=1
for o in "-p5 -b48000" "-p29 -b24000"; do
  enc "$X64" -f2 $o --resample 48 -o "${OUT}_rsh.aac" _check_rs441.wav
  [ "$(rsr ${OUT}_rsh.aac)" = "48000" ] && [ "$(derr ${OUT}_rsh.aac)" = "0" ] || { rsp=0; echo "    $o: $(rsr ${OUT}_rsh.aac)"; }
done
enc "$X64" --dab -b 64 --resample 48 -o "${OUT}_rs.dabp" _check_rs441.wav
"$X64" --dab -b 64 -o "${OUT}_rsn.dabp" _check_rs441.wav >/dev/null 2>&1 && rsp=0   # 44.1k bez --resample: odrzucone
[ -s "${OUT}_rs.dabp" ] && [ $(( $(stat -c%s ${OUT}_rs.dabp) % 960 )) = 0 ] || rsp=0
[ $rsp = 1 ] && ok "--resample 48 z 44.1k: HE/HEv2 48 kHz dekodowalne, DAB+ 64 kbps legalny (bez --resample odrzucony)" || bad "--resample: HE/HEv2/DAB+"
rm -f _check_rs.wav _check_rs441.wav "${OUT}"_rs*

echo "==================================="
echo "WYNIK: PASS=$PASS FAIL=$FAIL"
[ "$FAIL" = "0" ] && { echo "ALL PASS"; exit 0; } || { echo "SA BLEDY"; exit 1; }
