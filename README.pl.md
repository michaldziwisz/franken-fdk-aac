# Franken FDK AAC — laboratoryjny/"geekowski" enkoder AAC (FDK)

Zbudowany na bazie **libfdk-aac 2.0.3** (mstorsjo/fdk-aac) + frontendu
**nu774/fdkaac 1.0.2**, z doklejonymi przełącznikami CLI, które odslaniaja
normalnie zahardkodowane, wewnętrzne decyzję enkodera FDK. Sluzy do
ekstremalnego debugowania i eksperymentów z AAC/HE-AAC/HE-AAC v2.

Binarki (statyczne, bez zewnętrznych DLL):
- `fdkaac-franken-x64.exe` — Windows 64-bit (PE32+)
- `fdkaac-franken-x86.exe` — Windows 32-bit (PE32)

Wszystkie oryginalne opcje frontendu nu774 (`-p/--profile`, `-b/--bitrate`,
`-m/--bitrate-mode`, `-w/--bandwidth`, `-a/--afterburner`, `-s/--sbr-ratio`,
`-f/--transport-format`, tagowanie itd.) działają jak wczesniej. Poniżej tylko
NOWE przełączniki. Zobacz tez `fdkaac-franken-x64.exe --help`.

UWAGA: to jest narzędzie "wiem-co-robię". Większość tych parametrów celowo
pozwala wyjść poza to, co robi automatyka FDK — można nimi świadomie zepsuć
obraz stereo, pasmo albo jakość. Sentinel `-1` (lub `0` dla `--core-cutoff`)
= "zostaw domyślne zachowanie FDK".

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski.
Zbudowane na oprogramowaniu open source: libfdk-aac (Fraunhofer IIS) oraz
frontend nu774/fdkaac.

---

## Spis grup opcji (tak samo pogrupowane w `--help`)

Opcje są uporządkowane tematycznie, z grubsza od najlatwiejszych do najbardziej
geekowskich. Ta sama kolejność obowiazuje w `--help` (grupy A–E) i w sekcjach
poniżej:

- **A. Zacznij tutaj (konsumenckie):** `--verbose`, `--is-aggression`, `--speech`,
  `--uncap-bandwidth`, `--unlock-bitrate` — sekcje 0, 1, 2, 10.
- **B. Stereo:** MS (`--msmask`, `--msbands`, `--msbands-lo/-hi`, `--ms-bias`,
  `--ms-precision`), IS (`--is`, `--isbands`, `--is-*`), PS (`--ps`, `--ps-iid-quant`,
  `--ps-icc`, `--ps-icc-mode`) — sekcje 1, 4, 8.
- **C. Pasmo i SBR:** `--core-cutoff`, `--sbr-*` — sekcje 2, 3.
- **D. Maskowanie / szum / detal:** `--ath-scale`, `--spread-mask`, `--tns-*`,
  `--pns`, `--pns-start`, `--force-pns` — sekcje 5, 6.
- **E. Bloki i bitrate:** `--block-bias`, `--vbr-reservoir`, `--peak-bitrate`,
  `--max-bits-frame`, `--min-bits-frame`, `--bitres-mode` — sekcje 7, 9.

Wskazowka: `--verbose` na koncu wypisuje sekcje "franken overrides applied" —
dokładnie te przełączniki, które w danym uruchomieniu odbiegają od czystego FDK.

---

## 0. Diagnostyka

### `--verbose`
Przed enkodowaniem wypluwa na stderr REALNE, wybrane przez enkoder parametry
(nie tylko Twoje nadpisania): AOT, bitrate/tryb, samplerate, channel-mode,
EFEKTYWNY cutoff rdzenia w Hz, afterburner, transport, signaling, oraz stan
narzędzi kodujacych wybrany przez enkoder (TNS on/off, PNS on/off, Intensity
stereo on/off, MS stereo on/off). Gdy SBR aktywny: sbr-ratio + efektywne
start/stop freq index, freq scale, noise bands, amp res. Na koncu lista Twoich
override'ow (-1/0 = nie ustawione, zostawione enkoderowi). Idealne, by poznać
punkt wyjścia (np. domyślny cutoff HE-AAC v2 48k = 8613 Hz).

---

## 1. Joint stereo — MS / IS / niezależne stereo

FDK domyślnie sam decyduje per-pasmo o MS (mid/side) i IS (intensity). Tu można
te decyzję nadpisac i wymusic ekstremalne konfiguracje.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--msmask <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś MS: `0` = wszystkie pasma L/R (kompletnie niezależne stereo), `1` = MS na wszystkich pasmach. |
| `--msbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalny numer SFB, który może użyć MS (bierze N NAJNIŻSZYCH pasm). Powyżej — MS wyłączone. |
| `--msbands-lo <n>` | -1 off, 0..N | -1 | POCZATEK (najniższy numer pasma SFB) zakresu, w którym dozwolone jest MS. |
| `--msbands-hi <n>` | -1 off, 0..N | -1 | KONIEC (najwyższy numer pasma SFB) zakresu MS. Używane razem z `--msbands-lo` jako para OD–DO. Pasma poza tym zakresem ida czystym L/R. |
| `--ms-precision <n>` | 256..bez limitu (Q8) | -1 (off) | Precyzja pasm MS. `>256` = płytsze dziury w spektrum MS (koder alokuje więcej bitow na ich opis, styl LAME -q). 256=bez zmian, 384~1.5x, 512~2x. Użyteczny efekt SLYSZALNY do ~600-800; wyżej prog uderza w twarda podłogę FDK (ld64 -0.515625) i przy CBR bity są tylko PRZESUWANE między pasmami (stały budzet), więc brzmienie przestaje sie zmieniac. Działa najlepiej gdy masz zapas bitow (>=128k) lub w VBR. |
| `--is <n>` | -1 auto, 0 off, 1 on | -1 | Intensity stereo globalnie wł/wył. |
| `--isbands <n>` | -1 brak limitu, 0..N | -1 | Maksymalna liczba SFB, które mogą użyć intensity. Powyżej — kodowane normalnie. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | KONSUMENCKI suwak: jak bardzo enkoder ma isc w intensity stereo. Zaczynaj tu, zaawansowane `--is-*` zostaw. |
| `--is-min-sfbs <n>` | -1 def(6), 0..N | -1 | (zaawansowane) Min. liczba ciaglych SFB, zanim IS sie włączy. |
| `--is-corr-thresh <n>` | -1 def(243), Q8 | -1 | (zaawansowane) Prog korelacji L/R dla IS w Q8 (256=1.0). |
| `--is-lr-ratio <n>` | -1 def(179), Q8 | -1 | (zaawansowane) Prog balansu energii L/R dla IS w Q8 (256=1.0). |

### Intensity stereo w praktyce (jak tego używać, nie wzory)

Co to jest: intensity stereo (IS) w górnych pasmach porzuca osobne L/R i wysyla
JEDNA obwiednię energii + informację o kierunku (panoramie). Ucho słabo lokalizuje
wysokie tony, więc to oszczędza sporo bitow — ale za cena separacji stereo (szerokość
sceny w górze pasma sie zwęża). Płaci sie za to zwłaszcza na materiale z realna
różnica L/R w wysokich (blachy z jednej strony, efekty przestrzenne).

FDK domyślnie jest bardzo OSTROŻNY z IS (stąd Twoja obserwacja "ledwo słychać
różnice"). Powody są trzy i po to są te pokretla:

1. Bramka wpuszczenia IS: FDK w ogole rozważa IS tylko gdy `bitrate/pasmo < 5`.
   Przy wyższych bitrate'ach IS jest w ogole niedopuszczone. `--is-aggression >=1`
   zdejmuje te bramkę.
2. Prog korelacji (`--is-corr-thresh`, Q8, 256=1.0, default 243 ~= 0.95): oba
   kanały muszą byc do siebie podobne w danym pasmie w co najmniej ~95%, żeby IS
   sie włączył. To bardzo wysoko. Obniżasz -> IS lapie czesciej, nawet gdy kanały
   mniej podobne. Np. 180 (~0.70) = dużo agresywniej. Za nisko = słyszalne
   przekłamania kierunku.
3. Min. długość regionu (`--is-min-sfbs`, default 6): IS włącza sie dopiero na
   pasmie co najmniej 6 kolejnych SFB. Obniżasz do 1-2 -> IS lapie tez krótkie
   fragmenty.

Zależność między nimi: żeby dany SFB poszedł w IS, MUSZA byc spełnione WSZYSTKIE
naraz — bramka wpuszczenia ORAZ korelacja powyżej progu ORAZ region odpowiednio
długi ORAZ kierunek stabilny. Dlatego samo obniżenie jednego progu często nic nie
daje (inny nadal blokuje) — i dlatego zwykle nie widac różnicy manipulując tylko
korelacja. `--is-aggression` rusza WSZYSTKIE naraz, spójnie.

Jak ustawiac:
- Najprościej: `--is 1 --is-aggression 40` i słuchaj. Za malo IS -> podnos do 70,
  100. Za dużo (scena sie "skleja" w górze, artefakty kierunku) -> zejdź.
- 0 = domyślne FDK (praktycznie IS ledwo aktywne przy typowym bitrate).
- 100 = maksimum: bramka zdjeta, korelacja luzna (~0.475), region od 1 SFB,
  szeroka tolerancja kierunku. Duzo pasm w IS, mocno słyszalne, oszczędza bity.
- Ręczny tuning tylko gdy chcesz precyzji: ustaw `--is-aggression 0` i kreç
  `--is-corr-thresh` (glowny), potem `--is-min-sfbs`, na koncu `--is-lr-ratio`.
  Wartości --is-* NADPISUJA to co ustawil suwak agresywności.
- Diagnoza: `--verbose` pokazuje efektywne progi (IS corr threshold Q8, min SFBs),
  więc widzisz co realnie poszło do enkodera.

Bias MS/IS (punkt 2): powyższe `--is-*` sterują tym KIEDY enkoder wybiera
intensity stereo (progi decyzji z tabeli strojenia FDK), niezależnie od twardego
wł/wył. `--msbands` ogranicza MS do dolnych pasm (poprawnie — maska i motylek
L/R->M/S są zsynchronizowane, brak artefaktu "lewy=center, prawy=reszta").
- Kompletnie niezależne stereo: `--msmask 0 --is 0`.
- "Laboratoryjne" ograniczenie MS do dolnych pasm: np. `--msbands 6`.
- Wymuszony pelny MS: `--msmask 1`.
- IS chętniejsze: obniż `--is-corr-thresh` (np. 150) i/lub `--is-min-sfbs`.

### Zakres pasm MS: --msbands, --msbands-lo, --msbands-hi (WAZNE, często mylone)

Pasma widma są numerowane OD DOŁU: pasmo 0 = najniższe częstotliwości (basy),
im wyższy numer, tym wyżej w widmie. W typowym LC stereo jest ich okolo 49.

Są DWA niezależne sposoby ograniczenia, gdzie stosowane jest MS:

1. `--msbands <n>` — "dolne N pasm". MS dozwolone TYLKO w pasmach 0..(n-1),
   czyli od basu w górę do numeru n. To jest zawsze liczone OD DOŁU.
   Przykład: `--msbands 6` = MS tylko na 6 najniższych pasmach, reszta czyste L/R.

2. `--msbands-lo <lo>` + `--msbands-hi <hi>` — "zakres OD-DO". MS dozwolone TYLKO
   w pasmach o numerach od `lo` do `hi` włącznie. Poza tym zakresem czyste L/R.
   To para — podajesz oba. Pozwala umiescic MS GDZIEKOLWIEK, w tym na samej górze.

Konkretny przykład (zakladajac ~49 pasm w LC):
- Chcesz MS TYLKO na 5 najWYŻSZYCH pasmach (np. scalic szum w górze, a dół
  zostawić w pelnym niezaleznym stereo)? Najwyższe pasma to numery 44..48:
  `--msbands-lo 44 --msbands-hi 48`.
- Chcesz MS tylko w SRODKU pasma (np. 10..30)? `--msbands-lo 10 --msbands-hi 30`.
- Chcesz MS na 6 najNIŻSZYCH? Prościej `--msbands 6` (albo `--msbands-lo 0
  --msbands-hi 5` — to samo).

Zasada pamięciowa: `--msbands` = "od dołu do", `--msbands-lo/-hi` = "od..do".
Ile masz realnie pasm dla danego trybu/samplerate pokazuje `--verbose`
(pole "active SFBs").

## 2. Cutoff (odcięcie) rdzenia AAC gdy działa SBR

Standardowe `-w/--bandwidth` w FDK jest IGNOROWANE gdy aktywny jest SBR
(HE-AAC v1/v2) — bo `sbrEncoder_Init()` nadpisuje pasmo wartością z tabeli SBR.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--core-cutoff <hz>` | 0 = default, >0 = Hz | 0 | Wymusza pasmo rdzenia AAC W Hz nawet pod SBR. Odporny na nadpisanie przez SBR. |

Przykład (Twoj przypadek — 7.5 kHz rdzenia przy HE-AAC v2 48 kbps, gdzie tabela
daje mniej):
```
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav
```
Zweryfikowane: `--core-cutoff 7500` -> efektywne pasmo 7500 Hz; stock `-w 7500`
pod SBR pozostaje 8613 Hz (ignorowane).

UWAGA: sam pilnujesz limitow rdzenia. Maks. to Nyquist rdzenia (`sr/2`), a przy
**dual-rate SBR docelowy samplerate jest dzielony przez 2** — miej to na
uwadze przy doborze wartości.

## 3. Gęstość / dokładność danych SBR

Nadpisuje ustawienia z tabeli tuningowej SBR (po jej załadowaniu).

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--sbr-start <n>` | -1 def, 0..15 | -1 | Indeks `bs_start_freq` (start pasma SBR). |
| `--sbr-stop <n>` | -1 def, 0..13 | -1 | Indeks `bs_stop_freq` (koniec pasma SBR). |
| `--sbr-freqscale <n>` | -1 def, 0..3 | -1 | Grupowanie częstotliwości (0 = liniowe, wyżej = drobniejsze log). |
| `--sbr-alterscale <n>` | -1 def, 0/1 | -1 | Alternatywna rozdzielczość skali. |
| `--sbr-noise-bands <n>` | -1 def, 1..5 | -1 | Liczba pasm szumu SBR (gęstość opisu szumu). |
| `--sbr-amp-res <n>` | -1 def, 0/1 | -1 | Rozdzielczość amplitudy obwiedni: 0 = 1.5 dB, 1 = 3.0 dB. |
| `--sbr-data-extra <n>` | -1 def, 0/1 | -1 | Zapis dodatkowych danych nagłówka SBR. |
| `--sbr-num-env <1\|2\|4>` | -1 off | -1 | Liczba obwiedni na ramke. WYMUSZA statyczna siatke czasowa (ignoruje detektor transientów). Więcej = lepsza rozdzielczość czasowa górnego pasma, gorzej na atakach. (8 przekracza standardowy grid — odrzucone.) |
| `--sbr-freqres-fixfix <0\|1>` | -1 off | -1 | Rozdzielczość częstotliwości obwiedni FIXFIX (0 low, 1 high). |
| `--sbr-stereo-mode <0..3>` | -1 off | -1 | Tryb stereo SBR: 0 mono, 1 LR (pelna separacja górnego pasma), 2 coupling (oszczędny, wspolna obwiednia + poziom), 3 switch-LRC (domyślnie koder wybiera per-ramke). Wymuś 1 dla max separacji, 2 dla oszczędności. |
| `--sbr-invf <0..3>` | -1 auto | -1 | Wymuś inverse filtering SBR: 0 off, 1 low, 2 mid, 3 high. Sterowane normalnie estymatorem tonalności. Wyżej = mocniejsze "wybielanie" tonalnego SBR (mniej metaliczności kosztem detalu). |
| `--sbr-noise-floor-offset <n>` | -128 off | -128 | Offset poziomu szumu SBR (mala l. calkowita). Wieksze = więcej szumu wypełniającego w rekonstrukcji SBR. |

UWAGA: `--sbr-start`/`--sbr-stop` są walidowane PRZEZ FDK — niepoprawna
KOMBINACJA start/stop (zła liczba pasm master) da "encoder initialization
failed". To ograniczenie samego SBR, nie buga. Dobieraj pary (np. dla 64k
stereo działa start=5 stop=9, start=8 stop=14).

## 4. Parametric Stereo (HE-AAC v2)

PS opisuje stereo kilkoma parametrami (IID/ICC...). Tu można nimi sterować,
nawet kosztem obrazu stereo.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ps <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś wysylanie parametru IID PS. `0` = nigdy (spłaszcza obraz stereo), `1` = zawsze. Nadpisuje heurystykę różnicy głośności. |
| `--ps-iid-quant <n>` | -1 def, 0 coarse, 1 fine | -1 | Siatka kwantyzacji IID: gruba vs. dokładna. |
| `--ps-icc <n>` | -1 auto, 0 off, 1 on | -1 | Wymuś ICC (Interchannel Coherence — podobieństwo/spójność kanałów) on/off. |
| `--ps-icc-mode <n>` | -1 def, 0/1 | -1 | Tryb rotacji ICC: 0 = ROT_A, 1 = ROT_B. |

UWAGA o PS: FDK koduje IID (różnice głośności) i ICC (koherencja). IPD/OPD
(różnice fazy) NIE są wspierane w enkoderze FDK — kod dosłownie wpisuje zera
(`ps_encode.cpp: "IPD OPD not supported right now"`). Nie da sie ich wystawić bez
napisania od zera analizy fazy międzykanałowej.

## 5. Substytucja/ksztaltowanie szumu — TNS / PNS / afterburner

To, co w średnich bitrate'ach jest zastępowane szumem lub resyntezowane.

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--tns-mask <n>` | -1 def (0xF), 0..15 | -1 | Maska włączenia TNS (bitowa, per typ bloku). |
| `--tns-order <n>` | -1 def, 1..12 | -1 | Maks. rząd filtra TNS (bloki short dodatkowo kapowane do 5). |
| `--pns <n>` | -1 def, 0/1 | -1 | Perceptual Noise Substitution wł/wył. UWAGA: FDK wymusza PNS=off gdy aktywne SBR albo VBR. |
| `--pns-start <hz>` | -1 def, Hz | -1 | Częstotliwość startowa PNS. Niżej = więcej widma zastępowane szumem. |
| `--force-pns` | flaga | off | Obejdz bramkę niskiego bitrate dla PNS. |
| `--afterburner <n>` | 0/1 (tez stock `-a`) | 1 | Afterburner (dokładniejsza kwantyzacja). |

WAZNE o PNS przy niskim bitrate: FDK ma tabele tuningowa (`levelTable`), która
CALKOWICIE wyłącza PNS poniżej ~28 kbps (wiersz bitrate 0-27999 = same zera dla
kazdego samplerate). Dlatego przy 24 kbps `--pns`/`--pns-start` nie robia NIC (audio
brzmi "jak MP3/MDCT"), a przy 64 kbps różnica jest duza. `--force-pns` omija te
bramkę (używa pierwszego aktywnego wiersza tabeli), więc PNS działa tez przy 24k.
Ograniczenie FDK: PNS i tak wymaga włączonego TNS i trybu nie-VBR — inaczej jest
zerowane wyżej w łańcuchu (nic na to nie poradzimy bez głębszej przebudowy).

## 6. Maskowanie / ATH

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ath-scale <n>` | 1..~4096 (Q8) | 256 | Skala progu maskowania w Q8 (256 = x1.0). `>256` podnosi progi (więcej szumu, mniej bitow na pasmo), `<256` obniża (czysciej, więcej bitow). Działa w domenie ld64 FDK jako addytywny offset log2. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = mniej maskowania = więcej detalu. Największy efekt gdzie bity ograniczone (96-192k). |

### Co realnie pomaga w niskim i średnim bitrate (10-144 kbps)

Częste pytanie: czy da sie jeszcze cos wycisnac na dokładności/efektywności
kodowania (Huffman, iteracje kwantyzacji itp.)? Uczciwa odpowiedz po przejrzeniu
kodu FDK:

- Kodowanie Huffmana (łączenie sekcji, wybór codebookow w `dyn_bits.cpp`) jest juz
  optymalne (zachlanne łączenie sekcji dajace min. bitow). Nie ma tam sensownego
  pokretla — a wystawienie tego tylko by pogarszalo wynik.
- Pętla iteracji kwantyzacji (`maxIterations`) to mechanizm RATUNKOWY przy
  niedoborze bitow; zwiekszanie jej nic nie daje (szczegóły w manualu, rozdz. 9a).
- Wewnętrzne progi (adaptacja minSnr, `bits2PeFactor`) to arytmetyka staloprzecinkowa
  z twardymi zakresami — ruszanie ich grozi niestabilnością, nie poprawa.

REALNY zestaw dźwigni jakości dla 10-144 kbps jest JUZ wystawiony:
- `--ath-scale <256` — globalnie obniż prog maskowania (więcej detalu za bity).
- `--spread-mask <256` — mniej maskowania międzypasmowego (więcej pasm kodowanych).
- `--ms-precision >256` — płytsze dziury w pasmach MS.
- `--is-aggression` — steruj intensity stereo (kluczowe przy niskim bitrate).
- `--force-pns` + `--pns-start` — kontrola szumu przy bardzo niskim bitrate.
- pod HE-AAC: `--sbr-invf`, `--sbr-noise-floor-offset`, `--speech` (mowa).

To nie brak funkcji — to te same dźwignie, których używa profesjonalny tuning,
tyle ze ręcznie. Zacznij od `--ath-scale` i `--spread-mask`, po jednej naraz.

## 7. Bias przełączania blokow short/long (dowolny profil)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--block-bias <n>` | 0..255 | -1 (off) | Przesuwa prog decyzji short/long. 128 = domyślne enkodera (bez zmian), >128 faworyzuje bloki krótkie (bardziej "transient"), <128 faworyzuje długie, 0 = praktycznie tylko długie. |

WAZNE: `--block-bias` zawsze produkuje strumien zgodny ze standardem (przesuwa
prog detekcji ataku, nie wymusza na sile typu bloku). Zastąpił dawne
`--allshort`/`--alllong`, które tworzyły NIELEGALNY strumien (twarde wymuszenie
short window bez przeliczenia SFB/grupowania -> dekoder odrzucał, Winamp
"skakał jak po porysowanej płycie"). Jesli chcesz maksimum długich: `--block-bias 0`;
maksimum krótkich: `--block-bias 255`.

## 8. Bias decyzji MS (uczciwie: narzędzie o SŁABYM efekcie)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--ms-bias <n>` | 0..255 (Q8) | -1 (off) | Przesuwa prog decyzji L/R vs MS. Q8, 128 = +0.5 w jednostkach ld64 FDK. >0 = MS chętniejsze. Reaguje juz od ~32 (po rekalibracji skali). |

UCZCIWIE o `--ms-bias` — to najsłabsze narzędzie z całego zestawu i teraz wiadomo
dlaczego "niewiele robi". MS (mid/side) to transformacja BEZSTRATNA: mid=L+R,
side=L-R odtwarza sie dokładnie z powrotem na L/R. Włączenie/wyłączenie MS na
danym pasmie NIE zmienia brzmienia — zmienia tylko ILE BITOW zajmie zapis. Enkoder
i tak wybiera bliska optymalnej decyzję per pasmo; `--ms-bias` tylko przesuwa
kilka GRANICZNYCH pasm. Pomiar (korelacja L/R po dekodowaniu, rozmiar ADTS):
efekt rzędu <0.1% rozmiaru i zmian korelacji w 4. miejscu po przecinku.

Uwaga techniczna: w poprzedniej wersji skala biasu byla ~256x za słaba (mnożnik
<<15 zamiast <<23) — stąd "128 nic nie robilo, dopiero 2048 cos ruszalo". Teraz
128 = realne +0.5 ld64 jak w opisie, więc reaguje od ~32. Ale nawet poprawnie
wyskalowany bias ma z natury maly wpływ (patrz wyżej).

Chcesz REALNIE sterować MS? Użyj twardych przełączników, nie biasu:
- `--msmask 0` — WYŁĄCZ MS całkowicie (czyste, niezależne L/R). To właściwy
  wybór do center-cancel / usuwania wokalu (zero mieszania kanałów przez koder).
- `--msmask 1` — wymuś MS na wszystkich pasmach (maks. oszczędność bitow).
- `--msbands` / `--msbands-lo/-hi` — ogranicz MS do wybranych pasm.
Pomiar: msmask 0 vs 1 daje ~900 B różnicy na 2s probce; ms-bias tylko ~2 B.

## 9. Quasi-constrained VBR (silnik CBR + szersze oddychanie)

WAZNE: bez tych switchy CBR jest 100% NIEZMIENIONY (zweryfikowane: bit-identyczny
z oryginalna binarka). Włączasz je świadomie.

Jak AAC oddycha: nawet w CBR ramki pożyczają z bit-reservoir, więc jedna ramka
może miec ~122 kbps a następna ~141, byle średnia = target. Te pokretla
poszerzają/ograniczaja to oddychanie.

TWARDY SUFIT dla wszystkiego: ramka AAC miesci MAX 6144 bitow na kanał
(=768 bajtow/kanał); stereo => 12288 bitow/ramke. Przy 44100 Hz jedna ramka =
1024 probki = ~23.2 ms, więc bity/ramke = kbps * 23.22. (Np. 128k stereo:
średnia ~2972 bitow/ramke; sufit 12288.)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--vbr-reservoir <bity>` | 0..(6144*kanały - średnia) | -1 (off) | Rozmiar bit-reservoir. Więcej = większy rozrzut ramek wokol średniej. min 0 (trzymaj sie targetu ciasno). Auto-clamp do sufitu - nie przegniesz. Bezpieczny start: 2-3x default. |
| `--peak-bitrate <bps>` | > target | -1 (off) | Dopuszcza krótkie szczyty do tej wartości, trzymając średnia. Ustaw POWYŻEJ `-b` (np. -b 128000 --peak-bitrate 160000). Poniżej targetu ignorowane. |
| `--max-bits-frame <bity>` | średnia..12288(st.) | -1 (off) | Twardy sufit bitow w JEDNEJ ramce. Musi byc >= średnia i <= 6144*kanały (inaczej clamp). Rozsądny cap ~1.5x średnia (~4500 dla 128k st.). Za nisko = głodzi głośne ramki (słyszalne). |
| `--min-bits-frame <bity>` | 0..średnia | -1 (off) | Twarda podłoga bitow/ramke. Wyższa podłoga marnuje bity na ciszę. Zostaw 0 chyba ze eksperymentujesz. |
| `--bitres-mode <n>` | 0/1/2 | -1 (def) | Tryb reservoir: 0 pelny (jak default), 1 zredukowany, 2 wyłączony (sztywny, najbliżej twardego CBR per-ramka). |

JAK USTAWIAC OPTYMALNIE (dla mniej doświadczonych - żeby nie przegiąć):
- Bezpieczny quasi-CVBR ~128k stereo: `-b 128000 --peak-bitrate 160000 --vbr-reservoir 6000`.
- NIE ustawiaj `--max-bits-frame` PONIŻEJ średniej ani `--min-bits-frame` POWYŻEJ
  średniej - to walczy z targetem i psuje jakość.
- `--vbr-reservoir` jest auto-clampowany do sufitu, więc bezpiecznie eksperymentować.
- Zmierzone realnie (sygnal 4s zmienny, 128k stereo): CBR default rozrzut 95-167
  kbps; z `--vbr-reservoir 8000 --peak-bitrate 192000` rozrzut 36-158 kbps
  (mocniej oddycha, średnia trzymana); `--bitres-mode 2` rozrzut 127.8-128.2
  (sztywny). Wszystkie w pelni dekodowalne.
- Ograniczenie: to silnik CBR+reservoir, nie prawdziwy ABR jak LAME. Wahania
  umiarkowane (limit 6144 bitow/kanał), ale to jest ten "lekki lot" AAC.

## 10. Audiofilskie / ekstremalne (opt-in, poza typowym zakresem)

| Switch | Wartości | Domyślnie | Opis |
|---|---|---|---|
| `--uncap-bandwidth` | flaga | off | Zdejmij twardy cap 20 kHz rdzenia. `--core-cutoff` może wtedy sięgnąć az do Nyquista. |
| `--is-aggression <0..100>` | 0..100 | -1 (off) | Suwak agresywności IS (patrz sekcja 1). |
| `--force-pns` | flaga | off | PNS poniżej bramki ~28 kbps (patrz sekcja 5). |
| `--unlock-bitrate` | flaga | off | Zdejmij DOLNY prog bitrate. Pozwala na skrajnie niskie: 8k HE-AAC stereo, 6k LC. WAZNE: w tym trybie `-b` bierzemy DOSLOWNIE jako bps (bez konwencji nu774 x1000), więc `-b 6000` = 6000 bps. Górny sufit 6144*kanały zostaje (twardy limit AAC). Rezydualny floor ~10 kbps = minimum nagłówków AAC. |
| `--speech` | flaga | off | Tryb strojenia SBR pod MOWE ludzka (inne progi inverse filtering, poziom szumu, bez parametric coding). Dotyczy HE-AAC (SBR); LC nie ma osobnego trybu mowy. Dla czystej mowy w niskim bitrate. |
| `--spread-mask <n>` | Q8, >=0 | -1 (off) | Skaluje rozlewanie maskowania między pasmami. `<256` = MNIEJ maskowania = więcej detali (odpowiednik luzowania tone-masks-noise). Największy efekt gdzie bity ograniczone (96-192k). 256=bez zmian. Łącz z `--ath-scale <256`. |

PASMO POWYŻEJ 20 kHz (audiofilskie): FDK ma zaszyty cap `min(20000, sr/2)` na
pasmo rdzenia — nawet przy wejściu 96 kHz i wysokim bitrate realnie nic powyżej
20 kHz nie jest kodowane (Twoje podejrzenie bylo trafne). `--uncap-bandwidth` znosi
ten cap; wtedy `--core-cutoff` steruje pasmem az do sr/2.

Zmierzone (96 kHz wejście, LC 400k, szum szerokopasmowy):
- bez uncap, `--core-cutoff 40000`: verbose 20000 Hz, energia >20 kHz ~= 0%.
- `--core-cutoff 40000 --uncap-bandwidth`: verbose 40000 Hz, energia 20-24 kHz
  ~10%, 24-32 kHz ~20%, 32-44 kHz ~20% — pelne pasmo do 40 kHz realnie kodowane.

Preset audiofilski (pelne pasmo + ręczne maskowanie, dla 190+ kbps):
```
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 \
    --ath-scale 200 -o out.m4a in96k.wav
```
`--ath-scale <256` obniża progi maskowania (czysciej, więcej bitow na detale) —
sensowne gdy masz duzy zapas bitrate. Uwaga: pasmo >20 kHz i skrajne ustawienia
mogą byc odrzucone przez czesc dekoderow (poza typowa specyfikacja) — świadomy
opt-in.

---

## 11. Kontener MP4/M4A — które boxy są konieczne, a co można wyciąć

Plik .m4a to zestaw zagnieżdżonych "boxow" (atomow). Sprawdzone, co jest czym:

OBOWIAZKOWE (bez nich plik jest NIEGRYWALNY — nie ma do nich przełączników):
`ftyp`, `mdat` (surowe dane audio), oraz szkielet `moov` → `mvhd` + `trak` →
`tkhd` → `mdia` (`mdhd`/`hdlr`/`minf` → `smhd`/`dinf`/`stbl` z tabelami
`stsd`/`stts`/`stsc`/`stsz`/`stco`). To jest minimalna mapa pliku wymagana przez
standard ISO — kazdy dekoder tego potrzebuje, żeby w ogole znaleźć i odtworzyć
dźwięk. Wycinanie ich nie ma sensu (efekt: uszkodzony plik).

OPCJONALNE (można wyłączyć) — cały blok `udta` → `meta` → `ilst`, czyli:
- tag identyfikacyjny kodera (`©too`, teraz "PompAAC based on …"),
- `iTunSMPB` — dane o opóźnieniu kodera do bezszwowego (gapless) odtwarzania,
- wszystkie tagi (tytuł, artysta, album itd.).

| Switch | Działa na | Opis |
|---|---|---|
| `--no-tool-tag` | .m4a | Nie zapisuj tagu identyfikującego koder (`©too`). Reszta tagow i gapless zostaje. |
| `--minimal-moov` | .m4a | Najmniejszy legalny .m4a: pomija CALY blok `udta`/metadata (tag kodera + gapless iTunSMPB + wszystkie tagi). Szkielet odtwarzania zostaje nienaruszony. |

Ile to oszczędza (2 s, 128k stereo, pomiar): default ~34381 B →
`--no-tool-tag` ~34297 B (−84) → `--minimal-moov` ~34048 B (−333). To male
liczby — narzut MP4 to glownie obowiazkowy szkielet, którego usunąć sie nie da.
Chcesz naprawde zero narzutu kontenera? Użyj surowego ADTS: `-f 2 -o out.aac`
(strumien bez żadnych boxow, ale tez bez tagow i gapless).

UWAGA gapless: `--minimal-moov` usuwa iTunSMPB, więc przy łączeniu utworow mogą
pojawić sie mikro-przerwy (encoder delay nie bedzie zasygnalizowany). Do
zwykłego odsłuchu bez znaczenia; do płyt "bez przerw" zostaw domyślne.

---

## 12. Legenda odczytu `--verbose`

`--verbose` wypisuje SUROWE wartości (bez podpowiedzi w nawiasach, żeby nie
zaśmiecać). Poniżej co oznaczają te, które nie są oczywiste:

| Pole | Jak czytac |
|---|---|
| `AOT (profile)` | 2=AAC-LC, 5=HE-AAC, 29=HE-AAC v2, 23=AAC-LD, 39=AAC-ELD. |
| `bitrate-mode` | 0=CBR, 1..5=VBR (wyżej=lepiej). |
| `channel-mode` | 1=mono, 2=stereo (dla HE-AAC v2 rdzen jest mono, stereo robi PS). |
| `core bandwidth` | Górna częstotliwość rdzenia AAC. W nawiasie ZRODLO: `from -w`, `from --core-cutoff`, albo `auto`. Pod SBR to tylko rdzen — SBR gra wyżej. |
| `final BW (AAC+SBR)` | Pokazywane tylko gdy SBR aktywny: orientacyjna GÓRNA częstotliwość całego sygnalu (rdzen + SBR), wyliczona ze `sbr stop freq index`. To odpowiednik `core bandwidth`, ale dla pelnego pasma HE-AAC. |
| `signaling-mode` | Sposób sygnalizacji SBR/PS: 0=implicit, 1=explicit backward-compat, 2=explicit hierarchical, auto=biblioteka wybiera. |
| `SBR mode` | Wewnętrzny tryb SBR (-1/0 gdy nieuzywany). |
| `sbr-ratio` | 1=downsampled (single-rate), 2=dual-rate (rdzen na polowie częstotliwości). |
| `sbr amp res` | 0=1.5 dB, 1=3.0 dB (rozdzielczość amplitudy obwiedni). |
| `granule/frame length` | Długość ramki w probkach (1024 dla LC, 512/480 dla LD/ELD). |
| `codec delay` | Opóźnienie kodeka w probkach/kanał (total i sam rdzen). Do gapless. |
| `IS corr threshold` / `IS L/R ratio` | Progi w skali Q8: 256 = 1.0. Niższy prog korelacji = intensity stereo CHĘTNIEJSZE (odwrotnie do intuicji). |
| `IS min contiguous SFBs` | Ile sąsiadujących pasm musi sie "zgodzić", zanim włączy sie IS. |
| `TNS mask` | Maska bitowa 0x0..0xF które filtry TNS aktywne. |
| `MS/IS bands: auto up to N` | Górny indeks pasma, do którego narzędzie może działać. |
| `franken overrides applied` | Lista przełączników które w TYM uruchomieniu odbiegają od czystego FDK (albo "none"). |

Wartości Q8 (jak `IS corr threshold`, `--ms-precision`, `--ms-bias`,
`--ath-scale`, `--spread-mask`) to liczby staloprzecinkowe gdzie 256 = 1.0;
np. 243 oznacza 243/256 ≈ 0.95.

---

## 13. Tabele referencyjne (z tablic strojenia FDK)

Trzy tabele orientacyjne, żeby świadomie dobierać `--msbands`, `--sbr-start/stop`
i `-w`. Wartości wyliczone z tablic w kodzie FDK; są PRZYBLIZONE (siatka SFB jest
schodkowa), ale pokazuja właściwy rząd wielkości.

### Tabela 1 — orientacyjna górna częstotliwość pasma (SFB) [Hz]

Pasma numerowane od dołu (0=bas). Pokazano co 4. pasmo; ostatni wiersz = liczba
pasm i częstotliwość Nyquista. Użyteczne przy `--msbands`/`--isbands`/`--msbands-lo/-hi`.

| SFB | 16 kHz | 22.05 kHz | 32 kHz | 44.1 kHz | 48 kHz | 96 kHz |
|----:|-------:|----------:|-------:|---------:|-------:|-------:|
| 0   | 62   | 43   | 62   | 86    | 94    | 188   |
| 4   | 312  | 215  | 312  | 431   | 469   | 938   |
| 8   | 562  | 388  | 562  | 775   | 844   | 1688  |
| 12  | 875  | 646  | 1000 | 1378  | 1500  | 2438  |
| 16  | 1250 | 991  | 1500 | 2067  | 2250  | 3750  |
| 20  | 1656 | 1335 | 2250 | 3101  | 3375  | 5625  |
| 24  | 2188 | 1852 | 3375 | 4651  | 5062  | 8062  |
| 28  | 2875 | 2584 | 5000 | 6891  | 7500  | 12938 |
| 32  | 3844 | 3618 | 7000 | 9647  | 10500 | 24000 |
| 36  | 5188 | 5039 | 9000 | 12403 | 13500 | 36000 |
| 40  | 7000 | 7020 | 11000| 15159 | 16500 | 48000 |
| 44  | —    | 9647 | 13000| 17916 | 19500 | —     |
| 48  | —    | —    | 15000| 22050 | 24000 | —     |
| **liczba pasm / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### Tabela 2 — SBR: indeks start freq → orientacyjna częstotliwość przejścia [Hz]

To częstotliwość, od której SBR przejmuje pasmo powyżej rdzenia AAC (`--sbr-start`,
indeks 0..15; niższy = SBR startuje niżej = wezszy rdzen). "core" to częstotliwość
rdzenia; przy dual-rate wyjście jest dwa razy wyższe (np. core 24k → wyjście 48k).

| indeks start | core 16 kHz | core 24 kHz | core 32 kHz | core 44.1 kHz | core 48 kHz |
|----:|----:|----:|----:|----:|----:|
| 0 | 2750 | 2250 | 2500 | 1378 | 1500 |
| 1 | 3000 | 2625 | 3000 | 2067 | 2250 |
| 2 | 3250 | 3000 | 3500 | 2756 | 3000 |
| 3 | 3500 | 3375 | 4000 | 3445 | 3750 |
| 4 | 3750 | 3750 | 4500 | 4134 | 4500 |
| 5 | 4000 | 4125 | 5000 | 4823 | 5250 |
| 6 | 4250 | 4500 | 5500 | 5512 | 6000 |
| 7 | 4500 | 4875 | 6000 | 6202 | 6750 |
| 8 | 4750 | 5250 | 6500 | 6891 | 7500 |

Stop freq (`--sbr-stop`, 0..13) działa analogicznie na górnej granicy SBR — wyższy
indeks = SBR sięga wyżej (bliżej Nyquista wyjścia). Domyślnie biblioteka dobiera
oba pod bitrate.

### Tabela 3 — AAC-LC: orientacyjne odcięcie (`-w`/auto) wg bitrate na kanał [Hz]

Gdy nie podasz `-w`, FDK dobiera pasmo z tej tablicy wg bitrate NA KANAL (stereo
128k = 64k/kanał). Wartości interpolowane; kolumna mono i stereo różna. Pomaga
ocenic, czy `-w` ma sens (podawanie wyższego niż auto ma efekt tylko jesli jest zapas bitow).

| bitrate/kanał | pasmo (mono) | pasmo (stereo+) |
|----:|----:|----:|
| 0–12 kbps  | 3700  | 5000  |
| 20 kbps    | 6900  | 9640  |
| 28 kbps    | 9600  | 13050 |
| 40 kbps    | 12060 | 14260 |
| 56 kbps    | 13950 | 15500 |
| 72 kbps    | 14200 | 16120 |
| ≥96 kbps   | 17000 | 17000 |

Uwaga: ta tablica jest WSPOLNA dla 32/44.1/48 kHz i wyższych — FDK indeksuje ja
bitratem na kanał, nie samplerate (samplerate wpływa tylko na górny limit =
Nyquist). Dla LC bez SBR realny sufit to ~17 kHz z auto; wyżej tylko przez `-w`
(z zapasem bitow) lub `--uncap-bandwidth` przy sr≥96k.

---

## Przykłady

```
# Punkt wyjscia — co ustawil enkoder:
fdkaac-franken-x64.exe -p 29 -b 48000 --verbose -o out.m4a in.wav

# Quasi-constrained VBR ~128k (bezpieczny preset):
fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

# Agresywne intensity stereo przy 64k:
fdkaac-franken-x64.exe -p 2 -b 64000 --is 1 --is-aggression 70 -o out.m4a in.wav

# PNS na sile przy 24k (inaczej bramka FDK je wyłącza):
fdkaac-franken-x64.exe -p 2 -b 24000 --pns 1 --force-pns -o out.m4a in.wav

# Audiofilskie pelne pasmo 40 kHz z wejscia 96 kHz:
fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

# MS na 5 najwyzszych pasmach + plytsze dziury (nie najnizsze):
fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 --ms-precision 448 -o out.m4a in.wav

# Skrajnie niski HE-AAC v2: 8000 bps stereo 48 kHz:
fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

# SBR: pelna separacja stereo LR + wymuszony inverse filtering:
fdkaac-franken-x64.exe -p 5 -b 96000 --sbr-stereo-mode 1 --sbr-invf 2 -o out.m4a in.wav

# Twoj przypadek: 7.5 kHz rdzenia pod HE-AAC v2 48k:
fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav

# Kompletnie niezalezne stereo (bez MS/IS) na LC 128k:
fdkaac-franken-x64.exe -p 2 -b 128000 --msmask 0 --is 0 -o out.m4a in.wav

# MS tylko na 6 dolnych pasmach:
fdkaac-franken-x64.exe -p 2 -b 96000 --msmask 1 --msbands 6 -o out.m4a in.wav

# Tylko krotkie bloki + ograniczony TNS:
# Tylko dlugie bloki (bias) + ograniczony TNS:
fdkaac-franken-x64.exe -p 2 -b 96000 --block-bias 0 --tns-order 2 -o out.m4a in.wav

# Agresywniejsze maskowanie (progi x2) + wczesniejszy PNS:
fdkaac-franken-x64.exe -p 2 -b 80000 --ath-scale 512 --pns 1 --pns-start 4000 -o out.m4a in.wav

# Gestszy opis szumu SBR + dokladniejsza amplituda:
fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-noise-bands 5 --sbr-amp-res 0 -o out.m4a in.wav

# HE-AAC v2 z wyłączonym PS (splaszczone stereo):
fdkaac-franken-x64.exe -p 29 -b 32000 --ps 0 -o out.m4a in.wav
```

---

## Jakość domyślna vs oryginalna binarka (fdkaac2.exe)

Weryfikacja (21.07.2026) czy domyślne ustawienia franken = oryginalna dostarczona
binarka (dBpoweramp R17, ta sama wersja libfdk-aac 4.0.1 / pakiet 2.0.3):
- AAC-LC: **bit-identyczne** (ten sam md5) — zero regresji.
- HE-AAC v1/v2: bajty sie różnią, ALE: widmo HE-AAC v2 identyczne co do 0.000 dB,
  a błąd HE-AAC v1 wzgledem oryginału jest praktycznie taki sam jak w oryginale
  (7.05e11 vs 7.05e11). Różnica bajtow wynika z innego kompilatora (mingw/gcc vs
  MSVC) i zaokrągleń fixed-point, NIE z gorszej jakości. Słyszalna "inność" SBR
  to inna, równie poprawna realizacja, nie strata jakości.

---

## Testy (make check)

Upstream fdk-aac/nu774 nie maja testów jednostkowych (`make check` w nich = no-op).
Ten projekt ma WŁASNY funkcjonalny test suite w `tests/check.sh`, uruchamiany:

```
make check
```

Sprawdza (na binarkach x64 + x86 jesli obecne): kompletność switchy w --help,
dekodowalność strumienia dla kazdego przełącznika (ffmpeg, 0 błędów), realne
działanie quasi-CVBR (CVBR oddycha szerzej niż sztywny bitres-mode2), verbose bez
wartości -1, oraz BRAK REGRESJI (domyślny CBR bez franken-flag = ADTS
bit-identyczny z oryginalna fdkaac2.exe). Wymaga ffmpeg + python3 (są w WSL).
Exit 0 = OK, 1 = błędy, 77 = brak zależności. Ostatni wynik: 27/27 PASS.

---

Zrodla z naniesionymi patchami leza w `src-fdk-aac/` i `src-fdkaac/`.
Wszystkie zmiany w libfdk są spięte przez jeden modul
`libAACenc/src/franken.{h,cpp}` (globalny blok `g_franken`, sentinele = domyślne
FDK), a nowe `AACENC_PARAM` (zakres `0xF0xx`) są wystawione w `aacenc_lib.h`.

```bash
sudo apt-get install -y mingw-w64          # + autotools (autoconf/automake/libtool)

# --- libfdk-aac (x64) ---
cd src-fdk-aac && autoreconf -i
./configure --host=x86_64-w64-mingw32 --prefix=$PWD/../inst-x64 \
    --enable-static --disable-shared CFLAGS=-O2 CXXFLAGS=-O2
make -j && make install

# --- frontend (x64) ---
cd ../src-fdkaac && autoreconf -i
PKG_CONFIG_PATH=$PWD/../inst-x64/lib/pkgconfig ./configure \
    --host=x86_64-w64-mingw32 CFLAGS="-O2 -I$PWD/../inst-x64/include" \
    LDFLAGS="-static -static-libgcc -L$PWD/../inst-x64/lib"
make -j     # -> fdkaac.exe

# x86: to samo z --host=i686-w64-mingw32 i osobnym prefixem inst-x86.
```

## Lista zmienionych plikow FDK (mapa na punkty)
- `libAACenc/src/franken.{h,cpp}` — nowy modul sterujący.
- `libAACenc/include/aacenc_lib.h` — nowe `AACENC_PARAM` 0xF0xx.
- `libAACenc/src/aacenc_lib.cpp` — dispatch SetParam + override useMS/IS/PNS/
  afterburner/cutoff + guard cutoffu pod SBR (po `sbrEncoder_Init`) + read-only
  GetParam mirrors dla verbose (useTns/Pns/IS/MS, efektywne SBR).
- `libAACenc/src/ms_stereo.cpp` — kontrola MS per-pasmo W PETLI decyzyjnej (maska
  + motylek L/R->M/S zsynchronizowane; naprawiony artefakt "lewy=center") + MS bias
  + zakres pasm MS (--msbands-lo/-hi) + precyzja MS (--ms-precision, prog ld64).
- `libAACenc/src/intensity.cpp` — kap pasm IS (spójny z maska) + bias progow IS
  (initIsParams: min_is_sfbs, corr_thresh, left_right_ratio) + --is-aggression.
- `libAACenc/src/psy_configuration.cpp` — read-back liczby SFB + zniesienie gate IS.
- `libAACenc/src/bandwidth.cpp` — --uncap-bandwidth (zdjęcie capa 20kHz).
- `libAACenc/src/pnsparam.cpp` — override PNS start + --force-pns (bypass bramki tabeli).
- `libAACenc/src/aacenc.cpp` — override maski TNS + quasi-CVBR + --unlock-bitrate
  (zniesienie dolnego floora bitrate w FDKaacEnc_LimitBitrate).
- `libSBRenc/src/sbr_encoder.cpp` — override gęstości SBR + --sbr-num-env (static
  framing) + --sbr-freqres-fixfix + --sbr-stereo-mode + --sbr-noise-floor-offset.
- `libSBRenc/src/invf_est.cpp` — --sbr-invf (wymuszony poziom inverse filtering).
- `libSBRenc/src/ps_encode.cpp` — override PS IID + --ps-icc/--ps-icc-mode.
- `libAACenc/src/aacenc_tns.cpp` — kap rzędu TNS.
- `libAACenc/src/pnsparam.cpp` — override startowej częstotliwości PNS.
- `libSBRenc/src/sbr_encoder.cpp` — override gęstości/dokładności SBR + zapis
  efektywnych wartości SBR do g_franken (dla verbose).
- `libSBRenc/src/ps_encode.cpp` — override PS (wymuszenie IID + tryb kwantyzacji).
- `libAACenc/src/main.c`, `aacenc.c`, `aacenc.h` (frontend) — switche CLI,
  parsowanie, przekazanie do SetParam, dump `--verbose`, help.
