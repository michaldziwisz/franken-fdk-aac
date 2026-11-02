# Franken FDK AAC

Franken FDK AAC to koder Fraunhofer FDK AAC (libfdk-aac 2.0.3, mstorsjo/fdk-aac) z
frontendem nu774/fdkaac 1.0.2, rozszerzony o przełączniki odsłaniające decyzje, które FDK
zwykle podejmuje sam: jak kodowane jest stereo, jak odtwarzane jest górne pasmo, gdzie wolno
wyzerować pasma, jak traktowane są transjenty, jak bity rozkładają się w czasie. Jest
przeznaczony dla realizatorów, którzy chcą usłyszeć i zmierzyć, co te decyzje robią,
zwłaszcza w skrajnościach: przy bardzo niskim bitrate'ie, wymuszonym szerokim paśmie, mowie
i streamingu.

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski. Zbudowany na otwartym
kodzie: libfdk-aac (Fraunhofer IIS) i frontend nu774/fdkaac.

Bez żadnego z nowych przełączników koder daje dokładnie to samo co fabryczny FDK, bajt w
bajt. Każdy przełącznik trzeba włączyć świadomie i każdy może też pogorszyć wynik: pozwalają
wejść tam, gdzie strojenie FDK celowo nie wchodzi.

## Pobieranie

Gotowe binarki dla Windows są publikowane jako GitHub Releases i pobierają się bez konta:

**https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Wersje beta (na przykład `v1.4.0-beta.1`) są publikowane jako wydania wstępne, więc powyższy
odnośnik nadal pokazuje ostatnią wersję stabilną; bety są na pełnej liście:
**https://github.com/michaldziwisz/franken-fdk-aac/releases**

`franken-fdk-aac-x64-vX.Y.Z.zip` to wersja 64-bitowa, `x86` to 32-bitowa. Każde archiwum
zawiera statyczny koder (bez bibliotek DLL) i pełną dokumentację. Samo repozytorium ma
tylko źródła; budowanie opisuje koniec tego dokumentu.

## Pierwsze kroki

Wszystkie oryginalne opcje frontendu działają jak dotąd (`-p`, `-b`, `-m`, `-w`, `-a`,
`-s`, `-f`, znaczniki); `fdkaac-franken-x64.exe --help` wypisuje je razem z nowymi.

1. Zakoduj raz z `--verbose` i przeczytaj, co koder faktycznie ustawił:

       fdkaac-franken-x64.exe -p 2 -b 128000 --verbose -o out.m4a in.wav

2. Zmieniaj jeden przełącznik naraz i sprawdzaj w `--verbose`, w sekcji
   `franken overrides applied`, czy zadziałał.
3. Słuchaj, a gdzie to ważne, mierz: zdekoduj obie wersje i porównaj. Narzędzia w
   katalogu `tools/` repozytorium robią to dla dziur, pre-echa i błędu stereo.

## Przepisy

Każdy z nich to sprawdzony punkt wyjścia, a nie preset do stosowania w ciemno.

    # LC stereo przy niskim bitrate'ie z szerokim pasmem: mniej dziur, mniej pre-echa
    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

    # To samo przy 40 kbps, ze spokojniejszym detektorem ataków dla perkusji
    fdkaac-franken-x64.exe -p 2 -b 40000 -w 18000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 --attack-decay 0,9 -o out.m4a in.wav

    # Szersze stereo kosztem środka
    fdkaac-franken-x64.exe -p 2 -b 96000 --side-bias 6 -o out.m4a in.wav

    # Quasi-VBR wokół 128 kbps: silnik CBR z miejscem na oddech
    fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 160000 --vbr-reservoir 6000 -o out.m4a in.wav

    # HE-AAC z łagodniejszym, mniej metalicznym górnym pasmem
    fdkaac-franken-x64.exe -p 5 -b 32000 --sbr-softness 4 -o out.m4a in.wav

    # HE-AAC v2 z szerszym rdzeniem, niż wybiera FDK (9 kHz -> 10,5 kHz)
    fdkaac-franken-x64.exe -p 29 -b 48000 --sbr-start 14 -o out.m4a in.wav

    # Strumień, który słuchacz dołączający w trakcie od razu dekoduje z SBR
    fdkaac-franken-x64.exe -p 5 -b 48000 --sbr-header-period 1 -f 2 -o out.aac in.wav

    # AAC-LD z szerszym rdzeniem i częstszym TNS przeciw pre-echu (LD wymaga LOAS, nie ADTS)
    fdkaac-franken-x64.exe -p 23 -b 64000 --core-cutoff 12000 --tns-thresh 0,7 -f 10 -o out.loas in.wav

    # Skrajnie niski HE-AAC v2: 8000 bps stereo
    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

    # Pełny rdzeń 42 kHz ze źródła 96 kHz
    fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

Ostatni przepis daje w `--verbose` `core bandwidth: 42000 Hz`: żądane 40000 Hz zostaje
wyrównane w górę do najbliższej granicy pasma współczynnika skali.

# Opis przełączników

To jest opis wszystkich przełączników. Każdy ma własną kartę, a każda karta jest zbudowana
tak samo, więc można czytać tylko tę kartę, która jest potrzebna.

## Jak czytać kartę

Nagłówek to przełącznik w takiej postaci, w jakiej się go wpisuje, razem z argumentem. Pod
nim:

- Jedno zdanie pogrubione: do czego służy przełącznik, pod kątem dźwięku albo
  zachowania.
- **Dotyczy**: profile i sytuacje, w których przełącznik faktycznie działa. Jeśli w
  jakimś profilu nie robi nic, jest to napisane właśnie tutaj.
- **Wartości**: dozwolony zakres, jednostka i wartość domyślna. Domyślna to zawsze ta,
  która oznacza „decyduje fabryczny FDK”.
- **Wpisywanie**: czy przełącznik przyjmuje liczbę całkowitą, liczbę dziesiętną, flagę
  czy tekst.
- Dalej wyjaśnienie: najpierw problem, potem mechanizm w prostych słowach, a na końcu
  co się dzieje przy zwiększaniu i zmniejszaniu wartości.
- **Przykłady**: dla przełącznika liczbowego z miejscem na manewr nastawa łagodna,
  mocna i celowo przesadzona, w każdym kierunku. Przełączniki wyliczeniowe mają w tym
  miejscu listę wszystkich wartości.
- **Współpraca i konflikty**: zależności oraz informacja, który inny przełącznik
  przesłania ten.
- **Zmierzone**: wyłącznie rzeczywiste pomiary, z materiałem i warunkami. Jeśli pomiaru
  nie ma, nie ma też tej linii.
- **Pod maską**: jedna albo dwie linie dla ciekawych. Można je spokojnie pominąć.

## Wspólne zasady

Obowiązują w każdej karcie i nie są w nich powtarzane.

**Wyłączony = ustawienie fabryczne.** Każdy przełącznik Frankensteina jest wyłączony,
dopóki się go nie poda. Jeśli żadnego z nich nie ma w linii poleceń, wynik jest
identyczny bajt w bajt z wynikiem fabrycznego FDK (zachowanie niezmienionego FDK).
„Wyłączony” zapisuje się jako `-1` dla niemal wszystkich (to wartość specjalna oznaczająca
„zostaw własną decyzję FDK”) oraz jako `0` dla `--core-cutoff`, `--side-bias`,
`--side-knee` i `--mask-slope`, gdzie `-1` byłoby poprawną, działającą wartością.

**Liczby dziesiętne.** Przełączniki dziesiętne przyjmują `.` albo `,`, do trzech miejsc
po przecinku. Wartość jest zaokrąglana w połowie od zera, a zakres jest sprawdzany już
na wartości zaokrąglonej. Wszystko inne to błąd: `1e1`, `nan`, `3x`, spacje w środku.
Przełączniki całkowitoliczbowe odrzucają części dziesiętne, więc `128,5` tam, gdzie
oczekiwana jest liczba całkowita, jest błędem, a nie cichym obcięciem.

**Jednostki.** W całym tekście pojawiają się cztery konwencje:

- `dB`: zwykłe przesunięcie w decybelach, dodatnie albo ujemne.
- `x100`: liczba całkowita będąca stukrotnością wartości rzeczywistej, więc `100`
  oznacza 1,00, `90` oznacza 0,90, a `400` oznacza 4,00.
- `Q8`: liczba całkowita w formacie stałoprzecinkowym, gdzie `256` oznacza 1,0. Czyli
  `128` to 0,5, `384` to 1,5, `512` to 2,0, a `4096` to 16,0. Q8 to wewnętrzna skala
  samego FDK i dlatego trafia ona do linii poleceń.
- `SFB`: numer pasma współczynnika skali (scale factor band, SFB), liczony od `0` od dołu
  widma w górę. Pasmo współczynnika skali to grupa linii widmowych, które dzielą jeden
  współczynnik skali (scale factor),
  w przybliżeniu zgodnie z pasmem krytycznym: wąskie na dole, szerokie na górze. Ile
  tych pasm istnieje, zależy od profilu, częstotliwości próbkowania i kodowanego pasma,
  więc numer SFB ma sens tylko wtedy, gdy znana jest ta liczba. Wypisuje ją `--verbose`,
  na przykład `active SFBs (long): 49` przy AAC-LC 48 kHz stereo.

**Jak sprawdzić, co weszło w życie.** `--verbose` wypisuje na stderr rzeczywiste,
zainicjalizowane parametry kodera, zanim zacznie się kodowanie, a do tego linię
`franken overrides applied` z listą przełączników, które w tym przebiegu naprawdę
odbiegają od ustawień fabrycznych. Jeśli ustawionej wartości tam nie ma, to nie weszła ona w życie i
żadne słuchanie nie powie o tym szybciej.

## Jak czytany jest `-b`

Obok siebie żyją trzy konwencje, a pomylenie ich to najczęstszy sposób na zaskakujący
rozmiar pliku.

- **Praca normalna**: `-b` to bity na sekundę, ale frontend przyjmuje też postać skróconą.
  Małe liczby są czytane jako kilobity, więc `-b 128` i `-b 128000` oznaczają to samo,
  czyli 128 kbps, a `-b 1152` oznacza 1152 kbps.
- **Z `--unlock-bitrate`**: `-b` jest czytane dosłownie jako bity na sekundę, bez skrótu
  kilobitowego. `-b 6000` to 6000 bps. `-b 1152` w tym trybie to 1152 bps, czyli
  praktycznie nic; program ostrzega, gdy wartość wygląda na taką właśnie pomyłkę.
- **Z `--dab`**: `-b` to kilobity na sekundę, w zakresie 8..192 i jako wielokrotność 8.
  Koder sam mnoży przez 1000, więc `-b 96` to 96 kbps, także razem z `--unlock-bitrate`.

Górnego pułapu nie ustawia żaden przełącznik i nie da się go podnieść: ramka AAC mieści
najwyżej 6144 bity na kanał. Przy 48 kHz to z grubsza 576 kbps na kanał.

## Uwaga o transporcie dla LD i ELD

AAC-LD (`-p 23`) i AAC-ELD (`-p 39`) działają w tym koderze, ale nie da się ich zapisać do
ADTS. Żądanie `-f 2` z tymi profilami kończy się komunikatem „encoder initialization
failed”, a winowajcą jest format transportu, nie profil. Trzeba użyć M4A (`-f 0`, wartość
domyślna), LATM (`-f 6` albo `-f 7`) albo LOAS (`-f 10`). LOAS jest wyborem praktycznym, gdy
potrzebny jest goły strumień do pomiaru albo do podania dalej, bo jest to samowystarczalny
strumień bajtów, tak jak ADTS dla LC i HE-AAC.

## Mapa grup

Przełączniki są pogrupowane w `--help` z grubsza od łatwych do dziwactw, a ten opis trzyma
się tej samej kolejności.

- **A. Na początek**: pokrętła dla użytkownika. Diagnostyka, przeliczanie częstotliwości
  próbkowania wejścia, strojenie pod mowę, intensity stereo na jednym suwaku i dwie flagi
  włączane świadomie, które zdejmują wbudowane ograniczenia.
- **B. Kodowanie stereo**: mid/side, intensity stereo w szczegółach, parametric stereo.
- **C. Pasmo i SBR**: gdzie kończy się rdzeń i zaczyna SBR, potem siatka, detektory i
  estymatory górnego pasma; grupę zamyka `--core-cutoff`.
- **D. Maskowanie, kształtowanie szumu i szczegół**: progi psychoakustyczne, SNR, którego
  koder wymaga w każdym paśmie, TNS i PNS.
- **E. Transjenty rdzenia, pre-echo i dziury w widmie**: rdzeń LC, LD i ELD pod presją
  bitów.
- **F. Przełączanie bloków i kontrola bitrate'u**: decyzja blok długi czy krótki,
  rezerwuar bitów i pokrętła quasi-VBR.
- **G. Wyjście DAB+**: superramki radia cyfrowego.

## A. Na początek

Sześć przełączników, w kolejności, w jakiej zwykle są potrzebne: `--verbose` pokazuje, co
koder naprawdę zrobił, `--resample` wybiera częstotliwość próbkowania, z którą pracuje cały
koder, `--speech` i `--is-aggression` go stroją, a `--unlock-bitrate` i `--uncap-bandwidth`
zdejmują ograniczenia fabryczne FDK. Na początek warto przeczytać o `--verbose` i używać go
po każdym kolejnym przełączniku. Towarzyszem `--verbose` jest `--bit-stats` (grupa E), gdy
pytanie brzmi, na co poszły bity.

### `--verbose`

**Wypisuje na stderr rzeczywiste, zainicjalizowane ustawienia kodera przed kodowaniem,
żeby było widać, co faktycznie weszło w życie.**

- Dotyczy: każdego profilu i każdego transportu. Nie zmienia nic w strumieniu wyjściowym.
- Wartości: flaga, bez argumentu; domyślnie: wyłączona
- Wpisywanie: flaga

Koder podejmuje na podstawie linii poleceń bardzo wiele decyzji i większość z nich nie jest
wpisanymi wartościami. Pasmo zostaje przyciągnięte do granicy pasma współczynnika skali.
Bitrate wybiera wiersz w tabeli strojenia, a ten z kolei ustawia PNS, intensity stereo i
siatkę SBR. Poprawnie zapisany przełącznik może i tak zostać zignorowany, bo profil nie ma
takiego mechanizmu. `--verbose` to jedyny sposób, by zobaczyć wynik tego wszystkiego przed
zmarnowaniem czasu na słuchanie.

Co wypisuje: profil (AOT), bitrate i tryb bitrate'u, częstotliwość próbkowania, tryb
kanałów, efektywne pasmo rdzenia w Hz wraz ze źródłem tej decyzji (`from -w`, `from
--core-cutoff` albo `auto`), stan afterburnera, transport i sygnalizację oraz stan
narzędzi kodowania, które koder wybrał sobie sam, czyli TNS, PNS, intensity stereo i MS
stereo włączone albo wyłączone. Gdy działa SBR, dochodzi do tego współczynnik SBR,
efektywne indeksy częstotliwości początkowej i końcowej, skala częstotliwości, pasma
szumu, rozdzielczość amplitudy oraz, dla całego pasma, oszacowanie `final BW (AAC+SBR)`.
Wypisuje blok budżetu rezerwuaru, który mówi, ile jest średnio bitów na ramkę i jakie są
pułapy, a także `active SFBs (long)`, czyli liczbę potrzebną przed ustawieniem
jakiegokolwiek przełącznika opartego na SFB. Na końcu pojawia się lista
`franken overrides applied`, wymieniająca tylko te przełączniki, które w tym przebiegu
naprawdę odbiegają od ustawień fabrycznych.

Trzy odczyty warto obejrzeć uważnie. Pasmo rdzenia jest wyrównane do granicy pasma SFB,
więc wpisane `-w 17300` może wrócić jako `17915 Hz`: koder nie potrafi przyciąć w środku
pasma. Pod SBR wartość `-w` jest ignorowana, ale nadal opisana jako `[from -w]`; tam
rdzeń idzie za `--sbr-start`. Natomiast `active SFBs (long)` to pułap dla `--msbands`,
`--isbands` i każdego argumentu `<sfb>`: poproszenie o pasmo 48, gdy kodowanych jest tylko
40, nie jest błędem, po prostu nic nie robi.

Wartości:
- `--verbose`: wypisz zrzut. Nie ma przełącznika wyłączającego, bo pominięcie go jest
  wyłączeniem.

Przykład w kontekście:

    fdkaac -p 5 -b 64000 --verbose -o out.m4a in.wav

Współpraca i konflikty: brak. To narzędzie diagnostyczne dla wszystkiego innego w tym opisie
i najszybsza odpowiedź na pytanie „czy mój przełącznik w ogóle coś zrobił”.

Zmierzone: przy AAC-LC 48 kHz stereo raportuje `active SFBs (long): 49`. Przy HE-AAC v2 48
kbps, wejście 48 kHz, rdzeń wychodzi na 9000 Hz przy indeksie startu SBR 13.

### `--resample <częstotliwość>`

**Przelicza wejście na inną częstotliwość próbkowania, zanim zobaczy je cokolwiek innego,
więc cały koder pracuje już w nowej częstotliwości.**

- Dotyczy: każdego profilu i transportu, także `--dab`. Tak samo działa na plikach, na
  potoku (`-` jako wejście) i na wejściu `--raw`.
- Wartości: częstotliwość próbkowania AAC w Hz (`44100`) albo w kHz (`44`, `44.1`, `44,1`,
  `22`, `22.05`, `32`, `48`, `96` ...). Od 8000 do 96000. `-1` = wyłączone; domyślnie:
  wyłączone.
- Wpisywanie: częstotliwość

Przelicznik stoi na samym początku łańcucha PCM, zaraz za czytnikiem WAV, CAF albo surowego
PCM, przed limiterem, ekstrapolatorem brzegów i koderem. Nic dalej nie zna pierwotnej
częstotliwości: koder dobiera tabele, pasmo, układ SBR i budżet bitów dla nowej, skala
czasu M4A i długość gapless są liczone w nowej, a `--verbose` pokazuje nową wartość
`samplerate`. Źródło, które już ma żądaną częstotliwość, idzie nietkniętym łańcuchem, bit
w bit jak bez przełącznika.

Jak przelicza: dokładny stosunek wymierny (48000 na 44100 to 147/160, więc przez godziny
dźwięku nic się nie rozjeżdża), jądro sinc z oknem Kaisera liczone na starcie, arytmetyka
zmiennoprzecinkowa. Pasmo przenoszenia jest płaskie do 95 procent niższej częstotliwości
Nyquista, tłumienie zaczyna się na tej częstotliwości i wynosi 120 dB, więc nic nie odbija
się w pasmo słyszalne. Jądro jest symetryczne, a chwile wyjściowe są wyrównane z wejściem,
więc przeliczenie nie dodaje opóźnienia: opóźnienie kodera i priming gapless zostają takie,
jakie są w nowej częstotliwości. Jądro potrzebuje kilku milisekund wyprzedzenia wewnątrz
przelicznika, czego z zewnątrz nie widać. Żadnej zewnętrznej biblioteki ani DLL.

Po co: poniżej mniej więcej 48 kbps stereo z wąskim `-w` rdzeń 48 kHz wydaje bity na pasma
współczynników skali i siatkę ramek pomyślaną pod dźwięk do 20 kHz. Niższa częstotliwość
daje drobniejsze pasma poniżej odcięcia i dłuższą ramkę. Dłuższa ramka to też cena: przy 24
kHz jeden blok długi trwa 85 ms, więc pre-echo przed ostrymi atakami rośnie. Wyższa
częstotliwość też się przydaje: `--dab` przyjmuje tylko 32 i 48 kHz, więc źródło 44,1 kHz
potrzebuje `--resample 48`.

Wartości:
- pominięta albo `-1`: bez przeliczania (jak w fabrycznym FDK).
- `--resample 32`: źródło 48 albo 44,1 kHz kodowane w 32 kHz. Przy `-w 9500` i 32 kbps
  stereo zwykle najlepszy kompromis (patrz Zmierzone).
- `--resample 24`: jeszcze dalej w tę stronę, z dłuższymi blokami; warto tylko dla
  materiału bez ostrych ataków.
- `--resample 48`: źródło 44,1 kHz dla `--dab` albo dla HE-AAC w 48 kHz.

Przykład w kontekście:

    fdkaac -p 2 -b 32000 -w 9500 --resample 32 -o out.m4a in48k.wav
    ffmpeg -i in.flac -f wav - | fdkaac --dab -b 64 --resample 48 -o out.dabp -

Współpraca i konflikty: `-w` i `--core-cutoff` są w Hz, więc w każdej częstotliwości znaczą
to samo, ale nie mogą przekroczyć nowej częstotliwości Nyquista. `--raw-rate` podaje
częstotliwość surowego wejścia, a `--resample` przelicza z niej. Przełączniki z numerem
pasma (`--msbands`, `--is-lo` ...) liczą pasma tabeli nowej częstotliwości, sprawdź je w
`--verbose`.

Zmierzone: sam przelicznik (harness `tools/resample/rs_test.c`,
`tools/resample/rs_quality.py`) dla 48 na 32, 48 na 24, 44,1 na 32, 48 na 44,1, 44,1 na
22,05, 32 na 48, 44,1 na 48 i 48 na 16 kHz: odchyłka pasma przenoszenia poniżej 0,0001 dB
do 95 procent Nyquista, aliasy tonów leżących między nową a starą częstotliwością Nyquista
od -120 do -132 dB, THD+N tonu 1 kHz od -135 do -141 dB, przesunięcie fazy 0, długość
wyjścia dokładnie `ceil(n * nowa / stara)`, te same próbki niezależnie od wielkości porcji
odczytu. Szybkość: od 37 do 106 razy szybciej niż czas rzeczywisty dla stereo na jednym
rdzeniu (przypadek 48 na 32 kHz: kodowanie 30 s trwało 0,94 s wobec 0,62 s bez
przełącznika). Po kodowaniu (AAC-LC 32 kbps stereo, `-w 9500`, błąd mierzony w 200..9000 Hz
względem oryginału 48 kHz): to samo z dokładnością do 0,03 dB co wcześniejsze przeliczenie
programem SoX w najwyższej precyzji. W porównaniu z kodowaniem w 48 kHz stosunek szumu do
sygnału na czterech próbkach muzyki 48 / 32 / 24 kHz: -7,3 / -8,2 / -9,0 dB,
-11,1 / -14,8 / -16,8 dB, -7,6 / -8,4 / -9,1 dB, -8,3 / -7,7 / -8,6 dB; dziury w widmie
5,6 / 0,1 / 0 procent na pierwszej próbce. Pre-echo na sygnale testowym z atakami:
-33,9 dB przy 48 kHz, -28,4 dB przy 32 kHz, -28,1 dB przy 24 kHz.

### `--speech`

**Przełącza warstwę SBR na strojenie pod mowę: inne filtrowanie odwrotne, inny poziom
szumu (noise floor) i brak parametrycznego kodowania górnego pasma.**

- Dotyczy: HE-AAC i HE-AAC v2, czyli wszędzie, gdzie działa SBR, także ELD z SBR. Bez
  efektu w AAC-LC, które nigdzie w FDK nie ma osobnego trybu mowy.
- Wartości: flaga; domyślnie: wyłączona (strojenie pod muzykę)
- Wpisywanie: flaga

SBR odbudowuje górne pasmo z zakodowanego pasma dolnego i musi wiedzieć, jak tonalne jest
źródło, żeby zdecydować, jak mocno wybielić pasmo odtworzone przez SBR i ile szumu dosypać.
Muzyka i mowa odpowiadają na to zupełnie inaczej. Mowa przy niskim bitrate cierpi przy
strojeniu pod muzykę na konkretny artefakt: odbudowana góra bulgocze, bo estymator wciąż
uznaje odtworzone pasmo za tonalne, a w rzeczywistości jest to ruchomy formant plus oddech.

Flaga zmienia kilka rzeczy naraz: tabela decyzyjna filtrowania odwrotnego przełącza się na
tabelę mowy, wagowanie poziomu szumu przechodzi na wartość dla mowy, a parametryczne
kodowanie górnego pasma zostaje wyłączone. Dla podcastu, audiobooka albo dialogu przy 24..48
kbps efektem jest naturalniejsza góra z mniejszym bulgotaniem.

Jeśli potrzebna jest tylko część tego, co robi flaga, poszczególne elementy są dostępne
osobno: `--sbr-invf-table 1` daje samą tabelę decyzyjną mowy, `--sbr-nf-weight 1.0` samą
wagę szumu dla mowy, a `--sbr-softness 4` oba te elementy, w każdym przypadku bez ruszania
częstotliwości podziału i bez wyłączania kodowania parametrycznego.

Dla mowy w zwykłym AAC-LC nie ma odpowiednika i ten opis nie będzie udawał inaczej.
Model psychoakustyczny LC jest ten sam dla mowy i dla muzyki. Pomaga tam natomiast
zwężenie pasma, bo mowa ma mało treści wysoko, za pomocą `--core-cutoff 12000` do
`14000`, a przy bardzo niskim bitrate włączenie substytucji szumem przez
`--pns 1 --force-pns`.

Wartości:
- pominięta: strojenie pod muzykę (ustawienie fabryczne).
- `--speech`: strojenie pod mowę.

Przykład w kontekście:

    fdkaac -p 5 -b 32000 --speech -o speech.m4a podcast.wav

Współpraca i konflikty: jawne przełączniki estymatorów SBR z grupy C wygrywają z tą flagą,
więc `--sbr-invf` wymusza ustalony poziom filtrowania odwrotnego bez względu na nią.
`--sbr-invf-table` i `--sbr-nf-weight` idą za `--speech`, dopóki zostają na `-1`, i właśnie
tak jedna flaga potrafi ruszyć oba.

Zmierzone: zerowy efekt w AAC-LC; zmienia strumień HE-AAC.

### `--is-aggression <0..100>`

**Jeden suwak decydujący, jak ochoczo koder zwija górne pasma do intensity stereo,
wymieniając separację stereo na górze na bity.**

- Dotyczy: profili z prawdziwym rdzeniem stereo, czyli AAC-LC, AAC-LD i AAC-ELD, oraz
  rdzenia HE-AAC. Bez efektu w mono. Pod HE-AAC v2 obraz stereo niesie parametric stereo
  na rdzeniu mono, więc intensity stereo nie ma na czym działać.
- Wartości: `0..100`, gdzie `0` to zachowanie fabrycznego FDK; domyślnie: `-1`, wyłączone
  (identycznie jak bez przełącznika)
- Wpisywanie: liczba całkowita

Intensity stereo (IS) zastępuje osobne widma lewego i prawego kanału w górnych pasmach jedną
wspólną obwiednią energii plus kierunkiem. Ucho słabo lokalizuje wysokie tony, więc jest to
tanie: odrzuca się połowę danych w tych pasmach, a płaci separacją stereo na górze. Na
materiale, który naprawdę ma różne góry w obu kanałach, na przykład z talerzami ustawionymi
mocno na jedną stronę albo z szerokimi efektami przestrzennymi, płaci się słyszalnie.

Fabryczny FDK jest wobec IS wyjątkowo nieśmiały i właśnie dlatego łatwo uznać, że
przełącznik nie robi nic. Warunków dopuszczenia (gate) jest cztery i pasmo musi spełnić
wszystkie naraz: warunek bitrate'u, który bierze IS pod uwagę tylko wtedy, gdy bitrate na
pasmo spada poniżej progu, warunek korelacji między kanałami na poziomie około 0,95,
minimalna seria 6 kolejnych pasm i stabilny kierunek. Poluzowanie jednego warunku zwykle nie
zmienia nic, bo blokuje kolejny. `--is-aggression` rusza nimi wszystkimi razem, spójnie:
usuwa warunek bitrate'u i warunek liczby pasm oraz, w miarę wzrostu liczby, luzuje wymagania
co do korelacji, kierunku i rozmiaru obszaru.

Wyżej oznacza więcej pasm kodowanych jako IS, mniej bitów wydanych na górę widma i słabszą
separację stereo właśnie tam. `100` to prawdziwe maksimum: warunek bitrate'u usunięty,
korelacja poluzowana do około 0,475, obszar jednego pasma już wystarcza, a tolerancja
kierunku jest szeroka. Niskie wartości nie nadają się na delikatną korektę, bo małe
poluzowanie często i tak zostawia inny warunek jako wiążący; `1` nie zmieniło nic na
materiale testowym. Użyteczne okno pracy zaczyna się około 30.

Jest tylko jeden kierunek do przejścia, bo `0` to już ustawienie fabryczne, a fabryczne
strojenie jest blisko „nigdy”.

Przykłady:
- Łagodnie, więcej IS: `--is-aggression 30`. Na skorelowanym materiale pasma zaczynają
  trafiać do IS. Obraz na górze zwęża się nieznacznie, kilka procent bitów zostaje
  uwolnionych na resztę.
- Mocno, więcej IS: `--is-aggression 50`. Wyraźnie więcej pasm w IS. Słyszalne zwężenie
  na talerzach i ogonach pogłosu, realna oszczędność bitów przy 48..96 kbps.
- Przesadnie, więcej IS: `--is-aggression 100`. Do IS idzie prawie wszystko, co może.
  Góra obrazu zlepia się ze środkiem, a spanoramowane góry zaczynają dryfować. Przydatne
  przy 32 kbps i niżej oraz jako ustawienie skrajne, żeby usłyszeć, jak brzmi IS puszczone
  luzem.

Współpraca i konflikty: jawne przełączniki `--is-*` z grupy B nadpisują to, co ustawił
suwak, więc najpierw warto użyć suwaka, a po `--is-corr-thresh`, `--is-min-sfbs` i
`--is-lr-ratio` sięgać tylko wtedy, gdy potrzebna jest precyzja. `--is 0` wyłącza
intensity stereo całkowicie i czyni ten przełącznik bez znaczenia. `--is-force-lo` i
`--is-force-hi` obchodzą każdy warunek dopuszczenia, więc ten suwak nie jest im w ogóle potrzebny.
`--verbose` wypisuje efektywny próg korelacji i minimalną liczbę pasm, czym potwierdza
się, że suwak zadziałał.

Zmierzone: na mix48 przy AAC-LC 64 kbps `1` nie dało żadnej zmiany strumienia, a `30`,
`50` i `100` za każdym razem strumień zmieniły.

### `--unlock-bitrate`

**Zdejmuje dolne progi bitrate'u, których pilnuje każdy profil, i sprawia, że `-b` to
dosłownie bity na sekundę.**

- Dotyczy: każdego profilu. Zdejmowane progi różnią się w zależności od profilu, patrz
  niżej.
- Wartości: flaga; domyślnie: wyłączona (wszystkie progi obowiązują)
- Wpisywanie: flaga

Fabryczny FDK odmawia kodowania poniżej pewnego progu, a próg ten jest inny w każdym profilu
i często bywa mylony z ograniczeniem formatu. Nie jest nim. Rozpowszechnione twierdzenie, że
HE-AAC nie potrafi zejść poniżej 16 kbps, opisuje najniższy wiersz tabeli strojenia SBR dla
48 kHz stereo i nic więcej. Gdy żądanie jest mniejsze, fabryczny FDK po cichu podnosi
bitrate do tego wiersza.

Flaga zdejmuje pięć osobnych progów:

- minimum rdzenia wynoszące 40 bitów na kanał;
- regułę LD i ELD o 8 kbps na kanał oraz rezerwę transportową 208 bitów, którą te profile
  zgadują (rzeczywisty narzut nagłówków jest nadal sprawdzany wobec rzeczywistego kodera
  transportu);
- początek tabeli pasma dla LD i ELD, żeby te profile mogły użyć pasma węższego niż
  pierwszy wiersz tabeli;
- próg tabeli strojenia SBR i PS: HE-AAC, HE-AAC v2 oraz ELD z SBR poniżej tabeli
  używają ponownie jej najniższego wiersza, utrzymując tę samą siatkę SBR, podczas gdy
  żądany bitrate jest respektowany, a rdzeń AAC dostaje po prostu mniej bitów;
- regułę, że ramka niemogąca pomieścić własnej informacji pobocznej (side information)
  przerywa kodowanie.
  Takie ramki zamiast tego przestrzeliwują, a rezerwuar wchłania to w kolejnych ramkach.

Zostaje nagłówek transportu, 56 bitów na ramkę dla ADTS, oraz górny pułap 6144 bitów na
kanał, którego nie rusza żaden przełącznik. Poniżej oszacowanej zawartości SBR, czyli około
2,5 kbps na kanał plus parametric stereo, rdzeń nie miałby już żadnych bitów i koder
zatrzymuje się z czystym „encoder initialization failed”, a nie z asercją.

Flaga zdejmuje progi. Nie gwarantuje żadnego konkretnego bitrate'u. Przy takich wartościach
gęste fragmenty rutynowo wychodzą powyżej tego, o co poproszono, bo same nagłówki i
informacja poboczna kosztują więcej niż budżet, więc wynik należy oceniać po rozmiarze
pliku, a nie po liczbie w `--verbose`.

Uwaga na zmianę znaczenia `-b`. W trybie unlock liczba to dosłownie bity na sekundę.
`-b 6000` to 6000 bps. `-b 6` to 6 bps i nie zakoduje nic sensownego, a `-b 1152` wpisane
w przekonaniu, że znaczy 1152 kbps, daje 1152 bps. Zawsze trzeba tu wpisywać pełną liczbę
bitów.

Wartości:
- pominięta: wszystkie progi obowiązują, `-b` czytane ze skrótem kilobitowym (ustawienie
  fabryczne).
- `--unlock-bitrate`: progi zdjęte, `-b` to dosłowne bps.

Przykład w kontekście:

    fdkaac -p 29 -b 6000 --unlock-bitrate -f 2 -o out.aac in48k.wav

Współpraca i konflikty: `--vbr-reservoir` i `--peak-bitrate` stają się tu ciekawsze, bo
przestrzelenie jest w tym trybie normą, a rezerwuar jest tym, co za nie płaci. Przy `--dab`
konwencja kilobitowa DAB+ dla `-b` zostaje zastąpiona dosłownymi bps. Nic w grupie D ani F
nie przywraca progów.

Zmierzone: rzeczywisty bitrate obliczony z rozmiaru strumienia ADTS albo LOAS, bez narzutu
kontenera:

| Tryb | `-b` | rzeczywiste bps | dekoder FDK | ffmpeg 6.1 |
|---|---|---|---|---|
| HE-AAC 48 kHz stereo | 8000 / 6000 | 9232 / 7290 | 0 błędów | 0 błędów |
| HE-AAC 48 kHz mono | 4000 / 3000 | 4736 / 4348 | 0 | 0 |
| HE-AAC v2 48 kHz | 6000 / 4000 | 6678 / 4894 | 0 | 1 (patrz niżej) |
| ELD+SBR 48 kHz stereo | 12000 | 12984 | 0 | patrz niżej |
| ELD / LD 32 kHz stereo | 8000 | 8154 | 0 | patrz niżej |
| AAC-LC 48 kHz stereo | 4000 / 3000 | 6012 / 7054 | 0 | 0 |

Tam, gdzie rzeczywisty bitrate przekracza `-b`, chodzi o gęste ramki przestrzeliwujące na
samych nagłówkach i informacji pobocznej. Kolumnę ffmpeg 6.1 trzeba czytać ostrożnie:
raportuje te same błędy na koderze fabrycznym w tych trybach, jeden
`noise_facs_q is invalid` na HE-AAC v2 przy 12..14 kbps, co mieści się w tabelach
fabrycznych, oraz wiele błędów na każdym strumieniu LOAS z ELD albo LD przy dowolnym
bitrate. ffmpeg nie implementuje LD i ELD w pełni, więc te strumienie należy oceniać
dekoderem Fraunhofera.

### `--uncap-bandwidth`

**Zdejmuje twardy limit 20 kHz, jaki FDK nakłada na pasmo rdzenia, żeby `-w` albo
`--core-cutoff` mogły sięgnąć aż do Nyquista.**

- Dotyczy: każdego profilu z rdzeniem, który da się rozszerzyć, w praktyce AAC-LC przy
  wysokim bitrate ze źródła o wysokiej częstotliwości próbkowania. Bez `-w` albo
  `--core-cutoff` nie ma sensu.
- Wartości: flaga; domyślnie: wyłączona (obowiązuje limit 20 kHz)
- Wpisywanie: flaga

FDK ogranicza pasmo rdzenia do `min(20000, sr/2)`. Rozumowanie jest słuszne dla normalnego
materiału: powyżej 20 kHz prawie nikt nie słyszy, a bity lepiej wydać niżej. Konsekwencja
jest taka, że źródło 96 kHz zakodowane przy hojnym bitrate nadal nie koduje nic powyżej 20
kHz, niezależnie od żądań, a dodatkowa częstotliwość próbkowania kupuje tylko łagodniejsze
zbocze filtra antyaliasingowego.

Ta flaga zdejmuje ograniczenie. Sama z siebie nie robi zupełnie nic: podnosi tylko pułap,
nie rusza samego pasma. Pasmem ruszają `-w` i `--core-cutoff <hz>` (bez SBR identyczne), a
ze zdjętym limitem mogą sięgnąć do połowy częstotliwości próbkowania.

Warto mieć jasność, co się za to dostaje. Treść powyżej 20 kHz kosztuje bity, które
schodzą z pasma słyszalnego, chyba że bitrate ma realny zapas, a strumienie z pasmem poza
typowym zakresem leżą poza zwykłą praktyką, więc niektóre dekodery mogą się na nie boczyć.
To świadome włączenie i właśnie dlatego jest to osobna flaga, a nie efekt uboczny
`-w`.

Wartości:
- pominięta: pasmo rdzenia ograniczone do 20 kHz (ustawienie fabryczne).
- `--uncap-bandwidth`: ograniczenie zdjęte, `-w` i `--core-cutoff` wolne aż do `sr/2`.

Przykład w kontekście:

    fdkaac -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 200 \
        -o out.m4a in96k.wav

Współpraca i konflikty: wymaga `-w` albo `--core-cutoff`; bez SBR oba dają ten sam strumień.
Pod SBR limit 20 kHz nie ma znaczenia, bo rdzeń kończy się na starcie SBR, dużo niżej.
Naturalnie łączy się z `--ath-scale` poniżej 256, gdy jest zapas bitrate'u, bo nie ma sensu
kodować 40 kHz pasma przy progach strojonych pod niedostatek.

Zmierzone: wejście 96 kHz, AAC-LC 400 kbps, szum szerokopasmowy. Bez flagi
`--core-cutoff 40000` daje `20000 Hz` w `--verbose` i praktycznie zerową energię powyżej
20 kHz. Z flagą `--verbose` raportuje `40000 Hz`, a zdekodowany plik niesie około 10
procent swojej energii w 20..24 kHz, około 20 procent w 24..32 kHz i około 20 procent w
32..44 kHz. Ten koder nie ma w repozytorium sygnału testowego 96 kHz, więc należy to
traktować jako pomiar wcześniejszy, a nie taki, który da się odtworzyć z dostarczonego
materiału.

## B. Kodowanie stereo

Dwa kanały na mniejszą liczbę bitów zamieniają w tym koderze trzy różne maszyny, które
pracują na różnych częściach sygnału i mają różny koszt. Przed sięgnięciem po którykolwiek
przełącznik z tej grupy trzeba jasno ustalić, o którą z tych trzech maszyn chodzi.

**MS (mid/side)** to bezstratny obrót pary kanałów. Zamiast lewego i prawego koder
zapisuje w danym paśmie współczynnika skali (scale factor band, SFB) kanał mid (suma,
L+R) i kanał side (różnica, L-R). Nic nie jest wyrzucane: dekoder obraca z powrotem i
dostaje dokładnie L/R. Przy skorelowanej muzyce sygnał side jest mały, więc kwantyzuje się
do znacznie mniejszej liczby bitów niż drugi pełny kanał. MS to decyzja o ekonomii bitów,
a nie o dźwięku, i podejmowana jest osobno dla każdego pasma i każdej ramki. Dostępne w
rdzeniu AAC, czyli w LC, LD, ELD oraz w rdzeniu pod HE-AAC, dopóki ten rdzeń jest jeszcze stereo.

**IS (intensity stereo)** jest stratne i kierunkowe. W zakresie górnych pasm koder
przestaje wysyłać dwa widma i wysyła jedną obwiednię energii plus kierunek, więc dekoder
maluje oba kanały z tego samego widma, używając współczynnika panoramy. To jest tanie, a
ucho toleruje to wysoko, gdzie słabo lokalizuje drobny szczegół widmowy. To też naprawdę
usuwa informację: wszystko, co różniło lewy kanał od prawego w tych pasmach, czyli
talerze ustawione mocno na jedną stronę, efekty przestrzenne, treść w przeciwfazie,
zapada się w stronę jednego sterowanego źródła. IS także działa w rdzeniu, a koder
domyślnie obchodzi się z nim ostrożnie.

**PS (parametric stereo)** to maszyna HE-AAC v2. Rdzeń staje się mono: jedno widmo AAC,
jedno górne pasmo SBR, a obraz stereo jedzie obok jako garść parametrów na ramkę
(różnice poziomów, koherencja, opcjonalnie faza). Poniżej mniej więcej 32 kbps kupuje to
używalny obraz stereo tam, gdzie prawdziwe kodowanie dwukanałowe byłoby „zagłodzone”,
czyli brakowałoby mu bitów, więc koder zerowałby słabe pasma albo kodował je najgrubszym krokiem.
PS nie jest ulepszeniem MS ani IS, lecz je zastępuje: przy `-p 29` nie ma kanału side,
któremu można by przesunąć budżet, ani pasm intensity, które można by rozmieścić, więc
każdy przełącznik `--ms*` i `--is*` z tej grupy jest bezczynny. I odwrotnie, `--ps*` nie
robi nic poza HE-AAC v2.

W praktyce: od 96 kbps w górę MS wykonuje prawie całą pracę i stroi się podział bitów między
mid i side (`--side-bias`, `--side-knee`). Między mniej więcej 40 a 80 kbps intensity stereo
zaczyna zarabiać na siebie w górnych pasmach (`--is-aggression`, a potem przełączniki
rozmieszczenia `--is*`). Poniżej około 32 kbps w stereo rozsądną drogą są HE-AAC v2 i
parametry PS, a najciekawsze przełączniki to tam te od rozdzielczości (`--ps-env-reduce`,
`--ps-bands`).

**Jak czytać numery SFB.** Każdy przełącznik pasmowy w tej grupie liczy pasma
współczynnika skali od `0` na DOLE widma. Pasmo 0 to bas, najwyższy numer to szczyt
kodowanego zakresu. Ile pasm istnieje, zależy od profilu i częstotliwości próbkowania:
przy LC 48 kHz stereo `--verbose` wypisuje `active SFBs (long): 49`, więc dozwolone
numery pasm to 0..48. Zawsze warto przeczytać tę linię przed ustawieniem zakresu, bo
liczba zmienia się wraz z konfiguracją, a numer zapamiętany z innego kodowania może
wypadać całkiem poza widmo.

**Bloki krótkie.** Podawane numery to zawsze pasma bloku DŁUGIEGO. Ramka krótka ma własną,
dużo grubszą tablicę (około 14 pasm przy 44,1 i 48 kHz, liczonych od `0` wewnątrz każdej
grupy okien), więc ten sam numer oznaczałby tam inną częstotliwość. Dlatego koder
przenosi każdy zakres na tablicę krótką według częstotliwości: każda krawędź trafia na
najbliższą w Hz krawędź pasma krótkiego, a przy dokładnym remisie na wyższą. Pasmo krótkie
należy do zakresu, gdy leży w nim co najmniej połowa jego szerokości. Wąski zakres, który
mieści się w jednym szerokim paśmie krótkim, dostaje to pasmo. W efekcie ramki długie
i krótkie obejmują ten sam obszar widma, przy każdej częstotliwości próbkowania, w DAB+
(960/120) i przy każdym ustawieniu `--short-groups`. `--verbose` wypisuje oba przekłady,
np. `long 18..30 (2531..9000 Hz) -> short 3..7 (2250..8250 Hz)`. Do 1.4.0-beta.1 numery były
porównywane wprost: `--msbands-lo 18` zostawiało ramki krótkie całkiem bez MS, a
`--msbands 18` (MS do 2,5 kHz) dawało w ramkach krótkich MS aż do 24 kHz.

**Dlaczego zakres powyżej kodowanego pasma nie robi nic.** Liczba SFB to pełna tabela dla
profilu, ale tylko pasma do kodowanego pasma cokolwiek niosą. Pasma powyżej częstotliwości
odcięcia są zerowe, a decyzja stereo o paśmie zer nie zmienia ani jednego bitu. To
prawdziwa pułapka przy przykładach z górnym pasmem: przy domyślnym paśmie LC około
15,75 kHz `--msbands-lo 44 --msbands-hi 48` daje dokładnie ten sam strumień co
`--msmask 0`, bo pasma 44..48 leżą powyżej kodowanego pasma, a MS jest w związku z tym
wyłączone wszędzie tam, gdzie ma to znaczenie. Po otwarciu pasma siłą przez `-w 20000` te
dwa ustawienia przestają być równe. Ustawiany zakres trzeba więc sprawdzać wobec
kodowanego pasma, a nie tylko wobec liczby SFB.

Wszystkie przełączniki tej grupy są wyłączone, dopóki się ich nie poda. Wyłączony znaczy
domyślne zachowanie FDK: wartość `-1`, a dla `--side-bias` i `--side-knee` wartość `0`.
`--side-bias` i `--side-knee` przyjmują liczby dziesiętne, z `.` albo `,`, do 3 miejsc po
przecinku; `--side-cutoff` przyjmuje herce; pozostałe to liczby całkowite i odrzucają
części dziesiętne.

### MS

Sam obrót MS nic nie kosztuje w dźwięku, bo jest bezstratny. O tym, jak obraz stereo
przechodzi przez koder, decyduje podział bitów między kanał mid i kanał side w pasmach
kodowanych jako MS, a przede wszystkim decyzja, które pasma side w ogóle zostaną zakodowane.
Każde pasmo side wyzerowane w ramce to kawałek obrazu przesunięty na środek.

Przełączniki tej części dzielą się na trzy poziomy:

- **Dwie dźwignie**: `--side-bias` przesuwa granicę, przy której pasmo side zostaje albo
  wypada, a `--side-knee` stopniuje pasma leżące przy tej granicy według ich własnego
  poziomu. Szerokość obrazu wobec jakości środka ustawia się tymi dwoma, osobno albo razem.
- **Wymiana na niskie bitrate'y**: `--side-cutoff` oddaje side powyżej zadanej
  częstotliwości, a jego bity przekazuje kanałowi mid.
- **Laboratorium**: `--msmask`, `--msbands`, `--msbands-lo`, `--msbands-hi`, `--ms-bias`,
  `--ms-precision`, `--mid-bias`. Sięgają wprost do decyzji MS i do progów pasm MS. Służą
  do odtworzenia warunku technicznego albo do sprawdzenia, co robi koder. Na muzyce żaden
  z nich nie wypadł w pomiarze lepiej niż dwie dźwignie, a kilka pogarsza wynik względem
  ustawień domyślnych.

Jak to mierzono w tej części: AAC-LC stereo, dekoder Fraunhofera, plik po dekodowaniu
wyrównany w czasie do źródła i porównany w dziedzinie mid/side od 100 Hz do górnej granicy
kodowanego pasma. *Błąd mid* i *błąd side* to energia błędu kodowania względem energii
danego kanału w źródle, w dB, więc niżej znaczy czyściej. *Wyzerowane pasma side* to liczba
pasm side na ramkę zakodowanych jako zero, odczytana z `--bit-stats`. Głównym materiałem
jest akustyczna muzyka stereo 44,1 kHz; `dense48` to syntetyczny gęsty sygnał z grupy E.

#### `--side-bias <dB>`

**Przesuwa granicę, przy której pasmo side zostaje albo wypada: główna dźwignia
szerokości stereo wobec jakości środka.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD i rdzeń stereo pod HE-AAC), pasm kodowanych
  jako MS. Bez działania w HE-AAC v2, a przy `--msmask 0` nie ma na czym działać.
- Wartości: `-24.0..50.0` dB; `0` = wyłączone (domyślnie, próg FDK)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

W każdym paśmie MS koder porównuje energię sygnału side z jego progiem maskowania. Poniżej
progu pasmo side jest zerowane, powyżej jest kodowane, i to tym dokładniej, im wyżej ponad
progiem leży. Sygnał side to wszystko, czym różnią się kanały lewy i prawy, więc od tego
jednego porównania zależy szerokość obrazu. `--side-bias` przesuwa próg side w każdym paśmie
MS o podaną liczbę decybeli.

Wartość dodatnia obniża próg. Mniej pasm side jest zerowanych, pozostałe są kwantyzowane
dokładniej, a w odsłuchu obraz robi się stabilniejszy: pogłos wybrzmiewa zamiast się urywać,
a dźwięk pomieszczenia zostaje na swoim miejscu. Budżet bitów nie rośnie, więc te bity
zabiera kanał mid i przy niskim bitrate'ie słychać, że płaci za to środek. Między +3 a +9 ta
wymiana zwykle się opłaca. Powyżej około +12 side przestaje zyskiwać, a mid dalej traci.

Wartość ujemna podnosi próg: pasma side wypadają, obraz zwęża się w stronę mono, a
uwolnione bity idą do mid. Małe wartości ujemne działają płynnie. Przy około -0,97 dB jest
jednak skok. Podniesiony próg przekracza tam minimalny SNR, który mechanizm unikania dziur
w FDK utrzymuje w każdym paśmie, i pasma side, które FDK trzymało przy życiu tylko dzięki
temu ratunkowi, wypadają razem. Wielkość skoku zależy od tego, jak bardzo ramkom brakuje
bitów. Przy wygodnym bitrate'ie skok jest umiarkowany, a dalej zwężanie postępuje
stopniowo. W ramkach już „zagłodzonych” większość pasm side istnieje tylko dzięki
ratunkowi, więc skok zabiera prawie cały side, a -1, -4 i -12 dają ten sam strumień. Do
łagodnego zwężania warto zostać w przedziale od 0 do -0,95 albo użyć ujemnego
`--side-knee`, który skoku nie ma.

Przykłady (akustyczna muzyka stereo, 96 kbps; bez przełącznika: mid -20,3 dB, side
-11,4 dB, 7,0 wyzerowanych pasm side na ramkę):
- Łagodnie, więcej dla side: `--side-bias 3`. Side czystszy o 0,8 dB, mid gorszy o 0,4 dB,
  3,4 wyzerowanych pasm side.
- Mocno, więcej dla side: `--side-bias 6`. Side czystszy o 1,3 dB, mid gorszy o 1,3 dB,
  1,2 wyzerowanego pasma. Zwykła wartość robocza.
- Przesadnie, więcej dla side: `--side-bias 24`. Side znów gorszy niż przy +12, a mid
  stracił 3,3 dB: bity wlane w kanał, który nie umie ich już wykorzystać.
- Łagodnie, węziej: `--side-bias -0,5`. Kilka brzegowych pasm side oddanych; mid i side
  w granicach 0,15 dB od ustawień domyślnych.
- Mocno, węziej: `--side-bias -3`. Za skokiem: 13,8 wyzerowanych pasm side, side gorszy o
  0,9 dB, mid czystszy o 0,2 dB.
- Przesadnie, węziej: `--side-bias -12`. Wyzerowanych 26 z około 37 pasm side, błąd side
  wyższy o 5,7 dB. Na `dense48` przy 40 kbps już `-6` zostawia side 6 bitów na ramkę,
  czyli praktycznie mono.

Współpraca i konflikty: `--side-knee` działa na tej samej decyzji i łączy się z tym
przełącznikiem bez kolizji: bias ustawia granicę, knee stopniuje pasma przy niej.
`--side-cutoff` usuwa side powyżej swojej częstotliwości niezależnie od biasu.
`--msmask 0` wyłącza MS i czyni ten przełącznik bezczynnym; `--msbands`, `--msbands-lo` i
`--msbands-hi` ograniczają pasma, do których sięga.

Zmierzone (akustyczna muzyka stereo; błąd mid / błąd side w dB / wyzerowane pasma side na
ramkę):

| `--side-bias` | 48 kbps | 96 kbps |
|---|---|---|
| bez przełącznika | -21,6 / -8,5 / 16,0 | -20,3 / -11,4 / 7,0 |
| `+3` | -19,1 / -7,9 / 11,6 | -19,8 / -12,3 / 3,4 |
| `+6` | -16,5 / -11,3 / 3,5 | -19,0 / -12,7 / 1,2 |
| `+9` | -15,4 / -12,3 / 1,8 | -18,6 / -13,1 / 0,6 |
| `-1` | -22,9 / -8,6 / 18,3 | -20,4 / -11,2 / 10,0 |
| `-3` | -24,2 / -8,0 / 20,7 | -20,5 / -10,5 / 13,8 |

Przy 48 kbps `+3` koduje już więcej pasm side, ale tak zgrubnie, że błąd side jest jeszcze
o 0,6 dB gorszy niż bez przełącznika; zysk przychodzi przy `+6`. Przy 128 kbps `+6`
poprawia side z -14,2 na -16,9 dB, a mid zmienia z -28,5 na -24,8 dB. Skok: przy 96 kbps
wartość -0,96 zeruje 7,7 pasma side, a -0,97 już 9,9. Na `dense48` przy 40 kbps z
`-w 18000` side po dekodowaniu jest o 1,4 dB cichszy niż w źródle przy -0,95 i o 10,8 dB
cichszy przy -0,97, tak samo przy -1, -4 i -12.

#### `--side-knee <dB>`

**Stopniuje pasma side leżące przy granicy kodowane/wyzerowane według ich własnego
poziomu: więcej albo mniej szczegółu w side, bez skoku.**

- Dotyczy: tak samo jak `--side-bias`.
- Wartości: `-24.0..50.0` dB; `0` = wyłączone (domyślnie, zachowanie FDK)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

`--side-bias` przesuwa granicę, a ten przełącznik działa na zapasie każdego pasma side,
czyli na tym, o ile jego energia leży ponad progiem. Od zapasu zależy, jak dokładnie
pasmo jest kodowane i czy w ogóle zostaje. Wartość dodaje się do zapasu przy granicy i
wygasa wyżej, więc głośna, dobrze zakodowana część side zostaje nietknięta, a każde pasmo
zachowuje dokładność zgodną ze swoim prawdziwym poziomem.

Przełącznik istnieje z powodu jednego szczegółu FDK. Gdy ramce brakuje bitów, FDK nie
wyrzuca słabego pasma od razu: ustawia jego próg około 0,97 dB pod energią i koduje je
najgrubszym krokiem. W muzyce mniej więcej co dziesiąte pasmo side ląduje na tej jednej
wartości, niezależnie od swojego rzeczywistego poziomu. Knee czyta zapas, którego chciała
pętla dopasowania bitów przed tym ratunkiem, więc takie pasma znów są stopniowane według
prawdziwego poziomu, zamiast ruszać się jednym blokiem. Dlatego ujemny knee zwęża płynnie
tam, gdzie ujemny bias skacze.

Wartość dodatnia dodaje dokładności: pasma do N dB pod progiem zostają, stopniowane według
tego, jak daleko pod nim leżały, a pasma tuż ponad progiem są kodowane do N dB dokładniej.
Wartość ujemna odbiera dokładność: najpierw odchodzą najsłabsze pasma przy granicy, a ich
bity idą do mid. W ramkach „zagłodzonych” większość pasm side istnieje tylko dzięki
ratunkowi, więc tam już mały ujemny knee usuwa większość z nich; przy wygodnym bitrate'ie
zwęża obraz stopniowo aż do -24.

W porównaniu z `--side-bias` o tej samej wartości dodatni knee daje podobny zysk w side
mniejszym kosztem mid. Przy 96 kbps `+6` daje side -12,9 dB i mid -19,4 dB, a bias -12,7 i
-19,0; przy 128 kbps knee daje -16,8 i -25,7, a bias -16,9 i -24,8. Knee pasuje do
materiału, w którym najważniejszy jest środek, bias do materiału, w którym liczy się
szerokość, a razem ustawiają granicę i łagodzą jej krawędź.

Przykłady (akustyczna muzyka stereo, 96 kbps; wartości odniesienia jak wyżej):
- Łagodnie, dokładniej: `--side-knee 2`. Wyzerowane pasma side z 7,0 do 3,4, side czystszy
  o 0,5 dB, mid gorszy o 0,2 dB.
- Mocno, dokładniej: `--side-knee 6`. 1,2 wyzerowanego pasma, side czystszy o 1,5 dB, mid
  gorszy o 0,8 dB.
- Przesadnie, dokładniej: `--side-knee 24`. Więcej bitów trafia do side niż do mid, a oba
  kanały wychodzą gorzej niż przy +12.
- Łagodnie, zgrubniej: `--side-knee -1`. Odchodzi około pięciu najsłabszych pasm side
  (11,8 wyzerowanych), mid czystszy o 0,1 dB.
- Mocno, zgrubniej: `--side-knee -6`. 19 wyzerowanych pasm side, side gorszy o 2,2 dB, mid
  czystszy o 0,3 dB. Wyraźnie węziej, ale wciąż stereo.
- Przesadnie, zgrubniej: `--side-knee -24`. Znika 36 z 37 pasm side, side jest o 8,9 dB
  cichszy niż w źródle: obraz mono z księgowością stereo.

Przełączniki diagnostyczne, zostawione do pomiarów i możliwe do usunięcia w kolejnej
wersji; bez `--side-knee` nie robią nic:
- `--side-knee-zone <x>`, `1.0..20.0`, domyślnie `3`: przesunięcie działa w pełni do
  |knee| dB ponad progiem i wygasa liniowo do zera przy x razy |knee| dB.
- `--side-knee-spread <dB>`, `0.1..24.0`, domyślnie `1`: przy ujemnym knee, ile decybeli
  knee trzeba, żeby uratowane pasma rozłożyć w pełni według ich prawdziwego zapasu.

Współpraca i konflikty: zaprojektowany do używania razem z `--side-bias`. Bezczynny przy
`--msmask 0`.

Zmierzone: przy 96 kbps liczba kodowanych pasm side na ramkę spada z ujemnym knee
równomiernie: 30,2 (bez przełącznika), 29,4 (-0,25), 27,6 (-0,5), 25,4 (-1), 22,2 (-3), 18,1
(-6), 10,5 (-12), 1,1 (-24). Na `dense48` przy 40 kbps z `-w 18000`, gdzie ramki są
„zagłodzone”, -0,25 zostawia 1,1 z 10,3 kodowanych pasm side, a od -4 strumień już się nie
zmienia. Przełączniki diagnostyczne ruszają niewiele: `--side-knee 6` ze strefą 1 i 10 różni
się o 0,06 dB błędu side, `--side-knee -3` z rozłożeniem 0,1 i 10 o 0,15 dB.

#### `--side-cutoff <Hz>`

**Od tej częstotliwości w górę koduje każde pasmo stereo jako czysty mid: górę obrazu
wymienia na czystszy środek.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD). Domyślnie wyłączone.
- Wartości: `1000..24000` Hz, przesuwane do pasma współczynnika skali, które zaczyna się
  na tej częstotliwości albo wyżej
- Wpisywanie: liczba całkowita, w Hz

Powyżej granicy sygnał side jest porzucany, a pasmo zapisywane tylko jako mid, z tanią maską
MS; czystego L/R tam nie ma. Poniżej granicy stereo jest nietknięte. Bity, które zabrałby
side, dostaje mid, a to ma znaczenie przy niskim bitrate'ie, gdy side powyżej kilku kHz i
tak jest w większości zerowany, a to, co z niego zostaje, jest zgrubne.

Poziom zostaje. Bez side oba kanały grają mid, który sam niesie mniej energii niż kanały
oryginału, gdy tylko L i R się różnią (nieskorelowane L/R wyszłyby o 3 dB ciszej). Dlatego
mid jest podnoszony do energii oryginału, najwyżej o 6 dB i tylko tam, gdzie side leży ponad
progiem maskowania (masker, czyli sygnał maskujący); side, który FDK i tak by wyzerowało,
nie jest dodawany. Dwóch rzeczy mid nie przeniesie przy żadnym poziomie: treści w
przeciwfazie (L = -R, jej mid to zero) i kierunku twardo spanoramowanego dźwięku, który
ląduje na środku.

Intensity stereo powyżej granicy: własna decyzja FDK o IS jest tam kasowana, bo czysty mid
jest tańszy. IS zostaje, gdy prosi się o nie jawnie:
- `--is-force-lo/-hi`: IS na podanych pasmach, zawsze.
- poluzowane kryteria IS (`--is-aggression`, `--is-corr-thresh`, `--is-lr-ratio`,
  `--is-min-sfbs`, `--is-admit` powyżej 5): własna decyzja FDK z tymi kryteriami.
  Pojedyncze spanoramowane obszary powyżej granicy zachowują wtedy kierunek (IS niesie go
  w każdym paśmie), a reszta zostaje mid, jak w intensity stereo z MPEG-1 Layer II.

Pasma, które oba kanały kodują jako PNS, zostają bez zmian. `--verbose` pokazuje pasmo, od
którego granica działa, w tablicy długiej i krótkiej. Obie zaokrąglają w górę do najbliższej
krawędzi pasma, więc w grubej tablicy krótkiej granica może przesunąć się tylko w górę (9
kHz: pasmo długie 31 na 9,0 kHz, krótkie 9 na 10,5 kHz), czyli usuwa mniej side, nigdy
więcej.

Przykłady (akustyczna muzyka stereo, 48 kbps):
- Łagodnie: `--side-cutoff 8000`. Informacja poboczna z 238 do 235 bitów na ramkę, mid
  odrobinę czystszy.
- Mocno: `--side-cutoff 5000`. Informacja poboczna 222 bity, błąd mid niższy o 0,7 dB, side
  powyżej 5 kHz niżej o około 27..31 dB.
- Przesadnie: `--side-cutoff 1000`. Praktycznie mono ze stereofonicznym basem.

Współpraca i konflikty: `--side-bias` i `--side-knee` działają dalej poniżej granicy. Przy
14 kbps zysk jest mały, bo FDK i tak zeruje tam prawie cały side; większą dźwignią jest
wtedy `--is-admit 0`.

Zmierzone: na `dense48` przy 40 kbps `8000` zostawia mid bez zmian, a side obniża o 1,2 dB;
`5000` daje mid czystszy o 1,3 dB i side cichszy o 3,4 dB. Ze źródłem 3..6 kHz
panoramowanym na przemian 4 dB w lewo i w prawo na tle miksu mono (40 kbps,
`--side-cutoff 1000`) sama granica zostawia 0 dB panoramy, a z `--is-aggression 100`
3..4 dB, tak jak w źródle.

### Laboratorium MS

Te przełączniki sięgają wprost do decyzji MS i do progów pasm MS. Odpowiadają na pytania
techniczne (co się dzieje w czystym L/R, przy MS w stałym zakresie pasm, przy decyzji
przechylonej w jedną stronę) i służą eksperymentom. Na muzyce żaden z nich nie daje po
dekodowaniu wyraźnie lepszego wyniku niż ustawienia domyślne, a kilka go pogarsza. Trzy z
nich przyjmują liczby Q8, czyli wewnętrzną skalę stałoprzecinkową FDK, w której `256`
oznacza 1,0 (patrz *Wspólne zasady*).

Przy CBR trzy przełączniki progów (`--ms-bias`, `--ms-precision`, `--mid-bias`) w
większości odkręca pętla dopasowania bitów, która i tak musi trafić w tę samą liczbę bitów
na ramkę. Przy VBR (`-m 1..5`) liczba bitów idzie za progami, więc tam `--ms-precision` i
`--mid-bias` słyszalnie działają, i na jakość, i na rozmiar pliku.

#### `--msmask <n>`

**Wymusza decyzję mid/side we wszystkich pasmach, zamiast zostawić koderowi wybór pasmo
po paśmie.**

- Dotyczy: rdzenia AAC w stereo (LC, LD, ELD i rdzeń stereo pod HE-AAC). Bez działania w
  HE-AAC v2, gdzie rdzeń jest mono.
- Wartości: `-1` automatycznie (domyślnie, FDK decyduje dla każdego pasma), `0` wyłączone
  (czyste L/R wszędzie), `1` włączone (MS w każdym paśmie)
- Wpisywanie: liczba całkowita

Koder zwykle sprawdza każde pasmo na oba sposoby i zostawia tańszy. W skorelowanej muzyce to
MS w niemal każdym paśmie, a ponieważ obrót jest bezstratny, wybór nic nie kosztuje w
dźwięku. Wymuszenie w którąkolwiek stronę to narzędzie techniczne.

`--msmask 0` gwarantuje, że koder nigdy nie zmiesza kanałów. Szum kwantyzacji zostaje
wtedy w kanale, który go wywołał, zamiast dzielić się między mid i side i trafiać do obu
wyjść. Ma to znaczenie, gdy plik pójdzie później przez usuwanie środka, wycinanie wokalu
albo inne odejmowanie jednego kanału od drugiego. Przy tym samym bitrate'ie kosztuje
jakość, wyraźnie w skorelowanej muzyce.

`--msmask 1` wymusza MS także tam, gdzie L/R byłoby tańsze. Przydaje się jako punkt
odniesienia; przy materiale o naprawdę niezależnych kanałach, na przykład dwóch
niezwiązanych źródłach rozstawionych na skraje, marnuje bity.

Wartości:
- `-1`: domyślnie, decyzja dla każdego pasma z porównania kosztów w koderze.
- `0`: czyste L/R w każdym paśmie. Nic nie jest mieszane między kanałami.
- `1`: MS w każdym paśmie.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --msmask 0 -o lr.m4a in.wav

Współpraca i konflikty: `--msmask 0` czyni bezczynną całą rodzinę mid/side, w tym
`--side-bias`, `--side-knee`, `--msbands` i `--mid-bias`, bo żadne pasmo nie jest w MS.
`--msbands`, `--msbands-lo` i `--msbands-hi` zawężają obszar, w którym maska obowiązuje.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msmask 0` podnosi błąd mid o 1,3 dB i błąd
side o 3,9 dB względem ustawień domyślnych. `--msmask 1` mieści się w 0,4 dB od nich, bo FDK
i tak koduje 39,5 z około 40 pasm na ramkę jako MS.

#### `--msbands <n>`

**Dopuszcza MS tylko w N najniższych pasmach współczynnika skali, a wszystko wyżej
zostawia w L/R.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo `0..N`, gdzie N to liczba aktywnych SFB; `n`
  oznacza, że pasma `0..n-1` mogą używać MS
- Wpisywanie: liczba całkowita

To ograniczenie w postaci „od dołu do”, liczone od basu w górę: `--msbands 6` pozwala pasmom
0..5 używać MS, a pasma 6 i wyższe trzyma w L/R. `--msbands 0` nie dopuszcza MS nigdzie, tak
samo jak `--msmask 0`. Od pary `--msbands-lo` i `--msbands-hi` różni się tylko kształtem
obszaru: ten przełącznik zawsze zaczyna od pasma 0.

Powód, żeby go użyć, jest techniczny: późniejszy proces, który wymaga nietkniętego L/R
powyżej jakiejś częstotliwości. Powodu jakościowego nie ma: każde zmierzone dotąd
ograniczenie pogarszało oba kanały przy tym samym bitrate'ie.

Przykłady:
- Łagodnie: `--msbands 40`. Do L/R wracają tylko najwyższe pasma; na większości
  materiału strumień prawie się nie zmienia, bo te pasma niosą niewiele.
- Mocno: `--msbands 20`. MS tylko w dolnej części widma; przy 96 kbps na muzyce
  akustycznej błąd mid rośnie o 1,3 dB, a błąd side o 3,5 dB.
- Przesadnie: `--msbands 2`. MS praktycznie nigdzie: niemal pełna cena czystego L/R, a MS
  wciąż sygnalizowane w dwóch pasmach.

Współpraca i konflikty: równoważne z `--msbands-lo 0 --msbands-hi n-1` (`--msbands 6` i
`--msbands-lo 0 --msbands-hi 5` dają ten sam strumień). Bezczynne przy `--msmask 0`. Limit
powyżej liczby aktywnych SFB działa jak brak limitu.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msbands 30` podnosi błąd mid o 0,9 dB i
błąd side o 2,4 dB, `--msbands 20` o 1,3 i 3,5 dB.

#### `--msbands-lo <n>`

**Najniższe pasmo współczynnika skali w zakresie, w którym wolno używać MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  podawaj razem z `--msbands-hi`
- Wpisywanie: liczba całkowita

Razem z `--msbands-hi` wyznacza okno od..do: MS wolno użyć w pasmach `lo..hi` włącznie i
nigdzie poza nimi. W odróżnieniu od `--msbands` okno nie musi zaczynać się od dołu widma,
więc tylko w ten sposób da się trzymać MS z dala od basu. Podawaj obie połówki pary; samo
`--msbands-lo` zostawia górną krawędź nieustawioną, a to nie jest zakres.

Przykłady:
- Łagodnie, okno od dołu: `--msbands-lo 2 --msbands-hi 48`. Prawie wszystko może używać MS,
  wykluczone są dwa najniższe pasma.
- Mocno, okno w środku: `--msbands-lo 10 --msbands-hi 30`. MS tylko w środku pasma, bas i
  góra w L/R.
- Przesadnie, okno na samej górze: `--msbands-lo 44 --msbands-hi 48` z `-w 20000`. MS
  tylko w pięciu pasmach samego powietrza. Bez `-w 20000` te pasma leżą powyżej domyślnej
  granicy około 15,75 kHz i ustawienie zamienia się w `--msmask 0` (patrz *Dlaczego zakres
  powyżej kodowanego pasma nie robi nic*).

Współpraca i konflikty: zakres trzeba czytać wobec `active SFBs` z `--verbose` i wobec
kodowanego pasma. Bezczynne przy `--msmask 0`.

Zmierzone (akustyczna muzyka stereo, 96 kbps): `--msbands-lo 20 --msbands-hi 48` podnosi
błąd mid o 0,5 dB, a błąd side o 0,7 dB.

#### `--msbands-hi <n>`

**Najwyższe pasmo współczynnika skali, włącznie, w zakresie, w którym wolno używać MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  podawaj razem z `--msbands-lo`
- Wpisywanie: liczba całkowita

Górna krawędź opisanego wyżej okna, włącznie: `--msbands-hi 5` obejmuje pasmo 5, a pasma
powyżej idą do L/R. W blokach krótkich zakres obejmuje te same częstotliwości w tablicy
krótkiej (patrz *Bloki krótkie* wyżej), więc `--msbands-lo 18 --msbands-hi 30` przy 48 kHz
to pasma krótkie 3..7.

Przykłady:
- Łagodnie: `--msbands-lo 0 --msbands-hi 44`. Cztery najwyższe pasma wyłączone z MS.
- Mocno: `--msbands-lo 0 --msbands-hi 20`. MS mniej więcej do zakresu prezencji, wyżej
  L/R.
- Przesadnie: `--msbands-lo 0 --msbands-hi 0`. MS w dokładnie jednym paśmie, w basie.

Współpraca i konflikty: `--msbands-lo` to druga połówka; bez niej nie ma zakresu. Wartość
powyżej liczby aktywnych SFB działa jak brak górnego limitu. Bezczynne przy `--msmask 0`.

#### `--ms-bias <n>`

**Przechyla porównanie kosztów L/R i MS w stronę MS.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `0..255`, Q8 w logarytmicznych jednostkach kosztu FDK; `-1` i `0` = domyślnie
- Wpisywanie: liczba całkowita

Dla każdego pasma koder szacuje, ile kosztowałoby L/R, a ile MS, i wybiera taniej. Ten
przełącznik dodaje premię stronie MS. Ponieważ MS jest bezstratne, porównanie decyduje tylko
o sposobie zapisu pasma, a nie o jego brzmieniu, a FDK i tak wybiera MS w niemal każdym
paśmie skorelowanej muzyki.

Użyteczny zakres jest malutki. Wartości 1, 2 i 4 dają różne strumienie. Od około 8 (16 na
gęstym materiale) premia przewyższa każdą rzeczywistą różnicę kosztów, każde pasmo, któremu
wolno używać MS, je dostaje, a wyższe wartości nic już nie zmieniają: wynik jest bliski
`--msmask 1`.

Wartości:
- `-1` albo `0`: porównanie kosztów jak w FDK.
- `1..8`: decyzja coraz mocniej przechylona w stronę MS.
- `8..255`: MS wszędzie, gdzie wolno, ten sam strumień dla każdej wartości.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --ms-bias 4 -o biased.m4a in.wav

Współpraca i konflikty: bez sensu razem z `--msmask 0` albo `--msmask 1`, które usuwają
decyzję. Do wszystkiego, co słychać, służą `--side-bias` i `--side-knee`.

Zmierzone: akustyczna muzyka stereo przy 96 kbps, `128` podnosi błąd mid o 0,3 dB i nie
zmienia side; na `dense48` przy 40 kbps `255` nie zmienia niczego mierzalnego.

#### `--ms-precision <n>`

**Obniża progi każdego pasma MS, mid i side razem, więc oba kanały są kodowane
dokładniej.**

- Dotyczy: rdzenia AAC w stereo, tylko pasm MS. Bez działania w HE-AAC v2.
- Wartości: Q8 od `256` (bez zmian) w górę, `384` to 1,5x, `512` to 2x; poniżej 256 jest
  odrzucane; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

Progi pasma w MS są dzielone przez podany współczynnik, więc koder uznaje za słyszalną
większą część obu kanałów. Przesuwa mid i side w tę samą stronę o tyle samo, więc nie
zmienia równowagi między nimi; od równowagi są `--side-bias` i `--side-knee`.

Przy CBR prawie nic nie robi. Ramka dostaje tyle samo bitów, pętla dopasowania podnosi progi
z powrotem, aż ramka się zmieści, a wynik po dekodowaniu zostaje w granicach kilku
dziesiątych decybela od ustawień domyślnych. Przy VBR liczba bitów idzie za progami i tam
ten przełącznik podnosi jakość pary stereo, płacąc rozmiarem pliku.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo, bez przełącznika około 108 kbps):
- Łagodnie: `--ms-precision 384`. Plik większy o 3,1%, błąd mid niższy o 0,5 dB, side o
  0,4 dB.
- Mocno: `--ms-precision 512`. Plik większy o 5,3%, błąd mid niższy o 0,8 dB, side o
  0,7 dB.
- Przesadnie: `--ms-precision 2048`. Plik większy o 15,2%, błędy niższe o 2,2 i 1,9 dB:
  zakamuflowane podniesienie bitrate'u, które `-m` robi równiej.

Współpraca i konflikty: bezczynne przy `--msmask 0`. Przy VBR wyższe `-m` podnosi jakość we
wszystkich pasmach, nie tylko w MS.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `512` obniża błąd side o 0,13 dB,
`4096` o 0,4 dB, a mid nie zmienia się wcale; na `dense48` przy 40 kbps `1024` nie zmienia
niczego mierzalnego. VBR jak w przykładach.

Pod maską: współczynnik jest nakładany jako przesunięcie progów pasm w dziedzinie
logarytmicznej FDK, zaraz po obrocie MS.

#### `--mid-bias <n>`

**Podnosi próg kanału mid w pasmach MS, więc mid jest kodowany zgrubniej.**

- Dotyczy: rdzenia AAC w stereo, pasm MS. Bez działania w HE-AAC v2.
- Wartości: Q8, `256` = bez zmian, powyżej 256 podnosi próg mid; poniżej 256 jest
  odrzucane; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

Pomysł polega na oddaniu bitów z mid na rzecz side. W pomiarze nie wychodzi to dobrze. Przy
CBR pętla dopasowania odkręca większość efektu: przy 96 kbps `384` przesuwa mid i side o
około 0,1 dB, a dopiero `4096` daje side pełny decybel za 0,65 dB mid. Przy VBR mid po
prostu robi się zgrubniejszy, a plik mniejszy, i side nic nie zyskuje. Do szerokości wobec
środka służą `--side-bias` i `--side-knee`.

Przykłady (CBR, akustyczna muzyka stereo, 96 kbps):
- Łagodnie: `--mid-bias 288`. Bez mierzalnej zmiany.
- Mocno: `--mid-bias 1024`. Przy CBR side czystszy o 0,4 dB, mid gorszy o 0,2 dB; przy VBR
  `-m 3` plik mniejszy o 6,8%, mid gorszy o 1,7 dB, side bez zmian.
- Przesadnie: `--mid-bias 4096`. Side czystszy o 1 dB, mid gorszy o 0,65 dB. Dla
  porównania `--side-knee 3` daje side 0,8 dB za 0,3 dB mid.

Współpraca i konflikty: bezczynne przy `--msmask 0`. Razem z dodatnim `--side-bias` ciągnie
ten sam budżet z drugiego końca.

Zmierzone: na `dense48` przy 40 kbps `512` i `1024` nie zmieniają niczego mierzalnego. VBR
jak w przykładach.

### Intensity stereo (IS)

Intensity stereo to ta stratna maszyna. W ciągu górnych pasm koder przestaje wysyłać dwa
widma i wysyła jedną obwiednię energii plus kierunek, a dekoder odbudowuje oba kanały z tego
jednego widma. Oszczędność jest duża, kosztem jest separacja stereo dokładnie w tych
pasmach, a słyszalność tego kosztu zależy wyłącznie od tego, co tam żyje: skorelowana kąpiel
talerzy znosi to dobrze, mocno rozpanoramowany efekt albo szeroki pogłos nie.

Fabryczny FDK jest z IS bardzo ostrożny. Warunków dopuszczenia (gate) jest nie jeden, ale
kilka, a pasmo jest kodowane jako intensity tylko wtedy, gdy wszystkie są spełnione naraz:
warunek bitrate'u na pasmo, warunek korelacji między kanałami, minimalny ciąg przyległych
pasm oraz stabilny kierunek. Właśnie dlatego przesunięcie jednego progu tak często wygląda,
jakby nie robiło nic: kolejny warunek nadal blokuje. Samo rozluźnienie `--is-corr-thresh` na
przykład nie otwiera IS.

**Warto zacząć od `--is-aggression` z grupy A.** To prosty suwak dokładnie do tego
problemu: jedna wartość od 0 do 100, która rusza wszystkie warunki razem i spójnie. 0 to
ustawienie fabryczne, co w praktyce oznacza, że przy normalnych bitrate'ach IS prawie się
nie włącza, a 100 to prawdziwe maksimum: warunek bitrate'u porzucony, wymóg korelacji
rozluźniony, minimalny ciąg zredukowany do jednego pasma i tolerancja kierunku poszerzona.
Najpierw ustaw około 30 do 50 i posłuchaj, a potem idź wyżej, jeśli oszczędność jest warta
więcej niż szerokość. Dopiero po poznaniu działania suwaka warto sięgać po przełączniki
poniżej, które przesłaniają to, co on ustawił.

#### `--is-admit <x>`

**Przesuwa pierwszy warunek dopuszczenia w FDK, który decyduje, czy intensity stereo w
ogóle wolno użyć.**

- Dotyczy: rdzenia AAC w stereo. Bez działania w HE-AAC v2.
- Wartości: `0.0..20.0`, domyślnie w FDK `5`; `0` = nigdy; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

FDK dopuszcza intensity stereo tylko wtedy, gdy bitrate podzielony przez pasmo rdzenia, w
bitach na sekundę na herc, jest mniejszy od tej wartości. Przy bardzo niskich bitrate'ach IS
jest drogie: wymusza maskę MS pasmo po paśmie (około 22 bity na ramkę) i dokłada pozycje IS,
więc zamknięcie tego warunku potrafi uwolnić więcej bitów, niż IS oszczędza.
`--is-aggression` otwiera go niezależnie od tej wartości.

Zmierzone: przy 14 kbps stereo na gęstym materiale `--is-admit 0` zmniejsza udział
wyzerowanych pasm z 85% do 70%.

#### `--is <n>`

**Włącza albo wyłącza intensity stereo globalnie, zamiast zostawiać tę decyzję koderowi.**

- Dotyczy: rdzeń AAC w stereo (LC, LD, ELD oraz rdzeń stereo pod HE-AAC). Brak działania
  w HE-AAC v2.
- Wartości: `-1` auto (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

To przełącznik główny, i jest pozwoleniem, a nie rozkazem: `1` dopuszcza intensity stereo,
ale go nie rozmieszcza. Opisane wyżej warunki nadal decydują, które pasma naprawdę idą do
IS, więc samo `--is 1` przy materiale, który koder uznaje za nieodpowiedni, albo przy
bitrate'ie powyżej progu dopuszczenia, może zostawić strumień bez zmian. Aby IS naprawdę
zadziałało, trzeba połączyć ten przełącznik z `--is-aggression`.

`--is 0` to użyteczny koniec tego przełącznika samego w sobie. Gwarantuje brak
jakiegokolwiek kodowania intensity, czyli dokładnie to, co jest potrzebne, gdy separacja
stereo w górnych oktawach liczy się bardziej niż bity, albo gdy trzeba odizolować efekt
innego przełącznika i nie dopuścić, by IS ruszało się pod pomiarem.

Wartości:
- `-1`: ustawienie fabryczne. Koder decyduje, ostrożnie.
- `0`: intensity stereo nigdy nie użyte. Wszędzie pełne kodowanie L/R albo MS.
- `1`: intensity stereo dopuszczone, z zachowaniem zwykłych warunków.

```
fdkaac -p 2 -b 64000 --is 1 --is-aggression 40 in.wav -o is.m4a
```

Współpraca i konflikty: `--is-aggression` jest tym, co czyni `--is 1` widocznym.
`--is-force-lo`/`--is-force-hi` obchodzą te warunki w całości i działają nawet bez tego
przełącznika. W pełni niezależne stereo to `--msmask 0 --is 0`.

#### `--isbands <n>`

**Ogranicza liczbę pasm współczynnika skali, które mogą być kodowane jako intensity
stereo.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo liczba w granicach liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Pułap na całkowity zasięg kodowania intensity, wyrażony jako liczba pasm, a nie jako pozycja
w widmie. To hamulec na wypadek, gdy IS pomaga bitrate'owi, ale szkody mają zostać
ograniczone: koder sam wybiera, gdzie idzie intensity, lecz nie może pochłonąć połowy widma.

Ten pułap trzeba czytać wobec `active SFBs` z `--verbose`: przy LC 48 kHz stereo jest 49
pasm, więc pułap 40 jest ledwie pułapem, a pułap 4 jest surowy. Pasma za pułapem są kodowane
normalnie.

W ramkach krótkich pułap obowiązuje w każdej grupie okien osobno (każda grupa to odcinek
czasu z własną kopią tablicy pasm) i jest przeliczany na grubszą tablicę: przy 48 kHz
`--isbands 6` pozwala na 2 pasma krótkie w grupie. Do 1.4.0-beta.1 liczył przez grupy po
kolei, więc pierwsze grupy ramki transjentowej go zużywały, a reszta ramki zostawała
całkiem bez IS.

Przykłady:
- Łagodny pułap: `--isbands 24`. Połowa widma nadal może pójść w intensity. Przy
  większości materiału zmienia to niewiele, bo IS rzadko sięga tak daleko.
- Mocny pułap: `--isbands 8`. Intensity utrzymane w krótkim ciągu. Przydatne przy wysokim
  `--is-aggression`, gdzie powstrzymuje suwak od ucieczki.
- Przesadzony pułap: `--isbands 1`. Jedno jedyne pasmo intensity. Ustawienie skrajne:
  sygnalizacja zostaje, a oszczędność znika praktycznie w całości.

Współpraca i konflikty: `--is-lo`/`--is-hi` ograniczają, gdzie IS może pójść, a ten
ogranicza, ile. Łączą się. Bezczynne, gdy IS nigdy się nie włącza, więc najpierw warto
sprawdzić działanie z `--is-aggression`.

#### `--is-min-sfbs <n>`

**Minimalny ciąg przyległych pasm, zanim intensity stereo może się zacząć.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` domyślnie, co oznacza 6, albo liczba pasm
- Wpisywanie: liczba całkowita

Jeden z warunków dopuszczenia. Koder odmawia rozpoczęcia kodowania intensity na odosobnionym
paśmie czy dwóch; wymaga ciągłego obszaru o długości co najmniej tylu pasm, z których
wszystkie spełniają pozostałe warunki. Uzasadnienie jest takie, że krótki fragment intensity
otoczony pasmami kodowanymi normalnie to słyszalny szew, mały obszar, w którym obraz nagle
zapada się, a potem znów otwiera.

Po obniżeniu tej liczby IS łapie także krótkie fragmenty, czyli wyzwala się znacznie
częściej i na większej liczbie materiału, a ryzykiem są właśnie te szwy. Po podniesieniu IS
staje się rzadsze: kwalifikują się tylko szerokie, konsekwentnie skorelowane obszary, co
jest ustawieniem zachowawczym.

To przełącznik zaawansowany. `--is-aggression` już nim rusza w ramach swojego
skoordynowanego przebiegu, więc warto go używać tylko przy ręcznym strojeniu z agresją na 0,
pamiętając, że jawna wartość przesłania to, co ustawił suwak.

Przykłady:
- Łagodnie, więcej IS: `--is-min-sfbs 4`. Kwalifikują się nieco krótsze obszary. IS trochę
  częściej.
- Mocno, więcej IS: `--is-min-sfbs 2`. Krótkie fragmenty przyjmowane. Wyraźnie więcej
  kodowania intensity i pierwszy punkt, w którym szwy stają się prawdopodobne.
- Przesadnie, więcej IS: `--is-min-sfbs 1`. Każde pojedyncze pasmo może stać się
  intensity. Ustawienie skrajne: intensity rozsypane po widmie po jednym paśmie.
- Łagodnie, mniej IS: `--is-min-sfbs 8`. Nieco rzadziej niż przy ustawieniu fabrycznym.
- Mocno, mniej IS: `--is-min-sfbs 16`. Kwalifikują się tylko szerokie obszary; przy
  większości materiału IS prawie nigdy nie odpala.
- Przesadnie, mniej IS: `--is-min-sfbs 40`. Tak długi ciąg praktycznie nie istnieje, więc
  jest to intensity stereo wyłączone kuchennymi drzwiami. Skrajna postać ustawienia
  zachowawczego, a `--is 0` mówi to samo wprost.

Współpraca i konflikty: to jeden warunek z kilku, więc samo jego obniżenie może nic nie
zmienić, jeśli warunek korelacji albo bitrate'u nadal blokuje. Faktycznie przesłonięty przez
`--is-force-lo`/`-hi`, które go obchodzą. Lepszym wyborem jest `--is-aggression`.

#### `--is-corr-thresh <n>`

**Określa, jak podobne muszą być oba kanały w paśmie, żeby intensity stereo było w ogóle
dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = korelacja 1,0; `-1` domyślnie, czyli około 243, co oznacza
  mniej więcej 0,95
- Wpisywanie: liczba całkowita

Warunek korelacji, a przy tym ten, po który sięga się pierwszy. Fabryczne strojenie stawia
ostry wymóg: oba kanały muszą być w paśmie podobne w jakichś 95 procentach, zanim koder
rozważy zastąpienie ich jedną obwiednią i kierunkiem. Bardzo niewiele prawdziwej muzyki jest
tak skorelowane w górnych pasmach, i to właśnie jest główny powód, dla którego IS wydaje się
domyślnie nieaktywne.

Kierunek jest przeciwny intuicji, więc warto to przeczytać dwa razy: NIŻEJ znaczy WIĘCEJ
intensity. Poprzeczka, którą materiał ma przeskoczyć, zostaje obniżona, więc przeskakuje ją
więcej pasm. 180 to około 0,70, czyli znacznie luźniej niż fabrycznie. Za nisko i naprawdę
różne kanały zostają zakodowane jako jedno sterowane źródło, co słychać jako błędy kierunku
i jako zlepianie się góry obrazu.

To jeden z kilku warunków, a stąd praktyczny wniosek, który warto powiedzieć wprost: zmiana
samego tego przełącznika nie otwiera intensity stereo. Jeśli nadal blokuje warunek bitrate'u
albo minimalna seria, strumień nie ruszy się niezależnie od ustawionej tu wartości.

Przykłady:
- Łagodnie, więcej IS: `--is-corr-thresh 230`. Około 0,90. Nieco luźniej niż fabrycznie.
- Mocno, więcej IS: `--is-corr-thresh 180`. Około 0,70. Kwalifikuje się istotnie więcej
  pasm; to typowy cel ręcznego strojenia.
- Przesadnie, więcej IS: `--is-corr-thresh 60`. Około 0,23, czyli przyjmowane są kanały
  ledwo ze sobą związane. Ustawienie skrajne: słyszalne błędy kierunku wszędzie, gdzie
  to zadziała.
- Łagodnie, mniej IS: `--is-corr-thresh 250`. Ciaśniej niż fabrycznie, IS trochę rzadsze.
- Mocno, mniej IS: `--is-corr-thresh 254`. Kwalifikują się tylko kanały niemal identyczne.
- Przesadnie, mniej IS: `--is-corr-thresh 256`. Wymagana korelacja idealna, której
  prawdziwy materiał w praktyce nigdy nie osiąga. Ustawienie skrajne: intensity stereo
  wyłączone niemożliwym testem, podczas gdy `--is 0` byłoby jaśniejsze.

Współpraca i konflikty: samo z siebie nie otwiera IS, bo pozostałe warunki wciąż obowiązują.
Przesuwane automatycznie przez `--is-aggression`, a podana tu jawna wartość przesłania
suwak. Pomijane przez `--is-force-lo`/`-hi`.

#### `--is-lr-ratio <n>`

**Określa, jak równa musi być w paśmie równowaga energii lewo/prawo, żeby intensity
stereo było dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = 1,0; `-1` domyślnie, czyli około 179, co oznacza mniej więcej
  0,7
- Wpisywanie: liczba całkowita

Warunek panoramy. Korelacja mówi, że oba kanały mają ten sam kształt; ten warunek mówi, że
mają porównywalny poziom. Pasmo, w którym jedna strona nosi niemal wszystko, jest źle
wybranym miejscem na kodowanie intensity, bo jedno wspólne widmo plus kierunek musi
przedstawić mocno niesymetryczną parę, a cały ciężar spada na parametr kierunku.

Niższe wartości dopuszczają pasma bardziej niesymetryczne i dlatego dają więcej kodowania
intensity, w tym samym przeciwnym do intuicji kierunku co próg korelacji. Wyższe wartości
wymagają równiejszego balansu i czynią IS rzadszym.

To trzeci z trzech progów do ręcznego strojenia i ten, który ustawia się na końcu. Kolejność
pracy: korelacja, minimalna seria, potem proporcja. Dwa pierwsze decydują, czy IS w ogóle
zadziała, ten głównie decyduje, gdzie.

Przykłady:
- Łagodnie, więcej IS: `--is-lr-ratio 160`. Trochę większa tolerancja na pasma nierówne.
- Mocno, więcej IS: `--is-lr-ratio 110`. Przyjmowane są pasma wyraźnie niesymetryczne,
  więcej intensity w całym widmie.
- Przesadnie, więcej IS: `--is-lr-ratio 20`. Przyjmowana praktycznie dowolna asymetria.
  Ustawienie skrajne: materiał spanoramowany skrajnie zostaje sprowadzony do jednego
  widma plus wartości panoramy, a to dokładnie ta sytuacja, w której IS brzmi najgorzej.
- Łagodnie, mniej IS: `--is-lr-ratio 200`. Trochę surowiej niż fabrycznie.
- Mocno, mniej IS: `--is-lr-ratio 240`. Kwalifikują się tylko pasma dobrze zbalansowane;
  IS staje się rzadkie na czymkolwiek z szerokim miksem.
- Przesadnie, mniej IS: `--is-lr-ratio 256`. Wymagana dokładna równość poziomów.
  Ustawienie skrajne, które wyłącza IS warunkiem niespełnialnym przez żaden materiał.

Współpraca i konflikty: to jeden z kilku warunków; sam rzadko zmienia strumień. Przesuwany
przez `--is-aggression`, przesłaniana przez jawną wartość. Pomijana przez
`--is-force-lo`/`-hi`.

#### `--is-lo <sfb>`

**Dopuszcza intensity stereo tylko od tego pasma współczynnika skali w górę.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Ograniczenie miejsca, a nie wyzwalacz. Pasma poniżej podanego numeru zostają L/R albo MS bez
względu na warunki dopuszczenia; pasma od niego w górę podlegają zwykłym warunkom. Ten
przełącznik może kodowanie intensity tylko odebrać, nigdy dodać.

Oczywiste zastosowanie to ochrona dołu widma. Intensity stereo wysoko to rozsądny kompromis,
intensity stereo w środku pasma albo w basie już nie, bo tam ucho lokalizuje po poziomie i
czasie i tam zapadnięty obraz słychać natychmiast. Warto znać pułapkę: przy niskim
bitrate'cie koder lubi umieszczać IS na pasmach względnie NISKICH, niżej niż podpowiada
intuicja, więc żeby zobaczyć działanie tego przełącznika, trzeba przeszukiwać małe wartości,
a nie zaczynać od 30.

Przykłady:
- Łagodnie, wypchnij IS w górę: `--is-lo 10`. Bas i dolny środek chronione. Często
  wystarcza, żeby usunąć najgorsze lokalizacje.
- Mocno, wypchnij IS w górę: `--is-lo 25`. Intensity ograniczone do górnej połowy widma.
  Rozsądna wartość wyjściowa do starannej pracy przy średnim bitrate'cie.
- Przesadnie, wypchnij IS w górę: `--is-lo 46`. Intensity dozwolone tylko w kilku
  najwyższych pasmach, które przy domyślnym kodowanym paśmie mogą w ogóle leżeć ponad
  zakresem kodowania. Ustawienie skrajne i ta sama pułapka co w przykładzie z górnym
  pasmem MS: trzeba to sprawdzić wobec kodowanego pasma, bo inaczej oznacza po cichu
  „żadnego IS”.

Współpraca i konflikty: łączy się w okno z `--is-hi`. Tylko ogranicza, więc nie potrafi
sprawić, żeby IS wystąpiło; do tego służy `--is-aggression`. Nie ma nic wspólnego z
`--is-force-lo`, czyli przełącznikiem wymuszania o podobnej nazwie.

#### `--is-hi <sfb>`

**Dopuszcza intensity stereo tylko do tego pasma współczynnika skali włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Górna krawędź dozwolonego okna intensity, włącznie, i ten sam rodzaj ograniczenia co
`--is-lo`: zawęża miejsca, w których koder może umieścić IS, i nigdy tego nie wymusza.
Razem para definiuje obszar od..do, a wszystko poza nim zostaje kodowane konwencjonalnie.

Ograniczanie od góry to kierunek rzadszy, ale ma wyraźne zastosowanie. Gdy właśnie sama góra
widma nosi najważniejszą informację stereo, czyli powietrze, lokalizację blach,
pomieszczenie, można oddać intensity zatłoczony górny środek, a najwyższe pasma zatrzymać w
prawdziwym stereo.

Przykłady:
- Łagodne ograniczenie: `--is-hi 44`. Z intensity wyłączone są tylko pasma najwyższe.
- Mocne ograniczenie: `--is-hi 30`. Intensity ograniczone poniżej góry widma; pasmo
  powietrza zachowuje prawdziwą separację.
- Przesadne ograniczenie: `--is-hi 3`. Okno tak nisko, że warunki w jego wnętrzu
  praktycznie nigdy nie zostaną spełnione. Ustawienie skrajne, które sprowadza się do
  wyłączenia IS, a `--is 0` jest jaśniejsze i tańsze w myśleniu.

Współpraca i konflikty: `--is-lo` to druga połowa okna. Łączy się z `--isbands`, które
ogranicza liczbę, a nie pozycję. Nie wchodzi w interakcję z zakresem wymuszonym.

#### `--is-force-lo <sfb>`

**Wymusza intensity stereo od tego pasma w górę, pomijając wszystkie warunki
dopuszczenia.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-hi`
- Wpisywanie: liczba całkowita

To przełącznik laboratoryjny i zachowuje się inaczej niż wszystko powyżej. Warunek
korelacji, minimalna seria, warunek równowagi poziomów i warunek bitrate'u są pomijane:
pasma w wymuszonym zakresie są kodowane jako intensity, bo tak je ustawiono. Działa bez
żadnego innego ustawionego przełącznika IS, więc nie potrzebuje `--is 1` i ignoruje
`--is-aggression`.

Warto rozumieć, co to znaczy dla dźwięku. Kodowanie intensity zachowuje jedno widmo i
kierunek, więc w wymuszonym zakresie kanały nie mogą się już różnić niczym poza poziomem i
znakiem. Na materiale nieskorelowanym wynik nie jest subtelny; to celowe, słyszalne
zapadnięcie obrazu i może brzmieć fatalnie źle. I o to w tym przełączniku chodzi: pozwala
usłyszeć i zmierzyć mechanizm w pełnej sile, a także zbudować konkretny efekt. Strumień
pozostaje zgodny ze standardem, więc każdy dekoder go odtworzy.

Trzeba go używać w parze z `--is-force-hi`. Najpierw warto ustawić go wysoko w widmie, by
usłyszeć, co intensity robi tam, gdzie ma być używane, a potem schodzić w dół, by usłyszeć,
dlaczego te warunki istnieją.

Przykłady:
- Łagodne wymuszenie: `--is-force-lo 40 --is-force-hi 48`. Intensity tylko na najwyższych
  pasmach, czyli z grubsza tam, gdzie koder i tak by je umieścił. Najbliżej nastawy
  używalnej.
- Mocne wymuszenie: `--is-force-lo 20 --is-force-hi 48`. Cała górna połowa wymuszona na
  intensity. Wyraźne zwężenie obrazu powyżej środka pasma i realna oszczędność bitrate'u.
- Przesadne wymuszenie: `--is-force-lo 0 --is-force-hi 48`. Całe widmo kodowane jako jeden
  sterowany kanał. Ustawienie skrajne i przydatna demonstracja: tyle właśnie kosztuje
  obraz stereo, wszystko naraz.

Współpraca i konflikty: pomija `--is-corr-thresh`, `--is-min-sfbs`, `--is-lr-ratio` oraz
warunek bitrate'u, więc żadne z nich go nie ogranicza. To nie to samo co `--is-lo`, które
jedynie ogranicza. Potrzebuje `--is-force-hi`, żeby wyznaczyć górę zakresu.

Zmierzone: `--is-force-lo` i `--is-force-hi` zmieniają strumień bez żadnego innego podanego
przełącznika IS.

#### `--is-force-hi <sfb>`

**Górne pasmo wymuszonego zakresu intensity, włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-lo`
- Wpisywanie: liczba całkowita

Druga połowa wymuszonego zakresu, razem z podanym pasmem. Obie połowy razem wyznaczają
obszar kodowany jako intensity niezależnie od tego, jak wygląda materiał.

Zakres trzeba trzymać wewnątrz kodowanego pasma. Wymuszanie intensity na pasmach powyżej
częstotliwości odcięcia wymusza je na ciszy, co nic nie zmienia, więc poszukiwania winy w
przełączniku są bezcelowe: działa on bez zarzutu.

Przykłady:
- Łagodnie: `--is-force-lo 44 --is-force-hi 46`. Trzy pasma wymuszonego intensity blisko
  góry. Wąska sonda do słuchania A/B.
- Mocno: `--is-force-lo 30 --is-force-hi 48`. Szeroki wymuszony obszar przez górne widmo.
- Przesadnie: `--is-force-lo 5 --is-force-hi 48`. Wymuszone wszystko powyżej basu.
  Ustawienie skrajne: bas zachowuje swoje stereo, a cała reszta to sterowane mono, co
  brzmi dokładnie tak dziwnie, jak się czyta.

Współpraca i konflikty: bez `--is-force-lo` nie ma sensu. Niezależne od przełączników
ustawiających warunki dopuszczenia oraz od `--is-lo`/`--is-hi`. Zakres trzeba sprawdzić
wobec `active SFBs` z `--verbose` oraz wobec kodowanego pasma.

### Parametric stereo (PS)

Parametric stereo to mechanizm, z którego korzysta HE-AAC v2 (`-p 29`), i jest to zupełnie
inna propozycja niż wszystko powyżej. Koder przebiegu widzi jeden kanał: oba wejścia są
zdownmiksowane do mono, sygnał mono dostaje rdzeń AAC plus SBR, a obraz stereo podróżuje
osobno jako niewielki zestaw parametrów na ramkę. Dekoder bierze sygnał mono, dekoreluje go
i odbudowuje z tych parametrów lewy i prawy kanał.

Parametrów jest niedużo i są bardzo konkretne. IID to różnica poziomów między kanałami w
paśmie parametru, i to ona umieszcza źródło spanoramowane poziomem. ICC to koherencja, czyli
jak podobne są oba kanały, i to ona decyduje, czy pasmo brzmi jak źródło punktowe, czy jak
szerokie, rozmyte pole. Opcjonalnie IPD przenosi różnicę fazy między kanałami, a OPD mówi,
gdzie ta rotacja leży względem downmiksu. I to cały obraz stereo: kilkadziesiąt liczb na
ramkę zamiast drugiego widma.

Dlatego właśnie PS działa przy bitrate'ach, przy których nic innego nie działa. Przy 24 albo
32 kbps prawdziwy rdzeń stereo „zagłodziłby” oba kanały, czyli zabrakłoby im bitów; rdzeń
mono przy tej samej przepływności ma się wygodnie, a parametry kosztują niemal nic. Dlatego
też PS ma własne tryby awarii, które nie mają nic wspólnego z szumem kwantyzacji: obraz może
być zbyt zgrubnie próbkowany w częstotliwości (za mało pasm), zbyt zgrubnie próbkowany w
czasie (za mało obwiedni) albo chwilowo nieobecny (ramki wysłane zupełnie bez parametrów).
Przełączniki poniżej zajmują się dokładnie tymi trzema sprawami.

Jedna nastawa warta zapamiętania z tego rozdziału to `--ps-env 4 --ps-env-reduce 0`.
Poproszenie o cztery obwiednie samo w sobie często nic nie zmienia, bo automatyczna pętla
redukcji z powrotem je zwija; dopiero wyłączenie tej pętli przynosi poprawę.

Wszystko tutaj wymaga `-p 29`. Przy LC, HE-AAC v1, LD albo ELD nie ma warstwy PS, więc te
przełączniki nic nie robią.

#### `--ps <n>`

**Wymusza wysyłanie parametru stereo IID albo go zabrania, zamiast zostawiać tę decyzję
heurystyce kodera.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

IID to parametr różnicy poziomów, kręgosłup obrazu PS. Koder decyduje o jego wysyłaniu na
podstawie heurystyki różnicy głośności: jeśli oba kanały leżą niemal na tym samym poziomie,
może dojść do wniosku, że nie ma czego opisywać, i nie wysłać nic.

`--ps 0` całkowicie tłumi IID, co spłaszcza obraz w stronę samego downmiksu mono plus
tego, co daje ICC. To narzędzie tępe i przydatne głównie jako punkt odniesienia: wystarczy
zakodować raz z nim, posłuchać, jak brzmi sam rdzeń, i porównać.

`--ps 1` wymusza obecność IID w strumieniu bezwarunkowo. To ta nastawa, która ma znaczenie
na materiale źle ocenianym przez heurystykę, przede wszystkim o niemal równych poziomach,
a do tego właśnie ona przenosi liczbę pasm fazy przy włączonym IPD.

Wartości:
- `-1`: ustawienie fabryczne. Decyduje heurystyka różnicy głośności.
- `0`: IID nigdy nie wysyłane. Obraz spłaszczony, prawie mono.
- `1`: IID wysyłane zawsze.

```
fdkaac -p 29 -b 32000 --ps 1 in.wav -o ps.m4a
```

Współpraca i konflikty: `--ps-ipd 1` i tak sprawia, że strumień sygnalizuje IID, bo to
jedyna droga, którą liczba pasm fazy dochodzi do dekodera. `--ps-iid-quant` wybiera siatkę,
na której kwantowane jest IID. `--ps-icc` steruje drugim głównym parametrem.

#### `--ps-iid-quant <n>`

**Wybiera zgrubną albo dokładną siatkę kwantyzacji różnic poziomów IID.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` zgrubna, `1` dokładna
- Wpisywanie: liczba całkowita

IID jest kwantowane na stałej siatce różnic poziomów. Siatka dokładna rozdziela mniejsze
różnice poziomów i dlatego umieszcza źródła precyzyjniej, zwłaszcza blisko środka, gdzie
małe różnice robią większość roboty. Siatka zgrubna wydaje mniej bitów na parametr i na
materiale, którego obraz błądzi lekko z ramki na ramkę, potrafi brzmieć stabilniej, bo małe
fluktuacje analizy kwantują się do tego samego kroku, zamiast drgać między dwoma.

Przy bitrate'ach, w których używa się PS, bity parametrów to niewielka część budżetu, więc
dla muzyki z prawdziwą lokalizacją siatka dokładna jest zwykle właściwym wyborem. Zgrubną
warto wypróbować na mowie o bardzo niskiej przepływności albo na materiale, w którym słychać
drganie obrazu.

Wartości:
- `-1`: siatka fabryczna dla danej konfiguracji.
- `0`: siatka zgrubna. Mniej bitów parametrów, mniej precyzyjna lokalizacja, stabilniej
  przy drganiu.
- `1`: siatka dokładna. Dokładniejsza lokalizacja, trochę więcej bitów parametrów.

```
fdkaac -p 29 -b 32000 --ps-iid-quant 1 in.wav -o fine.m4a
```

Współpraca i konflikty: ma sens tylko wtedy, gdy IID jest faktycznie wysyłane, więc w parze
z `--ps 1`, gdy heurystyka je tłumi. Niezależne od `--ps-bands`, które decyduje, ile
wartości IID istnieje, a nie jak precyzyjnie każda z nich jest zakodowana.

#### `--ps-icc <n>`

**Wymusza włączenie albo wyłączenie parametru koherencji.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

ICC mówi dekoderowi, jaka część pasma ma pochodzić z dekorelowanej kopii sygnału mono, a nie
z samego sygnału mono. Wysoka koherencja to zwarte źródło punktowe, niska koherencja to
źródło szerokie i rozmyte. Na materiale z pogłosem to ICC, nie IID, decyduje o różnicy
między pomieszczeniem a suchym sygnałem mono z wartością panoramy.

`--ps-icc 0` odbiera tę kontrolę, więc pasma wracają do przypadku koherentnego, a wynik
brzmi bardziej sucho i węziej, nawet gdy IID nadal poprawnie umieszcza źródła. `--ps-icc 1`
wymusza obecność danych koherencji w strumieniu tam, gdzie koder mógł uznać je za
niepotrzebne, i to jest nastawa do wypróbowania na materiale ambientowym albo mocno
zapogłosowanym, którego szerokość brzmi zapadnięta.

Wartości:
- `-1`: ustawienie fabryczne. Decyduje koder.
- `0`: brak ICC. Bardziej sucho, węziej, wrażenie źródła punktowego.
- `1`: ICC wysyłane zawsze. Materiał rozmyty zachowuje swoją szerokość.

```
fdkaac -p 29 -b 32000 --ps-icc 1 in.wav -o icc.m4a
```

Współpraca i konflikty: `--ps-icc-mode` zmienia wyłącznie sposób sygnalizowania tej samej
macierzy i nie jest jakościową alternatywą dla tego przełącznika. Uzupełnia `--ps`, które
steruje parametrem poziomu.

#### `--ps-ipd <0|1>`

**Przesyła międzykanałową różnicę fazy w rozszerzeniu PS.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłączone (ustawienie fabryczne, faza nigdy nie wysyłana), `1` włączone
- Wpisywanie: liczba całkowita, włącz/wyłącz

Bez fazy PS pozycjonuje dźwięk wyłącznie różnicami poziomów i koherencją. To pokrywa
wszystko spanoramowane poziomem, czyli większość współczesnego miksu, ale nie potrafi
przedstawić źródła umieszczonego CZASEM. Prawdziwe opóźnienie międzykanałowe, czyli to, jak
lokalizacja niskich częstotliwości działa w naturze, jest dla IID i ICC niewidoczne: oba
kanały niosą ten sam sygnał na tym samym poziomie i różnią się tylko momentem dotarcia.
Włączenie IPD wysyła ten kąt, skwantowany na 8 zdefiniowanych krokach po pi/4, w
rozszerzeniu PS z prefiksem długości.

Zakres, w którym to pomaga, jest wąski i warto go znać dokładnie. IPD jest kodowane dla
dolnych 5 pasm parametru w konfiguracji 10-pasmowej i dolnych 11 w konfiguracji 20-pasmowej,
co w częstotliwości oznacza z grubsza dolne 690 Hz: te 11 pasm leży wewnątrz pierwszych
trzech pasm QMF. Powyżej 690 Hz ani odsłuch, ani pomiar nie pokażą zupełnie nic. Materiał,
który na tym korzysta, to niskie i nisko-średnie pasmo, szerokie, miękkie źródła, pogłos
oraz prawdziwe międzykanałowe mikroopóźnienia.

Jest też sufit, przy czym wynika on z samej reprezentacji, a nie z jakiejś wady. Siatka o
kroku 45 stopni może śledzić opóźnienie tylko dopóty, dopóki wytwarzana przez nie faza
pozostaje jednoznaczna w całym kodowanym zakresie. Na górze zakresu IPD opóźnienie około 0,7
ms obraca się już przez niemal całe plus minus 180 stopni, więc siatka nie potrafi już
powiedzieć, gdzie ono jest. To klasyczna granica niejednoznaczności fazy ITD i ten sam
powód, dla którego słuch opiera się na przesłankach czasowych głównie nisko.

Jedno praktyczne sprzężenie: rozszerzenie fazy nie nosi własnej liczby pasm. Dekoder
wyprowadza ją z sygnalizacji IID, więc włączenie `--ps-ipd` sprawia, że strumień sygnalizuje
też IID. To parametr tani i zawsze dozwolony, a przy tym jedyna droga, którą liczba pasm
dochodzi do dekodera.

Zgodność jest bezpieczna z samej konstrukcji. Ładunek siedzi w rozszerzeniu z prefiksem
długości, więc dekoder bez syntezy fazy czyta liczbę bajtów i pomija je, na co standard
wyraźnie pozwala. Trzeba jednak zwrócić uwagę, które dekodery z tego korzystają: dekoder
Fraunhofera IGNORUJE rozszerzenie IPD, więc dekodowanie nim nie pokaże żadnej zmiany. ffmpeg
z niego korzysta. Dekoder należy więc wybrać świadomie, przed orzeczeniem, że przełącznik
nic nie zrobił.

Wartości:
- `0`: ustawienie fabryczne. Brak fazy w strumieniu.
- `1`: IPD przesyłane dla dolnych 5 (10 pasm) albo 11 (20 pasm) pasm parametru, a razem z
  nim sygnalizowane IID.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 1 in.wav -o ipd.m4a
```

Współpraca i konflikty: `--ps-opd` to druga połowa pary fazowej i ma sens tylko przy tym
włączonym. Wymusza sygnalizację IID, więc pokrywa się z `--ps 1`.

Zmierzone, z ffmpegiem jako dekoderem: sondy zbudowane jako identyczny sygnał w obu
kanałach, równy poziom, przesunięty wyłącznie opóźnieniem międzykanałowym, z porównaniem
zdekodowanego profilu fazy wobec źródła wewnątrz zakresu IPD od 60 do 690 Hz.

| Opóźnienie międzykanałowe | błąd fazy, IPD wyłączone | błąd fazy, IPD włączone |
|---|---|---|
| 0.25 ms | 35.0 deg | 14.9 deg |
| 0.4 ms | 56.1 deg | 24.9 deg |
| 0.7 ms | 67.4 deg | 71.8 deg |

Przy 0,25 i 0,4 ms błąd fazy spada o ponad połowę, z identycznymi liczbami przy 32 i 48
kbps. Przy 0,7 ms korzyść znika, a to właśnie opisany wyżej sufit niejednoznaczności.

#### `--ps-opd <0|1>`

**Wysyła fazę kanału lewego względem downmiksu mono, towarzysza IPD.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`) i tylko razem z `--ps-ipd 1`
- Wartości: `0` przypina OPD do zera, `1` wylicza je (zachowanie domyślne, gdy IPD jest
  włączone)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Różnica między obydwoma parametrami fazy wynagradza uważną lekturę. IPD to faza kanału
lewego względem prawego. OPD to faza kanału lewego względem downmiksu mono. Dekoder
potrzebuje obu, bo ścieżki zasilane z downmiksu obraca o OPD, a pozostałe o OPD minus IPD.
Po odjęciu obu OPD się skraca, więc RÓŻNICA międzykanałowa zawsze wychodzi jako IPD, bez
względu na to, co mówi OPD. OPD decyduje o tym, gdzie ta rotacja leży względem downmiksu.

Właśnie to skracanie sprawia, że problem łatwo przeoczyć. Przy OPD przypiętym do zera lewy
kanał jest przyklejony do fazy downmiksu, a cała rotacja zostaje zrzucona na prawy. Każdy
pomiar arg(L) minus arg(R) jest na to ślepy, bo różnica nadal jest poprawna. Trzeba zamiast
tego zmierzyć fazę bezwzględną każdego kanału wobec źródła, wewnątrz zakresu IPD: wtedy
asymetria staje się oczywista, podobnie jak to, że OPD ją usuwa.

Wyprowadzenie OPD praktycznie nic nie kosztuje, i to jest w tym zgrabne. Downmiks to L+R,
więc sum(L * conj(L+R)) równa się sum(|L|^2) plus sum(L * conj(R)), której część rzeczywista
to pwrL + pwrCr, a część urojona to pwrCi: te same akumulatory, które już zasilają IID, ICC
i IPD. Zatem OPD to atan2(pwrCi, pwrL + pwrCr), czyli jeszcze jeden atan2 na liczbach już
obecnych w pętli, bez potrzeby podawania sygnału downmiksu skądkolwiek indziej. Dla
przypadku opóźnienia przy równych poziomach redukuje się to analitycznie do IPD/2, bo arg(1
+ e^-i.phi) = -phi/2, co jest przydatnym testem zdrowego rozsądku; postać ogólna jest
poprawna także wtedy, gdy kanały różnią się poziomem, a wtedy IPD/2 już nie byłoby.

Wartość `0` warto zostawić do porównań A/B na tym samym materiale, pamiętając, że
`--ps-opd 0` samo z siebie, bez `--ps-ipd 1`, daje strumień fabryczny, bo nie ma warstwy
fazy, którą mógłby zmodyfikować. Różnice OPD pojawiają się przy tym tylko na materiale z
prawdziwym opóźnieniem międzykanałowym; na treści spanoramowanej poziomem nie ma tu czego
poprawiać.

Wartości:
- `0`: OPD przypięte do zera. Błąd fazy asymetryczny między kanałami.
- `1`: OPD wyliczane z istniejących akumulatorów. Błąd fazy symetryczny.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 0 in.wav -o opd0.m4a
```

Współpraca i konflikty: bez `--ps-ipd 1` nie ma sensu. Nie zmienia różnicy międzykanałowej,
a tylko fazę bezwzględną każdego kanału, więc pomiar trzeba dobrać odpowiednio.

Zmierzone, z ffmpegiem jako dekoderem: bezwzględny błąd fazy każdego kanału wobec źródła,
wewnątrz zakresu IPD.

| Opóźnienie międzykanałowe | OPD = 0 (L / R) | OPD wyliczane (L / R) |
|---|---|---|
| 0.25 ms | 14.7 / 20.1 deg | 14.4 / 17.3 deg |
| 0.4 ms | 23.0 / 28.0 deg | 15.5 / 17.8 deg |
| 0.7 ms | 48.1 / 56.2 deg | 40.9 / 36.3 deg |

Przy OPD = 0 oba kanały nigdy się nie zgadzają, 14,7 przeciw 20,1, a dalej 48,1 przeciw
56,2. Przy wyliczanym OPD para się zbiega, średnio o +6,4 stopnia, a przy 0,7 ms o +13.
Warto zauważyć, że 0,7 ms to przypadek, w którym zwykły pomiar IPD nie pokazał żadnej
korzyści: różnica międzykanałowa była już poza tym, co potrafi rozdzielić siatka 45 stopni,
a bezwzględna faza każdego kanału i tak poprawiła się istotnie.

#### `--ps-icc-mode <n>`

**Wybiera, który z dwóch zdefiniowanych trybów rotacji ICC jest sygnalizowany.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` ROT_A, `1` ROT_B
- Wpisywanie: liczba całkowita

Zanim poświęci się na to czas, warto ustawić oczekiwania: to wyłącznie sygnalizacja. Oba
tryby opisują tę samą macierz stereo, dekoder po prostu wyprowadza ją inaczej. To
pokrętło zgodności, a nie jakości; słyszalne różnice stereo w PS robią `--ps-bands` i
`--ps-env`.

Warto go mieć, bo nie wszystkie implementacje dekoderów są równie dobrze przećwiczone na obu
ścieżkach, więc gdy jakiś konkretny odtwarzacz dziwnie renderuje strumień PS, przełączenie
trybu rotacji jest tanie do sprawdzenia.

Wartości:
- `-1`: wybór fabryczny dla danej konfiguracji.
- `0`: sygnalizowane ROT_A.
- `1`: sygnalizowane ROT_B.

```
fdkaac -p 29 -b 32000 --ps-icc-mode 1 in.wav -o rotb.m4a
```

Współpraca i konflikty: istotne tylko wtedy, gdy ICC jest wysyłane, więc warto sprawdzić
`--ps-icc`. Słuchowo znacznie mniej znaczące niż `--ps-bands`.

#### `--ps-bands <10|20>`

**Rozdzielczość częstotliwościowa parametrów stereo: ile pasm PS opisuje obraz.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `10` albo `20`; `-1` wybiera na podstawie bitrate'u (domyślnie), a od 22 kbps w
  górę tabela zawsze daje 20
- Wpisywanie: liczba całkowita

To jedna z dwóch osi decydujących o tym, ile parametrów stereo w ogóle istnieje, i jest to
oś częstotliwości. Dwadzieścia pasm opisuje obraz z dokładniejszym szczegółem widmowym, więc
miks, którego lokalizacja zmienia się w widmie (bas w środku, gitary szeroko, talerze
przesunięte), zachowuje tę strukturę. Dziesięć pasm uśrednia ten sam obraz po dwa razy
większym fragmencie widma każde.

Ciekawy jest dolny koniec zakresu bitrate'u. Zgrubna siatka nie tylko jest tańsza, ale może
też brzmieć stabilniej, bo każdy parametr jest uśredniany po większym fragmencie widma i
dlatego mniej drga z ramki na ramkę. Błądzący obraz bardziej rozprasza niż lekko rozmyty.
Fabryczny FDK wyprowadza tę liczbę wyłącznie z bitrate'u i od 22 kbps w górę zawsze wybiera
20, więc bez tego przełącznika nie da się na normalnym materiale w ogóle przesłuchać siatki
zgrubnej.

Wartości:
- `10`: siatka zgrubna. Mniej bitów parametrów, bardziej rozmyta lokalizacja w
  częstotliwości, stabilniej przy drganiu analizy.
- `20`: siatka dokładna. Dokładniejsza lokalizacja, więcej bitów parametrów.
- `-1`: ustawienie fabryczne, wybrane na podstawie bitrate'u.

```
fdkaac -p 29 -b 24000 --ps-bands 10 in.wav -o coarse.m4a
```

Współpraca i konflikty: `--ps-env` to oś czasu tego samego pomysłu. Oba są słuchowo
istotniejsze niż `--ps-icc-mode`. `--ps-iid-quant` zmienia precyzję każdej wartości, a nie
to, ile tych wartości jest.

#### `--ps-env <1|2|4>`

**Rozdzielczość czasowa parametrów stereo: ile obwiedni parametrów na ramkę.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `1`, `2` albo `4`; `-1` wybiera na podstawie bitrate'u (domyślnie), a od 36 kbps
  w górę tabela daje 4
- Wpisywanie: liczba całkowita

Druga oś rozdzielczości. Jedna obwiednia oznacza, że cała ramka dostaje jedną migawkę
stereo, cztery oznaczają, że obraz jest próbkowany cztery razy w ramce. To właśnie decyduje
o tym, czy ruchoma panorama albo transjent lewo/prawo zachowa swoje położenie, czy rozmaże
się po ramce w uśrednienie.

Jest tu pułapka i to najważniejsza rzecz w tym rozdziale: poproszenie o cztery obwiednie nie
znaczy, że zostaną przesłane. Automatyczna pętla redukcji wciąż dzieli tę liczbę na pół, z
czterech na dwie, z dwóch na jedną, kiedy tylko sąsiednie obwiednie wyglądają podobnie
według wbudowanego progu błędu, a na zwykłej muzyce ten próg jest spełniany bardzo często.
Żądanie jest odwoływane, zanim dotrze do strumienia. Zatrzymuje to `--ps-env-reduce 0`, a
pomiar w jego karcie pokazuje różnicę wprost.

Wartości:
- `1`: jedna migawka stereo na ramkę. Najtaniej, rozmazuje każdy ruch.
- `2`: dwie obwiednie. Rozsądna nastawa środkowa razem z `--ps-env-reduce 0`.
- `4`: cztery obwiednie. Dokładnie nadąża za ruchomą panoramą i transjentami.
- `-1`: ustawienie fabryczne, wybrane na podstawie bitrate'u.

```
fdkaac -p 29 -b 32000 --ps-env 4 --ps-env-reduce 0 in.wav -o env4.m4a
```

Współpraca i konflikty: żeby na zwykłym materiale zostało uszanowane, wymaga w praktyce
`--ps-env-reduce 0`. `--ps-bands` to częstotliwościowa oś tego samego kompromisu.

Zmierzone: przy HE-AAC v2 32 kbps `--ps-env 4` faktycznie zmienia strumień, bo tabela
bitrate'u wybiera tam 2; przy 48 kbps tabela daje już 4, więc żądanie zgadza się z
ustawieniem fabrycznym i różnicę robi tylko `--ps-env-reduce 0`.

#### `--ps-env-reduce <0|1>`

**Wyłącza automatyczną pętlę połowienia obwiedni, żeby przesłana została żądana liczba
obwiedni.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłącza pętlę redukcji, `1` zostawia ją włączoną; `-1` wyłączone, czyli
  zachowanie fabryczne, które ma pętlę włączoną (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Koder utrzymuje pętlę redukcji, która porównuje sąsiednie obwiednie parametrów i zwija
cztery w dwie, a dwie w jedną, kiedy tylko wyglądają dość podobnie według wbudowanego progu
błędu. Intencją jest oszczędzenie bitów parametrów na materiale statycznym. Skutkiem
ubocznym jest to, że zamówiona liczba obwiedni często nie jest tą, która zostaje przesłana,
a ponieważ oszczędność jest mała, a kosztem jest rozdzielczość czasowa obrazu, na materiale
z jakimkolwiek ruchem ten kompromis jest często zły.

`--ps-env-reduce 0` wyłącza pętlę i czyni `--ps-env` dosłownym. To najwartościowsza
pojedyncza nastawa w grupie PS. Zysk jest w dokładności czasowej obrazu stereo, przy
praktycznie tym samym rozmiarze pliku, bo bity parametrów to niewielka część budżetu przy
tych przepływnościach.

Wartości:
- `-1` albo `1`: ustawienie fabryczne. Pętla działa i może zwinąć liczbę obwiedni.
- `0`: pętla wyłączona, `--ps-env` brane dosłownie.

```
fdkaac -p 29 -b 48000 --ps-env 4 --ps-env-reduce 0 in.wav -o literal.m4a
```

Współpraca i konflikty: bez `--ps-env` nie ma sensu, bo cała jego funkcja to ochrona tego
żądania. Łączy się dowolnie z `--ps-bands`.

Zmierzone: 4-sekundowa sonda stereo z panoramą przemiataną z częstotliwością 0,25 Hz plus
naprzemienne transjenty z lewej i z prawej, HE-AAC v2 przy 48 kbps, z porównaniem
zdekodowanej trajektorii panoramy wobec źródła.

| Nastawa | Błąd panoramy (RMS) | Korelacja ze źródłem |
|---|---|---|
| fabryczne (20 bands / 4 envelopes) | 0.177 | 0.9655 |
| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
| `--ps-env 4 --ps-env-reduce 0` | 0.117 | 0.9944 |

Samo żądanie 4 obwiedni nie zmienia przy 48 kbps nic, wyjście jest identyczne bajt w bajt z
fabrycznym, bo pętla redukcji natychmiast odwołuje żądanie. Zysk pojawia się tylko wtedy,
gdy pętla zostanie zatrzymana: o jedną trzecią mniejszy błąd panoramy przy z grubsza tym
samym rozmiarze pliku.

#### `--ps-noenv-skip <0|1>`

**Zabrania ramek PS bez parametrów, żeby każda ramka nosiła informację stereo.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` bez ramek bez parametrów, `1` zezwolenie na nie; `-1` wyłączone, czyli
  zachowanie fabryczne, które na nie zezwala (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Poza przerzedzaniem obwiedni koder może też całkowicie pominąć parametry stereo na odcinku
ramek. Kiedy kolejne zestawy IID i ICC wyglądają podobnie, może wysłać do dziesięciu
następujących po sobie ramek, które nie niosą żadnej informacji stereo, pozostawiając
dekoder na biegu jałowym z ostatnimi wartościami, jakie otrzymał.

Na materiale statycznym jest to darmowe. Na materiale, w którym obraz dryfuje powoli,
słychać to jako spłaszczanie się obrazu stereo i jego powrót z trzaskiem, gdy parametry
wracają, bo dekoder trzymał przez cały dryf nieaktualną migawkę, a potem przeskoczył do
bieżącej. Raz rozpoznany artefakt zwykle słychać już za każdym razem, a rozprasza on
bardziej, niż są warte zaoszczędzone bity.

`--ps-noenv-skip 0` zabrania pomijania i wymusza parametry w każdej ramce. Warto go użyć,
gdy obraz wydaje się oddychać albo zacinać. Kosztem jest skromna liczba bitów parametrów na
ramkę.

Wartości:
- `-1` albo `1`: ustawienie fabryczne. Do dziesięciu następujących po sobie ramek może nie
  nieść żadnych parametrów stereo.
- `0`: parametry w każdej ramce.

```
fdkaac -p 29 -b 32000 --ps-noenv-skip 0 --ps-env 4 --ps-env-reduce 0 in.wav -o steady.m4a
```

Współpraca i konflikty: ta sama rodzina ukrytego przerzedzania co `--ps-env-reduce`, i te
dwa ustawia się zwykle razem, gdy celem jest stabilny obraz stereo. Żaden z nich nie zmienia audio
rdzenia, tylko strumień parametrów stereo.

## C. Pasmo i SBR

O tym, jak wysoko brzmi plik, decydują dwie różne rzeczy, które łatwo ze sobą pomylić.

Pierwsza to **pasmo rdzenia**: najwyższa częstotliwość, którą sam rdzeń AAC faktycznie
koduje, prawdziwymi liniami widmowymi i prawdziwymi bitami. Powyżej tej granicy rdzeń nie
wysyła nic.

Druga to **częstotliwość podziału SBR**, nazywana też częstotliwością startu SBR. SBR
(spectral band replication) nie koduje górnego pasma. Kopiuje w górę niższe, już zakodowane
widmo, a potem opisuje garścią parametrów na ramkę, jak ta kopia ma zostać ukształtowana.
Częstotliwość podziału to miejsce, w którym kopia się zaczyna. W poprawnie zbudowanym
strumieniu HE-AAC pasmo rdzenia i częstotliwość podziału leżą w tym samym miejscu: rdzeń się
kończy, SBR przejmuje pałeczkę, a słuchacz słyszy jedno ciągłe widmo.

To, co SBR wysyła na ramkę, jest krótką listą:

- **obwiednie**: poziom każdego pasma SBR w czasie. Więcej obwiedni na ramkę oznacza
  drobniejszą rozdzielczość czasową na górze i więcej bitów.
- **poziom szumu SBR (noise floor)**: ile syntetycznego szumu domieszać do każdego pasma. To
  właśnie on zamienia skopiowany obszar tonalny z powrotem w coś, co brzmi jak powietrze,
  talerze albo oddech.
- **poziom filtrowania odwrotnego** (invf): jak mocno wybielać kopię w każdym paśmie.
  Skopiowany materiał niesie strukturę tonalną pasma, z którego pochodzi. Jeśli ta
  struktura nie pasuje do góry, słychać ją jako metaliczną, dzwoniącą, chórowaną górę.
  Wybielanie tę strukturę spłaszcza.
- **brakujące harmoniczne**: flagi, które każą dekoderowi wstawić syntetyczną sinusoidę w
  paśmie, gdzie prawdziwa składowa oryginału zginęła albo wylądowała w złym miejscu.

O dwóch ostatnich decydują **estymatory** w koderze, które porównują tonalność oryginalnego
górnego pasma z tonalnością pasma odtworzonego przez SBR, pasmo po paśmie i ramka po ramce.
To w estymatorach ustala się charakter góry, a one wymieniają się między sobą wzdłuż jednej
osi: za mało wybielania i za mało szumu wypełniającego daje górę metaliczną, dzwoniącą,
pofazowaną; za dużo jednego i drugiego daje górę syczącą i rozmytą, w której talerze
zamieniają się w kształtowany szum, a utrzymujące się składowe znikają. Większość pokręteł w
tej grupie przesuwa punkt pracy dokładnie po tej osi. Nie ma na niej punktu uniwersalnie
poprawnego; zależy on od materiału i od bitrate'u.

SBR jest aktywne w tych profilach:

- `-p 5` HE-AAC
- `-p 29` HE-AAC v2 (SBR plus parametric stereo)
- `-p 39 -L 1` AAC-ELD z SBR

W `-p 2` (AAC-LC), `-p 23` (AAC-LD) i `-p 39` bez `-L 1` nie ma SBR, więc każdy przełącznik
`--sbr-*` z tej grupy nic nie robi. `--core-cutoff`, na końcu grupy, działa wszędzie, ale
bez SBR jest tym samym co `-w`.

Dwie konwencje używane w całej tej grupie:

- **pokrętła x100** skalują wewnętrzną stałą: `100` oznacza 1,00 i jest dokładnie
  ustawieniem fabrycznym, poniżej 100 stała maleje, powyżej 100 rośnie. Oba kierunki
  działają. Kilka pokręteł jest opisanych jako **surowe x100**, co znaczy, że wartość
  zastępuje stałą wprost, zamiast ją skalować (`--sbr-tran-peak 90` *jest* fabryczną
  wartością 0,90).
- `--verbose` wypisuje efektywne pasmo rdzenia, częstotliwość podziału, końcową górną
  krawędź i okres nagłówka SBR. Pod SBR to jedyny pewny sposób, żeby zobaczyć, co
  naprawdę ustawił koder, a kilka przełączników z tej grupy FDK sprawdza dopiero przy
  inicjalizacji, więc sprawdzanie nie jest opcjonalne.

### Układ częstotliwości SBR: `--sbr-start` `--sbr-stop` `--sbr-freqscale` `--sbr-alterscale` `--sbr-noise-bands` `--sbr-amp-res` `--sbr-data-extra`

Tych siedem przełączników definiuje kształt pasma SBR, zanim cokolwiek zostanie w nim
zmierzone: gdzie zaczyna się obszar replikowany, gdzie się kończy, jak jest podzielony na
pasma, jak drobno opisywany jest poziom szumu i jak dokładnie kwantowane są poziomy
obwiedni.

Dwa ważne to `--sbr-start` i `--sbr-stop`. Oba przyjmują indeks tabeli, a nie częstotliwość,
a to, na jaką częstotliwość mapuje się indeks, zależy od częstotliwości próbkowania, profilu
i bitrate'u. Wartości zmierzone podajemy w kartach poniżej. Warto czytać je jako wskazówkę
co do kierunku i odstępów, a rzeczywiste liczby dla konkretnych ustawień potwierdzić przez
`--verbose`.

Reguła, na której wykłada się każdy: **legalność zależy od pary, bitrate'u i częstotliwości
próbkowania.** SBR wymaga poprawnej liczby pasm nadrzędnych między startem a stopem, a FDK
sprawdza tę kombinację przy inicjalizacji kodera. Zła para nie degraduje się łagodnie;
zgłasza "encoder initialization failed" i nic nie zostaje zapisane. Legalne kombinacje
tworzą przekątny pas: podnosząc start, trzeba zwykle podnieść razem z nim stop. Przy HE-AAC
64 kbps stereo, wejście 48 kHz, każdy start od 0 do 4 jest nielegalny z każdym stopem, a
wyglądająca na udokumentowaną para `--sbr-start 5 --sbr-stop 9` tam zawodzi. Pary, które
przy tym ustawieniu działają: `8/9`, `8/14`, `14/12`.

Czyli: zmieniać po jednym indeksie naraz, uruchomić kodowanie i przeczytać `--verbose`.
Kodowanie, które nie zainicjalizuje się, kosztuje sekundę; strumień z niezamierzoną
częstotliwością podziału kosztuje całą sesję odsłuchową.

#### `--sbr-start <n>`

**Ustawia miejsce, w którym kończy się rdzeń, a zaczyna replikowane górne pasmo.**

- Dotyczy: tylko profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: indeks tabeli `0..15`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

To najbardziej brzemienna w skutki pojedyncza liczba w strumieniu HE-AAC. Wszystko poniżej
częstotliwości podziału to prawdziwe, zakodowane widmo; wszystko powyżej to opisana kopia.
Obniżenie startu daje rdzeniowi mniej roboty, więc to, co koduje, koduje czyściej, a większa
część widma staje się parametryczna. Podniesienie startu działa odwrotnie: więcej prawdziwej
góry, mniejszy obszar SBR i rdzeń, który musi za to zapłacić.

Przy bardzo niskim bitrate obniżenie częstotliwości podziału jest często jedynym sposobem,
żeby rdzeń w ogóle brzmiał czysto. Przy 64 kbps i wyżej wygrywa raczej podnoszenie, bo rdzeń
stać na autentyczny detal w zakresie 10..12 kHz, a skopiowane pasmo zawsze niesie część
charakteru pasma, z którego je skopiowano.

Zmierzone pasmo rdzenia dla kolejnych indeksów startu, tak jak wypisuje je `--verbose` przy
HE-AAC `-p 5`, 64 kbps, wejście stereo 48 kHz, z domyślnym stopem. "Nielegalny" znaczy, że
inicjalizacja kodera zawiodła dla każdego wypróbowanego stopu:

| `--sbr-start` | pasmo rdzenia |
|---|---|
| 0..4 | nielegalny przy tym ustawieniu |
| 5 | 5062 Hz (tylko z `--sbr-stop 0`) |
| 6 | 5484 Hz |
| 7 | 5953 Hz |
| 8 | 6468 Hz |
| 9 | 6468 Hz |
| 10 | 7031 Hz |
| 11 | 7640 Hz |
| 12 | 8250 Hz |
| 13 | 9000 Hz |
| 14 | 10500 Hz (fabryczne przy tym bitrate) |
| 15 | 12000 Hz |

Ten sam przegląd przy wejściu 44,1 kHz, HE-AAC 32 kbps: start 5 daje 5038 Hz, 8 daje 5943
Hz, 11 daje 7579 Hz, 14 daje 9646 Hz, 15 daje 11025 Hz. Warto zauważyć, że indeksy 8 i 9
przy 48 kHz trafiają w to samo pasmo rdzenia: to mapowanie jest tabelą, a nie wzorem, i
sąsiednie indeksy mogą się pokrywać.

Przykłady, obniżanie częstotliwości podziału (HE-AAC 64 kbps stereo, 48 kHz, fabryczny
start 14):
- Łagodnie: `--sbr-start 13`. Rdzeń do 9000 Hz. Minimalnie czystszy rdzeń, odrobinę
  szerszy obszar odtwarzany przez SBR.
- Mocno: `--sbr-start 11`. Rdzeń do 7640 Hz. Zauważalnie większa część góry jest
  parametryczna. Warto tego spróbować, gdy rdzeń brzmi ziarniście.
- Przesadnie: `--sbr-start 6 --sbr-stop 9`. Rdzeń do 5484 Hz. Prawie wszystko powyżej
  obszaru obecności jest kopią; talerze i syczące głoski to w całości wymysł SBR. To
  właściwy kształt przy 24 kbps i ustawienie skrajne przy 64.

Przykłady, podnoszenie częstotliwości podziału:
- Łagodnie: `--sbr-start 15`. Rdzeń do 12000 Hz, szczyt zakresu indeksów przy tej
  częstotliwości. Prawdziwie kodowane widmo do 12 kHz, węższy obszar SBR powyżej.
- Mocno: dalsze podnoszenie częstotliwości podziału nie jest tym przełącznikiem dostępne
  przy tym ustawieniu, bo 15 to najwyższy indeks. Kolejny krok to porzucić SBR i zakodować
  całe pasmo przez `-p 2` plus `-w`.
- Przesadnie: to samo doprowadzone do końca: `-p 2 -b 64000 -w 16000`. Bez SBR, bez kopii,
  a przy 64 kbps stereo rdzeń wyraźnie nie ma bitów na pasmo 16 kHz. Pouczające do
  porównania z HE-AAC na tym samym materiale.

Współpraca i konflikty: musi tworzyć legalną parę z `--sbr-stop`. Rdzeń sam idzie za
startem; `--core-cutoff` może go ściągnąć poniżej startu, co otwiera dziurę, a powyżej nie
da się go przesunąć z pożytkiem. Start warto podnosić o jeden indeks i słuchać: każdy krok
przesuwa w górę prawdziwie kodowane widmo i kosztuje rdzeń bity. Obniżenie startu poszerza
pasmo, za które odpowiadają `--sbr-noise-bands`, `--sbr-invf*` i detektor brakujących
harmonicznych, więc im niżej leży start, tym bardziej liczą się ich ustawienia.

#### `--sbr-stop <n>`

**Ustawia górną krawędź pasma replikowanego, czyli górną krawędź pliku.**

- Dotyczy: tylko profili z SBR
- Wartości: indeks tabeli `0..13`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Tam, gdzie `--sbr-start` dzieli widmo, ten przełącznik ustawia sufit. Podniesienie go
rozciąga obszar odtwarzany przez SBR wyżej, co prawie nic nie kosztuje w bitach (SBR opisuje
pasma, a nie linie), ale rozkłada ten sam budżet parametrów na szerszy obszar, więc każde
pasmo jest opisane grubiej. Obniżenie go zamyka plik wcześniej, co brzmi jak zwykła
dolnoprzepustowa i bywa odbierane jako czystsze, bo mierne odtworzenie zakresu 18..22 kHz
wnosi więcej syku niż treści.

Zmierzona końcowa górna krawędź zdekodowanego wyjścia (rdzeń plus SBR) dla kolejnych
indeksów stopu, `--verbose` przy HE-AAC `-p 5`, 64 kbps, wejście stereo 48 kHz, z
`--sbr-start 14`:

| `--sbr-stop` | końcowa górna krawędź |
|---|---|
| 0..3 | nielegalny przy tym ustawieniu |
| 4 | około 10875 Hz |
| 5 | około 12000 Hz |
| 6 | około 13125 Hz |
| 7 | około 14250 Hz |
| 8 | około 15375 Hz |
| 9 | około 16875 Hz |
| 10 | około 18375 Hz |
| 11 | około 20250 Hz |
| 12 | około 22125 Hz (fabryczne) |
| 13 | nielegalny przy tym ustawieniu |

Przykłady, obniżanie górnej krawędzi (z fabrycznego 12):
- Łagodnie: `--sbr-start 14 --sbr-stop 11`. Góra na około 20250 Hz. Trudne do usłyszenia,
  nieco gęstszy opis tego, co zostaje.
- Mocno: `--sbr-start 14 --sbr-stop 9`. Góra na około 16875 Hz. Świadomy, słyszalny sufit,
  a pozostałe pasma opisane drobniej. Często lepszy kompromis przy 32 kbps i niżej.
- Przesadnie: `--sbr-start 14 --sbr-stop 4`. Góra na około 10875 Hz przy rdzeniu 10500 Hz:
  SBR obejmuje teraz kilkaset Hz i nie wnosi nic. W praktyce powstaje strumień AAC-LC z
  doczepionym narzutem SBR.

Przykłady, podnoszenie górnej krawędzi:
- Łagodnie: w górę przy tym ustawieniu nic nie ma. Stop 12 (około 22125 Hz) to już
  domyślna góra, a stop 13 nie jest legalny ze startem 14. Górę można utrzymać, przesuwając
  częstotliwość podziału w górę: `--sbr-start 15 --sbr-stop 12` zachowuje około 22125 Hz
  przy rdzeniu 12000 Hz.
- Mocno: `--sbr-start 8 --sbr-stop 14`. Szeroki obszar odtwarzany przez SBR od 6468 Hz w
  górę. Tani w bitach, zgrubny w opisie: na gęstym materiale grozi "migotliwy" charakter.
- Przesadnie: `--sbr-stop 13` przy tym ustawieniu, co kończy się nieudaną inicjalizacją.
  Indeks istnieje w standardzie, ale nie w każdej legalnej parze, i to najszybszy sposób,
  żeby zobaczyć w praktyce, co oznacza ten przekątny pas legalności.

Współpraca i konflikty: ograniczenie pary z `--sbr-start` rządzi obydwoma. Indeks 13 jest
przyjmowany przez parser, ale nie jest legalny w każdej parze, i dlatego `--verbose` oraz
testowe kodowanie wyprzedzają jakikolwiek odsłuch.

#### `--sbr-freqscale <n>`

**Wybiera, jak replikowane pasmo jest dzielone na pasma SBR: liniowo albo na jednej z
trzech siatek logarytmicznych.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = liniowo, `1`, `2`, `3` = logarytmicznie, 12, 10 i 8 pasm na oktawę
  (coraz grubiej); `-1` albo pominięcie przełącznika = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

SBR opisuje swój obszar pasmo po paśmie, a to jest wybór linijki. Siatka liniowa daje
każdemu pasmu tę samą szerokość w Hz, więc górna oktawa dostaje wiele pasm, a dół obszaru
SBR niewiele. Siatka logarytmiczna rozstawia pasma według stosunku częstotliwości, co pasuje
do słuchu: więcej rozdzielczości tam, gdzie ucho rozróżnia więcej, czyli nisko, przy
częstotliwości podziału.

Wartości:
- `0` liniowo. Równe odstępy w Hz. Drobna rozdzielczość wysoko, zgrubna tuż powyżej
  częstotliwości podziału, czyli odwrotnie niż oczekuje ucho. Bywa użyteczne na materiale,
  którego górne pasmo jest naprawdę jednorodne, na przykład na szerokopasmowych podkładach
  szumowych.
- `1` najdrobniejsze grupowanie logarytmiczne, 12 pasm na oktawę. Najwięcej pasm, najwięcej
  parametrów, najlepsze kształtowanie nierównego górnego pasma.
- `2` środkowe grupowanie logarytmiczne, 10 pasm na oktawę.
- `3` najgrubsze grupowanie logarytmiczne, 8 pasm na oktawę. Najmniej pasm, najtańszy opis,
  najmniejsza kontrola.

Zmierzone przy HE-AAC 64 kbps, dane SBR na ramkę: 220 bitów przy `0`, 188 przy `1`, 170
przy `2`, 152 przy `3`.
- `-1` wyłączone: decyduje tabela strojenia, co jest właściwym wyborem, dopóki nie ma
  powodu, żeby zrobić inaczej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 1 -o out.m4a in.wav` (najdrobniejsza siatka)

Współpraca i konflikty: liczba pasm, którą to daje, wchodzi w interakcję z
`--sbr-noise-bands` (pasma szumu wyprowadza się na oktawę, a nie na pasmo SBR) oraz z tym,
jak bardzo decyzja filtrowania odwrotnego (invf) może się zmieniać w obrębie obszaru.
Drobniejsza siatka oznacza drobniejszą mapę invf.

#### `--sbr-alterscale <n>`

**Przełącza na alternatywną rozdzielczość grupowania pasm, czyli wtórną korektę siatki
wybranej przez `--sbr-freqscale`.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = wyłączone, `1` = włączone; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

To standardowa flaga nagłówka SBR, która nieco zmienia sposób wyprowadzania tabeli pasm
nadrzędnych, co w praktyce przesuwa kilka granic pasm. To dopracowanie, a nie kontrola
charakteru: nie ma tu wyniku odsłuchowego, który warto byłoby obiecywać w którejkolwiek
pozycji, i nic w naszych pomiarach go nie izoluje.

Wartości:
- `0` alternatywna rozdzielczość wyłączona.
- `1` alternatywna rozdzielczość włączona.
- `-1` wyłączone: decyduje tabela strojenia. Zalecane.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 2 --sbr-alterscale 1 -o out.m4a in.wav`

Współpraca i konflikty: ma sens tylko razem z `--sbr-freqscale`, bo koryguje właśnie tamtą
siatkę. Gdy celem jest zmiana barwy na górze, prawdziwą dźwignię mają `--sbr-invf*` i
`--sbr-noise-bands`; po ten warto sięgać na końcu.

#### `--sbr-noise-bands <n>`

**Ustawia, jak drobno poziom szumu SBR jest opisany wzdłuż częstotliwości.**

- Dotyczy: tylko profili z SBR
- Wartości: `0`, `1`, `2`, `3`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziom szumu to sposób, w jaki SBR wkłada z powrotem "powietrze" do skopiowanego pasma. Przy
jednym paśmie szumu na cały obszar koder wysyła jeden poziom szumu, a dekoder rozsmarowuje
go równo: tanio i błędnie zawsze wtedy, gdy górne pasmo nie jest jednorodne, na przykład
przy utrzymywanym dźwięku smyczków pod hi-hatem. Przy większej liczbie pasm szumu poziom
może podążać za materiałem, więc cichy obszar tonalny nie dostaje tyle samo syku, co obszar
ruchliwy.

Pole w nagłówku SBR ma szerokość dwóch bitów, więc ustawień jest dokładnie cztery i ani
jednego więcej. Wartości *nie* są dosłowną liczbą pasm: to gęstość, z grubsza pasma na
oktawę.

Wartości:
- `0` jedno pasmo szumu na cały obszar SBR. Najtaniej, najbardziej tępo. Przy bardzo niskim
  bitrate oszczędność jest realna; na czymkolwiek, co ma na górze strukturę, słychać szum w
  złych miejscach.
- `1` mniej więcej jedno pasmo szumu na oktawę.
- `2` mniej więcej dwa na oktawę.
- `3` mniej więcej trzy na oktawę. Najgęstszy opis, najlepsze rozdzielenie obszarów
  tonalnych od szumowych, najwięcej informacji pobocznej.
- `-1` wyłączone: decyduje tabela strojenia, zależnie od bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-noise-bands 3 -o out.m4a in.wav`

Współpraca i konflikty: to decyduje o *rozdzielczości* poziomu szumu; `--sbr-noise-max`,
`--sbr-nf-weight` i `--sbr-noise-floor-offset` decydują o jego *poziomie*. Podnoszenie
gęstości idzie na marne, jeśli poziom jest przycięty niskim sufitem, więc przy pogoni za
powietrzem warto najpierw zajrzeć do `--sbr-noise-max`.

#### `--sbr-amp-res <n>`

**Wybiera krok kwantyzacji poziomów obwiedni SBR: 1,5 dB albo 3,0 dB.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = 1,5 dB, `1` = 3,0 dB; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziomy obwiedni są wysyłane jako liczby skwantowane, a to jest rozmiar kroku. Krok 1,5 dB
śledzi zmiany poziomu w górnym paśmie dokładniej i kosztuje więcej bitów na obwiednię. Krok
3,0 dB jest tańszy i grubszy; na materiale z dużym ruchem poziomu na górze zgrubny krok
słychać jako skokowe, a nie płynne, zmiany górnego pasma.

Wartości:
- `0` kroki 1,5 dB. Drobniej, drożej. Lepszy wybór przy wyższym bitrate albo na materiale
  dynamicznym.
- `1` kroki 3,0 dB. Zgrubniej, taniej. Rozsądny kompromis, gdy bitów brakuje, i dlatego
  tabela często wybiera go nisko.
- `-1` wyłączone: decyduje tabela strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 80000 --sbr-amp-res 0 -o out.m4a in.wav`

Współpraca i konflikty: wchodzi w bezpośrednią interakcję z `--sbr-num-env` i detektorem
transjentów, bo każda dodatkowa obwiednia kosztuje pełny komplet poziomów przy obowiązującym
rozmiarze kroku. Cztery obwiednie przy rozdzielczości 1,5 dB to najdroższy róg tej grupy.

#### `--sbr-data-extra <n>`

**Zapisuje opcjonalne dodatkowe pola nagłówka SBR, zamiast je pomijać.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = nie zapisuj, `1` = zapisz; domyślnie `-1` = wartość z tabeli strojenia
  SBR
- Wpisywanie: liczba całkowita

Nagłówek SBR ma opcjonalny blok z dodatkową konfiguracją. Zapisywanie go powiększa
nieznacznie każdy nagłówek i czyni konfigurację bardziej jawną; pominięcie zostawia dekoder
z domyślnymi założeniami dla tych pól. To decyzja o formatowaniu strumienia, a nie kontrola
brzmienia, i warto jej dotykać tylko wtedy, gdy celem jest zgodność z konkretnym dekoderem
albo wgląd w nagłówki.

Wartości:
- `0` nie zapisuj dodatkowych danych nagłówka.
- `1` zapisz je.
- `-1` wyłączone: decyduje tabela strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-data-extra 1 -o out.m4a in.wav`

Współpraca i konflikty: w połączeniu z krótkim `--sbr-header-period` dodatkowe bajty
powtarzają się często, więc przy optymalizowaniu strumienia o niskim bitrate pod szybkie
zaskoczenie SBR lepiej trzymać to wyłączone.

### Rozdzielczość czasowa SBR i stereo: `--sbr-num-env` `--sbr-freqres-fixfix` `--sbr-stereo-mode` `--sbr-header-period` `--sbr-header-time`

Układ częstotliwości decyduje o tym, gdzie jest pasmo SBR. Ta sekcja decyduje, jak często
jest opisywane, jak dzieli się między kanały i jak często sama konfiguracja powtarza się w
strumieniu.

Rozdzielczość czasowa to bezpośredni kompromis między bitami a rozmazaniem. Jedna obwiednia
na ramkę oznacza jeden poziom na pasmo dla całej ramki, więc hi-hat wewnątrz tej ramki jest
uśredniany z otaczającą go ciszą. Cztery obwiednie śledzą kształt, ale czterokrotnie
zwiększają dane obwiedni. Zwykle tę decyzję podejmuje detektor transjentów dla każdej ramki,
i dlatego wymuszenie stałej liczby jest większą ingerencją, niż wygląda.

Okres nagłówka to ustawienie innego rodzaju: nie zmienia tego, jak plik brzmi, gdy
odtwarzanie już trwa, tylko to, jak długo dekoder dołączający się w środku strumienia czeka,
zanim w ogóle będzie miał górne pasmo. Ma znaczenie przy transmisji na żywo i poza tym nie
ma znaczenia nigdzie.

#### `--sbr-num-env <1|2|4>`

**Wymusza stałą liczbę obwiedni SBR na ramkę i wyłącza decyzję detektora transjentów o
podziale ramki.**

- Dotyczy: tylko profili z SBR
- Wartości: `1`, `2`, `4`; domyślnie `-1` = wyłączone, decyduje detektor transjentów dla
  każdej ramki
- Wpisywanie: liczba całkowita

Normalnie SBR używa zmiennej siatki czasowej: ramki z wykrytym atakiem dostają więcej
krótszych obwiedni, żeby atak nie został rozmazany, a ramki ustabilizowane dostają jedną.
Ten przełącznik zastępuje to siatką statyczną, taką samą w każdej ramce, niezależnie od
treści.

To czyni go dobrym narzędziem na materiał stabilny i złym na materiał perkusyjny. Na
utrzymywanym padzie, organach albo podkładzie smyczkowym wymuszenie 2 lub 4 obwiedni daje
rozdzielczość czasową, o którą detektor by nie poprosił, a koszt jest przewidywalny. Na
perkusji statyczna siatka oznacza, że ataki nie dostają już własnej obwiedni, a w zamian
pojawia się dokładnie to rozmazanie wstecz, któremu detektor ma zapobiegać.

Wartość 8 przekroczyłaby siatkę ze standardu i jest odrzucana.

Wartości:
- `1` jedna obwiednia na ramkę. Najtaniej, najbardziej tępo; górne pasmo trzyma jeden
  poziom na pasmo przez całą ramkę. Na czymkolwiek transjentowym słychać rozmazanie.
- `2` dwie obwiednie na ramkę. Złoty środek i sensowne wymuszone ustawienie na materiale
  stabilnym przy niskim bitrate.
- `4` cztery obwiednie na ramkę. Najdrobniejsza statyczna rozdzielczość, najwięcej danych
  obwiedni. Dobre na dźwięki utrzymywane i dzwoniące; na ostrych atakach nadal przegrywa z
  siatką dynamiczną, bo podziały są w stałych miejscach, a nie w miejscu ataku.
- `-1` wyłączone. Detektor transjentów wybiera dla każdej ramki. To lepsze ustawienie
  domyślne, a pokrętła do jego kształtowania są w sekcji o detektorze transjentów poniżej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-num-env 4 -o out.m4a organ.wav`

Współpraca i konflikty: to nadpisuje tę część podziału ramki, na którą wpływają
`--sbr-tran-thr` i `--sbr-tran-split`, więc łączenie ich jest sprzeczne: gdy celem jest
lepsze zachowanie na atakach, trzeba zostawić `--sbr-num-env` wyłączone i zamiast tego
obniżyć `--sbr-tran-thr`. Koszt bitowy obwiedni ustawia `--sbr-amp-res`.

#### `--sbr-freqres-fixfix <0|1>`

**Ustawia rozdzielczość częstotliwościową obwiedni FIXFIX, czyli typu obwiedni używanego w
ramkach bez wykrytego transjentu.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = niska, `1` = wysoka; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Obwiednia może opisać górne pasmo niewieloma pasmami albo wieloma. FIXFIX to kształt
obwiedni używany dla zwykłych ramek bez transjentu, a na większości materiału stanowią one
zdecydowaną większość, więc to ustawienie po cichu wpływa na większość pliku.

Niska rozdzielczość grupuje pasma razem: mniej poziomów na obwiednię, taniej i gorsza
zdolność podążania za nierównym górnym pasmem. Wysoka rozdzielczość wysyła więcej poziomów,
lepiej śledzi kształt widma i kosztuje bity, które przy niskim bitrate biorą się z rdzenia.

Wartości:
- `0` niska rozdzielczość częstotliwościowa. Taniej; górne pasmo jest opisane grubiej
  wzdłuż częstotliwości. Sensowne przy niskim bitrate i na jednorodnych górnych pasmach.
- `1` wysoka rozdzielczość częstotliwościowa. Więcej poziomów na obwiednię, lepsze
  kształtowanie górnych pasm o wyraźnej strukturze. Sensowne, gdy bity są dostępne.
- `-1` wyłączone: wybór samego kodera, zgodnie z tabelą strojenia i typem ramki.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-freqres-fixfix 1 -o out.m4a in.wav`

Współpraca i konflikty: to jest oś częstotliwości; `--sbr-num-env` to oś czasu. Oba mnożą
się w ten sam budżet danych obwiedni, a `--sbr-amp-res` ustala cenę za poziom. Dotyczy tylko
ramek bez transjentów, więc na mocno perkusyjnym materiale jego udział w pliku jest
mniejszy.

#### `--sbr-stereo-mode <1..3>`

**Wybiera, jak górne pasmo SBR jest dzielone między dwa kanały, niezależnie od kodowania
stereo w rdzeniu.**

- Dotyczy: tylko profili z SBR, wejście stereo
- Wartości: `1` LR, `2` coupling, `3` switch-LRC; domyślnie `-1` = wyłączone (decyduje
  tabela strojenia: `3` poniżej 60 kbps stereo, `1` od 60 kbps)
- Wpisywanie: liczba całkowita

Rdzeń i SBR podejmują osobne decyzje stereo. Rdzeń może kodować mid/side, podczas gdy SBR
stosuje coupling, albo odwrotnie. Ten przełącznik dotyczy wyłącznie górnego pasma.

Pełna separacja lewy/prawy wysyła niezależny komplet parametrów SBR dla każdego kanału:
najszersza, najwierniejsza góra, za cenę mniej więcej podwojonej informacji pobocznej SBR.
Coupling wysyła jedną wspólną obwiednię oraz informację o poziomie i balansie, co jest
znacznie tańsze i ściąga obraz stereo na górze w stronę środka. Na większości materiału
słychać to mniej, niż można by się spodziewać, i dlatego jest to normalny wybór przy niskim
bitrate'ie; na szeroko i ostro spanoramowanych talerzach oraz na nagraniach z prawdziwą
różnicą akustyki między kanałami słychać to wyraźnie.

Wartości:
- `1` LR, pełna separacja. Niezależne górne pasmo dla każdego kanału. Tego trybu FDK używa
  od 60 kbps stereo; przy 32 kbps jest kosztowny.
- `2` coupling. Wspólna obwiednia oraz poziom i balans w każdej ramce. Tanio, węższa góra.
- `3` switch-LRC. Koder w każdej ramce wybiera między LR a couplingiem. Tego trybu FDK
  używa poniżej 60 kbps stereo.
- `-1` wyłączone: decyduje tabela strojenia, jak wyżej.

Wejście mono nie wymaga przełącznika: FDK sam koduje jednokanałowe górne pasmo. Trybu mono
dla wejścia stereo nie ma; `0` jest odrzucane.

Przykład w kontekście: `fdkaac -p 5 -b 48000 --sbr-stereo-mode 1 -o out.m4a in.wav`, pełna
separacja poniżej progu 60 kbps.

Współpraca i konflikty: w `-p 29` (HE-AAC v2) obraz stereo opisuje parametric stereo, a
część SBR jest mono, więc ten przełącznik nie ma tam czego rozdzielać. Przełączniki stereo
rdzenia z grupy B to osobna decyzja i go nie nadpisują.

Zmierzone: HE-AAC, wejście 48 kHz stereo; strumień bez przełącznika jest identyczny co do
bajtu z `3` przy 24, 32, 40, 48, 56 i 59 kbps oraz z `1` przy 60, 64 i 96 kbps. Przed beta.2
wartość `0` była przyjmowana i przy wejściu stereo wywracała koder, bez komunikatu i z
pustym plikiem.

#### `--sbr-header-period <n>`

**Ustawia, co ile ramek zapisywane są nagłówki SBR, co decyduje, jak długo dekoder
dołączający się do strumienia na żywo słyszy sam rdzeń, zanim pojawi się górne pasmo.**

- Dotyczy: tylko profili z SBR. W HE-AAC i HE-AAC v2 steruje nagłówkiem `sbr_header` w
  strumieniu; w AAC-ELD z SBR nie ma nagłówka SBR w strumieniu, więc okres dotyczy
  StreamMuxConfig w LATM/LOAS i wymaga `-f 6`, `-f 7` albo `-f 10`
- Wartości: ramki, `1` wzwyż; domyślnie `-1` = wyłączone, własny okres FDK około 10 ramek
  (około 0,46 s przy 44,1 kHz)
- Wpisywanie: liczba całkowita

Cała konfiguracja SBR jest zapisana w okresowym nagłówku, a nie w każdej ramce. Słuchacz,
który podłącza się do strumienia Icecast, Shoutcast albo HLS w środku, od razu dekoduje
rdzeń i słyszy stłumioną, obciętą dolnoprzepustowo wersję programu aż do nadejścia kolejnego
nagłówka SBR. Wtedy pojawia się góra. W pliku jest to niewidoczne, bo odtwarzanie zaczyna
się od początku; w strumieniu na żywo to pierwsza rzecz, jaką słyszy nowy słuchacz.

Zapisywanie nagłówka w każdej ramce niemal całkowicie usuwa ten moment, kosztem bajtów
nagłówka w każdej ramce. Przy niskim bitrate te bajty to nie jest nic. Rozciągnięcie okresu
je oszczędza i wydłuża moment z samym rdzeniem.

Wartość jest brana dosłownie, bez nakładania sufitu jednej sekundy.

Przykłady, skracanie okresu:
- Łagodnie: `--sbr-header-period 5`. Z grubsza o połowę skraca czekanie względem wartości
  domyślnej.
- Mocno: `--sbr-header-period 2`. Nagłówek co drugą ramkę; moment z samym rdzeniem spada do
  dziesiątek milisekund.
- Przesadnie: `--sbr-header-period 1`. Nagłówek w każdej pojedynczej ramce, niemal
  natychmiastowe zaskoczenie SBR, maksymalny narzut. Przy strumieniu 24 kbps ten narzut to
  realna część budżetu, i dlatego tam jest ustawieniem skrajnym, a przy 96 kbps rozsądnym
  wyborem.

Przykłady, wydłużanie okresu:
- Łagodnie: `--sbr-header-period 25`. Około sekundy między nagłówkami przy 44,1 kHz. Ledwie
  zauważalne na strumieniu, przy którym ludzie zostają.
- Mocno: `--sbr-header-period 100`. Kilka sekund dźwięku z samego rdzenia dla nowego
  słuchacza. Oszczędza bajty nagłówków, brzmi jak zepsute dla każdego, kto przeskakuje
  między stacjami.
- Przesadnie: `--sbr-header-period 5000`. Minuty między nagłówkami. Dla pliku nieszkodliwe;
  dla strumienia oznacza, że większość słuchaczy nigdy nie usłyszy górnego pasma.

Współpraca i konflikty: `--sbr-header-time` wyraża to samo w sekundach i wygrywa, gdy podane
są oba. `--sbr-data-extra 1` powiększa każdy nagłówek, więc razem te dwa mnożą narzut.
`--verbose` wypisuje efektywny okres w ramkach i w milisekundach.

#### `--sbr-header-time <s>`

**To samo ustawienie co `--sbr-header-period`, wyrażone w sekundach zamiast w ramkach.**

- Dotyczy: tylko profili z SBR; w AAC-ELD z SBR ustawia okres StreamMuxConfig w LATM/LOAS,
  więc trzeba użyć `-f 6`, `-f 7` albo `-f 10`
- Wartości: `0.01..600` sekund; domyślnie `-1` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Ramki są niewygodną jednostką, gdy operator strumienia określa wymaganie, bo czas trwania
ramki zależy od częstotliwości próbkowania i profilu. Ten przełącznik bierze sekundy wprost
i przelicza je sam, więc ta sama linia poleceń zachowuje się tak samo przy 44,1 i 48 kHz.

Wygrywa z `--sbr-header-period`, gdy podane są oba.

Przy AAC-ELD w ADTS albo M4A nie ma czego powtarzać: konfiguracja siedzi tylko w kontenerze,
a `--verbose` raportuje okres nagłówka SBR jako okres konfiguracji LATM/LOAS, a nie jako
nagłówek SBR w strumieniu.

Przykłady, skracanie czasu:
- Łagodnie: `--sbr-header-time 0,25`. Ćwierć sekundy czekania w najgorszym przypadku.
- Mocno: `--sbr-header-time 0,05`. Praktycznie natychmiastowe zaskoczenie dla słuchacza,
  który dołącza w losowym momencie.
- Przesadnie: `--sbr-header-time 0.01`, czyli minimum. Przy większości częstotliwości
  próbkowania to mniej niż ramka, więc sprowadza się to do nagłówka w każdej ramce z
  maksymalnym narzutem, jaki z tego wynika.

Przykłady, wydłużanie czasu:
- Łagodnie: `--sbr-header-time 1`. Jedna sekunda, blisko typowego wymagania operatorów.
- Mocno: `--sbr-header-time 3.5`. Trzy i pół sekundy dźwięku z samego rdzenia dla nowego
  słuchacza. Zmierzone po stronie dekodera przy tym ustawieniu: najdłuższe zaobserwowane
  oczekiwanie na górne pasmo po dołączeniu w środku strumienia wyniosło 3,41 s dla HE-AAC,
  3,46 s dla HE-AAC v2 i 3,52 s dla AAC-ELD.
- Przesadnie: `--sbr-header-time 600`, czyli maksimum, dziesięć minut. Legalne, a dla
  strumienia na żywo nie do odróżnienia od wysyłania strumienia z samym rdzeniem.

Współpraca i konflikty: nadpisuje `--sbr-header-period`. Wynik widać w `--verbose`, który
wypisuje efektywny okres na oba sposoby. Jako separator dziesiętny przyjmowane są zarówno
`.`, jak i `,`.

### Detektor transjentów SBR: `--sbr-tran-peak` `--sbr-tran-quiet` `--sbr-tran-dom` `--sbr-tran-thr` `--sbr-tran-split`

SBR dzieli każdą ramkę na szczeliny czasowe i na podstawie energii w tych szczelinach
rozstrzyga, czy w górnym paśmie coś zaatakowało. Jeśli uzna, że tak, atak dostaje własną
krótką obwiednię, a skok poziomu jest zakodowany tam, gdzie faktycznie następuje. Jeśli
uzna, że nie, jedna obwiednia pokrywa cały odcinek, a energia uderzenia zostaje po nim
uśredniona.

Uśrednianie przez atak jest słyszalne i słychać je *przed* atakiem, a nie po nim: tuż przed
hi-hatem albo klaśnięciem pojawia się krótki wybuch wysokoczęstotliwościowego syku. To
pre-echo, a w paśmie odtworzonym przez SBR jest to jeden z łatwiej rozpoznawalnych
artefaktów kodowania, bo materiał na górze jest już i tak dość syntetyczny, więc rozmycie
odbiera się jako szum, a nie jako miękki atak.

Do tego detektora sięga pięć pokręteł. Jedno z nich, `--sbr-tran-thr`, jest znacznie
skuteczniejsze od pozostałych i od niego warto zacząć. Dwa z nich, `--sbr-tran-quiet` i
`--sbr-tran-dom`, siedzą w drugim, szybszym detektorze, który działa tylko w AAC-ELD z SBR,
więc w HE-AAC i HE-AAC v2 nie robią nic.

Wszystkie pięć to pokrętła x100 i wszystkie działają w obu kierunkach: `100` oznacza
dokładnie stałą fabryczną, poniżej 100 stała maleje, powyżej 100 rośnie. `--sbr-tran-peak`
jest wyjątkiem co do znaczenia, nie co do formatu: zastępuje stałą, zamiast ją skalować.

#### `--sbr-tran-thr <n>`

**Skaluje główny próg, który decyduje o tym, jak chętnie detektor transjentów SBR w ogóle
się odpala, i jest to jedyne pokrętło w tej sekcji o dużym, zmierzonym efekcie.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor porównuje energie szczelin z progiem wyznaczonym z sygnału. Po obniżeniu progu
ataki z pogranicza, które wcześniej przechodziły niezauważone, dostają własną obwiednię; po
jego podniesieniu własną obwiednię dostają tylko najbardziej oczywiste uderzenia. Skoro to
przeoczony atak produkuje pre-echo, obniżanie tej wartości jest lekarstwem wprost, a kosztem
są dane obwiedni: więcej wykrytych transjentów to więcej obwiedni, a więcej obwiedni to
więcej bitów, zabranych rdzeniowi.

To pokrętło, które uzasadnia istnienie całej grupy. Na materiale perkusyjnym usuwa pre-echo
w górnym paśmie niemal całkowicie.

Zmierzone, pre-echo w paśmie SBR. Sygnał testowy: 24 ataki perkusyjne o charakterze hi-hatu
i klaśnięcia plus cztery wybrzmiewające tony między 8 a 13 kHz. Metryka: energia powyżej 9
kHz pojawiająca się w dwóch okienkach 5,8 ms przed każdym atakiem, której nie ma w źródle,
więc mniej znaczy lepiej. Zakodowane jako HE-AAC, zdekodowane, wcześniej dopasowane w czasie
korelacją wzajemną:

| `--sbr-tran-thr` | pre-echo przy 32 kbps | pre-echo przy 64 kbps |
|---|---|---|
| fabryczne | 1.610 | 1.186 |
| 20 / 40 / 60 | 0.041 | 0.059 |
| 80 | 0.661 | 0.502 |
| 100 i więcej | 1.610 (= fabryczne) | 1.186 (= fabryczne) |

Efekt jest monotoniczny, odtwarzalny na obu bitrate'ach i poniżej 60 wchodzi w nasycenie.
Wartości 150 i 400 też zmieniają strumień, tylko w drugą stronę.

Przykłady, obniżanie progu (detektor odpala się chętniej):
- Łagodnie: `--sbr-tran-thr 80`. Pre-echo na zmierzonym sygnale zmniejszone z grubsza o
  połowę, umiarkowany dodatkowy koszt obwiedni.
- Mocno: `--sbr-tran-thr 40`. Na materiale perkusyjnym pre-echo praktycznie znika. To
  zalecany punkt startu, gdy na górze słychać rozmycie.
- Przesadnie: `--sbr-tran-thr 1`. Detektor odpala się na niemal wszystkim. Poniżej 60 nie
  ma już nic do ugrania w pre-echu, więc rosną tylko dane obwiedni wydane na
  niezdarzenia, co przy niskim bitrate'ie zabiera bity rdzeniowi i pogarsza cały plik.

Przykłady, podnoszenie progu (detektor odpala się rzadziej):
- Łagodnie: `--sbr-tran-thr 150`. Ataki z pogranicza są ignorowane, trochę mniej obwiedni,
  trochę więcej rozmycia. Czasem warto na materiale spokojnym przy bardzo ciasnym budżecie.
- Mocno: `--sbr-tran-thr 400`. Własną obwiednię dostają tylko ataki niebudzące
  wątpliwości. Na perkusji słychać wyraźnie więcej pre-echa.
- Przesadnie: `--sbr-tran-thr 10000`. Detektor praktycznie nigdy się nie odpala; każda
  ramka jest opisana statyczną siatką. Ustawienie skrajne, i to przydatne, gdy celem jest
  usłyszenie artefaktu, któremu ten detektor ma zapobiegać.

Współpraca i konflikty: `--sbr-num-env` wymusza statyczną siatkę, a więc usuwa korzyść z
podziału czasu, którą kupuje to pokrętło, co sprawia, że łączenie ich jest wzajemnym
sabotażem. `--sbr-tran-split` zajmuje się osobnym pytaniem, czyli dzieleniem ramek, w
których żaden transjent nie został wykryty. Na pre-echo w *rdzeniu* (LC, LD, ELD) narzędzia
są w grupie E.

#### `--sbr-tran-split <n>`

**Skaluje próg dzielenia na dwie obwiednie ramki, w której nie wykryto żadnego
transjentu.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Nie każda zmiana poziomu w górnym paśmie jest atakiem. Crescendo, narastanie, zmiana akordu
albo nierówno wybrzmiewający talerz poruszają górnym pasmem, nie wyzwalając detektora
transjentów, a jedna obwiednia na całą ramkę ten ruch uśrednia. Ten próg rozstrzyga, jak
chętnie taka ramka zostanie jednak podzielona na dwie, tak żeby druga połowa dostała własne
poziomy.

Obniżanie dzieli częściej: drobniejsza rozdzielczość czasowa na materiale, który się
porusza, ale nie atakuje, kosztem drugiego zestawu poziomów obwiedni w wielu ramkach.
Podnoszenie dzieli rzadziej: taniej, a górne pasmo mniej dokładnie podąża za powolnym
ruchem.

Efekt jest mniejszy niż przy `--sbr-tran-thr` i dotyczy innego objawu. Gdy problemem jest
syk przed uderzeniem, trzeba sięgnąć po próg; gdy problemem jest górne pasmo brzmiące
statycznie w czasie narastania muzyki, po to pokrętło.

Przykłady, obniżanie (dzielenie częściej):
- Łagodnie: `--sbr-tran-split 70`. Trochę więcej ramek dostaje dwie obwiednie.
- Mocno: `--sbr-tran-split 30`. Większość ramek z jakimkolwiek ruchem zostaje podzielona.
  Zauważalnie więcej danych obwiedni, lepsze podążanie za narastaniami.
- Przesadnie: `--sbr-tran-split 1`. Niemal każda ramka podzielona bez względu na treść;
  koszt bitowy jest realny, a korzyść na materiale spokojnym żadna.

Przykłady, podnoszenie (dzielenie rzadziej):
- Łagodnie: `--sbr-tran-split 150`. Marginalnie mniej podziałów, marginalnie taniej.
- Mocno: `--sbr-tran-split 300`. Podziały stają się rzadkie. Zmierzone: to zmienia
  strumień HE-AAC.
- Przesadnie: `--sbr-tran-split 10000`. Żadna ramka nie zostaje podzielona wyłącznie z
  powodu ruchu. Jedna obwiednia na ramkę bez transjentu, maksymalne rozmycie powolnych
  zmian w górnym paśmie, minimum danych obwiedni.

Współpraca i konflikty: w praktyce ignorowane, gdy `--sbr-num-env` wymusza statyczną siatkę.
Uzupełnia `--sbr-tran-thr`, a nie dubluje: tamto dotyczy ataków, to ramek bez ataku. Każda
dodatkowa obwiednia jest wyceniana przez `--sbr-amp-res`.

#### `--sbr-tran-peak <n>`

**Ustala, jak szczytowy musi być kandydat, żeby policzył się jako atak: sygnał musi spaść
poniżej tego ułamka poprzedniej szczeliny.**

- Dotyczy: profili z SBR
- Wartości: `1..200`, surowe x100, gdzie `90` = fabryczne 0,90; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Prawdziwy atak nie jest tylko głośny, jest też krótki: energia rośnie, a potem znów spada.
Detektor sprawdza ten spadek, wymagając, żeby kolejna szczelina spadła poniżej ułamka
poprzedniej. Tym ułamkiem jest właśnie ta stała.

Uwaga na jednostkę. To **surowa** wartość x100, a nie współczynnik skalujący: zastępuje
stałą wprost, więc `90` to fabryczne 0,90 i nic nie zmienia, a `100` to 1,0, a nie wartość
fabryczna. A 1,0 ma szczególną konsekwencję: ułamek równy 1,0 albo większy oznacza, że
warunek nigdy nie może zostać spełniony przez prawdziwy spadek w taki sposób, w jaki
detektor go sprawdza, więc żadna szczelina nigdy nie liczy się jako spadająca. Każda wartość
od 100 do 200 produkuje zatem jeden i ten sam strumień. Żywy zakres to 1..99.

Obniżanie wartości wymaga ostrzejszego spadku, więc jako ataki kwalifikuje się mniej
kandydatów. Podnoszenie w stronę 99 przyjmuje za ataki łagodniejsze wybrzmienia, więc
kwalifikuje się ich więcej.

Przykłady, obniżanie (surowiej, mniej transjentów):
- Łagodnie: `--sbr-tran-peak 80`. Trochę surowiej niż fabrycznie; miękkie ataki przestają
  się kwalifikować.
- Mocno: `--sbr-tran-peak 50`. Liczą się tylko ostro wybrzmiewające uderzenia. Na
  wszystkim poza suchą perkusją słychać więcej rozmycia.
- Przesadnie: `--sbr-tran-peak 1`. Wymóg spadku jest absurdalny; prawie nic nie kwalifikuje
  się jako atak i sam test szczytu tłumi detektor.

Przykłady, podnoszenie (luźniej, więcej transjentów):
- Łagodnie: `--sbr-tran-peak 95`. Łagodniejsze wybrzmienia liczą się już jako ataki.
- Mocno: `--sbr-tran-peak 99`. Najluźniejsze ustawienie, które jeszcze cokolwiek robi.
  Ataki z długimi ogonami dostają własne obwiednie.
- Przesadnie: `--sbr-tran-peak 200`. Poza użytecznym zakresem: 100 i więcej znaczy zawsze
  1,0, żadna szczelina nie liczy się jako spadająca, a wartości od 100 do 200 to jeden i
  ten sam strumień. Przydatne tylko jako pokaz, że to pokrętło ma twardą krawędź.

Współpraca i konflikty: to jeden z kilku warunków; `--sbr-tran-thr` skaluje główny próg i ma
znacznie większą dźwignię. Gdy celem jest w ogóle więcej albo mniej transjentów, warto
zacząć od tamtego, a tego użyć tylko do zmiany *charakteru* tego, co się kwalifikuje.

#### `--sbr-tran-quiet <n>`

**Ustala najniższy dozwolony poziom energii cichej szczeliny w szybkim detektorze
transjentów SBR, co rozstrzyga, jak chętnie ciche fragmenty produkują transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2, które
  używają drugiego detektora
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

AAC-ELD z SBR uruchamia drugi detektor transjentów, o niższym opóźnieniu, a ten detektor ma
dolną granicę energii, poniżej której szczelina jest traktowana jako cicha. Granica istnieje
po to, żeby szum w ciszy nie rejestrował się jako ataki. Po jej obniżeniu detektor zagląda w
cichsze fragmenty, więc miękkie uderzenia w rzadkiej aranżacji mogą dostać własną obwiednię.
Po jej podniesieniu cichy materiał jest ignorowany.

Zachowanie silnie zależy od materiału, co warto wiedzieć przed poświęceniem temu czasu: na
sygnale testowym czysto tonalnym detektor nigdy nie doszedł do decyzji, którą to pokrętło
mogłoby zmienić.

Zmierzone na AAC-ELD z SBR, 24 kbps, na pięciu sygnałach testowych: wartości od 30 do 99
zmieniają strumień, `100` i więcej nie dały żadnej zmiany, a `1` i `10` dają ten sam
strumień co siebie wzajemnie. Żywy zakres to więc w dół z grubsza 11..99, z nasyceniem
poniżej i sufitem przy 100.

Przykłady, obniżanie (ciche fragmenty chętniej produkują transjenty):
- Łagodnie: `--sbr-tran-quiet 90`. Niewielkie rozszerzenie na cichsze szczeliny.
- Mocno: `--sbr-tran-quiet 50`. Wyraźnie czulej na materiale rzadkim; zmierzone, że
  zmienia strumień.
- Przesadnie: `--sbr-tran-quiet 1`. Na samym dole: `1` i `10` dają ten sam strumień, więc
  detektor dobił do dna tego, co ta dolna granica potrafi wyrazić.

Przykłady, podnoszenie (ciche fragmenty ignorowane):
- Od łagodnie do przesadnie: `--sbr-tran-quiet 120`, `300`, `10000` nie dały w naszych
  pomiarach żadnej zmiany strumienia na ELD plus SBR 24 kbps. Fabryczna dolna granica jest
  już tak wysoka, że dalsze jej podnoszenie nie zmienia na tym materiale żadnej decyzji.
  Uczciwy status: kierunek jest podłączony, efektu w górę nie zaobserwowano.

Współpraca i konflikty: nic w HE-AAC ani HE-AAC v2, bo szybki detektor nie jest tam
używany. W obrębie ELD plus SBR stoi obok `--sbr-tran-dom`; `--sbr-tran-thr` i
`--sbr-tran-split` dotyczą głównego detektora.

#### `--sbr-tran-dom <n>`

**Ustala, o ile głośniejsza musi być szczelina od dwóch poprzedzających, kiedy te są już
oznaczone jako transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2
- Wartości: `100..500`, x100, gdzie `140` = fabryczne 1,4; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Werbel, flam albo podwójne uderzenie produkują kilka szczelin kandydujących z rzędu. Ta
reguła nie pozwala detektorowi oznaczyć wszystkich: gdy dwie poprzedzające szczeliny są już
transjentami, następna musi być wyraźnie głośniejsza, o ten właśnie współczynnik, żeby też
została oznaczona. To test dominacji, stąd nazwa.

Obniżanie współczynnika w stronę 100 pozwala liczyć się wszystkim kolejnym szczelinom, więc
gęsta perkusja produkuje więcej transjentów. Podnoszenie oznacza, że liczy się tylko
zdecydowana eskalacja, więc werbel rejestruje się raz.

Uczciwy status: to pokrętło jest podłączone do stałej, którą dokumentuje, i w naszych
pomiarach nie dało żadnej zmiany strumienia. Wartości od 100 do 500 dały identyczne wyjście
na pięciu sygnałach testowych na ELD plus SBR. Powód jest najprawdopodobniej taki, że
warunki wokół rzadko są na tym materiale spełnione jednocześnie, więc o wyniku nie decyduje
test dominacji. Warto traktować je jako dostępne do eksperymentów, a nie jako narzędzie o
przewidywalnym rezultacie, i sięgać raczej po `--sbr-tran-thr`.

Przykłady, obniżanie (kolejne szczeliny liczą się chętniej):
- Łagodnie: `--sbr-tran-dom 130`. Trochę poniżej ustawienia fabrycznego.
- Mocno: `--sbr-tran-dom 110`. Praktycznie żaden wymóg dominacji.
- Przesadnie: `--sbr-tran-dom 100`. Współczynnik wynosi 1,0, więc szczelina musi jedynie
  dorównać dwóm przed nią. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (liczy się tylko zdecydowana eskalacja):
- Łagodnie: `--sbr-tran-dom 180`. Powyżej ustawienia fabrycznego.
- Mocno: `--sbr-tran-dom 300`. Szczelina musi mieć trzykrotny poziom swoich poprzedników.
- Przesadnie: `--sbr-tran-dom 500`, maksimum. Przy żadnej z tych wartości nie
  zaobserwowano zmiany strumienia.

Współpraca i konflikty: ten sam szybki detektor ELD plus SBR co przy `--sbr-tran-quiet` i to
samo ograniczenie: nic w HE-AAC. Gdy celem jest słyszalna zmiana zachowania transjentowego
tego detektora, efekt zmierzony ma `--sbr-tran-quiet` w zakresie 30..99.

### Detektor brakujących harmonicznych: `--sbr-mh-tone` `--sbr-mh-diff` `--sbr-mh-decay-orig` `--sbr-mh-decay-diff` `--sbr-mh-sfm-sbr` `--sbr-mh-sfm-orig` `--sbr-mh-maxcomp` `--sbr-mh-deltatime`

Detektor transjentów zajmuje się czasem. Ten detektor zajmuje się problemem, który tworzy
samo kopiowanie.

SBR buduje górne pasmo, odtwarzając je z widma z dołu. Ton, który istniał w oryginalnym
górnym paśmie, nie ma gwarancji, że to zostanie: odtworzenie podnosi to, co akurat było w
obszarze źródłowym, więc prawdziwa składowa może wylądować na złej częstotliwości, przybyć w
towarzystwie składowych, których nigdy tam nie było, albo po prostu zniknąć. Na dzwonkach,
trójkącie, dzwonie albo jasnej harmonicznej smyczków to jest dokładnie ta treść, którą słuch
wychwytuje. Detektor brakujących harmonicznych porównuje tonalność oryginalnego pasma z
tonalnością pasma odtworzonego przez SBR, rozstrzyga, że tonu brakuje, i sygnalizuje
dekoderowi, żeby wstawił w tym paśmie syntetyczną sinusoidę.

Oba kierunki porażki są słyszalne. Zbyt gorliwa praca detektora daje świstanie: stałe,
dźwięczne tony na górze, których nigdy nie było w nagraniu, często trochę obok prawdziwego
szeregu harmonicznego, czyli klasyczne "falowanie SBR". Zbyt zachowawcza sprawia, że dzwony
oraz talerze matowieją, gdy ich składowe po cichu znikają.

Do tego detektora sięga osiem pokręteł. Przed poświęceniem im czasu warto poznać ich
prawdziwy status: na naszym materiale testowym tylko dwa z nich w ogóle poruszyły strumień.
`--sbr-mh-tone` zmieniło wyjście na sygnale tonalnym w HE-AAC 24 kbps przy wartościach 1, 5
i 10, a `--sbr-mh-diff` przy wartości 10. Pozostałych sześć nie dało żadnej zmiany
strumienia w HE-AAC od 24 do 96 kbps na materiale tonalnym i mieszanym, w żadnym kierunku.
To właściwość tego detektora na tym materiale, a nie skalowania: pokrętła x100 działają tu w
obu kierunkach, a `100` to dokładnie ustawienie fabryczne. Detektor po prostu potrzebuje
zbieżności kilku warunków, zanim którykolwiek z nich zdecyduje o wyniku, a na zwyczajnej
muzyce rzadko do tego dochodzi. Każda karta poniżej mówi wprost, jak z tym jest.

#### `--sbr-mh-tone <n>`

**Skaluje to, jak tonalne musi być pasmo, zanim SBR sfabrykuje w nim syntetyczną
harmoniczną, i jest to główna kontrolka tej sekcji.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor mierzy tonalność w oryginalnym paśmie i porównuje ją z progiem. Skalowanie tego
progu w dół sprawia, że więcej pasm kwalifikuje się jako tonalne, więc dodawanych jest
więcej syntetycznych harmonicznych. Skalowanie w górę sprawia, że kwalifikuje się ich mniej.

To, co słychać w każdym kierunku, jest konkretne. Więcej dodanych tonów oznacza, że więcej
dźwięcznej treści dzwonu albo talerza zostaje przywrócone, a zarazem rośnie ryzyko
artefaktów świstania, których nigdy nie było w źródle. Mniej dodanych tonów oznacza
bezpieczniejszą, bardziej matową górę, w której jasna perkusja traci dzwonienie.

Zmierzone: to jedno z dwóch pokręteł w tej sekcji, które poruszyły strumień, na tonalnym
sygnale testowym w HE-AAC 24 kbps, przy wartościach 1, 5 i 10. Wszystkie leżą daleko poniżej
ustawienia fabrycznego, co pokazuje, jak nisko musi zejść próg, żeby detektor zmienił zdanie
na tym materiale.

Przykłady, obniżanie progu (więcej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 70`. Umiarkowane poszerzenie tego, co liczy się jako tonalne. Na
  wielu materiałach w ogóle nie zmieni to strumienia.
- Mocno: `--sbr-mh-tone 10`. Zmierzone, że zmienia strumień na materiale tonalnym. Słychać
  więcej dodanych tonów i jaśniejsze, bardziej dźwięczne górne pasmo.
- Przesadnie: `--sbr-mh-tone 1`. Niemal każde pasmo liczy się jako tonalne. Zmierzone, że
  zmienia strumień, i to kierunek do wybrania, gdy celem jest usłyszenie, jak brzmią
  artefakty świstania.

Przykłady, podnoszenie progu (mniej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 150`. Trochę surowiej.
- Mocno: `--sbr-mh-tone 400`. Harmoniczną dostają tylko pasma silnie tonalne.
- Przesadnie: `--sbr-mh-tone 10000`. Detektor praktycznie nigdy nie dodaje harmonicznej.
  Powyżej 100 nie zaobserwowano na naszym materiale zmiany strumienia, więc przy takim celu
  trzeba to najpierw sprawdzić na własnym materiale.

Współpraca i konflikty: `--sbr-mh-diff` to druga ścieżka wyzwalania tej samej decyzji, a
`--sbr-mh-sfm-orig` / `--sbr-mh-sfm-sbr` przesuwają granicę między tonem a szumem, która
zasila pomiar tonalności. `--sbr-mh-maxcomp` rządzi tym, co dzieje się wokół harmonicznej
po jej dodaniu, a nie tym, czy zostaje dodana.

#### `--sbr-mh-diff <n>`

**Skaluje różnicę tonalności między oryginałem a pasmem odtworzonym przez SBR, która
wyzwala kompensację harmonicznych.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

`--sbr-mh-tone` pyta "czy oryginalne pasmo jest dość tonalne". To pyta o coś innego: "czy
odtworzenie zmieniło tonalność". To ścieżka, która wyłapuje przypadek, gdy oryginał miał
jeden czysty ton, a pasmo odtworzone przez SBR ma ich kilka, albo odwrotnie, czyli dokładnie
to, co psuje kopiowanie.

Obniżanie progu sprawia, że mniejsza różnica liczy się jako szkoda, więc kompensacja wyzwala
się częściej. Podnoszenie oznacza, że liczy się tylko duża niezgodność.

Zmierzone: to drugie z dwóch pokręteł w tej sekcji, które poruszyły strumień, przy wartości
10 na naszym materiale. Wartości bliższe ustawieniu fabrycznemu oraz wartości powyżej niego
nie dały żadnej zmiany.

Przykłady, obniżanie (kompensacja przy mniejszych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 70`. Umiarkowanie; na danym materiale może nie zmienić nic.
- Mocno: `--sbr-mh-diff 20`. Blisko wartości, przy której widzieliśmy zmianę.
- Przesadnie: `--sbr-mh-diff 10`. Zmierzone, że zmienia strumień. Niemal każda niezgodność
  tonalności między oryginałem a pasmem odtworzonym przez SBR wyzwala kompensację.

Przykłady, podnoszenie (kompensacja tylko przy dużych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 150`.
- Mocno: `--sbr-mh-diff 400`.
- Przesadnie: `--sbr-mh-diff 10000`. Ścieżka różnicowa praktycznie nigdy się nie odpala,
  zostawiając `--sbr-mh-tone` jako jedyny wyzwalacz. W górę nie zaobserwowano na naszym
  materiale zmiany strumienia.

Współpraca i konflikty: obie ścieżki wyzwalania, ta i `--sbr-mh-tone`, zasilają tę samą
decyzję, więc podnoszenie jednej przy obniżaniu drugiej częściowo się znosi. Pokrętła
płaskości `--sbr-mh-sfm-sbr` i `--sbr-mh-sfm-orig` rozstrzygają, co w ogóle liczy się jako
tonalne po każdej stronie porównania.

#### `--sbr-mh-decay-orig <n>`

**Skaluje to, jak długo już wykryty ton jest nadal śledzony w oryginalnym paśmie, gdy
wybrzmiewa.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor, który patrzyłby tylko na bieżącą ramkę, porzucałby ton, gdy tylko spadnie poniżej
progu, a na ogonie dzwonu albo talerza jest to dokładnie moment, w którym ton jest jeszcze
potrzebny. Dlatego detektor nadal śledzi ton, który już znalazł, z wybrzmiewaniem. To
pokrętło skaluje długość tej pamięci.

Dłuższe śledzenie oznacza, że syntetyczne harmoniczne utrzymują się w ogonach, więc
wybrzmienia pozostają dźwięczne, zamiast zamieniać się w szum. Krótsze śledzenie oznacza, że
tony są porzucane szybciej, więc ogony tracą dzwonienie wcześniej.

Uczciwy status: podłączone, efektu nie zaobserwowano. Na materiale tonalnym i mieszanym w
HE-AAC od 24 do 96 kbps nie dało to żadnej zmiany strumienia w żadnym kierunku.
Wybrzmiewanie ma znaczenie dopiero wtedy, gdy detektor znalazł ton do śledzenia, a warunki
wyzwalania rzadko zbiegały się na naszym materiale.

Przykłady, skracanie śledzenia:
- Łagodnie: `--sbr-mh-decay-orig 70`.
- Mocno: `--sbr-mh-decay-orig 30`.
- Przesadnie: `--sbr-mh-decay-orig 1`. Praktycznie brak pamięci: ton jest zapominany, gdy
  tylko spadnie. Nie zaobserwowano zmiany strumienia.

Przykłady, wydłużanie śledzenia:
- Łagodnie: `--sbr-mh-decay-orig 150`.
- Mocno: `--sbr-mh-decay-orig 400`.
- Przesadnie: `--sbr-mh-decay-orig 10000`. Raz znaleziony ton jest śledzony niemal
  bezterminowo. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: ma znaczenie tylko tam, gdzie detektor się odpala, więc obniżenie
`--sbr-mh-tone` do jego zmierzonego zakresu aktywnego jest warunkiem wstępnym, żeby to
pokrętło w ogóle coś znaczyło. `--sbr-mh-decay-diff` to ten sam mechanizm na prowadnicy
różnicowej.

#### `--sbr-mh-decay-diff <n>`

**To samo wybrzmiewanie co w `--sbr-mh-decay-orig`, zastosowane do prowadnicy różnicowej, a
nie do oryginalnego pasma.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor prowadzi dwie prowadnice śledzące: jedną podążającą za tonami w oryginalnym paśmie,
drugą podążającą za różnicą tonalności między oryginałem a pasmem odtworzonym przez SBR. To
pokrętło skaluje pamięć drugiej, więc rządzi tym, jak długo wykryta *niezgodność* jest nadal
traktowana jako obecna po tym, gdy przestanie być oczywista.

Dłużej oznacza, że decyzje o kompensacji utrzymują się między ramkami i są stabilniejsze.
Krócej oznacza, że są przeliczane niemal od zera w każdej ramce, co reaguje szybciej i może
migotać.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps, w żadnym kierunku.

Przykłady, skracanie:
- Łagodnie: `--sbr-mh-decay-diff 70`.
- Mocno: `--sbr-mh-decay-diff 30`.
- Przesadnie: `--sbr-mh-decay-diff 1`. Brak pamięci na prowadnicy różnicowej. Nie
  zaobserwowano zmiany strumienia.

Przykłady, wydłużanie:
- Łagodnie: `--sbr-mh-decay-diff 150`.
- Mocno: `--sbr-mh-decay-diff 400`.
- Przesadnie: `--sbr-mh-decay-diff 10000`. Prowadnica nigdy nie zapomina. Nie zaobserwowano
  zmiany strumienia.

Współpraca i konflikty: tworzy parę z `--sbr-mh-decay-orig` i podobnie jak ono zależy od
tego, czy `--sbr-mh-diff` faktycznie się wyzwala, zanim długość pamięci może mieć znaczenie.

#### `--sbr-mh-sfm-sbr <n>`

**Skaluje próg płaskości widmowej, powyżej którego pasmo odtworzone przez SBR jest uznawane
za szumopodobne, a nie tonalne.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Płaskość widmowa mierzy, jak równo energia rozkłada się w paśmie: blisko 0 dla jednego
czystego tonu, blisko 1 dla szumu. Detektor używa jej do klasyfikowania każdego pasma, a to
pokrętło przesuwa granicę dla strony *odtworzonej przez SBR* w tym porównaniu.

Obniżanie progu oznacza, że pasmo odtworzone przez SBR szybciej zostaje nazwane
szumopodobnym, więc detektor częściej dochodzi do wniosku, że kopia zgubiła ton i
kompensacja jest potrzebna. Podnoszenie oznacza, że odtworzone pasmo dostaje kredyt zaufania
jako tonalne, więc dodawanych jest mniej harmonicznych.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Jedna praktyczna uwaga do pomiarów własnych: płaskość trzeba mierzyć w
prawdziwym obszarze SBR, od pasma rdzenia aż do końcowej górnej krawędzi wypisanej przez
`--verbose`. Okno sięgające do Nyquista obejmuje puste miejsce powyżej górnej krawędzi i
zwraca płaskość równą 0, co wygląda jak doskonała tonalność, a jest artefaktem okna.

Przykłady, obniżanie (odtworzone pasmo szybciej nazywane szumopodobnym, więcej
kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 70`.
- Mocno: `--sbr-mh-sfm-sbr 30`.
- Przesadnie: `--sbr-mh-sfm-sbr 1`. Niemal każde pasmo odtworzone przez SBR liczy się jako
  szumopodobne. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (odtworzone pasmo dłużej nazywane tonalnym, mniej kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 150`.
- Mocno: `--sbr-mh-sfm-sbr 400`.
- Przesadnie: `--sbr-mh-sfm-sbr 10000`. Pasmo odtworzone przez SBR jest praktycznie zawsze
  tonalne. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: działa w parze z `--sbr-mh-sfm-orig`; razem wyłapują sytuację "z
jednego tonu w oryginale po odtworzeniu zrobiło się kilka". Przesunięcie tylko jednej strony
przechyla porównanie asymetrycznie, co bywa pożądane, ale jest trudniejsze do przemyślenia.

#### `--sbr-mh-sfm-orig <n>`

**Ten sam próg płaskości co `--sbr-mh-sfm-sbr`, dla oryginalnego pasma.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = fabryczne x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

To przesuwa granicę między tonem a szumem po stronie źródła. Obniżanie oznacza, że
oryginalne pasmo szybciej zostaje nazwane szumopodobnym, więc detektor rzadziej dochodzi do
wniosku, że był tam ton wart przywrócenia. Podnoszenie oznacza, że oryginał jest chętniej
traktowany jako tonalny, więc więcej pasm staje się kandydatami na syntetyczną harmoniczną.

Warto zauważyć, że oba pokrętła sfm pchają końcową decyzję w przeciwnych sensach, choć są
tym samym rodzajem progu, bo jedno opisuje stan wejściowy, a drugie wynik. Gdy celem jest
"dodawać więcej harmonicznych", trzeba obniżyć `--sbr-mh-sfm-sbr` i podnieść to.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Przykłady, obniżanie (oryginał szybciej nazywany szumopodobnym, mniej harmonicznych):
- Łagodnie: `--sbr-mh-sfm-orig 70`.
- Mocno: `--sbr-mh-sfm-orig 30`.
- Przesadnie: `--sbr-mh-sfm-orig 1`. Źródło niemal nigdy nie jest tonalne, więc niemal nic
  nie jest uznawane za brakujące. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (oryginał chętniej nazywany tonalnym, więcej harmonicznych):
- Łagodnie: `--sbr-mh-sfm-orig 150`.
- Mocno: `--sbr-mh-sfm-orig 400`.
- Przesadnie: `--sbr-mh-sfm-orig 10000`. Niemal każde oryginalne pasmo liczy się jako
  tonalne. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: zwierciadlane odbicie `--sbr-mh-sfm-sbr`, a oba zasilają porównanie
tonalności, dla którego `--sbr-mh-diff` ustawia próg. Zmiana wszystkich trzech naraz
sprawia, że wynik jest bardzo trudny do przypisania; lepiej ruszać po jednym.

#### `--sbr-mh-maxcomp <n>`

**Ogranicza, ile kompensacji obwiedni jest stosowane wokół syntetycznej harmonicznej.**

- Dotyczy: profili z SBR
- Wartości: `0..200`, zwykła liczba całkowita, gdzie `50` = fabryczne; domyślnie `-1` =
  wyłączone
- Wpisywanie: liczba całkowita

Wstawienie sinusoidy w pasmo zmienia energię tego pasma, więc obwiednia wokół niej musi
zostać skorygowana, inaczej pasmo wyjdzie za głośne. To pokrętło ogranicza tę korektę. Nie
jest to skala x100: liczba jest samym limitem.

Wyższy limit dopuszcza większe korekty, więc pasma sąsiednie są ściągane dalej i mniej szumu
zostaje obok dodanego tonu: harmoniczna wybija się czyściej. Niższy limit ogranicza korektę,
więc dodany ton siedzi w większej części swojego pierwotnego szumu otoczenia i się w nim
zlewa, albo staje się słyszalnie za głośny, gdy potrzebna korekta była większa od limitu.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku, co wynika z tego, że
detektor i tak rzadko dodaje na tym materiale harmoniczną.

Przykłady, obniżanie limitu:
- Łagodnie: `--sbr-mh-maxcomp 35`.
- Mocno: `--sbr-mh-maxcomp 10`. Dopuszczona bardzo niewielka kompensacja.
- Przesadnie: `--sbr-mh-maxcomp 0`. Żadnej kompensacji wokół dodanej harmonicznej. Nie
  zaobserwowano zmiany strumienia.

Przykłady, podnoszenie limitu:
- Łagodnie: `--sbr-mh-maxcomp 70`.
- Mocno: `--sbr-mh-maxcomp 120`.
- Przesadnie: `--sbr-mh-maxcomp 200`, maksimum. Korekta jest praktycznie nieograniczona.
  Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: działa tylko tam, gdzie harmoniczna faktycznie zostaje dodana, więc
`--sbr-mh-tone` musi najpierw być w swoim zakresie aktywnym. Szumem, który siedzi obok
dodanego tonu, rządzą także `--sbr-noise-max` i pokrętła poziomu szumu, działające na każde
pasmo, a nie tylko na sąsiadów harmonicznej.

#### `--sbr-mh-deltatime <n>`

**Ustala maksymalną odległość transjentu, przy której ramka liczy się jako ramka
transjentowa wewnątrz detektora brakujących harmonicznych.**

- Dotyczy: profili z SBR
- Wartości: `0..64`, zwykła liczba całkowita; domyślnie `-1` = wyłączone, co zostawia 9 dla
  AAC i 16 dla AAC-LD
- Wpisywanie: liczba całkowita

Detektor brakujących harmonicznych zachowuje się inaczej w ramkach, które uznaje za
transjentowe, bo dodawanie stałej syntetycznej sinusoidy przez atak to zły pomysł. To
pokrętło ustala, jak daleko, w jednostkach czasu detektora, transjent może być od ramki,
żeby ramka wciąż liczyła się jako transjentowa. To zwykła liczba całkowita, a nie skala
x100, a wartość fabryczna różni się zależnie od profilu.

Większa odległość oznacza, że więcej ramek liczy się jako transjentowe, więc detektor
częściej wybiera swoją ostrożną ścieżkę i dodaje mniej harmonicznych w sąsiedztwie uderzeń.
Mniejsza odległość oznacza, że mniej ramek liczy się jako transjentowe, więc detektor dodaje
harmoniczne bliżej ataków, czyli tam, gdzie syntetyczna sinusoida najpewniej zabrzmi źle.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Przykłady, obniżanie odległości:
- Łagodnie: `--sbr-mh-deltatime 6`.
- Mocno: `--sbr-mh-deltatime 2`.
- Przesadnie: `--sbr-mh-deltatime 0`. Żadna ramka nie liczy się jako transjentowa z racji
  odległości, więc ostrożna ścieżka nigdy nie jest wybierana. Nie zaobserwowano zmiany
  strumienia.

Przykłady, podnoszenie odległości:
- Łagodnie: `--sbr-mh-deltatime 16`, czyli fabryczna wartość AAC-LD zastosowana do AAC.
- Mocno: `--sbr-mh-deltatime 32`.
- Przesadnie: `--sbr-mh-deltatime 64`, maksimum. Niemal każda ramka blisko jakiegokolwiek
  uderzenia liczy się jako transjentowa. Nie zaobserwowano zmiany strumienia.

Współpraca i konflikty: to pokrętło czyta decyzję o transjencie, więc leży poniżej
`--sbr-tran-thr` i `--sbr-tran-peak` w strumieniu przetwarzania: zmiana tego, co liczy się
jako transjent, zmienia to, do czego to pokrętło mierzy odległość. Na samo dzielenie ramek
nie ma wpływu.

### Filtrowanie odwrotne i poziom szumu: `--sbr-invf` `--sbr-softness` `--sbr-invf-min` `--sbr-invf-max` `--sbr-invf-bias` `--sbr-invf-table` `--sbr-invf-hyst` `--sbr-invf-sbr-thr` `--sbr-invf-orig-thr` `--sbr-invf-smooth` `--sbr-nf-weight` `--sbr-nf-weight-from` `--sbr-nf-smooth` `--sbr-noise-max` `--sbr-noise-floor-offset`

Tutaj faktycznie rozstrzyga się charakter górnego pasma i to jest ta część SBR, którą warto
zrozumieć porządnie.

Skopiowane górne pasmo dziedziczy drobną strukturę pasma, z którego zostało skopiowane. Ta
struktura jest tonalna: szczyty i doliny wypadają na częstotliwościach nieodpowiednich dla
miejsca, w którym wylądowały. Efekt brzmi metalicznie, dzwoni, czasem jest chórowany, a kto
raz to usłyszał, nie potrafi już tego odsłyszeć. Lekarstwem jest filtrowanie odwrotne:
dekoder nakłada na odtworzone pasmo filtr wybielający, który tę odziedziczoną strukturę
spłaszcza. Są cztery poziomy mocy, 0 wyłączone, 1 niski, 2 średni, 3 wysoki, wybierane na
pasmo i na ramkę. Poziom szumu to druga połowa tego samego lekarstwa: syntetyczny szum
domieszany do każdego pasma, który zastępuje strukturę usuniętą przez wybielanie. Za mało
jednego i drugiego daje metaliczną górę; za dużo daje górę syczącą i rozmytą, w której
talerze redukują się do kształtowanego szumu.

Poziom wybiera normalnie **estymator tonalności**, który porównuje pasmo oryginału z pasmem
odtworzonym przez SBR, pasmo po paśmie i ramka po ramce. To tę dynamiczną decyzję warto
zachować: ma rację znacznie częściej niż jakakolwiek stała wartość wybrana z ręki.

Ta sekcja zawiera więc dwa rodzaje przełączników, a pierwszeństwo między nimi ma znaczenie:

1. `--sbr-invf` **wymusza** jeden ustalony poziom na cały plik, w każdym paśmie i w każdej
   chwili. Wygrywa ze wszystkim innym w tym miejscu. Warto go używać do diagnozy albo
   wtedy, gdy materiał jest na pewno jednorodny.
2. Wszystko pozostałe zostawia decyzję dynamiczną i zmienia reguły estymatora: granice,
   przesunięcie, tabelę decyzyjną, histerezę, wygładzanie, położenie granic, wagę szumu.
   Decyzja nadal jest podejmowana na pasmo i na ramkę, tylko według innych reguł.

W grupie 2 `--sbr-softness` jest makrem, które ustawia kilka z tych reguł naraz, a **każde
jawne pokrętło wygrywa z makrem**. Czyli `--sbr-softness 6 --sbr-invf-max 2` daje miękkość z
makra z własnym sufitem.

Sposób korzystania z tej sekcji kształtują dwa fakty pomiarowe.

Pierwszy: makro miękkości celowo *nie* rusza granic tonalności. Zmierzyliśmy, że ich
przesuwanie powoduje migotanie decyzji w obrębie pasma, od 0,2 do 0,29 zmiany na pasmo na
ramkę wobec 0,004 do 0,04 w ustawieniu fabrycznym, co jest słyszalne jako niestabilność.
`--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` i tak udostępniają ten mechanizm, bo czasem
jest to właśnie pożądane, ale to dlatego makro ich unika.

Drugi: waga poziomu szumu jest obcinana przez sufit szumu. Przy sufitach z tabeli strojenia
(0,125 przy 24 do 48 kbps) około 95 procent pasm i tak już siedzi na sufice, więc
`--sbr-nf-weight` samo w sobie często nie zmienia zupełnie nic. Żeby cokolwiek zrobiło,
potrzebuje podniesionego sufitu *oraz* pasm z wysokim invf: zmierzone, strumień ruszył
dopiero przy `--sbr-noise-max 6` w połączeniu z `--sbr-invf 3` albo
`--sbr-nf-weight-from 1`. Samo albo tylko z jednym z nich nie zmieniło nic. Poziomy od 5 do
7 w `--sbr-softness` podnoszą sufit samoczynnie.

Przy wartościach neutralnych (`--sbr-invf-hyst 1.0`, `--sbr-invf-bias 0`,
`--sbr-invf-smooth 2`, `--sbr-nf-weight 0.25`, `--sbr-nf-weight-from 3`,
`--sbr-invf-table 0`, `--sbr-invf-min 0`, `--sbr-invf-max 3`) te pokrętła dają strumień
identyczny bajt w bajt z ustawieniem fabrycznym, podobnie jak `--sbr-softness 2` i
`--speech --sbr-softness 4`.

#### `--sbr-invf <0..3>`

**Wymusza jeden ustalony poziom filtrowania odwrotnego na cały plik, całkowicie pomijając
estymator tonalności.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `0` wyłączone, `1` niski, `2` średni, `3` wysoki; domyślnie `-1` = automat,
  czyli estymator tonalności decyduje na pasmo i na ramkę
- Wpisywanie: liczba całkowita

Wyżej znaczy mocniejsze wybielanie odtworzonego pasma: mniej metalicznego dzwonienia, mniej
szczegółu, góra bardziej płaska i bardziej szumopodobna. Niżej znaczy, że odziedziczona
struktura tonalna zostaje na miejscu: więcej pozornego szczegółu, więcej metalicznego
charakteru.

Wada wymuszania jest dokładnie taka, jak brzmi: jeden poziom obowiązuje wszędzie, także w
tych pasmach i momentach, w których estymator wybrałby inaczej. Plik z cichym, tonalnym
wstępem i gęstym refrenem potrzebuje w tych miejscach różnych poziomów, a wymuszona wartość
daje im ten sam.

Mimo tego jest to najbardziej przydatne pokrętło w tej sekcji, do dwóch zadań. Pierwsze to
diagnoza: zakodować na `0` i na `3`, posłuchać skrajności i od razu wiadomo, którego końca
tej osi potrzebuje dany materiał. Drugie to materiał jednorodny, gdzie estymator i tak nie
ma do czego się dostrajać.

Wartości:
- `0` filtrowanie odwrotne wyłączone. Żadnego wybielania. Odtworzone pasmo zachowuje
  odziedziczoną strukturę: maksimum pozornego szczegółu, maksimum metalicznego dzwonienia.
  Na materiale tonalnym to wyraźny artefakt; na materiale szumopodobnym może być najczystszą
  opcją.
- `1` niski. Lekkie wybielanie.
- `2` średni. Umiarkowane wybielanie, z grubsza tam, gdzie estymator siedzi średnio przy
  tabeli muzycznej.
- `3` wysoki. Mocne wybielanie. Góra staje się płaska i szumopodobna; metaliczne dzwonienie
  znika, a razem z nim spora część odczuwanego szczegółu.
- `-1` automat. Decyduje estymator. Ustawienie zalecane poza diagnostyką.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf 3 -o out.m4a in.wav`

Współpraca i konflikty: ten przełącznik **wygrywa z każdym innym w tej sekcji**, w tym z
`--sbr-softness` i z pokrętłami min/max/bias, bo nie zostaje już żadna dynamiczna decyzja
do ograniczania. Gdy celem jest kształtowanie decyzji, a nie jej zastąpienie, trzeba
zostawić go wyłączonym. Zestawienie `--sbr-invf 3` z `--sbr-noise-max 6` to jedna z dwóch
kombinacji, które sprawiają, że `--sbr-nf-weight` w ogóle coś robi.

#### `--sbr-softness <0..7>`

**Jedno makro od góry twardej i metalicznej do bardzo miękkiej, ustawiające kilka reguł
estymatora naraz i zachowujące dynamiczną decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0..7`; domyślnie `-1` = wyłączone (fabrycznie odpowiada poziomowi `2` dla
  muzyki)
- Wpisywanie: liczba całkowita

To pokrętło warto wypróbować pierwsze. Przesuwa punkt pracy po osi metaliczne kontra syczące
bez zamrażania decyzji, ustawiając tabelę decyzyjną, przesunięcie, wagę szumu, granice
min/max i sufit szumu w kombinacjach, które zmierzyliśmy jako monotoniczne i stabilne.
Celowo nie rusza granic tonalności, bo zmierzyliśmy, że to powoduje migotanie decyzji w
obrębie pasma.

Wartości:
- `0` o jeden stopień mniej wybielania niż strojenie muzyczne: twardziej, bardziej
  metalicznie niż fabrycznie.
- `1` nigdy wybielanie HIGH. Na muzyce estymator i tak rzadko wybiera HIGH, więc na
  materiale muzycznym ta granica często wcale nie działa: zmierzone na HE-AAC
  32 kbps, poziomy `1` i `2` dały strumień identyczny z fabrycznym na sygnałach tonalnym,
  głosowym i mieszanym.
- `2` fabryczne estymatory muzyczne. Identyczny bajt w bajt z niekorzystaniem z
  przełącznika w ogóle.
- `3` co najmniej wybielanie LOW: estymator nie może już wybrać wyłączenia. Zmierzone jako
  równe `--sbr-invf 1` na sygnale tonalnym, bo tam estymator dążył do wyłączenia wszędzie, a
  dolna granica podniosła wszystko do LOW.
- `4` fabryczne estymatory mowy: tabela decyzyjna mowy i waga szumu, bez pozostałej części
  `--speech`, więc częstotliwość podziału zostaje nietknięta.
- `5` jak `4` plus podniesiony sufit szumu (0,5) i waga rozszerzona na pasma MID.
- `6` jak `5` plus jeszcze jeden stopień wybielania.
- `7` bardzo miękko: sufit 1,0, maksimum wybielania i szumu wypełniającego. Góra staje się
  jednoznacznie szumopodobna.
- `-1` wyłączone.

Zmierzone na HE-AAC 32 kbps, poziomy invf odczytane ze strumienia, płaskość zdekodowanego
pasma SBR od 7 do 14,5 kHz, gdzie 0 jest tonalne, a 1 szumopodobne:

| `--sbr-softness` | średni poziom invf (mowa / tonalny / muzyka) | płaskość pasma SBR (mowa / tonalny) |
|---|---|---|
| 0 | 0.15 / 0.00 / 0.00 | 0.366 / 0.396 |
| 2 (fabryczna muzyka) | 1.12 / 0.98 / 0.99 | 0.373 / 0.430 |
| 4 (fabryczna mowa) | 1.93 / 1.96 / 1.98 | 0.379 / 0.440 |
| 5 | 1.93 / 1.96 / 1.98 | 0.403 / 0.518 |
| 7 | 2.93 / 2.96 / 2.98 | 0.417 / 0.521 |

Skala jest monotoniczna na każdej próbce, a zmiany trybu w obrębie pasma zostają na poziomie
fabrycznym (0,004 do 0,04 na pasmo na ramkę). Zmierzone również: poziom `0` równa się
`--sbr-invf 0` na materiale tonalnym i mieszanym, a poziomy od `4` do `7` różnią się między
sobą.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-softness 6 -o out.m4a in.wav`

Współpraca i konflikty: **każde jawne pokrętło poniżej wygrywa z tym makrem**, i właśnie tak
się je łączy: `--sbr-softness 6 --sbr-invf-max 2` zachowuje obsługę szumu z makra przy
własnym sufice. `--sbr-invf` wygrywa z tym makrem całkowicie. Sąsiednie poziomy mogą dać ten
sam strumień, gdy materiał nigdy nie dociera do granicy, którą się różnią; to oczekiwane, a
nie awaria.

#### `--sbr-invf-min <0..3>`

**Ustawia dolną granicę dla estymatora: najniższy poziom filtrowania odwrotnego, jaki wolno
mu wybrać.**

- Dotyczy: profili z SBR
- Wartości: `0..3`; domyślnie `-1` = wyłączone (bez dolnej granicy, odpowiednik `0`)
- Wpisywanie: liczba całkowita

Estymator nadal decyduje na pasmo i na ramkę; to tylko usuwa dolną część jego zakresu. To
narzędzie na „góra jest metaliczna w cichych fragmentach”, gdy estymator poprawnie widzi
pasmo tonalne i wybiera mało wybielania albo wcale, a odsłuch temu przeczy.

Podniesienie dolnej granicy gwarantuje minimalną ilość wybielania wszędzie, co kosztuje
szczegół w tych pasmach, które naprawdę go nie potrzebowały.

Wartości:
- `0` dolna granica na wyłączeniu. Bez ograniczenia; identyczne z niekorzystaniem z
  przełącznika.
- `1` estymator nie może nigdy wybrać wyłączenia. Najdelikatniejszy użyteczny poziom i ten,
  który warto wypróbować, gdy metaliczne dzwonienie pojawia się tylko sporadycznie.
- `2` co najmniej wybielanie MID wszędzie. Mocna interwencja: pasma, które nie chciały
  żadnego, dostają teraz umiarkowane wybielanie.
- `3` co najmniej HIGH wszędzie, co oznacza, że poziom jest przypięty do HIGH, bo 3 to
  maksimum. W efekcie jest to odpowiednik `--sbr-invf 3`, więc przy takim celu lepiej
  zapisać to wprost.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 48000 --sbr-invf-min 1 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, razem z `--sbr-invf-max` jako
drugim końcem klamry. Ustawienie `--sbr-invf-min 3` albo `--sbr-invf-min` równego
`--sbr-invf-max` zwija dynamiczny zakres do jednej wartości, czyli jest to `--sbr-invf`
drogą okrężną. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.
`--sbr-softness 3` to zapakowana wersja `--sbr-invf-min 1`.

#### `--sbr-invf-max <0..3>`

**Ustawia sufit nad estymatorem: najwyższy poziom filtrowania odwrotnego, jaki wolno mu
wybrać.**

- Dotyczy: profili z SBR
- Wartości: `0..3`; domyślnie `-1` = wyłączone (bez sufitu, odpowiednik `3`)
- Wpisywanie: liczba całkowita

Odbicie `--sbr-invf-min` i częściej użyteczne z tych dwóch. To narzędzie na „góra robi się
sycząca i traci szczegół w głośnych fragmentach”, gdy estymator wybiera mocne wybielanie i
zabiera razem z nim blask.

Obniżenie sufitu zachowuje szczegół za cenę przepuszczenia pewnej dawki metalicznego
charakteru w pasmach, które naprawdę potrzebowały wybielania.

Wartości:
- `3` sufit na HIGH. Bez ograniczenia; identyczne z niekorzystaniem z przełącznika.
- `2` nigdy HIGH. Zwykły wybór: usuwa tylko najbardziej agresywne wybielanie i zachowuje
  resztę oceny estymatora.
- `1` nigdy powyżej LOW. Mocne ustawienie chroniące szczegół; na materiale tonalnym słychać
  metaliczny charakter.
- `0` sufit jest na wyłączeniu, więc nigdzie nie wolno wybielać. W efekcie odpowiednik
  `--sbr-invf 0`, który lepiej zapisać właśnie tak.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-max 2 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, w parze z `--sbr-invf-min`.
`--sbr-softness 1` to zapakowana wersja `--sbr-invf-max 2`, ze zmierzonym zastrzeżeniem z
tamtego miejsca: na muzyce estymator rzadko dociera do HIGH, więc ten sufit może nie robić
nic na materiale muzycznym i jednocześnie być dokładnie tym, czego trzeba na mowie. Wygrywa
z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-bias <-3..3>`

**Przesuwa każdą dynamiczną decyzję filtrowania odwrotnego w górę albo w dół o ustaloną
liczbę stopni.**

- Dotyczy: profili z SBR
- Wartości: `-3..3` stopnie; domyślnie `0` = wyłączone
- Wpisywanie: liczba całkowita

Tam gdzie min i max obcinają zakres, to pokrętło (przesunięcie progu, bias) go przesuwa.
Wzór decyzji estymatora na pasmo i na ramkę jest zachowany dokładnie, a cały wzór się
przesuwa. To czyni je najczystszym sposobem powiedzenia „estymator ma dobry pomysł, tylko
konsekwentnie jest za miękki” albo „konsekwentnie za twardy”. Wartości dodatnie oznaczają
więcej wybielania.

Ponieważ wynik jest potem obcinany do 0..3, duże przesunięcia spłaszczają wzór na tym końcu,
w który uderzą: przy `+3` niemal wszystko ląduje na HIGH i zróżnicowanie między pasmami,
które miało zostać, znika.

Przykłady, więcej wybielania:
- Łagodnie: `--sbr-invf-bias 1`. Każde pasmo o jeden stopień mocniej wybielone. Wzór
  pozostaje nietknięty, charakter jest łagodniejszy.
- Mocno: `--sbr-invf-bias 2`. Dwa stopnie. Pasma, które chciały wyłączenia, dostają teraz
  MID; metaliczny charakter w dużej mierze zniknął, a razem z nim część szczegółu.
- Przesadnie: `--sbr-invf-bias 3`. Wszystko, czego estymator i tak nie chciał na HIGH,
  zostaje tam zepchnięte przez obcięcie. To `--sbr-invf 3` okrężną drogą.

Przykłady, mniej wybielania:
- Łagodnie: `--sbr-invf-bias -1`. O jeden stopień mniej wszędzie. Więcej szczegółu, cień
  więcej metaliczności.
- Mocno: `--sbr-invf-bias -2`. Dwa stopnie mniej. Jakiekolwiek wybielanie zachowują tylko
  te pasma, które estymator chciał na HIGH.
- Przesadnie: `--sbr-invf-bias -3`. Niemal wszystko obcina się do wyłączenia, czyli jest to
  `--sbr-invf 0` drogą okrężną, z maksymalnym metalicznym dzwonieniem na materiale tonalnym.

Współpraca i konflikty: nakładane **przed** `--sbr-invf-min` i `--sbr-invf-max`, więc można
najpierw przesunąć, a potem obciąć: `--sbr-invf-bias 2 --sbr-invf-max 2` przesuwa wszystko w
górę, ale odmawia HIGH. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-table <0|1>`

**Wybiera, której tabeli decyzyjnej używa estymator tonalności: tabeli muzycznej czy tabeli
mowy.**

- Dotyczy: profili z SBR
- Wartości: `0` muzyka, `1` mowa; domyślnie `-1` = zgodnie z `--speech`
- Wpisywanie: liczba całkowita

Progi estymatora pochodzą z tabeli, a FDK dostarcza dwie: jedną strojoną pod muzykę, drugą
pod mowę. Tabela mowy wybiela znacznie chętniej, bo mowa powyżej częstotliwości podziału to
w większości szczelinowe i oddech, gdzie odziedziczona struktura tonalna jest czystym
artefaktem i nie ma tam szczegółu wartego ochrony.

Ten przełącznik wybiera tabelę i **tylko** tabelę. `--speech` zmienia również częstotliwość
podziału i inne elementy strojenia; gdy celem jest zachowanie estymatora mowy na materiale
muzycznym bez przestawiania układu częstotliwości, to jest przełącznik do tego. Taka
kombinacja jest naprawdę przydatna: audycja słowna, audiobooki, materiał podcastowy przy
niskim bitrate'ie, gdzie potrzebne jest zachowanie wybielania przy własnej częstotliwości
podziału.

Wartości:
- `0` tabela muzyczna. Zachowawcze wybielanie, chroni szczegół tonalny.
- `1` tabela mowy. Wybiela znacznie chętniej. Zmierzony średni poziom invf przy tabeli mowy
  jest blisko 2,0 wobec około 1,0 przy tabeli muzycznej na HE-AAC 32 kbps.
- `-1` wyłączone: zgodnie z `--speech`, czyli tabela muzyczna, chyba że podano `--speech`.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-invf-table 1 -o out.m4a speech.wav`

Współpraca i konflikty: `--sbr-softness 4` to zapakowana wersja tego plus waga szumu dla
mowy. `--speech` ustawia to niejawnie i przesuwa również częstotliwość podziału, i to jest
różnica warta zapamiętania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-hyst <x>`

**Ustawia histerezę granic decyzyjnych: jak bardzo musi zmienić się tonalność, zanim
estymator przełączy pasmo na inny poziom.**

- Dotyczy: profili z SBR
- Wartości: `0..2`, liczba dziesiętna, gdzie `1.0` = fabryczne; domyślnie `-1` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Pasmo, którego tonalność siedzi dokładnie na granicy decyzyjnej, przeskakiwałoby w
przeciwnym razie na inny poziom w każdej ramce, a przeskakiwanie poziomów wybielania z ramki
na ramkę jest słyszalne jako rodzaj trzepotania albo niestabilności na górze. Histereza
rozszerza granice w strefę martwą, więc pasmo trzyma bieżący poziom, dopóki tonalność nie
przesunie się znacząco za linię.

Obniżanie oznacza szybszą reakcję na prawdziwe zmiany i częstsze przełączanie. Podnoszenie
oznacza większą stabilność i wolniejsze podążanie za rzeczywistymi zmianami, więc fragment,
który w połowie staje się tonalny, trzyma dłużej poprzednie traktowanie. Powyżej 2
rozszerzone granice zachodziłyby na siebie, co nie ma sensu, więc jest odrzucane.

Dla odniesienia: fabryczne tempo przełączania w obrębie pasma to od 0,004 do 0,04 zmiany na
pasmo na ramkę, a konfiguracja zmierzona na 0,2 do 0,29 była wyraźnie niestabilna.

Przykłady, mniej histerezy (szybciej, więcej przełączania):
- Łagodnie: `--sbr-invf-hyst 0,8`. Nieco szybsze podążanie za zmianami.
- Mocno: `--sbr-invf-hyst 0,3`. Wyraźnie więcej przełączania; warto stosować, gdy estymator
  wydaje się ospały na materiale, który szybko zmienia charakter.
- Przesadnie: `--sbr-invf-hyst 0`. Zupełny brak strefy martwej. Pasma siedzące na granicy
  przeskakują w każdej ramce, czyli pojawia się dokładnie ta niestabilność, której histereza
  ma zapobiegać.

Przykłady, więcej histerezy (wolniej, stabilniej):
- Łagodnie: `--sbr-invf-hyst 1,2`. Odrobinę bardziej lepkie niż fabrycznie.
- Mocno: `--sbr-invf-hyst 1,5`. Poziomy trzymają się mimo umiarkowanych zmian tonalności.
  Dobra para dla `--sbr-invf-max 2`, gdy celem jest spokojna, przewidywalna góra.
- Przesadnie: `--sbr-invf-hyst 2`, czyli maksimum. Strefy martwe są tak szerokie, jak tylko
  mogą być; pasma zasadniczo trzymają ten poziom, na którym wylądowały na początku, co na
  materiale zmieniającym charakter jest po prostu złym poziomem przez większość czasu.

Współpraca i konflikty: `--sbr-invf-smooth` atakuje tę samą niestabilność z drugiej strony,
uśredniając po ramkach zamiast poszerzać granice; używanie obu w skrajnych ustawieniach jest
nadmiarowe. `--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` przesuwają te granice, które to
pokrętło poszerza. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-sbr-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma odtworzonego przez SBR, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Estymator mierzy tonalność po obu stronach porównania i każdą z nich zestawia z granicami.
To pokrętło przesuwa granice po stronie odtworzonej przez SBR. Wartości dodatnie oznaczają,
że odtworzone pasmo szybciej liczy się jako tonalne, a tonalne pasmo odtworzone jest właśnie
tym, po co jest wybielanie, więc dodatnie oznaczają więcej invf.

To instrument ostrzejszy niż przesunięcie progu czy min/max, i bardziej ryzykowny.
Przesuwanie granic zmienia to, *które* pasma je przekraczają i w których ramkach, więc może
wprowadzić migotanie decyzji: dla tego mechanizmu zmierzyliśmy od 0,2 do 0,29 zmiany na
pasmo na ramkę wobec 0,004 do 0,04 w ustawieniu fabrycznym, i to dlatego `--sbr-softness` z
niego nie korzysta. Przy sięganiu po to pokrętło warto jednocześnie podnieść
`--sbr-invf-hyst`, żeby odkupić stabilność.

Przykłady, wartości dodatnie (odtworzone pasmo szybciej tonalne, więcej invf):
- Łagodnie: `--sbr-invf-sbr-thr 2`. Niewielkie przesunięcie; kilka pasm więcej przechodzi
  do wybielania.
- Mocno: `--sbr-invf-sbr-thr 8`. Dużo więcej pasm liczy się jako tonalne. Słychać
  zauważalnie większe wybielanie i pewne migotanie.
- Przesadnie: `--sbr-invf-sbr-thr 20`, czyli maksimum. Zasadniczo każde pasmo odtworzone
  przez SBR liczy się jako tonalne, więc pomiar przestaje rozróżniać, a ocena estymatora
  zostaje zastąpiona stałą. `--sbr-invf 3` robi to samo bez migotania.

Przykłady, wartości ujemne (odtworzone pasmo tonalne później, mniej invf):
- Łagodnie: `--sbr-invf-sbr-thr -2`. Odrobinę mniej wybielanych pasm.
- Mocno: `--sbr-invf-sbr-thr -8`. Wybielane są tylko mocno tonalne pasma odtworzone; więcej
  szczegółu, więcej metalicznego charakteru.
- Przesadnie: `--sbr-invf-sbr-thr -20`, czyli minimum. Nic nie liczy się jako tonalne, więc
  nic nie jest wybielane. Ustawienie skrajne „zachowaj cały szczegół”, i tak właśnie brzmi.

Współpraca i konflikty: odbicie `--sbr-invf-orig-thr`, a obydwa składają się na porównanie
oryginału z pasmem odtworzonym przez SBR. `--sbr-invf-hyst` jest przeciwwagą po stronie
stabilności. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-orig-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma oryginalnego, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Ten sam mechanizm po stronie źródła, przy czym znak czyta się odwrotnie, i to jest rzecz,
którą trzeba dobrze zrozumieć. Estymator wybiela, gdy pasmo odtworzone wygląda na bardziej
tonalne, niż na to zasługuje oryginał. Zatem sprawienie, żeby *oryginał* liczył się jako
tonalny **później**, oznacza, że porównanie częściej znajduje większą niezgodność, a więc
wartości dodatnie także tutaj oznaczają więcej wybielania. Oba pokrętła progowe prowadzą do
tego samego skutku, mimo że opisują przeciwne strony.

Zastrzeżenie o migotaniu obowiązuje tu identycznie: to jest przesunięcie granicy, a
przesunięcia granic destabilizują decyzję w obrębie pasma mocniej niż przesunięcie progu czy
obcinanie.

Przykłady, wartości dodatnie (oryginał liczy się jako tonalny później, więcej invf):
- Łagodnie: `--sbr-invf-orig-thr 2`.
- Mocno: `--sbr-invf-orig-thr 8`. Oryginałowi rzadko przypisuje się tonalność, więc pasmo
  odtworzone częściej wygląda na błędne i zostaje wybielone.
- Przesadnie: `--sbr-invf-orig-thr 20`, czyli maksimum. Oryginał nigdy nie liczy się jako
  tonalny, więc każde odtworzone pasmo jest traktowane jak uszkodzenie. Maksymalne
  wybielanie drogą, która maksymalizuje też migotanie.

Przykłady, wartości ujemne (oryginał liczy się jako tonalny wcześniej, mniej invf):
- Łagodnie: `--sbr-invf-orig-thr -2`.
- Mocno: `--sbr-invf-orig-thr -8`. Oryginałowi hojnie przypisuje się tonalność, więc pasmo
  odtworzone częściej jest uznawane za akceptowalne i zostawiane w spokoju.
- Przesadnie: `--sbr-invf-orig-thr -20`, czyli minimum. Wszystko w oryginale liczy się jako
  tonalne, porównanie nie znajduje niezgodności, a wybielanie praktycznie ustaje.

Współpraca i konflikty: para dla `--sbr-invf-sbr-thr`; przesuwanie obu w tę samą stronę
częściowo się znosi, bo jedno podnosi niezgodność, a drugie ją obniża. Poziom szumu
odczytuje tę samą różnicę między oryginałem a pasmem odtworzonym przez `--sbr-nf-weight`,
więc przesuwanie tych granic zmienia także szum wypełniający, nie tylko wybielanie. Wygrywa
z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-smooth <0..2>`

**Ustawia, po ilu ramkach estymator tonalności uśrednia, zanim podejmie decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0`, `1`, `2`; domyślnie `-1` = wyłączone (fabrycznie uśrednianie po 2)
- Wpisywanie: liczba całkowita

Uśrednianie po ramkach to druga obrona przed niestabilną decyzją. Z uśrednianiem jedna
odbiegająca ramka nie przestawi poziomu wybielania pasma; bez niego każda ramka jest
oceniana sama. Wymiana jest typowa: stabilność za szybkość reakcji.

Wartości:
- `0` tylko bieżąca ramka. Najszybsza reakcja na prawdziwą zmianę charakteru, najwięcej
  przełączania z ramki na ramkę. Warto stosować na materiale, który zmienia się szybko,
  licząc się z pewną chwiejnością.
- `1` uśrednianie krótsze niż fabryczne. Opcja pośrednia.
- `2` ustawienie fabryczne. Równowaga wybrana przez FDK, i to dobra na większości materiału.
- `-1` wyłączone, czyli to samo co `2`.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-smooth 0 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: to pokrętło i `--sbr-invf-hyst` kupują stabilność, każde innym
sposobem, więc zestawienie niskiego wygładzania z wysoką histerezą to rozsądna droga do
szybkiej reakcji bez trzepotania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`,
któremu nie zostaje już żadna decyzja do wygładzania.

#### `--sbr-nf-weight <x>`

**Ustawia, jak mocno różnica tonalności między oryginałem a pasmem odtworzonym przez SBR
podnosi poziom szumu.**

- Dotyczy: profili z SBR, i tylko tam, gdzie sufit szumu zostawia zapas (patrz niżej)
- Wartości: `0.0..1.0`, liczba dziesiętna; domyślnie `-1` = zgodnie z `--speech`
  (muzyka 0,25, mowa 1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Wybielanie usuwa strukturę; szum wypełniający ją zastępuje. Ta waga rozstrzyga, jaka część
zmierzonej niezgodności między pasmem oryginału a pasmem odtworzonym przez SBR zamienia się
w poziom szumu. Wyżej oznacza więcej szumu wypełniającego wszędzie tam, gdzie odtworzone
pasmo jest zbyt tonalne, i to dlatego strojenie mowy używa 1,0: w mowie górne pasmo powinno
być szumem, więc wypełnianie go jest zawsze słuszne. Niżej oznacza, że różnicą zajmuje się
głównie samo wybielanie.

**Warto to przeczytać przed użyciem.** Ważony szum jest obcinany przez sufit szumu, a przy
sufice z tabeli strojenia (0,125 przy 24 do 48 kbps) około 95 procent pasm już na nim
siedzi, więc podnoszenie wagi nic nie zmienia, bo nie ma zapasu do wykorzystania.
Zmierzone, to pokrętło ruszyło strumień tylko przy `--sbr-noise-max 6` **plus**
`--sbr-invf 3` albo `--sbr-nf-weight-from 1`. Samo, albo tylko z jednym z tych dwóch, nie
zmieniło zupełnie nic. Potrzebuje podniesionego sufitu i pasm na wysokim poziomie invf, bo
domyślnie wagę dostają tylko pasma HIGH.

Działający przepis to zatem `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight <x>`,
albo pozostawienie podniesienia sufitu poziomom `--sbr-softness 5`, `6` czy `7`.

Przykłady, więcej szumu wypełniającego (przy podniesionym sufice):
- Łagodnie: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.4`. Trochę więcej
  powietrza tam, gdzie odtworzone pasmo jest zbyt tonalne.
- Mocno: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7`. Wyraźnie więcej
  szumu wypełniającego; góra czyta się jako bardziej powietrzna i mniej metaliczna, przy
  mniejszym pozornym szczególe.
- Przesadnie: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 1`, czyli maksimum,
  a więc wartość dla mowy nałożona na każde pasmo od LOW w górę przy szeroko otwartym
  sufice. Na muzyce to syk w miejscu, gdzie kiedyś były talerze.

Przykłady, mniej szumu wypełniającego:
- Łagodnie: `--sbr-nf-weight 0,15`. Poniżej domyślnej wartości dla muzyki; nieco bardziej
  sucha góra.
- Mocno: `--sbr-nf-weight 0,05`. Różnica tonalności nie wnosi prawie żadnego szumu;
  wybielanie musi odwalić robotę samo.
- Przesadnie: `--sbr-nf-weight 0`, czyli minimum. Różnica jest całkowicie ignorowana. Przy
  domyślnym sufice może to spokojnie dać strumień identyczny bitowo z fabrycznym, bo
  większość pasm i tak jest obcięta, co samo w sobie jest lekcją o tym pokrętle.

Współpraca i konflikty: potrzebuje `--sbr-noise-max 3` albo `6`, żeby mieć zapas, i
`--sbr-nf-weight-from`, żeby sięgnąć pasm poniżej HIGH. `--sbr-noise-floor-offset` podnosi
poziom szumu bezwarunkowo i jest bardziej topornym wyjściem awaryjnym, gdy to pokrętło
odmawia ruchu. Wygrywa z `--sbr-softness`; `--sbr-invf 3` go nie blokuje, tylko jest jedną z
dwóch rzeczy, które go odblokowują.

#### `--sbr-nf-weight-from <1..3>`

**Ustawia najniższy poziom filtrowania odwrotnego, którego pasma dostają wagę poziomu
szumu.**

- Dotyczy: profili z SBR
- Wartości: `1` LOW i wyżej, `2` MID i wyżej, `3` tylko HIGH; domyślnie `-1` = wyłączone
  (fabrycznie `3`)
- Wpisywanie: liczba całkowita

Waga różnicy tonalności nie jest nakładana wszędzie. Domyślnie dostają ją tylko pasma, które
dostały wybielanie HIGH, z rozumowania, że to w nich struktura została naprawdę zniszczona i
wymaga zastąpienia. Ten przełącznik poszerza ten zbiór w dół.

To czyni go przełącznikiem odblokowującym `--sbr-nf-weight`: na muzyce estymator rzadko
wybiera HIGH, więc przy ustawieniu fabrycznym waga nie ma prawie na czym działać, a pokrętło
wygląda na martwe. Po ustawieniu na `1` w grze jest każde wybielone pasmo.

Wartości:
- `3` tylko HIGH. Ustawienie fabryczne. Najbardziej zachowawcze i powód, dla którego
  `--sbr-nf-weight` tak często sprawia na muzyce wrażenie, że nic nie robi.
- `2` MID i HIGH. Umiarkowane poszerzenie, i to właśnie robi `--sbr-softness 5` i wyżej.
- `1` LOW, MID i HIGH. Wagę dostaje każde pasmo z jakimkolwiek wybielaniem. Najszerszy
  zasięg, i ten, przy którym zmierzyliśmy, że `--sbr-nf-weight` zmienia strumień, w parze z
  `--sbr-noise-max 6`.
- `-1` wyłączone, to samo co `3`.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Współpraca i konflikty: bez sensu, dopóki `--sbr-nf-weight` czegoś nie robi, a
`--sbr-nf-weight` jest bez sensu bez zapasu od `--sbr-noise-max`. Wszystkie trzy ruszają
razem. Wygrywa z `--sbr-softness`.

#### `--sbr-nf-smooth <0>`

**Wyłącza wygładzanie poziomu szumu w czasie.**

- Dotyczy: profili z SBR
- Wartości: `0` = bez wygładzania; domyślnie `-1` = wyłączone, co zostawia fabryczne
  wygładzanie po 4 ramkach
- Wpisywanie: liczba całkowita

Poziom szumu jest normalnie uśredniany po czterech ramkach, więc ilość szumu wypełniającego
w paśmie zmienia się stopniowo. To słuszne na materiale ustabilizowanym, gdzie skaczący
poziom szumu byłby słyszalny jako pompowanie albo oddychanie na górze. Jest niesłuszne tam,
gdzie materiał naprawdę zmienia się szybko: wygładzony poziom przychodzi z opóźnieniem, więc
nagle cichy fragment jeszcze przez kilka ramek niesie szum z fragmentu głośnego, a nagle
jasny fragment jest na starcie niedopełniony.

Ten przełącznik działa jednokierunkowo. Nie ma wartości, która zwiększa wygładzanie; `0` je
usuwa, a cokolwiek innego oznacza zostawienie go w spokoju.

Wartości:
- `0` bez wygładzania w czasie. Poziom szumu podąża dokładnie za oszacowaniem z ramki na
  ramkę. Szybciej, i na materiale ustabilizowanym słyszalnie mniej stabilnie.
- `-1` wyłączone: fabryczne wygładzanie po 4 ramkach. Właściwa wartość domyślna.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-nf-smooth 0 -o out.m4a transients.wav`

Współpraca i konflikty: szumowy odpowiednik `--sbr-invf-smooth 0`, i obowiązuje to samo
rozumowanie: warto stosować na materiale z szybkimi zmianami charakteru, licząc się z
mniejszą stabilnością na materiale utrzymującym się. Jego efekt jest ograniczony przez
`--sbr-noise-max`: gdy sufit obcina większość pasm, zostaje niewiele zróżnicowania, które
wygładzanie mogłoby wygładzić. Wygrywa z `--sbr-softness`.

#### `--sbr-noise-max <6|3|-3>`

**Ustawia sufit tego, jak głośny może być szum wstrzykiwany przez SBR: limit między
powietrzem a sykiem na górze.**

- Dotyczy: profili z SBR
- Wartości: `6` = 1,0, `3` = 0,5, `-3` = 0,125; domyślnie `-1` = wartość z tabeli strojenia
  dla bitrate'u
- Wpisywanie: liczba całkowita

To najbardziej doniosłe pokrętło szumu, nie dlatego, że ustawia poziom, ale dlatego, że
ustawia limit, w który uderza wszystko pozostałe. Te trzy wartości to trzy, które rozumie
estymator szumu w FDK, i odpowiadają sufitom amplitudy 1,0, 0,5 oraz 0,125. Tabela strojenia
wybiera 0,125 przy 24 do 48 kbps, a przy tym sufice około 95 procent pasm jest już
obciętych, i to dlatego szum wypełniający sprawia wrażenie ustalonego aż do jego
podniesienia.

Podniesienie sufitu samo w sobie nie dodaje szumu; pozwala estymatorowi i wadze dodać go
więcej tam, gdzie i tak już chciały. Obniżenie go przycina górę do czegoś bardziej suchego i
tonalnego, co na materiale rzadkim brzmi czyściej, a na talerzach i oddechu chudo.

Która z tych trzech wartości nic nie robi, zależy od obowiązującego wiersza tabeli
strojenia, więc lepiej mierzyć niż zakładać. Zmierzone: na sygnałach tonalnym i mieszanym
przy HE-AAC 32 kbps `-3` równa się ustawieniu fabrycznemu, a `6` równa się `3`; na sygnale
głosowym przy tym samym ustawieniu `6`, `3` i ustawienie fabryczne są identyczne.

Wartości:
- `-3` sufit 0,125. Najbardziej sucho i taki wybiera tabela przy niskim bitrate'ie.
  Bardziej tonalnie, bardziej metalicznie, mniej syku.
- `3` sufit 0,5. Czterokrotnie większy zapas. Szum wypełniający może faktycznie zmieniać
  się z pasma na pasmo.
- `6` sufit 1,0. Maksymalny zapas. W parze z `--sbr-nf-weight` i `--sbr-nf-weight-from 1` to
  konfiguracja, w której pokrętła poziomu szumu wreszcie mają pole do pracy; samo z siebie
  podnosi głównie te pasma, które estymator już i tak dopychał do sufitu.
- `-1` wyłączone: decyduje tabela strojenia dla bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Współpraca i konflikty: to jest warunek dopuszczenia (gate) przed `--sbr-nf-weight`,
`--sbr-nf-weight-from` i `--sbr-nf-smooth`, a poziomy od 5 do 7 w `--sbr-softness` podnoszą
go samoczynnie (5 i 6 do 0,5, 7 do 1,0). `--sbr-noise-bands` ustawia rozdzielczość
częstotliwościową poziomu szumu, który ten sufit przycina, więc gęsty poziom szumu przy
niskim sufice to zmarnowana informacja poboczna.

#### `--sbr-noise-floor-offset <n>`

**Przesuwa cały poziom szumu SBR, dodając albo odejmując szum wypełniający wszędzie naraz.**

- Dotyczy: profili z SBR
- Wartości: `0..11`; domyślnie `-128` = wyłączone
- Wpisywanie: liczba całkowita

Tam gdzie `--sbr-nf-weight` dodaje szum warunkowo, na podstawie zmierzonej niezgodności, to
pokrętło po prostu przesuwa poziom szumu. Więcej oznacza więcej szumu wypełniającego w
rekonstrukcji na całej długości, co czyta się jako więcej powietrza i więcej syku, i co
przytłacza tę odziedziczoną strukturę tonalną, która przetrwała wybielanie. Mniej oznacza
górę bardziej suchą i bardziej tonalną.

Ta toporność bywa dokładnie tym, co jest potrzebne. Gdy `--sbr-nf-weight` odmawia ruchu, bo
każde pasmo jest obcięte na sufice, to pokrętło nadal działa, bo działa na samym poziomie
szumu, a nie na pomiarze różnicy.

Akceptowany zakres to `0..11` i nic poza tym. Poza nim własna obsługa szumu w FDK trafia na
asercję i przerywa pracę, więc koder odrzuca wartości spoza zakresu czystym błędem, zamiast
do tego dopuścić.

Przykłady, więcej szumu wypełniającego:
- Łagodnie: `--sbr-noise-floor-offset 3`. Odrobinę więcej powietrza na górze.
- Mocno: `--sbr-noise-floor-offset 7`. Wyraźnie głośniejsza szumowo góra; metaliczny
  charakter zostaje zasypany, a razem z nim drobny szczegół.
- Przesadnie: `--sbr-noise-floor-offset 11`, czyli maksimum. W górnym paśmie dominuje szum
  wypełniający. Talerze zamieniają się w kształtowany syk, a wynik jest jednoznacznym
  ustawieniem skrajnym, co czyni go przydatnym punktem odniesienia dla oceny, jak dużą część
  góry stanowi szum.

Przykłady, mniej szumu wypełniającego:
- Łagodnie: `--sbr-noise-floor-offset 2`. Odrobinę bardziej sucho.
- Mocno: `--sbr-noise-floor-offset 1`. Minimalne wypełnienie; górę niosą własna struktura
  pasma odtworzonego przez SBR i to, co zostawia po sobie wybielanie.
- Przesadnie: `--sbr-noise-floor-offset 0`, czyli minimum. Żadnego wkładu z przesunięcia.
  Na materiale tonalnym słychać metaliczne dzwonienie, które szum wypełniający ma maskować,
  co źle łączy się z niskimi ustawieniami `--sbr-invf` i co właśnie dlatego warto raz
  usłyszeć.

Współpraca i konflikty: nadal podlega `--sbr-noise-max`, więc wysokie przesunięcie wobec
sufitu 0,125 zostaje obcięte. Działa w tym samym kierunku co `--sbr-nf-weight`, ale
bezwarunkowo, co czyni je właściwym wyborem przy zmianie globalnej, a niewłaściwym, gdy o
miejscu dla szumu ma decydować koder. Wobec niskich poziomów
`--sbr-invf` walczy z innym artefaktem przy użyciu złego narzędzia: metaliczne dzwonienie od
niewybielonej struktury lepiej naprawić wybielaniem niż maskować szumem.

### Pasmo rdzenia: `--core-cutoff`

#### `--core-cutoff <hz>`

**Wymusza górną granicę pasma kodowanego przez rdzeń AAC, w Hz. Bez SBR robi dokładnie to
samo co `-w`; pod SBR może tylko zwęzić rdzeń, a to już eksperyment.**

- Dotyczy: każdego profilu
- Wartości: `0..48000` Hz (spoza zakresu jest odrzucane); koder zatrzymuje się na
  częstotliwości Nyquista rdzenia (połowa częstotliwości próbkowania rdzenia); domyślnie
  `0` = decyzja FDK
- Wpisywanie: liczba całkowita

Bez SBR (`-p 2`, `-p 23`, `-p 39` bez `-L 1`, DAB+ LC) ten przełącznik i `-w` to jedno i to
samo: w AAC-LC CBR i VBR, AAC-LD, AAC-ELD, DAB+ i na materiale 96 kHz z `--uncap-bandwidth`
`-w N` i `--core-cutoff N` dały strumienie identyczne co do bitu. Można używać tego, który
lepiej czyta się w skrypcie. Typowy przypadek to pełne pasmo przy 96 kHz:
`--uncap-bandwidth --core-cutoff 40000` albo `--uncap-bandwidth -w 40000`.

Pod SBR jest inaczej i dlatego ta karta stoi na końcu grupy. Przy inicjalizacji SBR tabela
strojenia ustawia pasmo rdzenia tak, żeby rdzeń kończył się tam, gdzie zaczyna się SBR, a
wartość z `-w` jest odrzucana. `--core-cutoff` jest stosowany później i zostaje, ale
użyteczny jest tylko w jedną stronę:

- **Podniesienie powyżej startu SBR nie daje nic słyszalnego.** Dekoder odtwarza wszystko
  powyżej startu SBR z własnych danych SBR i nadpisuje to, co wysłał tam rdzeń. Pomiar przy
  HE-AAC 64 kbps stereo, wejście 48 kHz: z `--core-cutoff 12000` sygnał po dekodowaniu
  powyżej 10,5 kHz ma koherencję z oryginałem 0,03, tyle samo co bez przełącznika, zarówno
  w dekoderze FDK, jak i w ffmpeg. Rdzeń i tak płaci za te prążki, co widać niżej: koherencja
  w paśmie 7..9,5 kHz spada z 0,95 do 0,92.
- **Obniżenie poniżej startu SBR otwiera dziurę.** Między nową górną granicą rdzenia a
  startem SBR nic nie jest kodowane, więc tego pasma brakuje, zamiast łagodnie opadać. Przy
  tych samych ustawieniach `--core-cutoff 9000` zostawia pasmo 9..10 kHz na poziomie od -15
  do -33 dB.

Szerszy rdzeń pod SBR daje podniesienie startu SBR przez `--sbr-start`, węższy jego
obniżenie. Rdzeń sam idzie za startem i oba pasma pozostają złączone. Przy HE-AAC 64 kbps
`--sbr-start 15` poszerza rdzeń z 10,5 do 12 kHz, z koherencją 0,93 aż do 11,5 kHz. Dla
`--core-cutoff` pod SBR zostaje świadomy eksperyment: posłuchanie, jak brzmi szczelina
między rdzeniem a SBR.

Granica odcięcia jest wyrównywana do granicy pasma SFB, więc `--verbose` podaje wartość
rzeczywistą: prośba o 17300 Hz i odpowiedź 17915 Hz to nie błąd. Górny limit to połowa
częstotliwości próbkowania rdzenia (12 kHz dla wejścia 48 kHz przy SBR w trybie dual rate).
Bez SBR obowiązuje też limit 20 kHz, który zdejmuje `--uncap-bandwidth` (grupa A).

Przykłady, bez SBR:
- Łagodnie: `-p 2 -b 128000 --core-cutoff 16000`. Ten sam strumień co z `-w 16000`.
- Mocno: `-p 23 -b 64000 --core-cutoff 12000 -f 10`. AAC-LD sam wybiera bardzo wąski rdzeń
  (około 5,8 kHz przy 64 kbps); ten przełącznik go poszerza, tak jak zrobiłoby `-w 12000`.
- Przesadnie: `-p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000` na wejściu 96 kHz.
  Dozwolone; przy niższym bitrate'ie to samo żądanie przepala budżet na niesłyszalną górę.

Przykłady, pod SBR (HE-AAC 64 kbps stereo, wejście 48 kHz, rdzeń 10,5 kHz):
- Podniesienie: `--core-cutoff 11500` albo `12000`. Bity wydane na prążki, które dekoder
  nadpisuje; powyżej 10,5 kHz nie słychać nic nowego.
- Obniżenie: `--core-cutoff 9000`. Opisana wyżej dziura 9..10 kHz.
- Obniżenie, przesadnie: `--core-cutoff 2000`. Brakuje wszystkiego od 2 kHz do startu SBR;
  przydaje się tylko po to, żeby usłyszeć, co niesie sam rdzeń.

Współpraca i konflikty: pod SBR o rdzeniu decyduje `--sbr-start`, a `--verbose` podaje
źródło pasma rdzenia: `[from --core-cutoff]` albo `auto`. `--uncap-bandwidth` jest potrzebny
tylko po to, żeby bez SBR wyjść powyżej 20 kHz.

## D. Maskowanie, kształtowanie szumu i szczegół

To grupa, która decyduje o tym, ile szumu koder zgadza się zostawić w każdym paśmie i gdzie
zamiast kodować sygnał po prostu go fabrykuje. Całą grupę niosą cztery pomysły, więc
wyjaśniamy je raz tutaj, a karty będą się do nich odwoływać.

**Próg maskowania.** Dla każdego pasma współczynnika skali (scale factor band, SFB),
w każdej ramce, model psychoakustyczny wylicza poziom, poniżej którego dodany szum
powinien być niesłyszalny, bo głośniejsza treść w tym paśmie i wokół niego go przykrywa.
Ten poziom to próg maskowania. Kwantyzator wydaje potem dokładnie tyle bitów, ile trzeba,
żeby zepchnąć szum kwantyzacji do progu, i ani jednego więcej. Po globalnym podniesieniu
progów koder uznaje, że więcej szumu jest dopuszczalne, więc wydaje mniej bitów, a wynik
robi się grubszy przy tym samym bitrate. Po ich obniżeniu koder żąda większej precyzji
wszędzie i bity kończą się szybciej, co przy CBR oznacza, że coś innego musi ustąpić.
`--ath-scale` to globalna skala tego progu. `--spread-mask` zmienia, jak daleko
maskowanie rozlewa się na sąsiednie pasma, a to już inne pytanie: nie jak głośna jest
maska, lecz jak szeroka.

**Wymagany SNR.** Niezależnie od progu FDK trzyma wymagany stosunek sygnału do szumu dla
każdego pasma, czyli minimalną jakość, jaką koder musi w tym paśmie dostarczyć bez względu
na to, co mówi próg. To najbliższy odpowiednik stałych tone masking noise i noise masking
tone, które udostępniają MusePack i LAME. Znaczy więcej, niż się wydaje, bo logika
unikania dziur przyciąga progi z powrotem do tego żądania: jeśli pętla dopasowania
bitrate'u chce „zagłodzić” pasmo, czyli zabrać mu bity tak, że koder je zeruje albo koduje
najgrubszym krokiem, żądanie SNR jest linią, której nie wolno jej przekroczyć.
`--minsnr-scale` skaluje to wymaganie, a `--minsnr-clamp-lo` i `--minsnr-clamp-hi`
przesuwają najwyższy i najniższy koniec okna, w którym wymaganie jest trzymane.

**TNS, czyli kształtowanie szumu w czasie.** Wewnątrz jednego bloku szum kwantyzacji
rozkłada się równo na cały blok w czasie. Dla transjentu to zły rozkład: blok z uderzeniem
perkusji ma szum także w cichym fragmencie przed uderzeniem, gdzie nic go nie maskuje, co
słychać jako pre-echo. TNS stosuje filtr predykcyjny w poprzek częstotliwości, co w
efekcie kształtuje obwiednię szumu w czasie tak, by szła za obwiednią sygnału i chowała
się pod atakiem. `--tns-mask` wybiera, które z filtrów mogą działać, a `--tns-order`, jak
długie mogą być. Dla LD i ELD to główna obrona przed pre-echem, bo te profile nie mają
bloków krótkich wcale.

**PNS, czyli percepcyjne podstawianie szumu.** Kiedy pasmo jest po prostu szumem,
kodowanie jego dokładnych linii widmowych jest marnotrawstwem: nikt na tym poziomie nie
odróżni jednej realizacji szumu od drugiej. PNS wyrzuca linie i zapisuje jedną liczbę,
energię pasma, plus flagę mówiącą "tutaj szum". Dekoder odtwarza losowy szum wyskalowany
do tej energii. Na materiale szumowym oszczędność jest duża. Ryzyko polega na tym, że
podstawione zostanie pasmo, które szumem naprawdę nie jest, i wtedy prawdziwy ton zamienia
się w syk. PNS steruje osiem przełączników z tej grupy: czy w ogóle działa, od jakiej
częstotliwości, czy obowiązuje warunek dopuszczenia (gate) przy niskim bitrate, jak głośny
jest fabrykowany szum, oraz trzy progi detekcji i minimalna szerokość pasma, które razem
decydują, które pasma się kwalifikują.

Jedna konwencja przed kartami: wszystkie pokrętła skalujące `--pns-*` przyjmują zwykły
mnożnik dziesiętny na fabryczną wartość FDK. `1.0` oznacza "zostaw wartość fabryczną",
`1.5` oznacza półtora raza tyle, `0.5` oznacza połowę. Wyjątkiem jest `--pns-min-width`,
które jest surową liczbą.

### `--tns-mask <n>`

**Wybiera, których filtrów kształtowania szumu w czasie koder może używać.**

- Dotyczy: każdego profilu z rdzeniem AAC. Najważniejszy w LD i ELD, gdzie TNS jest jedyną
  obroną przed pre-echem, oraz na materiale bogatym w transjenty w LC.
- Wartości: `0..15`, maska czterobitowa; domyślnie: `-1`, fabryczne `0xF`, wszystkie filtry
  dozwolone
- Wpisywanie: liczba całkowita

TNS kształtuje szum kwantyzacji w czasie, żeby chował się pod atakiem, a nie siedział w
ciszy przed nim. FDK ma więcej niż jeden filtr TNS, różniące się typem bloku i obszarem
częstotliwości, który obejmują, a maska mówi, które z nich są włączone. Fabrycznie jest to
`0xF`, czyli 15, czyli wszystkie cztery bity ustawione.

To raczej przełącznik diagnostyczny i laboratoryjny niż pokrętło jakości. Użyteczne
ustawienia to `0`, które wyłącza TNS zupełnie i pozwala usłyszeć, co robił, oraz `15`, czyli
wartość domyślna. Maski pośrednie pozwalają wyizolować jeden filtr, co bywa przydatne przy
przypisywaniu artefaktu do źródła, a poza tym rzadko.

Wyłączenie TNS ma drugą konsekwencję, o której warto wiedzieć: FDK wymusza wyłączenie PNS,
gdy TNS jest wyłączony. Zatem `--tns-mask 0` po cichu zabiera ze sobą percepcyjne
podstawianie szumu.

Wartości:
- `-1`: ustawienie fabryczne, wszystkie filtry włączone (`0xF`).
- `0`: TNS całkowicie wyłączony. Pre-echo na ostrych atakach robi się zauważalnie gorsze,
  szczególnie w LD i ELD; PNS też zostaje wymuszenie wyłączony.
- `1` do `14`: poszczególne filtry włączane bitami. Do izolowania wkładu jednego filtru.
- `15`: wszystkie filtry włączone, tak jak domyślnie.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --tns-mask 0 --verbose -o no_tns.m4a attacks.wav

Współpraca i konflikty: `--tns-order` ogranicza, jak długie mogą być włączone filtry, więc
te dwa czyta się razem. `--tns-thresh` z grupy E decyduje, jak często TNS w ogóle się
włącza, i zwykle to on jest właściwym pokrętłem zamiast maski. Ustawienie `0` tutaj wymusza
wyłączenie PNS bez względu na `--pns`. `--verbose` wypisuje efektywną maskę TNS jako maskę
bitową w zapisie szesnastkowym.

### `--tns-order <n>`

**Ogranicza długość filtru kształtowania szumu w czasie, wymieniając precyzję, z jaką
obwiednia szumu może iść za sygnałem, na informację poboczną, która to kosztuje.**

- Dotyczy: każdego profilu z rdzeniem AAC. Bloki krótkie są dodatkowo ograniczone przez
  standard do rzędu 5, więc wartości powyżej tej liczby dotyczą tylko bloków długich.
- Wartości: `1..12`; domyślnie: `-1`, własny rząd FDK
- Wpisywanie: liczba całkowita

Filtr TNS to predyktor liniowy działający w poprzek częstotliwości. Jego rząd to liczba
współczynników, a rząd decyduje, jak szczegółową obwiednię czasową może narzucić szumowi.
Niski rząd potrafi wytworzyć tylko zgrubny kształt, z grubsza "więcej szumu później, mniej
wcześniej". Wysoki rząd potrafi iść za skomplikowaną obwiednią, kosztem wysyłania większej
liczby współczynników filtru na ramkę jako informacja poboczna.

Niższe wartości robią TNS tańszym i tępszym. Przy niskim bitrate ta wymiana może być tego
warta, bo współczynniki konkurują z widmem o te same bity. Wyższe wartości pozwalają
kształtowaniu śledzić ataki precyzyjniej, i to jest kierunek, gdy pre-echo jest problemem, a
bity są dostępne, zwłaszcza w LD i ELD, gdzie nie istnieją bloki krótkie, które by pomogły.

Przykłady:
- Łagodnie, dłuższy filtr: `--tns-order 8`. Nieco dokładniejsze kształtowanie w czasie w
  blokach długich, mały koszt informacji pobocznej.
- Mocno, dłuższy filtr: `--tns-order 12`. Maksimum. Najlepsza rozdzielczość czasowa, jaką
  TNS może dać; warto spróbować w LD i ELD na materiale perkusyjnym.
- Przesadnie, dłuższy filtr: `--tns-order 12` jest już sufitem, więc ustawieniem skrajnym
  jest tutaj połączenie go z `--tns-thresh 0,5` z grupy E, żeby długi filtr działał też na
  prawie każdej ramce. Informacja poboczna zjada wtedy widoczną część niskiego budżetu
  bitowego, a w zamian pojawiają się dziury.
- Łagodnie, krótszy filtr: `--tns-order 5`. Limit bloku krótkiego zastosowany wszędzie.
  Taniej, z nieco grubszym kształtowaniem w blokach długich.
- Mocno, krótszy filtr: `--tns-order 3`. Tylko zgrubne kształtowanie obwiedni. Tanio;
  większość korzyści przeciw pre-echu zniknęła, ale grube kształtowanie zostaje.
- Przesadnie, krótszy filtr: `--tns-order 1`. Jeden współczynnik, który o obwiedni nie
  potrafi wyrazić prawie nic. TNS jest formalnie włączony i praktycznie bezużyteczny, co jest
  dobrym sposobem potwierdzenia, ile korzyści pochodzi z długości filtru, a nie z samego
  włączenia TNS.

Współpraca i konflikty: `--tns-mask` decyduje, które filtry istnieją, a to, jak długie są;
`0` na masce czyni ten przełącznik bez znaczenia. `--tns-thresh` z grupy E decyduje, jak
często działają. W LC bloki krótkie i tak załatwiają większość transjentów, więc rząd ma tam
mniejsze znaczenie niż w LD i ELD.

### `--pns <n>`

**Włącza lub wyłącza percepcyjne podstawianie szumu, nadpisując własną decyzję kodera.**

- Dotyczy: rdzeni AAC-LC, AAC-LD i AAC-ELD przy CBR. FDK wymusza wyłączenie PNS, gdy tryb
  bitrate'u to VBR i gdy TNS jest wyłączony, a te reguły stoją nad tym przełącznikiem.
- Wartości: `0` wyłączone, `1` włączone; domyślnie: `-1`, decyduje koder
- Wpisywanie: liczba całkowita, włącz/wyłącz

PNS zastępuje pasmo uznane za szum jedną wartością energii i flagą, a dekoder odtwarza szum
o tej energii. Na materiale szumowym oszczędność jest duża, a na materiale tonalnym ryzyko
polega na tym, że prawdziwy ton zamieni się w syk.

Własna decyzja kodera pochodzi z jego tabeli strojenia i zależy głównie od bitrate'u. Przy
64 kbps PNS jest normalnie już włączony, więc `1` to tam to samo co ustawienie fabryczne
i tylko `0` coś zmienia. Poniżej około 28 kbps tabela wyłącza PNS kompletnie, a w tym punkcie
ani `1` tutaj, ani `--pns-start` nie robią nic, bo PNS jest wyłączony wyżej w łańcuchu;
`--force-pns` to przełącznik, który ten warunek dopuszczenia (gate) obchodzi.

Szczere zastosowanie tej karty to diagnoza i świadomy wybór: `0`, żeby usłyszeć, co PNS
wnosił, `1`, żeby go narzucić tam, gdzie tabela wydaje się zachowawcza. `--verbose`
raportuje wynikowy stan PNS, co jest najszybszym sposobem sprawdzenia, czy żądanie przeszło
przez warunki dopuszczenia.

Wartości:
- `-1`: decyzja należy do FDK (ustawienie fabryczne, domyślne).
- `0`: PNS wyłączony. Na materiale szumowym pasma szumowe zostają porządnie zakodowane,
  a bity biorą się skądinąd.
- `1`: PNS włączony tam, gdzie pozwalają warunki dopuszczenia. Przy 64 kbps i wyżej zwykle
  pokrywa się to z zachowaniem fabrycznym; przy niskim bitrate wymaga `--force-pns`, żeby
  mieć jakikolwiek efekt.

Przykład w kontekście:

    fdkaac -p 2 -b 64000 --pns 0 --verbose -o no_pns.m4a in.wav

Współpraca i konflikty: `--force-pns` usuwa warunek niskiego bitrate'u. `--tns-mask 0`
wymusza wyłączenie PNS niezależnie od wartości ustawionej tutaj, bo FDK wymaga TNS dla PNS. Tryb
VBR (`-m 1..5`) robi to samo. `--pns-start`, `--pns-tonality`, `--pns-refpower`,
`--pns-gapfill` i `--pns-min-width` kształtują PNS, gdy już działa, a nie robią nic, gdy nie
działa.

Zmierzone: przy AAC-LC 64 kbps `--pns 1` daje strumień fabryczny, bo ustawienie domyślne ma
tam PNS już włączony, natomiast `--pns 0` go zmienia.

### `--pns-start <hz>`

**Ustawia najniższą częstotliwość, na której podstawianie szumu wolno działać.**

- Dotyczy: tych samych miejsc co `--pns`, i tylko wtedy, gdy PNS faktycznie działa.
- Wartości: częstotliwość w Hz; domyślnie: `-1`, własna częstotliwość startowa FDK
- Wpisywanie: liczba całkowita

PNS jest bezpieczny wysoko i groźny nisko. Niskie pasma noszą tony podstawowe, które
definiują wysokość i barwę, a podstawienie tam szumu jest natychmiast słyszalne. FDK ma więc
częstotliwość startową, poniżej której PNS nigdy nie jest używany, i zależy ona od bitrate'u
oraz częstotliwości próbkowania.

Przy niższym starcie kwalifikuje się więcej widma, więc więcej pasm dostaje kodowanie
szumowe i zwalnia się więcej bitów, przy rosnącym ryzyku podstawienia czegoś, co szumem nie
było. Przy wyższym PNS wycofuje się w górę, co jest bezpieczniejsze i tańsze w
oszczędnościach.

Jedna praktyczna pułapka: wartość jest wyrównywana do granicy pasma SFB, więc zmiana o
kilkaset Hz może wylądować w tym samym paśmie i dać identyczny strumień. Przy sondowaniu
warto przesuwać ją w krokach kilohercowych albo większych.

Przykłady:
- Łagodnie, niższy start: `--pns-start 3000`. PNS sięga trochę głębiej. Nieco więcej pasm
  kodowanych szumowo na materiale szerokopasmowym.
- Mocno, niższy start: `--pns-start 2000`. Wyraźnie więcej widma się kwalifikuje. Realna
  oszczędność na materiale szumowym; przy czymkolwiek z tonalnością w górnym średnim paśmie
  warto nasłuchiwać, czy ton nie robi się ziarnisty.
- Przesadnie, niższy start: `--pns-start 500`. PNS dozwolony prawie wszędzie. Dźwięki
  trzymane tracą swoją tożsamość tam, gdzie detektor się myli, a dokładnie temu artefaktowi
  częstotliwość startowa ma zapobiegać.
- Łagodnie, wyższy start: `--pns-start 6000`. PNS popchnięty trochę w górę, mała strata
  oszczędności i mały zysk bezpieczeństwa.
- Mocno, wyższy start: `--pns-start 12000`. Podstawiana może być tylko góra widma.
  Bezpiecznie, z większością korzyści utraconą.
- Przesadnie, wyższy start: `--pns-start 20000`. W większości konfiguracji powyżej
  kodowanego pasma, więc PNS nie ma już gdzie działać i jest to w praktyce `--pns 0` inną
  drogą.

Współpraca i konflikty: nie robi nic, gdy PNS jest wyłączony, co przy niskim bitrate
oznacza, że najpierw trzeba podać `--force-pns`. `--pns-min-width` ogranicza PNS przez
szerokość pasma, a nie przez częstotliwość, a skoro pasma rosną wraz z częstotliwością, oba
ograniczenia się nakładają. `--verbose` pokazuje wynikowy stan PNS.

Zmierzone: przy AAC-LC 64 kbps i 48 kHz fabryczny start to 4100 Hz. `4000` trafia do tego
samego pasma SFB i daje identyczny strumień; `2000` i `12000` oba go zmieniają.

### `--force-pns`

**Obchodzi warunek niskiego bitrate'u, który inaczej wyłącza podstawianie szumu całkowicie
poniżej około 28 kbps.**

- Dotyczy: AAC-LC, AAC-LD i AAC-ELD przy CBR, przy bitrate'ach poniżej tego warunku. Powyżej
  niego nie zmienia nic, bo PNS jest już dozwolony.
- Wartości: flaga; domyślnie: wyłączone
- Wpisywanie: flaga

Tabela strojenia FDK ma wiersz bitrate'u poniżej około 28 kbps, w którym każdy wpis PNS jest
zerem, dla każdej częstotliwości próbkowania. Konsekwencja zaskakuje ludzi: przy 24 kbps,
dokładnie tam, gdzie tanie kodowanie szumowe pomogłoby najbardziej, PNS jest całkowicie
wyłączony, a zarówno `--pns 1`, jak i `--pns-start` są ignorowane. Słyszalny skutek to
artefakt w stylu MDCT na treści szumowej, kojarzony raczej z MP3.

Ta flaga każe koderowi użyć pierwszego aktywnego wiersza tabeli zamiast wiersza zerowego,
więc PNS działa też przy 24 kbps i niżej. Reszta wymagań FDK nadal obowiązuje: PNS
potrzebuje włączonego TNS i trybu innego niż VBR, i nic w tej kompilacji tego nie zmienia.

Wartości:
- pominięte: warunek obowiązuje, PNS wyłączony poniżej około 28 kbps (zachowanie fabryczne).
- `--force-pns`: warunek obchodzony, PNS dostępny przy niskim bitrate.

Przykład w kontekście:

    fdkaac -p 2 -b 24000 --pns 1 --force-pns --pns-start 4000 -o out.m4a in.wav

Współpraca i konflikty: zwykle podaje się go razem z `--pns 1` i często z `--pns-start`, bo
sensem wymuszania PNS jest potem nim sterować. Bez sensu z `--tns-mask 0` albo przy VBR,
które i tak wymuszają wyłączenie PNS. Przy bitrate'ach powyżej tego warunku nie robi nic.

### `--pns-shared <0|1>`

**W pasmach, które oba kanały kodują jako szum, zapisuje jeden wspólny szum albo dwa niezależne, wszędzie tak samo.**

- Dotyczy: rdzeń AAC-LC w stereo z włączonym PNS (`--pns 1`, `--force-pns` albo `--pns-above`). Domyślnie wyłączone.
- Wartości: `1` = jeden wspólny szum (ten sam w L i R), `0` = niezależny szum w każdym kanale

PNS zastępuje pasmo szumem o właściwym poziomie. W parze kanałów format pozwala zaznaczyć,
pasmo po paśmie, że dekoder ma użyć tego samego szumu w obu kanałach. Ta flaga jedzie na
masce MS. Fabryczny FDK rozstrzyga ją dla każdego pasma osobno, mierząc korelację, więc
maskę zwykle trzeba zapisać pasmo po paśmie: przy niskim bitrate do około 25 bitów na ramkę.
Jednolity wybór pozwala masce wrócić do 2-bitowej postaci dla wszystkich pasm, jeśli reszta
ramki na to pozwala. Poziom szumu nadal jest zapisywany dla każdego kanału: format nie ma
wspólnego poziomu.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Typowo: `--pns-shared 1`. Maska MS z 28 do 2 bitów na ramkę na materiale skorelowanym.
- Szerokie stereo: `--pns-shared 0`. Niezależny szum zachowuje szerokość, maska zostaje tak
  droga jak przy ustawieniu fabrycznym.

Gdy maska i tak zostaje pasmo po paśmie (pasma pod obszarem PNS kodowane jako L/R na
szerokim materiale), przełącznik oszczędza niewiele: maska jest tania tylko wtedy, gdy
zgadzają się wszystkie pasma. ffmpeg 6.1.1 gra wspólny szum jako niezależny; dekoder FDK
jako wspólny.

### `--pns-hold <dB>`

**Pozwól sąsiednim pasmom szumu dzielić jeden poziom, żeby obwiednia szumu kosztowała mniej bitów.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `0.1..12.0` dB

Każde pasmo PNS zapisuje swój poziom szumu jako różnicę względem poprzedniego: 1 bit, gdy
jest taki sam, 3 do 9 bitów w innym wypadku. Z tym przełącznikiem pasmo, którego poziom
mieści się w podanej tolerancji względem poprzedniego pasma szumu, przejmuje jego poziom.
Obwiednia szumu w częstotliwości robi się grubsza, w krokach około 1,5 dB.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Łagodnie: `--pns-hold 3`. Poziomy szumu z 94 do 87 bitów na ramkę.
- Mocno: `--pns-hold 6`. 70 bitów na ramkę; uwolnione bity idą do kodowanego widma.
- Przesadnie: `--pns-hold 12`. Szum jest prawie płaski w całym swoim obszarze.

### `--pns-short <0|1>`

**Stosuj `--pns-above` także w ramkach krótkich (transjentach), tam gdzie to się opłaca.**

- Dotyczy: rdzeń AAC-LC z `--pns-above`. Domyślnie wyłączone.
- Wartości: `1` = włączone, `0` = fabryczne

Format AAC pozwala na podstawianie szumu w oknach krótkich; FDK nigdy tego nie robi, więc
powyżej `--pns-above` ramka transjentowa koduje pasma szumowe widmem. Z tym przełącznikiem
one też stają się szumem, ale tylko w ramce zakodowanej jako jedna grupa wszystkich 8 okien
(`--short-groups 1`). Zmierzone (1.4.0-beta.2) w mniejszych grupach: energie szumu zajmowały
19% ramki zamiast 2%, a atak wychodził o 2,6 do 6 dB ciszej, więc te grupy zostają przy
widmie. Przy grupowaniu fabrycznym przełącznik nie zmienia więc niczego (bit w bit).

Zmierzone z `--short-groups 1 --pns-above 4000`, próbka z atakiem w każdej ramce: plik przy
24 kbps z 9810 do 9226 B (-6%), pasmo szumu na poziomie źródła (+1,1 dB zamiast -1,4 dB).
Koszt: jeden poziom szumu obejmuje 8 okien, więc pasmo rozmazuje się na całą ramkę: pre-echo
+1 dB, atak o około 2 dB ciszej. Na muzyce z nielicznymi ramkami krótkimi nie robi prawie
nic (-0,3% pliku). Dla niskich bitrate'ów, przy których pasma inaczej by brakowało; najpierw
odsłuch.

### `--pns-budget <0|1>`

**Licz bity, które zajmują poziomy szumu PNS, żeby ramce nie zabrakło bitów i nie obcięła widma.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `1` = włączone, `0` = fabryczne

FDK planując bity ramki traktuje pasmo PNS jako darmowe, a każdy poziom szumu kosztuje bity
w strumieniu. Przy niskim bitrate i dużym obszarze PNS ramka się wtedy przepełnia, a
awaryjne cięcie zdejmuje pasma od góry. Domyślnie koder nie zalicza mu pasm PNS, więc tnie
dalej przez obszar szumu w kodowane widmo pod nim: całe ramki tracą wszystko powyżej około
1,5 kHz, co słychać jako trzask w rytmie ramek. Z tym przełącznikiem cel bitowy dla widma
jest obniżony o koszt poziomów szumu, a cięcie odzyskuje te bity, gdy zdejmuje pasmo szumu.

Zmierzone na akustycznej muzyce stereo, `-b 24000 --block-bias 0 -w 5500 --pns-above 2200`:
ramki z awaryjnym cięciem z 42 na 766 do 1, modulacja pasma szumu w rytmie ramek z 19,7 do
11,0 dB (11,6 bez PNS). Razem z `--pns-shared 1`: 0 obciętych ramek, 8,3 dB.

Zalecane przy każdym wymuszonym PNS przy niskim bitrate: `--pns-budget 1 --pns-shared 1`, opcjonalnie `--pns-hold 3`.

### `--pns-gain <x>`

**Ustawia, jak głośny jest fabrykowany szum względem energii pasma, które zastąpił.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` zostawia energię bez zmian; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy PNS podstawia pasmo, jedyną liczbą, którą zapisuje, jest energia szumu tego pasma, a
dekoder odtwarza losowy szum wyskalowany do niej. Ten przełącznik skaluje zapisaną energię
bezpośrednio, więc jest pokrętłem "jak głośny jest podstawiony szum" i tym pokrętłem PNS, po
które sięga się najczęściej.

Przy `1.0` odtworzony szum nosi energię pierwotnego pasma, czyli to, co mechanizm zakłada.
Powyżej `1.0` wypełnienie jest głośniejsze od pierwotnego, co pomaga, gdy podstawiony szum
brzmi zbyt nieśmiało i miks robi się matowy: prawdziwe poziomy szumu po podstawieniu często
wydają się cichsze, bo odtworzony szum nie ma struktury oryginału. Poniżej `1.0` jest
cichsze, i to jest ruch na sytuację, gdy wypełnienie syczy bardziej niż źródło.

To czyste terytorium gustu, bez poprawnej wartości, i właśnie dlatego oba kierunki warto
wypróbować na konkretnym materiale przed podjęciem decyzji.

Przykłady:
- Łagodnie, głośniej: `--pns-gain 1,2`. Nieco pełniejsze wypełnienie szumem; przydatne, gdy
  szum blach traci obecność.
- Mocno, głośniej: `--pns-gain 2.0`. Dwa razy więcej energii. Wyraźnie więcej syku i
  powietrza; na szumie szerokopasmowym może to brzmieć bliżej źródła niż wartość nominalnie
  poprawna.
- Przesadnie, głośniej: `--pns-gain 8.0`. Osiem razy więcej energii. Podstawione pasma
  dominują górę miksu oczywistym sztucznym sykiem, co jest szybkim sposobem usłyszenia
  dokładnie tego, które pasma zabrał PNS.
- Łagodnie, ciszej: `--pns-gain 0,8`. Nieco dyskretniejsze wypełnienie.
- Mocno, ciszej: `--pns-gain 0.4`. Wypełnienie szumem wyraźnie poniżej pierwotnej energii.
  Góra brzmi czyściej, a także bardziej matowo, i poziom szumu może zniknąć.
- Przesadnie, ciszej: `--pns-gain 0.0`. Podstawione pasma nie noszą w ogóle energii, czyli
  są ciche, więc PNS staje się generatorem dziur. Ustawienie skrajne i przydatny pokaz, ile
  sygnału nosiły pasma szumowe.

Współpraca i konflikty: nie robi nic, gdy PNS jest wyłączony. `--pns-tonality`,
`--pns-refpower`, `--pns-gapfill` i `--pns-min-width` decydują, które pasma są podstawiane;
ten zmienia tylko, jak głośny jest wynik. Użycie go na `0.0` to niezdarniejszy sposób
osiągnięcia tego, co `--pns 0` daje porządnie.

### `--pns-tonality <x>`

**Skaluje próg tonalności, który decyduje, czy pasmo liczy się jako szum i może być
podstawione.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

PNS odpala tylko na pasmach, które koder uznaje za szumopodobne, co mierzy jako niską
tonalność. Próg to linia między "tonalne, zakoduj porządnie" a "szum, podstaw je". Ten
przełącznik skaluje tę linię.

Wyżej niż `1.0` pozwala kwalifikować się większej liczbie pasm, w tym nieco tonalnym, więc
podstawianie szumu rozszerza się: więcej widma zastąpionego tanim opisem, więcej
oszczędności i więcej ryzyka, że prawdziwy ton rozmaże się w syk. Niżej niż `1.0` czyni
detektor wybredniejszym, więc podstawiane są tylko pasma wyraźnie szumowe, co jest
bezpieczniejsze i oszczędza mniej.

To pokrętło od szerokości PNS, w przeciwieństwie do `--pns-start`, które kontroluje, gdzie
PNS jest geograficznie dozwolony, i do `--pns-min-width`, które kontroluje go rozmiarem
pasma.

Przykłady:
- Łagodnie, szersze podstawianie: `--pns-tonality 1,3`. Kilka granicznych pasm więcej
  staje się PNS. Mała oszczędność, małe ryzyko.
- Mocno, szersze podstawianie: `--pns-tonality 2.0`. Dwa razy wyższy próg. Wyraźnie więcej
  pasm podstawionych; na materiale szumowym realna oszczędność, na materiale tonalnym
  słyszalne rozmazanie składowych.
- Przesadnie, szersze podstawianie: `--pns-tonality 20.0`. Prawie wszystko powyżej
  częstotliwości startowej liczy się jako szum. Tony trzymane zamieniają się w syk, a góra
  miksu staje się posłaniem szumu: ustawienie skrajne, przy którym mechanizmu nie da się
  pomylić z niczym innym.
- Łagodnie, węższe podstawianie: `--pns-tonality 0,8`. Nieco wybredniejsza detekcja.
- Mocno, węższe podstawianie: `--pns-tonality 0.4`. Podstawiany jest tylko jednoznaczny
  szum. Bezpiecznie, z dużą częścią oszczędności utraconą.
- Przesadnie, węższe podstawianie: `--pns-tonality 0.01`. W praktyce nic się nie kwalifikuje,
  więc trafia to tam, gdzie `--pns 0`, zostawiając PNS formalnie włączony.

Współpraca i konflikty: `--pns-refpower` jest drugą połową decyzji detekcyjnej i oba muszą
się zgodzić, zanim pasmo zostanie PNS, więc ruszanie tylko tym może być zablokowane przez
warunek mocy. `--pns-gain` zmienia głośność wyniku, nie wybór. `--pns-min-width` może
wykluczyć pasmo, które ten test przeszło.

### `--pns-refpower <x>`

**Skaluje próg mocy odniesienia, który poziom pasma musi spełnić, zanim zostanie uznane za
kandydata na szum.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Tonalność nie jest jedynym testem. Moc każdego pasma jest też porównywana z odniesieniem,
zanim pasmo może zostać kandydatem PNS, co trzyma mechanizm z dala od pasm, których poziom
czyni podstawianie złą wymianą. Ten przełącznik przesuwa ten warunek mocy.

Oba testy detekcji są koniunkcyjne: tonalność i moc odniesienia muszą się obie zgodzić,
zanim pasmo zostanie PNS. To dlatego ruszanie samym `--pns-tonality` czasem nie robi nic i
dlatego tę kartę i tamtą próbuje się zwykle razem.

Wartość mnoży sam próg. Powyżej `1.0` go podnosi, więc przechodzi mniej pasm i PNS się
wycofuje; poniżej `1.0` go obniża, więc przechodzi więcej pasm. Zmierzone na akustycznej
muzyce stereo przy 64 kbps LC, pasm PNS na ramkę: 5,5 przy `0.01`, 5,1 przy `0.4`, 4,8 przy
`0.8`, 4,6 przy `1.0`, 4,3 przy `1.3`, 3,6 przy `2.0`, 0 przy `20.0`.

Przykłady:
- Łagodnie, luźniejszy warunek: `--pns-refpower 0,8`. Nieco więcej kandydatów (z 4,6 do 4,8 pasma).
- Mocno, luźniejszy warunek: `--pns-refpower 0.4`. 5,1 pasma. W połączeniu z podniesionym
  `--pns-tonality` to miejsce, w którym szerokie podstawianie szumu naprawdę się zaczyna.
- Przesadnie, luźniejszy warunek: `--pns-refpower 0.01`. Test mocy przestaje rozróżniać i wybór
  zostaje po stronie tonalności (5,5 pasma), co dobrze pokazuje, ile pracy ten warunek wykonywał.
- Łagodnie, surowszy warunek: `--pns-refpower 1,3`. Nieco mniej kandydatów (4,3 pasma).
- Mocno, surowszy warunek: `--pns-refpower 2.0`. Dwa razy wyższy próg, 3,6 pasma. PNS wycofuje
  się do pasm, gdzie jest najmniej ryzykowny.
- Przesadnie, surowszy warunek: `--pns-refpower 20.0`. Nic nie przechodzi, więc PNS jest na
  papierze włączony, a w praktyce bezczynny.

Współpraca i konflikty: koniunkcyjny z `--pns-tonality`, więc warto testować je parą.
`--pns-gapfill` może i tak dodać pasmo między dwoma pasmami PNS, które tego testu nie
przeszło. Nic tutaj nie ma znaczenia, gdy PNS jest wyłączony.

### `--pns-gapfill <x>`

**Skaluje próg heurystyki, która zamyka jednopasmową lukę między dwoma podstawionymi
pasmami.**

- Dotyczy: wszędzie tam, gdzie PNS działa, i tylko wtedy, gdy dwa pasma PNS już siedzą po
  obu stronach zakodowanego pasma. Stan faktyczny: podłączone, bez zaobserwowanego efektu
  na materiale testowym.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy PNS oznaczył pasma po obu stronach wąskiej luki, zakodowana wyspa uwięziona między
dwoma pasmami szumu jest zwykle gorsza niż rozciągnięcie szumu przez nią: wyspa to jedyne
miejsce z prawdziwą strukturą widmową i odstaje. Heurystyka wypełniania luk może oznaczyć to
pasmo też jako PNS, a ten przełącznik skaluje próg, którego ona używa.

Wyżej czyni wypełnianie luk chętniejszym, niżej niechętnym. Oba kierunki są co najwyżej
subtelne, bo warunek wstępny jest surowy: dla AAC-LC reguła wymaga, żeby oba sąsiednie pasma
były już PNS, a na naszym materiale taka konfiguracja nie wystąpiła, więc żadna wartość tego
przełącznika nie zmieniła wyjścia. Przełącznik jest obecny i podłączony i może działać na
materiale, którego widmo wytworzy wymagany wzorzec, ale nie należy go traktować jako
codziennego narzędzia.

Prawdziwą dźwignię na to, jak duża część widma staje się szumem, dają zamiast tego
`--pns-tonality` albo `--pns-start`.

Przykłady:
- Łagodnie, więcej wypełniania: `--pns-gapfill 1,3`. Nieco chętniej zamyka lukę.
- Mocno, więcej wypełniania: `--pns-gapfill 3.0`. Wyraźnie chętniej, tam gdzie warunek
  wstępny w ogóle występuje.
- Przesadnie, więcej wypełniania: `--pns-gapfill 50.0`. Każda luka między dwoma pasmami PNS
  zostaje zamknięta. Na materiale, który to wyzwala, daje ciągłe obszary szumu tam, gdzie
  koder zachował wyspę.
- Łagodnie, mniej wypełniania: `--pns-gapfill 0,7`. Nieco bardziej niechętnie.
- Mocno, mniej wypełniania: `--pns-gapfill 0.3`. Luki zostają zakodowanymi wyspami.
- Przesadnie, mniej wypełniania: `--pns-gapfill 0.0`. Wypełnianie luk nie zdarza się nigdy,
  co jest najbliższą rzeczą do wyłączenia tej heurystyki.

Współpraca i konflikty: zależy całkowicie od tego, czy `--pns-tonality` i `--pns-refpower`
wytworzą dwa sąsiadujące pasma PNS z luką, więc rozszerzenie tamtych najpierw to jedyny
sposób, żeby dać temu przełącznikowi na czym działać.

Zmierzone: brak zmiany strumienia na 4 sygnałach testowych, 5 bitrate'ach i 6 wartościach
tego przełącznika w AAC-LC. Warunek wstępny reguły wypełniania luk w AAC-LC, czyli oba
sąsiednie pasma już jako PNS, na tym materiale nie wystąpił.

### `--pns-min-width <n>`

**Ustawia minimalną szerokość pasma SFB, w liniach widmowych, jaką pasmo musi mieć, zanim
podstawianie szumu będzie mogło na nim zadziałać.**

- Dotyczy: wszędzie tam, gdzie PNS działa. Jakikolwiek efekt mają tylko wartości powyżej
  wbudowanej wartości domyślnej, co dla AAC-LC oznacza powyżej 16.
- Wartości: surowa liczba linii, `1` i wyżej; domyślnie: `-1`, wyłączone, czyli własne
  minimum FDK (16 dla AAC-LC)
- Wpisywanie: liczba całkowita

Pasma SFB robią się szersze wraz ze wzrostem częstotliwości. Wąskie pasmo na dole widma
mieści niewiele linii i zwykle niesie treść tonalną, a podstawienie tam szumu rzuca się w
uszy. Szerokie pasmo wysoko mieści wiele linii i tam właśnie PNS jest na miejscu. Ten
przełącznik wyraża tę preferencję wprost, jako minimalną szerokość.

Warto zwrócić uwagę na asymetrię, bo oczywisty odczyt nazwy jest błędny. Podniesienie
minimum ogranicza PNS do pasm szerszych, czyli do górnego widma, i to działa. Obniżenie go
poniżej własnego minimum FDK nie otwiera PNS na węższe pasma: wartość domyślna FDK jest już
efektywną dolną granicą, więc wartości poniżej niej dają strumień fabryczny. Jest tu jeden
użyteczny kierunek, w górę.

W odróżnieniu od przełączników `--pns-*` powyżej, to surowa liczba linii, nie mnożnik.

Przykłady:
- Łagodnie, tylko szersze pasma: `--pns-min-width 24`. PNS ograniczony trochę wyżej w widmie.
  Małe zmniejszenie podstawiania, mała strata oszczędności.
- Mocno, tylko szersze pasma: `--pns-min-width 32`. Podstawiane mogą być tylko wyraźnie
  szerokie pasma. Bezpieczniej na materiale tonalnym, z widocznym spadkiem oszczędności z PNS.
- Przesadnie, tylko szersze pasma: `--pns-min-width 64`. Przy AAC-LC 64 kbps nie zostaje żadne
  kwalifikujące się pasmo, więc wynik jest identyczny jak `--pns 0`: PNS jest włączony i nie
  ma niczego, czego wolno mu dotknąć.
- W dół nie ma użytecznego przykładu: `2` i `4` oba dają strumień fabryczny, bo wbudowane
  minimum 16 już wiąże.

Współpraca i konflikty: nakłada się z `--pns-start`, który ogranicza PNS częstotliwością, a
nie szerokością; skoro szerokość rośnie wraz z częstotliwością, oba ograniczenia wzmacniają
się nawzajem i wygrywa ciaśniejsze. Nie robi nic, gdy PNS jest wyłączony.

Zmierzone: przy AAC-LC wartości `2` i `4` dają strumień fabryczny, co potwierdza, że
wbudowana wartość domyślna 16 jest najniższym dozwolonym poziomem. `64` przy AAC-LC 64 kbps
daje dokładnie ten strumień, który daje `--pns 0`, więc przy tej szerokości PNS jest
praktycznie wyłączony.

### Progi maskowania: warstwa zaawansowana

Siedem przełączników poniżej przesuwa sam model psychoakustyczny: próg, rozlewanie
maskowania, wymagany SNR i granice wokół niego. Większość przyjmuje liczby Q8. Przy CBR
pętla dopasowania musi trafić w tę samą liczbę bitów na ramkę bez względu na to, co mówi
model, więc odbiera większość ich efektu: zmierzone na muzyce, każdy z nich zmienia wynik po
dekodowaniu o setne albo dziesiąte części decybela. Przy VBR (`-m 1..5`) liczba bitów idzie
za modelem, a `--ath-scale` i `--minsnr-scale` stają się prawdziwymi ustawieniami jakości,
opłaconymi rozmiarem pliku. `--spread-mask`, obie granice okna, `--reduce-clamp` i
`--mask-slope` zostają przełącznikami laboratoryjnymi w obu trybach.

#### `--ath-scale <n>`

**Globalna skala progu maskowania: jedno pokrętło na "bądź czystszy" albo "bądź
oszczędniejszy" w całym widmie.**

- Dotyczy: każdego profilu z rdzeniem AAC, czyli wszystkich. Pod SBR działa tylko na
  rdzeń, bo SBR ma własny model szumu.
- Wartości: Q8, `1..4096`, gdzie `256` to x1,0; domyślnie: `256`, bez zmiany
- Wpisywanie: liczba całkowita

Próg maskowania to poziom szumu, o którym model sądzi, że w danym paśmie jest niesłyszalny.
`--ath-scale` mnoży wszystkie progi naraz. Powyżej 256 progi rosną, koder dochodzi do
wniosku, że więcej szumu jest niesłyszalne, i wydaje mniej bitów na pasmo: kodowanie
bardziej agresywne, co przy stałym bitrate oznacza, że te bity idą gdzie indziej albo
ramka po prostu kończy się wcześniej. Poniżej 256 progi spadają, więcej treści uznaje się
za warte dokładnego zakodowania, a zapotrzebowanie na bity rośnie.

Dwie rzeczy nie pozwalają temu pokrętłu być tak mocnym, jak sugeruje jego zakres. Działa na
logarytmicznej kopii progu, a część jego efektu znosi dalej w łańcuchu logika minimalnego
SNR oraz sufit redukcji progu 29 dB w kwantyzatorze CBR. Poza tym przy CBR globalna zmiana
progu raczej przetasowuje bity, niż zmienia ich liczbę, bo pętla dopasowania bitrate'u i tak
musi trafić w cel. `--minsnr-scale` działa na linii, do której unikanie dziur przyciąga
progi z powrotem, a tej `--ath-scale` przesunąć nie może.

Na swoje zarabia przy wysokim bitrate, gdzie jest nadwyżka bitów, a celem jest znalezienie
przez koder większej liczby rzeczy wartych zakodowania. Wartości trochę poniżej 256
zamieniają tę nadwyżkę w zachowany szczegół. Przy niskim bitrate użyteczny kierunek jest
zwykle w górę, jako sposób na świadome przyjęcie szorstkości zamiast pozostawienia pętli
dopasowania bitrate'u decyzji, gdzie ciąć.

Przykłady:
- Łagodnie, czyściej: `--ath-scale 230`. Progi niższe o jakieś 10 procent. Trochę więcej
  bitów na cichy szczegół; subtelne przy każdym bitrate, najlepiej widoczne powyżej
  192 kbps.
- Mocno, czyściej: `--ath-scale 180`. Wyraźne przesunięcie wysiłku w stronę treści o
  niskim poziomie. Ogony pogłosu i szum pomieszczenia zostają w lepszym stanie; przy napiętym
  bitrate coś innego jest wtedy „głodzone”.
- Przesadnie, czyściej: `--ath-scale 64`. Progi na jednej czwartej wartości fabrycznej.
  Modelowi mówi się praktycznie, że nic nie jest maskowane. Przy CBR pętla dopasowania
  bitrate'u walczy przez całą ramkę, a wynik nie jest lepszy, tylko inaczej skompromitowany; warto raz
  posłuchać, żeby zrozumieć, gdzie leży granica.
- Łagodnie, grubiej: `--ath-scale 290`. Trochę więcej szumu przyjętego na pasmo, mała
  oszczędność bitów.
- Mocno, grubiej: `--ath-scale 512`. Progi podwojone. Słyszalnie bardziej ziarniste na
  materiale szerokopasmowym, zauważalnie tańsze na pasmo.
- Przesadnie, grubiej: `--ath-scale 2048`. Progi osiem razy wyższe niż fabryczne. Koder
  wierzy, że prawie wszystko jest zamaskowane, a wynik to skrajna wersja agresywnego kodowania:
  wydrążony, syczący, z utraconym szczegółem wszędzie tam, gdzie obok siedzi coś
  głośniejszego.

Współpraca i konflikty: `--minsnr-scale` jest mocniejszym narzędziem do tego samego celu i
częściowo nadpisuje efekt tego przełącznika, bo żądanie SNR jest dolną granicą jakości,
przez którą podniesiony próg nie przepchnie. `--reduce-clamp 0` pozwala pchać progi głębiej
niż sufit 29 dB, co daje niskim wartościom tutaj więcej miejsca na działanie.
`--spread-mask` to pokrętło towarzyszące, od szerokości maskowania, a nie od jego wysokości.

Zmierzone (akustyczna muzyka stereo): CBR 96 kbps, `128` obniża łączny błąd o 0,04 dB, a
`384` podnosi go o 0,04 dB; przy 256 kbps `190` obniża go o 0,2 dB przy niezmienionym
rozmiarze pliku. VBR `-m 3` (około 108 kbps): `128` daje plik większy o 5,5% i błąd niższy
o 0,8 dB, `192` odpowiednio 2,3% i 0,3 dB, `384` plik mniejszy o 3,3% i błąd wyższy o
0,5 dB, `512` mniejszy o 5,6% i błąd wyższy o 0,8 dB.

#### `--minsnr-scale <n>`

**Skaluje stosunek sygnału do szumu, jaki koder musi dostarczyć w każdym paśmie; najbardziej
bezpośrednie pokrętło od szczegółu w tej grupie.**

- Dotyczy: każdego profilu z rdzeniem AAC.
- Wartości: Q8, `1` i wyżej bez udokumentowanego sufitu, gdzie `256` to x1,0; domyślnie:
  `-1`, wyłączone
- Wpisywanie: liczba całkowita

Oprócz progu maskowania każde pasmo nosi wymagany SNR kodowania. To gwarantowane minimum:
cokolwiek mówi wyliczenie progu, koder musi w tym paśmie osiągnąć taką jakość. FDK
wyprowadza je ze swoich tabel strojenia i jest to najbliższy odpowiednik stałych tone
masking noise i noise masking tone, które MusePack udostępnia bezpośrednio.

Poniżej 256 żąda wyższego SNR niż ustawienie fabryczne, czyli więcej szczegółu i więcej
bitów na pasmo. Powyżej 256 żąda mniej i wynik jest grubszy. Warto zwrócić uwagę na
odwrócenie wbrew intuicji: skalowany jest dozwolony szum, nie żądana jakość, więc mniej
znaczy czyściej.

Od `--ath-scale` różni się miejscem, w które trafia zmiana. Logika unikania dziur przyciąga
progi z powrotem do linii minimalnego SNR, więc wymagany SNR jest dolną granicą, której
pętla dopasowania nie może przebić, a przesunięty próg może zostać cofnięty. W pomiarze przy
VBR oba kosztują mniej więcej tyle samo za każdy zyskany decybel; `--minsnr-scale` kieruje
więcej zysku w dolne pasmo, `--ath-scale` rozkłada go równiej.

Przykłady:
- Łagodnie, więcej szczegółu: `--minsnr-scale 220`. Skromne podniesienie wymaganej jakości
  na pasmo. Widoczne przy średnim bitrate jako mniej wypadających pasm.
- Mocno, więcej szczegółu: `--minsnr-scale 150`. Wyraźnie wyższe żądanie. W połączeniu z
  hojnym bitrate to właśnie to pokrętło zamienia nadwyżkę bitów w zachowany szczegół; przy
  napiętym bitrate zmusza pętlę dopasowania bitrate'u, żeby cięła w innym miejscu.
- Przesadnie, więcej szczegółu: `--minsnr-scale 16`. Żądanie szesnastokrotne. Ramka nigdy go
  nie zaspokoi, a pętla dopasowania bitrate'u spędza całą ramkę w trybie ratunkowym, więc
  wynik jest gorszy niż fabryczny; pouczające raz, ale to nie ustawienie do pracy.
- Łagodnie, grubiej: `--minsnr-scale 300`. Trochę mniej wymagane na pasmo, mała oszczędność.
- Mocno, grubiej: `--minsnr-scale 512`. Połowa żądanej jakości. Słyszalnie bardziej
  ziarniste, z realną oszczędnością bitów, którą można przekierować innymi przełącznikami.
- Przesadnie, grubiej: `--minsnr-scale 4096`. Żądanie jest praktycznie usunięte, dolna granica
  jakości znika, a pasma swobodnie się zapadają. Ustawienie skrajne, które pokazuje, czego to
  żądanie SNR broniło.

Współpraca i konflikty: wynik jest trzymany w oknie, które wyznaczają `--minsnr-clamp-lo`
(najwyższe wymaganie, w FDK około 25 dB) i `--minsnr-clamp-hi` (najniższe, około 1 dB).
Daleko poniżej 256 wymaganie dochodzi do górnej granicy, którą `--minsnr-clamp-lo` poniżej
256 odsuwa dalej. `--reduce-clamp 0` zdejmuje sufit redukcji 29 dB w pętli CBR.

Zmierzone: przy CBR pętla dopasowania odbiera większość efektu: przy 96 kbps na muzyce
akustycznej `150` daje dolne pasmo czystsze o 0,8 dB i górę zgrubniejszą o 0,05 dB, a łączny
wynik zmienia się o setne części decybela. Przy VBR `-m 3` działa jak ustawienie jakości:
`150` daje plik większy o 1,3% i błąd niższy o 0,3 dB, `64` plik większy o 7,2% i błąd
niższy o 1,1 dB, `512` plik mniejszy o 0,7% i błąd wyższy o 0,3 dB.

#### `--spread-mask <n>`

**Skaluje, jak daleko maskowanie rozlewa się z jednego pasma na sąsiednie, co decyduje, ile
cichych pasm obok głośnych zostanie zakodowanych.**

- Dotyczy: każdego profilu z rdzeniem AAC. W naszych pomiarach efekt był mały przy każdym
  sprawdzonym bitrate'ie, przy CBR i przy VBR.
- Wartości: Q8, `0` i wyżej, gdzie `256` to x1,0; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Głośny ton, czyli sygnał maskujący (masker), maskuje szum nie tylko we własnym paśmie:
maskuje też skraj pasm wokół siebie, bardziej w górę niż w dół. Model symuluje to funkcją
rozmycia. To właśnie rozmycie jest powodem, dla którego ciche pasmo stojące obok głośnego
dostaje prawie zero bitów: model uznaje jego treść za dźwięk maskowany, przykryty przez
sąsiada.

Ten przełącznik skaluje rozmycie. Poniżej 256 oznacza mniej rozmycia maskowania, więc skraj
się zwęża, mniej pasm liczy się jako przykryte i więcej z nich zostaje zakodowanych. To ta
sama idea, co rozluźnienie założenia "ton maskuje szum" w innych koderach. Powyżej 256 skraj
się rozszerza i więcej pasm zostaje spisanych na straty jako przykryte przez sąsiadów, co
jest tańsze i bardziej płaskie.

W teorii rozlewanie decyduje najwięcej tam, gdzie bitów jest mało, ale jeszcze nie
rozpaczliwie: znacznie niżej „głodzona” jest cała ramka i pętla dopasowania tnie wszędzie,
znacznie wyżej bitów starcza na wszystko. Na materiale testowym rozlewanie decydowało jednak
o bardzo niewielu pasmach przy każdym bitrate'ie, zobacz Zmierzone.

Przykłady:
- Łagodnie, mniej maskowania: `--spread-mask 230`. Nieco węższy skraj. Kilka pasm więcej
  zostaje obok głośnej treści; więcej szczegółu wokół mocnych składowych.
- Mocno, mniej maskowania: `--spread-mask 180`. Wyraźnie więcej pasm zakodowanych obok
  głośnych. Na gęstym materiale to właśnie tutaj odzyskuje się szczegół wokół energii
  transjentów, realnym kosztem bitów.
- Przesadnie, mniej maskowania: `--spread-mask 32`. Rozmycie prawie usunięte. Model traci
  większość zdolności do decydowania, co jest przykryte, więc bity rozsypują się po pasmach,
  które naprawdę są niesłyszalne; ustawienie skrajne, pokazujące, co daje funkcja rozmycia.
- Łagodnie, więcej maskowania: `--spread-mask 300`. Odrobinę szerszy skraj, mała oszczędność.
- Mocno, więcej maskowania: `--spread-mask 512`. Rozmycie podwojone. Zauważalnie bardziej
  płaskie widmo obok głośnej treści, tanio, z cichym szczegółem spisanym na straty.
- Przesadnie, więcej maskowania: `--spread-mask 1536`. Prawie wszystko blisko czegokolwiek
  głośnego liczy się jako zamaskowane. Wynik jest wydrążony i rozmazany, przydatny głównie
  jako przeciwny koniec pokazu.

Współpraca i konflikty: warto łączyć z `--ath-scale` poniżej 256, gdy maska ma być zarazem
niższa i węższa; oba są niezależne i efekty się dodają. `--minsnr-scale` nadal ogranicza
wynik od dołu, więc bardzo niskie rozmycie nie zepchnie żadnego pasma poniżej jego żądania
SNR.

Zmierzone (akustyczna muzyka stereo): CBR 96 kbps, `128` i `384` w granicach 0,01 dB od
ustawień domyślnych; na `dense48` przy 40 kbps w granicach 0,05 dB. VBR `-m 3`: `128` daje
plik większy o 0,4%, `64` o 0,6%, a błąd zmienia się o mniej niż 0,01 dB. Na materiale
testowym rozlewanie maskowania decyduje o bardzo niewielu pasmach; karta opisuje mechanizm,
pomiar jego skalę.

#### `--minsnr-clamp-hi <n>`

**Przesuwa najgrubszy SNR, jaki koder musi zachować w każdym paśmie: gwarancję, że nawet
najmniej ważne pasmo zachowa trochę szczegółu.**

- Dotyczy: każdego profilu z rdzeniem AAC. Najwięcej znaczy przy VBR.
- Wartości: Q8, `1..320`, gdzie `256` to x1,0 (domyślnie w FDK); `-1` wyłączone (domyślnie).
  `320` to najluźniejsza możliwa granica, 0 dB.
- Wpisywanie: liczba całkowita

Każde pasmo ma wymagany SNR z tabel strojenia (zobacz *Wymagany SNR* wyżej), a FDK trzyma to
wymaganie w oknie. Ten przełącznik przesuwa zgrubny koniec okna, który w FDK oznacza, że
żadne pasmo nie może mieć więcej szumu niż około 1 dB pod własną energią. Ma to znaczenie
dla pasm, które tabele uznają za mało ważne: bez tej granicy zostałyby niemal całkiem
odpuszczone.

Poniżej 256 granica się zacieśnia, więc każde pasmo ma zagwarantowany wyższy SNR, a pasma
najmniej wymagające dostają więcej bitów. Powyżej 256 granica luzuje się, aż do 0 dB przy
`320`, gdzie pasmo może mieć tyle szumu, ile sygnału. Przy CBR pętla dopasowania odkręca
większość efektu w obu kierunkach; mierzalny skutek jest przy VBR, gdzie liczba bitów idzie
za wymaganiem.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo, bez przełącznika około 108 kbps):
- Łagodnie, ciaśniej: `--minsnr-clamp-hi 192`. Plik większy o 0,3%, łączny błąd niższy o
  0,06 dB.
- Mocno, ciaśniej: `--minsnr-clamp-hi 128`. Plik większy o 1,4%, łączny błąd niższy o
  0,2 dB, głównie powyżej 4 kHz.
- Przesadnie, ciaśniej: `--minsnr-clamp-hi 64`. Plik większy o 5,9%, błąd niższy o 0,5 dB:
  ciche pasma odkupione wyższym bitrate'em.
- Luźniej: `--minsnr-clamp-hi 320`. Najluźniejsza granica; na materiale testowym nie zmieniła
  niczego mierzalnego, bo tabele rzadko żądają tak mało.

Współpraca i konflikty: `--minsnr-scale` przesuwa wymaganie wewnątrz okna, a ten przełącznik
i `--minsnr-clamp-lo` przesuwają końce okna. Przy CBR różnice są rzędu setnych części
decybela.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `128` zmienia łączny błąd o 0,06 dB w złą
stronę, bo pętla dopasowania płaci za ciche pasma gdzie indziej. Wartości powyżej 320 są
odrzucane: za 0 dB wewnętrzna liczba stałoprzecinkowa się przepełniała i koder wyliczał
bezsensowne wymagania.

#### `--minsnr-clamp-lo <n>`

**Przesuwa najwyższy SNR, jakiego pasmo może zażądać: granicę, w którą uderza
`--minsnr-scale` poniżej 256.**

- Dotyczy: każdego profilu z rdzeniem AAC, gdy `--minsnr-scale` żąda więcej SNR niż FDK albo
  gdy tabele już żądają najwięcej.
- Wartości: Q8, `1` i wyżej, gdzie `256` to x1,0 (domyślnie w FDK); `-1` wyłączone
  (domyślnie)
- Wpisywanie: liczba całkowita

To dokładny koniec tego samego okna: w FDK żadne pasmo nie może żądać więcej niż około 25 dB
SNR, choćby tabele uważały je za bardzo ważne. Dzięki temu jedno pasmo nie zabiera całej
ramki.

Poniżej 256 granica odsuwa się, więc pasmo może żądać więcej niż 25 dB. Samo z siebie
zmienia to niewiele, bo do granicy dochodzi mało pasm; przydaje się razem z `--minsnr-scale`
dobrze poniżej 256, które dopycha wymagające pasma do tej granicy. Powyżej 256 granica się
przybliża, najbardziej wymagające pasma są ograniczane wcześniej, a ich bity idą do
pozostałych. Bardzo duże wartości zsuwają oba końce okna i każde pasmo żąda tego samego, a
to już nie jest model psychoakustyczny.

Przykłady (VBR `-m 3`, akustyczna muzyka stereo):
- Łagodnie, dokładniej: `--minsnr-clamp-lo 64`. Bez `--minsnr-scale` bez mierzalnej zmiany.
- Mocno, dokładniej: `--minsnr-scale 64 --minsnr-clamp-lo 64`. Dolne pasmo (100 Hz..1 kHz)
  czystsze o 0,4 dB niż z samym `--minsnr-scale 64`.
- Łagodnie, zgrubniej: `--minsnr-clamp-lo 1024`. Plik mniejszy o 0,2%, błąd w dolnym
  paśmie wyższy o 0,3 dB.
- Przesadnie, zgrubniej: `--minsnr-clamp-lo 4096`. Plik mniejszy o 0,8%, dolne pasmo gorsze o
  1,4 dB: tracą właśnie pasma, na których tabelom najbardziej zależy.

Współpraca i konflikty: partner `--minsnr-scale` poniżej 256. Przełączniki dziur z grupy E
zajmują się tym samym problemem z drugiej strony, decydując, które pasma wolno wyzerować.

Zmierzone: CBR 96 kbps, akustyczna muzyka stereo: `4096` podnosi błąd w dolnym paśmie o 1,1
dB, a łączny błąd o 0,03 dB; samo `64` nie zmienia niczego mierzalnego.

#### `--reduce-clamp <0|1>`

**Usuwa sufit 29 dB na to, jak daleko kwantyzator CBR może pchać progi w dół, pozwalając
innym przełącznikom od szczegółu działać bez uderzania w ścianę.**

- Dotyczy: CBR, czyli domyślnej ścieżki `-m 0`. VBR używa innej ścieżki kodu i nie jest tym
  dotknięte.
- Wartości: `0` albo `1`; domyślnie: `1`, sufit obowiązuje, jak w fabrycznym FDK
- Wpisywanie: liczba całkowita, włącz/wyłącz

Kiedy pętla dopasowania bitrate'u CBR potrzebuje w paśmie więcej jakości, redukuje próg tego
pasma. Na łączną redukcję jest sufit, około 29 dB, który nie pozwala pętli gonić jednego
wymagającego pasma w nieskończoność. Przy normalnym materiale sufit nigdy nie wiąże. Przy
celowym żądaniu znacznie większej precyzji niż fabryczna, przez `--minsnr-scale` dobrze
poniżej 256 albo `--ath-scale` dobrze poniżej 256, to właśnie ten sufit zatrzymuje dalszy
efekt ustawienia.

Ustawienie `0` zdejmuje sufit, więc progi mogą być pchane głębiej i do wymagających pasm
płynie więcej bitów. Samo z siebie zmienia niewiele, bo fabryczny koder rzadko dochodzi do
29 dB redukcji. To przełącznik towarzyszący, nie samodzielny.

Wartości:
- `1`: sufit redukcji 29 dB obowiązuje (ustawienie fabryczne, domyślne).
- `0`: sufit usunięty, progi mogą być pchane głębiej.

Przykład w kontekście:

    fdkaac -p 2 -b 256000 --minsnr-scale 150 --reduce-clamp 0 -o out.m4a in.wav

Współpraca i konflikty: warto łączyć w parę z `--minsnr-scale` poniżej 256 dla skrajnego
szczegółu; sam jest niemal bez sensu. Bez efektu przy VBR (`-m 1..5`).

#### `--mask-slope <dB>`

**Przesuwa, jak głęboko pod średnią energią ramki musi leżeć pasmo, zanim koder rozluźni
jego wymagany SNR.**

- Dotyczy: każdego profilu z rdzeniem AAC, obu kanałów. Domyślnie wyłączone.
- Wartości: `-24.0..50.0` dB, `0` = wyłączone (domyślnie); odmienne wyniki tylko w
  przybliżeniu w zakresie -9,9..+9
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

FDK ma heurystykę, która rozluźnia wymagany SNR pasma, gdy jego energia leży daleko pod
średnią energią ramki, w FDK więcej niż około 10 dB pod nią. Prościej: koder wydaje mniej na
bardzo ciche pasma, zakładając, że głośne je zamaskują. Ten przełącznik przesuwa tę granicę
10 dB: wartość dodatnia ją podnosi, więc mniej pasm uchodzi za pomijalne, a wartość ujemna
ją obniża, więc pomijalnych jest więcej. Mowa tu o dźwięku maskowanym, czyli o tym, co
sygnał maskujący (masker) przykrywa.

Zmierzony na muzyce efekt jest mały. Heurystyka dotyka tylko najcichszych pasm, a pętla
dopasowania rozdziela to, co się zaoszczędzi, więc przy CBR błąd po dekodowaniu zmienia się
o setne części decybela. Przy VBR plik rośnie albo maleje o ułamek procenta. To przełącznik
laboratoryjny do sprawdzenia, co ta heurystyka wnosi, a nie narzędzie do szczegółu. Do
cichego szczegółu służą `--minsnr-scale` przy VBR i przełączniki dziur z grupy E.

Przyjmowany zakres jest szerszy niż żywy. Odmienne wyniki pojawiają się mniej więcej od -9,9
do +9; przy -10 i niżej wynik przestaje się zmieniać, +10 do +40 daje jeden strumień, a 45
do 50 inny.

Przykłady:
- Łagodnie, ochrona cichych pasm: `--mask-slope 2`. Kilka cichych pasm więcej zachowuje
  wymaganie.
- Mocno, ochrona cichych pasm: `--mask-slope 6`. Przy VBR plik większy o 0,1% na muzyce
  akustycznej, łączny błąd niższy o 0,03 dB.
- Przesadnie, ochrona cichych pasm: `--mask-slope 9`. Szczyt żywego zakresu; dalej nic się
  nie zmienia.
- Łagodnie, mniej dla cichych pasm: `--mask-slope -2`. Kilka cichych pasm więcej
  rozluźnionych.
- Mocno, mniej dla cichych pasm: `--mask-slope -6`. Przy CBR w granicach 0,1 dB od ustawień
  domyślnych.
- Przesadnie, mniej dla cichych pasm: `--mask-slope -9,5`. Dół żywego zakresu.

Współpraca i konflikty: działa jednakowo na mid i side. Nie rusza progu side, który należy
do `--side-bias` i `--side-knee`.

Zmierzone: mix48 przy 40 kbps daje odmienne strumienie tylko dla około -9,9..+9. Akustyczna
muzyka stereo przy 96 kbps CBR (`+6`, `-6`) i `dense48` przy 40 kbps (`+9`, `-9`): mid i
side w granicach 0,1 dB od ustawień domyślnych. VBR `-m 3`: `+9` i `-9` zmieniają plik o
+0,3% i +0,6%, błąd mid o mniej niż 0,2 dB.

## E. Transjenty rdzenia, pre-echo i dziury w widmie

Te trzynaście przełączników sięga samego rdzenia AAC, pod SBR. Znaczą najwięcej tam, gdzie
rdzeniowi brakuje bitów: AAC-LC stereo przy 24..50 kbps z pasmem wymuszonym dobrze powyżej
tego, co wybrałby FDK (`-w 16000..20000`), oraz LD albo ELD z rozszerzonym `--core-cutoff`.
Przy wygodnych bitrate'ach większość z nich zmienia niewiele, bo problemy, którymi się
zajmują, prawie tam nie występują.

Pod presją bitową pojawiają się dwie różne awarie i warto je trzymać osobno.

**Pre-echo.** Długi blok AAC obejmuje 2048 próbek, czyli około 43 ms przy 48 kHz. Szum
kwantyzacji bloku rozkłada się na cały blok w czasie. Kiedy ostry atak siedzi późno w bloku,
ten szum trafia też przed atakiem, gdzie ucho ma prawie nic, co by go maskowało: maskowanie
wstecz trwa tylko kilka milisekund. Słychać to jako delikatne rozmycie albo "cień" przed
kastanietami, klaśnięciem, szarpniętą struną czy uderzeniem fortepianu. Koder ma przeciw
temu cztery narzędzia i ta grupa udostępnia wszystkie cztery:

1. detektor ataku, który przełącza ramkę na osiem bloków krótkich po 256 próbek
   (`--attack-decay`, `--attack-floor`, razem z `--block-bias` z grupy F);
2. grupowanie tych ośmiu krótkich okien w grupy współczynników skali (`--short-groups`);
3. kształt okna, który decyduje, ile szumu wycieka z miejsca, gdzie powinien być
   (`--window-shape`);
4. dla przypadków zostających w blokach długich, TNS (temporal noise shaping, które
   układa szum w czasie wewnątrz bloku: `--tns-thresh`, `--tns-start`) oraz kontrola
   pre-echa, która ogranicza, jak szybko próg maskowania może rosnąć z ramki na ramkę
   (`--preecho-rise`, `--preecho-depth`).

AAC-LD i AAC-ELD nie mają bloków krótkich i mają ustalone okno, więc dla nich zostaje tylko
TNS i kontrola pre-echa. Dlatego te przełączniki znaczą tam więcej niż w LC.

**Dziury w widmie.** Kiedy ramki nie stać na wszystkie pasma, które ma zakodować, FDK
najpierw rozluźnia SNR, jakiego żąda od pasm będących szczytami albo dolinami widma, a jeśli
to nadal nie wystarcza, ścina całe pasma współczynnika skali (scale factor band, SFB) do
zera. Wyzerowane pasmo to cisza w tym zakresie częstotliwości w tej ramce. W czasie brzmi to jak bulgoczące, migające
albo "podwodne" góry: pasma przychodzą i odchodzą z ramki na ramkę. FDK nazywa cały ten
mechanizm "avoid holes", co jest trochę ironiczne: to on decyduje, które dziury zrobić.
Kolejność jest ustalona: pasma są ścinane w maksymalnie ośmiu krokach energetycznych, od
najcichszych, i tylko od pasma początkowego w górę, żeby dół widma był chroniony.
Przełączniki `--avoid-holes`, `--hole-start`, `--hole-start-short`, `--hole-spread` i
`--hole-ladder` zmieniają ten mechanizm.

W żadnym z tych przypadków nie ma nic za darmo. Przy CBR budżet ramki jest ustalony. Pasmo,
którego nie ścięto, kosztuje bity, których nie dostanie inne pasmo, a blok krótki kosztuje
więcej informacji pobocznej niż długi. Każdy przełącznik tutaj przesuwa bity; żaden ich nie
dodaje.

Wszystkie pomiary cytowane w tej grupie zrobiliśmy po dekodowaniu, na dwóch sygnałach
syntetycznych zbudowanych do tego celu. `attacks48.wav` zawiera kliknięcia przypominające
kastaniety na bardzo cichym tle; pre-echo to energia błędu w 1024 próbkach przed każdym
atakiem, względem energii samego ataku. `dense48.wav` zawiera gęste akordy szerokopasmowe,
jasny szum i hi-hat; dziurą jest pasmo 1/3 oktawy między 2 a 16 kHz, którego energia
oryginalna jest powyżej -70 dBFS i które wychodzi z dekodera co najmniej 15 dB za cicho.
Narzędzia, które produkują te liczby, są w `tools/core/` w repozytorium, więc pomiary można
powtórzyć na dowolnym materiale.

Punkt wyjścia dla AAC-LC stereo przy 24..50 kbps z szerokim pasmem:

    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

### `--hole-ladder <1..8>`

**Ogranicza, jak daleko koder może pójść, ścinając pasma do zera, tak by „zagłodzona” ramka
dostawała grubszą kwantyzację zamiast większej liczby pustych pasm.**

- Dotyczy: rdzenia AAC w każdym profilu (LC, rdzeń HE-AAC i HE-AAC v2, LD, ELD). Znaczy coś
  tylko wtedy, gdy kończą się bity.
- Wartości: 1..8; domyślnie: wyłączone, czyli zachowanie fabryczne (8 kroków)
- Wpisywanie: liczba całkowita

Ramka „zagłodzona” to taka, której brakuje bitów, więc koder zeruje słabe pasma albo koduje
je najgrubszym krokiem.

Kiedy ramka przekracza budżet, FDK sortuje pasma kandydujące według energii na osiem kroków
i ścina je krok po kroku, od najcichszych, sprawdzając budżet po każdym kroku. Przy ośmiu
krokach trudna ramka może skończyć na ścinaniu całkiem głośnych pasm. `--hole-ladder N`
zatrzymuje się po N krokach. Wtedy wolno ścinać tylko najcichsze pasma. Jeśli ramka nadal
przekracza budżet, pętla dopasowania bitrate'u rozwiązuje resztę inaczej, podnosząc krok
kwantyzacji we wszystkich pasmach: wynikiem jest więcej szumu rozłożonego równomiernie
zamiast większej liczby brakujących pasm.

To najmocniejszy przełącznik przeciw bulgoczącej górze przy niskim bitrate. Wymiana jest
realna, ale zwykle korzystna: nieco bardziej ziarniste wypełnienie w zamian za pasma, które
zostają obecne z ramki na ramkę. Ucho jest znacznie bardziej tolerancyjne na stały szum niż
na pasma, które mrugają.

Przykłady (istnieje tylko jeden kierunek: mniej kroków niż fabrycznie):
- Łagodnie: `--hole-ladder 6`. Znikają tylko najgłośniejsze kroki ścinania. Trudne ramki
  tracą kilka pasm mniej; łatwe ramki są nietknięte.
- Mocno: `--hole-ladder 2`. Ścinać wolno tylko dwa najcichsze kroki. Na gęstym materiale
  przy 32..50 kbps usuwa to większość dziur (patrz Zmierzone).
- Przesadnie: `--hole-ladder 1`. Ścinać wolno tylko najcichszy krok, więc prawie każde pasmo
  zostaje, a cały problem budżetowy zamienia się w grubszą kwantyzację. Na bardzo
  gęstym materiale przy 24 kbps może to brzmieć jednolicie chropowato: ustawienie skrajne
  dla tej wymiany.

Współpraca i konflikty: dobrze łączy się z `--avoid-holes 1` i `--hole-start`. Pętla
dopasowania bitrate'u może podnieść krok tylko do pewnej granicy; przy skrajnych budżetach
pasma i tak znikają, bo same kwantyzują się do zera, czemu żaden przełącznik tutaj nie
zapobiega.

Zmierzone (dense48, LC stereo, dziury po dekodowaniu): 32 kbps / 17 kHz: z 37% do 8% przy
`2`; 40 kbps / 18 kHz: z 37% do 1,7%; 50 kbps / 19 kHz: z 35% do 0,2%. Stosunek błędu do
sygnału w pasmach, które zostają, pogarsza się o 0,3..0,5 dB.

Pod maską: ograniczenie `enIdx` w pętli "allow more holes" w `adj_thr.cpp`.

### `--avoid-holes <0|1>`

**Wymusza włączenie albo wyłączenie ochrony szczytów i dolin w FDK.**

- Dotyczy: rdzenia AAC w każdym profilu.
- Wartości: `0` wyłączone, `1` włączone; domyślnie: wyłączone, czyli zachowanie fabryczne
  (włączona od 20 kbps na kanał, wyłączona poniżej)
- Wpisywanie: liczba całkowita, 0 albo 1

Przed ścinaniem pasm FDK może rozluźnić SNR, jakiego żąda, w mądrzejszy sposób: obniża
żądanie w dolinach między szczytami widma, a utrzymuje je w szczytach, tak by wąskie głośne
pasma zostały, a oszczędności przyszły stamtąd, gdzie ucho najmniej je zauważa. Fabryczny
FDK włącza to dopiero od 20 kbps na kanał, co dla stereo oznacza od 40 kbps. Poniżej,
dokładnie w zakresie 24..50 kbps, w który ta grupa jest wycelowana, ochrona jest wyłączona i
ścinanie zaczyna się wcześniej.

Wartości:
- `1`: ochrona włączona przy każdym bitrate. Poniżej 40 kbps stereo to jest sensowne
  ustawienie; na tej wartości i powyżej nic się nie zmienia, bo domyślnie jest już włączona.
- `0`: ochrona wyłączona przy każdym bitrate. Ustawienie diagnostyczne: pokazuje, co ta
  ochrona dawała.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --avoid-holes 1 --hole-ladder 2`.

Współpraca i konflikty: sam z siebie daje mały efekt (zmierzone), bo ochrona tylko
przestawia, skąd biorą się oszczędności; najbardziej pomaga razem z `--hole-ladder`.

Zmierzone: przy LC 40 i 128 kbps stereo `--avoid-holes 1` daje strumień identyczny z
fabrycznym (tam już jest włączona), a `--avoid-holes 0` go zmienia.

### `--hole-start <sfb>`

**Chroni dolną część widma w blokach długich przed ścięciem do zera.**

- Dotyczy: rdzenia AAC, bloków długich (LC, rdzeń HE-AAC, LD, ELD).
- Wartości: pasmo SFB 0..51; domyślnie: wyłączone, czyli zachowanie fabryczne
  (pasmo 15, gdy ochrona jest włączona, pasmo 0, gdy jest wyłączona)
- Wpisywanie: liczba całkowita

Pasma poniżej tej liczby nigdy nie są wybierane jako dziury. Fabrycznie start wypada na
paśmie 15, co przy 48 kHz jest z grubsza 2 do 3 kHz: bas i dolna średnica są bezpieczne,
wszystko powyżej wolno ściąć. Podniesienie liczby przesuwa chroniony obszar w górę;
obniżenie dopuszcza dziury głębiej. Warto pamiętać, że chronione pasmo nadal kosztuje bity:
oszczędności muszą wtedy przyjść z pasm powyżej albo z grubszej kwantyzacji wszędzie.

Ile pasm istnieje, zależy od profilu i częstotliwości próbkowania; `--verbose` wypisuje tę
liczbę jako `active SFBs (long)`, 49 dla LC stereo przy 48 kHz.

Przykłady, chronić więcej:
- Łagodnie: `--hole-start 20`. Kilka pasm więcej w dolnej średnicy jest bezpiecznych.
- Mocno: `--hole-start 25`. Ochrona z grubsza do obszaru obecności; zmierzone jako
  zmniejszenie dziur z 37% do 26% przy 32 kbps.
- Przesadnie: `--hole-start 51`. Żadnego pasma w bloku długim nie wolno temu mechanizmowi
  ściąć. Cały problem budżetowy spada na kwantyzator; przy niskim bitrate góra robi się
  bardzo zgrubna, a pasma i tak mogą zniknąć, kwantyzując się do zera.

Przykłady, dopuszczać głębsze dziury:
- Łagodnie: `--hole-start 12`. Trochę więcej swobody zaraz poniżej startu fabrycznego.
- Mocno: `--hole-start 6`. Dziury dopuszczone w dół, w dolną średnicę.
- Przesadnie: `--hole-start 0`. Nawet pasma basu wolno ściąć. Przydatne tylko do usłyszenia,
  czemu zapobiega ochrona fabryczna.

Bloki krótkie: bez `--hole-start-short` używana jest tam ta sama częstotliwość (najbliższa
krawędź pasma krótkiego; `--hole-start 25`, czyli 5 kHz przy 48 kHz, staje się pasmem
krótkim 6, a para fabryczna 15/3 przechodzi sama na siebie). `--verbose` to pokazuje.

Współpraca i konflikty: `--hole-start-short` ustawia wartość krótką jawnie i wygrywa.

### `--hole-start-short <sfb>`

**Ta sama ochrona dla bloków krótkich.**

- Dotyczy: LC i rdzenia HE-AAC, ramek kodowanych jako bloki krótkie. LD i ELD nie mają
  żadnych.
- Wartości: pasmo SFB bloku krótkiego 0..15; domyślnie: wyłączone, czyli
  zachowanie fabryczne (pasmo 3) albo częstotliwość z `--hole-start`, gdy ten jest podany
- Wpisywanie: liczba całkowita

Blok krótki ma około 14 pasm przy 44,1 albo 48 kHz, więc każde pasmo jest szerokie, a liczby
są małe. Fabrycznie chronione są pasma 0..2.

Przykłady, chronić więcej:
- Łagodnie: `--hole-start-short 5`.
- Mocno: `--hole-start-short 8`. Większość pasma w ramce transjentowej jest chroniona.
- Przesadnie: `--hole-start-short 15`. Żadnego krótkiego pasma nie wolno ściąć; ramki
  transjentowe stają się zamiast tego zgrubne.

Przykłady, dopuszczać głębsze dziury:
- Łagodnie: `--hole-start-short 2`.
- Mocno: `--hole-start-short 1`.
- Przesadnie: `--hole-start-short 0`. Ramka ataku może stracić swój bas.

### `--hole-spread <dB>`

**Decyduje, jak chętnie pasmo liczy się jako przykryte przez sąsiadów, a zatem jako
bezpieczny kandydat na dziurę.**

- Dotyczy: rdzenia AAC w każdym profilu.
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (3 dB dla bloków
  długich, 2 dB dla bloków krótkich)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Pasmo można ściąć bezpieczniej, kiedy sąsiedzi i tak je maskują. Żeby to ocenić, FDK
porównuje energię pasma z energią rozmytą do niego z pasm sąsiednich, po obniżeniu tej
rozmytej energii o margines. Mały margines sprawia, że sąsiedzi wyglądają mocno, więc wiele
pasm wygląda na przykryte i staje się kandydatami. Duży margines sprawia, że sąsiedzi
wyglądają słabo, więc mniej pasm wygląda na przykryte i więcej z nich jest chronionych.

To delikatne pokrętło. Na materiale testowym przesunęło liczbę dziur o jeden lub dwa procent
pasm, znacznie mniej niż `--hole-ladder`.

Przykłady, chronić więcej:
- Łagodnie: `--hole-spread 4,5`.
- Mocno: `--hole-spread 9`.
- Przesadnie: `--hole-spread 24`. Sąsiedzi praktycznie nigdy nie liczą się jako
  przykrywający, więc prawie żadne pasmo nie kwalifikuje się z tego powodu.

Przykłady, dopuszczać więcej dziur:
- Łagodnie: `--hole-spread 2`.
- Mocno: `--hole-spread 1`.
- Przesadnie: `--hole-spread 0`. Rozmyta energia jest brana za dobrą monetę; każde pasmo
  stojące obok głośniejszych jest kandydatem.

Wartość dotyczy bloków długich; bloki krótkie dostają dwie trzecie, co zachowuje proporcję
fabryczną (3 dB długie, 2 dB krótkie). `--hole-spread 3` to więc dokładnie ustawienie
domyślne w obu. Do 1.4.0-beta.1 jedna wartość zastępowała obie.

### `--preecho-rise <dB>`

**Ogranicza, jak szybko próg maskowania pasma może rosnąć z ramki na ramkę, tak by szum
kwantyzacji nie mógł wybiegać przed atak w bloku długim.**

- Dotyczy: bloków długich w każdym profilu. Najistotniejszy w LD i ELD oraz w LC z
  wymuszonymi blokami długimi (`--block-bias 0`).
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (3,01 dB, czyli
  dwukrotność w mocy)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Model psychoakustyczny wylicza próg maskowania dla pasma i ramki. W ramce zawierającej atak
próg skacze w górę, bo atak maskuje dużo. Ale szum tej ramki siedzi także przed atakiem,
gdzie nic go nie maskuje. Kontrola pre-echa ogranicza więc wzrost: próg tej ramki może być
najwyżej N dB powyżej progu ramki poprzedniej. Poniżej 3,01 ograniczenie jest ciaśniejsze
(mniej pre-echa, więcej bitów na nagłosy); powyżej jest luźniejsze (mniej bitów, więcej
pre-echa).

W LC efekt jest zwykle mały, bo prawdziwe ataki są już przenoszone do bloków krótkich, zanim
to ograniczenie zaczyna mieć znaczenie. LD i ELD nie mają bloków krótkich, więc tam ten
przełącznik i TNS są główną obroną.

Przykłady, ciaśniej:
- Łagodnie: `--preecho-rise 2`.
- Mocno: `--preecho-rise 1`. Nagłosy w blokach długich dostają zauważalnie więcej bitów.
- Przesadnie: `--preecho-rise 0`. Próg nie może w ogóle rosnąć z ramki na ramkę. Każde
  crescendo jest kodowane tak, jakby nadal było cicho, co marnuje bity na głośnych
  fragmentach i „głodzi” wszystko inne przy CBR.

Przykłady, luźniej:
- Łagodnie: `--preecho-rise 4,5`.
- Mocno: `--preecho-rise 9`.
- Przesadnie: `--preecho-rise 24`. Ograniczenia praktycznie nie ma: kontrola pre-echa jest
  wyłączona.

Współpraca i konflikty: `--preecho-depth` ogranicza przeciwną stronę tej samej reguły.

Zmierzone: dziesiąte części dB energii pre-echa w LC na attacks48 (ataki i tak idą do bloków
krótkich). Przełącznik zmienia strumienie LD i ELD.

### `--preecho-depth <dB>`

**Ogranicza, jak nisko kontrola pre-echa może najwyżej obniżyć próg.**

- Dotyczy: tego samego co `--preecho-rise`.
- Wartości: 0.0..60.0 dB (efekt przestaje rosnąć około 45 dB); domyślnie: wyłączone, czyli
  zachowanie fabryczne (20 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy ograniczenie wzrostu ciągnie próg w dół, ten przełącznik ustawia dolną granicę: próg
nie może skończyć niżej niż N dB poniżej tego, co model psychoakustyczny wyliczył dla ramki.
Nie pozwala ograniczeniu żądać absurdalnej precyzji po długiej ciszy, kiedy próg poprzedniej
ramki był znikomy.

Przykłady, dopuszczać głębsze obniżanie:
- Łagodnie: `--preecho-depth 25`.
- Mocno: `--preecho-depth 40`. Po ciszy pierwszy atak jest kodowany bardzo czysto i bardzo
  drogo.
- Przesadnie: `--preecho-depth 60`. Praktycznie brak dolnej granicy; przy CBR jeden nagłos po
  pauzie może zjeść rezerwuar. Wewnętrzny współczynnik osiąga dno około 45 dB, więc wartości
  między około 45 a 60 zachowują się tak samo.

Przykłady, płycej:
- Łagodnie: `--preecho-depth 15`.
- Mocno: `--preecho-depth 6`.
- Przesadnie: `--preecho-depth 0`. Progu nie wolno w ogóle obniżyć, co wyłącza kontrolę
  pre-echa niezależnie od `--preecho-rise`.

### `--attack-decay <x>`

**Ustawia pamięć detektora ataku, który wybiera między blokami długimi a krótkimi.**

- Dotyczy: LC i rdzenia HE-AAC (profili z blokami krótkimi). W naszym teście nie miał też
  efektu na LD ani ELD.
- Wartości: 0.0..0.99; domyślnie: wyłączone, czyli zachowanie fabryczne (0,70)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Detektor dzieli każdą ramkę na podokna, odfiltrowuje niskie częstotliwości i porównuje
energię każdego podokna z bieżącą średnią tego, co było wcześniej. Dostatecznie duży
stosunek to atak. Bieżąca średnia zachowuje część swojej wartości na podokno: ta część to
właśnie ten przełącznik. Wyższe wartości oznaczają długą pamięć, więc atak jest porównywany
z długą średnią; niższe oznaczają porównanie z ostatnią chwilą.

Długa pamięć jest spokojniejsza i bardziej selektywna na gęstym materiale, ale po głośnym
fragmencie średnia zostaje wysoko i atak, który po nim następuje, może zostać przeoczony.
Krótka pamięć łapie ataki zaraz po głośnych fragmentach, ale na ruchliwym materiale może
odpalać zbyt często i wrzucać ramki tonalne do bloków krótkich, co kosztuje bity i
rozdzielczość.

Przykłady, dłuższa pamięć:
- Łagodnie: `--attack-decay 0,8`.
- Mocno: `--attack-decay 0.9`. Zmierzone: pre-echo w najgorszym przypadku niższe o 2,9 dB na
  attacks48.
- Przesadnie: `--attack-decay 0.99`. Średnia prawie się nie rusza; ataki po głośnych
  fragmentach są często przeoczane.

Przykłady, krótsza pamięć:
- Łagodnie: `--attack-decay 0,6`.
- Mocno: `--attack-decay 0.4`.
- Przesadnie: `--attack-decay 0`. Każde podokno jest porównywane tylko z poprzednim;
  detektor robi się nerwowy na materiale szumowym.

### `--attack-floor <dB>`

**Przesuwa minimalną energię, jaką atak musi mieć, zanim się zaliczy.**

- Dotyczy: LC i rdzenia HE-AAC. W naszym teście zmienił też strumień LD, ale nie ELD.
- Wartości: -60.0..30.0 dB; domyślnie: wyłączone, czyli zachowanie fabryczne (przesunięcie 0 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Oprócz stosunku detektor wymaga bezwzględnego minimum energii, żeby stuknięcia w niemal
ciszy nie wyzwalały bloków krótkich. Wartości ujemne obniżają ten najniższy dozwolony
poziom: ciche ataki, takie jak miotełki, miękkie hi-haty czy ciche szarpnięcie gitary, też
przełączają na bloki krótkie. Wartości dodatnie go podnoszą: liczą się tylko głośne ataki.

Przykłady, łapanie cichszych ataków:
- Łagodnie: `--attack-floor -6`.
- Mocno: `--attack-floor -20`.
- Przesadnie: `--attack-floor -60`. Praktycznie każdy wzrost w cichym fragmencie się zalicza.
  Bloki krótkie pojawiają się wtedy na szumie pomieszczenia i na miękkich fragmentach
  tonalnych, wraz z kosztem bitowym, który to niesie.

Przykłady, tylko głośne ataki:
- Łagodnie: `--attack-floor 6`.
- Mocno: `--attack-floor 15`.
- Przesadnie: `--attack-floor 30`. Miękka perkusja zostaje w blokach długich i pojawia się
  na niej pre-echo.

### `--window-shape <0|1>`

**Wybiera kształt okna każdego bloku: sinusowe wszędzie albo KBD wszędzie.**

- Dotyczy: AAC-LC oraz rdzenia LC w HE-AAC i HE-AAC v2. LD i ELD używają ustalonego okna i
  nie są tym objęte.
- Wartości: `0` sinusowe, `1` KBD; domyślnie: wyłączone, czyli zachowanie fabryczne (KBD dla
  bloków długich, sinusowe dla bloków krótkich)
- Wpisywanie: liczba całkowita, 0 albo 1

Okno decyduje o tym, jak energia bloku wycieka w sąsiednie częstotliwości. Okno sinusowe ma
wąski listek główny (dobra selektywność obok szczytu) i wolno opadające listki boczne
(energia i szum wyciekają daleko). Okno KBD ma szerszy listek główny i listki boczne, które
opadają znacznie szybciej. W czasie ta sama właściwość oznacza, że KBD trzyma szum
kwantyzacji bliżej miejsca, gdzie powinien być. Fabryczny FDK używa okna sinusowego dla
bloków krótkich, czyli dokładnie dla bloków wokół ataków.

Ten wybór kosztuje jeden bit na ramkę i jest legalny wszędzie w LC; strumień niesie kształt
faktycznie użyty, także w blokach krótkich, i każdy dekoder go stosuje.

Wartości:
- `1`: KBD w każdym bloku. Zmierzone: pre-echo niższe o 3,4..6,1 dB średnio i o 1,2..6,6 dB
  w najgorszym przypadku przy 24, 40 i 128 kbps. Najlepszy pojedynczy przełącznik przeciw
  pre-echu w LC.
- `0`: sinusowe w każdym bloku. Lepsza selektywność obok mocnych szczytów tonalnych w blokach
  długich, więcej wycieku; głównie ustawienie do porównania.

Przykład w kontekście: `-p 2 -b 40000 -w 18000 --window-shape 1`.

### `--short-groups <0..3>`

**Wybiera, jak grupowanych jest osiem krótkich okien ramki transjentowej.**

- Dotyczy: LC i rdzenia HE-AAC, ramek kodowanych jako bloki krótkie.
- Wartości: `0`..`3`; domyślnie: wyłączone, czyli zachowanie fabryczne (tabela, którą FDK
  wybiera według pozycji ataku)
- Wpisywanie: liczba całkowita

Ramka transjentowa składa się z ośmiu krótkich okien. Okna w tej samej grupie dzielą jeden
zestaw współczynników skali, więc mniej grup oznacza mniej informacji pobocznej i więcej
bitów na widmo, ale grubszą kontrolę szumu w czasie. Więcej grup oznacza, że szum można
kształtować okno po oknie, za odpowiednią cenę.

Wartości:
- `0`: tabela fabryczna (to samo co pozostawienie przełącznika wyłączonego).
- `1`: jedna grupa na wszystkie osiem okien. Najtaniej; szum rozkłada się na całą ramkę, co
  częściowo odkręca to, po co były bloki krótkie.
- `2`: cztery pary. Równa rozdzielczość czasowa w całej ramce. Zmierzone: najmniej dziur w
  ramkach krótkich na materiale testowym.
- `3`: ciasno wokół ataku: samo okno ataku, po jednym oknie z każdej jego strony, reszta
  zbita razem. Najwięcej bitów tam, gdzie jest atak.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --short-groups 2`.

### `--tns-thresh <x>`

**Zmienia, jak chętnie włącza się TNS.**

- Dotyczy: każdego profilu z rdzeniem: LC, rdzenia HE-AAC, LD, ELD.
- Wartości: 0.5..4.0, mnożnik; domyślnie: wyłączone, czyli zachowanie fabryczne (1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

TNS (kształtowanie szumu w czasie) prowadzi filtr predykcyjny w poprzek częstotliwości. Tam,
gdzie sygnał ma mocną obwiednię czasową, na przykład atak wewnątrz bloku, filtr sprawia, że
szum kwantyzacji idzie za tą obwiednią w czasie, więc szum siedzi pod głośną częścią zamiast
przed nią. FDK włącza TNS w ramce tylko wtedy, gdy zysk predykcji jest dość wysoki, żeby
opłacić informację poboczną. Ten przełącznik mnoży wymagany zysk. Poniżej 1 TNS jest używany
w większej liczbie ramek; powyżej 1 w mniejszej.

W LC bloki krótkie obsługują już najostrzejsze ataki, więc TNS zajmuje się tymi
łagodniejszymi. W LD i ELD, które nie mają bloków krótkich, TNS jest głównym narzędziem
przeciw pre-echu, a niższy próg warto tam często wypróbować w pierwszej kolejności.

Przykłady, więcej TNS:
- Łagodnie: `--tns-thresh 0,85`.
- Mocno: `--tns-thresh 0.7`. Zmierzone: w LC przy niskim bitrate dodatkowa informacja
  poboczna zostawia więcej dziur.
- Przesadnie: `--tns-thresh 0.5`. TNS włączony prawie wszędzie, także tam, gdzie nie ma czego
  kształtować; bity idą na współczynniki filtru.

Przykłady, mniej TNS:
- Łagodnie: `--tns-thresh 1,3`.
- Mocno: `--tns-thresh 2`. Mniej bitów na informację poboczną, nieco mniej dziur, więcej
  pre-echa na miękkich atakach.
- Przesadnie: `--tns-thresh 4`. TNS praktycznie wyłączony.

Współpraca i konflikty: `--tns-mask` i `--tns-order` (grupa D) decydują, czy i jak duży może
być filtr; ten przełącznik decyduje, jak często się to opłaca.

### `--tns-start <Hz>`

**Ustawia najniższą częstotliwość, którą obejmuje dolny filtr TNS.**

- Dotyczy: AAC-LC i rdzenia LC w HE-AAC, bloków długich i krótkich. Zmierzony brak efektu
  w LD i ELD.
- Wartości: 0..24000 Hz; domyślnie: wyłączone, czyli zachowanie fabryczne (8 pasm SFB,
  około 1,4 kHz przy 48 kHz)
- Wpisywanie: liczba całkowita

Fabrycznie najniższe pasma zostają poza TNS w blokach długich, a w krótkich zakres zaczyna
się od 0 Hz. Wartość jest wyrównywana do najbliższej krawędzi pasma w każdej z tablic i nie
może przekroczyć górnej granicy zakresu TNS. Do 1.4.0-beta.1 bloki krótkie ją pomijały.

Przykłady, sięganie niżej:
- Łagodnie: `--tns-start 1000`.
- Mocno: `--tns-start 400`. TNS kształtuje też szum dolnej średnicy, co przydaje się na
  fortepianie i szarpanych dźwiękach basu.
- Przesadnie: `--tns-start 0`. TNS obejmuje widmo od samego dołu, łącznie z pasmami basu,
  gdzie jego korzyść uboczna jest mała, a jego błędy mogą objawiać się jako
  niskoczęstotliwościowe "oddychanie".

Przykłady, start wyżej:
- Łagodnie: `--tns-start 2000`.
- Mocno: `--tns-start 5000`.
- Przesadnie: `--tns-start 15000`. Kształtowana jest tylko sama góra; dla większości materiału
  TNS jest wtedy w blokach długich praktycznie wyłączony.

### `--coverage <-5..5>`

**Jedna oś przy stałym bitrate: koduj każde pasmo grubo albo mniej pasm czysto.**

- Dotyczy: rdzeń AAC (LC, LD, ELD i rdzeń LC w HE-AAC). Domyślnie wyłączone.
- Wartości: liczby całkowite `-5..5`; `0` = fabryczne
- Wpisywanie: liczba całkowita

Przy stałym bitrate koder może wydać bity na pokrycie całego widma grubym opisem albo na
dobry opis mniejszej liczby pasm, zostawiając dziury. To makro przesuwa tę równowagę jedną
liczbą. Ustawia sześć przełączników poniżej (`--band-knee`, `--quant-round`, `--thr-hold`,
`--scf-range`, a przy `+4` i `+5` także `--pns-above`); każdy z nich podany jawnie wygrywa
z makrem. `--verbose` pokazuje wartości, które faktycznie działają.

Zmierzone przy LC 32 kbps, 17 kHz, po dekodowaniu (wyzerowane pasma i szum pasm, które
zostały): `-5` 51% dziur, a zachowane pasma 4,75 dB czystsze od sygnału; fabrycznie 11,7%
dziur, 1,96 dB; `+4` 0,2% dziur, a zachowane pasma mniej więcej tak szumne jak treść.

Przykłady:
- Łagodnie, więcej pokrycia: `--coverage 2`. Mniej dziur, trochę więcej ziarna.
- Mocno, więcej pokrycia: `--coverage 4`. Praktycznie bez dziur; dokłada podstawianie szumu od 11 kHz.
- Przesadnie, więcej pokrycia: `--coverage 5`. Wszystko wypełnione, szum od 7 kHz.
- Łagodnie, więcej precyzji: `--coverage -2`. Najpierw odchodzą najsłabsze pasma.
- Mocno, więcej precyzji: `--coverage -4`. Dużo dziur, to, co zostaje, jest czyste.
- Przesadnie, więcej precyzji: `--coverage -5`. Znika mniej więcej połowa pasm.

### `--band-knee <dB>`

**Przesuwa zapas każdego pasma nad progiem przy krawędzi kodowane/wyzerowane, we wszystkich kanałach.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone.
- Wartości: `-24.0..24.0` dB, `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Wartość dodatnia zatrzymuje pasma do N dB pod progiem, na najgrubszym kroku, zamiast je
zerować. Wartość ujemna zeruje pasma, które przechodzą próg o mniej niż N dB po pierwszym
oszacowaniu bitów, więc drugie oszacowanie oddaje te bity reszcie. Pasm PNS i IS nie rusza;
w pasmach side pary MS wygrywa `--side-knee`.

Przykłady:
- Łagodnie, więcej zatrzymanych: `--band-knee 3`.
- Mocno, więcej zatrzymanych: `--band-knee 6`. Użyteczne maksimum: powyżej około 9 dB uratowane
  grube pasma kosztują tyle bitów, że pętla tnie gdzie indziej.
- Przesadnie, więcej zatrzymanych: `--band-knee 24`. Zmierzone: gorzej niż 6.
- Łagodnie, więcej wyrzuconych: `--band-knee -2`.
- Mocno, więcej wyrzuconych: `--band-knee -6`.
- Przesadnie, więcej wyrzuconych: `--band-knee -24`. Zostają tylko wyraźnie słyszalne pasma.

### `--quant-round <x>`

**Przesuwa punkt zaokrąglania kwantyzatora: mniej albo więcej małych linii widma staje się zerem.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (fabrycznie 0,405).
- Wartości: `0.0..0.5`
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kwantyzator zaokrągla każdą linię widma do całego kroku. Fabrycznie zaokrągla z
przesunięciem około 0,405, czyli lekko w stronę zera, co oszczędza bity. `0.5` zaokrągla do
najbliższej: mniej małych linii znika, kosztuje to bity. Niższe wartości wysyłają więcej
linii do zera i są tańsze.

Przykłady:
- Łagodnie, dokładniej: `--quant-round 0.45`.
- Mocno, dokładniej: `--quant-round 0.5`. Zaokrąglanie do najbliższej.
- Łagodnie, taniej: `--quant-round 0.35`.
- Mocno, taniej: `--quant-round 0.25`. Ciche szczegóły znikają.
- Przesadnie, taniej: `--quant-round 0.0`. Obcinanie: każda linia zaokrąglona w dół.

### `--pns-above <Hz>`

**Od tej częstotliwości w górę zastępuje każde pasmo bloku długiego szumem o właściwym poziomie, bez względu na tonalność.**

- Dotyczy: AAC-LC, bloki długie. Domyślnie wyłączone.
- Wartości: `1000..48000` Hz
- Wpisywanie: liczba całkowita

Górne pasmo „jak SBR, bez SBR”: pasmo zachowuje poziom, a jego treść staje się szumem.
Włącza PNS nawet poniżej jego warunku bitrate'u; `--pns 0` nadal wygrywa. Ciche pasma
zostają ciche. Przy niskim bitrate należy używać razem z `--pns-budget 1` i
`--pns-shared 1`, inaczej ramkom może zabraknąć bitów i pojawi się trzask (patrz
`--pns-budget`).

Przykłady:
- Łagodnie: `--pns-above 11000`. Szumem staje się tylko sama góra.
- Mocno: `--pns-above 5000`.
- Przesadnie: `--pns-above 2200`. Większość obszaru prezencji to szum.

### `--thr-hold <n>`

**Mierzy model maskowania tylko co n ramek długich.**

- Dotyczy: rdzeń AAC, bloki długie. Domyślnie wyłączone.
- Wartości: `1..32` ramki, `1` = wyłączone
- Wpisywanie: liczba całkowita

Pomiędzy pomiarami każde pasmo zachowuje ostatni stosunek szumu do sygnału, więc szum
spóźnia się za muzyką. Blok krótki zeruje to trzymanie. Efekt jest słaby: strumień się
zmienia, ale zmierzona jakość rusza się o mniej niż 1 dB nawet przy 32, bo FDK i tak pracuje
na wygładzonych energiach.

Przykłady:
- Łagodnie: `--thr-hold 2`.
- Mocno: `--thr-hold 8`.
- Przesadnie: `--thr-hold 32`.

### `--scf-range <n>`

**Ogranicza, jak daleko od siebie mogą być najdrobniej i najgrubiej kodowane pasmo kanału.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (fabrycznie 60).
- Wartości: `0..60`, kroki po 1,5 dB
- Wpisywanie: liczba całkowita

Niższe wartości robią szum kwantyzacji bardziej płaskim w częstotliwości, z mniejszym
kształtowaniem psychoakustycznym. Na zwykłym materiale efekt jest bliski zera; widać go przy
małych wartościach na materiale z silnym nachyleniem widma (szum 6 do 14 kHz z 12,4 do 7,1
dB pod sygnałem przy `0`, 64 kbps bez PNS). Przy 14 kbps nie zmienia budżetu.

Przykłady:
- Łagodnie: `--scf-range 40`.
- Mocno: `--scf-range 20`.
- Przesadnie: `--scf-range 0`. Jeden krok na cały kanał.

### `--bit-stats`

**Liczy, na co idą bity zapisanych ramek, i wypisuje tabelę na końcu.**

- Dotyczy: każdy profil. Domyślnie wyłączone.
- Wartości: brak, to flaga

Tabela dzieli ramki na nagłówek transportu, nagłówki elementów, maskę MS, dane sekcji,
współczynniki skali, poziomy szumu PNS, TNS, widmo każdego kanału (mid i side dla MS),
SBR/PS i wypełnienie, a do tego kodowane, wyzerowane, PNS i IS pasma na ramkę. Liczy bity
naprawdę zapisane; strumień jest identyczny z przełącznikiem i bez niego, a suma zgadza się
z plikiem ADTS.

Przykład: `fdkaac -p 2 -b 14000 --bit-stats -f 2 -o out.aac in.wav`

### `--enfac-patch <0|1>`

**Wyłącza przeważanie energii płaskich, szumowych ramek długich albo włącza je dla każdej ramki.**

- Dotyczy: rdzeń AAC, bloki długie, CBR i VBR. Domyślnie wyłączone (decyduje fabryczny FDK).
- Wartości: `0` = nigdy, `1` = każda ramka długa

Gdy słyszalne widmo ramki jest płaskie, fabryczny FDK przeważa energie pasm (potęgi 3/4, 1/2
albo 1/4) i w ten sposób przesuwa bity między pasmami. `0` traktuje każdą ramkę jak tonalną,
`1` stosuje przeważanie do każdej ramki długiej. W CBR to drobne pokrętło: wyzerowane pasma
zmieniają się najwyżej o 1 punkt, a szum o mniej niż 0,1 dB przy 14 do 32 kbps; `0` na
materiale tonalnym często daje to samo co ustawienie domyślne. W VBR (`-m 3`) `1`
powiększyło plik z tonem i szumem o 29%, a `0` zmniejszyło plik z gęstym szumem o 3%.

## F. Przełączanie bloków i kontrola bitrate'u

Sześć przełączników: jeden, który przechyla decyzję o bloku długim albo krótkim, i pięć,
które kontrolują, jak swobodnie bitrate może się ruszać z ramki na ramkę. Te pięć od
bitrate'u ma sens tylko przy znajomości silnika pod spodem, więc od tego zaczynamy.

**Silnik CBR.** Cel wybiera się przez `-b`, a koder mierzy w niego jako w średnią. Poszczególne
ramki nie są wymuszane do tego rozmiaru. Przy 48 kHz ramka AAC-LC obejmuje 1024 próbki, czyli
około 21,3 ms, więc cel 128 kbps stereo wychodzi na z grubsza 2730 bitów na ramkę średnio.
To ta liczba, do której steruje pętla dopasowania bitrate'u.

**Rezerwuar bitów.** Nawet przy CBR między ramkami a celem siedzi bufor. Kiedy ramka jest
łatwa, koder koduje ją oszczędnie i wkłada niewydane bity do rezerwuaru. Kiedy przychodzi
trudna ramka, pożycza z rezerwuaru i wydaje więcej, niż mu się należy. Średnia zostaje
zachowana, a chwilowy bitrate rusza się w górę i w dół, i dlatego CBR w AAC nigdy nie jest
idealnie płaski. Rozmiar rezerwuaru decyduje, jak daleko i jak długo koder może zbaczać.

**Twardy sufit.** Ramka AAC pomieści najwyżej 6144 bity na kanał, czyli 12288 bitów dla
stereo. Nic w tym koderze tego nie podnosi; to ograniczenie formatu. Każde ustawienie
rezerwuaru i szczytu jest o nie przycinane, i to też dlatego dostępne tutaj wahania są
umiarkowane w porównaniu z prawdziwym koderem o zmiennym bitrate.

**Quasi-VBR.** Problem, którym zajmują się te przełączniki, jest konkretny i prawdopodobnie
go słychać: przychodzi trudny fragment, koder wypuszcza kilka ramek dobrze powyżej średniej,
a potem dusi następne ramki, żeby natychmiast spłacić dług. Wynikiem jest słyszalne
przyciemnienie zaraz po głośnym momencie. Rozszerzenie rezerwuaru i dopuszczenie
krótkoterminowego szczytu pozwala koderowi trzymać podniesioną jakość dłużej i zamiast tego
zejść łagodnie. To nadal silnik CBR z większym buforem, a nie przydział na podstawie treści
jak w LAME, więc efekt jest lżejszy, niż sugeruje nazwa VBR.

Uwaga o dwóch twardych limitach na ramkę: nie należy ustawiać `--max-bits-frame` poniżej
średniej ani `--min-bits-frame` powyżej niej. Oba walczą wtedy z celem, w który pętla
dopasowania bitrate'u próbuje trafić, a wynikiem jest gorsza jakość, nie inny kompromis.
`--verbose` wypisuje blok budżetu rezerwuaru, w tym średnią liczbę bitów na ramkę i sufity,
i stamtąd pochodzą liczby do ustawienia tych przełączników.

### `--block-bias <n>`

**Ustawia udział bloków krótkich, od samych długich (0) do samych krótkich (255). Strumień
jest zgodny ze standardem przy każdej wartości.**

- Dotyczy: AAC-LC i rdzenia LC w HE-AAC, które mają bloki krótkie. LD i ELD ich nie mają
  (LD przełącza się zamiast tego na okno o małym nakładaniu, także przesuwane tym
  przełącznikiem; ELD w ogóle nie przełącza okien).
- Wartości: `0..255`, `128` to wartość fabryczna; domyślnie: `-1`, wyłączone (ustawienie fabryczne)
- Wpisywanie: liczba całkowita

Blok długi daje dobrą rozdzielczość częstotliwościową i złą czasową; blok krótki odwrotnie.
Detektor ataku decyduje ramka po ramce: porównuje energię każdej ósmej części ramki z
bieżącą energią przed nią i uznaje atak powyżej stosunku 10. Na muzyce ten stosunek wynosi
1,2 do 1,5 w zwykłej ramce i przekracza 10 tylko w około 2..3% ramek.

Ten przełącznik przesuwa ten próg według tablicy skalibrowanej na samym koderze (sześć
fragmentów: akustyczne stereo, eurodance, wokal, perkusja, 64 kbps, udział ramek krótkich
odczytany ze strumienia), tak żeby udział ramek krótkich szedł za suwakiem równo:
- `0`: tylko bloki długie, atak nie jest wykrywany nigdy.
- `1..127`: mniej ramek krótkich niż fabrycznie; przy `64` około połowy udziału fabrycznego.
- `128`: zachowanie fabryczne, bit w bit.
- `129..240`: więcej ramek krótkich, na muzyce około 12% ramek na 16 kroków; w okolicy `192`
  mniej więcej połowa ramek, przy `240` około 80..99%.
- `241..254`: na krótkie idą też ciche fragmenty (obniżona minimalna energia detektora).
- `255`: tylko bloki krótkie, każda ramka, łącznie z ciszą.

Materiał bez transjentów reaguje później niż muzyka: stały szum różowy i sinus zostają
długie mniej więcej do 208 i przechodzą na krótkie między 216 a 236, bo ich stosunek zostaje
blisko 1. Materiał z samymi mocnymi uderzeniami (ścieżka perkusji) ma swoje uderzenia przy
każdej wartości, więc jego udział rusza się mniej. Do 1.4.0-beta.1 przełącznik skalował próg
liniowo: powyżej 128 próg szedł tylko z 10 do 5 (6,5% ramek krótkich przy 255 na eurodance),
a tuż poniżej 128 przeskakiwał większość prawdziwych ataków, dlatego środek wydawał się nic
nie robić.

Zmienia się tylko decyzja o ataku. Sekwencję okien dalej budują własne przejścia kodera
(długie, start, krótkie, stop, długie), a oba kanały są synchronizowane jak w fabrycznym
FDK, więc dekoder zawsze dostaje legalną sekwencję. Sprawdzone: 300 kodowań (od 0 do 255,
LC, HE-AAC, HE-AAC v2, grupowanie krótkich 1 i 3, cisza, sinus, muzyka), 0 nielegalnych
przejść okien i 0 błędów dekodera. `--verbose` pokazuje wartość.

Przykłady (eurodance, 64 kbps; udział ramek krótkich):
- `--block-bias 64`: 0,1% zamiast 1,6%. Materiał stały zostaje długi, mocne uderzenia nadal
  idą na krótkie.
- `--block-bias 160`: 23%. Każdy większy wzrost poziomu dostaje bloki krótkie.
- `--block-bias 192`: 57%.
- `--block-bias 224`: 87%.
- `--block-bias 255`: 100%. Materiał tonalny brzmi szorstko, pokaz tego, po co są bloki
  długie.
- `--block-bias 0`: 0%. Każdy transjent niesie blok długi, pre-echo staje się głównym
  artefaktem na materiale perkusyjnym.

Współpraca i konflikty: przy `--block-bias 0` `--preecho-rise`, `--preecho-depth` i
`--tns-thresh` z grupy E zostają jedynymi narzędziami przeciw pre-echu, czyli sytuacja, w
jakiej LD i ELD są zawsze. `--short-groups`, `--window-shape` i `--pns-short` mają znaczenie
tylko tam, gdzie występują bloki krótkie. `--attack-decay` i `--attack-floor` zmieniają sam
detektor; łączą się z tym przełącznikiem.

### `--vbr-reservoir <bits>`

**Ustawia rozmiar rezerwuaru bitów, który decyduje, jak daleko i jak długo ramka może zbaczać
od średniej.**

- Dotyczy: każdego profilu, w trybie CBR. To główne pokrętło quasi-VBR.
- Wartości: bity, od `1` do `6144 * liczba kanałów` minus średnia liczba bitów na ramkę, i
  tam automatycznie przycinane; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Rezerwuar to bufor, z którego koder pożycza i do którego wpłaca. Jego rozmiar jest pamięcią
systemu: mały rezerwuar oznacza, że każde przekroczenie musi być spłacone niemal
natychmiast, co daje przyciemnienie zaraz po kulminacji; duży oznacza, że koder może trzymać
się powyżej średniej przez kilka ramek, a potem wracać stopniowo.

Większy jest zatem kierunkiem dla materiału, który powinien żyć dalej po trudnym fragmencie.
Mniejszy trzyma średnią ciaśniej z chwili na chwilę i jest właściwy, gdy łącze transportowe
ma wąski budżet chwilowy. Dobry punkt wyjścia to dwa do trzech razy tyle, ile `--verbose`
raportuje jako wartość domyślną.

Dwa zachowania, o których warto wiedzieć. `0` jest traktowane jako nieustawione i nie robi
nic, więc do ciasnego trzymania średniej służy mała wartość dodatnia albo
`--bitres-mode 2`. A każda wartość równa sufitowi albo większa, czyli `6144 * liczba kanałów`
minus średnia liczba bitów na ramkę, zapada się do sufitu, więc nie ma sposobu, by przesadzić,
ani korzyści z próbowania; proszenie o 100000 bitów na pliku stereo daje ten sam strumień, co
proszenie dokładnie o sufit.

Liczby pochodzą z `--verbose`, który wypisuje średnią liczbę bitów na ramkę i sufity w swoim
bloku budżetu rezerwuaru. Ustawianie tego przełącznika na zgadywanie to sposób, w jaki
ludzie przypadkiem kończą na sufice.

Przykłady:
- Łagodnie, więcej oddechu: `--vbr-reservoir 4000`. Nieco szersze wahania przy 128 kbps
  stereo; koder dochodzi do siebie po szczycie przez kilka ramek, a nie przez jedną.
- Mocno, więcej oddechu: `--vbr-reservoir 12000`. Praktyczne ustawienie quasi-VBR przy
  128 kbps stereo. Kulminacje trzymają swoją jakość, a powrót do średniej jest stopniowy.
- Przesadnie, więcej oddechu: `--vbr-reservoir 100000`. Dobrze powyżej sufitu, więc zapada
  się do sufitu. Koder używa maksymalnego oddechu, jaki format pozwala, a to ustawienie
  skrajne jest pouczające właśnie dlatego, że pokazuje, jak skromne jest to maksimum: lot AAC
  ogranicza limit 6144 bitów na kanał, a nie rezerwuar.
- Łagodnie, ciaśniej: `--vbr-reservoir 2000`. Poniżej zwykłej wartości domyślnej. Wahania
  się zwężają, średnia jest trzymana bliżej.
- Mocno, ciaśniej: `--vbr-reservoir 500`. Bardzo mało buforu. Trudne ramki nie mogą dużo
  pożyczyć, więc trudne fragmenty są kodowane zgrubnie, a nie drogo.
- Przesadnie, ciaśniej: `--vbr-reservoir 1`. Praktycznie brak rezerwuaru. Każda ramka musi
  zmieścić się we własnym przydziale, więc transjenty i gęste fragmenty nie mają gdzie się
  podziać, co brzmi jak gorsza wersja tego, co `--bitres-mode 2` robi celowo.

Współpraca i konflikty: łączy się w parę z `--peak-bitrate`, który podnosi krótkoterminowy
sufit, o jaki rezerwuar może się odbijać; rezerwuar bez podniesionego szczytu ma mniej
miejsca na działanie. `--bitres-mode` może zmniejszyć albo wyłączyć rezerwuar niezależnie od
rozmiaru ustawionego tutaj. `--max-bits-frame` ogranicza poszczególne ramki po wszystkim
tym, więc ciasne ograniczenie czyni duży rezerwuar bezsensownym. Przy `--unlock-bitrate`
rezerwuar staje się ważniejszy, bo przekraczanie jest tam normalnym przypadkiem.

Zmierzone: `0` jest traktowane jako nieustawione i nie daje żadnego efektu. Wartości równe
`6144 * liczba kanałów` minus średnia liczba bitów na ramkę albo większe zapadają się do
sufitu. `--verbose` wypisuje blok budżetu rezerwuaru, który podaje obie liczby.

### `--peak-bitrate <bps>`

**Dopuszcza krótkoterminowe szczyty bitrate'u do podanej wartości, przy czym średnia nadal
jest trzymana na celu z `-b`.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity na sekundę, powyżej celu z `-b`; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Rezerwuar decyduje, jak długo ramka może zbaczać; ten przełącznik decyduje, jak wysoko może
pójść. Ustawiony powyżej celu pozwala koderowi na chwilowe osiągnięcie tego bitrate'u przez
trudne ramki, podczas gdy pętla dopasowania bitrate'u dalej steruje średnią do `-b`.

Poniżej celu jest ignorowany, i to celowo: sufit szczytowy pod średnią nie ma sensu i koder
nie działa na tej sprzeczności. Użyteczny zakres jest więc tylko powyżej `-b`, a naturalnym
sposobem wyrażenia tego jest krotność: półtora raza cel jest ostrożne, dwa razy jest hojne.

Sufit 6144 bitów na kanał nadal obowiązuje, więc proszenie o szczyt poza to, co ramka
fizycznie pomieści, daje limit ramki. Przy 48 kHz stereo to ramka warta około 576 kbps (6144
x 2 bity x 48000 / 1024), więc przy zwykłych celach jest dużo miejsca, zanim limit formatu
zacznie wiązać.

Przykłady:
- Łagodnie, wyższy szczyt: `-b 128000 --peak-bitrate 150000`. Niewielki zapas. Kulminacje
  wychodzą nieco mniej sprasowane.
- Mocno, wyższy szczyt: `-b 128000 --peak-bitrate 200000`. Praktyczne ustawienie, zwykle w
  parze z szerokim rezerwuarem. Trudne ramki sięgają dobrze powyżej celu, a średnia jest
  utrzymana.
- Przesadnie, wyższy szczyt: `-b 128000 --peak-bitrate 1100000`. Blisko limitu ramki w
  formacie. Koder może wydać na jedną ramkę prawie całą fizyczną pojemność ramki, po czym pętla
  dopasowania bitrate'u musi wszystko to odebrać, a wynikiem jest dramatyczna wersja dokładnie
  tego przyciemnienia, któremu ustawienia quasi-VBR mają zapobiegać.
- W dół nie ma użytecznego kierunku: każda wartość równa celowi z `-b` albo mniejsza jest
  ignorowana, więc `--peak-bitrate 100000` przy `-b 128000` zachowuje się tak, jakby
  przełącznika nie było.

Współpraca i konflikty: naturalny towarzysz `--vbr-reservoir`; oba są potrzebne do zachowania
"nie duś się po szczycie", bo jeden daje wysokość, a drugi czas. `--max-bits-frame` jest
stosowany po tym przełączniku, więc wyraźne ograniczenie ramki wygrywa nad szczytem.
`--bitres-mode 2` czyni go bez znaczenia, usuwając rezerwuar, z którego szczyt miałby być
czerpany.

Zmierzone: konfiguracja z podpowiedzi w `--help` dla quasi-VBR przy około 128 kbps stereo to
`-b 128000 --peak-bitrate 200000 --vbr-reservoir 12000`.

### `--max-bits-frame <bits>`

**Twardy sufit na liczbę bitów, jakich może użyć pojedyncza ramka.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity, co najmniej średnia liczba bitów na ramkę i najwyżej `6144 * liczba
  kanałów`; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

To tępe narzędzie tej grupy. Tam gdzie `--peak-bitrate` wyraża bitrate, który silnik może
osiągnąć, ten wyraża bezwzględne ograniczenie na ramkę, stosowane później i wygrywające. Gdy
podane są oba, wiąże wyraźne ograniczenie.

To ograniczenie, a nie pokrętło jakości, i jego głównym zastosowaniem jest spełnienie
zewnętrznego wymagania: transportu, który nie uniesie ramki powyżej pewnego rozmiaru, albo
testu, który potrzebuje ograniczonej ramki maksymalnej. Sensowne ograniczenie to około
półtora raza średnia, co nadal dopuszcza prawdziwy oddech, ograniczając najgorszy przypadek.

Przy zbyt niskim ustawieniu głośne ramki zostają „zagłodzone”, co jest słyszalne dokładnie
jako to przyciemnienie, którego zwykle chce się uniknąć. Poniżej średniej walczy z celem
wprost i koder nie może jednocześnie uszanować ograniczenia i trafić w żądany bitrate.

Przykłady:
- Łagodnie, wyższe ograniczenie: przy 128 kbps stereo (średnio około 2730 bitów na ramkę),
  `--max-bits-frame 8000`. Dobrze powyżej czegokolwiek, co silnik normalnie wyprodukuje, więc
  w praktyce siatka bezpieczeństwa.
- Mocno, wyższe ograniczenie: `--max-bits-frame 12288`. Maksimum formatu dla stereo.
  Ograniczenie jest obecne, ale nigdy nie wiąże, co jest właściwym ustawieniem, gdy celem jest
  udokumentowanie przełącznika w wierszu poleceń bez zmiany zachowania.
- Przesadnie, wyższe ograniczenie: `--max-bits-frame 100000`. Powyżej limitu formatu, więc
  koder bierze mniejszą z tej wartości i własnego maksimum rezerwuaru (`6144 * liczba kanałów`),
  więc w stereo strumień jest identyczny jak przy `12288` (zmierzone: 12288, 20000 i 100000 dają
  ten sam strumień ADTS). Nieszkodliwe i przydatne jako przypomnienie, że żaden przełącznik
  tutaj nie podnosi limitu ramki.
- Łagodnie, niższe ograniczenie: `--max-bits-frame 4000` przy 128 kbps stereo. Szczyty
  przycięte, średnia nietknięta; blisko użytecznej wskazówki 1,5x.
- Mocno, niższe ograniczenie: `--max-bits-frame 3000` przy tym samym celu. Głośne ramki są
  wyraźnie „zagłodzone”, bo ograniczenie siedzi tylko trochę powyżej średniej.
- Przesadnie, niższe ograniczenie: `--max-bits-frame 2000` przy 128 kbps stereo, czyli poniżej
  średniej. Ograniczenie i cel przeczą sobie teraz w każdej ramce; jakość spada na całej linii
  i nic się nie zyskuje, i dlatego reguła mówi, żeby zostać na średniej albo powyżej.

Współpraca i konflikty: stosowany po `--peak-bitrate`, więc gdy podane są oba, wygrywa
wyraźne ograniczenie. Ciasne ograniczenie czyni duży `--vbr-reservoir` bezsensownym, bo bity
rezerwuaru nie mają gdzie zostać wydane. Przed wyborem wartości warto odczytać średnią
liczbę bitów na ramkę z bloku budżetu rezerwuaru w `--verbose`.

Zmierzone: AAC-LC 128 kbps na attacks48. Największa ramka fabrycznie to 3816 bitów ładunku. Przy
`--max-bits-frame 3000` największa staje się 2888; przy `4000` staje się 3440.

### `--min-bits-frame <bits>`

**Twarda dolna granica liczby bitów, jakich musi użyć każda ramka.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity, od `0` do średniej liczby bitów na ramkę; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Lustrzane odbicie sufitu i mniej użyteczna połowa. Dolna granica zmusza koder do wydania co
najmniej tyle na każdą ramkę, w tym na ramki, które naprawdę potrzebują mniej, takie jak
cisza albo trzymany cichy fragment. Te bity nie są oszczędzane na później, są wydawane, więc
dolna granica zamienia łatwe ramki w zmarnowany budżet.

Powodem jej istnienia jest transport o stałym bitrate i eksperymenty: część sprzętu dalej w
łańcuchu woli przewidywalną minimalną ramkę, a czasem warto zobaczyć, co robi kwantyzator,
gdy mu się każe wydać więcej, niż chce.

Bez takiego powodu wartość powinna zostać na `0`. Powyżej średniej walczy z celem tak samo,
jak zbyt niski sufit, i koder nie może uszanować obu.

Przykłady:
- Łagodnie, wyższa granica: `--min-bits-frame 1000` przy 128 kbps stereo. Ciche i bardzo łatwe
  ramki zostają dopełnione; średnią trzeba odrobić na trudniejszych ramkach.
- Mocno, wyższa granica: `--min-bits-frame 2000` przy tym samym celu, blisko średniej. Prawie
  każda ramka siedzi teraz na tej granicy, więc silnik traci większość zdolności do redystrybucji.
- Przesadnie, wyższa granica: `--min-bits-frame 2730` przy 128 kbps stereo, czyli dokładnie
  średnia. W połączeniu ze średnią jako celem jest to bliskie wymuszeniu sztywnego bitrate'u na
  ramkę, z tą różnicą, że `--bitres-mode 2` robi to porządnie i bez walki z pętlą dopasowania
  bitrate'u.
- Łagodnie, niższa granica: `--min-bits-frame 200`. Z trudem ograniczenie; łatwe ramki
  zostają tanie.
- Mocno, niższa granica: `--min-bits-frame 1`. Praktycznie brak dolnej granicy, ta sama intencja
  co pozostawienie przełącznika wyłączonego.
- Przesadnie, niższa granica: `--min-bits-frame 0`. Udokumentowana wartość wyłączająca dolną
  granicę; identyczna w efekcie z nieustawieniem jej i ustawienie, do którego wraca się po
  zakończeniu eksperymentu.

Współpraca i konflikty: przeciwny koniec `--max-bits-frame`, a oba omawia się zwykle razem
jako VBR z ograniczeniami. Wysoka dolna granica marnuje bity, które `--vbr-reservoir` ma
redystrybuować, więc te dwa działają przeciw sobie. `--bitres-mode 2` to właściwy sposób
uzyskania sztywnego zachowania na ramkę.

Zmierzone: AAC-LC 128 kbps na attacks48. Wyjście fabryczne 97056 bajtów; przy
`--min-bits-frame 1000` 97111 bajtów; przy `2900` 97709 bajtów.

### `--bitres-mode <n>`

**Wybiera, jak duża część mechanizmu rezerwuaru bitów jest aktywna, od pełnego oddechu do
sztywnego bitrate'u na ramkę.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: `0` pełny, `1` zredukowany, `2` wyłączony; domyślnie: `-1`, decyzja kodera
- Wpisywanie: liczba całkowita

Tam gdzie `--vbr-reservoir` ustawia rozmiar bufora, ten wybiera politykę. Pełny to normalne
zachowanie, takie samo jak fabryczne. Zredukowany zwęża działanie rezerwuaru. Wyłączony go
usuwa, więc każda ramka musi zmieścić się we własnym przydziale budżetu, a to najbliższa
rzecz do sztywnego, stałego bitrate'u ramka po ramce, jaką ten koder osiąga.

Jedynym wyraźnie użytecznym ustawieniem jest `2`, a przypadkiem użycia jest transport albo
test, który potrzebuje, by każda ramka miała około tego samego rozmiaru. Koszt jest realny:
transjent albo gęsty fragment nie może pożyczyć, więc zostaje zakodowany zgrubnie.

Wartości:
- `-1`: decyzja należy do kodera (domyślnie).
- `0`: pełny rezerwuar, to samo co zachowanie fabryczne.
- `1`: zredukowany rezerwuar. Na materiale testowym dało to ten sam strumień co `2`, więc różnicę
  między tymi dwoma należy traktować jako nieprzetestowaną, a nie ustaloną.
- `2`: rezerwuar wyłączony. Ramki stają się niemal stałe w rozmiarze, a trudne fragmenty tracą
  swoją drogę ucieczki.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --bitres-mode 2 -o rigid.m4a in.wav

Współpraca i konflikty: `2` czyni `--vbr-reservoir` i `--peak-bitrate` bez znaczenia, bo nie
ma rezerwuaru, na którym mogłyby działać. `0` to tryb, w którym należy używać reszty grupy
F.

Zmierzone: AAC-LC 128 kbps na attacks48. `0` daje strumień fabryczny; `1` i `2` dały ten sam
strumień, więc na tym materiale nie zaobserwowaliśmy różnicy między zredukowanym a
wyłączonym.

## G. Wyjście DAB+

Dwa przełączniki i raczej tryb niż poprawka. Wszędzie indziej w tym opisie koder robi plik
albo goły strumień AAC; przy `--dab` robi strumień superramek DAB+ dla naziemnego radia
cyfrowego.

### `--dab`

**Wypuszcza strumień superramek DAB+ zgodny z ETSI TS 102 563 zamiast pliku albo zwykłego
strumienia AAC.**

- Dotyczy: AAC-LC, HE-AAC i HE-AAC v2, mono albo stereo, przy 32000 albo 48000 Hz. Zastępuje
  normalną ścieżkę wyjścia całkowicie, więc `-f` i opcje kontenera nie mają zastosowania.
- Wartości: flaga; domyślnie: wyłączone (normalne wyjście do pliku albo strumienia)
- Wpisywanie: flaga

DAB+ nie niesie po prostu AAC w innym kontenerze. Różnią się trzy rzeczy. Transformata ma
960 próbek, a nie zwykłe 1024, żeby długość ramki pasowała do siatki czasowej radia. Dźwięk
jest pakowany w superramki o dokładnie 120 ms. A skoro eter jest nieprzyjazny, z zanikami,
tunelami i zakłóceniami, strumień jest chroniony podwójnie: firecode, czyli Fire CRC,
pilnuje nagłówka, a Reed-Solomon RS(120,110) nad GF(256) chroni ładunek, więc odbiornik może
odbudować dane, które przyszły uszkodzone. Koder robi to wszystko.

Wyjściem jest surowy strumień `.dabp`, jedna superramka po drugiej i nic więcej. To nie jest
plik do odtwarzacza multimedialnego; to coś, co pobiera multiplekser DAB+. Zwykły łańcuch to
`out.dabp` do `odr-dabmux`, który łączy kilka stacji w jeden zespół i wypuszcza ETI, a stamtąd
do nadajnika albo do odbiornika programowego, takiego jak welle.io albo dablin, do testów.

Trzy ograniczenia, które narzuca standard, a koder ich pilnuje: częstotliwość próbkowania
musi być 32000 albo 48000 Hz, nic innego nie jest w DAB+ legalne; bitrate musi być
wielokrotnością 8 kbps między 8 a 192; mono i stereo oba są w porządku.

Konwencja bitrate'u jest tu własna. Przy `--dab` `-b` to kilobity na sekundę, a koder sam
mnoży przez 1000, więc `-b 96` oznacza 96 kbps. `--unlock-bitrate` tego nie zmienia:
kontrola DAB+ i mnożenie przez 1000 dzieją się wcześniej, więc
`--dab --unlock-bitrate -b 96` daje ten sam strumień co bez odblokowania, a `-b 96000`
zostaje odrzucone jako niewielokrotność 8 kbps.

Normalnie profilu nie wybiera się ręcznie. Praktyka DAB+, idąc za narzędziem referencyjnym
`odr-audioenc`, wyprowadza go z bitrate'u i liczby kanałów, tak by niskie bitrate'y
automatycznie dostawały oszczędniejsze narzędzia:

- stereo przy 48 kbps albo niżej, czyli rozmiar podkanału 6 lub mniej, daje HE-AAC v2 z
  parametric stereo;
- mono do 64 kbps albo stereo do 80 kbps daje HE-AAC z SBR;
- cokolwiek wyżej daje AAC-LC.

Zatem `-b 96` na pliku stereo trafia na AAC-LC, `-b 64` na HE-AAC, a `-b 32` na HE-AAC v2
automatycznie. Profil nadal można wymusić przez `-p` (`2` dla LC, `5` dla HE-AAC, `29` dla
HE-AAC v2), gdy wybór jest świadomy. Wymuszony profil daje bit w bit ten sam strumień, który
dobór automatyczny daje dla tego profilu. Inne wartości `-p` są odrzucane, tak samo `-p 29` na
wejściu mono. Wymuszony profil poniżej tablic strojenia FDK, na przykład `-p 2` przy 8 kbps albo
`-p 5` przy 8 kbps stereo, kończy się komunikatem: pomiń `-p` albo dodaj `--unlock-bitrate`.

Wartości:
- pominięte: normalne wyjście, a koder zachowuje się dokładnie jak fabryczny.
- `--dab`: wyjście superramek DAB+.

Przykłady w kontekście:

    fdkaac --dab -b 96 -o out.dabp input.wav     # auto AAC-LC
    fdkaac --dab -b 64 -o out.dabp input.wav     # auto HE-AAC
    fdkaac --dab -b 32 -o out.dabp input.wav     # auto HE-AAC v2

Współpraca i konflikty: `--dab-label` robi coś tylko w tym trybie. `-f` i przełączniki
kontenera MP4 nie mają zastosowania, bo nie ma kontenera. `--unlock-bitrate` zmienia sposób
odczytu `-b`. Przełączniki z grup od B do F nadal działają na rdzeń AAC wewnątrz superramek,
więc pokrętła stereo, SBR, szczegółu, transjentów, pokrycia i bitrate'u pozostają dostępne.
Częstotliwości podane w Hz są przeliczane na siatce 960 próbek, dlatego `--verbose` pokazuje
nieco inne granice pasm niż w strumieniu 1024 próbek. Zmierzone na muzyce i na teście
ataków, LC 48 i 96 kbps, HE-AAC stereo 64 kbps, HE-AAC mono 48 kbps i HE-AAC v2 32 kbps: 58
ustawień przełączników dodanych od v1.3.2 oraz przełączników pasmowych, każdy strumień
zdekodowany przez faad2 za dablinem bez błędu. Przełącznik, którego narzędzia nie ma w
wybranym profilu, nic nie robi, tak samo jak w zwykłym strumieniu: pokrętła SBR w AAC-LC,
pokrętła stereo w mono i w HE-AAC v2.

Zmierzone: strumienie dekodują się niezależnym dekoderem faad2 za dablinem, etykieta DLS
odczytuje się poprawnie, a wyjście AAC-LC jest identyczne bit w bit z tym, co z tego samego
wejścia produkuje referencyjny `odr-audioenc`. Przetestowaliśmy dziewięć kombinacji, 48 i 32
kHz przeciw mono i stereo w trzech profilach, każda niezależnie dekodowalna.

### `--dab-label <text>`

**Niesie statyczną etykietę stacji w superramce, pokazywaną na wyświetlaczu odbiornika DAB+.**

- Dotyczy: wyłącznie trybu `--dab`. Bez `--dab` etykieta jest ignorowana, bo nie ma superramki,
  w której można by ją umieścić.
- Wartości: tekst, do około 48 znaków; domyślnie: brak etykiety
- Wpisywanie: łańcuch tekstowy

Odbiornik DAB+ pokazuje tekst na swoim wyświetlaczu, zwykle nazwę stacji i często tytuł
tego, co leci. Ten tekst podróżuje w kanale zwanym DLS, czyli Dynamic Label Segment,
upchniętym w superramce jako dane X-PAD. Ten koder niesie etykietę statyczną: jeden ustalony
łańcuch dla całego pliku albo strumienia.

Statyczna to słowo kluczowe. Tekst nie zmienia się w trakcie kodowania. Prawdziwie
dynamiczna etykieta, która aktualizuje się utwór po utworze, oraz pokaz slajdów MOT, który
wyświetla okładki, to zadanie `ODR-PadEnc` w łańcuchu OpenDigitalRadio i nie są częścią tego
kodera.

Limit długości około 48 znaków wynika z trzech segmentów DLS upakowanych w jeden PAD.
Odbiorniki różnią się tym, ile z długiej etykiety pokazują naraz, więc dla nazwy stacji
użyteczna długość jest krótsza niż limit.

Wartości:
- pominięte: brak etykiety w strumieniu.
- `--dab-label <text>`: podany łańcuch jest niesiony jako DLS. Łańcuch ze spacjami należy
  wziąć w cudzysłowy.

Przykład w kontekście:

    fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

Współpraca i konflikty: wymaga `--dab`. Niezależny od każdego innego przełącznika, bo dotyka
PAD, a nie dźwięku.

# Dodatki

## Jak czytać `--verbose`

`--verbose` wypisuje wartości, które koder faktycznie zainicjalizował, zanim zacznie
kodować. Blok ma kilka sekcji. Większość pól tłumaczy się sama; te poniżej wymagają słowa
komentarza.

| Pole | Jak je czytać |
|---|---|
| `AOT (profile)` | 2 AAC-LC, 5 HE-AAC, 29 HE-AAC v2, 23 AAC-LD, 39 AAC-ELD. |
| `bitrate-mode` | 0 CBR, 1..5 VBR (wyżej = lepiej). |
| `channel-mode` | 1 mono, 2 stereo. W HE-AAC v2 rdzeń jest mono, a stereo tworzy PS. |
| `core bandwidth` | Górna krawędź rdzenia AAC, wyrównana do najbliższej granicy pasma współczynnika skali, więc może się różnić od wpisanej wartości (`-w 17300` daje `17915 Hz`). W nawiasie źródło: `from -w`, `from --core-cutoff` albo `auto`. Pod SBR to tylko rdzeń; SBR ciągnie pasmo wyżej. Pod SBR `-w` jest ignorowane, choć źródło może nadal brzmieć `from -w`. |
| `final BW (AAC+SBR)` | Tylko z SBR: przybliżona górna granica całego sygnału, rdzeń razem z SBR, wyliczona z indeksu stop SBR. |
| `signaling-mode` | Sposób sygnalizacji SBR i PS: 0 niejawna, 1 jawna zgodna wstecz, 2 jawna hierarchiczna. |
| `SBR ratio` | 1 downsampled (single rate), 2 dual rate (rdzeń pracuje z połową częstotliwości próbkowania). |
| `sbr amp res` | Rozdzielczość amplitudy obwiedni: 0 = 1,5 dB, 1 = 3,0 dB. |
| `granule length`, `frame length` | Próbki na ramkę: 1024 dla LC, 512 albo 480 dla LD i ELD. |
| `codec delay` | Opóźnienie w próbkach na kanał, całkowite i samego rdzenia. Ważne przy odtwarzaniu bez przerw. |
| `active SFBs (long)` | Liczba pasm współczynnika skali w bloku długim. Górna granica dla `--msbands`, `--isbands` i każdego argumentu `<sfb>`. |
| `IS corr threshold`, `IS L/R ratio thresh` | Wartości Q8 (256 = 1,0). Niższy próg korelacji sprawia, że intensity stereo włącza się chętniej. |
| `IS min contiguous SFBs` | Ile sąsiednich pasm musi się kwalifikować, zanim ruszy intensity stereo. |
| `TNS mask` | Maska bitowa 0x0..0xF aktywnych filtrów TNS. |
| `MS/IS bands: auto up to N` | Najwyższy indeks pasma, do którego narzędzie może działać. |
| `sbr header period` | Ramki i milisekundy między nagłówkami SBR: tyle czeka dekoder, który dołącza do strumienia, zanim usłyszy SBR. Dla ELD podawany jest okres konfiguracji LATM/LOAS, bo ELD nie ma nagłówka SBR w strumieniu. |
| `bit reservoir budget` | Średnia liczba bitów na ramkę i sufity rezerwuaru dla mono i stereo. Na tej podstawie dobiera się `--vbr-reservoir`. |
| `franken overrides applied` | Przełączniki, które w tym uruchomieniu odbiegają od ustawień fabrycznych, z wartością efektywną. Jeśli podanego przełącznika tu nie ma, nie zadziałał. |

## Pasma współczynnika skali i częstotliwości

Pasma numerujemy od 0 na dole. Tabela pokazuje przybliżoną górną krawędź co czwartego
pasma w bloku długim dla każdej częstotliwości próbkowania; ostatni wiersz podaje liczbę
pasm i częstotliwość Nyquista. Służy do przeliczania `--msbands`, `--isbands`,
`--hole-start` i innych argumentów `<sfb>` na herce. Dokładną liczbę dla danego
uruchomienia podaje `--verbose` (`active SFBs (long)`); zależy ona też od kodowanego pasma.

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
| 44  | brak    | 9647 | 13000| 17916 | 19500 | brak     |
| 48  | brak    | brak    | 15000| 22050 | 24000 | brak     |
| **liczba pasm / Nyquist** | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

### AAC-LC: automatyczne pasmo rdzenia zależnie od bitrate'u

Bez `-w` FDK wybiera pasmo z tabeli indeksowanej bitrate'em na kanał, a potem wyrównuje je
do granicy pasma SFB. Zmierzone przez `--verbose` przy 48 kHz (dla stereo `-b` to suma obu
kanałów):

| `-b` stereo | pasmo rdzenia | `-b` mono | pasmo rdzenia |
|---:|---:|---:|---:|
| 24000, 32000 | 6843 Hz | 12000, 16000 | 5062 Hz |
| 40000, 48000 | 9750 Hz | 20000, 24000 | 7500 Hz |
| 56000, 64000 | 13500 Hz | 28000, 32000 | 9750 Hz |
| 80000, 96000 | 14250 Hz | 40000, 48000 | 12750 Hz |
| 128000 | 15750 Hz | 64000 | 14250 Hz |
| 192000 | 17250 Hz | 96000 | 17250 Hz |

Wyższe `-w` pomaga tylko wtedy, gdy są na nie bity: przy 24..50 kbps wymuszone szerokie
pasmo to dokładnie obszar grupy E (dziury). Powyżej 20 kHz rdzeń wymaga
`--uncap-bandwidth` i częstotliwości próbkowania, która ma na to miejsce.

## Kontener M4A: co jest konieczne, a co można usunąć

Plik `.m4a` to drzewo pudełek (boxów). Szkielet odtwarzania jest obowiązkowy i nie ma do
niego przełącznika: `ftyp`, `mdat` (dane audio) i `moov` z `mvhd`, `trak`, `tkhd`, `mdia`
(`mdhd`, `hdlr`, `minf` z `smhd`, `dinf` i `stbl`, w którym są `stsd`, `stts`, `stsc`,
`stsz`, `stco`). Bez niego żaden dekoder nie znajdzie dźwięku.

Opcjonalny jest cały blok `udta` / `meta` / `ilst`: znacznik identyfikujący koder (`©too`),
`iTunSMPB` (opóźnienie kodera dla odtwarzania bez przerw) i wszystkie znaczniki tekstowe.

| Przełącznik | Działa na | Co robi |
|---|---|---|
| `--no-tool-tag` | `.m4a` | Pomija znacznik identyfikujący koder. Pozostałe znaczniki i dane gapless zostają. |
| `--minimal-moov` | `.m4a` | Najmniejszy legalny plik: pomija cały blok metadanych (znacznik kodera, `iTunSMPB`, wszystkie znaczniki). Szkielet odtwarzania zostaje. |

Zmierzone na 2 s stereo 128 kbps: 34381 bajtów domyślnie, 34297 z `--no-tool-tag` (o 84
mniej), 34048 z `--minimal-moov` (o 333 mniej). Narzut to głównie obowiązkowy szkielet.
Całkowitą rezygnację z kontenera daje zapis surowego ADTS (`-f 2 -o out.aac`), który nie ma
znaczników ani danych gapless.

`--minimal-moov` usuwa `iTunSMPB`, więc przy łączeniu utworów mogą pojawić się drobne
przerwy, bo opóźnienie kodera nie jest wtedy sygnalizowane. Dla albumów odtwarzanych
bez przerw lepiej zostawić ustawienie domyślne.

## Domyślne wyjście a oryginalna binarka

Sprawdzone względem oryginalnej binarki dostarczanej z dBpoweramp R17 (ta sama libfdk-aac
4.0.1, pakiet 2.0.3). AAC-LC jest identyczny bit w bit (ta sama suma md5). HE-AAC i HE-AAC
v2 różnią się bajtami, ale nie jakością: widmo HE-AAC v2 zgadza się co do 0,000 dB, a błąd
HE-AAC względem oryginału jest taki sam jak błąd samego oryginału (7,05e11 wobec 7,05e11).
Różnice bajtów wynikają z innego kompilatora (mingw/gcc zamiast MSVC) i zaokrągleń
arytmetyki stałoprzecinkowej.

# Budowanie i testy

## Budowanie

Repozytorium zawiera tylko źródła. GitHub Actions buduje obie binarki dla Windows na żądanie
(workflow „Build Franken FDK AAC (Windows x64 + x86)”, przycisk „Run workflow”) oraz przy
każdym tagu wydania. Lokalnie, na Linuksie albo w WSL z mingw-w64 i autotools:

    bash tools/build_win.sh both      # albo x64 / x86

Skrypt buduje każdą architekturę ze świeżej kopii źródeł, tak samo jak CI, i zostawia
`fdkaac-franken-x64.exe` oraz `fdkaac-franken-x86.exe` w katalogu projektu. Wszystkie
zmiany w libfdk-aac przechodzą przez jeden moduł, `libAACenc/src/franken.{h,cpp}` (globalny
blok `g_franken`, w którym każda nieustawiona wartość oznacza ustawienie fabryczne); nowe parametry
biblioteki (`AACENC_PARAM`, zakres `0xF0xx`) są zadeklarowane w `aacenc_lib.h`.

## Testy

    make check

uruchamia `tests/check.sh` na zbudowanych binarkach. Sprawdza, że każdy przełącznik jest w
`--help`, że każdy daje strumień przyjmowany przez dekodery bez błędów, że każdy faktycznie
zmienia strumień na materiale, na który powinien działać, że wartości odczytane ze
strumienia zgadzają się z żądanymi (kształt okna, grupy, rozmiary ramek), że dziury i
pre-echo po dekodowaniu zmieniają się w zmierzonym kierunku, oraz że bez nowych
przełączników wynik jest identyczny bajt w bajt z fabrycznym. Wymaga ffmpeg i python3; kod
wyjścia 0 oznacza sukces, 1 błędy, 77 brak zależności.

Narzędzia pomiarowe, z których pochodzą liczby w tym dokumencie, są w `tools/`:
`tools/core/` (dziury i pre-echo po dekodowaniu), `tools/invf/` (filtrowanie odwrotne SBR i
progi niskiego bitrate'u), `tools/msis/` (MS i IS odczytane ze strumienia, nagłówki SBR,
dołączanie do strumienia), `tools/dab/` (weryfikacja DAB+).
