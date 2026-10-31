# Franken FDK AAC

Franken FDK AAC to enkoder Fraunhofer FDK AAC (libfdk-aac 2.0.3, mstorsjo/fdk-aac) z
frontendem nu774/fdkaac 1.0.2, rozszerzony o przełączniki odsłaniające decyzje, które FDK
zwykle podejmuje sam: jak kodowane jest stereo, jak odtwarzane jest górne pasmo, gdzie
wolno wyzerować pasma, jak traktowane są transjenty, jak bity rozkładają się w czasie.
Jest przeznaczony dla realizatorów, którzy chcą usłyszeć i zmierzyć, co te decyzje robią,
zwłaszcza w skrajnościach: przy bardzo niskim bitrate'ie, wymuszonym szerokim paśmie,
mowie i streamingu.

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski.
Zbudowany na otwartym kodzie: libfdk-aac (Fraunhofer IIS) i frontend nu774/fdkaac.

Bez żadnego z nowych przełączników enkoder daje dokładnie to samo co stockowy FDK, bajt w
bajt. Każdy przełącznik trzeba włączyć świadomie i każdy może też pogorszyć wynik:
pozwalają wejść tam, gdzie strojenie FDK celowo nie wchodzi.

## Pobieranie

Gotowe binarki dla Windows są publikowane jako GitHub Releases i pobierają się bez konta:

**https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Wersje beta (na przykład `v1.4.0-beta.1`) są publikowane jako wydania wstępne, więc powyższy
odnośnik nadal pokazuje ostatnią wersję stabilną; bety są na pełnej liście:
**https://github.com/michaldziwisz/franken-fdk-aac/releases**

`franken-fdk-aac-x64-vX.Y.Z.zip` to wersja 64-bitowa, `x86` to 32-bitowa. Każde archiwum
zawiera statyczny enkoder (bez bibliotek DLL) i pełną dokumentację. Samo repozytorium ma
tylko źródła; budowanie opisuje koniec tego dokumentu.

## Pierwsze kroki

Wszystkie oryginalne opcje frontendu działają jak dotąd (`-p`, `-b`, `-m`, `-w`, `-a`,
`-s`, `-f`, znaczniki); `fdkaac-franken-x64.exe --help` wypisuje je razem z nowymi.

1. Zakoduj raz z `--verbose` i przeczytaj, co enkoder faktycznie ustawił:

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

    # HE-AAC v2 z szerszym rdzeniem, niż wybiera FDK
    fdkaac-franken-x64.exe -p 29 -b 48000 --core-cutoff 7500 -o out.m4a in.wav

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

To jest opis wszystkich przełączników. Każdy ma własną kartę, a każda
karta jest zbudowana tak samo, więc można czytać tylko tę kartę, która jest potrzebna.

## Jak czytać kartę

Nagłówek to przełącznik w takiej postaci, w jakiej się go wpisuje, razem z argumentem.
Pod nim:

- Jedno zdanie pogrubione: do czego służy przełącznik, pod kątem dźwięku albo
  zachowania.
- **Dotyczy**: profile i sytuacje, w których przełącznik faktycznie działa. Jeśli w
  jakimś profilu nie robi nic, jest to napisane właśnie tutaj.
- **Wartości**: dozwolony zakres, jednostka i wartość domyślna. Domyślna to zawsze ta,
  która oznacza „decyduje stock FDK”.
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

**Wyłączony = stock.** Każdy przełącznik Frankensteina jest wyłączony, dopóki się go nie
poda. Jeśli żadnego z nich nie ma w linii poleceń, wynik jest identyczny bajt w bajt z
wynikiem stock FDK (zachowanie niezmienionego FDK). „Wyłączony” zapisuje się jako `-1`
dla niemal wszystkich (to wartownik oznaczający „zostaw własną decyzję FDK”) oraz jako
`0` dla `--core-cutoff`, `--side-bias`, `--side-knee` i `--mask-slope`, gdzie `-1` byłoby
poprawną, działającą wartością.

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
  samego FDK i dlatego przecieka ona do linii poleceń.
- `SFB`: numer pasma współczynnika skali, liczony od `0` od dołu widma w górę. Pasmo
  współczynnika skali to grupa linii widmowych, które dzielą jeden współczynnik skali,
  w przybliżeniu zgodnie z pasmem krytycznym: wąskie na dole, szerokie na górze. Ile
  tych pasm istnieje, zależy od profilu, częstotliwości próbkowania i kodowanego pasma,
  więc numer SFB ma sens tylko wtedy, gdy znana jest ta liczba. Wypisuje ją `--verbose`,
  na przykład `active SFBs (long): 49` przy AAC-LC 48 kHz stereo.

**Jak sprawdzić, co weszło w życie.** `--verbose` wypisuje na stderr rzeczywiste,
zainicjalizowane parametry kodera, zanim zacznie się kodowanie, a do tego linię
`franken overrides applied` z listą przełączników, które w tym przebiegu naprawdę
odbiegają od stock. Jeśli ustawionej wartości tam nie ma, to nie weszła ona w życie i
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

AAC-LD (`-p 23`) i AAC-ELD (`-p 39`) działają w tym koderze, ale nie da się ich zapisać
do ADTS. Żądanie `-f 2` z tymi profilami kończy się komunikatem „encoder initialization
failed”, a winowajcą jest format transportu, nie profil. Trzeba użyć M4A (`-f 0`, wartość
domyślna), LATM (`-f 6` albo `-f 7`) albo LOAS (`-f 10`). LOAS jest wyborem praktycznym,
gdy potrzebny jest goły strumień do pomiaru albo do podania dalej, bo jest to
samowystarczalny strumień bajtów, tak jak ADTS dla LC i HE-AAC.

## Mapa grup

Przełączniki są pogrupowane w `--help` z grubsza od łatwych do dziwactw, a ten opis
trzyma się tej samej kolejności.

- **A. Na początek**: pokrętła dla użytkownika. Diagnostyka, intensity stereo na jednym
  suwaku, strojenie pod mowę i dwie flagi włączane świadomie, które zdejmują wbudowane
  ograniczenia.
- **B. Kodowanie stereo**: mid/side, intensity stereo w szczegółach, parametric stereo.
- **C. Pasmo i SBR**: górne pasmo, jego detektory, estymatory i siatka.
- **D. Maskowanie, kształtowanie szumu i szczegół**: progi psychoakustyczne, SNR, którego
  koder wymaga w każdym pasmie, TNS i PNS.
- **E. Transjenty rdzenia, pre-echo i dziury w widmie**: rdzeń LC, LD i ELD pod presją
  bitów.
- **F. Przełączanie bloków i kontrola bitrate'u**: decyzja blok długi czy krótki,
  rezerwuar bitów i pokrętła quasi-VBR.
- **G. Wyjście DAB+**: superramki radia cyfrowego.

## A. Na początek

Pięć przełączników. Jeden tylko zagląda do kodera, dwa go stroją, a dwa zdejmują
ograniczenia, których pilnuje stock FDK. Jeśli ten build jest dla ciebie nowy, przeczytaj
najpierw o `--verbose` i używaj go po każdym kolejnym przełączniku, który spróbujesz.

### `--verbose`

**Wypisuje na stderr rzeczywiste, zainicjalizowane ustawienia kodera przed kodowaniem,
żeby było widać, co faktycznie weszło w życie.**

- Dotyczy: każdego profilu i każdego transportu. Nie zmienia nic w strumieniu wyjściowym.
- Wartości: flaga, bez argumentu; domyślnie: wyłączona
- Wpisywanie: flaga

Koder podejmuje na podstawie linii poleceń bardzo wiele decyzji i większość z nich nie
jest wpisanymi wartościami. Pasmo zostaje przyciągnięte do granicy pasma współczynnika
skali. Bitrate wybiera wiersz w tabeli strojenia, a ten z kolei ustawia PNS, intensity
stereo i siatkę SBR. Poprawnie zapisany przełącznik może i tak zostać zignorowany, bo
profil nie ma takiego mechanizmu. `--verbose` to jedyny sposób, by zobaczyć wynik tego
wszystkiego przed zmarnowaniem czasu na słuchanie.

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
naprawdę odbiegają od stock.

Trzy odczyty warto obejrzeć uważnie. Pasmo rdzenia jest zakotwiczone w SFB, więc wpisane
`-w 17300` może wrócić jako `17915 Hz`: koder nie potrafi przyciąć w środku pasma. Pod
SBR wartość `-w` jest ignorowana, ale nadal opisana jako `[from -w]`, co dezorientuje do
chwili, gdy przypomnimy sobie, że tam działa przełącznik `--core-cutoff`. A
`active SFBs (long)` to pułap dla `--msbands`, `--isbands` i każdego argumentu `<sfb>`:
poproszenie o pasmo 48, gdy kodowanych jest tylko 40, nie jest błędem, po prostu nic nie
robi.

Wartości:
- `--verbose`: wypisz zrzut. Nie ma przełącznika wyłączającego, bo pominięcie go jest
  wyłączeniem.

Przykład w kontekście:

    fdkaac -p 5 -b 64000 --verbose -o out.m4a in.wav

Współpraca i konflikty: brak. To narzędzie diagnostyczne dla wszystkiego innego w tym
opisie i najszybsza odpowiedź na pytanie „czy mój przełącznik w ogóle coś zrobił”.

Zmierzone: przy AAC-LC 48 kHz stereo raportuje `active SFBs (long): 49`. Przy HE-AAC v2
48 kHz domyślne odcięcie rdzenia wychodzi na 8613 Hz.

### `--is-aggression <0..100>`

**Jeden suwak decydujący, jak ochoczo koder zwija górne pasma do intensity stereo,
wymieniając separację stereo na górze na bity.**

- Dotyczy: profili z prawdziwym rdzeniem stereo, czyli AAC-LC, AAC-LD i AAC-ELD, oraz
  rdzenia HE-AAC. Bez efektu w mono. Pod HE-AAC v2 obraz stereo niesie parametric stereo
  na rdzeniu mono, więc intensity stereo nie ma na czym działać.
- Wartości: `0..100`, gdzie `0` to stock FDK; domyślnie: `-1`, wyłączone (identycznie jak
  stock)
- Wpisywanie: liczba całkowita

Intensity stereo (IS) zastępuje osobne widma lewego i prawego kanału w górnych pasmach
jedną wspólną obwiednią energii plus kierunkiem. Ucho słabo lokalizuje wysokie tony,
więc jest to tanie: odrzuca się połowę danych w tych pasmach, a płaci separacją stereo
na górze. Na materiale, który naprawdę ma różne góry w obu kanałach, na przykład z
talerzami ustawionymi mocno na jedną stronę albo z szerokimi efektami przestrzennymi,
płaci się słyszalnie.

Stock FDK jest wobec IS wyjątkowo nieśmiały i właśnie dlatego ludzie dochodzą do wniosku,
że przełącznik nie robi nic. Bramek jest cztery, a pasmo musi przejść wszystkie naraz:
bramka dopuszczenia, która bierze IS pod uwagę tylko wtedy, gdy bitrate na pasmo spada
poniżej progu, próg korelacji między kanałami na poziomie około 0,95, minimalna seria 6
kolejnych pasm i stabilny kierunek. Poluzowanie jednej bramki zwykle nie zmienia nic, bo
blokuje kolejna. `--is-aggression` rusza nimi wszystkimi razem, spójnie: usuwa bramkę
bitrate'u i pasma oraz, w miarę wzrostu liczby, luzuje wymagania co do korelacji,
kierunku i rozmiaru obszaru.

Wyżej oznacza więcej pasm kodowanych jako IS, mniej bitów wydanych na górę widma i
słabszą separację stereo właśnie tam. `100` to prawdziwe maksimum: bramka usunięta,
korelacja poluzowana do około 0,475, obszar jednego pasma już wystarcza, a tolerancja
kierunku jest szeroka. Niskie wartości nie nadają się na delikatną korektę, bo małe
poluzowanie często i tak zostawia inną bramkę jako wiążącą; `1` nie zmieniło nic na
naszym materiale testowym. Użyteczne okno pracy zaczyna się około 30.

Jest tylko jeden kierunek do przejścia, bo `0` to już stock, a stock jest blisko
„nigdy”.

Przykłady:
- Łagodnie, więcej IS: `--is-aggression 30`. Na skorelowanym materiale pasma zaczynają
  trafiać do IS. Obraz na górze zwęża się nieznacznie, kilka procent bitów zostaje
  uwolnionych na resztę.
- Mocno, więcej IS: `--is-aggression 50`. Wyraźnie więcej pasm w IS. Słyszalne zwężenie
  na talerzach i ogonach pogłosu, realna oszczędność bitów przy 48..96 kbps.
- Przesadnie, więcej IS: `--is-aggression 100`. Do IS idzie prawie wszystko, co może.
  Góra obrazu zlepia się ze środkiem, a spanoramowane góry zaczynają dryfować. Przydatne
  przy 32 kbps i niżej oraz jako karykatura, żeby usłyszeć, jak brzmi IS puszczone
  luzem.

Współpraca i konflikty: jawne przełączniki `--is-*` z grupy B nadpisują to, co ustawił
suwak, więc najpierw użyj suwaka, a po `--is-corr-thresh`, `--is-min-sfbs` i
`--is-lr-ratio` sięgaj tylko wtedy, gdy potrzebna jest precyzja. `--is 0` wyłącza
intensity stereo całkowicie i czyni ten przełącznik bez znaczenia. `--is-force-lo` i
`--is-force-hi` obchodzą każdą bramkę, więc ten suwak nie jest im w ogóle potrzebny.
`--verbose` wypisuje efektywny próg korelacji i minimalną liczbę pasm, czym potwierdza
się, że suwak zadziałał.

Zmierzone: na mix48 przy AAC-LC 64 kbps `1` nie dało żadnej zmiany strumienia, a `30`,
`50` i `100` za każdym razem strumień zmieniły.

### `--speech`

**Przełącza warstwę SBR na strojenie pod mowę: inne filtrowanie odwrotne, inny poziom
szumu (noise floor) i brak parametrycznego kodowania górnego pasma.**

- Dotyczy: HE-AAC i HE-AAC v2, czyli wszędzie, gdzie działa SBR, także ELD z SBR. Bez
  efektu w AAC-LC, które nigdzie w FDK nie ma osobnego trybu mowy.
- Wartości: flaga; domyślnie: wyłączona (strojenie pod muzykę)
- Wpisywanie: flaga

SBR odbudowuje górne pasmo z zakodowanego pasma dolnego i musi wiedzieć, jak tonalne
jest źródło, żeby zdecydować, jak mocno wybielić łatane pasmo i ile szumu dosypać.
Muzyka i mowa odpowiadają na to zupełnie inaczej. Mowa przy niskim bitrate cierpi przy
strojeniu pod muzykę na konkretny artefakt: odbudowana góra bulgocze, bo estymator wciąż
uznaje łatę za tonalną, a w rzeczywistości jest to ruchomy formant plus oddech.

Flaga zmienia kilka rzeczy naraz: tabela decyzyjna filtrowania odwrotnego przełącza się
na tabelę mowy, wagowanie poziomu szumu przechodzi na wartość dla mowy, a parametryczne
kodowanie górnego pasma zostaje wyłączone. Dla podcastu, audiobooka albo dialogu przy
24..48 kbps efektem jest naturalniejsza góra z mniejszym bulgotaniem.

Jeśli potrzebna jest tylko część tego, co robi flaga, poszczególne elementy są dostępne
osobno: `--sbr-invf-table 1` daje samą tabelę decyzyjną mowy, `--sbr-nf-weight 1.0` samą
wagę szumu dla mowy, a `--sbr-softness 4` oba te elementy, w każdym przypadku bez
ruszania częstotliwości podziału i bez wyłączania kodowania parametrycznego.

Dla mowy w zwykłym AAC-LC nie ma odpowiednika i ten opis nie będzie udawał inaczej.
Model psychoakustyczny LC jest ten sam dla mowy i dla muzyki. Pomaga tam natomiast
zwężenie pasma, bo mowa ma mało treści wysoko, za pomocą `--core-cutoff 12000` do
`14000`, a przy bardzo niskim bitrate włączenie substytucji szumem przez
`--pns 1 --force-pns`.

Wartości:
- pominięta: strojenie pod muzykę (stock).
- `--speech`: strojenie pod mowę.

Przykład w kontekście:

    fdkaac -p 5 -b 32000 --speech -o speech.m4a podcast.wav

Współpraca i konflikty: jawne przełączniki estymatorów SBR z grupy C wygrywają z tą
flagą, więc `--sbr-invf` wymusza ustalony poziom filtrowania odwrotnego bez względu na
nią. `--sbr-invf-table` i `--sbr-nf-weight` idą za `--speech`, dopóki zostają na `-1`, i
właśnie tak jedna flaga potrafi ruszyć oba.

Zmierzone: zerowy efekt w AAC-LC; zmienia strumień HE-AAC.

### `--uncap-bandwidth`

**Zdejmuje twardy limit 20 kHz, jaki FDK nakłada na pasmo rdzenia, żeby `--core-cutoff`
mógł sięgnąć aż do Nyquista.**

- Dotyczy: każdego profilu z rdzeniem, który da się rozszerzyć, w praktyce AAC-LC przy
  wysokim bitrate ze źródła o wysokiej częstotliwości próbkowania. Bez `--core-cutoff`
  nie ma sensu.
- Wartości: flaga; domyślnie: wyłączona (obowiązuje limit 20 kHz)
- Wpisywanie: flaga

FDK ogranicza pasmo rdzenia do `min(20000, sr/2)`. Rozumowanie jest słuszne dla
normalnego materiału: powyżej 20 kHz prawie nikt nie słyszy, a bity lepiej wydać niżej.
Konsekwencja jest taka, że źródło 96 kHz zakodowane przy hojnym bitrate nadal nie koduje
nic powyżej 20 kHz, niezależnie od żądań, a dodatkowa częstotliwość próbkowania kupuje
tylko łagodniejsze zbocze filtra antyaliasingowego.

Ta flaga zdejmuje ograniczenie. Sama z siebie nie robi zupełnie nic: podnosi tylko pułap,
nie rusza samego pasma. Pasmem rusza przełącznik `--core-cutoff <hz>`, a ze zdjętym
limitem może on sięgnąć do połowy częstotliwości próbkowania.

Warto mieć jasność, co się za to dostaje. Treść powyżej 20 kHz kosztuje bity, które
schodzą z pasma słyszalnego, chyba że bitrate ma realny zapas, a strumienie z pasmem poza
typowym zakresem leżą poza zwykłą praktyką, więc niektóre dekodery mogą się na nie boczyć.
To świadome włączenie i właśnie dlatego jest to osobna flaga, a nie efekt uboczny
`--core-cutoff`.

Wartości:
- pominięta: pasmo rdzenia ograniczone do 20 kHz (stock).
- `--uncap-bandwidth`: ograniczenie zdjęte, `--core-cutoff` wolny aż do `sr/2`.

Przykład w kontekście:

    fdkaac -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 200 \
        -o out.m4a in96k.wav

Współpraca i konflikty: wymaga `--core-cutoff`. Pod SBR `-w` jest ignorowane, a działa
`--core-cutoff`, więc ta para jest też sposobem na rozszerzenie rdzenia siedzącego pod
SBR. Naturalnie łączy się z `--ath-scale` poniżej 256, gdy jest zapas bitrate'u, bo nie
ma sensu kodować 40 kHz pasma przy progach strojonych pod niedostatek.

Zmierzone: wejście 96 kHz, AAC-LC 400 kbps, szum szerokopasmowy. Bez flagi
`--core-cutoff 40000` daje `20000 Hz` w `--verbose` i praktycznie zerową energię powyżej
20 kHz. Z flagą `--verbose` raportuje `40000 Hz`, a zdekodowany plik niesie około 10
procent swojej energii w 20..24 kHz, około 20 procent w 24..32 kHz i około 20 procent w
32..44 kHz. Ten koder nie ma w repozytorium sygnału testowego 96 kHz, więc należy to
traktować jako pomiar wcześniejszy, a nie taki, który da się odtworzyć z dostarczonego
materiału.

### `--unlock-bitrate`

**Zdejmuje dolne progi bitrate'u, których pilnuje każdy profil, i sprawia, że `-b` to
dosłownie bity na sekundę.**

- Dotyczy: każdego profilu. Zdejmowane progi różnią się w zależności od profilu, patrz
  niżej.
- Wartości: flaga; domyślnie: wyłączona (wszystkie progi obowiązują)
- Wpisywanie: flaga

Stock FDK odmawia kodowania poniżej pewnego progu, a próg ten jest inny w każdym profilu
i często bywa mylony z ograniczeniem formatu. Nie jest nim. Rozpowszechnione twierdzenie,
że HE-AAC nie potrafi zejść poniżej 16 kbps, opisuje najniższy wiersz tabeli strojenia SBR
dla 48 kHz stereo i nic więcej. Gdy poprosić o mniej, stock FDK po cichu podnosi bitrate
do tego wiersza.

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
- regułę, że ramka niemogąca pomieścić własnych danych pobocznych przerywa kodowanie.
  Takie ramki zamiast tego przestrzeliwują, a rezerwuar wchłania to w kolejnych ramkach.

Zostaje nagłówek transportu, 56 bitów na ramkę dla ADTS, oraz górny pułap 6144 bitów na
kanał, którego nie rusza żaden przełącznik. Poniżej oszacowanej zawartości SBR, czyli
około 2,5 kbps na kanał plus parametric stereo, rdzeń nie miałby już żadnych bitów i koder
zatrzymuje się z czystym „encoder initialization failed”, a nie z asercją.

Flaga zdejmuje progi. Nie gwarantuje żadnego konkretnego bitrate'u. Przy takich
wartościach gęste fragmenty rutynowo wychodzą powyżej tego, o co poproszono, bo same
nagłówki i dane poboczne kosztują więcej niż budżet, więc wynik należy oceniać po
rozmiarze pliku, a nie po liczbie w `--verbose`.

Uwaga na zmianę znaczenia `-b`. W trybie unlock liczba to dosłownie bity na sekundę.
`-b 6000` to 6000 bps. `-b 6` to 6 bps i nie zakoduje nic sensownego, a `-b 1152` wpisane
w przekonaniu, że znaczy 1152 kbps, daje 1152 bps. Zawsze trzeba tu wpisywać pełną liczbę
bitów.

Wartości:
- pominięta: wszystkie progi obowiązują, `-b` czytane ze skrótem kilobitowym (stock).
- `--unlock-bitrate`: progi zdjęte, `-b` to dosłowne bps.

Przykład w kontekście:

    fdkaac -p 29 -b 6000 --unlock-bitrate -f 2 -o out.aac in48k.wav

Współpraca i konflikty: `--vbr-reservoir` i `--peak-bitrate` stają się tu ciekawsze, bo
przestrzelenie jest w tym trybie normą, a rezerwuar jest tym, co za nie płaci.
Przy `--dab` konwencja kilobitowa DAB+ dla `-b` zostaje zastąpiona dosłownymi bps. Nic w
grupie D ani F nie przywraca progów.

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
samych nagłówkach i danych pobocznych. Kolumnę ffmpeg 6.1 trzeba czytać ostrożnie:
raportuje te same błędy na koderze stock w tych trybach, jeden `noise_facs_q is invalid`
na HE-AAC v2 przy 12..14 kbps, co mieści się w tabelach stock, oraz wiele błędów na każdym
strumieniu LOAS z ELD albo LD przy dowolnym bitrate. ffmpeg nie implementuje LD i ELD w
pełni, więc te strumienie należy oceniać dekoderem Fraunhofera.

## B. Kodowanie stereo

Dwa kanały na mniejszą liczbę bitów zamieniają w tym koderze trzy różne maszyny, które
pracują na różnych częściach sygnału i mają różny koszt. Zanim sięgniesz po którykolwiek
przełącznik z tej grupy, ustal jasno, o której z tych trzech maszyn właściwie mówisz.

**MS (mid/side)** to bezstratny obrót pary kanałów. Zamiast lewego i prawego koder
zapisuje w danym pasmie współczynnika skali kanał mid (suma, L+R) i kanał side (różnica,
L-R). Nic nie jest wyrzucane: dekoder obraca z powrotem i dostaje dokładnie L/R. Przy
skorelowanej muzyce sygnał side jest mały, więc kwantyzuje się do znacznie mniejszej
liczby bitów niż drugi pełny kanał. MS to decyzja o ekonomii bitów, a nie o dźwięku, i
podejmowana jest osobno dla każdego pasma i każdej ramki. Dostępne w rdzeniu AAC, czyli
w LC, LD, ELD oraz w rdzeniu pod HE-AAC, dopóki ten rdzeń jest jeszcze stereo.

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
używalny obraz stereo tam, gdzie prawdziwe kodowanie dwukanałowe przymierałoby głodem.
PS nie jest ulepszeniem MS ani IS, lecz je zastępuje: przy `-p 29` nie ma kanału side,
któremu można by przesunąć budżet, ani pasm intensity, które można by rozmieścić, więc
każdy przełącznik `--ms*` i `--is*` z tej grupy jest bezczynny. I odwrotnie, `--ps*` nie
robi nic poza HE-AAC v2.

W praktyce: od 96 kbps w górę MS wykonuje prawie całą pracę i stroi się podział bitów
między mid i side (`--side-bias`, `--side-knee`). Między mniej więcej 40 a 80 kbps
intensity stereo zaczyna zarabiać na siebie w górnych pasmach (`--is-aggression`, a potem
przełączniki rozmieszczenia `--is*`). Poniżej około 32 kbps w stereo rozsądną drogą są
HE-AAC v2 i parametry PS, a najciekawsze przełączniki to tam te od rozdzielczości (`--ps-env-reduce`,
`--ps-bands`).

**Jak czytać numery SFB.** Każdy przełącznik pasmowy w tej grupie liczy pasma
współczynnika skali od `0` na DOLE widma. Pasmo 0 to bas, najwyższy numer to szczyt
kodowanego zakresu. Ile pasm istnieje, zależy od profilu i częstotliwości próbkowania:
przy LC 48 kHz stereo `--verbose` wypisuje `active SFBs (long): 49`, więc dozwolone
numery pasm to 0..48. Zawsze przeczytaj tę linię, zanim ustawisz zakres, bo liczba zmienia
się wraz z konfiguracją, a numer zapamiętany z innego kodowania może wypadać całkiem poza
widmo. Bloki krótkie są numerowane tak samo, od `0` wewnątrz każdej grupy okien, a ramka
krótka ma znacznie mniej pasm (około 14 przy 44,1 i 48 kHz); zakres, który sięga powyżej
liczby pasm krótkich, po prostu zostawia ramki krótkie w zwykłym L/R.

**Dlaczego zakres powyżej kodowanego pasma nie robi nic.** Liczba SFB to pełna tabela dla
profilu, ale tylko pasma do kodowanego pasma cokolwiek niosą. Pasma powyżej częstotliwości
odcięcia są zerowe, a decyzja stereo o pasmie zer nie zmienia ani jednego bitu. To
prawdziwa pułapka przy przykładach z górnym pasmem: przy domyślnym pasmie LC około
15,75 kHz `--msbands-lo 44 --msbands-hi 48` daje dokładnie ten sam strumień co
`--msmask 0`, bo pasma 44..48 leżą powyżej kodowanego pasma, a MS jest w związku z tym
wyłączone wszędzie tam, gdzie ma to znaczenie. Otwórz pasmo siłą przez `-w 20000` i te
dwa ustawienia przestaną być równe. Sprawdzaj więc swój zakres wobec kodowanego pasma, a
nie tylko wobec liczby SFB.

Wszystkie przełączniki w tej grupie są wyłączone, dopóki się ich nie poda. Wyłączony
znaczy stock (zachowanie niezmienionego FDK): wartownik `-1`, a dla `--side-bias`,
`--side-knee` i `--mask-slope` wartość `0`, zostawia własną decyzję FDK w spokoju.
`--side-bias`, `--side-knee` i `--mask-slope` przyjmują liczby dziesiętne, z `.` albo `,`
i do 3 miejsc po przecinku; pozostałe to liczby całkowite i odrzucają części dziesiętne.

### MS

MS nic nie kosztuje w jakości dźwięku, więc nie ma tu kompromisu typu „MS psuje obraz”,
którym trzeba by zarządzać. Zarządzać można tym, gdzie MS jest dozwolone i jak budżet
bitów dzieli się między kanał mid i kanał side, gdy pasmo już jest w MS. Dwa z tych
przełączników to prawdziwa kontrola (`--msmask` oraz zakres pasm), dwa to szczere
ciekawostki (`--ms-bias`, `--ms-precision`), a trzy to przełączniki jakości, które wynagradzają
czas poświęcony na naukę (`--side-bias`, `--side-knee`, `--mask-slope`).

#### `--msmask <n>`

**Wymuś decyzję mid/side globalnie, zamiast pozwolić koderowi wybierać osobno dla każdego pasma.**

- Dotyczy: rdzeń AAC w stereo (LC, LD, ELD oraz rdzeń stereo pod HE-AAC). Brak działania
  w HE-AAC v2, gdzie rdzeń jest mono.
- Wartości: `-1` auto (domyślnie, FDK decyduje dla każdego pasma), `0` wyłączone (czyste
  L/R wszędzie), `1` włączone (MS w każdym pasmie); domyślnie `-1`
- Wpisywanie: liczba całkowita

Koder normalnie testuje każde pasmo na oba sposoby i zostawia to, które pakuje się
mniejszym kosztem. To jest blisko optimum dla rozmiaru pliku, a ponieważ przekształcenie
jest bezstratne, jest też blisko nieistotne dla dźwięku. Mimo to dwie sytuacje sprawiają,
że wymuszone ustawienia warto mieć pod ręką.

`--msmask 0` gwarantuje, że koder nigdy nie zmiesza kanałów. Szum kwantyzacji
zostaje wtedy w tym kanale, który go wywołał, zamiast być dzielony między mid i side i
rozprowadzany na oba wyjścia przy rekonstrukcji. To ma znaczenie, jeśli plik pójdzie
później przez kasowanie środka, usuwanie wokalu albo jakiekolwiek inne odejmowanie
jednego kanału od drugiego: przy czystym L/R pozostałość to materiał plus jego
własny szum, przy MS to materiał plus szum dopasowany do sumy i różnicy. Kosztuje to
bity, a przy skorelowanej muzyce kosztuje prawdziwe bity.

`--msmask 1` to przeciwna skrajność, MS wymuszone nawet tam, gdzie koder uznał L/R za
tańsze. Przydatne jako punkt odniesienia i przy materiale mocno skorelowanym, trochę
marnotrawne przy materiale o naprawdę niezależnych kanałach, na przykład dwóch
niezwiązanych źródłach mono rozpanoramowanych na skraje.

Wartości:
- `-1`: stock. Decyzja osobno dla każdego pasma, z własnego porównania kosztów kodera.
- `0`: czyste L/R w każdym pasmie. Nic nie jest mieszane między kanałami.
- `1`: MS w każdym pasmie.

```
fdkaac -p 2 -b 128000 --msmask 0 in.wav -o lr.m4a
```

Współpraca i konflikty: `--msmask 0` czyni bezczynną całą rodzinę mid/side, w tym
`--msbands`, `--side-bias`, `--side-knee` i `--mid-bias`, bo żadne pasmo nie jest w MS.
`--msbands`/`--msbands-lo`/`--msbands-hi` zwężają obszar, w którym wybrana maska może
obowiązywać.

Zmierzone: na dwusekundowej próbce różnica między `--msmask 0` a `--msmask 1` to około
900 bajtów ładunku, co ustawia całą decyzję MS w odpowiedniej perspektywie obok
`--ms-bias` z jego około 2 bajtami.

#### `--msbands <n>`

**Dopuść MS tylko w N najniższych pasmach współczynnika skali, a wszystko powyżej zostaw w L/R.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo `0..N`, gdzie N to liczba aktywnych SFB; `n`
  oznacza, że pasma `0..n-1` mogą użyć MS
- Wpisywanie: liczba całkowita

To jest postać „od dołu aż do” ograniczenia pasmowego. Liczy się zawsze od basu w górę,
więc `--msbands 6` oznacza, że pasma od 0 do 5 mogą być kodowane mid/side, a pasma 6 i
wyższe zostają wyłącznie w L/R. `--msbands 0` nie dopuszcza MS nigdzie, co jest tym samym
co `--msmask 0`.

Po co w ogóle ograniczać MS, skoro jest bezstratne? Bo podział bitów idzie za maską.
Wewnątrz pasm MS koder przydziela bity wobec progów mid i side, a kanał side danego pasma
jest tylko tak dobry, jak pozwalają na to bity z budżetu. Trzymanie szczytu widma w L/R
utrzymuje tam separację kanałów niezależnie od tych negocjacji, co jest obronnym wyborem
przy materiale, którego góra naprawdę jest nieskorelowana. Dobrze łączy się to też z
eksperymentami na górnym pasmie: L/R na górze i MS nisko to inny rozkład ryzyka niż
własny wybór kodera dla każdego pasma.

Różnica wobec `--msbands-lo`/`--msbands-hi` to tylko kształt obszaru: ten przełącznik
zawsze startuje od pasma 0, para może startować gdziekolwiek.

Przykłady:
- Łagodne ograniczenie: `--msbands 40`. Tylko najwyższe pasma są odepchnięte do L/R; przy
  większości materiału strumień prawie się nie rusza, bo te pasma często niosły niewiele.
- Mocne ograniczenie: `--msbands 12`. MS zamknięte mniej więcej w basie i niskim środku.
  Oczekuj mierzalnie większego pliku przy tym samym celu jakościowym albo grubszej
  kwantyzacji przy tym samym bitrate'ie.
- Przesadzone ograniczenie: `--msbands 2`. MS praktycznie na niczym. Karykatura: płacisz
  niemal pełną cenę niezależnego stereo, a przy tym nadal sygnalizujesz MS w dwóch
  pasmach.

Współpraca i konflikty: równoważne z `--msbands-lo 0 --msbands-hi n-1` (`--msbands 6` i
`--msbands-lo 0 --msbands-hi 5` dają ten sam strumień). Bezczynne przy `--msmask 0`.
Limit powyżej liczby aktywnych SFB jest tym samym co brak limitu.

#### `--msbands-lo <n>`

**Najniższe pasmo współczynnika skali w zakresie, w którym MS jest dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  używaj w parze z `--msbands-hi`
- Wpisywanie: liczba całkowita

Razem z `--msbands-hi` definiuje to okno od..do: MS może być użyte w pasmach `lo..hi`
włącznie i nigdzie więcej. W odróżnieniu od `--msbands` okno nie musi dotykać dołu widma,
a to jedyny sposób, żeby umieścić MS wyłącznie wysoko, na przykład po to, by koder scalał
skorelowany szum i powietrze na górze, podczas gdy bas i środek zostają w pełni
niezależnym L/R.

Podaj obie połówki pary. Samo `--msbands-lo` zostawia górną krawędź nieustawioną, a to
nie jest zakres.

Przykłady:
- Łagodnie, okno startujące nisko: `--msbands-lo 2 --msbands-hi 48`. Prawie wszystko może
  użyć MS, wykluczone są dwa najniższe pasma. Rozmiar blisko stock.
- Mocno, okno w środku: `--msbands-lo 10 --msbands-hi 30`. MS tylko w zakresie środka.
  Bas i góra są w L/R, więc plik rośnie, a góra zachowuje pełną separację.
- Przesadnie, okno na samej górze: `--msbands-lo 44 --msbands-hi 48` z `-w 20000`. MS na
  pięciu pasmach samego powietrza. Karykatura funkcji zakresu pasm i najwyraźniejszy
  pokaz tego, dlaczego kodowane pasmo ma znaczenie: bez `-w 20000` te pasma leżą powyżej
  domyślnego odcięcia 15,75 kHz i ustawienie degeneruje się do `--msmask 0`.

Współpraca i konflikty: trzeba to czytać wobec `active SFBs` z `--verbose` oraz wobec
kodowanego pasma. Bezczynne przy `--msmask 0`. Nakłada się na `--msbands`, które jest tą
samą funkcją zakotwiczoną w pasmie 0.

#### `--msbands-hi <n>`

**Najwyższe pasmo współczynnika skali, włącznie, w zakresie, w którym MS jest dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w granicach liczby aktywnych SFB;
  używaj w parze z `--msbands-lo`
- Wpisywanie: liczba całkowita

Górna krawędź opisanego wyżej okna od..do, i to włącznie: `--msbands-hi 5` obejmuje pasmo
5. Pasma powyżej idą w zwykłe L/R.

W blokach krótkich numer oznacza ten sam obszar częstotliwości, liczony od `0` wewnątrz
każdej grupy okien, a ramki krótkie mają tylko około 14 pasm przy 44,1 i 48 kHz. Zakres,
którego górna krawędź leży powyżej liczby pasm krótkich, zostawia ramki krótkie w L/R,
a nie kończy się błędem.

Przykłady:
- Łagodnie: `--msbands-lo 0 --msbands-hi 44`. Cztery najwyższe pasma wykluczone z MS.
  Mała zmiana.
- Mocno: `--msbands-lo 0 --msbands-hi 20`. MS do mniej więcej obszaru obecności, powyżej
  L/R.
- Przesadnie: `--msbands-lo 0 --msbands-hi 0`. MS w dokładnie jednym pasmie, w basie.
  Funkcjonalnie niezależne stereo z jednym pasmem buchalterii.

Współpraca i konflikty: `--msbands-lo` to druga połówka; bez niego nie ma zakresu.
Wartość powyżej liczby aktywnych SFB jest tym samym co brak górnego limitu. Bezczynne
przy `--msmask 0`.

#### `--ms-bias <n>`

**Przechyl porównanie kosztów L/R kontra MS w stronę MS.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie, porównanie stock), `0` też stock, `1..255` w Q8,
  gdzie `256` oznaczałoby 1,0 w wewnętrznych jednostkach logarytmicznych FDK, więc 128 to
  +0,5
- Wpisywanie: liczba całkowita

Bądźmy przy tym szczerzy: to binarne szturchnięcie przebrane za suwak i najsłabszy
przełącznik w tej grupie. Pomiar mówi to wprost. `0` daje strumień stock; każda wartość
od 1 do 255 daje jeden i ten sam inny strumień. Nie ma gradientu, po którym można by
jechać, więc nie ma czego stroić.

Powód jest strukturalny. MS jest bezstratne, więc ten próg nie decyduje o tym, jak pasmo
brzmi, a tylko o tym, które z dwóch kodowań taniej się zapisuje. Własne porównanie kodera
jest już blisko optimum, więc przesunięcie linii decyzyjnej może przerzucić tylko te
pasma, które były niemal na remis, a pasma niemal na remis kosztują niemal tyle samo w
jednym i w drugim wariancie. Wypadkowy efekt na rozmiarze pliku to części procenta, a
korelacja kanałów po dekodowaniu rusza się na czwartym miejscu po przecinku.

Używaj go po to, żeby potwierdzić, że punkt decyzyjny istnieje, a nie żeby kształtować
dźwięk. Prawdziwą kontrolę nad MS porządnie zapewniają `--msmask 0` albo `--msmask 1`
oraz przełączniki zakresu pasm, a `--side-bias` to przełącznik, który faktycznie zmienia to,
co słyszysz.

Wartości:
- `-1` albo `0`: porównanie kosztów stock.
- `1..255`: jeden jedyny alternatywny strumień, nieco chętniejszy do wybierania MS.

```
fdkaac -p 2 -b 128000 --ms-bias 128 in.wav -o biased.m4a
```

Współpraca i konflikty: bez sensu razem z `--msmask 0` albo `--msmask 1`, które usuwają
decyzję w całości. Dla każdego praktycznego celu przesłonięte przez `--msmask` i
`--side-bias`.

Zmierzone: mix48 przy 128 kbps oraz tonal48/sparse48 przy 96 kbps, wartości 1..255 dają
wszystkie identyczny strumień, który różni się jedynie od `0`. Efekt na rozmiarze poniżej
0,1 procenta, rzędu 2 bajtów na dwusekundowej próbce.

#### `--ms-precision <n>`

**Przeskaluj precyzję kwantyzacji pasm MS jako całości, mid i side razem.**

- Dotyczy: rdzeń AAC w stereo, tylko pasma MS. Brak działania w HE-AAC v2.
- Wartości: mnożnik Q8 od `256` (bez zmiany) w górę, `384` to około 1,5x, `512` około 2x,
  bez górnego limitu; wartości poniżej 256 są odrzucane; `-1` wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

To tutaj najbliższa rzecz do globalnej dźwigni jakości w stylu LAME dla pary stereo:
podnieś ją i progi w pasmach MS zacieśniają się, co oznacza płytsze dziury w widmie i
drobniejszą kwantyzację naraz w mid i w side. Jest to narzędzie tępe, bo rusza mid i side
w tym samym kierunku i o ten sam współczynnik, więc nie potrafi przesunąć równowagi
między nimi, a to zwykle jest to, czego się naprawdę chce.

Jego zasięg jest też ograniczony na dwa sposoby, które warto znać, zanim się na niego
poświęci czas. Powyżej mniej więcej 600 do 800 progi wpadają we własną twardą podłogę FDK
i dalsze podnoszenie przestaje cokolwiek dawać. A przy CBR budżet bitów nie rośnie, więc
zacieśniony próg w pasmach MS przenosi głównie bity skądś indziej z tej samej ramki,
zamiast dodawać szczegół. Efekt spłaszcza się w przetasowanie.

Gdy celem jest jakość stereo, sięgnij raczej po `--side-bias`: działa on osobno na kanał,
w tej decyzji, która faktycznie porzuca albo zachowuje pasmo side.

Przykłady:
- Łagodnie: `--ms-precision 320`. Nieco ciaśniejsze progi MS, mniej więcej 1,25x. Mały
  spadek liczby wyzerowanych pasm wewnątrz MS.
- Mocno: `--ms-precision 512`. Około 2x. Wyraźnie mniej dziur w pasmach MS; przy CBR
  oczekuj, że zysk zostanie opłacony gdzie indziej w ramce.
- Przesadnie: `--ms-precision 2048`. Głęboko w obszarze nasycenia, gdzie podłoga progu
  już wzięła władzę. Karykatura: liczba wygląda dramatycznie, a dźwięk przestaje za nią
  iść.
Drugiego kierunku nie ma: ten przełącznik tylko zacieśnia. Aby celowo pogrubić pasma MS,
podnieś próg side ujemnym `--side-bias` albo próg mid przez `--mid-bias`.

Współpraca i konflikty: bezczynne przy `--msmask 0`. Nakłada się na `--mid-bias` i
`--side-bias`, które ruszają po jednym kanale naraz; jeśli używasz tych dwóch, ten zostaw
domyślny.

Pod maską: mnożnik Q8 nakładany na progi pasm MS, stąd 256 oznaczające 1,00.

#### `--mid-bias <n>`

**Celowo podnieś próg kanału mid, żeby uwolnione bity mogły pójść do side.**

- Dotyczy: rdzeń AAC w stereo, pasma MS. Brak działania w HE-AAC v2.
- Wartości: Q8, `256` = wyłączone, powyżej 256 podnosi próg mid, bez górnego limitu; `-1`
  wyłączone (domyślnie)
- Wpisywanie: liczba całkowita

Po obrocie MS kanał mid niesie sumę, a to przy normalnej muzyce większość głośności i
większość informacji, której ludzie słuchają. Ten przełącznik celowo podnosi jego próg
maskowania, co czyni mid tańszym w kodowaniu i zostawia więcej ze stałego budżetu dla
side. To uprawniony sposób kupienia szerokości stereo, a przy tym ryzykowny, bo płacisz
tym kanałem, który niesie melodię.

Używaj oszczędnie, a do tej samej pracy wybieraj raczej `--side-bias`. Oba ciągną ten sam
budżet mid/side, ale `--side-bias` ciągnie od strony side, gdzie pomiar jest czystszy i
gdzie efekt przedobrzenia objawia się jako węższy obraz, a nie jako bardziej mętny wokal
prowadzący.

Przykłady:
- Łagodnie: `--mid-bias 288`. Próg mid podniesiony nieznacznie, trochę zapasu przekazane
  do side. Trudno się przyczepić przy gęstym materiale.
- Mocno: `--mid-bias 384`. Około 1,5x. Side dostaje realny udział; mid zaczyna tracić
  definicję przy niskim bitrate'ie.
- Przesadnie: `--mid-bias 1024`. Mid jest oddany, żeby wykarmić sygnał różnicowy.
  Karykatura: szeroki, wydrążony obraz z miękkim środkiem, czyli coś, czego przy
  kodowaniu muzyki prawie nikt nie chce.

Współpraca i konflikty: bezczynne przy `--msmask 0` i bez sensu w połączeniu z dużym
dodatnim `--side-bias`, bo oba wydają ten sam budżet z przeciwnych stron. Nakłada się na
`--ms-precision`, które zamiast tego rusza mid i side razem.

#### `--side-bias <dB>`

**Przesuń próg maskowania kanału side w pasmach MS: główna dźwignia szerokości stereo
kontra jakość środka.**

- Dotyczy: rdzeń AAC w stereo, pasma MS. Brak działania w HE-AAC v2, a przy `--msmask 0`
  nie ma tu nic do roboty.
- Wartości: `-24.0..50.0` dB, `0` = wyłączone (domyślnie, próg stock)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

To ten przełącznik, którego warto się w tej grupie nauczyć. Koder decyduje, osobno dla
każdego pasma współczynnika skali, czy sygnał side jest wart kodowania, porównując jego
energię z progiem maskowania; poniżej progu pasmo zostaje wyzerowane. Od tego jednego
porównania zależy szerokość stereo. Przesunięcie progu przesuwa tę linię.

Wartość dodatnia obniża efektywną poprzeczkę dla kanału side: mniej pasm side jest
zerowanych, te, które przeżyją, są kwantyzowane drobniej, a to, co słyszysz, to czystsza
szerokość, ogony pogłosu, które wybrzmiewają, a nie urywają się, i ambience, które
zostaje na miejscu, a nie zapada się do środka. Nie jest to darmowe. Budżet bitów jest
stały, więc te bity biorą się z kanału mid, a przy niskim bitrate'ie da się usłyszeć, jak
środek je oddaje. To zamierzony kompromis, a nie wada. Pomiar
poniżej pokazuje dokładnie ten kształt: błąd side poprawia się równo od +1 do +9, podczas
gdy błąd mid na tym samym odcinku równo się pogarsza.

Wartość ujemna działa odwrotnie i podnosi poprzeczkę, więc pasma side wypadają, a obraz
zwęża się w stronę mono. To celowe narzędzie do pracy przy bardzo niskim bitrate'ie albo
do efektu, i ma jedną twardą cechę strukturalną, o której trzeba wiedzieć: nie zwęża
gładko do samego dołu.

**Skok przy około -0,97 dB.** Od 0 do -0,95 dB błąd side rusza się gładko, czyli
następuje łagodne zwężanie, którego można się spodziewać. Przy około -0,97 dB podniesiony
próg side przekracza linię minimalnego SNR, którą koder trzyma w swojej logice unikania
dziur, i większość pasm side wypada naraz. Dalej przełącznik nic już nie zmienia: -1, -4
i -12 dają ten sam wynik, bo side jest już odcięty. To właściwość kodera, prawdziwa
nieciągłość w jego łańcuchu decyzyjnym, a nie artefakt zaokrąglania w linii poleceń:
liczby dziesiętne są czytane do 3 miejsc, a wpisana wartość jest tą użytą. Praktyczna
reguła wynika z tego bezpośrednio. Do łagodnego zwężania zostań w przedziale 0 do -0,95,
a wszystko przy -0,97 i dalej traktuj jako jedno ustawienie, które znaczy „porzuć side”.

W kierunku dodatnim użyteczny odcinek kończy się około +9. Dalej pomiar pokazuje, że oba
kanały się pogarszają: side przestaje się poprawiać, a mid nadal się degraduje, co jest
złym interesem naraz w obu kierunkach.

Przykłady:
- Łagodnie, więcej bitów do side: `--side-bias 1`. Odrobinę więcej szczegółu side, mid
  praktycznie nietknięty. Bezpieczne na wszystkim.
- Mocno, więcej bitów do side: `--side-bias 6`. Wyraźnie szerzej i przestrzenniej, a mid
  zaczyna za to płacić przy niskim bitrate'ie. To jest normalna wartość robocza.
- Przesadnie, więcej bitów do side: `--side-bias 24`. Za użytecznym odcinkiem. Side nie
  zyskuje już nic, a mid jest mierzalnie uszkodzony, więc obraz staje się szeroki, a
  środek chudy. Karykatura, która pokazuje, gdzie poszedł budżet.
- Łagodnie, zwężanie: `--side-bias -0,3`. Nieco ciaśniejszy obraz, kilka brzegowych pasm
  side oddanych. Precyzyjna kontrola jest możliwa właśnie w tym obszarze.
- Mocno, zwężanie: `--side-bias -0,9`. Zauważalnie węziej, jeszcze wewnątrz obszaru
  gładkiego i jeszcze tuż przed skokiem.
- Przesadnie, zwężanie: `--side-bias -4`. Za skokiem przy -0,97 dB, więc side po prostu
  zniknął, a wynik jest blisko mono. Karykatura, i to identyczna z -1 albo -12: nie ma już
  czego usuwać.

Współpraca i konflikty: `--side-knee` kształtuje krawędź tej samej decyzji i łączy się z
tym czysto. `--mid-bias` naciska ten sam budżet od strony mid, więc używanie obu mocno to
liczenie podwójne. `--msmask 0` usuwa kanał side w całości i czyni ten przełącznik
bezczynnym. `--msbands`/`--msbands-lo`/`--msbands-hi` ograniczają, których pasm może on
dotknąć.

Zmierzone: dense48, LC stereo 40 kbps, dekoder Fraunhofera, energia błędu względem
źródła w każdym kanale. Pasmo domyślne (9,75 kHz): błąd side -0,53, -0,58, -0,62, -0,63,
-0,66 dB przy 0, +1, +3, +6, +9, a błąd mid w tym czasie -8,62, -8,53, -8,27, -7,08,
-6,43 dB. Przy +24 side znów się pogarsza (-0,42 dB), a mid traci dalej (-5,81 dB).
Kierunek ujemny przy paśmie wymuszonym na 18 kHz (`-w 18000`): błąd side -1,13 dB przy 0,
-1,03 przy -0,5, -0,93 przy -0,9, -0,92 przy -0,95, potem -0,31 przy -0,97 i tyle samo przy
-1, -4 i -12. To, w której wielkości widać skok, zależy od materiału i pasma: przy paśmie
domyślnym na tym samym sygnale skok pojawia się w błędzie mid (z -8,72 na -9,04 dB przy
-0,97).

#### `--side-cutoff <Hz>`

**Od tej częstotliwości w górę koduj każde pasmo stereo jako czysty mid.**

- Dotyczy: rdzeń AAC w stereo (LC, LD, ELD). Domyślnie wyłączone.
- Wartości: `1000..24000` Hz, zaokrąglane do pasma współczynnika skali, które zaczyna się na tej częstotliwości lub wyżej

Powyżej granicy sygnał side jest porzucany, a pasmo zapisywane tylko jako mid, z tanią maską
MS. Własna decyzja FDK o intensity stereo jest tam kasowana, bo czysty mid jest tańszy (IS
potrzebuje pozycji i blokuje maskę MS zapisywaną jednym kodem); wymuszone IS z
`--is-force-lo/-hi` nadal wygrywa. Pasma, które oba kanały kodują jako PNS, zostają bez zmian.
Poniżej granicy stereo jest nietknięte. `--verbose` pokazuje pasmo, od którego działa.

Przykłady (akustyczna muzyka stereo):
- Łagodnie: `-b 48000 --side-cutoff 8000`. Informacje boczne z 238 do 235 bitów na ramkę, mid trochę czystszy.
- Mocno: `-b 48000 --side-cutoff 5000`. Informacje boczne 222 bity, błąd mid o 0,7 dB mniejszy, side
  powyżej 5 kHz niżej o około 27 do 31 dB.
- Przesadnie: `-b 48000 --side-cutoff 1000`. Praktycznie mono ze stereofonicznym basem.

Przy 14 kbps zysk jest mały, bo FDK i tak zeruje tam prawie cały side; przy tej prędkości
większą dźwignią jest `--is-admit 0`.

#### `--is-admit <x>`

**Przesuń bramkę FDK, która decyduje, czy intensity stereo w ogóle wolno użyć.**

- Dotyczy: rdzeń AAC w stereo. Stock `5`.
- Wartości: `0.0..20.0`; `0` = nigdy. `--is-aggression` nadal otwiera bramkę.

FDK dopuszcza intensity stereo tylko wtedy, gdy bitrate podzielony przez pasmo rdzenia (bity
na sekundę na Hz) jest mniejszy niż 5. Przy bardzo niskich prędkościach IS jest drogie:
wymusza maskę MS pasmo po paśmie (około 22 bity na ramkę) i dokłada pozycje IS. Zmierzone przy
14 kbps stereo na gęstym materiale: `--is-admit 0` zmniejsza udział wyzerowanych pasm z 85% do 70%.

#### `--side-knee <dB>`

**Nadaj każdemu pasmu side przy krawędzi kodowane/wyzerowane precyzję zależną od jego własnego poziomu.**

- Dotyczy: rdzeń AAC w stereo, pasma MS. Brak działania w HE-AAC v2, a przy `--msmask 0`
  nie ma tu nic do roboty.
- Wartości: `-24.0..50.0` dB, `0` = wyłączone (domyślnie, zachowanie stock)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

`--side-bias` przesuwa to, gdzie leży linia kodowane/wyzerowane. Ten przełącznik działa na
zapasie każdego pasma: o ile jego energia leży nad progiem maskowania, co decyduje, jak
dokładnie pasmo jest kodowane i czy w ogóle przetrwa. Wartość to przesunięcie dodawane do
tego zapasu, więc pasmo zachowuje precyzję należną jego prawdziwemu poziomowi, tylko
przesuniętą.

Potrzebne jest to przez jeden szczegół FDK. Gdy brakuje bitów, FDK nie wyrzuca słabego pasma
od razu: przypina je około 0,97 dB nad progiem i trzyma na najgrubszym kroku. W prawdziwej
muzyce około 10% pasm side ląduje na tej jednej wartości, niezależnie od ich rzeczywistego
poziomu. Przełącznik czyta zapas, którego pętla bitów chciała przed tym ratunkiem, więc
takie pasma znów rozkładają się według prawdziwego poziomu, zamiast ruszać się jednym
blokiem. Zmierzone na muzyce akustycznej przy 96 kbps: kodowanych pasm side na ramkę ubywa
równo z 30,2 (stock) przez 25,4 (-1), 22,2 (-3), 18,1 (-6), 10,5 (-12) do 1,1 (-24), bez skoku.

Wartość dodatnia dodaje precyzji: pasma do N dB pod progiem zostają, stopniowane według
tego, jak daleko pod nim były, a pasma przy krawędzi dostają do N dB dokładniej. Wartość
ujemna odbiera precyzję: najpierw odchodzą pasma najbliżej krawędzi, a bity idą do mid.
Przesunięcie działa w pełni przy krawędzi i wygasa wyżej, więc głośna, dobrze zakodowana
część side zostaje nietknięta (strefę wygasania ustawia `--side-knee-zone`).

Przykłady (akustyczna muzyka stereo, 96 kbps):
- Łagodnie, dokładniej: `--side-knee 2`. Dziury w side z 6,9% do 2,2%, błąd mid o 0,8 dB większy.
- Mocno, dokładniej: `--side-knee 6`. Dziury w side 0,6%, błąd side o 2,4 dB mniejszy, mid płaci 3,5 dB.
- Przesadnie, dokładniej: `--side-knee 24`. Więcej bitów idzie do side niż do mid. Karykatura.
- Łagodnie, grubiej: `--side-knee -1`. Odchodzą najsłabsze pasma side, około 5 z 37 na ramkę.
- Mocno, grubiej: `--side-knee -6`. Dziury w side 29%, błąd mid o 2,4 dB mniejszy.
- Przesadnie, grubiej: `--side-knee -24`. Side praktycznie znika; obraz mono z księgowością stereo.

Diagnostyczne przełączniki towarzyszące (mogą zniknąć w kolejnej wersji):
- `--side-knee-zone <x>`, `1.0..20.0`, stock `3`: przesunięcie działa w pełni do |knee| dB
  nad progiem i wygasa liniowo do zera przy x razy |knee| dB.
- `--side-knee-spread <dB>`, `0.1..24.0`, stock `1`: po stronie ujemnej, ile dB knee
  potrzeba, by w pełni rozłożyć uratowaną kupkę pasm według ich prawdziwego zapasu.
Oba w pomiarze zmieniały niewiele. Bez `--side-knee` nic nie robią.

Współpraca i konflikty: ortogonalne wobec `--side-bias` i zaprojektowane do używania razem
z nim; bias ustala, gdzie jest próg, kolano ustala, jak miękka jest jego krawędź.
Bezczynne przy `--msmask 0`.

Zmierzone: gęste stereo LC. Wartości ujemne nasycają się przy -6 dB (-6, -12 i -24 dają
identyczny strumień); wartości dodatnie zmieniają strumień w całym zakresie.

#### `--mask-slope <dB>`

**Przesuń, jak głęboko pod średnią ramki musi leżeć pasmo, żeby koder przestał się nim
przejmować.**

- Dotyczy: oba kanały, mid i side, w rdzeniu AAC. Brak działania w HE-AAC v2.
- Wartości: `-24.0..50.0` dB, `0` = wyłączone (domyślnie); odmienne wyniki tylko w
  przybliżeniu w zakresie -9.9..+9
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

FDK nosi w sobie heurystykę niemaskowania: gdy energia pasma współczynnika skali leży
daleko pod średnią energią ramki, w strojeniu stock więcej niż około 10 dB pod nią,
wymagany SNR dla tego pasma jest rozluźniany. Prościej: koder celowo głodzi bardzo ciche
pasma, żeby wydać bity na te głośne. Przez większość czasu jest to słuszne, bo pasmo
20 dB pod resztą ramki jest zwykle zamaskowane. Czasem nie jest, a tym, co zostaje
zagłodzone, jest ogon pogłosu, pomieszczenie albo wybrzmienie poprzedniej nuty.

Ten przełącznik przesuwa tę linię „jak głęboko pod średnią, zanim przestanę się
przejmować”, naraz dla obu kanałów. Wartość dodatnia ją podnosi, więc mniej pasm
kwalifikuje się jako pomijalne i cichy szczegół przeżywa. Ciche fragmenty, wybrzmienia i
ogony pogłosu zachowują więcej swojej struktury, a bity biorą się skądś indziej w ramce.
Wartość ujemna ją obniża, więc więcej pasm jest głodzonych, a wynik jest chudszy i
bardziej wydrążony, z większą liczbą bitów dla materiału głośnego.

Ustaw oczekiwania szczerze: to subtelna kontrola. Dotyka tylko najcichszych pasm w ramce,
więc przy materiale gęstym błąd po dekodowaniu prawie się nie rusza, a na naszej rzadkiej
próbce przesunęły go tylko duże wartości. Najbardziej warto jej próbować przy materiale
rzadkim, pogłosowym albo celowo cichym, a najmniej przy głośnym, gęstym miksie.

Zakres żywy jest węższy niż zakres przyjmowany. Odmienne wyniki pojawiają się w
przybliżeniu od -9,9 do +9. Przy -10 i niżej wynik przestaje się zmieniać, +10 do +40
zapada się w jeden strumień, a 45 do 50 w inny. Trzymaj swoje przykłady wewnątrz żywego
odcinka, gdzie liczba faktycznie odpowiada zachowaniu.

Przykłady:
- Łagodnie, ochrona cichych pasm: `--mask-slope 2`. Nieco mniej pasm zagłodzonych. Oczekuj
  dziesiątych części dB w pomiarze i minimalnie bardziej otwartego wybrzmienia.
- Mocno, ochrona cichych pasm: `--mask-slope 6`. Realna część cichych pasm zachowana.
  Najprzydatniejsze ustawienie przy materiale pogłosowym.
- Przesadnie, ochrona cichych pasm: `--mask-slope 9`. Szczyt żywego zakresu: niemal nic nie
  jest traktowane jako pomijalne, a bity są wydawane na treść, która bardzo prawdopodobnie
  jest zamaskowana. Karykatura i ostatnia wartość, która jeszcze odpowiada, bo +10 i
  powyżej zachowują się tak samo.
- Łagodnie, głodzenie cichych pasm: `--mask-slope -2`. Kilka cichych pasm więcej oddanych.
  Trochę chudziej.
- Mocno, głodzenie cichych pasm: `--mask-slope -6`. Ciche pasma wyraźnie zagłodzone, więcej
  bitów dla treści głośnej, bardziej wydrążone brzmienie przy materiale rzadkim.
- Przesadnie, głodzenie cichych pasm: `--mask-slope -9.5`. Dno żywego zakresu, cichy
  szczegół zdarty wszędzie, gdzie heurystyka sięgnie. Karykatura ustawienia chudego i
  podłoga tej kontroli, bo -10 i niżej przestają cokolwiek zmieniać.

Współpraca i konflikty: ta sama rodzina co `--side-bias`, ale kluczem jest energia wobec
średniej ramki, a nie próg side, i stosuje się to jednakowo do mid i do side. Używaj
jednego albo drugiego, póki uczysz się, co każde robi, a potem je łącz.

Zmierzone: mix48 przy 40 kbps daje odmienne strumienie tylko dla około -9,9..+9; -10 i
niżej zapada się w jeden strumień, +10..+40 w drugi, 45..50 w trzeci. Na sparse48 przy
40 kbps błąd po dekodowaniu prawie się nie rusza, przesuwają go tylko +12 i +24.

### Intensity stereo (IS)

Intensity stereo to ta stratna maszyna. W ciągu górnych pasm koder przestaje wysyłać dwa
widma i wysyła jedną obwiednię energii plus kierunek, a dekoder odbudowuje oba kanały z
tego jednego widma. Oszczędność jest duża, kosztem jest separacja stereo dokładnie w tych
pasmach, a czy ten koszt usłyszysz, zależy wyłącznie od tego, co tam żyje: skorelowana
kąpiel talerzy znosi to dobrze, mocno rozpanoramowany efekt albo szeroki pogłos nie.

Stock FDK jest z IS bardzo ostrożny. Jest nie jedna bramka, ale kilka, a pasmo jest
kodowane jako intensity tylko wtedy, gdy wszystkie przejdą naraz: reguła dopuszczenia po
bitrate'ie na pasmo, test korelacji między kanałami, minimalny ciąg przyległych pasm oraz
stabilny kierunek. Właśnie dlatego przesunięcie jednego progu tak często wygląda, jakby
nie robiło nic: kolejna bramka nadal blokuje. Samo rozluźnienie `--is-corr-thresh` na
przykład nie otwiera IS.

**Zacznij od `--is-aggression` z grupy A.** To konsumencki suwak dokładnie do tego
problemu: jedna wartość od 0 do 100, która rusza wszystkie bramki razem i spójnie. 0 to
stock, co w praktyce oznacza, że przy normalnych bitrate'ach IS prawie się nie włącza, a
100 to prawdziwe maksimum: bramka bitrate'u porzucona, wymóg korelacji rozluźniony,
minimalny ciąg zredukowany do jednego pasma i tolerancja kierunku poszerzona. Zacznij
około 30 do 50 i posłuchaj, a potem idź wyżej, jeśli oszczędność jest dla ciebie warta
więcej niż szerokość. Dopiero gdy wiesz, co robi ten suwak, sięgaj po przełączniki
poniżej, które przesłaniają to, co on ustawił.

#### `--is <n>`

**Włącz albo wyłącz intensity stereo globalnie, zamiast zostawiać to koderowi.**

- Dotyczy: rdzeń AAC w stereo (LC, LD, ELD oraz rdzeń stereo pod HE-AAC). Brak działania
  w HE-AAC v2.
- Wartości: `-1` auto (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

To przełącznik główny, i jest pozwoleniem, a nie rozkazem: `1` dopuszcza intensity stereo,
ale go nie rozmieszcza. Opisane wyżej bramki nadal decydują, które pasma naprawdę idą do
IS, więc samo `--is 1` przy materiale, który koder uznaje za nieodpowiedni, albo przy
bitrate'ie powyżej reguły dopuszczenia, może zostawić strumień bez zmian. Połącz go z
`--is-aggression`, gdy chcesz, żeby IS naprawdę się zadziało.

`--is 0` to użyteczny koniec tego przełącznika samego w sobie. Gwarantuje brak
jakiegokolwiek kodowania intensity, czyli to, czego chcesz, gdy separacja stereo w górnych
oktawach liczy się bardziej niż bity, albo gdy izolujesz efekt innego przełącznika i nie
chcesz, żeby IS ruszało się pod pomiarem.

Wartości:
- `-1`: stock. Koder decyduje, ostrożnie.
- `0`: intensity stereo nigdy nie użyte. Wszędzie pełne kodowanie L/R albo MS.
- `1`: intensity stereo dopuszczone, z zachowaniem zwykłych bramek.

```
fdkaac -p 2 -b 64000 --is 1 --is-aggression 40 in.wav -o is.m4a
```

Współpraca i konflikty: `--is-aggression` jest tym, co czyni `--is 1` widocznym.
`--is-force-lo`/`--is-force-hi` obchodzą bramki w całości i działają nawet bez tego
przełącznika. W pełni niezależne stereo to `--msmask 0 --is 0`.

#### `--isbands <n>`

**Ogranicz, ile pasm współczynnika skali może być kodowanych jako intensity stereo.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` bez limitu (domyślnie) albo liczba w granicach liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Pułap na całkowity zasięg kodowania intensity, wyrażony jako liczba pasm, a nie jako
pozycja w widmie. To hamulec na wypadek, gdy IS pomaga bitrate'owi, ale chcesz ograniczyć
szkody: niech koder sam wybiera, gdzie idzie intensity, ale nie pozwól mu pochłonąć
połowy widma.

Czytaj ten pułap wobec `active SFBs` z `--verbose`: przy LC 48 kHz stereo jest 49 pasm,
więc pułap 40 jest ledwie pułapem, a pułap 4 jest surowy. Pasma za pułapem są kodowane
normalnie.

Przykłady:
- Łagodny pułap: `--isbands 24`. Połowa widma nadal może pójść w intensity. Przy
  większości materiału zmienia to niewiele, bo IS rzadko sięga tak daleko.
- Mocny pułap: `--isbands 8`. Intensity utrzymane w krótkim ciągu. Przydatne przy wysokim
  `--is-aggression`, gdzie powstrzymuje suwak od ucieczki.
- Przesadzony pułap: `--isbands 1`. Jedno jedyne pasmo intensity. Karykatura: zachowujesz
  sygnalizację i tracisz praktycznie całą oszczędność.

Współpraca i konflikty: `--is-lo`/`--is-hi` ograniczają, gdzie IS może pójść, a ten
ogranicza, ile. Łączą się. Bezczynne, gdy IS nigdy się nie włącza, więc sprawdź najpierw
z `--is-aggression`.

#### `--is-min-sfbs <n>`

**Minimalny ciąg przyległych pasm, zanim intensity stereo może się zacząć.**

- Dotyczy: rdzeń AAC w stereo. Brak działania w HE-AAC v2.
- Wartości: `-1` domyślnie, co oznacza 6, albo liczba pasm
- Wpisywanie: liczba całkowita

Jedna z bramek. Koder odmawia rozpoczęcia kodowania intensity na odosobnionym pasmie czy
dwóch; chce ciągłego obszaru o długości co najmniej tylu pasm, z których wszystkie
przechodzą pozostałe testy. Uzasadnienie jest takie, że krótka łata intensity otoczona
pasmami kodowanymi normalnie to słyszalny szew, mały obszar, w którym obraz nagle zapada
się, a potem znów otwiera.

Obniż tę liczbę i IS łapie także krótkie fragmenty, co znaczy, że wyzwala się znacznie
częściej i na większej liczbie materiału, a ryzykiem są właśnie te szwy. Podnieś ją i IS
staje się rzadsze: kwalifikują się tylko szerokie, konsekwentnie skorelowane obszary, co
jest ustawieniem zachowawczym.

To przełącznik zaawansowany. `--is-aggression` już nią rusza w ramach swojego skoordynowanego
przebiegu, więc używaj jej tylko wtedy, gdy stroisz ręcznie przy agresji na 0, i pamiętaj,
że jawna wartość przesłania to, co ustawił suwak.

Przykłady:
- Łagodnie, więcej IS: `--is-min-sfbs 4`. Kwalifikują się nieco krótsze obszary. IS trochę
  częściej.
- Mocno, więcej IS: `--is-min-sfbs 2`. Krótkie fragmenty przyjmowane. Wyraźnie więcej
  kodowania intensity i pierwszy punkt, w którym szwy stają się prawdopodobne.
- Przesadnie, więcej IS: `--is-min-sfbs 1`. Każde pojedyncze pasmo może stać się
  intensity. Karykatura: intensity rozsypane po widmie po jednym pasmie.
- Łagodnie, mniej IS: `--is-min-sfbs 8`. Nieco rzadziej niż stock.
- Mocno, mniej IS: `--is-min-sfbs 16`. Kwalifikują się tylko szerokie obszary; przy
  większości materiału IS prawie nigdy nie odpala.
- Przesadnie, mniej IS: `--is-min-sfbs 40`. Tak długi ciąg praktycznie nie istnieje, więc
  jest to intensity stereo wyłączone kuchennymi drzwiami. Karykatura ustawienia
  zachowawczego, a `--is 0` mówi to samo szczerze.

Współpraca i konflikty: jedna bramka z kilku, więc samo jej obniżenie może nic nie
zmienić, jeśli bramka korelacji albo bitrate'u nadal blokuje. Faktycznie przesłonięte
przez `--is-force-lo`/`-hi`, które ją obchodzą. Wybieraj raczej `--is-aggression`.
#### `--is-corr-thresh <n>`

**Jak podobne muszą być oba kanały w pasmie, żeby intensity stereo było w ogóle dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = korelacja 1,0; `-1` domyślnie, czyli około 243, co oznacza
  mniej więcej 0,95
- Wpisywanie: liczba całkowita

Bramka korelacji, a przy tym ta, po którą sięga się pierwszą. Stock stawia ostry warunek:
oba kanały muszą być w pasmie podobne w jakichś 95 procentach, zanim koder rozważy
zastąpienie ich jedną obwiednią i kierunkiem. Bardzo niewiele prawdziwej muzyki jest tak
skorelowane w górnych pasmach, i to właśnie jest główny powód, dla którego IS wydaje się
domyślnie nieaktywne.

Kierunek jest przeciwny intuicji, więc przeczytaj to dwa razy: NIŻEJ znaczy WIĘCEJ
intensity. Obniżasz poprzeczkę, którą materiał ma przeskoczyć, więc przeskakuje ją więcej
pasm. 180 to około 0,70, czyli znacznie luźniej niż stock. Za nisko i zaczynasz kodować
naprawdę różne kanały jako jedno sterowane źródło, co słychać jako błędy kierunku i jako
zlepianie się góry obrazu.

To jedna z kilku bramek, a stąd praktyczny wniosek, który warto powiedzieć wprost: zmiana
samego tego przełącznika nie otwiera intensity stereo. Jeśli nadal blokuje reguła
dopuszczenia po bitrate'cie albo minimalna seria, strumień nie ruszy się niezależnie od
tego, co tu ustawisz.

Przykłady:
- Łagodnie, więcej IS: `--is-corr-thresh 230`. Około 0,90. Nieco luźniej niż stock.
- Mocno, więcej IS: `--is-corr-thresh 180`. Około 0,70. Kwalifikuje się istotnie więcej
  pasm; to typowy cel ręcznego strojenia.
- Przesadnie, więcej IS: `--is-corr-thresh 60`. Około 0,23, czyli przyjmowane są kanały
  ledwo ze sobą związane. Karykatura: licz się ze słyszalnymi błędami kierunku wszędzie,
  gdzie to zadziała.
- Łagodnie, mniej IS: `--is-corr-thresh 250`. Ciaśniej niż stock, IS trochę rzadsze.
- Mocno, mniej IS: `--is-corr-thresh 254`. Kwalifikują się tylko kanały niemal identyczne.
- Przesadnie, mniej IS: `--is-corr-thresh 256`. Wymagana korelacja idealna, której
  prawdziwy materiał w praktyce nigdy nie osiąga. Karykatura: intensity stereo wyłączone
  niemożliwym testem, podczas gdy `--is 0` byłoby jaśniejsze.

Współpraca i konflikty: samo z siebie nie otwiera IS, bo pozostałe bramki wciąż
obowiązują. Przesuwane automatycznie przez `--is-aggression`, a podana tu jawna wartość
przesłania suwak. Pomijane przez `--is-force-lo`/`-hi`.

#### `--is-lr-ratio <n>`

**Jak równa musi być w pasmie równowaga energii lewo/prawo, żeby intensity stereo było
dozwolone.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: Q8, gdzie 256 = 1,0; `-1` domyślnie, czyli około 179, co oznacza mniej więcej 0,7
- Wpisywanie: liczba całkowita

Bramka panoramy. Korelacja mówi, że oba kanały mają ten sam kształt; ta reguła mówi, że
mają porównywalny poziom. Pasmo, w którym jedna strona nosi niemal wszystko, jest źle
wybranym miejscem na kodowanie intensity, bo jedno wspólne widmo plus kierunek musi
przedstawić mocno niesymetryczną parę, a cały ciężar spada na parametr kierunku.

Niższe wartości dopuszczają pasma bardziej niesymetryczne i dlatego dają więcej kodowania
intensity, w tym samym przeciwnym do intuicji kierunku co próg korelacji. Wyższe wartości
wymagają równiejszego balansu i czynią IS rzadszym.

To trzeci z trzech progów do ręcznego strojenia i ten, który ustawia się na końcu. Pracuj
w kolejności: korelacja, minimalna seria, potem proporcja. Dwa pierwsze decydują, czy IS w
ogóle zadziała, ten głównie decyduje, gdzie.

Przykłady:
- Łagodnie, więcej IS: `--is-lr-ratio 160`. Trochę większa tolerancja na pasma nierówne.
- Mocno, więcej IS: `--is-lr-ratio 110`. Przyjmowane są pasma wyraźnie niesymetryczne,
  więcej intensity w całym widmie.
- Przesadnie, więcej IS: `--is-lr-ratio 20`. Przyjmowana praktycznie dowolna asymetria.
  Karykatura: materiał spanoramowany skrajnie zostaje sprowadzony do jednego widma plus
  wartości panoramy, a to dokładnie ta sytuacja, w której IS brzmi najgorzej.
- Łagodnie, mniej IS: `--is-lr-ratio 200`. Trochę surowiej niż stock.
- Mocno, mniej IS: `--is-lr-ratio 240`. Kwalifikują się tylko pasma dobrze zbalansowane;
  IS staje się rzadkie na czymkolwiek z szerokim miksem.
- Przesadnie, mniej IS: `--is-lr-ratio 256`. Wymagana dokładna równość poziomów.
  Karykatura, która wyłącza IS testem, którego nic nie przechodzi.

Współpraca i konflikty: jedna z kilku bramek; sama rzadko zmienia strumień. Przesuwana
przez `--is-aggression`, przesłaniana przez jawną wartość. Pomijana przez
`--is-force-lo`/`-hi`.

#### `--is-lo <sfb>`

**Dopuść intensity stereo tylko od tego pasma współczynnika skali w górę.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Ograniczenie miejsca, a nie wyzwalacz. Pasma poniżej podanego numeru zostają L/R albo MS
bez względu na to, co mówią bramki; pasma od niego w górę podlegają normalnym bramkom. Ten
przełącznik może kodowanie intensity tylko odebrać, nigdy dodać.

Oczywiste zastosowanie to ochrona dołu widma. Intensity stereo wysoko to rozsądny
kompromis, intensity stereo w środku pasma albo w basie już nie, bo tam ucho lokalizuje po
poziomie i czasie i tam zapadnięty obraz słychać natychmiast. Warto znać pułapkę: przy
niskim bitrate'cie koder lubi umieszczać IS na pasmach względnie NISKICH, niżej niż
podpowiada intuicja, więc jeśli chcesz zobaczyć, że ten przełącznik cokolwiek robi,
przeszukuj małe wartości, a nie zaczynaj od 30.

Przykłady:
- Łagodnie, wypchnij IS w górę: `--is-lo 10`. Bas i dolny środek chronione. Często
  wystarcza, żeby usunąć najgorsze lokalizacje.
- Mocno, wypchnij IS w górę: `--is-lo 25`. Intensity ograniczone do górnej połowy widma.
  Rozsądna wartość wyjściowa do starannej pracy przy średnim bitrate'cie.
- Przesadnie, wypchnij IS w górę: `--is-lo 46`. Intensity dozwolone tylko w kilku
  najwyższych pasmach, które przy domyślnym kodowanym pasmie mogą w ogóle leżeć ponad
  zakresem kodowania. Karykatura, i ta sama pułapka co w przykładzie z górnym pasmem MS:
  sprawdź to wobec kodowanego pasma, bo inaczej oznacza po cichu „żadnego IS”.

Współpraca i konflikty: łączy się w okno z `--is-hi`. Tylko ogranicza, więc nie potrafi
sprawić, żeby IS wystąpiło; do tego służy `--is-aggression`. Nie ma nic wspólnego z
`--is-force-lo`, czyli przełącznikiem wymuszania o podobnej nazwie.

#### `--is-hi <sfb>`

**Dopuść intensity stereo tylko do tego pasma współczynnika skali włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB
- Wpisywanie: liczba całkowita

Górna krawędź dozwolonego okna intensity, włącznie, i ten sam rodzaj ograniczenia co
`--is-lo`: zawęża miejsca, w których koder może umieścić IS, i nigdy tego nie wymusza.
Razem para definiuje obszar od..do, a wszystko poza nim zostaje kodowane konwencjonalnie.

Ograniczanie od góry to kierunek rzadszy, ale ma wyraźne zastosowanie. Jeśli właśnie sama
góra widma nosi tę informację stereo, na której najbardziej ci zależy, czyli powietrze,
lokalizację blach, pomieszczenie, możesz oddać intensity zatłoczony górny środek, a
najwyższe pasma zatrzymać w prawdziwym stereo.

Przykłady:
- Łagodne ograniczenie: `--is-hi 44`. Z intensity wyłączone są tylko pasma najwyższe.
- Mocne ograniczenie: `--is-hi 30`. Intensity ograniczone poniżej góry widma; pasmo
  powietrza zachowuje prawdziwą separację.
- Przesadne ograniczenie: `--is-hi 3`. Okno tak nisko, że bramki w jego wnętrzu
  praktycznie nigdy nie zadziałają. Karykatura, która sprowadza się do wyłączenia IS, a
  `--is 0` jest jaśniejsze i tańsze w myśleniu.

Współpraca i konflikty: `--is-lo` to druga połowa okna. Łączy się z `--isbands`, które
ogranicza liczbę, a nie pozycję. Nie wchodzi w interakcję z zakresem wymuszonym.

#### `--is-force-lo <sfb>`

**Wymuś intensity stereo od tego pasma w górę, pomijając wszystkie bramki.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-hi`
- Wpisywanie: liczba całkowita

To przełącznik laboratoryjny i zachowuje się inaczej niż wszystko powyżej. Test korelacji,
minimalna seria, reguła równowagi poziomów i reguła dopuszczenia po bitrate'cie są
pomijane: pasma w wymuszonym zakresie są kodowane jako intensity, bo tak je ustawiono. Działa
bez żadnego innego ustawionego przełącznika IS, więc nie potrzebuje `--is 1` i ignoruje
`--is-aggression`.

Zrozum, co to znaczy dla dźwięku. Kodowanie intensity zachowuje jedno widmo i kierunek,
więc w wymuszonym zakresie kanały nie mogą się już różnić niczym poza poziomem i znakiem.
Na materiale nieskorelowanym wynik nie jest subtelny; to celowe, słyszalne zapadnięcie
obrazu i może brzmieć fatalnie źle. I o to w tym przełączniku chodzi: pozwala usłyszeć i
zmierzyć mechanizm w pełnej sile, a także zbudować konkretny efekt. Strumień pozostaje
zgodny ze standardem, więc każdy dekoder go odtworzy.

Używaj go w parze z `--is-force-hi`. Ustaw go najpierw wysoko w widmie, żeby usłyszeć, co
intensity robi tam, gdzie ma być używane, a potem schodź w dół, żeby usłyszeć, dlaczego
bramki istnieją.

Przykłady:
- Łagodne wymuszenie: `--is-force-lo 40 --is-force-hi 48`. Intensity tylko na najwyższych
  pasmach, czyli z grubsza tam, gdzie koder i tak by je umieścił. Najbliżej nastawy
  używalnej.
- Mocne wymuszenie: `--is-force-lo 20 --is-force-hi 48`. Cała górna połowa wymuszona na
  intensity. Wyraźne zwężenie obrazu powyżej środka pasma i realna oszczędność bitrate'u.
- Przesadne wymuszenie: `--is-force-lo 0 --is-force-hi 48`. Całe widmo kodowane jako jeden
  sterowany kanał. Karykatura i przydatna demonstracja: tyle właśnie kosztuje cię obraz
  stereo, wszystko naraz.

Współpraca i konflikty: pomija `--is-corr-thresh`, `--is-min-sfbs`, `--is-lr-ratio` oraz
bramkę bitrate'u, więc żadne z nich go nie ogranicza. To nie to samo co `--is-lo`, które
jedynie ogranicza. Potrzebuje `--is-force-hi`, żeby wyznaczyć górę zakresu.

Zmierzone: `--is-force-lo` i `--is-force-hi` zmieniają strumień bez żadnego innego podanego
przełącznika IS.

#### `--is-force-hi <sfb>`

**Górne pasmo wymuszonego zakresu intensity, włącznie.**

- Dotyczy: rdzeń AAC w stereo. Bez efektu w HE-AAC v2.
- Wartości: `-1` wyłączone (domyślnie) albo `0..N` w obrębie liczby aktywnych SFB; w parze
  z `--is-force-lo`
- Wpisywanie: liczba całkowita

Druga połowa wymuszonego zakresu, razem z pasmem, które podajesz. Obie połowy razem
wyznaczają obszar kodowany jako intensity niezależnie od tego, jak wygląda materiał.

Trzymaj zakres wewnątrz kodowanego pasma. Wymuszanie intensity na pasmach powyżej
częstotliwości odcięcia wymusza je na ciszy, co nic nie zmienia, a ty będziesz goniąć
przełącznik, który działa bez zarzutu.

Przykłady:
- Łagodnie: `--is-force-lo 44 --is-force-hi 46`. Trzy pasma wymuszonego intensity blisko
  góry. Wąska sonda do słuchania A/B.
- Mocno: `--is-force-lo 30 --is-force-hi 48`. Szeroki wymuszony obszar przez górne widmo.
- Przesadnie: `--is-force-lo 5 --is-force-hi 48`. Wymuszone wszystko powyżej basu.
  Karykatura: bas zachowuje swoje stereo, a cała reszta to sterowane mono, co brzmi
  dokładnie tak dziwnie, jak się czyta.

Współpraca i konflikty: bez `--is-force-lo` nie ma sensu. Niezależne od przełączników
bramkowych oraz od `--is-lo`/`--is-hi`. Sprawdź zakres wobec `active SFBs` z `--verbose`
oraz wobec kodowanego pasma.

### Parametric stereo (PS)

Parametric stereo to mechanizm, z którego korzysta HE-AAC v2 (`-p 29`), i jest to zupełnie
inna propozycja niż wszystko powyżej. Koder przebiegu widzi jeden kanał: oba wejścia są
zdownmiksowane do mono, sygnał mono dostaje rdzeń AAC plus SBR, a obraz stereo podróżuje
osobno jako niewielki zestaw parametrów na ramkę. Dekoder bierze sygnał mono, dekoreluje
go i odbudowuje z tych parametrów lewy i prawy kanał.

Parametrów jest niedużo i są bardzo konkretne. IID to różnica poziomów między kanałami w
pasmie parametru, i to ona umieszcza źródło spanoramowane poziomem. ICC to koherencja,
czyli jak podobne są oba kanały, i to ona decyduje, czy pasmo brzmi jak źródło punktowe,
czy jak szerokie, rozmyte pole. Opcjonalnie IPD przenosi różnicę fazy między kanałami, a
OPD mówi, gdzie ta rotacja leży względem downmiksu. I to cały obraz stereo: kilkadziesiąt
liczb na ramkę zamiast drugiego widma.

Dlatego właśnie PS działa przy bitrate'ach, przy których nic innego nie działa. Przy 24
albo 32 kbps prawdziwy rdzeń stereo głodziłby oba kanały; rdzeń mono przy tej samej
przepływności ma się wygodnie, a parametry kosztują niemal nic. Dlatego też PS ma własne
tryby awarii, które nie mają nic wspólnego z szumem kwantyzacji: obraz może być zbyt
zgrubnie próbkowany w częstotliwości (za mało pasm), zbyt zgrubnie próbkowany w czasie (za
mało obwiedni) albo chwilowo nieobecny (ramki wysłane zupełnie bez parametrów).
Przełączniki poniżej zajmują się dokładnie tymi trzema sprawami.

Jeśli masz wziąć z tego rozdziału jedną nastawę, weź `--ps-env 4 --ps-env-reduce 0`.
Poproszenie o cztery obwiednie samo w sobie często nic nie zmienia, bo automatyczna pętla
redukcji z powrotem je zwija; dopiero wyłączenie tej pętli przynosi poprawę.

Wszystko tutaj wymaga `-p 29`. Przy LC, HE-AAC v1, LD albo ELD nie ma warstwy PS i te
przełączniki nic nie robią.

#### `--ps <n>`

**Wymuś wysyłanie parametru stereo IID albo go zabroń, zamiast zostawiać to heurystyce
kodera.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

IID to parametr różnicy poziomów, kręgosłup obrazu PS. Koder decyduje o jego wysyłaniu na
podstawie heurystyki różnicy głośności: jeśli oba kanały leżą niemal na tym samym poziomie,
może dojść do wniosku, że nie ma czego opisywać, i nie wysłać nic.

`--ps 0` całkowicie tłumi IID, co spłaszcza obraz w stronę samego downmiksu mono plus
tego, co daje ICC. To narzędzie tępe i przydatne głównie jako punkt odniesienia: zakoduj
raz z nim, żeby usłyszeć, jak brzmi sam rdzeń, a potem porównaj.

`--ps 1` wymusza obecność IID w strumieniu bezwarunkowo. To ta nastawa, która ma znaczenie
na materiale źle ocenianym przez heurystykę, przede wszystkim o niemal równych poziomach, a
do tego właśnie ona przenosi liczbę pasm fazy, kiedy używasz IPD.

Wartości:
- `-1`: stock. Decyduje heurystyka różnicy głośności.
- `0`: IID nigdy nie wysyłane. Obraz spłaszczony, prawie mono.
- `1`: IID wysyłane zawsze.

```
fdkaac -p 29 -b 32000 --ps 1 in.wav -o ps.m4a
```

Współpraca i konflikty: `--ps-ipd 1` i tak sprawia, że strumień sygnalizuje IID, bo to
jedyna droga, którą liczba pasm fazy dochodzi do dekodera. `--ps-iid-quant` wybiera
siatkę, na której kwantowane jest IID. `--ps-icc` steruje drugim głównym parametrem.

#### `--ps-iid-quant <n>`

**Wybierz zgrubną albo dokładną siatkę kwantyzacji różnic poziomów IID.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` zgrubna, `1` dokładna
- Wpisywanie: liczba całkowita

IID jest kwantowane na stałej siatce różnic poziomów. Siatka dokładna rozdziela mniejsze
różnice poziomów i dlatego umieszcza źródła precyzyjniej, zwłaszcza blisko środka, gdzie
małe różnice robią większość roboty. Siatka zgrubna wydaje mniej bitów na parametr i na
materiale, którego obraz błądzi lekko z ramki na ramkę, potrafi brzmieć stabilniej, bo małe
fluktuacje analizy kwantują się do tego samego kroku, zamiast drgać między dwoma.

Przy bitrate'ach, w których używa się PS, bity parametrów to niewielka część budżetu, więc
dla muzyki z prawdziwą lokalizacją siatka dokładna jest zwykle właściwym wyborem. Zgrubnej
spróbuj na mowie o bardzo niskiej przepływności albo na materiale, w którym słyszysz drganie
obrazu.

Wartości:
- `-1`: siatka stock dla danej konfiguracji.
- `0`: siatka zgrubna. Mniej bitów parametrów, mniej precyzyjna lokalizacja, stabilniej
  przy drganiu.
- `1`: siatka dokładna. Dokładniejsza lokalizacja, trochę więcej bitów parametrów.

```
fdkaac -p 29 -b 32000 --ps-iid-quant 1 in.wav -o fine.m4a
```

Współpraca i konflikty: ma sens tylko wtedy, gdy IID jest faktycznie wysyłane, więc w
parze z `--ps 1`, jeśli heurystyka je tłumi. Niezależne od `--ps-bands`, które decyduje,
ile wartości IID istnieje, a nie jak precyzyjnie każda z nich jest zakodowana.

#### `--ps-icc <n>`

**Wymuś włączenie albo wyłączenie parametru koherencji.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` automatycznie (domyślnie), `0` wyłączone, `1` włączone
- Wpisywanie: liczba całkowita

ICC mówi dekoderowi, jaka część pasma ma pochodzić z dekorelowanej kopii sygnału mono, a
nie z samego sygnału mono. Wysoka koherencja to zwarte źródło punktowe, niska koherencja to
źródło szerokie i rozmyte. Na materiale z pogłosem to ICC, nie IID, decyduje o różnicy
między pomieszczeniem a suchym sygnałem mono z wartością panoramy.

`--ps-icc 0` odbiera tę kontrolę, więc pasma wracają do przypadku koherentnego, a wynik
brzmi bardziej sucho i węziej, nawet gdy IID nadal poprawnie umieszcza źródła. `--ps-icc 1`
wymusza obecność danych koherencji w strumieniu tam, gdzie koder mógł uznać je za
niepotrzebne, i to jest nastawa do wypróbowania na materiale ambientowym albo mocno
zapogłosowanym, którego szerokość brzmi zapadnięta.

Wartości:
- `-1`: stock. Decyduje koder.
- `0`: brak ICC. Bardziej sucho, węziej, wrażenie źródła punktowego.
- `1`: ICC wysyłane zawsze. Materiał rozmyty zachowuje swoją szerokość.

```
fdkaac -p 29 -b 32000 --ps-icc 1 in.wav -o icc.m4a
```

Współpraca i konflikty: `--ps-icc-mode` zmienia wyłącznie sposób sygnalizowania tej samej
macierzy i nie jest jakościową alternatywą dla tego przełącznika. Uzupełnia `--ps`, które
steruje parametrem poziomu.

#### `--ps-ipd <0|1>`

**Przesyłaj międzykanałową różnicę fazy w rozszerzeniu PS.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłączone (domyślnie, stock, faza nigdy nie wysyłana), `1` włączone
- Wpisywanie: liczba całkowita, włącz/wyłącz

Bez fazy PS pozycjonuje dźwięk wyłącznie różnicami poziomów i koherencją. To pokrywa
wszystko spanoramowane poziomem, czyli większość współczesnego miksu, ale nie potrafi
przedstawić źródła umieszczonego CZASEM. Prawdziwe opóźnienie międzykanałowe, czyli to, jak
lokalizacja niskich częstotliwości działa w naturze, jest dla IID i ICC niewidoczne: oba
kanały niosą ten sam sygnał na tym samym poziomie i różnią się tylko momentem dotarcia.
Włączenie IPD wysyła ten kąt, skwantowany na 8 zdefiniowanych krokach po pi/4, w
rozszerzeniu PS z prefiksem długości.

Zakres, w którym to pomaga, jest wąski i warto go znać dokładnie. IPD jest kodowane dla
dolnych 5 pasm parametru w konfiguracji 10-pasmowej i dolnych 11 w konfiguracji
20-pasmowej, co w częstotliwości oznacza z grubsza dolne 690 Hz: te 11 pasm leży wewnątrz
pierwszych trzech pasm QMF. Posłuchaj albo zmierz powyżej 690 Hz i nie znajdziesz zupełnie
nic. Materiał, który na tym korzysta, to niskie i nisko-średnie pasmo, szerokie, miękkie
źródła, pogłos oraz prawdziwe międzykanałowe mikroopóźnienia.

Jest też sufit, przy czym wynika on z samej reprezentacji, a nie z jakiejś wady. Siatka o
kroku 45 stopni może śledzić opóźnienie tylko dopóty, dopóki wytwarzana przez nie faza
pozostaje jednoznaczna w całym kodowanym zakresie. Na górze zakresu IPD opóźnienie około
0,7 ms obraca się już przez niemal całe plus minus 180 stopni, więc siatka nie potrafi już
powiedzieć, gdzie ono jest. To klasyczna granica niejednoznaczności fazy ITD i ten sam
powód, dla którego słuch opiera się na przesłankach czasowych głównie nisko.

Jedno praktyczne sprzężenie: rozszerzenie fazy nie nosi własnej liczby pasm. Dekoder
wyprowadza ją z sygnalizacji IID, więc włączenie `--ps-ipd` sprawia, że strumień sygnalizuje
też IID. To parametr tani i zawsze dozwolony, a przy tym jedyna droga, którą liczba pasm
dochodzi do dekodera.

Zgodność jest bezpieczna z samej konstrukcji. Ładunek siedzi w rozszerzeniu z prefiksem
długości, więc dekoder bez syntezy fazy czyta liczbę bajtów i pomija je, na co standard
wyraźnie pozwala. Zwróć uwagę, które dekodery z tego korzystają: dekoder Fraunhofera
IGNORUJE rozszerzenie IPD, więc dekodowanie nim nie pokaże żadnej zmiany. ffmpeg z niego
korzysta. Wybierz dekoder świadomie, zanim orzekniesz, że przełącznik nic nie zrobił.

Wartości:
- `0`: stock. Brak fazy w strumieniu.
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

Przy 0,25 i 0,4 ms błąd fazy spada o ponad połowę, z identycznymi liczbami przy 32 i
48 kbps. Przy 0,7 ms korzyść znika, a to właśnie opisany wyżej sufit niejednoznaczności.

#### `--ps-opd <0|1>`

**Wysyłaj fazę kanału lewego względem downmiksu mono, towarzysza IPD.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`) i tylko razem z `--ps-ipd 1`
- Wartości: `0` przypina OPD do zera, `1` wylicza je (zachowanie domyślne, gdy IPD jest
  włączone)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Różnica między obydwoma parametrami fazy wynagradza uważną lekturę. IPD to faza kanału
lewego względem prawego. OPD to faza kanału lewego względem downmiksu mono. Dekoder
potrzebuje obu, bo ścieżki zasilane z downmiksu obraca o OPD, a pozostałe o OPD minus IPD.
Odejmij te dwa i OPD się skraca, więc RÓŻNICA międzykanałowa zawsze wychodzi jako IPD, bez
względu na to, co mówi OPD. OPD decyduje o tym, gdzie ta rotacja leży względem downmiksu.

Właśnie to skracanie sprawia, że problem łatwo przeoczyć. Przy OPD przypiętym do zera lewy
kanał jest przyklejony do fazy downmiksu, a cała rotacja zostaje zrzucona na prawy. Każdy
pomiar arg(L) minus arg(R) jest na to ślepy, bo różnica nadal jest poprawna. Zmierz
zamiast tego fazę bezwzględną każdego kanału wobec źródła, wewnątrz zakresu IPD, i asymetria
staje się oczywista, podobnie jak to, że OPD ją usuwa.

Wyprowadzenie OPD praktycznie nic nie kosztuje, i to jest w tym zgrabne. Downmiks to L+R,
więc sum(L * conj(L+R)) równa się sum(|L|^2) plus sum(L * conj(R)), której część
rzeczywista to pwrL + pwrCr, a część urojona to pwrCi: te same akumulatory, które już
zasilają IID, ICC i IPD. Zatem OPD to atan2(pwrCi, pwrL + pwrCr), czyli jeszcze jeden
atan2 na liczbach już obecnych w pętli, bez potrzeby podawania sygnału downmiksu skądkolwiek
indziej. Dla przypadku opóźnienia przy równych poziomach redukuje się to analitycznie do
IPD/2, bo arg(1 + e^-i.phi) = -phi/2, co jest przydatnym testem zdrowego rozsądku; postać
ogólna jest poprawna także wtedy, gdy kanały różnią się poziomem, gdzie IPD/2 już nie byłoby.

Zostaw `0` do porównań A/B na tym samym materiale i zapamiętaj, że `--ps-opd 0` samo z
siebie, bez `--ps-ipd 1`, daje strumień stock, bo nie ma warstwy fazy, którą mógłby
zmodyfikować. Zwróć też uwagę, że różnice OPD pojawiają się tylko na materiale z prawdziwym
opóźnieniem międzykanałowym; na treści spanoramowanej poziomem nie ma tu czego poprawiać.

Wartości:
- `0`: OPD przypięte do zera. Błąd fazy asymetryczny między kanałami.
- `1`: OPD wyliczane z istniejących akumulatorów. Błąd fazy symetryczny.

```
fdkaac -p 29 -b 32000 --ps-ipd 1 --ps-opd 0 in.wav -o opd0.m4a
```

Współpraca i konflikty: bez `--ps-ipd 1` nie ma sensu. Nie zmienia różnicy międzykanałowej,
a tylko fazę bezwzględną każdego kanału, więc dobierz pomiar odpowiednio.

Zmierzone, z ffmpegiem jako dekoderem: bezwzględny błąd fazy każdego kanału wobec źródła,
wewnątrz zakresu IPD.

| Opóźnienie międzykanałowe | OPD = 0 (L / R) | OPD wyliczane (L / R) |
|---|---|---|
| 0.25 ms | 14.7 / 20.1 deg | 14.4 / 17.3 deg |
| 0.4 ms | 23.0 / 28.0 deg | 15.5 / 17.8 deg |
| 0.7 ms | 48.1 / 56.2 deg | 40.9 / 36.3 deg |

Przy OPD = 0 oba kanały nigdy się nie zgadzają, 14,7 przeciw 20,1, a dalej 48,1 przeciw
56,2. Przy wyliczanym OPD para się zbiega, średnio o +6,4 stopnia, a przy 0,7 ms o +13.
Zwróć uwagę, że 0,7 ms to przypadek, w którym zwykły pomiar IPD nie pokazał żadnej
korzyści: różnica międzykanałowa była już poza tym, co potrafi rozdzielić siatka 45 stopni,
a bezwzględna faza każdego kanału i tak poprawiła się istotnie.

#### `--ps-icc-mode <n>`

**Wybierz, który z dwóch zdefiniowanych trybów rotacji ICC jest sygnalizowany.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `-1` domyślnie, `0` ROT_A, `1` ROT_B
- Wpisywanie: liczba całkowita

Ustaw oczekiwania, zanim poświęcisz na to czas: to wyłącznie sygnalizacja. Oba tryby
opisują tę samą macierz stereo, dekoder po prostu wyprowadza ją inaczej. Traktuj to jako
pokrętło zgodności, a nie jakości, a jeśli szukasz słyszalnych różnic stereo w PS, to
mieszkają one w `--ps-bands` i `--ps-env`.

Warto go mieć, bo nie wszystkie implementacje dekoderów są równie dobrze przećwiczone na
obu ścieżkach, więc jeśli jakiś konkretny odtwarzacz dziwnie renderuje strumień PS,
przełączenie trybu rotacji jest tanie do sprawdzenia.

Wartości:
- `-1`: wybór stock dla danej konfiguracji.
- `0`: sygnalizowane ROT_A.
- `1`: sygnalizowane ROT_B.

```
fdkaac -p 29 -b 32000 --ps-icc-mode 1 in.wav -o rotb.m4a
```

Współpraca i konflikty: istotne tylko wtedy, gdy ICC jest wysyłane, więc sprawdź
`--ps-icc`. Słuchowo znacznie mniej znaczące niż `--ps-bands`.

#### `--ps-bands <10|20>`

**Rozdzielczość częstotliwościowa parametrów stereo: ile pasm PS opisuje obraz.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `10` albo `20`; `-1` wybiera na podstawie bitrate'u (domyślnie), a od 22 kbps w
  górę tabela zawsze daje 20
- Wpisywanie: liczba całkowita

To jedna z dwóch osi decydujących o tym, ile parametrów stereo w ogóle istnieje, i jest to
oś częstotliwości. Dwadzieścia pasm opisuje obraz z dokładniejszym szczegółem widmowym,
więc miks, którego lokalizacja zmienia się w widmie (bas w środku, gitary szeroko, talerze
przesunięte), zachowuje tę strukturę. Dziesięć pasm uśrednia ten sam obraz po dwa razy
większym fragmencie widma każde.

Ciekawy jest dolny koniec zakresu bitrate'u. Zgrubna siatka nie tylko jest tańsza, ale może
też brzmieć stabilniej, bo każdy parametr jest uśredniany po większym fragmencie widma i
dlatego mniej drga z ramki na ramkę. Błądzący obraz bardziej rozprasza niż lekko rozmyty.
Stock FDK wyprowadza tę liczbę wyłącznie z bitrate'u i od 22 kbps w górę zawsze wybiera 20,
więc bez tego przełącznika nie da się na normalnym materiale w ogóle przesłuchać siatki
zgrubnej.

Wartości:
- `10`: siatka zgrubna. Mniej bitów parametrów, bardziej rozmyta lokalizacja w
  częstotliwości, stabilniej przy drganiu analizy.
- `20`: siatka dokładna. Dokładniejsza lokalizacja, więcej bitów parametrów.
- `-1`: stock, wybrane na podstawie bitrate'u.

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

Jest tu pułapka i to najważniejsza rzecz w tym rozdziale: poproszenie o cztery obwiednie
nie znaczy, że je dostaniesz. Automatyczna pętla redukcji wciąż dzieli tę liczbę na pół, z
czterech na dwie, z dwóch na jedną, kiedy tylko sąsiednie obwiednie wyglądają podobnie
według wbudowanego progu błędu, a na zwykłej muzyce ten próg jest spełniany bardzo często.
Żądanie jest odwoływane, zanim dotrze do strumienia. Zatrzymuje to `--ps-env-reduce 0`, a
pomiar w jego karcie pokazuje różnicę wprost.

Wartości:
- `1`: jedna migawka stereo na ramkę. Najtaniej, rozmazuje każdy ruch.
- `2`: dwie obwiednie. Rozsądna nastawa środkowa razem z `--ps-env-reduce 0`.
- `4`: cztery obwiednie. Dokładnie nadąża za ruchomą panoramą i transjentami.
- `-1`: stock, wybrane na podstawie bitrate'u.

```
fdkaac -p 29 -b 32000 --ps-env 4 --ps-env-reduce 0 in.wav -o env4.m4a
```

Współpraca i konflikty: żeby na zwykłym materiale zostało uszanowane, wymaga w praktyce
`--ps-env-reduce 0`. `--ps-bands` to częstotliwościowa oś tego samego kompromisu.

Zmierzone: przy HE-AAC v2 32 kbps `--ps-env 4` faktycznie zmienia strumień, bo tabela
bitrate'u wybiera tam 2; przy 48 kbps tabela daje już 4, więc żądanie zgadza się ze stock i
różnicę robi tylko `--ps-env-reduce 0`.

#### `--ps-env-reduce <0|1>`

**Wyłącz automatyczną pętlę połowienia obwiedni, żeby przesłana została ta liczba obwiedni,
o którą prosisz.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` wyłącza pętlę redukcji, `1` zostawia ją włączoną; `-1` wyłączone, czyli
  stock, który ma pętlę włączoną (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Koder utrzymuje pętlę redukcji, która porównuje sąsiednie obwiednie parametrów i zwija
cztery w dwie, a dwie w jedną, kiedy tylko wyglądają dość podobnie według wbudowanego progu
błędu. Intencją jest oszczędzenie bitów parametrów na materiale statycznym. Skutkiem
ubocznym jest to, że zamówiona liczba obwiedni często nie jest tą, która zostaje
przesłana, a ponieważ oszczędność jest mała, a kosztem jest rozdzielczość czasowa obrazu,
na materiale z jakimkolwiek ruchem ten kompromis jest często zły.

`--ps-env-reduce 0` wyłącza pętlę i czyni `--ps-env` dosłownym. To najwartościowsza
pojedyncza nastawa w grupie PS. Zysk jest w dokładności czasowej obrazu stereo, przy
praktycznie tym samym rozmiarze pliku, bo bity parametrów to niewielka część budżetu przy
tych przepływnościach.

Wartości:
- `-1` albo `1`: stock. Pętla działa i może zwinąć liczbę obwiedni.
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
| stock (20 bands / 4 envelopes) | 0.177 | 0.9655 |
| `--ps-env 2 --ps-env-reduce 0` | 0.138 | 0.9896 |
| `--ps-env 4 --ps-env-reduce 0` | 0.117 | 0.9944 |

Samo żądanie 4 obwiedni nie zmienia przy 48 kbps nic, wyjście jest identyczne bajt w bajt
ze stock, bo pętla redukcji natychmiast odwołuje żądanie. Zysk pojawia się tylko wtedy, gdy
pętla zostanie zatrzymana: o jedną trzecią mniejszy błąd panoramy przy z grubsza tym samym
rozmiarze pliku.

#### `--ps-noenv-skip <0|1>`

**Zabroń ramek PS bez parametrów, żeby każda ramka nosiła informację stereo.**

- Dotyczy: wyłącznie HE-AAC v2 (`-p 29`)
- Wartości: `0` nigdy nie emituj ramek bez parametrów, `1` zezwalaj na nie; `-1` wyłączone,
  czyli stock, który na nie zezwala (domyślnie)
- Wpisywanie: liczba całkowita, włącz/wyłącz

Poza przerzedzaniem obwiedni koder może też całkowicie pominąć parametry stereo na odcinku
ramek. Kiedy kolejne zestawy IID i ICC wyglądają podobnie, może wysłać do dziesięciu
następujących po sobie ramek, które nie niosą żadnej informacji stereo, pozostawiając
dekoder na biegu jałowym z ostatnimi wartościami, jakie otrzymał.

Na materiale statycznym jest to darmowe. Na materiale, w którym obraz dryfuje powoli,
słychać to jako spłaszczanie się obrazu stereo i jego powrót z trzaskiem, gdy parametry
wracają, bo dekoder trzymał przez cały dryf nieaktualną migawkę, a potem przeskoczył do
bieżącej. Kiedy już znasz ten artefakt, zwykle zaczynasz go słyszeć, a rozprasza bardziej,
niż są warte zaoszczędzone bity.

`--ps-noenv-skip 0` zabrania pomijania i wymusza parametry w każdej ramce. Używaj go, gdy
obraz wydaje się oddychać albo zacinać. Kosztem jest skromna liczba bitów parametrów na
ramkę.

Wartości:
- `-1` albo `1`: stock. Do dziesięciu następujących po sobie ramek może nie nieść żadnych
  parametrów stereo.
- `0`: parametry w każdej ramce.

```
fdkaac -p 29 -b 32000 --ps-noenv-skip 0 --ps-env 4 --ps-env-reduce 0 in.wav -o steady.m4a
```

Współpraca i konflikty: ta sama rodzina ukrytego przerzedzania co `--ps-env-reduce`, i te
dwa ustawia się zwykle razem, gdy celem jest stabilny obraz. Żaden z nich nie zmienia audio
rdzenia, tylko strumień parametrów stereo.
## C. Pasmo i SBR

O tym, jak wysoko brzmi plik, decydują dwie różne rzeczy, które łatwo ze sobą pomylić.

Pierwsza to **pasmo rdzenia**: najwyższa częstotliwość, którą sam rdzeń AAC faktycznie
koduje, prawdziwymi liniami widmowymi i prawdziwymi bitami. Powyżej tej granicy rdzeń nie
wysyła nic.

Druga to **częstotliwość podziału SBR**, nazywana też częstotliwością startu SBR. SBR
(spectral band replication) nie koduje górnego pasma. Kopiuje w górę niższe, już
zakodowane widmo, a potem opisuje garścią parametrów na ramkę, jak ta kopia ma zostać
ukształtowana. Częstotliwość podziału to miejsce, w którym kopia się zaczyna. W poprawnie
zbudowanym strumieniu HE-AAC pasmo rdzenia i częstotliwość podziału leżą w tym samym
miejscu: rdzeń się kończy, SBR przejmuje pałeczkę, a słuchacz słyszy jedno ciągłe widmo.

To, co SBR wysyła na ramkę, jest krótką listą:

- **obwiednie**: poziom każdego pasma SBR w czasie. Więcej obwiedni na ramkę oznacza
  drobniejszą rozdzielczość czasową na górze i więcej bitów.
- **poziom szumu (noise floor)**: ile syntetycznego szumu domieszać do każdego pasma. To
  właśnie on zamienia skopiowany obszar tonalny z powrotem w coś, co brzmi jak powietrze,
  talerze albo oddech.
- **poziom filtrowania odwrotnego** (invf): jak mocno wybielać kopię w każdym paśmie.
  Skopiowany materiał niesie strukturę tonalną pasma, z którego pochodzi. Jeśli ta
  struktura nie pasuje do góry, słychać ją jako metaliczną, dzwoniącą, chórowaną górę.
  Wybielanie tę strukturę spłaszcza.
- **brakujące harmoniczne**: flagi, które każą dekoderowi wstawić syntetyczną sinusoidę w
  paśmie, gdzie prawdziwa składowa oryginału zginęła albo wylądowała w złym miejscu.

O dwóch ostatnich decydują **estymatory** w koderze, które porównują tonalność
oryginalnego górnego pasma z tonalnością załatanej kopii, pasmo po paśmie i ramka po
ramce. To w estymatorach ustala się charakter góry, a one wymieniają się między sobą
wzdłuż jednej osi: za mało wybielania i za mało szumu wypełniającego daje górę metaliczną,
dzwoniącą, pofazowaną; za dużo jednego i drugiego daje górę syczącą i rozmytą, w której
talerze zamieniają się w kształtowany szum, a utrzymujące się składowe znikają. Większość
pokręteł w tej grupie przesuwa cię dokładnie po tej osi. Nie ma na niej punktu uniwersalnie
poprawnego; zależy on od materiału i od bitrate'u.

SBR jest aktywne w tych profilach:

- `-p 5` HE-AAC
- `-p 29` HE-AAC v2 (SBR plus parametric stereo)
- `-p 39 -L 1` AAC-ELD z SBR

W `-p 2` (AAC-LC), `-p 23` (AAC-LD) i `-p 39` bez `-L 1` nie ma SBR, więc każdy
przełącznik `--sbr-*` z tej grupy nic nie robi. `--core-cutoff` działa wszędzie.

Dwie konwencje używane w całej tej grupie:

- **pokrętła x100** skalują wewnętrzną stałą: `100` oznacza 1,00 i jest dokładnie
  stockiem, poniżej 100 stała maleje, powyżej 100 rośnie. Oba kierunki działają. Kilka
  pokręteł jest opisanych jako **surowe x100**, co znaczy, że wartość zastępuje stałą
  wprost, zamiast ją skalować (`--sbr-tran-peak 90` *jest* stockowym 0,90).
- `--verbose` wypisuje efektywne pasmo rdzenia, częstotliwość podziału, końcową górną
  krawędź i okres nagłówka SBR. Pod SBR to jedyny pewny sposób, żeby zobaczyć, co
  naprawdę ustawił enkoder, a kilka przełączników z tej grupy FDK sprawdza dopiero przy
  inicjalizacji, więc sprawdzanie nie jest opcjonalne.

### Pasmo rdzenia: `--core-cutoff`

#### `--core-cutoff <hz>`

**Wymusza górną granicę pasma kodowanego przez rdzeń AAC, w Hz, i działa także pod SBR,
gdzie `-w` nie działa.**

- Dotyczy: każdego profilu. To jedyny przełącznik z grupy C, który przydaje się bez SBR.
- Wartości: `0..48000` Hz (spoza zakresu jest odrzucane); koder zatrzymuje się na
  częstotliwości Nyquista rdzenia (połowa częstotliwości próbkowania rdzenia); domyślnie `0` =
  zostaw decyzję FDK
- Wpisywanie: liczba całkowita

Standardowe `-w/--bandwidth` zostaje nadpisane przy inicjalizacji SBR: o paśmie rdzenia
decyduje tabela strojenia SBR, tak żeby rdzeń kończył się dokładnie tam, gdzie zaczyna się
częstotliwość podziału, a wartość podana przez `-w` jest odrzucana. `--core-cutoff`
jest stosowany później i to przeżywa.

Dwie sytuacje sprawiają, że warto po niego sięgnąć. Pierwsza to materiał z SBR, w którym
chcesz mieć pod częstotliwością podziału więcej prawdziwego, zakodowanego widma, niż daje
tabela, zwykle dlatego, że załatane pasmo brzmi syntetycznie i wolisz wydać bity na
autentyczną średnią górę. Druga nie ma z SBR nic wspólnego: AAC-LD i AAC-ELD dobierają
pasmo rdzenia z tabeli, która jest bardzo zachowawcza, około 2 kHz przy 48 kbps i około
5,8 kHz przy 64 kbps. Już z samego tego powodu te profile brzmią głucho, a `--core-cutoff`
jest na to lekarstwem.

Koder nie potrafi uciąć na dowolnej wartości w Hz. Widmo jest podzielone na pasma
współczynnika skali, więc odcięcie przyciąga się do najbliższej granicy SFB, a `--verbose`
raportuje prawdziwy wynik, zakotwiczony w SFB. Prośba o 17300 Hz i odpowiedź 17915 Hz to
nie błąd; w strumieniu jest 17915 Hz.

Uważaj na sufit. Maksimum to połowa częstotliwości próbkowania *rdzenia*, a przy SBR w
trybie dual rate rdzeń pracuje na połowie częstotliwości wejścia: dla wejścia 48 kHz pod
HE-AAC rdzeń chodzi na 24 kHz, więc twardy sufit to 12 kHz. Jest też osobne ograniczenie
20 kHz, które obowiązuje bez SBR; `--uncap-bandwidth` (grupa A) je zdejmuje, żeby
`--core-cutoff` mógł sięgnąć do połowy częstotliwości próbkowania na materiale o wysokiej
częstotliwości, a sama ta flaga nie robi nic.

Za wynik odpowiadasz sam. Zepchnięcie odcięcia rdzenia daleko *poniżej* częstotliwości
podziału zostawia wszystko między szczytem rdzenia a startem SBR puste, co słychać jako
dziurę w średniej górze, a nie jako łagodne opadanie. Jeśli chcesz naprawdę niskiego
rdzenia, zjedź w dół również z częstotliwością podziału przez `--sbr-start`, żeby oba
punkty znów się spotkały.

Przykłady, zwężanie rdzenia (HE-AAC `-p 5`, 64 kbps stereo, wejście 48 kHz, gdzie tabela
daje około 10500 Hz rdzenia):
- Łagodnie: `--core-cutoff 9000`. Nieco tańszy rdzeń, nieco szerszy obszar SBR. Bity
  uwolnione w rdzeniu idą na dolne widmo.
- Mocno: `--core-cutoff 7500`. Wyraźnie większa część widma jest teraz replikowana, a nie
  kodowana. Spodziewaj się cieńszego, bardziej syntetycznego obszaru 8..10 kHz i
  czystszego niskiego środka.
- Przesadnie: `--core-cutoff 2000`. Rdzeń kończy się na szczycie tonów podstawowych głosu.
  Wszystko od 2 kHz do częstotliwości podziału jest dziurą. Karykatura, chyba że zjedziesz
  też mocno w dół z `--sbr-start`.

Przykłady, poszerzanie rdzenia (te same warunki):
- Łagodnie: `--core-cutoff 11500`. Trochę prawdziwego widma powyżej typowej częstotliwości
  podziału.
- Mocno: `--core-cutoff 12000` przy wejściu 48 kHz pod HE-AAC. Rdzeń dochodzi teraz do
  swojego Nyquista. Prawdziwie kodowana góra, ale rdzeń płaci pełną cenę za pasmo, które
  SBR obsługiwało za darmo, więc przy 64 kbps reszta widma robi się grubsza.
- Przesadnie: `--core-cutoff 20000` na AAC-LC przy 320 kbps, z `-p 2` i całkiem bez SBR.
  Legalne i czasem naprawdę pożądane, ale przy niskim bitrate ta sama prośba wypompowałaby
  budżet bitów w niesłyszalną górę.

Współpraca i konflikty: pod SBR `-w` jest ignorowane, a `--verbose` nadal opisuje
automatyczną wartość etykietą "[from -w]", choć to `--core-cutoff` ją ustawił. Warto
wyrobić sobie nawyk patrzenia na raportowaną liczbę, a nie na etykietę. Przy dużym ruchu
odcięcia łącz go z `--sbr-start`, żeby rdzeń i częstotliwość podziału nadal się spotykały.
`--uncap-bandwidth` jest potrzebny tylko po to, żeby bez SBR wyjść powyżej 20 kHz.

Zmierzone: `--core-cutoff 7500` pod HE-AAC v2 przy 48 kbps daje efektywny rdzeń 7500 Hz,
podczas gdy stockowe `-w 7500` zostawia 8613 Hz, bo jest ignorowane.

### Układ częstotliwości SBR: `--sbr-start` `--sbr-stop` `--sbr-freqscale` `--sbr-alterscale` `--sbr-noise-bands` `--sbr-amp-res` `--sbr-data-extra`

Tych siedem przełączników definiuje kształt pasma SBR, zanim cokolwiek zostanie w nim
zmierzone: gdzie zaczyna się obszar replikowany, gdzie się kończy, jak jest podzielony na
pasma, jak drobno opisywany jest poziom szumu i jak dokładnie kwantowane są poziomy
obwiedni.

Dwa ważne to `--sbr-start` i `--sbr-stop`. Oba przyjmują indeks tabeli, a nie
częstotliwość, a to, na jaką częstotliwość mapuje się indeks, zależy od częstotliwości
próbkowania, profilu i bitrate'u. Wartości zmierzone podajemy w kartach poniżej; czytaj je
jako wskazówkę co do kierunku i odstępów, a rzeczywiste liczby dla swoich ustawień
potwierdź przez `--verbose`.

Reguła, na której wykłada się każdy: **legalność zależy od pary, bitrate'u i
częstotliwości próbkowania.** SBR wymaga poprawnej liczby pasm nadrzędnych między startem
a stopem, a FDK sprawdza tę kombinację przy inicjalizacji kodera. Zła para nie degraduje
się łagodnie; zgłasza "encoder initialization failed" i nic nie zostaje zapisane. Legalne
kombinacje tworzą przekątny pas: podnosząc start, zwykle musisz podnieść razem z nim stop.
Przy HE-AAC 64 kbps stereo, wejście 48 kHz, każdy start od 0 do 4 jest nielegalny z każdym
stopem, a wyglądająca na udokumentowaną para `--sbr-start 5 --sbr-stop 9` tam zawodzi.
Pary, które przy tym ustawieniu działają: `8/9`, `8/14`, `14/12`.

Czyli: zmieniaj po jednym indeksie naraz, uruchom kodowanie i czytaj `--verbose`.
Kodowanie, które nie zainicjalizuje się, kosztuje cię sekundę; strumień z niezamierzoną
częstotliwością podziału kosztuje cię całą sesję odsłuchową.

#### `--sbr-start <n>`

**Ustawia miejsce, w którym kończy się rdzeń, a zaczyna replikowane górne pasmo.**

- Dotyczy: tylko profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: indeks tabeli `0..15`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

To najbardziej brzemienna w skutki pojedyncza liczba w strumieniu HE-AAC. Wszystko poniżej
częstotliwości podziału to prawdziwe, zakodowane widmo; wszystko powyżej to opisana kopia.
Obniżenie startu daje rdzeniowi mniej roboty, więc to, co koduje, koduje czyściej, a
większa część widma staje się parametryczna. Podniesienie startu działa odwrotnie: więcej
prawdziwej góry, mniejszy obszar SBR i rdzeń, który musi za to zapłacić.

Przy bardzo niskim bitrate obniżenie częstotliwości podziału jest często jedynym sposobem,
żeby rdzeń w ogóle brzmiał czysto. Przy 64 kbps i wyżej wygrywa raczej podnoszenie, bo
rdzeń stać na autentyczny detal w zakresie 10..12 kHz, a skopiowane pasmo zawsze niesie
część charakteru pasma, z którego je skopiowano.

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
| 14 | 10500 Hz (stock przy tym bitrate) |
| 15 | 12000 Hz |

Ten sam przegląd przy wejściu 44,1 kHz, HE-AAC 32 kbps: start 5 daje 5038 Hz, 8 daje
5943 Hz, 11 daje 7579 Hz, 14 daje 9646 Hz, 15 daje 11025 Hz. Zwróć uwagę, że indeksy 8 i 9
przy 48 kHz trafiają w to samo pasmo rdzenia: to mapowanie jest tabelą, a nie wzorem, i
sąsiednie indeksy mogą się pokrywać.

Przykłady, obniżanie częstotliwości podziału (HE-AAC 64 kbps stereo, 48 kHz, stock 14):
- Łagodnie: `--sbr-start 13`. Rdzeń do 9000 Hz. Minimalnie czystszy rdzeń, odrobinę
  szersza łata.
- Mocno: `--sbr-start 11`. Rdzeń do 7640 Hz. Zauważalnie większa część góry jest
  parametryczna. Warto spróbować, gdy rdzeń brzmi ziarniście.
- Przesadnie: `--sbr-start 6 --sbr-stop 9`. Rdzeń do 5484 Hz. Prawie wszystko powyżej
  obszaru obecności jest kopią; talerze i syczące głoski to w całości wymysł SBR. To
  właściwy kształt przy 24 kbps i karykatura przy 64.

Przykłady, podnoszenie częstotliwości podziału:
- Łagodnie: `--sbr-start 15`. Rdzeń do 12000 Hz, szczyt zakresu indeksów przy tej
  częstotliwości. Prawdziwie kodowane widmo do 12 kHz, węższy obszar SBR powyżej.
- Mocno: dalsze podnoszenie częstotliwości podziału nie jest tym przełącznikiem dostępne
  przy tym ustawieniu, bo 15 to najwyższy indeks. Kolejny krok to porzucić SBR i zakodować
  całe pasmo przez `-p 2` plus `-w`.
- Przesadnie: to samo doprowadzone do końca: `-p 2 -b 64000 -w 16000`. Bez SBR, bez kopii,
  a przy 64 kbps stereo rdzeń wyraźnie nie ma bitów na pasmo 16 kHz. Pouczające do
  porównania z HE-AAC na tym samym materiale.

Współpraca i konflikty: musi tworzyć legalną parę z `--sbr-stop`. `--core-cutoff` potrafi
przesunąć pasmo rdzenia niezależnie od częstotliwości podziału, i właśnie tak, przy braku
uwagi, robi się dziurę. Obniżenie startu poszerza pasmo, za które odpowiadają
`--sbr-noise-bands`, `--sbr-invf*` i detektor brakujących harmonicznych, więc im niżej
zjeżdżasz, tym bardziej liczą się ich ustawienia.

#### `--sbr-stop <n>`

**Ustawia górną krawędź pasma replikowanego, czyli górną krawędź pliku.**

- Dotyczy: tylko profili z SBR
- Wartości: indeks tabeli `0..13`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Tam, gdzie `--sbr-start` dzieli widmo, ten przełącznik ustawia sufit. Podniesienie go
rozciąga obszar załatany wyżej, co prawie nic nie kosztuje w bitach (SBR opisuje pasma, a
nie linie), ale rozkłada ten sam budżet parametrów na szerszy obszar, więc każde pasmo jest
opisane grubiej. Obniżenie go zamyka plik wcześniej, co brzmi jak zwykła dolnoprzepustowa
i bywa odbierane jako czystsze, bo mierna łata w zakresie 18..22 kHz wnosi więcej syku niż
treści.

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
| 12 | około 22125 Hz (stock) |
| 13 | nielegalny przy tym ustawieniu |

Przykłady, obniżanie górnej krawędzi (ze stockowego 12):
- Łagodnie: `--sbr-start 14 --sbr-stop 11`. Góra na około 20250 Hz. Trudne do usłyszenia,
  nieco gęstszy opis tego, co zostaje.
- Mocno: `--sbr-start 14 --sbr-stop 9`. Góra na około 16875 Hz. Świadomy, słyszalny sufit,
  a pozostałe pasma opisane drobniej. Często lepszy kompromis przy 32 kbps i niżej.
- Przesadnie: `--sbr-start 14 --sbr-stop 4`. Góra na około 10875 Hz przy rdzeniu 10500 Hz:
  SBR obejmuje teraz kilkaset Hz i nie wnosi nic. W praktyce powstaje strumień AAC-LC z
  doczepionym narzutem SBR.

Przykłady, podnoszenie górnej krawędzi:
- Łagodnie: w górę przy tym ustawieniu nic nie ma. Stop 12 (około 22125 Hz) to już
  stockowa góra, a stop 13 nie jest legalny ze startem 14. Górę można utrzymać, przesuwając
  częstotliwość podziału w górę: `--sbr-start 15 --sbr-stop 12` zachowuje około 22125 Hz
  przy rdzeniu 12000 Hz.
- Mocno: `--sbr-start 8 --sbr-stop 14`. Szeroki obszar załatany od 6468 Hz w górę. Tani w
  bitach, zgrubny w opisie: uważaj na "migotliwy" charakter na gęstym materiale.
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
  częstotliwości podziału, czyli odwrotnie niż chce ucho. Bywa użyteczne na materiale,
  którego górne pasmo jest naprawdę jednorodne, na przykład na szerokopasmowych podkładach
  szumowych.
- `1` najdrobniejsze grupowanie logarytmiczne, 12 pasm na oktawę. Najwięcej pasm, najwięcej
  parametrów, najlepsze kształtowanie nierównego górnego pasma.
- `2` środkowe grupowanie logarytmiczne, 10 pasm na oktawę.
- `3` najgrubsze grupowanie logarytmiczne, 8 pasm na oktawę. Najmniej pasm, najtańszy opis,
  najmniejsza kontrola.

Zmierzone przy HE-AAC 64 kbps, dane SBR na ramkę: 220 bitów przy `0`, 188 przy `1`, 170 przy
`2`, 152 przy `3`.
- `-1` wyłączone: decyduje tabela strojenia, co jest właściwym wyborem, dopóki nie masz
  powodu, żeby zrobić inaczej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 1 -o out.m4a in.wav` (najdrobniejsza siatka)

Współpraca i konflikty: liczba pasm, którą to daje, wchodzi w interakcję z
`--sbr-noise-bands` (pasma szumu wyprowadza się na oktawę, a nie na pasmo SBR) oraz z tym,
jak bardzo decyzja invf może się zmieniać w obrębie obszaru. Drobniejsza siatka oznacza
drobniejszą mapę invf.

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
- `-1` wyłączone: idź za tabelą strojenia. Zalecane.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-freqscale 2 --sbr-alterscale 1 -o out.m4a in.wav`

Współpraca i konflikty: ma sens tylko razem z `--sbr-freqscale`, bo koryguje właśnie tamtą
siatkę. Jeśli polujesz na zmianę barwy na górze, prawdziwą dźwignię mają `--sbr-invf*` i
`--sbr-noise-bands`; po ten sięgaj na końcu.

#### `--sbr-noise-bands <n>`

**Ustawia, jak drobno poziom szumu SBR jest opisany wzdłuż częstotliwości.**

- Dotyczy: tylko profili z SBR
- Wartości: `0`, `1`, `2`, `3`; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziom szumu to sposób, w jaki SBR wkłada z powrotem "powietrze" do skopiowanego pasma.
Przy jednym paśmie szumu na cały obszar koder wysyła jeden poziom szumu, a dekoder
rozsmarowuje go równo: tanio i błędnie zawsze wtedy, gdy górne pasmo nie jest jednorodne,
na przykład przy utrzymywanym dźwięku smyczków pod hi-hatem. Przy większej liczbie pasm
szumu poziom może podążać za materiałem, więc cichy obszar tonalny nie dostaje tyle samo
syku, co obszar ruchliwy.

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
  tonalnych od szumowych, najwięcej danych pobocznych.
- `-1` wyłączone: decyduje tabela strojenia, zależnie od bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-noise-bands 3 -o out.m4a in.wav`

Współpraca i konflikty: to decyduje o *rozdzielczości* poziomu szumu; `--sbr-noise-max`,
`--sbr-nf-weight` i `--sbr-noise-floor-offset` decydują o jego *poziomie*. Podnoszenie
gęstości idzie na marne, jeśli poziom jest przycięty niskim sufitem, więc jeśli gonisz za
powietrzem, zajrzyj najpierw do `--sbr-noise-max`.

#### `--sbr-amp-res <n>`

**Wybiera krok kwantyzacji poziomów obwiedni SBR: 1,5 dB albo 3,0 dB.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = 1,5 dB, `1` = 3,0 dB; domyślnie `-1` = wartość z tabeli strojenia SBR
- Wpisywanie: liczba całkowita

Poziomy obwiedni są wysyłane jako liczby skwantowane, a to jest rozmiar kroku. Krok 1,5 dB
śledzi zmiany poziomu w górnym paśmie dokładniej i kosztuje więcej bitów na obwiednię. Krok
3,0 dB jest tańszy i grubszy; na materiale z dużym ruchem poziomu na górze zgrubny
krok słychać jako skokowe, a nie płynne, zmiany górnego pasma.

Wartości:
- `0` kroki 1,5 dB. Drobniej, drożej. Preferuj przy wyższym bitrate albo na materiale
  dynamicznym.
- `1` kroki 3,0 dB. Zgrubniej, taniej. Rozsądny kompromis, gdy bitów brakuje, i dlatego
  tabela często wybiera go nisko.
- `-1` wyłączone: decyduje tabela strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 80000 --sbr-amp-res 0 -o out.m4a in.wav`

Współpraca i konflikty: wchodzi w bezpośrednią interakcję z `--sbr-num-env` i detektorem
transjentów, bo każda dodatkowa obwiednia kosztuje pełny komplet poziomów przy
obowiązującym rozmiarze kroku. Cztery obwiednie przy rozdzielczości 1,5 dB to najdroższy
róg tej grupy.

#### `--sbr-data-extra <n>`

**Zapisuje opcjonalne dodatkowe pola nagłówka SBR, zamiast je pomijać.**

- Dotyczy: tylko profili z SBR
- Wartości: `0` = nie zapisuj, `1` = zapisz; domyślnie `-1` = wartość z tabeli strojenia
  SBR
- Wpisywanie: liczba całkowita

Nagłówek SBR ma opcjonalny blok z dodatkową konfiguracją. Zapisywanie go powiększa
nieznacznie każdy nagłówek i czyni konfigurację bardziej jawną; pominięcie zostawia dekoder
z domyślnymi założeniami dla tych pól. To decyzja o formatowaniu strumienia, a nie kontrola
brzmienia, i warto jej dotykać tylko wtedy, gdy gonisz zgodność z konkretnym dekoderem albo
oglądasz nagłówki.

Wartości:
- `0` nie zapisuj dodatkowych danych nagłówka.
- `1` zapisz je.
- `-1` wyłączone: idź za tabelą strojenia.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-data-extra 1 -o out.m4a in.wav`

Współpraca i konflikty: w połączeniu z krótkim `--sbr-header-period` dodatkowe bajty
powtarzają się często, więc jeśli optymalizujesz strumień o niskim bitrate pod szybkie
zaskoczenie SBR, trzymaj to wyłączone.

### Rozdzielczość czasowa SBR i stereo: `--sbr-num-env` `--sbr-freqres-fixfix` `--sbr-stereo-mode` `--sbr-header-period` `--sbr-header-time`

Układ częstotliwości decyduje o tym, gdzie jest pasmo SBR. Ta sekcja decyduje, jak często
jest opisywane, jak dzieli się między kanały i jak często sama konfiguracja powtarza się w
strumieniu.

Rozdzielczość czasowa to bezpośredni kompromis między bitami a rozmazaniem. Jedna obwiednia
na ramkę oznacza jeden poziom na pasmo dla całej ramki, więc hi-hat wewnątrz tej ramki jest
uśredniany z otaczającą go ciszą. Cztery obwiednie śledzą kształt, ale czterokrotnie
zwiększają dane obwiedni. Zwykle tę decyzję podejmuje detektor transjentów dla każdej
ramki, i dlatego wymuszenie stałej liczby jest większą ingerencją, niż wygląda.

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
utrzymywanym padzie, organach albo podkładzie smyczkowym wymuszenie 2 lub 4 obwiedni kupuje
ci rozdzielczość czasową, o którą detektor by nie poprosił, a koszt jest przewidywalny. Na
perkusji statyczna siatka oznacza, że ataki nie dostają już własnej obwiedni, i dostajesz
dokładnie to rozmazanie wstecz, któremu detektor ma zapobiegać.

Wartość 8 przekroczyłaby siatkę ze standardu i jest odrzucana.

Wartości:
- `1` jedna obwiednia na ramkę. Najtaniej, najbardziej tępo; górne pasmo trzyma jeden
  poziom na pasmo przez całą ramkę. Spodziewaj się słyszalnego rozmazania na czymkolwiek
  transjentowym.
- `2` dwie obwiednie na ramkę. Złoty środek i sensowne wymuszone ustawienie na materiale
  stabilnym przy niskim bitrate.
- `4` cztery obwiednie na ramkę. Najdrobniejsza statyczna rozdzielczość, najwięcej danych
  obwiedni. Dobre na dźwięki utrzymywane i dzwoniące; na ostrych atakach nadal przegrywa z
  siatką dynamiczną, bo podziały są w stałych miejscach, a nie w miejscu ataku.
- `-1` wyłączone. Detektor transjentów wybiera dla każdej ramki. To lepsze ustawienie
  domyślne, a pokrętła do jego kształtowania są w sekcji o detektorze transjentów poniżej.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-num-env 4 -o out.m4a organ.wav`

Współpraca i konflikty: to nadpisuje tę część podziału ramki, na którą wpływają
`--sbr-tran-thr` i `--sbr-tran-split`, więc łączenie ich jest sprzeczne: jeśli chcesz
lepszego zachowania na atakach, zostaw `--sbr-num-env` wyłączone i zamiast tego obniż
`--sbr-tran-thr`. Koszt bitowy obwiedni ustawia `--sbr-amp-res`.

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
- `-1` wyłączone: wybór samego kodera, który idzie za tabelą strojenia i typem ramki.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-freqres-fixfix 1 -o out.m4a in.wav`

Współpraca i konflikty: to jest oś częstotliwości; `--sbr-num-env` to oś czasu. Oba mnożą
się w ten sam budżet danych obwiedni, a `--sbr-amp-res` ustala cenę za poziom. Dotyczy tylko
ramek bez transjentów, więc na mocno perkusyjnym materiale jego udział w pliku jest
mniejszy.

#### `--sbr-stereo-mode <0..3>`

**Wybiera, jak górne pasmo SBR jest dzielone między dwa kanały, niezależnie od kodowania
stereo w rdzeniu.**

- Dotyczy: tylko profili z SBR, wejście stereo
- Wartości: `0` mono, `1` LR, `2` coupling, `3` switch-LRC; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Rdzeń i SBR podejmują osobne decyzje stereo. Rdzeń może kodować mid/side, podczas gdy SBR
stosuje coupling, albo odwrotnie. Ten przełącznik dotyczy wyłącznie górnego pasma.

Pełna separacja lewy/prawy wysyła niezależny komplet parametrów SBR na kanał: najszersza,
najwierniejsza góra, za cenę mniej więcej podwojonych danych pobocznych SBR. Coupling wysyła
jedną wspólną obwiednię plus informację o poziomie, co jest znacznie tańsze i ściąga obraz
stereo na górze w stronę środka. Na większości materiału to ściągnięcie jest mniej słyszalne,
niż można by się spodziewać, i dlatego jest normalnym wyborem przy niskim bitrate; na
szeroko, twardo spanoramowanych talerzach i na nagraniach z prawdziwą różnicą pomieszczenia
między kanałami słychać je wyraźnie.

Wartości:
- `0` mono. Jedno górne pasmo dla obu kanałów. Sensowne tylko na treści faktycznie
  monofonicznej albo jako świadoma karykatura, żeby usłyszeć, co dają pozostałe tryby.
- `1` LR, pełna separacja. Niezależne górne pasmo na kanał. Ustawienie audiofilskie przy
  wyższym bitrate; kosztowne przy 32 kbps.
- `2` coupling. Wspólna obwiednia plus poziom i balans. Tanio, nieco węższa góra.
- `3` switch-LRC. Koder decyduje dla każdej ramki, co jest sensownym przypadkiem ogólnym i
  na to właśnie sprowadza się zachowanie domyślne.
- `-1` wyłączone: decyduje koder, jak przy `3`.

Przykład w kontekście: `fdkaac -p 5 -b 96000 --sbr-stereo-mode 1 -o out.m4a in.wav`

Współpraca i konflikty: w `-p 29` (HE-AAC v2) obraz opisuje także parametric stereo, więc
wymuszanie tutaj LR kłóci się z założeniem tego profilu; przy bitrate'ach HE-AAC v2 bity
lepiej wydać gdzie indziej. Przełączniki stereo rdzenia z grupy B to osobna decyzja i nie
nadpisują tego.

#### `--sbr-header-period <n>`

**Ustawia, co ile ramek zapisywane są nagłówki SBR, co decyduje, jak długo dekoder
dołączający się do strumienia na żywo słyszy sam rdzeń, zanim pojawi się górne pasmo.**

- Dotyczy: tylko profili z SBR. W HE-AAC i HE-AAC v2 steruje nagłówkiem `sbr_header` w
  strumieniu; w AAC-ELD z SBR nie ma nagłówka SBR w strumieniu, więc okres dotyczy
  StreamMuxConfig w LATM/LOAS i wymaga `-f 6`, `-f 7` albo `-f 10`
- Wartości: ramki, `1` wzwyż; domyślnie `-1` = wyłączone, własny okres FDK około 10 ramek
  (około 0,46 s przy 44,1 kHz)
- Wpisywanie: liczba całkowita

Cała konfiguracja SBR jest zapisana w okresowym nagłówku, a nie w każdej ramce. Słuchacz, który
podłącza się do strumienia Icecast, Shoutcast albo HLS w środku, od razu dekoduje rdzeń i
słyszy stłumioną, obciętą dolnoprzepustowo wersję programu aż do nadejścia kolejnego
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
  realna część budżetu, i dlatego tam jest karykaturą, a przy 96 kbps rozsądnym wyborem.

Przykłady, wydłużanie okresu:
- Łagodnie: `--sbr-header-period 25`. Około sekundy między nagłówkami przy 44,1 kHz. Ledwie
  zauważalne na strumieniu, przy którym ludzie zostają.
- Mocno: `--sbr-header-period 100`. Kilka sekund dźwięku z samego rdzenia dla nowego
  słuchacza. Oszczędza bajty nagłówków, brzmi jak zepsute dla każdego, kto przeskakuje
  między stacjami.
- Przesadnie: `--sbr-header-period 5000`. Minuty między nagłówkami. Dla pliku nieszkodliwe;
  dla strumienia oznacza, że większość słuchaczy nigdy nie usłyszy górnego pasma.

Współpraca i konflikty: `--sbr-header-time` wyraża to samo w sekundach i wygrywa, gdy
podasz oba. `--sbr-data-extra 1` powiększa każdy nagłówek, więc razem te dwa mnożą narzut.
`--verbose` wypisuje efektywny okres w ramkach i w milisekundach.

#### `--sbr-header-time <s>`

**To samo ustawienie co `--sbr-header-period`, wyrażone w sekundach zamiast w ramkach.**

- Dotyczy: tylko profili z SBR; w AAC-ELD z SBR ustawia okres StreamMuxConfig w LATM/LOAS,
  więc używaj `-f 6`, `-f 7` albo `-f 10`
- Wartości: `0.01..600` sekund; domyślnie `-1` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Ramki są niewygodną jednostką, gdy operator strumienia określa wymaganie, bo czas trwania
ramki zależy od częstotliwości próbkowania i profilu. Ten przełącznik bierze sekundy wprost
i przelicza je za ciebie, więc ta sama linia poleceń zachowuje się tak samo przy 44,1 i
48 kHz.

Wygrywa z `--sbr-header-period`, gdy podasz oba.

Przy AAC-ELD w ADTS albo M4A nie ma czego powtarzać: konfiguracja siedzi tylko w
kontenerze, a `--verbose` raportuje okres nagłówka SBR jako okres konfiguracji LATM/LOAS, a
nie jako nagłówek SBR w strumieniu.

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

Współpraca i konflikty: nadpisuje `--sbr-header-period`. Wynik sprawdź w `--verbose`, który
wypisuje efektywny okres na oba sposoby. Jako separator dziesiętny przyjmowane są zarówno
`.`, jak i `,`.
### Detektor transjentów SBR: `--sbr-tran-peak` `--sbr-tran-quiet` `--sbr-tran-dom` `--sbr-tran-thr` `--sbr-tran-split`

SBR dzieli każdą ramkę na szczeliny czasowe i na podstawie energii w tych szczelinach
decyduje, czy w górnym pasmie coś zaatakowało. Jeśli uzna, że tak, atak dostaje własną
krótką obwiednię, a skok poziomu jest zakodowany tam, gdzie faktycznie następuje. Jeśli
uzna, że nie, jedna obwiednia pokrywa cały odcinek, a energia uderzenia zostaje po nim
uśredniona.

Uśrednianie przez atak jest słyszalne i słychać je *przed* atakiem, a nie po nim: tuż przed
hi-hatem albo klaśnięciem pojawia się krótki wybuch wysokoczęstotliwościowego syku. To
pre-echo, a w załatanym pasmie jest to jeden z łatwiej rozpoznawalnych artefaktów
kodowania, bo materiał na górze jest już i tak dość syntetyczny, więc rozmycie czyta się
jako szum, a nie jako miękki atak.

Do tego detektora sięga pięć pokręteł. Jedno z nich, `--sbr-tran-thr`, jest znacznie
skuteczniejsze od pozostałych i od niego warto zacząć. Dwa z nich, `--sbr-tran-quiet` i
`--sbr-tran-dom`, siedzą w drugim, szybszym detektorze, który działa tylko w AAC-ELD z SBR,
więc w HE-AAC i HE-AAC v2 nie robią nic.

Wszystkie pięć to pokrętła x100 i wszystkie działają w obu kierunkach: `100` oznacza
dokładnie stockową stałą, poniżej 100 stała maleje, powyżej 100 rośnie. `--sbr-tran-peak`
jest wyjątkiem co do znaczenia, nie co do formatu: zastępuje stałą, zamiast ją skalować.

#### `--sbr-tran-thr <n>`

**Skaluje główny próg, który decyduje o tym, jak chętnie detektor transjentów SBR w ogóle
się odpala, i jest to jedyne pokrętło w tej sekcji o dużym, zmierzonym efekcie.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor porównuje energie szczelin z progiem wyznaczonym z sygnału. Obniż próg i ataki z
pogranicza, które wcześniej przechodziły niezauważone, dostają teraz własną obwiednię;
podnieś go i własną obwiednię dostają tylko najbardziej oczywiste uderzenia. Skoro to
przeoczony atak produkuje pre-echo, obniżanie tej wartości jest lekarstwem wprost, a kosztem
są dane obwiedni: więcej wykrytych transjentów to więcej obwiedni, a więcej obwiedni to
więcej bitów, zabranych rdzeniowi.

To pokrętło, które uzasadnia istnienie całej grupy. Na materiale perkusyjnym usuwa
pre-echo w górnym pasmie niemal całkowicie.

Zmierzone, pre-echo w pasmie SBR. Sygnał testowy: 24 ataki perkusyjne o charakterze
hi-hatu i klaśnięcia plus cztery wybrzmiewające tony między 8 a 13 kHz. Metryka: energia
powyżej 9 kHz pojawiająca się w dwóch okienkach 5,8 ms przed każdym atakiem, której nie ma
w źródle, więc mniej znaczy lepiej. Zakodowane jako HE-AAC, zdekodowane, wcześniej
dopasowane w czasie korelacją wzajemną:

| `--sbr-tran-thr` | pre-echo przy 32 kbps | pre-echo przy 64 kbps |
|---|---|---|
| stock | 1.610 | 1.186 |
| 20 / 40 / 60 | 0.041 | 0.059 |
| 80 | 0.661 | 0.502 |
| 100 i więcej | 1.610 (= stock) | 1.186 (= stock) |

Efekt jest monotoniczny, odtwarzalny na obu bitrate'ach i poniżej 60 wchodzi w nasycenie.
Wartości 150 i 400 też zmieniają strumień, tylko w drugą stronę.

Przykłady, obniżanie progu (detektor odpala się chętniej):
- Łagodnie: `--sbr-tran-thr 80`. Pre-echo na zmierzonym sygnale zmniejszone z grubsza o
  połowę, umiarkowany dodatkowy koszt obwiedni.
- Mocno: `--sbr-tran-thr 40`. Na materiale perkusyjnym pre-echo praktycznie znika. To
  zalecany punkt startu, kiedy słyszysz rozmycie na górze.
- Przesadnie: `--sbr-tran-thr 1`. Detektor odpala się na niemal wszystkim. Poniżej 60 nie
  ma już nic do ugrania w pre-echu, więc kupujesz wyłącznie dane obwiedni wydane na
  niezdarzenia, co przy niskim bitrate'ie zabiera bity rdzeniowi i pogarsza cały plik.

Przykłady, podnoszenie progu (detektor odpala się rzadziej):
- Łagodnie: `--sbr-tran-thr 150`. Ataki z pogranicza są ignorowane, trochę mniej obwiedni,
  trochę więcej rozmycia. Czasem warto na materiale spokojnym przy bardzo ciasnym budżecie.
- Mocno: `--sbr-tran-thr 400`. Własną obwiednię dostają tylko ataki niebudzące wątpliwości.
  Spodziewaj się wyraźnie więcej pre-echa na perkusji.
- Przesadnie: `--sbr-tran-thr 10000`. Detektor praktycznie nigdy się nie odpala; każda
  ramka jest opisana statyczną siatką. Karykatura, i to przydatna, jeśli chcesz usłyszeć
  artefakt, któremu ten detektor ma zapobiegać.

Współpraca i konflikty: `--sbr-num-env` wymusza statyczną siatkę, a więc usuwa korzyść z
podziału czasu, którą kupuje to pokrętło, co sprawia, że łączenie ich jest wzajemnym
sabotażem. `--sbr-tran-split` zajmuje się osobnym pytaniem, czyli dzieleniem ramek, w
których żaden transjent nie został wykryty. Na pre-echo w *rdzeniu* (LC, LD, ELD)
narzędzia są w grupie E.

#### `--sbr-tran-split <n>`

**Skaluje próg dzielenia na dwie obwiednie ramki, w której nie wykryto żadnego
transjentu.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Nie każda zmiana poziomu w górnym pasmie jest atakiem. Crescendo, narastanie, zmiana
akordu albo nierówno wybrzmiewający talerz poruszają górnym pasmem, nie wyzwalając
detektora transjentów, a jedna obwiednia na całą ramkę ten ruch uśrednia. Ten próg
decyduje o tym, jak chętnie taka ramka zostanie jednak podzielona na dwie, tak żeby druga
połowa dostała własne poziomy.

Obniżanie dzieli częściej: drobniejsza rozdzielczość czasowa na materiale, który się
porusza, ale nie atakuje, kosztem drugiego zestawu poziomów obwiedni w wielu ramkach.
Podnoszenie dzieli rzadziej: taniej, a górne pasmo mniej dokładnie podąża za powolnym
ruchem.

Efekt jest mniejszy niż przy `--sbr-tran-thr` i dotyczy innego objawu. Jeśli problemem jest
syk przed uderzeniem, użyj progu; jeśli problemem jest górne pasmo brzmiące statycznie,
gdy muzyka narasta, użyj tego.

Przykłady, obniżanie (dzielenie częściej):
- Łagodnie: `--sbr-tran-split 70`. Trochę więcej ramek dostaje dwie obwiednie.
- Mocno: `--sbr-tran-split 30`. Większość ramek z jakimkolwiek ruchem zostaje podzielona.
  Zauważalnie więcej danych obwiedni, lepsze podążanie za narastaniami.
- Przesadnie: `--sbr-tran-split 1`. Niemal każda ramka podzielona bez względu na treść;
  koszt bitowy jest realny, a korzyść na materiale spokojnym żadna.

Przykłady, podnoszenie (dzielenie rzadziej):
- Łagodnie: `--sbr-tran-split 150`. Marginalnie mniej podziałów, marginalnie taniej.
- Mocno: `--sbr-tran-split 300`. Podziały stają się rzadkie. Zmierzone: to zmienia strumień
  HE-AAC.
- Przesadnie: `--sbr-tran-split 10000`. Żadna ramka nie zostaje podzielona wyłącznie z
  powodu ruchu. Jedna obwiednia na ramkę bez transjentu, maksymalne rozmycie powolnych
  zmian w górnym pasmie, minimum danych obwiedni.

Współpraca i konflikty: w praktyce ignorowane, gdy `--sbr-num-env` wymusza statyczną
siatkę. Uzupełnia `--sbr-tran-thr`, a nie dubluje: tamto dotyczy ataków, to ramek bez
ataku. Każda dodatkowa obwiednia jest wyceniana przez `--sbr-amp-res`.

#### `--sbr-tran-peak <n>`

**Ustala, jak szczytowy musi być kandydat, żeby policzył się jako atak: sygnał musi spaść
poniżej tego ułamka poprzedniej szczeliny.**

- Dotyczy: profili z SBR
- Wartości: `1..200`, surowe x100, gdzie `90` = stockowe 0,90; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Prawdziwy atak nie jest tylko głośny, jest też krótki: energia rośnie, a potem znów spada.
Detektor sprawdza ten spadek, wymagając, żeby kolejna szczelina spadła poniżej ułamka
poprzedniej. Tym ułamkiem jest właśnie ta stała.

Uważaj na jednostkę. To **surowa** wartość x100, a nie współczynnik skalujący: zastępuje
stałą wprost, więc `90` to stockowe 0,90 i nic nie zmienia, a `100` to 1,0, a nie "stock".
A 1,0 ma szczególną konsekwencję: ułamek równy 1,0 albo większy oznacza, że warunek nigdy
nie może zostać spełniony przez prawdziwy spadek w taki sposób, w jaki detektor go
sprawdza, więc żadna szczelina nigdy nie liczy się jako spadająca. Każda wartość od 100 do
200 produkuje zatem jeden i ten sam strumień. Żywy zakres to 1..99.

Obniżanie wartości wymaga ostrzejszego spadku, więc jako ataki kwalifikuje się mniej
kandydatów. Podnoszenie w stronę 99 przyjmuje za ataki łagodniejsze wybrzmienia, więc
kwalifikuje się ich więcej.

Przykłady, obniżanie (surowiej, mniej transjentów):
- Łagodnie: `--sbr-tran-peak 80`. Trochę surowiej niż stock; miękkie ataki przestają się
  kwalifikować.
- Mocno: `--sbr-tran-peak 50`. Liczą się tylko ostro wybrzmiewające uderzenia. Spodziewaj
  się więcej rozmycia na wszystkim poza suchą perkusją.
- Przesadnie: `--sbr-tran-peak 1`. Wymóg spadku jest absurdalny; prawie nic nie kwalifikuje
  się jako atak i sam test szczytu tłumi detektor.

Przykłady, podnoszenie (luźniej, więcej transjentów):
- Łagodnie: `--sbr-tran-peak 95`. Łagodniejsze wybrzmienia liczą się teraz jako ataki.
- Mocno: `--sbr-tran-peak 99`. Najluźniejsze ustawienie, które jeszcze cokolwiek robi.
  Ataki z długimi ogonami dostają własne obwiednie.
- Przesadnie: `--sbr-tran-peak 200`. Poza użytecznym zakresem: 100 i więcej znaczy zawsze
  1,0, żadna szczelina nie liczy się jako spadająca, a wartości od 100 do 200 to jeden i
  ten sam strumień. Przydatne tylko jako pokaz, że to pokrętło ma twardą krawędź.

Współpraca i konflikty: to jeden z kilku warunków; `--sbr-tran-thr` skaluje główny próg i
ma znacznie większą dźwignię. Jeśli chcesz w ogóle więcej albo mniej transjentów, zacznij
od tamtego, a tego użyj tylko do zmiany *charakteru* tego, co się kwalifikuje.

#### `--sbr-tran-quiet <n>`

**Ustala podłogę energii cichej szczeliny w szybkim detektorze transjentów SBR, która
decyduje o tym, jak chętnie ciche fragmenty produkują transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2, które
  używają drugiego detektora
- Wartości: `1..10000`, x100, gdzie `100` = stock; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

AAC-ELD z SBR uruchamia drugi detektor transjentów, o niższym opóźnieniu, a ten detektor
ma podłogę energii, poniżej której szczelina jest traktowana jako cicha. Podłoga istnieje
po to, żeby szum w ciszy nie rejestrował się jako ataki. Obniż ją i detektor zagląda w
cichsze fragmenty, więc miękkie uderzenia w rzadkiej aranżacji mogą dostać własną
obwiednię. Podnieś ją i cichy materiał jest ignorowany.

Zachowanie silnie zależy od materiału, co warto wiedzieć, zanim poświęcisz na to czas: na
sygnale testowym czysto tonalnym detektor nigdy nie doszedł do decyzji, którą to pokrętło
mogłoby zmienić.

Zmierzone na AAC-ELD z SBR, 24 kbps, na pięciu sygnałach testowych: wartości od 30 do 99
zmieniają strumień, `100` i więcej nie dały żadnej zmiany, a `1` i `10` dają ten sam
strumień co siebie wzajemnie. Żywy zakres to więc w dół z grubsza 11..99, z efektem podłogi
poniżej i sufitem przy 100.

Przykłady, obniżanie (ciche fragmenty chętniej produkują transjenty):
- Łagodnie: `--sbr-tran-quiet 90`. Niewielkie rozszerzenie na cichsze szczeliny.
- Mocno: `--sbr-tran-quiet 50`. Wyraźnie czulej na materiale rzadkim; zmierzone, że zmienia
  strumień.
- Przesadnie: `--sbr-tran-quiet 1`. Na podłodze: `1` i `10` dają ten sam strumień, więc
  detektor dobił do dna tego, co ta podłoga potrafi wyrazić.

Przykłady, podnoszenie (ciche fragmenty ignorowane):
- Od łagodnie do przesadnie: `--sbr-tran-quiet 120`, `300`, `10000` nie dały w naszych
  pomiarach żadnej zmiany strumienia na ELD plus SBR 24 kbps. Stockowa podłoga jest już
  tak wysoka, że dalsze jej podnoszenie nie zmienia na tym materiale żadnej decyzji.
  Uczciwy status: kierunek jest podłączony, efektu w górę nie zaobserwowano.

Współpraca i konflikty: nic w HE-AAC ani HE-AAC v2, bo szybki detektor nie jest tam
używany. W obrębie ELD plus SBR stoi obok `--sbr-tran-dom`; `--sbr-tran-thr` i
`--sbr-tran-split` dotyczą głównego detektora.

#### `--sbr-tran-dom <n>`

**Ustala, o ile głośniejsza musi być szczelina od dwóch poprzedzających, kiedy te są już
oznaczone jako transjenty.**

- Dotyczy: wyłącznie AAC-ELD z SBR (`-p 39 -L 1`). Bez efektu w HE-AAC i HE-AAC v2
- Wartości: `100..500`, x100, gdzie `140` = stockowe 1,4; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Werbel, flam albo podwójne uderzenie produkują kilka szczelin kandydujących z rzędu. Ta
reguła nie pozwala detektorowi oznaczyć wszystkich: kiedy dwie poprzedzające szczeliny są
już transjentami, następna musi być wyraźnie głośniejsza, o ten właśnie współczynnik, żeby
też została oznaczona. To test dominacji, stąd nazwa.

Obniżanie współczynnika w stronę 100 pozwala liczyć się wszystkim kolejnym szczelinom, więc
gęsta perkusja produkuje więcej transjentów. Podnoszenie oznacza, że liczy się tylko
zdecydowana eskalacja, więc werbel rejestruje się raz.

Uczciwy status: to pokrętło jest podłączone do stałej, którą dokumentuje, i w naszych
pomiarach nie dało żadnej zmiany strumienia. Wartości od 100 do 500 dały identyczne wyjście
na pięciu sygnałach testowych na ELD plus SBR. Powód jest najprawdopodobniej taki, że
warunki wokół rzadko są na tym materiale spełnione jednocześnie, więc o wyniku nie decyduje
test dominacji. Traktuj je jako dostępne do eksperymentów, a nie jako narzędzie o
przewidywalnym rezultacie, i sięgnij raczej po `--sbr-tran-thr`.

Przykłady, obniżanie (kolejne szczeliny liczą się chętniej):
- Łagodnie: `--sbr-tran-dom 130`. Trochę poniżej stocku.
- Mocno: `--sbr-tran-dom 110`. Praktycznie żaden wymóg dominacji.
- Przesadnie: `--sbr-tran-dom 100`. Współczynnik wynosi 1,0, więc szczelina musi jedynie
  dorównać dwóm przed nią. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (liczy się tylko zdecydowana eskalacja):
- Łagodnie: `--sbr-tran-dom 180`. Powyżej stocku.
- Mocno: `--sbr-tran-dom 300`. Szczelina musi mieć trzykrotny poziom swoich poprzedników.
- Przesadnie: `--sbr-tran-dom 500`, maksimum. Przy żadnej z tych wartości nie zaobserwowano
  zmiany strumienia.

Współpraca i konflikty: ten sam szybki detektor ELD plus SBR co przy `--sbr-tran-quiet` i
to samo ograniczenie: nic w HE-AAC. Jeśli chcesz, żeby zachowanie transjentowe tego
detektora zmieniło się słyszalnie, to `--sbr-tran-quiet` w zakresie 30..99 jest tym o
zmierzonym efekcie.

### Detektor brakujących harmonicznych: `--sbr-mh-tone` `--sbr-mh-diff` `--sbr-mh-decay-orig` `--sbr-mh-decay-diff` `--sbr-mh-sfm-sbr` `--sbr-mh-sfm-orig` `--sbr-mh-maxcomp` `--sbr-mh-deltatime`

Detektor transjentów zajmuje się czasem. Ten detektor zajmuje się problemem, który tworzy
samo kopiowanie.

SBR buduje górne pasmo, łatając je widmem z dołu. Ton, który istniał w oryginalnym górnym
pasmie, nie ma gwarancji, że to przetrwa: łata podnosi to, co akurat było w obszarze
źródłowym, więc prawdziwa składowa może wylądować na złej częstotliwości, przybyć w
towarzystwie składowych, których nigdy tam nie było, albo po prostu zniknąć. Na dzwonkach,
trójkącie, dzwonie albo jasnej harmonicznej smyczków to jest dokładnie ta treść, którą
zauważasz. Detektor brakujących harmonicznych porównuje tonalność oryginalnego pasma z
tonalnością pasma załatanego, decyduje, że tonu brakuje, i sygnalizuje dekoderowi, żeby
wstawił w tym paśmie syntetyczną sinusoidę.

Oba kierunki porażki są słyszalne. Zbyt gorliwie i dostajesz świstanie: stałe, dźwięczne
tony na górze, których nigdy nie było w nagraniu, często trochę obok prawdziwego szeregu
harmonicznego, czyli klasyczne "falowanie SBR". Zbyt zachowawczo i dzwony oraz talerze
matowieją, gdy ich składowe po cichu znikają.

Do tego detektora sięga osiem pokręteł. Zorientuj się w ich prawdziwym statusie, zanim
poświęcisz tu czas: na naszym materiale testowym tylko dwa z nich w ogóle poruszyły
strumień. `--sbr-mh-tone` zmieniło wyjście na sygnale tonalnym w HE-AAC 24 kbps przy
wartościach 1, 5 i 10, a `--sbr-mh-diff` przy wartości 10. Pozostałych sześć nie dało
żadnej zmiany strumienia w HE-AAC od 24 do 96 kbps na materiale tonalnym i mieszanym, w
żadnym kierunku. To właściwość tego detektora na tym materiale, a nie skalowania: pokrętła
x100 działają tu w obu kierunkach, a `100` to dokładnie stock. Detektor po prostu potrzebuje
zbieżności kilku warunków, zanim którykolwiek z nich zdecyduje o wyniku, a na zwyczajnej
muzyce rzadko do tego dochodzi. Każda karta poniżej mówi wprost, jak z tym jest.

#### `--sbr-mh-tone <n>`

**Skaluje to, jak tonalne musi być pasmo, zanim SBR sfabrykuje w nim syntetyczną
harmoniczną, i jest to główna kontrolka tej sekcji.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor mierzy tonalność w oryginalnym paśmie i porównuje ją z progiem. Skalowanie tego
progu w dół sprawia, że więcej pasm kwalifikuje się jako tonalne, więc dodawanych jest
więcej syntetycznych harmonicznych. Skalowanie w górę sprawia, że kwalifikuje się ich
mniej.

To, co słychać w każdym kierunku, jest konkretne. Więcej dodanych tonów oznacza, że więcej
dźwięcznej treści dzwonu albo talerza zostaje przywrócone, i większe ryzyko artefaktów
świstania, których nigdy nie było w źródle. Mniej dodanych tonów oznacza bezpieczniejszą,
bardziej matową górę, w której jasna perkusja traci dzwonienie.

Zmierzone: to jedno z dwóch pokręteł w tej sekcji, które poruszyły strumień, na tonalnym
sygnale testowym w HE-AAC 24 kbps, przy wartościach 1, 5 i 10. Wszystkie leżą daleko
poniżej stocku, co pokazuje, jak nisko musi zejść próg, żeby detektor zmienił zdanie na tym
materiale.

Przykłady, obniżanie progu (więcej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 70`. Umiarkowane poszerzenie tego, co liczy się jako tonalne. Na
  wielu materiałach w ogóle nie zmieni to strumienia.
- Mocno: `--sbr-mh-tone 10`. Zmierzone, że zmienia strumień na materiale tonalnym.
  Spodziewaj się więcej dodanych tonów i jaśniejszego, bardziej dźwięcznego górnego pasma.
- Przesadnie: `--sbr-mh-tone 1`. Niemal każde pasmo liczy się jako tonalne. Zmierzone, że
  zmienia strumień, i to kierunek do wybrania, jeśli chcesz usłyszeć, jak brzmią artefakty
  świstania.

Przykłady, podnoszenie progu (mniej syntetycznych harmonicznych):
- Łagodnie: `--sbr-mh-tone 150`. Trochę surowiej.
- Mocno: `--sbr-mh-tone 400`. Harmoniczną dostają tylko pasma silnie tonalne.
- Przesadnie: `--sbr-mh-tone 10000`. Detektor praktycznie nigdy nie dodaje harmonicznej.
  Powyżej 100 nie zaobserwowano na naszym materiale zmiany strumienia, więc jeśli chcesz
  takiego wyniku, sprawdź to na swoim, zanim na tym polegniesz.

Współpraca i konflikty: `--sbr-mh-diff` to druga ścieżka wyzwalania tej samej decyzji, a
`--sbr-mh-sfm-orig` / `--sbr-mh-sfm-sbr` przesuwają granicę między tonem a szumem, która
zasila pomiar tonalności. `--sbr-mh-maxcomp` kontroluje to, co dzieje się wokół
harmonicznej, kiedy już została dodana, a nie to, czy zostaje dodana.

#### `--sbr-mh-diff <n>`

**Skaluje różnicę tonalności między oryginałem a łatą, która wyzwala kompensację
harmonicznych.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

`--sbr-mh-tone` pyta "czy oryginalne pasmo jest dość tonalne". To pyta o coś innego: "czy
łatanie zmieniło tonalność". To ścieżka, która wyłapuje przypadek, gdy oryginał miał jeden
czysty ton, a załatane pasmo ma ich kilka, albo odwrotnie, czyli dokładnie to, co psuje
kopiowanie.

Obniżanie progu sprawia, że mniejsza różnica liczy się jako szkoda, więc kompensacja
wyzwala się częściej. Podnoszenie oznacza, że liczy się tylko duża niezgodność.

Zmierzone: to drugie z dwóch pokręteł w tej sekcji, które poruszyły strumień, przy wartości
10 na naszym materiale. Wartości bliższe stocku i wartości powyżej stocku nie dały żadnej
zmiany.

Przykłady, obniżanie (kompensacja przy mniejszych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 70`. Umiarkowanie; na danym materiale może nie zmienić nic.
- Mocno: `--sbr-mh-diff 20`. Blisko wartości, przy której widzieliśmy zmianę.
- Przesadnie: `--sbr-mh-diff 10`. Zmierzone, że zmienia strumień. Niemal każda niezgodność
  tonalności między oryginałem a łatą wyzwala kompensację.

Przykłady, podnoszenie (kompensacja tylko przy dużych niezgodnościach):
- Łagodnie: `--sbr-mh-diff 150`.
- Mocno: `--sbr-mh-diff 400`.
- Przesadnie: `--sbr-mh-diff 10000`. Ścieżka różnicowa praktycznie nigdy się nie odpala,
  zostawiając `--sbr-mh-tone` jako jedyny wyzwalacz. W górę nie zaobserwowano na naszym
  materiale zmiany strumienia.

Współpraca i konflikty: obie ścieżki wyzwalania, ta i `--sbr-mh-tone`, zasilają tę samą
decyzję, więc podnoszenie jednej przy obniżaniu drugiej częściowo się znosi. Pokrętła
płaskości `--sbr-mh-sfm-sbr` i `--sbr-mh-sfm-orig` decydują o tym, co w ogóle liczy się
jako tonalne po każdej stronie porównania.

#### `--sbr-mh-decay-orig <n>`

**Skaluje to, jak długo już wykryty ton jest nadal śledzony w oryginalnym pasmie, gdy
wybrzmiewa.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor, który patrzyłby tylko na bieżącą ramkę, porzucałby ton, gdy tylko spadnie poniżej
progu, a na ogonie dzwonu albo talerza jest to dokładnie moment, w którym jeszcze go
chcesz. Dlatego detektor nadal śledzi ton, który już znalazł, z wybrzmiewaniem. To pokrętło
skaluje długość tej pamięci.

Dłuższe śledzenie oznacza, że syntetyczne harmoniczne utrzymują się w ogonach, więc
wybrzmienia pozostają dźwięczne, zamiast zamieniać się w szum. Krótsze śledzenie oznacza,
że tony są porzucane szybciej, więc ogony tracą dzwonienie wcześniej.

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
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Detektor prowadzi dwie prowadnice śledzące: jedną podążającą za tonami w oryginalnym
pasmie, drugą podążającą za różnicą tonalności między oryginałem a łatą. To pokrętło
skaluje pamięć drugiej, więc rządzi tym, jak długo wykryta *niezgodność* jest nadal
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
tego, czy `--sbr-mh-diff` faktycznie się wyzwala, zanim długość pamięci może mieć
znaczenie.

#### `--sbr-mh-sfm-sbr <n>`

**Skaluje próg płaskości widmowej, powyżej którego załatane pasmo jest uznawane za
szumopodobne, a nie tonalne.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

Płaskość widmowa mierzy, jak równo energia rozkłada się w paśmie: blisko 0 dla jednego
czystego tonu, blisko 1 dla szumu. Detektor używa jej do klasyfikowania każdego pasma, a to
pokrętło przesuwa granicę dla *załatanej* strony porównania.

Obniżanie progu oznacza, że załatane pasmo szybciej zostaje nazwane szumopodobnym, więc
detektor częściej dochodzi do wniosku, że kopia zgubiła ton i kompensacja jest potrzebna.
Podnoszenie oznacza, że łata dostaje kredyt zaufania jako tonalna, więc dodawanych jest
mniej harmonicznych.

Uczciwy status: podłączone, efektu nie zaobserwowano. Brak zmiany strumienia na materiale
tonalnym i mieszanym w HE-AAC od 24 do 96 kbps w żadnym kierunku.

Jedna praktyczna uwaga, jeśli mierzysz to samodzielnie: płaskość trzeba mierzyć w
prawdziwym obszarze SBR, od pasma rdzenia aż do końcowej górnej krawędzi wypisanej przez
`--verbose`. Okno sięgające do Nyquista obejmuje puste miejsce powyżej górnej krawędzi i
zwraca płaskość równą 0, co wygląda jak doskonała tonalność, a jest artefaktem okna.

Przykłady, obniżanie (łata szybciej nazywana szumopodobną, więcej kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 70`.
- Mocno: `--sbr-mh-sfm-sbr 30`.
- Przesadnie: `--sbr-mh-sfm-sbr 1`. Niemal każde załatane pasmo liczy się jako
  szumopodobne. Nie zaobserwowano zmiany strumienia.

Przykłady, podnoszenie (łata dłużej nazywana tonalną, mniej kompensacji):
- Łagodnie: `--sbr-mh-sfm-sbr 150`.
- Mocno: `--sbr-mh-sfm-sbr 400`.
- Przesadnie: `--sbr-mh-sfm-sbr 10000`. Załatane pasmo jest praktycznie zawsze tonalne. Nie
  zaobserwowano zmiany strumienia.

Współpraca i konflikty: działa w parze z `--sbr-mh-sfm-orig`; razem wyłapują sytuację "z
jednego tonu w oryginale po łataniu zrobiło się kilka". Przesunięcie tylko jednej strony
przechyla porównanie asymetrycznie, co bywa tym, czego chcesz, i jest trudniejsze do
przemyślenia.

#### `--sbr-mh-sfm-orig <n>`

**Ten sam próg płaskości co `--sbr-mh-sfm-sbr`, dla oryginalnego pasma.**

- Dotyczy: profili z SBR
- Wartości: `1..10000`, x100, gdzie `100` = stockowe x1,00; domyślnie `-1` = wyłączone
- Wpisywanie: liczba całkowita

To przesuwa granicę między tonem a szumem po stronie źródła. Obniżanie oznacza, że
oryginalne pasmo szybciej zostaje nazwane szumopodobnym, więc detektor rzadziej dochodzi do
wniosku, że był tam ton wart przywrócenia. Podnoszenie oznacza, że oryginał jest chętniej
traktowany jako tonalny, więc więcej pasm staje się kandydatami na syntetyczną harmoniczną.

Zwróć uwagę, że oba pokrętła sfm pchają końcową decyzję w przeciwnych sensach, choć są tym
samym rodzajem progu, bo jedno opisuje stan wejściowy, a drugie wynik. Jeśli
chcesz "dodawać więcej harmonicznych", obniż `--sbr-mh-sfm-sbr` i podnieś to.

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
sprawia, że wynik jest bardzo trudny do przypisania; ruszaj po jednym.

#### `--sbr-mh-maxcomp <n>`

**Ogranicza, ile kompensacji obwiedni jest stosowane wokół syntetycznej harmonicznej.**

- Dotyczy: profili z SBR
- Wartości: `0..200`, zwykła liczba całkowita, gdzie `50` = stock; domyślnie `-1` =
  wyłączone
- Wpisywanie: liczba całkowita

Wstawienie sinusoidy w pasmo zmienia energię tego pasma, więc obwiednia wokół niej musi
zostać skorygowana, inaczej pasmo wyjdzie za głośne. To pokrętło ogranicza tę korektę. Nie
jest to skala x100: liczba jest samym limitem.

Wyższy limit dopuszcza większe korekty, więc pasma sąsiednie są ściągane dalej i mniej
szumu zostaje obok dodanego tonu: harmoniczna wybija się czyściej. Niższy limit ogranicza
korektę, więc dodany ton siedzi w większej części swojego pierwotnego szumu otoczenia i się
w nim zlewa, albo staje się słyszalnie za głośny, jeśli potrzebna korekta była większa od
limitu.

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
x100, i wartość stockowa różni się zależnie od profilu.

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
- Łagodnie: `--sbr-mh-deltatime 16`, czyli stockowa wartość AAC-LD zastosowana do AAC.
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
dekoder nakłada na łatę filtr wybielający, który tę odziedziczoną strukturę spłaszcza. Są
cztery poziomy mocy, 0 wyłączone, 1 niski, 2 średni, 3 wysoki, wybierane na pasmo i na
ramkę. Poziom szumu to druga połowa tego samego lekarstwa: syntetyczny szum domieszany do
każdego pasma, który zastępuje strukturę usuniętą przez wybielanie. Za mało jednego i
drugiego daje metaliczną górę; za dużo daje górę syczącą i rozmytą, w której talerze
redukują się do kształtowanego szumu.

Poziom wybiera normalnie **estymator tonalności**, który porównuje pasmo oryginału z pasmem
załatanym, pasmo po paśmie i ramka po ramce. To tę dynamiczną decyzję warto zachować: ma
rację znacznie częściej niż jakakolwiek stała wartość, którą mógłbyś wybrać.

Ta sekcja zawiera więc dwa rodzaje przełączników, a pierwszeństwo między nimi ma znaczenie:

1. `--sbr-invf` **wymusza** jeden ustalony poziom na cały plik, w każdym paśmie i w każdej
   chwili. Wygrywa ze wszystkim innym w tym miejscu. Używaj go do diagnozy albo wtedy, gdy
   wiesz, że materiał jest jednorodny.
2. Wszystko pozostałe zostawia decyzję dynamiczną i zmienia reguły estymatora: granice,
   przesunięcie, tabelę decyzyjną, histerezę, wygładzanie, położenie granic, wagę szumu.
   Decyzja nadal jest podejmowana na pasmo i na ramkę, tylko według innych reguł.

W grupie 2 `--sbr-softness` jest makrem, które ustawia kilka z tych reguł naraz, a **każde
jawne pokrętło wygrywa z makrem**. Czyli `--sbr-softness 6 --sbr-invf-max 2` daje
miękkość z makra z własnym sufitem.

Sposób korzystania z tej sekcji kształtują dwa fakty pomiarowe.

Pierwszy: makro miękkości celowo *nie* rusza granic tonalności. Zmierzyliśmy, że ich
przesuwanie powoduje migotanie decyzji w obrębie pasma, od 0,2 do 0,29 zmiany na pasmo na
ramkę wobec 0,004 do 0,04 w stocku, co jest słyszalne jako niestabilność.
`--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` i tak udostępniają ten mechanizm, bo czasem
właśnie tego chcesz, ale to dlatego makro ich unika.

Drugi: waga poziomu szumu jest obcinana przez sufit szumu. Przy sufitach z tabeli strojenia
(0,125 przy 24 do 48 kbps) około 95 procent pasm i tak już siedzi na sufice, więc
`--sbr-nf-weight` samo w sobie często nie zmienia zupełnie nic. Żeby cokolwiek zrobiło,
potrzebuje podniesionego sufitu *oraz* pasm z wysokim invf: zmierzone, strumień ruszył
dopiero przy `--sbr-noise-max 6` w połączeniu z `--sbr-invf 3` albo
`--sbr-nf-weight-from 1`. Samo albo tylko z jednym z nich nie zmieniło nic. Poziomy od 5 do
7 w `--sbr-softness` podnoszą sufit za ciebie.

Przy wartościach neutralnych (`--sbr-invf-hyst 1.0`, `--sbr-invf-bias 0`,
`--sbr-invf-smooth 2`, `--sbr-nf-weight 0.25`, `--sbr-nf-weight-from 3`,
`--sbr-invf-table 0`, `--sbr-invf-min 0`, `--sbr-invf-max 3`) te pokrętła dają strumień
identyczny bajt w bajt ze stockiem, podobnie jak `--sbr-softness 2` i
`--speech --sbr-softness 4`.

#### `--sbr-invf <0..3>`

**Wymusza jeden ustalony poziom filtrowania odwrotnego na cały plik, całkowicie pomijając
estymator tonalności.**

- Dotyczy: profili z SBR (`-p 5`, `-p 29`, `-p 39 -L 1`)
- Wartości: `0` wyłączone, `1` niski, `2` średni, `3` wysoki; domyślnie `-1` = automat,
  czyli estymator tonalności decyduje na pasmo i na ramkę
- Wpisywanie: liczba całkowita

Wyżej znaczy mocniejsze wybielanie załatanego pasma: mniej metalicznego dzwonienia, mniej
szczegółu, góra bardziej płaska i bardziej szumopodobna. Niżej znaczy, że odziedziczona
struktura tonalna zostaje na miejscu: więcej pozornego szczegółu, więcej metalicznego
charakteru.

Wada wymuszania jest dokładnie taka, jak brzmi: jeden poziom obowiązuje wszędzie, także w
tych pasmach i momentach, w których estymator wybrałby inaczej. Plik z cichym, tonalnym
wstępem i gęstym refrenem chce w tych miejscach różnych poziomów, a wymuszona wartość daje
im ten sam.

Mimo tego jest to najbardziej przydatne pokrętło w tej sekcji, do dwóch zadań. Pierwsze to
diagnoza: zakoduj na `0` i na `3`, posłuchaj skrajności i od razu będziesz wiedział, którego
końca tej osi potrzebuje dany materiał. Drugie to materiał jednorodny, gdzie estymator i tak
nie ma do czego się dostrajać.

Wartości:
- `0` filtrowanie odwrotne wyłączone. Żadnego wybielania. Łata zachowuje odziedziczoną
  strukturę: maksimum pozornego szczegółu, maksimum metalicznego dzwonienia. Na materiale
  tonalnym to wyraźny artefakt; na materiale szumopodobnym może być najczystszą opcją.
- `1` niski. Lekkie wybielanie.
- `2` średni. Umiarkowane wybielanie, z grubsza tam, gdzie estymator siedzi średnio przy
  tabeli muzycznej.
- `3` wysoki. Mocne wybielanie. Góra staje się płaska i szumopodobna; metaliczne dzwonienie
  znika, a razem z nim spora część odczuwanego szczegółu.
- `-1` automat. Decyduje estymator. Ustawienie zalecane, chyba że właśnie diagnozujesz.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf 3 -o out.m4a in.wav`

Współpraca i konflikty: ten przełącznik **wygrywa z każdym innym w tej sekcji**, w tym z
`--sbr-softness` i z pokrętłami min/max/bias, bo nie zostaje już żadna dynamiczna decyzja do
ograniczania. Jeśli chcesz decyzję kształtować, a nie zastępować, zostaw go wyłączony.
Zestawienie `--sbr-invf 3` z `--sbr-noise-max 6` to jedna z dwóch kombinacji, które sprawiają,
że `--sbr-nf-weight` w ogóle coś robi.

#### `--sbr-softness <0..7>`

**Jedno makro od góry twardej i metalicznej do bardzo miękkiej, ustawiające kilka reguł
estymatora naraz i zachowujące dynamiczną decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0..7`; domyślnie `-1` = wyłączone (stock, co odpowiada poziomowi `2` dla muzyki)
- Wpisywanie: liczba całkowita

To pokrętło warto wypróbować pierwsze. Przesuwa cię po osi metaliczne kontra syczące bez
zamrażania decyzji, ustawiając tabelę decyzyjną, przesunięcie, wagę szumu, granice min/max
i sufit szumu w kombinacjach, które zmierzyliśmy jako monotoniczne i stabilne. Celowo nie
rusza granic tonalności, bo zmierzyliśmy, że to powoduje migotanie decyzji w obrębie pasma.

Wartości:
- `0` o jeden stopień mniej wybielania niż strojenie muzyczne: twardziej, bardziej
  metalicznie niż stock.
- `1` nigdy wybielanie HIGH. Zwróć uwagę, że na muzyce estymator i tak rzadko wybiera HIGH,
  więc na materiale muzycznym ta granica często wcale nie działa: zmierzone na HE-AAC
  32 kbps, poziomy `1` i `2` dały strumień identyczny ze stockiem na sygnałach tonalnym,
  głosowym i mieszanym.
- `2` stockowe estymatory muzyczne. Identyczny bajt w bajt z niekorzystaniem z przełącznika
  w ogóle.
- `3` co najmniej wybielanie LOW: estymator nie może już wybrać wyłączenia. Zmierzone jako
  równe `--sbr-invf 1` na sygnale tonalnym, bo tam estymator chciał wyłączenia wszędzie, a
  podłoga podniosła wszystko do LOW.
- `4` stockowe estymatory mowy: tabela decyzyjna mowy i waga szumu, bez pozostałej części
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
| 2 (stockowa muzyka) | 1.12 / 0.98 / 0.99 | 0.373 / 0.430 |
| 4 (stockowa mowa) | 1.93 / 1.96 / 1.98 | 0.379 / 0.440 |
| 5 | 1.93 / 1.96 / 1.98 | 0.403 / 0.518 |
| 7 | 2.93 / 2.96 / 2.98 | 0.417 / 0.521 |

Skala jest monotoniczna na każdej próbce, a zmiany trybu w obrębie pasma zostają na
stockowym poziomie (0,004 do 0,04 na pasmo na ramkę). Zmierzone również: poziom `0` równa
się `--sbr-invf 0` na materiale tonalnym i mieszanym, a poziomy od `4` do `7` różnią się
między sobą.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-softness 6 -o out.m4a in.wav`

Współpraca i konflikty: **każde jawne pokrętło poniżej wygrywa z tym makrem**, i właśnie tak
się je łączy: `--sbr-softness 6 --sbr-invf-max 2` zachowuje obsługę szumu z makra przy własnym
własnym sufice. `--sbr-invf` wygrywa z tym makrem całkowicie. Sąsiednie poziomy mogą dać
ten sam strumień, jeśli materiał nigdy nie dociera do granicy, którą się różnią; to
oczekiwane, a nie awaria.

#### `--sbr-invf-min <0..3>`

**Ustawia podłogę pod estymatorem: najniższy poziom filtrowania odwrotnego, jaki wolno mu
wybrać.**

- Dotyczy: profili z SBR
- Wartości: `0..3`; domyślnie `-1` = wyłączone (bez podłogi, odpowiednik `0`)
- Wpisywanie: liczba całkowita

Estymator nadal decyduje na pasmo i na ramkę; to tylko usuwa dolną część jego zakresu. To
narzędzie na „góra jest metaliczna w cichych fragmentach”, gdy estymator poprawnie widzi
pasmo tonalne i wybiera mało wybielania albo wcale, a ty się z tym nie zgadzasz.

Podniesienie podłogi gwarantuje minimalną ilość wybielania wszędzie, co kosztuje szczegół w
tych pasmach, które naprawdę go nie potrzebowały.

Wartości:
- `0` podłoga na wyłączeniu. Bez ograniczenia; identyczne z niekorzystaniem z przełącznika.
- `1` estymator nie może nigdy wybrać wyłączenia. Najdelikatniejsza użyteczna podłoga i ta,
  którą warto wypróbować, gdy metaliczne dzwonienie pojawia się tylko sporadycznie.
- `2` co najmniej wybielanie MID wszędzie. Mocna interwencja: pasma, które nie chciały
  żadnego, dostają teraz umiarkowane wybielanie.
- `3` co najmniej HIGH wszędzie, co oznacza, że poziom jest przypięty do HIGH, bo 3 to
  maksimum. W efekcie jest to odpowiednik `--sbr-invf 3`, a jeśli właśnie tego chcesz,
  powiedz to wprost.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 48000 --sbr-invf-min 1 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, razem z `--sbr-invf-max` jako drugim
końcem klamry. Ustawienie `--sbr-invf-min 3` albo `--sbr-invf-min` równego
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
- `1` nigdy powyżej LOW. Mocne ustawienie chroniące szczegół; spodziewaj się słyszalnego
  metalicznego charakteru na materiale tonalnym.
- `0` sufit jest na wyłączeniu, więc nigdzie nie wolno wybielać. W efekcie odpowiednik
  `--sbr-invf 0`, który lepiej zapisać właśnie tak.
- `-1` wyłączone.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-max 2 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: nakładane po `--sbr-invf-bias`, w parze z `--sbr-invf-min`.
`--sbr-softness 1` to zapakowana wersja `--sbr-invf-max 2`, i zwróć uwagę na zmierzone
zastrzeżenie w tamtym miejscu: na muzyce estymator rzadko dociera do HIGH, więc ten sufit
może nie robić nic na materiale muzycznym i jednocześnie być dokładnie tym, czego trzeba na
mowie. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-bias <-3..3>`

**Przesuwa każdą dynamiczną decyzję filtrowania odwrotnego w górę albo w dół o ustaloną
liczbę stopni.**

- Dotyczy: profili z SBR
- Wartości: `-3..3` stopnie; domyślnie `0` = wyłączone
- Wpisywanie: liczba całkowita

Tam gdzie min i max obcinają zakres, to pokrętło go przesuwa. Wzór decyzji estymatora na
pasmo i na ramkę jest zachowany dokładnie, a cały wzór się przesuwa. To czyni je
najczystszym sposobem powiedzenia „estymator ma dobry pomysł, tylko konsekwentnie jest za
miękki” albo „konsekwentnie za twardy”. Wartości dodatnie oznaczają więcej wybielania.

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

Współpraca i konflikty: nakładane **przed** `--sbr-invf-min` i `--sbr-invf-max`, więc możesz
najpierw przesunąć, a potem obciąć: `--sbr-invf-bias 2 --sbr-invf-max 2` przesuwa wszystko w
górę, ale odmawia HIGH. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-table <0|1>`

**Wybiera, której tabeli decyzyjnej używa estymator tonalności: tabeli muzycznej czy tabeli
mowy.**

- Dotyczy: profili z SBR
- Wartości: `0` muzyka, `1` mowa; domyślnie `-1` = idzie za `--speech`
- Wpisywanie: liczba całkowita

Progi estymatora pochodzą z tabeli, a FDK dostarcza dwie: jedną strojoną pod muzykę, drugą
pod mowę. Tabela mowy wybiela znacznie chętniej, bo mowa powyżej częstotliwości podziału to
w większości szczelinowe i oddech, gdzie odziedziczona struktura tonalna jest czystym
artefaktem i nie ma tam szczegółu wartego ochrony.

Ten przełącznik wybiera tabelę i **tylko** tabelę. `--speech` zmienia również częstotliwość
podziału i inne elementy strojenia; jeśli chcesz zachowania estymatora mowy na materiale
muzycznym bez przestawiania układu częstotliwości, to jest przełącznik do tego. Taka
kombinacja jest naprawdę przydatna: audycja słowna, audiobooki, materiał podcastowy przy
niskim bitrate'ie, gdzie chcesz zachowania wybielania, ale własnej częstotliwości podziału.

Wartości:
- `0` tabela muzyczna. Zachowawcze wybielanie, chroni szczegół tonalny.
- `1` tabela mowy. Wybiela znacznie chętniej. Zmierzony średni poziom invf przy tabeli mowy
  jest blisko 2,0 wobec około 1,0 przy tabeli muzycznej na HE-AAC 32 kbps.
- `-1` wyłączone: idź za `--speech`, czyli tabela muzyczna, chyba że podano `--speech`.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-invf-table 1 -o out.m4a speech.wav`

Współpraca i konflikty: `--sbr-softness 4` to zapakowana wersja tego plus waga szumu dla
mowy. `--speech` ustawia to niejawnie i przesuwa również częstotliwość podziału, i to jest
różnica warta zapamiętania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-hyst <x>`

**Ustawia histerezę granic decyzyjnych: jak bardzo musi zmienić się tonalność, zanim
estymator przełączy pasmo na inny poziom.**

- Dotyczy: profili z SBR
- Wartości: `0..2`, liczba dziesiętna, gdzie `1.0` = stock; domyślnie `-1` = wyłączone
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

Dla odniesienia: stockowe tempo przełączania w obrębie pasma to od 0,004 do 0,04 zmiany na
pasmo na ramkę, a konfiguracja zmierzona na 0,2 do 0,29 była wyraźnie niestabilna.

Przykłady, mniej histerezy (szybciej, więcej przełączania):
- Łagodnie: `--sbr-invf-hyst 0,8`. Nieco szybsze podążanie za zmianami.
- Mocno: `--sbr-invf-hyst 0,3`. Wyraźnie więcej przełączania; stosuj, gdy estymator wydaje
  się ospały na materiale, który szybko zmienia charakter.
- Przesadnie: `--sbr-invf-hyst 0`. Zupełny brak strefy martwej. Pasma siedzące na granicy
  przeskakują w każdej ramce, czyli dostajesz tę niestabilność, której histereza ma
  zapobiegać.

Przykłady, więcej histerezy (wolniej, stabilniej):
- Łagodnie: `--sbr-invf-hyst 1,2`. Odrobinę bardziej lepkie niż stock.
- Mocno: `--sbr-invf-hyst 1,5`. Poziomy trzymają się mimo umiarkowanych zmian tonalności.
  Dobra para dla `--sbr-invf-max 2`, gdy chcesz spokojnej, przewidywalnej góry.
- Przesadnie: `--sbr-invf-hyst 2`, czyli maksimum. Strefy martwe są tak szerokie, jak tylko
  mogą być; pasma zasadniczo trzymają ten poziom, na którym wylądowały na początku, co na
  materiale zmieniającym charakter jest po prostu złym poziomem przez większość czasu.

Współpraca i konflikty: `--sbr-invf-smooth` atakuje tę samą niestabilność z drugiej strony,
uśredniając po ramkach zamiast poszerzać granice; używanie obu w skrajnych ustawieniach jest
nadmiarowe. `--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` przesuwają te granice, które to
pokrętło poszerza. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-sbr-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma załatanego, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Estymator mierzy tonalność po obu stronach porównania i każdą z nich zestawia z granicami. To
pokrętło przesuwa granice po stronie załatanej. Wartości dodatnie oznaczają, że załatane
pasmo szybciej liczy się jako tonalne, a tonalna łata jest właśnie tym, po co jest
wybielanie, więc dodatnie oznaczają więcej invf.

To instrument ostrzejszy niż bias czy min/max, i bardziej ryzykowny. Przesuwanie granic
zmienia to, *które* pasma je przekraczają i w których ramkach, więc może wprowadzić migotanie
decyzji: dla tego mechanizmu zmierzyliśmy od 0,2 do 0,29 zmiany na pasmo na ramkę wobec 0,004
do 0,04 w stocku, i to dlatego `--sbr-softness` z niego nie korzysta. Jeśli po to sięgasz,
rozważ jednoczesne podniesienie `--sbr-invf-hyst`, żeby odkupić stabilność.

Przykłady, wartości dodatnie (załatane pasmo szybciej tonalne, więcej invf):
- Łagodnie: `--sbr-invf-sbr-thr 2`. Niewielkie przesunięcie; kilka pasm więcej przechodzi do
  wybielania.
- Mocno: `--sbr-invf-sbr-thr 8`. Dużo więcej pasm liczy się jako tonalne. Spodziewaj się
  zauważalnie większego wybielania i pewnego migotania.
- Przesadnie: `--sbr-invf-sbr-thr 20`, czyli maksimum. Zasadniczo każde załatane pasmo liczy
  się jako tonalne, więc pomiar przestaje rozróżniać, a ocena estymatora zostaje zastąpiona
  stałą. `--sbr-invf 3` robi to samo bez migotania.

Przykłady, wartości ujemne (załatane pasmo tonalne później, mniej invf):
- Łagodnie: `--sbr-invf-sbr-thr -2`. Odrobinę mniej wybielanych pasm.
- Mocno: `--sbr-invf-sbr-thr -8`. Wybielane są tylko mocno tonalne łaty; więcej szczegółu,
  więcej metalicznego charakteru.
- Przesadnie: `--sbr-invf-sbr-thr -20`, czyli minimum. Nic nie liczy się jako tonalne, więc
  nic nie jest wybielane. Karykatura „zachowaj cały szczegół”, i tak właśnie brzmi.

Współpraca i konflikty: odbicie `--sbr-invf-orig-thr`, a obydwa składają się na porównanie
oryginału z łatą. `--sbr-invf-hyst` jest przeciwwagą po stronie stabilności. Wygrywa z
`--sbr-softness`; przegrywa z `--sbr-invf`.

#### `--sbr-invf-orig-thr <x>`

**Przesuwa granice decyzji o tonalności dla pasma oryginalnego, z grubsza w dB.**

- Dotyczy: profili z SBR
- Wartości: `-20..20`, mniej więcej w dB, liczba dziesiętna; domyślnie `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Ten sam mechanizm po stronie źródła, przy czym znak czyta się odwrotnie, i to jest rzecz,
którą trzeba dobrze zrozumieć. Estymator wybiela, kiedy łata wygląda na bardziej tonalną,
niż na to zasługuje oryginał. Zatem sprawienie, żeby *oryginał* liczył się jako tonalny
**później**, oznacza, że porównanie częściej znajduje większą niezgodność, a więc wartości
dodatnie także tutaj oznaczają więcej wybielania. Oba pokrętła progowe prowadzą do tego
samego skutku, mimo że opisują przeciwne strony.

Zastrzeżenie o migotaniu obowiązuje tu identycznie: to jest przesunięcie granicy, a
przesunięcia granic destabilizują decyzję w obrębie pasma mocniej niż bias czy obcinanie.

Przykłady, wartości dodatnie (oryginał liczy się jako tonalny później, więcej invf):
- Łagodnie: `--sbr-invf-orig-thr 2`.
- Mocno: `--sbr-invf-orig-thr 8`. Oryginałowi rzadko przypisuje się tonalność, więc łata
  częściej wygląda na błędną i zostaje wybielona.
- Przesadnie: `--sbr-invf-orig-thr 20`, czyli maksimum. Oryginał nigdy nie liczy się jako
  tonalny, więc każda łata jest traktowana jak uszkodzenie. Maksymalne wybielanie drogą,
  która maksymalizuje też migotanie.

Przykłady, wartości ujemne (oryginał liczy się jako tonalny wcześniej, mniej invf):
- Łagodnie: `--sbr-invf-orig-thr -2`.
- Mocno: `--sbr-invf-orig-thr -8`. Oryginałowi hojnie przypisuje się tonalność, więc łata
  częściej jest uznawana za akceptowalną i zostawiana w spokoju.
- Przesadnie: `--sbr-invf-orig-thr -20`, czyli minimum. Wszystko w oryginale liczy się jako
  tonalne, porównanie nie znajduje niezgodności, a wybielanie praktycznie ustaje.

Współpraca i konflikty: para dla `--sbr-invf-sbr-thr`; przesuwanie obu w tę samą stronę
częściowo się znosi, bo jedno podnosi niezgodność, a drugie ją obniża. Poziom szumu odczytuje
tę samą różnicę między oryginałem a łatą przez `--sbr-nf-weight`, więc przesuwanie tych
granic zmienia także szum wypełniający, nie tylko wybielanie. Wygrywa z `--sbr-softness`;
przegrywa z `--sbr-invf`.

#### `--sbr-invf-smooth <0..2>`

**Ustawia, po ilu ramkach estymator tonalności uśrednia, zanim podejmie decyzję.**

- Dotyczy: profili z SBR
- Wartości: `0`, `1`, `2`; domyślnie `-1` = wyłączone (stock uśrednia po 2)
- Wpisywanie: liczba całkowita

Uśrednianie po ramkach to druga obrona przed niestabilną decyzją. Z uśrednianiem jedna
odbiegająca ramka nie przestawi poziomu wybielania pasma; bez niego każda ramka jest oceniana
sama. Wymiana jest typowa: stabilność za szybkość reakcji.

Wartości:
- `0` tylko bieżąca ramka. Najszybsza reakcja na prawdziwą zmianę charakteru, najwięcej
  przełączania z ramki na ramkę. Stosuj na materiale, który zmienia się szybko, i spodziewaj
  się pewnej chwiejności.
- `1` uśrednianie krótsze niż stockowe. Opcja pośrednia.
- `2` stock. Równowaga wybrana przez FDK, i to dobra na większości materiału.
- `-1` wyłączone, czyli to samo co `2`.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-invf-smooth 0 --sbr-invf-hyst 1.5 -o out.m4a in.wav`

Współpraca i konflikty: to pokrętło i `--sbr-invf-hyst` kupują stabilność, każde innym
sposobem, więc zestawienie niskiego wygładzania z wysoką histerezą to rozsądna droga do
szybkiej reakcji bez trzepotania. Wygrywa z `--sbr-softness`; przegrywa z `--sbr-invf`,
któremu nie zostaje już żadna decyzja do wygładzania.

#### `--sbr-nf-weight <x>`

**Ustawia, jak mocno różnica tonalności między oryginałem a łatą podnosi poziom szumu.**

- Dotyczy: profili z SBR, i tylko tam, gdzie sufit szumu zostawia zapas (patrz niżej)
- Wartości: `0.0..1.0`, liczba dziesiętna; domyślnie `-1` = idzie za `--speech`
  (muzyka 0,25, mowa 1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` lub `,`

Wybielanie usuwa strukturę; szum wypełniający ją zastępuje. Ta waga decyduje, jaka część
zmierzonej niezgodności między pasmem oryginału a pasmem załatanym zamienia się w poziom
szumu. Wyżej oznacza więcej szumu wypełniającego wszędzie tam, gdzie łata jest zbyt tonalna,
i to dlatego strojenie mowy używa 1,0: w mowie górne pasmo powinno być szumem, więc
wypełnianie go jest zawsze słuszne. Niżej oznacza, że różnicą zajmuje się głównie samo
wybielanie.

**Przeczytaj to, zanim tego użyjesz.** Ważony szum jest obcinany przez sufit szumu, a przy
sufice z tabeli strojenia (0,125 przy 24 do 48 kbps) około 95 procent pasm już na nim siedzi,
więc podnoszenie wagi nic nie zmienia, bo nie ma zapasu do wykorzystania. Zmierzone, to
pokrętło ruszyło strumień tylko przy `--sbr-noise-max 6` **plus** `--sbr-invf 3` albo
`--sbr-nf-weight-from 1`. Samo, albo tylko z jednym z tych dwóch, nie zmieniło zupełnie nic.
Potrzebuje podniesionego sufitu i pasm na wysokim poziomie invf, bo domyślnie wagę dostają
tylko pasma HIGH.

Działający przepis to zatem `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight <x>`,
albo pozwól `--sbr-softness 5`, `6` czy `7` podnieść sufit za ciebie.

Przykłady, więcej szumu wypełniającego (przy podniesionym sufice):
- Łagodnie: `--sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.4`. Trochę więcej
  powietrza tam, gdzie łata jest zbyt tonalna.
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
  domyślnym sufice może to spokojnie dać strumień identyczny bitowo ze stockiem, bo
  większość pasm i tak jest obcięta, co samo w sobie jest lekcją o tym pokrętle.

Współpraca i konflikty: potrzebuje `--sbr-noise-max 3` albo `6`, żeby mieć zapas, i
`--sbr-nf-weight-from`, żeby sięgnąć pasm poniżej HIGH. `--sbr-noise-floor-offset` podnosi
poziom szumu bezwarunkowo i jest bardziej topornym wyjściem awaryjnym, kiedy to pokrętło
odmawia ruchu. Wygrywa z `--sbr-softness`; `--sbr-invf 3` go nie blokuje, tylko jest jedną z
dwóch rzeczy, które go odblokowują.

#### `--sbr-nf-weight-from <1..3>`

**Ustawia najniższy poziom filtrowania odwrotnego, którego pasma dostają wagę poziomu
szumu.**

- Dotyczy: profili z SBR
- Wartości: `1` LOW i wyżej, `2` MID i wyżej, `3` tylko HIGH; domyślnie `-1` = wyłączone
  (stock to `3`)
- Wpisywanie: liczba całkowita

Waga różnicy tonalności nie jest nakładana wszędzie. Domyślnie dostają ją tylko pasma, które
dostały wybielanie HIGH, z rozumowania, że to w nich struktura została naprawdę zniszczona i
wymaga zastąpienia. Ten przełącznik poszerza ten zbiór w dół.

To czyni go przełącznikiem odblokowującym `--sbr-nf-weight`: na muzyce estymator rzadko
wybiera HIGH, więc przy ustawieniu stockowym waga nie ma prawie na czym działać, a pokrętło
wygląda na martwe. Ustaw to na `1` i w grze jest każde wybielone pasmo.

Wartości:
- `3` tylko HIGH. Stock. Najbardziej zachowawcze i powód, dla którego `--sbr-nf-weight` tak
  często sprawia na muzyce wrażenie, że nic nie robi.
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
- Wartości: `0` = bez wygładzania; domyślnie `-1` = wyłączone, co zostawia stockowe
  wygładzanie po 4 ramkach
- Wpisywanie: liczba całkowita

Poziom szumu jest normalnie uśredniany po czterech ramkach, więc ilość szumu wypełniającego w
paśmie zmienia się stopniowo. To słuszne na materiale ustabilizowanym, gdzie skaczący poziom
szumu byłby słyszalny jako pompowanie albo oddychanie na górze. Jest niesłuszne tam, gdzie
materiał naprawdę zmienia się szybko: wygładzony poziom przychodzi z opóźnieniem, więc nagle
cichy fragment jeszcze przez kilka ramek niesie szum z fragmentu głośnego, a nagle jasny
fragment jest na starcie niedopełniony.

Ten przełącznik działa jednokierunkowo. Nie ma wartości, która zwiększa wygładzanie; `0` je
usuwa, a cokolwiek innego oznacza zostawienie go w spokoju.

Wartości:
- `0` bez wygładzania w czasie. Poziom szumu podąża dokładnie za oszacowaniem z ramki na
  ramkę. Szybciej, i na materiale ustabilizowanym słyszalnie mniej stabilnie.
- `-1` wyłączone: stockowe wygładzanie po 4 ramkach. Właściwa wartość domyślna.

Przykład w kontekście: `fdkaac -p 5 -b 64000 --sbr-nf-smooth 0 -o out.m4a transients.wav`

Współpraca i konflikty: szumowy odpowiednik `--sbr-invf-smooth 0`, i obowiązuje to samo
rozumowanie: stosuj na materiale z szybkimi zmianami charakteru i spodziewaj się mniejszej
stabilności na materiale utrzymującym się. Jego efekt jest ograniczony przez
`--sbr-noise-max`: jeśli sufit obcina większość pasm, zostaje niewiele zróżnicowania, które
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
wybiera 0,125 przy 24 do 48 kbps, a przy tym sufice około 95 procent pasm jest już obciętych,
i to dlatego szum wypełniający sprawia wrażenie ustalonego, dopóki go nie podniesiesz.

Podniesienie sufitu samo w sobie nie dodaje szumu; pozwala estymatorowi i wadze dodać go
więcej tam, gdzie i tak już chciały. Obniżenie go przycina górę do czegoś bardziej suchego i
tonalnego, co na materiale rzadkim brzmi czyściej, a na talerzach i oddechu chudo.

Która z tych trzech wartości nic nie robi, zależy od wiersza tabeli strojenia, na którym
jesteś, więc raczej mierz, niż zakładaj. Zmierzone: na sygnałach tonalnym i mieszanym przy
HE-AAC 32 kbps `-3` równa się stockowi, a `6` równa się `3`; na sygnale głosowym przy tym
samym ustawieniu `6`, `3` i stock są identyczne.

Wartości:
- `-3` sufit 0,125. Najbardziej sucho i taki wybiera tabela przy niskim bitrate'ie. Bardziej
  tonalnie, bardziej metalicznie, mniej syku.
- `3` sufit 0,5. Czterokrotnie większy zapas. Szum wypełniający może faktycznie zmieniać się
  z pasma na pasmo.
- `6` sufit 1,0. Maksymalny zapas. W parze z `--sbr-nf-weight` i `--sbr-nf-weight-from 1` to
  konfiguracja, w której pokrętła poziomu szumu wreszcie mają pole do pracy; samo z siebie
  podnosi głównie te pasma, które estymator już i tak dopychał do sufitu.
- `-1` wyłączone: decyduje tabela strojenia dla bitrate'u.

Przykład w kontekście: `fdkaac -p 5 -b 32000 --sbr-noise-max 6 --sbr-nf-weight-from 1 --sbr-nf-weight 0.7 -o out.m4a in.wav`

Współpraca i konflikty: to jest bramka przed `--sbr-nf-weight`, `--sbr-nf-weight-from` i
`--sbr-nf-smooth`, a poziomy od 5 do 7 w `--sbr-softness` podnoszą ją za ciebie (5 i 6 do
0,5, 7 do 1,0). `--sbr-noise-bands` ustawia rozdzielczość częstotliwościową poziomu szumu,
który ten sufit przycina, więc gęsty poziom szumu przy niskim sufice to zmarnowane dane
poboczne.

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

Ta toporność bywa dokładnie tym, czego chcesz. Kiedy `--sbr-nf-weight` odmawia ruchu, bo
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
  wypełniający. Talerze zamieniają się w kształtowany syk, a wynik jest jednoznaczną
  karykaturą, co czyni go przydatnym punktem odniesienia dla oceny, jak dużą część
  góry stanowi szum.

Przykłady, mniej szumu wypełniającego:
- Łagodnie: `--sbr-noise-floor-offset 2`. Odrobinę bardziej sucho.
- Mocno: `--sbr-noise-floor-offset 1`. Minimalne wypełnienie; górę niosą własna struktura
  załatanego pasma i to, co zostawia po sobie wybielanie.
- Przesadnie: `--sbr-noise-floor-offset 0`, czyli minimum. Żadnego wkładu z przesunięcia. Na
  materiale tonalnym spodziewaj się metalicznego dzwonienia, które szum wypełniający ma
  maskować, co źle łączy się z niskimi ustawieniami `--sbr-invf` i co właśnie dlatego warto
  raz usłyszeć.

Współpraca i konflikty: nadal podlega `--sbr-noise-max`, więc wysokie przesunięcie wobec
sufitu 0,125 zostaje obcięte. Działa w tym samym kierunku co `--sbr-nf-weight`, ale
bezwarunkowo, co czyni je właściwym wyborem, gdy chcesz zmiany globalnej, i niewłaściwym, gdy
chcesz, żeby to koder decydował, gdzie szumowi jest miejsce. Wobec niskich poziomów
`--sbr-invf` walczy z innym artefaktem przy użyciu złego narzędzia: metaliczne dzwonienie od
niewybielonej struktury lepiej naprawić wybielaniem niż maskować szumem.
## D. Maskowanie, kształtowanie szumu i szczegół

To grupa, która decyduje o tym, ile szumu koder zgadza się zostawić w każdym pasmie
i gdzie zamiast kodować sygnał po prostu go fabrykuje. Całą grupę niosą cztery pomysły,
więc wyjaśniamy je raz tutaj, a karty będą się do nich odwoływać.

**Próg maskowania.** Dla każdego pasma współczynnika skali, w każdej ramce, model
psychoakustyczny wylicza poziom, poniżej którego dodany szum powinien być niesłyszalny,
bo głośniejsza treść w tym pasmie i wokół niego go przykrywa. Ten poziom to próg
maskowania. Kwantyzator wydaje potem dokładnie tyle bitów, ile trzeba, żeby zepchnąć szum
kwantyzacji do progu, i ani jednego więcej. Podnieś progi globalnie, a koder uzna, że
więcej szumu jest dopuszczalne, więc wyda mniej bitów i plik stanie się grubszy przy
tym samym bitrate. Obniż je, a koder będzie żądał większej precyzji wszędzie i bity
skończą mu się szybciej, co przy CBR oznacza, że coś innego musi ustąpić. `--ath-scale`
to globalna skala tego progu. `--spread-mask` zmienia, jak daleko maskowanie przecieka
między sąsiednimi pasmami, a to już inne pytanie: nie jak głośna jest maska, lecz jak
szeroka.

**Żądanie SNR.** Niezależnie od progu FDK trzyma wymagany stosunek sygnału do szumu dla
każdego pasma, czyli minimalną jakość, jaką koder musi w tym pasmie dostarczyć bez względu
na to, co mówi próg. To najbliższy odpowiednik stałych tone masking noise i noise masking
tone, które udostępniają MusePack i LAME. Znaczy więcej, niż się wydaje, bo logika
unikania dziur przyciąga progi z powrotem do tego żądania: jeśli pętla dopasowania
bitrate'u chce zagłodzić pasmo, żądanie SNR jest linią, której nie wolno jej przekroczyć.
`--minsnr-scale` skaluje to żądanie, a dwa przełączniki klamrowe przesuwają sufit i podłogę,
między którymi samo żądanie jest trzymane.

**TNS, czyli kształtowanie szumu w czasie.** Wewnątrz jednego bloku szum kwantyzacji
rozkłada się równo na cały blok w czasie. Dla transjentu to zły rozkład: blok z uderzeniem
perkusji ma szum także w cichym fragmencie przed uderzeniem, gdzie nic go nie maskuje, co
słyszysz jako pre-echo. TNS stosuje filtr predykcyjny w poprzek częstotliwości, co w
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
częstotliwości, czy obowiązuje bramka niskiego bitrate'u, jak głośny jest fabrykowany szum,
oraz trzy progi detekcji i minimalna szerokość pasma, które razem decydują, które pasma
się kwalifikują.

Jedna konwencja przed kartami: wszystkie pokrętła skalujące `--pns-*` przyjmują zwykły
mnożnik dziesiętny na fabryczną wartość FDK. `1.0` oznacza "zostaw wartość fabryczną",
`1.5` oznacza półtora raza tyle, `0.5` oznacza połowę. Wyjątkiem jest `--pns-min-width`,
które jest surową liczbą.

### `--ath-scale <n>`

**Globalna skala progu maskowania: jedno pokrętło na "bądź czystszy" albo "bądź
oszczędniejszy" w całym widmie.**

- Dotyczy: każdego profilu z rdzeniem AAC, czyli wszystkich. Pod SBR działa tylko na
  rdzeń, bo SBR ma własny model szumu.
- Wartości: Q8, `1..4096`, gdzie `256` to x1,0; domyślnie: `256`, bez zmiany
- Wpisywanie: liczba całkowita

Próg maskowania to poziom szumu, o którym model sądzi, że w danym pasmie go nie usłyszysz.
`--ath-scale` mnoży wszystkie progi naraz. Powyżej 256 progi rosną, koder dochodzi do
wniosku, że więcej szumu jest niesłyszalne, i wydaje mniej bitów na pasmo: kodowanie
bardziej agresywne, co przy stałym bitrate oznacza, że te bity idą gdzie indziej albo
ramka po prostu kończy się wcześniej. Poniżej 256 progi spadają, więcej treści uznaje się
za warte dokładnego zakodowania, a zapotrzebowanie na bity rośnie.

Dwie rzeczy nie pozwalają temu pokrętłu być tak mocnym, jak sugeruje jego zakres. Działa
na logarytmicznej kopii progu, a część jego efektu znosi dalej w łańcuchu logika minimalnego
SNR oraz sufit redukcji progu 29 dB w kwantyzatorze CBR. Poza tym przy CBR globalna zmiana
progu raczej przetasowuje bity, niż zmienia ich liczbę, bo pętla dopasowania bitrate'u i tak
musi trafić w cel. Jeśli chcesz mocniejszej i bardziej bezpośredniej dźwigni na szczegół,
to `--minsnr-scale`, bo przesuwa linię, do której unikanie dziur przyciąga progi z powrotem.

Na swoje zarabia przy wysokim bitrate, gdzie masz nadwyżkę bitów i chcesz, żeby koder
znalazł więcej rzeczy wartych zakodowania. Wartości trochę poniżej 256 zamieniają tę
nadwyżkę w zachowany szczegół. Przy niskim bitrate użyteczny kierunek jest zwykle w górę,
jako sposób na świadome przyjęcie szorstkości zamiast pozostawienia pętli dopasowania
bitrate'u decyzji, gdzie ciąć.

Przykłady:
- Łagodnie, czyściej: `--ath-scale 230`. Progi niższe o jakieś 10 procent. Trochę więcej
  bitów na cichy szczegół; subtelne przy każdym bitrate, najlepiej widoczne powyżej
  192 kbps.
- Mocno, czyściej: `--ath-scale 180`. Wyraźne przesunięcie wysiłku w stronę treści o
  niskim poziomie. Ogony pogłosu i szum pomieszczenia przeżywają lepiej; przy napiętym
  bitrate coś innego na tym głodzi.
- Przesadnie, czyściej: `--ath-scale 64`. Progi na jednej czwartej stocka. Modelowi mówi
  się praktycznie, że nic nie jest maskowane. Przy CBR pętla dopasowania bitrate'u walczy
  przez całą ramkę, a wynik nie jest lepszy, tylko inaczej skompromitowany; warto raz
  posłuchać, żeby zrozumieć, gdzie leży granica.
- Łagodnie, grubiej: `--ath-scale 290`. Trochę więcej szumu przyjętego na pasmo, mała
  oszczędność bitów.
- Mocno, grubiej: `--ath-scale 512`. Progi podwojone. Słyszalnie bardziej ziarniste na
  materiale szerokopasmowym, zauważalnie tańsze na pasmo.
- Przesadnie, grubiej: `--ath-scale 2048`. Progi osiem razy wyższe niż w stocku. Koder
  wierzy, że prawie wszystko jest zamaskowane, a wynik to karykatura agresywnego kodowania:
  wydrążony, syczący, z utraconym szczegółem wszędzie tam, gdzie obok siedzi coś
  głośniejszego.

Współpraca i konflikty: `--minsnr-scale` jest mocniejszym narzędziem do tego samego celu i
częściowo nadpisuje efekt tego przełącznika, bo żądanie SNR jest podłogą jakości, przez
którą podniesiony próg nie przepchnie. `--reduce-clamp 0` pozwala pchać progi głębiej niż
sufit 29 dB, co daje niskim wartościom tutaj więcej miejsca na działanie. `--spread-mask`
to pokrętło towarzyszące, od szerokości maskowania, a nie od jego wysokości.

### `--spread-mask <n>`

**Skaluje, jak daleko maskowanie przecieka z jednego pasma do sąsiadów, co decyduje, ile
cichych pasm obok głośnych przeżyje.**

- Dotyczy: każdego profilu z rdzeniem AAC. Największy efekt tam, gdzie bitów jest mało,
  ale jeszcze nie rozpaczliwie, czyli z grubsza 96..192 kbps.
- Wartości: Q8, `0` i wyżej, gdzie `256` to x1,0; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Głośny ton maskuje szum nie tylko we własnym pasmie, maskuje też skraj pasm wokół siebie,
bardziej w górę niż w dół. Model symuluje to funkcją rozmycia. To właśnie rozmycie jest
powodem, dla którego ciche pasmo stojące obok głośnego dostaje prawie zero bitów: model
mówi, że jego treść jest przykryta przez sąsiada.

Ten przełącznik skaluje rozmycie. Poniżej 256 oznacza mniej rozmycia maskowania, więc skraj
się zwęża, mniej pasm liczy się jako przykryte i więcej z nich zostaje zakodowanych. To ta
sama idea, co rozluźnienie założenia "ton maskuje szum" w innych koderach. Powyżej 256 skraj
się rozszerza i więcej pasm zostaje spisanych na straty jako przykryte przez sąsiadów, co
jest tańsze i bardziej płaskie.

Powód, dla którego efekt koncentruje się w okolicy 96..192 kbps, to prosta arytmetyka.
Znacznie niżej pasma obok głośnych nie są problemem, bo głodzi się cała ramka i pętla
dopasowania bitrate'u cięła wszędzie. Znacznie wyżej bitów jest tyle, że rozmycie rzadko
o czymkolwiek decyduje. Pomiędzy rozmycie jest tym, czym koder faktycznie wybiera, co
pominąć.

Przykłady:
- Łagodnie, mniej maskowania: `--spread-mask 230`. Nieco węższy skraj. Kilka pasm więcej
  przeżywa obok głośnej treści; więcej szczegółu wokół mocnych składowych.
- Mocno, mniej maskowania: `--spread-mask 180`. Wyraźnie więcej pasm zakodowanych obok
  głośnych. Na gęstym materiale to właśnie tutaj odzyskuje się szczegół wokół energii
  transjentów, realnym kosztem bitów.
- Przesadnie, mniej maskowania: `--spread-mask 32`. Rozmycie prawie usunięte. Model traci
  większość zdolności do decydowania, co jest przykryte, więc bity rozsypują się po pasmach,
  które naprawdę są niesłyszalne; karykatura pokazująca, co daje funkcja rozmycia.
- Łagodnie, więcej maskowania: `--spread-mask 300`. Odrobinę szerszy skraj, mała oszczędność.
- Mocno, więcej maskowania: `--spread-mask 512`. Rozmycie podwojone. Zauważalnie bardziej
  płaskie widmo obok głośnej treści, tanio, z cichym szczegółem spisanym na straty.
- Przesadnie, więcej maskowania: `--spread-mask 1536`. Prawie wszystko blisko czegokolwiek
  głośnego liczy się jako zamaskowane. Wynik jest wydrążony i rozmazany, przydatny głównie
  jako przeciwny koniec pokazu.

Współpraca i konflikty: łącz z `--ath-scale` poniżej 256, kiedy chcesz maski zarazem
niższej i węższej; oba są niezależne i efekty się dodają. `--minsnr-scale` nadal ogranicza
wynik od dołu, więc bardzo niskie rozmycie nie zepchnie żadnego pasma poniżej jego żądania
SNR.

### `--minsnr-scale <n>`

**Skaluje stosunek sygnału do szumu, jaki koder musi dostarczyć w każdym pasmie; najbardziej
bezpośrednie pokrętło od szczegółu w tej grupie.**

- Dotyczy: każdego profilu z rdzeniem AAC.
- Wartości: Q8, `1` i wyżej bez udokumentowanego sufitu, gdzie `256` to x1,0; domyślnie:
  `-1`, wyłączone
- Wpisywanie: liczba całkowita

Oprócz progu maskowania każde pasmo nosi wymagany SNR kodowania. To gwarantowane minimum:
cokolwiek mówi wyliczenie progu, koder musi w tym pasmie osiągnąć taką jakość. FDK wyprowadza
je ze swoich tabel strojenia i jest to najbliższy odpowiednik stałych tone masking noise i
noise masking tone, które MusePack udostępnia bezpośrednio.

Poniżej 256 żąda wyższego SNR niż stock, czyli więcej szczegółu i więcej bitów na pasmo.
Powyżej 256 żąda mniej i wynik jest grubszy. Zwróć uwagę na odwrócenie wbrew intuicji:
skalujesz dozwolony szum, nie żądaną jakość, więc mniej znaczy czyściej.

Ten przełącznik jest skuteczniejszy od `--ath-scale` w tym samym celu, a powód jest
strukturalny. Logika unikania dziur przyciąga progi z powrotem do linii minimalnego SNR, co
oznacza, że żądanie SNR jest podłogą jakości, której pętli dopasowania bitrate'u nie wolno
przebić. Przesuń próg, a klamra może ci to odkręcić. Przesuń żądanie, a klamra pójdzie za tobą.

Przykłady:
- Łagodnie, więcej szczegółu: `--minsnr-scale 220`. Skromne podniesienie wymaganej jakości
  na pasmo. Widoczne przy średnim bitrate jako mniej wypadających pasm.
- Mocno, więcej szczegółu: `--minsnr-scale 150`. Wyraźnie wyższe żądanie. W połączeniu z
  hojnym bitrate to właśnie to pokrętło zamienia nadwyżkę bitów w zachowany szczegół; przy
  napiętym bitrate zmusza pętlę dopasowania bitrate'u, żeby cięła w innym miejscu.
- Przesadnie, więcej szczegółu: `--minsnr-scale 16`. Żądanie szesnastokrotne. Ramka nigdy go
  nie zaspokoi, a pętla dopasowania bitrate'u spędza całą ramkę w trybie ratunkowym, więc
  wynik jest gorszy niż stock; pouczające raz, ale to nie ustawienie do pracy.
- Łagodnie, grubiej: `--minsnr-scale 300`. Trochę mniej wymagane na pasmo, mała oszczędność.
- Mocno, grubiej: `--minsnr-scale 512`. Połowa żądanej jakości. Słyszalnie bardziej
  ziarniste, z realną oszczędnością bitów, którą można przekierować innymi przełącznikami.
- Przesadnie, grubiej: `--minsnr-scale 4096`. Żądanie jest praktycznie usunięte, podłoga
  pod jakością znika, a pasma swobodnie się zapadają. Karykatura pokazująca, czego to żądanie
  SNR broniło.

Współpraca i konflikty: `--minsnr-clamp-hi` i `--minsnr-clamp-lo` ograniczają wynik tego
skalowania, więc schodząc daleko poniżej 256, w końcu uderzysz w stockowy sufit około -1 dB,
o ile go nie podniesiesz przez `--minsnr-clamp-hi`. `--reduce-clamp 0` to zwykły towarzysz
przy skrajnym szczególe, bo usuwa sufit redukcji 29 dB, który inaczej ogranicza, jak głęboko
da się pchać progi. Jest mocniejszy od `--ath-scale`, więc ustaw najpierw ten, a `--ath-scale`
użyj jako docinki.

### `--minsnr-clamp-hi <n>`

**Podnosi lub obniża sufit tego, ile SNR pasmo ma prawo żądać.**

- Dotyczy: każdego profilu z rdzeniem AAC. Działa tylko wtedy, gdy jakieś pasmo faktycznie
  chce więcej niż stockowy sufit, co w praktyce oznacza, że używasz też `--minsnr-scale`
  poniżej 256.
- Wartości: Q8, `1` i wyżej, gdzie `256` to x1,0; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Żądanie SNR dla pasma jest trzymane między sufitem i podłogą. Sufit, czyli MAX_SNR w FDK,
siedzi około -1 dB i istnieje po to, by żadne pasmo nie mogło żądać absurdalnej precyzji i
zagłodzić resztę ramki. Jeśli przeskalujesz żądanie w górę przez `--minsnr-scale`, pasma
piętrzą się pod tym sufitem i skalowanie przestaje dawać jakikolwiek dalszy efekt.

Powyżej 256 sufit tutaj się podnosi, więc pasma mogą żądać więcej, niż pozwala stock. To
jedyny sposób, żeby bardzo niskie wartości `--minsnr-scale` dalej działały. Poniżej 256
sufit się obniża, ograniczając żądanie szybciej, co jest tępym sposobem ograniczenia, ile
jedno pasmo może zabrać z ramki.

Przykłady:
- Łagodnie, wyższy sufit: `--minsnr-clamp-hi 320`. Niewielki zapas nad stockowym limitem.
  Pozwala skromnemu `--minsnr-scale` dalej działać w najbardziej wymagających pasmach.
- Mocno, wyższy sufit: `--minsnr-clamp-hi 512`. Dwa razy stockowy limit. To para do
  poważnej pracy nad szczegółem przy wysokim bitrate.
- Przesadnie, wyższy sufit: `--minsnr-clamp-hi 4096`. Sufitu praktycznie nie ma i jedno
  pasmo może zażądać wszystkiego, co ramka ma. Spodziewaj się, że pętla dopasowania
  bitrate'u wyda cały budżet na kilka pasm, a resztę zapadnie.
- Łagodnie, niższy sufit: `--minsnr-clamp-hi 220`. Żądania ograniczone trochę szybciej;
  lekkie wyrównanie rozkładu bitów.
- Mocno, niższy sufit: `--minsnr-clamp-hi 128`. Połowa stockowego limitu. Żadne pasmo nie
  może prosić o wiele, więc widmo jest kodowane bardziej jednolicie i grubiej w szczytach.
- Przesadnie, niższy sufit: `--minsnr-clamp-hi 8`. Sufit jest tak nisko, że żądanie zostaje
  w zasadzie usunięte jako mechanizm, co daje ten sam stan końcowy, co usunięcie go od dołu.

Współpraca i konflikty: ma sens tylko razem z `--minsnr-scale`; sam nie zmienia nic, o ile
stockowe żądania nie dochodziły już do limitu. To odpowiednik `--minsnr-clamp-lo`, który
przesuwa drugi koniec tej samej klamry.

Zmierzone: przy AAC-LC 64 kbps `--minsnr-clamp-hi 512` zmienia strumień.

### `--minsnr-clamp-lo <n>`

**Podnosi lub obniża podłogę pod żądaniem SNR dla pasma, czyli poziom, poniżej którego
żadnemu pasmu nie wolno spaść.**

- Dotyczy: każdego profilu z rdzeniem AAC, ale tylko tam, gdzie pasma faktycznie są pchane
  do podłogi, co oznacza bardzo niski bitrate. Przy wygodnych bitrate'ach podłoga nigdy nie
  zostaje osiągnięta i przełącznik nie robi nic.
- Wartości: Q8, `1` i wyżej, gdzie `256` to x1,0; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Drugi koniec klamry względem `--minsnr-clamp-hi`. Podłoga MIN_SNR w FDK siedzi około -25 dB
i jest najgorszą jakością, jaką można przypisać pasmu. Bez niej pasmo pod presją bitową
mogłoby dostać praktycznie nic, formalnie nadal będąc kodowane.

Poniżej 256 podłoga się obniża i pozwala pchać pasma dalej w dół, co zwalnia bity dla resztek
ramki kosztem tego, że najgorsze pasma stają się gorsze. Powyżej 256 podłoga się podnosi, więc
nic nie może zrobić się tak złe, jak pozwala stock, co chroni najsłabsze pasma i zabiera te
bity skądinąd.

Szczerze o stanie: przy zwykłych bitrate'ach ten przełącznik nie ma obserwowalnego efektu,
bo podłoga po prostu nie zostaje osiągnięta. To narzędzie na okolice 16..32 kbps i na
eksperymenty, w których rdzeń jest celowo zagłodzony, na przykład szerokim `--core-cutoff` przy
niskim bitrate.

Przykłady:
- Łagodnie, wyższa podłoga: `--minsnr-clamp-lo 300`. Najgorsze pasma trzymane nieco lepiej.
  Widoczne tylko przy bardzo niskim bitrate.
- Mocno, wyższa podłoga: `--minsnr-clamp-lo 512`. Dwa razy stockowa podłoga. Przy niskim
  bitrate zmniejsza to przepaść między najlepiej i najgorzej zakodowanymi pasmami.
- Przesadnie, wyższa podłoga: `--minsnr-clamp-lo 2048`. Podłoga siedzi tak wysoko, że
  spotyka sufit i klamra się zapada; każde pasmo żąda tego samego, a to już nie jest żaden
  model psychoakustyczny.
- Łagodnie, niższa podłoga: `--minsnr-clamp-lo 220`. Najgorsze pasma mogą zejść trochę
  dalej w dół, trochę zwolnionych bitów.
- Mocno, niższa podłoga: `--minsnr-clamp-lo 128`. Połowa stockowej podłogi. Słyszalne przy
  niskim bitrate jako wcześniejsze poddawanie się najcichszych pasm w zamian za czystsze
  średnie pasmo.
- Przesadnie, niższa podłoga: `--minsnr-clamp-lo 4`. Nie zostaje żadna sensowna ochrona;
  pasma pod presją dostają prawie nic, co jest blisko pozwolenia im zostać dziurami bez
  udziału logiki dziur.

Współpraca i konflikty: łączy się w parę z `--minsnr-clamp-hi` jako dwa końce jednej klamry
oraz z `--minsnr-scale`, który przesuwa żądanie w jej środku. Przełączniki dziur z grupy E
zajmują się tym samym objawem z drugiej strony, kontrolując, które pasma wolno wyzerować
całkowicie.

Zmierzone: `--minsnr-clamp-lo 512` nie dało żadnej zmiany przy AAC-LC 64 kbps, bo podłoga
nigdy tam nie zostaje osiągnięta, natomiast `--minsnr-clamp-hi 512` przy tych samych
ustawieniach strumień zmienił. Licz się z tym, że ten potrzebuje znacznie niższego bitrate'u,
żeby zadziałać.

### `--reduce-clamp <0|1>`

**Usuwa sufit 29 dB na to, jak daleko kwantyzator CBR może pchać progi w dół, pozwalając
innym przełącznikom od szczegółu działać bez uderzania w ścianę.**

- Dotyczy: CBR, czyli domyślnej ścieżki `-m 0`. VBR używa innej ścieżki kodu i nie jest tym
  dotknięte.
- Wartości: `0` albo `1`; domyślnie: `1`, sufit obowiązuje, jak w stocku
- Wpisywanie: liczba całkowita, włącz/wyłącz

Kiedy pętla dopasowania bitrate'u CBR potrzebuje w pasmie więcej jakości, redukuje próg tego
pasma. Na łączną redukcję jest sufit, około 29 dB, który nie pozwala pętli gonić jednego
wymagającego pasma w nieskończoność. Przy normalnym materiale sufit nigdy nie wiąże. Jeśli
celowo żądasz znacznie większej precyzji niż stock, przez `--minsnr-scale` dobrze poniżej 256
albo `--ath-scale` dobrze poniżej 256, to właśnie ten sufit zatrzymuje dalszy efekt
ustawienia.

Ustawienie `0` zdejmuje sufit, więc progi mogą być pchane głębiej i do wymagających pasm
płynie więcej bitów. Samo z siebie zmienia niewiele, bo stock rzadko dochodzi do 29 dB
redukcji. To przełącznik towarzyszący, nie samodzielny.

Wartości:
- `1`: sufit redukcji 29 dB obowiązuje (stock, domyślnie).
- `0`: sufit usunięty, progi mogą być pchane głębiej.

Przykład w kontekście:

    fdkaac -p 2 -b 256000 --minsnr-scale 150 --reduce-clamp 0 -o out.m4a in.wav

Współpraca i konflikty: łącz w parę z `--minsnr-scale` poniżej 256 dla skrajnego szczegółu;
sam jest niemal bez sensu. Bez efektu przy VBR (`-m 1..5`).

### `--tns-mask <n>`

**Wybiera, których filtrów kształtowania szumu w czasie koder może używać.**

- Dotyczy: każdego profilu z rdzeniem AAC. Najważniejszy w LD i ELD, gdzie TNS jest jedyną
  obroną przed pre-echem, oraz na materiale bogatym w transjenty w LC.
- Wartości: `0..15`, maska czterobitowa; domyślnie: `-1`, stockowe `0xF`, wszystkie filtry
  dozwolone
- Wpisywanie: liczba całkowita

TNS kształtuje szum kwantyzacji w czasie, żeby chował się pod atakiem, a nie siedział w
ciszy przed nim. FDK ma więcej niż jeden filtr TNS, różniące się typem bloku i obszarem
częstotliwości, który obejmują, a maska mówi, które z nich są włączone. Stock to `0xF`,
czyli 15, czyli wszystkie cztery bity ustawione.

To raczej przełącznik diagnostyczny i laboratoryjny niż pokrętło jakości. Użyteczne
ustawienia to `0`, które wyłącza TNS zupełnie i pozwala usłyszeć, co robił, oraz `15`, które
jest stockiem. Maski pośrednie pozwalają wyizolować jeden filtr, co jest ciekawe, gdy
próbujesz przypisać artefakt do źródła, a poza tym rzadko.

Wyłączenie TNS ma drugą konsekwencję, o której warto wiedzieć: FDK wymusza wyłączenie PNS,
gdy TNS jest wyłączony. Zatem `--tns-mask 0` po cichu zabiera ze sobą percepcyjne
podstawianie szumu.

Wartości:
- `-1`: stock, wszystkie filtry włączone (`0xF`).
- `0`: TNS całkowicie wyłączony. Pre-echo na ostrych atakach robi się zauważalnie gorsze,
  szczególnie w LD i ELD; PNS też zostaje wymuszenie wyłączony.
- `1` do `14`: poszczególne filtry włączane bitami. Do izolowania wkładu jednego filtru.
- `15`: wszystkie filtry włączone, tak jak stock.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --tns-mask 0 --verbose -o no_tns.m4a attacks.wav

Współpraca i konflikty: `--tns-order` ogranicza, jak długie mogą być włączone filtry, więc
te dwa czyta się razem. `--tns-thresh` z grupy E decyduje, jak często TNS w ogóle się włącza,
i zwykle to jego chcesz zamiast maski. Ustawienie `0` tutaj wymusza wyłączenie PNS bez
względu na `--pns`. `--verbose` wypisuje efektywną maskę TNS jako maskę bitową w zapisie
szesnastkowym.

### `--tns-order <n>`

**Ogranicza długość filtru kształtowania szumu w czasie, wymieniając precyzję, z jaką
obwiednia szumu może iść za sygnałem, na dane poboczne, które to kosztuje.**

- Dotyczy: każdego profilu z rdzeniem AAC. Bloki krótkie są dodatkowo ograniczone przez
  standard do rzędu 5, więc wartości powyżej tej liczby dotyczą tylko bloków długich.
- Wartości: `1..12`; domyślnie: `-1`, własny rząd FDK
- Wpisywanie: liczba całkowita

Filtr TNS to predyktor liniowy działający w poprzek częstotliwości. Jego rząd to liczba
współczynników, a rząd decyduje, jak szczegółową obwiednię czasową może narzucić szumowi.
Niski rząd potrafi wytworzyć tylko zgrubny kształt, z grubsza "więcej szumu później, mniej
wcześniej". Wysoki rząd potrafi iść za skomplikowaną obwiednią, kosztem wysyłania większej
liczby współczynników filtru na ramkę jako dane poboczne.

Niższe wartości robią TNS tańszym i tępszym. Przy niskim bitrate ta wymiana może być tego
warta, bo współczynniki konkurują z widmem o te same bity. Wyższe wartości pozwalają
kształtowaniu śledzić ataki precyzyjniej, i to jest kierunek, gdy pre-echo jest problemem,
a bity masz, zwłaszcza w LD i ELD, gdzie nie istnieją bloki krótkie, które by pomogły.

Przykłady:
- Łagodnie, dłuższy filtr: `--tns-order 8`. Nieco dokładniejsze kształtowanie w czasie w
  blokach długich, mały koszt danych pobocznych.
- Mocno, dłuższy filtr: `--tns-order 12`. Maksimum. Najlepsza rozdzielczość czasowa, jaką
  TNS może dać; warto spróbować w LD i ELD na materiale perkusyjnym.
- Przesadnie, dłuższy filtr: `--tns-order 12` jest już sufitem, więc karykaturą będzie tutaj
  połączenie go z `--tns-thresh 0,5` z grupy E, żeby długi filtr działał też na prawie każdej
  ramce. Spodziewaj się, że dane poboczne zjedzą widoczną część niskiego budżetu bitowego,
  a w zamian pojawią się dziury.
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
64 kbps PNS jest normalnie już włączony, więc `1` to tam to samo co stock i tylko `0` coś
zmienia. Poniżej około 28 kbps tabela wyłącza PNS kompletnie, a w tym punkcie ani `1` tutaj,
ani `--pns-start` nie robią nic, bo PNS jest wyłączony wyżej w łańcuchu; `--force-pns` to
przełącznik, który tę bramkę obchodzi.

Szczere zastosowanie tej karty to diagnoza i świadomy wybór: `0`, żeby usłyszeć, co PNS
wnosił, `1`, żeby go narzucić tam, gdzie uważasz, że tabela jest zachowawcza. `--verbose`
raportuje wynikowy stan PNS, co jest najszybszym sposobem sprawdzenia, czy żądanie
przeszło przez bramki.

Wartości:
- `-1`: zostaw decyzję FDK (stock, domyślnie).
- `0`: PNS wyłączony. Na materiale szumowym spodziewaj się, że pasma szumowe zostaną
  porządnie zakodowane, a bity wezmą się skądinąd.
- `1`: PNS włączony tam, gdzie bramki pozwalają. Przy 64 kbps i wyżej zwykle pokrywa się to
  ze stockiem; przy niskim bitrate potrzebuje `--force-pns`, żeby mieć jakikolwiek efekt.

Przykład w kontekście:

    fdkaac -p 2 -b 64000 --pns 0 --verbose -o no_pns.m4a in.wav

Współpraca i konflikty: `--force-pns` usuwa bramkę niskiego bitrate'u. `--tns-mask 0`
wymusza wyłączenie PNS niezależnie od tego, co tu ustawisz, bo FDK wymaga TNS dla PNS. Tryb
VBR (`-m 1..5`) robi to samo. `--pns-start`, `--pns-tonality`, `--pns-refpower`,
`--pns-gapfill` i `--pns-min-width` kształtują PNS, gdy już działa, a nie robią nic, gdy nie
działa.

Zmierzone: przy AAC-LC 64 kbps `--pns 1` daje stockowy strumień, bo stock ma tam PNS już
włączony, natomiast `--pns 0` go zmienia.

### `--pns-start <hz>`

**Ustawia najniższą częstotliwość, na której podstawianie szumu wolno działać.**

- Dotyczy: tych samych miejsc co `--pns`, i tylko wtedy, gdy PNS faktycznie działa.
- Wartości: częstotliwość w Hz; domyślnie: `-1`, własna częstotliwość startowa FDK
- Wpisywanie: liczba całkowita

PNS jest bezpieczny wysoko i groźny nisko. Niskie pasma noszą tony podstawowe, które
definiują wysokość i barwę, a podstawienie tam szumu jest natychmiast słyszalne. FDK ma
więc częstotliwość startową, poniżej której PNS nigdy nie jest używany, i zależy ona od
bitrate'u oraz częstotliwości próbkowania.

Obniż start, a kwalifikuje się więcej widma, więc więcej pasm dostaje kodowanie szumowe i
zwalnia się więcej bitów, przy rosnącym ryzyku podstawienia czegoś, co szumem nie było.
Podnieś go, a PNS wycofuje się w górę, co jest bezpieczniejsze i tańsze w oszczędnościach.

Jedna praktyczna pułapka: wartość jest zaokrąglana do granicy pasma współczynnika skali,
więc zmiana o kilkaset Hz może wylądować w tym samym pasmie i dać identyczny strumień. Przy
sondowaniu ruszaj się w krokach kilohercowych albo większych.

Przykłady:
- Łagodnie, niższy start: `--pns-start 3000`. PNS sięga trochę głębiej. Nieco więcej pasm
  kodowanych szumowo na materiale szerokopasmowym.
- Mocno, niższy start: `--pns-start 2000`. Wyraźnie więcej widma się kwalifikuje. Realna
  oszczędność na materiale szumowym; przy czymkolwiek z tonalnością w górnym średnim pasmie
  nasłuchuj, czy ton nie robi się ziarnisty.
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

Współpraca i konflikty: nie robi nic, gdy PNS jest wyłączony, co przy niskim bitrate oznacza,
że najpierw potrzebujesz `--force-pns`. `--pns-min-width` ogranicza PNS przez szerokość pasma,
a nie przez częstotliwość, a skoro pasma rosną wraz z częstotliwością, oba ograniczenia się
nakładają. `--verbose` pokazuje wynikowy stan PNS.

Zmierzone: przy AAC-LC 64 kbps i 48 kHz stockowy start to 4100 Hz. `4000` mapuje się do tego
samego pasma współczynnika skali i daje identyczny strumień; `2000` i `12000` oba go zmieniają.

### `--force-pns`

**Obchodzi bramkę niskiego bitrate'u, która inaczej wyłącza podstawianie szumu całkowicie
poniżej około 28 kbps.**

- Dotyczy: AAC-LC, AAC-LD i AAC-ELD przy CBR, przy bitrate'ach poniżej bramki. Powyżej
  bramki nie zmienia nic, bo PNS jest już dozwolony.
- Wartości: flaga; domyślnie: wyłączone
- Wpisywanie: flaga

Tabela strojenia FDK ma wiersz bitrate'u poniżej około 28 kbps, w którym każdy wpis PNS jest
zerem, dla każdej częstotliwości próbkowania. Konsekwencja zaskakuje ludzi: przy 24 kbps,
dokładnie tam, gdzie tanie kodowanie szumowe pomogłoby najbardziej, PNS jest całkowicie
wyłączony, a zarówno `--pns 1`, jak i `--pns-start` są ignorowane. Słyszalny skutek to
artefakt w stylu MDCT na treści szumowej, który inaczej kojarzyłbyś z MP3.

Ta flaga każe koderowi użyć pierwszego aktywnego wiersza tabeli zamiast wiersza zerowego,
więc PNS działa też przy 24 kbps i niżej. Reszta wymagań FDK nadal obowiązuje: PNS potrzebuje
włączonego TNS i trybu innego niż VBR, i nic w tej kompilacji tego nie zmienia.

Wartości:
- pominięte: bramka obowiązuje, PNS wyłączony poniżej około 28 kbps (stock).
- `--force-pns`: bramka obejścia, PNS dostępny przy niskim bitrate.

Przykład w kontekście:

    fdkaac -p 2 -b 24000 --pns 1 --force-pns --pns-start 4000 -o out.m4a in.wav

Współpraca i konflikty: zwykle podaje się go razem z `--pns 1` i często z `--pns-start`, bo
sensem wymuszania PNS jest potem nim sterować. Bez sensu z `--tns-mask 0` albo przy VBR,
które i tak wymuszają wyłączenie PNS. Przy bitrate'ach powyżej bramki nie robi nic.

### `--pns-shared <0|1>`

**W pasmach, które oba kanały kodują jako szum, zapisz jeden wspólny szum albo dwa niezależne, wszędzie tak samo.**

- Dotyczy: rdzeń AAC-LC w stereo z włączonym PNS (`--pns 1`, `--force-pns` albo `--pns-above`). Domyślnie wyłączone.
- Wartości: `1` = jeden wspólny szum (ten sam w L i R), `0` = niezależny szum w każdym kanale

PNS zastępuje pasmo szumem o właściwym poziomie. W parze kanałów format pozwala zaznaczyć,
pasmo po paśmie, że dekoder ma użyć tego samego szumu w obu kanałach. Ta flaga jedzie na masce
MS. Stock rozstrzyga ją dla każdego pasma osobno, mierząc korelację, więc maskę zwykle trzeba
zapisać pasmo po paśmie: przy niskim bitrate do około 25 bitów na ramkę. Jednolity wybór
pozwala masce wrócić do 2-bitowej postaci dla wszystkich pasm, jeśli reszta ramki na to
pozwala. Poziom szumu nadal jest zapisywany dla każdego kanału: format nie ma wspólnego poziomu.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Typowo: `--pns-shared 1`. Maska MS z 28 do 2 bitów na ramkę na materiale skorelowanym.
- Szerokie stereo: `--pns-shared 0`. Niezależny szum zachowuje szerokość, maska zostaje tak droga jak w stock.

Gdy maska i tak zostaje pasmo po paśmie (pasma pod obszarem PNS kodowane jako L/R na szerokim
materiale), przełącznik oszczędza niewiele: maska jest tania tylko wtedy, gdy zgadzają się
wszystkie pasma. ffmpeg 6.1.1 gra wspólny szum jako niezależny; dekoder FDK jako wspólny.

### `--pns-hold <dB>`

**Pozwól sąsiednim pasmom szumu dzielić jeden poziom, żeby obwiednia szumu kosztowała mniej bitów.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `0.1..12.0` dB

Każde pasmo PNS zapisuje swój poziom szumu jako różnicę względem poprzedniego: 1 bit, gdy jest
taki sam, 3 do 9 bitów w innym wypadku. Z tym przełącznikiem pasmo, którego poziom mieści się w
podanej tolerancji względem poprzedniego pasma szumu, przejmuje jego poziom. Obwiednia szumu w
częstotliwości robi się grubsza, w krokach około 1,5 dB.

Przykłady (muzyka stereo, 24 kbps, PNS od 2,2 kHz):
- Łagodnie: `--pns-hold 3`. Poziomy szumu z 94 do 87 bitów na ramkę.
- Mocno: `--pns-hold 6`. 70 bitów na ramkę; uwolnione bity idą do kodowanego widma.
- Przesadnie: `--pns-hold 12`. Szum jest prawie płaski w całym swoim obszarze.

### `--pns-budget <0|1>`

**Licz bity, które zajmują poziomy szumu PNS, żeby ramce nie zabrakło bitów i nie obcięła widma.**

- Dotyczy: rdzeń AAC z włączonym PNS. Domyślnie wyłączone.
- Wartości: `1` = włączone, `0` = stock

FDK planując bity ramki traktuje pasmo PNS jako darmowe, a każdy poziom szumu kosztuje bity w
strumieniu. Przy niskim bitrate i dużym obszarze PNS ramka się wtedy przepełnia, a awaryjne
cięcie zdejmuje pasma od góry. Stock nie zalicza mu pasm PNS, więc tnie dalej przez obszar szumu
w kodowane widmo pod nim: całe ramki tracą wszystko powyżej około 1,5 kHz, co słychać jako
trzask w rytmie ramek. Z tym przełącznikiem cel bitowy dla widma jest obniżony o koszt poziomów
szumu, a cięcie odzyskuje te bity, gdy zdejmuje pasmo szumu.

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
które będziesz sięgać najczęściej.

Przy `1.0` odtworzony szum nosi energię pierwotnego pasma, czyli to, co mechanizm zakłada.
Powyżej `1.0` wypełnienie jest głośniejsze od pierwotnego, co pomaga, gdy podstawiony szum
brzmi zbyt nieśmiało i miks robi się matowy: prawdziwe poziomy szumu po podstawieniu często
wydają się cichsze, bo odtworzony szum nie ma struktury oryginału. Poniżej `1.0` jest cichsze,
i to jest ruch na sytuację, gdy wypełnienie syczy bardziej niż źródło.

To czyste terytorium gustu, bez poprawnej wartości, i właśnie dlatego oba kierunki warto
wypróbować na własnym materiale przed podjęciem decyzji.

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
  są ciche, więc PNS staje się generatorem dziur. Karykatura i przydatny pokaz, ile sygnału
  nosiły pasma szumowe.

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
podstawianie szumu rozszerza się: więcej widma zastąpionego tanim opisem, więcej oszczędności
i więcej ryzyka, że prawdziwy ton rozmaże się w syk. Niżej niż `1.0` czyni detektor
wybredniejszym, więc podstawiane są tylko pasma wyraźnie szumowe, co jest bezpieczniejsze i
oszczędza mniej.

To pokrętło od szerokości PNS, w przeciwieństwie do `--pns-start`, które kontroluje, gdzie
PNS jest geograficznie dozwolony, i do `--pns-min-width`, które kontroluje go rozmiarem pasma.

Przykłady:
- Łagodnie, szersze podstawianie: `--pns-tonality 1,3`. Kilka granicznych pasm więcej
  staje się PNS. Mała oszczędność, małe ryzyko.
- Mocno, szersze podstawianie: `--pns-tonality 2.0`. Dwa razy wyższy próg. Wyraźnie więcej
  pasm podstawionych; na materiale szumowym realna oszczędność, na materiale tonalnym
  słyszalne rozmazanie składowych.
- Przesadnie, szersze podstawianie: `--pns-tonality 20.0`. Prawie wszystko powyżej
  częstotliwości startowej liczy się jako szum. Tony trzymane zamieniają się w syk, a góra
  miksu staje się posłaniem szumu: karykatura, która czyni mechanizm nie do pomylenia.
- Łagodnie, węższe podstawianie: `--pns-tonality 0,8`. Nieco wybredniejsza detekcja.
- Mocno, węższe podstawianie: `--pns-tonality 0.4`. Podstawiany jest tylko jednoznaczny
  szum. Bezpiecznie, z dużą częścią oszczędności utraconą.
- Przesadnie, węższe podstawianie: `--pns-tonality 0.01`. W praktyce nic się nie kwalifikuje,
  więc trafia to tam, gdzie `--pns 0`, zostawiając PNS formalnie włączony.

Współpraca i konflikty: `--pns-refpower` jest drugą połową decyzji detekcyjnej i oba muszą
się zgodzić, zanim pasmo zostanie PNS, więc ruszanie tylko tym może być zablokowane przez
bramkę mocy. `--pns-gain` zmienia głośność wyniku, nie wybór. `--pns-min-width` może wykluczyć
pasmo, które ten test przeszło.

### `--pns-refpower <x>`

**Skaluje próg mocy odniesienia, który poziom pasma musi spełnić, zanim zostanie uznane za
kandydata na szum.**

- Dotyczy: wszędzie tam, gdzie PNS faktycznie działa.
- Wartości: `0.0` i wyżej, gdzie `1.0` to próg fabryczny; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Tonalność nie jest jedynym testem. Moc każdego pasma jest też porównywana z odniesieniem,
zanim pasmo może zostać kandydatem PNS, co trzyma mechanizm z dala od pasm, których poziom
czyni podstawianie złą wymianą. Ten przełącznik przesuwa tę bramkę mocy.

Oba testy detekcji są koniunkcyjne: tonalność i moc odniesienia muszą się obie zgodzić, zanim
pasmo zostanie PNS. To dlatego ruszanie samym `--pns-tonality` czasem nie robi nic i dlatego
tę kartę i tamtą próbuje się zwykle razem.

Wartość mnoży sam próg. Powyżej `1.0` go podnosi, więc przechodzi mniej pasm i PNS się
wycofuje; poniżej `1.0` go obniża, więc przechodzi więcej pasm. Zmierzone na akustycznej
muzyce stereo przy 64 kbps LC, pasm PNS na ramkę: 5,5 przy `0.01`, 5,1 przy `0.4`, 4,8 przy
`0.8`, 4,6 przy `1.0`, 4,3 przy `1.3`, 3,6 przy `2.0`, 0 przy `20.0`.

Przykłady:
- Łagodnie, luźniejsza bramka: `--pns-refpower 0,8`. Nieco więcej kandydatów (z 4,6 do 4,8 pasma).
- Mocno, luźniejsza bramka: `--pns-refpower 0.4`. 5,1 pasma. W połączeniu z podniesionym
  `--pns-tonality` to miejsce, w którym szerokie podstawianie szumu naprawdę się zaczyna.
- Przesadnie, luźniejsza bramka: `--pns-refpower 0.01`. Test mocy przestaje rozróżniać i wybór
  zostaje po stronie tonalności (5,5 pasma), co dobrze pokazuje, ile pracy ta bramka wykonywała.
- Łagodnie, surowsza bramka: `--pns-refpower 1,3`. Nieco mniej kandydatów (4,3 pasma).
- Mocno, surowsza bramka: `--pns-refpower 2.0`. Dwa razy wyższy próg, 3,6 pasma. PNS wycofuje
  się do pasm, gdzie jest najmniej ryzykowny.
- Przesadnie, surowsza bramka: `--pns-refpower 20.0`. Nic nie przechodzi, więc PNS jest na
  papierze włączony, a w praktyce bezczynny.

Współpraca i konflikty: koniunkcyjny z `--pns-tonality`, więc testuj je parą. `--pns-gapfill`
może i tak dodać pasmo między dwoma pasmami PNS, które tego testu nie przeszło. Nic tutaj nie
ma znaczenia, gdy PNS jest wyłączony.

### `--pns-gapfill <x>`

**Skaluje próg heurystyki, która zamyka jednopasmową lukę między dwoma podstawionymi
pasmami.**

- Dotyczy: wszędzie tam, gdzie PNS działa, i tylko wtedy, gdy dwa pasma PNS już siedzą po
  obu stronach zakodowanego pasma. Szczerze o stanie: podłączone, bez zaobserwowanego efektu
  na naszym materiale testowym.
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
materiale, którego widmo wytworzy wymagany wzorzec, ale nie licz na to, że będzie narzędziem,
po które sięgasz.

Jeśli chcesz prawdziwej dźwigni na to, jak duża część widma staje się szumem, użyj zamiast
tego `--pns-tonality` albo `--pns-start`.

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

Zmierzone: brak zmiany strumienia na 4 sygnałach testowych, 5 bitrate'ach i 6 wartościach tego
przełącznika w AAC-LC. Warunek wstępny reguły wypełniania luk w AAC-LC, czyli oba sąsiednie
pasma już jako PNS, na tym materiale nie wystąpił.

### `--pns-min-width <n>`

**Ustawia minimalną szerokość pasma współczynnika skali, w liniach widmowych, jaką pasmo
musi mieć, zanim podstawianie szumu będzie mogło na nim zadziałać.**

- Dotyczy: wszędzie tam, gdzie PNS działa. Jakikolwiek efekt mają tylko wartości powyżej
  wbudowanej wartości domyślnej, co dla AAC-LC oznacza powyżej 16.
- Wartości: surowa liczba linii, `1` i wyżej; domyślnie: `-1`, wyłączone, czyli własne
  minimum FDK (16 dla AAC-LC)
- Wpisywanie: liczba całkowita

Pasma współczynnika skali robią się szersze wraz ze wzrostem częstotliwości. Wąskie pasmo na
dole widma mieści niewiele linii i zwykle niesie treść tonalną, a podstawienie tam szumu rzuca
się w uszy. Szerokie pasmo wysoko mieści wiele linii i tam właśnie PNS jest na miejscu. Ten
przełącznik wyraża tę preferencję wprost, jako minimalną szerokość.

Zwróć uwagę na asymetrię, bo oczywisty odczyt nazwy jest błędny. Podniesienie minimum
ogranicza PNS do pasm szerszych, czyli do górnego widma, i to działa. Obniżenie go poniżej
własnego minimum FDK nie otwiera PNS na węższe pasma: wartość domyślna FDK jest już efektywną
podłogą, więc wartości poniżej niej dają stockowy strumień. Jest tu jeden użyteczny kierunek,
w górę.

W odróżnieniu od przełączników `--pns-*` powyżej, to surowa liczba linii, nie mnożnik.

Przykłady:
- Łagodnie, tylko szersze pasma: `--pns-min-width 24`. PNS ograniczony trochę wyżej w widmie.
  Małe zmniejszenie podstawiania, mała strata oszczędności.
- Mocno, tylko szersze pasma: `--pns-min-width 32`. Podstawiane mogą być tylko wyraźnie
  szerokie pasma. Bezpieczniej na materiale tonalnym, z widocznym spadkiem oszczędności z PNS.
- Przesadnie, tylko szersze pasma: `--pns-min-width 64`. Przy AAC-LC 64 kbps nie zostaje żadne
  kwalifikujące się pasmo, więc wynik jest identyczny jak `--pns 0`: PNS jest włączony i nie
  ma niczego, czego wolno mu dotknąć.
- W dół nie ma użytecznego przykładu: `2` i `4` oba dają stockowy strumień, bo wbudowane
  minimum 16 już wiąże.

Współpraca i konflikty: nakłada się z `--pns-start`, który ogranicza PNS częstotliwością, a
nie szerokością; skoro szerokość rośnie wraz z częstotliwością, oba ograniczenia wzmacniają
się nawzajem i wygrywa ciaśniejsze. Nie robi nic, gdy PNS jest wyłączony.

Zmierzone: przy AAC-LC wartości `2` i `4` dają stockowy strumień, co potwierdza, że wbudowana
wartość domyślna 16 jest podłogą. `64` przy AAC-LC 64 kbps daje dokładnie ten strumień, który
daje `--pns 0`, więc przy tej szerokości PNS jest praktycznie wyłączony.

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
wstecz trwa tylko kilka milisekund. Słyszysz to jako delikatne rozmycie albo "cień" przed
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
to nadal nie wystarcza, ścina całe pasma współczynnika skali do zera. Wyzerowane pasmo to
cisza w tym zakresie częstotliwości w tej ramce. W czasie brzmi to jak bulgoczące, migające
albo "podwodne" góry: pasma przychodzą i odchodzą z ramki na ramkę. FDK nazywa cały ten
mechanizm "avoid holes", co jest trochę ironiczne: to on decyduje, które dziury zrobić.
Kolejność jest ustalona: pasma są ścinane w maksymalnie ośmiu krokach energetycznych, od
najcichszych, i tylko od pasma początkowego w górę, żeby dół widma był chroniony.
Przełączniki `--avoid-holes`, `--hole-start`, `--hole-start-short`, `--hole-spread` i
`--hole-ladder` zmieniają ten mechanizm.

W żadnym z tych przypadków nie ma nic za darmo. Przy CBR budżet ramki jest ustalony. Pasmo,
którego nie ścięto, kosztuje bity, których nie dostanie inne pasmo, a blok krótki kosztuje
więcej danych pobocznych niż długi. Każdy przełącznik tutaj przesuwa bity; żaden ich nie
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

**Ogranicza, jak daleko koder może pójść, ścinając pasma do zera, tak by zagłodzona ramka
dostawała grubszą kwantyzację zamiast większej liczby pustych pasm.**

- Dotyczy: rdzenia AAC w każdym profilu (LC, rdzeń HE-AAC i HE-AAC v2, LD, ELD). Znaczy coś
  tylko wtedy, gdy kończą się bity.
- Wartości: 1..8; domyślnie: wyłączone, czyli zachowanie stock (8 kroków)
- Wpisywanie: liczba całkowita

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

Przykłady (istnieje tylko jeden kierunek: mniej kroków niż stock):
- Łagodnie: `--hole-ladder 6`. Znikają tylko najgłośniejsze kroki ścinania. Trudne ramki
  tracą kilka pasm mniej; łatwe ramki są nietknięte.
- Mocno: `--hole-ladder 2`. Ścinać wolno tylko dwa najcichsze kroki. Na gęstym materiale
  przy 32..50 kbps usuwa to większość dziur (patrz Zmierzone).
- Przesadnie: `--hole-ladder 1`. Ścinać wolno tylko najcichszy krok, więc prawie każde pasmo
  przeżywa, a cały problem budżetowy zamienia się w grubszą kwantyzację. Na bardzo
  gęstym materiale przy 24 kbps może to brzmieć jednolicie chropowato: karykatura tej
  wymiany.

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
- Wartości: `0` wyłączone, `1` włączone; domyślnie: wyłączone, czyli zachowanie stock
  (włączona od 20 kbps na kanał, wyłączona poniżej)
- Wpisywanie: liczba całkowita, 0 albo 1

Przed ścinaniem pasm FDK może rozluźnić SNR, jakiego żąda, w mądrzejszy sposób: obniża
żądanie w dolinach między szczytami widma, a utrzymuje je w szczytach, tak by wąskie głośne
pasma przeżyły, a oszczędności przyszły stamtąd, gdzie ucho najmniej je zauważa. Stock włącza
to dopiero od 20 kbps na kanał, co dla stereo oznacza od 40 kbps. Poniżej, dokładnie w
zakresie 24..50 kbps, w który ta grupa jest wycelowana, ochrona jest wyłączona i ścinanie
zaczyna się wcześniej.

Wartości:
- `1`: ochrona włączona przy każdym bitrate. Poniżej 40 kbps stereo to jest sensowne
  ustawienie; na tej wartości i powyżej nic się nie zmienia, bo stock ma ją już włączoną.
- `0`: ochrona wyłączona przy każdym bitrate. Ustawienie diagnostyczne: pokazuje, co ta
  ochrona ci dawała.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --avoid-holes 1 --hole-ladder 2`.

Współpraca i konflikty: sam z siebie daje mały efekt (zmierzone), bo ochrona tylko
przestawia, skąd biorą się oszczędności; najbardziej pomaga razem z `--hole-ladder`.

Zmierzone: przy LC 40 i 128 kbps stereo `--avoid-holes 1` daje strumień identyczny ze
stockiem (tam już jest włączona), a `--avoid-holes 0` go zmienia.

### `--hole-start <sfb>`

**Chroni dolną część widma w blokach długich przed ścięciem do zera.**

- Dotyczy: rdzenia AAC, bloków długich (LC, rdzeń HE-AAC, LD, ELD).
- Wartości: pasmo współczynnika skali 0..51; domyślnie: wyłączone, czyli zachowanie stock
  (pasmo 15, gdy ochrona jest włączona, pasmo 0, gdy jest wyłączona)
- Wpisywanie: liczba całkowita

Pasma poniżej tej liczby nigdy nie są wybierane jako dziury. Stock startuje od pasma 15, co
przy 48 kHz jest z grubsza 2 do 3 kHz: bas i dolna średnica są bezpieczne, wszystko powyżej
wolno ściąć. Podniesienie liczby przesuwa chroniony obszar w górę; obniżenie dopuszcza dziury
głębiej. Pamiętaj, że chronione pasmo nadal kosztuje bity: oszczędności muszą wtedy przyjść
z pasm powyżej albo ze grubszej kwantyzacji wszędzie.

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
- Łagodnie: `--hole-start 12`. Trochę więcej swobody zaraz poniżej stockowego startu.
- Mocno: `--hole-start 6`. Dziury dopuszczone w dół, w dolną średnicę.
- Przesadnie: `--hole-start 0`. Nawet pasma basu wolno ściąć. Przydatne tylko do usłyszenia,
  czemu zapobiega stockowa ochrona.

Współpraca i konflikty: lustrzanym przełącznikiem dla bloków krótkich jest
`--hole-start-short`.

### `--hole-start-short <sfb>`

**Ta sama ochrona dla bloków krótkich.**

- Dotyczy: LC i rdzenia HE-AAC, ramek kodowanych jako bloki krótkie. LD i ELD nie mają
  żadnych.
- Wartości: pasmo współczynnika skali bloku krótkiego 0..15; domyślnie: wyłączone, czyli
  zachowanie stock (pasmo 3)
- Wpisywanie: liczba całkowita

Blok krótki ma około 14 pasm przy 44,1 albo 48 kHz, więc każde pasmo jest szerokie, a liczby
są małe. Stock chroni pasma 0..2.

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
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie stock (3 dB dla bloków
  długich, 2 dB dla bloków krótkich)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Pasmo można ściąć bezpieczniej, kiedy sąsiedzi i tak je maskują. Żeby to ocenić, FDK
porównuje energię pasma z energią rozmytą do niego z pasm sąsiednich, po obniżeniu tej
rozmytej energii o margines. Mały margines sprawia, że sąsiedzi wyglądają mocno, więc wiele
pasm wygląda na przykryte i staje się kandydatami. Duży margines sprawia, że sąsiedzi
wyglądają słabo, więc mniej pasm wygląda na przykryte i więcej z nich jest chronionych.

To delikatne pokrętło. Na naszym materiale przesunęło liczbę dziur o jeden lub dwa procent
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

Uwaga: ustawienie tego przełącznika stosuje ten sam margines do bloków długich i krótkich,
podczas gdy stock używa 3 dB i 2 dB.

### `--preecho-rise <dB>`

**Ogranicza, jak szybko próg maskowania pasma może rosnąć z ramki na ramkę, tak by szum
kwantyzacji nie mógł wybiegać przed atak w bloku długim.**

- Dotyczy: bloków długich w każdym profilu. Najistotniejszy w LD i ELD oraz w LC z
  wymuszonymi blokami długimi (`--block-bias 0`).
- Wartości: 0.0..24.0 dB; domyślnie: wyłączone, czyli zachowanie stock (3,01 dB, czyli
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
  fragmentach i głodzi wszystko inne przy CBR.

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
  zachowanie stock (20 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kiedy ograniczenie wzrostu ciągnie próg w dół, ten przełącznik ustawia podłogę: próg nie może
skończyć niżej niż N dB poniżej tego, co model psychoakustyczny wyliczył dla ramki. Nie
pozwala ograniczeniu żądać absurdalnej precyzji po długiej ciszy, kiedy próg poprzedniej
ramki był znikomy.

Przykłady, dopuszczać głębsze obniżanie:
- Łagodnie: `--preecho-depth 25`.
- Mocno: `--preecho-depth 40`. Po ciszy pierwszy atak jest kodowany bardzo czysto i bardzo
  drogo.
- Przesadnie: `--preecho-depth 60`. Praktycznie brak podłogi; przy CBR jeden nagłos po
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
- Wartości: 0.0..0.99; domyślnie: wyłączone, czyli zachowanie stock (0,70)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Detektor dzieli każdą ramkę na podokna, odfiltrowuje niskie częstotliwości i porównuje
energię każdego podokna z bieżącą średnią tego, co było wcześniej. Dostatecznie duży stosunek
to atak. Bieżąca średnia zachowuje część swojej wartości na podokno: ta część to właśnie ten
przełącznik. Wyższe wartości oznaczają długą pamięć, więc atak jest porównywany z długą
średnią; niższe oznaczają porównanie z ostatnią chwilą.

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
- Wartości: -60.0..30.0 dB; domyślnie: wyłączone, czyli zachowanie stock (przesunięcie 0 dB)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Oprócz stosunku detektor wymaga bezwzględnego minimum energii, żeby stuknięcia w niemal
ciszy nie wyzwalały bloków krótkich. Wartości ujemne obniżają tę podłogę: ciche ataki, takie
jak miotełki, miękkie hi-haty czy ciche szarpnięcie gitary, też przełączają na bloki krótkie.
Wartości dodatnie ją podnoszą: liczą się tylko głośne ataki.

Przykłady, łapanie cichszych ataków:
- Łagodnie: `--attack-floor -6`.
- Mocno: `--attack-floor -20`.
- Przesadnie: `--attack-floor -60`. Praktycznie każdy wzrost w cichym fragmencie się zalicza.
  Spodziewaj się bloków krótkich na szumie pomieszczenia i na miękkich fragmentach tonalnych,
  wraz z kosztem bitowym, który to niesie.

Przykłady, tylko głośne ataki:
- Łagodnie: `--attack-floor 6`.
- Mocno: `--attack-floor 15`.
- Przesadnie: `--attack-floor 30`. Miękka perkusja zostaje w blokach długich i pojawia się
  na niej pre-echo.

### `--window-shape <0|1>`

**Wybiera kształt okna każdego bloku: sinusowe wszędzie albo KBD wszędzie.**

- Dotyczy: AAC-LC oraz rdzenia LC w HE-AAC i HE-AAC v2. LD i ELD używają ustalonego okna i
  nie są tym objęte.
- Wartości: `0` sinusowe, `1` KBD; domyślnie: wyłączone, czyli zachowanie stock (KBD dla
  bloków długich, sinusowe dla bloków krótkich)
- Wpisywanie: liczba całkowita, 0 albo 1

Okno decyduje o tym, jak energia bloku wycieka w sąsiednie częstotliwości. Okno sinusowe ma
wąski listek główny (dobra selektywność obok szczytu) i wolno opadające listki boczne
(energia i szum wyciekają daleko). Okno KBD ma szerszy listek główny i listki boczne, które
opadają znacznie szybciej. W czasie ta sama właściwość oznacza, że KBD trzyma szum
kwantyzacji bliżej miejsca, gdzie powinien być. Stock używa okna sinusowego dla bloków
krótkich, czyli dokładnie dla bloków wokół ataków.

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
- Wartości: `0`..`3`; domyślnie: wyłączone, czyli zachowanie stock (tabela, którą FDK wybiera
  według pozycji ataku)
- Wpisywanie: liczba całkowita

Ramka transjentowa składa się z ośmiu krótkich okien. Okna w tej samej grupie dzielą jeden
zestaw współczynników skali, więc mniej grup oznacza mniej danych pobocznych i więcej bitów
na widmo, ale grubszą kontrolę szumu w czasie. Więcej grup oznacza, że szum można
kształtować okno po oknie, za odpowiednią cenę.

Wartości:
- `0`: tabela stockowa (to samo co pozostawienie przełącznika wyłączonego).
- `1`: jedna grupa na wszystkie osiem okien. Najtaniej; szum rozkłada się na całą ramkę, co
  częściowo odkręca to, po co były bloki krótkie.
- `2`: cztery pary. Równa rozdzielczość czasowa w całej ramce. Zmierzone: najmniej dziur w
  ramkach krótkich na naszym materiale.
- `3`: ciasno wokół ataku: samo okno ataku, po jednym oknie z każdej jego strony, reszta
  zbita razem. Najwięcej bitów tam, gdzie jest atak.

Przykład w kontekście: `-p 2 -b 32000 -w 17000 --short-groups 2`.

### `--tns-thresh <x>`

**Zmienia, jak chętnie włącza się TNS.**

- Dotyczy: każdego profilu z rdzeniem: LC, rdzenia HE-AAC, LD, ELD.
- Wartości: 0.5..4.0, mnożnik; domyślnie: wyłączone, czyli zachowanie stock (1,0)
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

TNS (kształtowanie szumu w czasie) prowadzi filtr predykcyjny w poprzek częstotliwości. Tam,
gdzie sygnał ma mocną obwiednię czasową, na przykład atak wewnątrz bloku, filtr sprawia, że
szum kwantyzacji idzie za tą obwiednią w czasie, więc szum siedzi pod głośną częścią zamiast
przed nią. FDK włącza TNS w ramce tylko wtedy, gdy zysk predykcji jest dość wysoki, żeby
opłacić dane poboczne. Ten przełącznik mnoży wymagany zysk. Poniżej 1 TNS jest używany w
większej liczbie ramek; powyżej 1 w mniejszej.

W LC bloki krótkie obsługują już najostrzejsze ataki, więc TNS zajmuje się tymi łagodniejszymi.
W LD i ELD, które nie mają bloków krótkich, TNS jest głównym narzędziem przeciw pre-echu, a
niższy próg warto tam często wypróbować w pierwszej kolejności.

Przykłady, więcej TNS:
- Łagodnie: `--tns-thresh 0,85`.
- Mocno: `--tns-thresh 0.7`. Zmierzone: w LC przy niskim bitrate dodatkowe dane poboczne
  zostawiają więcej dziur.
- Przesadnie: `--tns-thresh 0.5`. TNS włączony prawie wszędzie, także tam, gdzie nie ma czego
  kształtować; bity idą na współczynniki filtru.

Przykłady, mniej TNS:
- Łagodnie: `--tns-thresh 1,3`.
- Mocno: `--tns-thresh 2`. Mniej bitów na dane poboczne, nieco mniej dziur, więcej pre-echa
  na miękkich atakach.
- Przesadnie: `--tns-thresh 4`. TNS praktycznie wyłączony.

Współpraca i konflikty: `--tns-mask` i `--tns-order` (grupa D) decydują, czy i jak duży może
być filtr; ten przełącznik decyduje, jak często się to opłaca.

### `--tns-start <Hz>`

**Ustawia najniższą częstotliwość, którą dolny filtr TNS obejmuje w blokach długich.**

- Dotyczy: AAC-LC i rdzenia LC w HE-AAC, bloków długich. Zmierzony brak efektu w LD i ELD.
- Wartości: 0..24000 Hz; domyślnie: wyłączone, czyli zachowanie stock (8 pasm współczynnika
  skali, około 1,4 kHz przy 48 kHz)
- Wpisywanie: liczba całkowita

Stock trzyma najniższe pasma poza TNS. Wartość jest zaokrąglana do najbliższej krawędzi pasma
współczynnika skali i nie może przekroczyć górnej granicy zakresu TNS.

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
- Wartości: liczby całkowite `-5..5`; `0` = stock
- Wpisywanie: liczba całkowita

Przy stałym bitrate koder może wydać bity na pokrycie całego widma grubym opisem albo na
dobry opis mniejszej liczby pasm, zostawiając dziury. To makro przesuwa tę równowagę jedną
liczbą. Ustawia sześć przełączników poniżej (`--band-knee`, `--quant-round`, `--thr-hold`,
`--scf-range`, a przy `+4` i `+5` także `--pns-above`); każdy z nich podany jawnie wygrywa
z makrem. `--verbose` pokazuje wartości, które faktycznie działają.

Zmierzone przy LC 32 kbps, 17 kHz, po dekodowaniu (wyzerowane pasma i szum pasm, które
zostały): `-5` 51% dziur, a zachowane pasma 4,75 dB czystsze od sygnału; stock 11,7% dziur,
1,96 dB; `+4` 0,2% dziur, a zachowane pasma mniej więcej tak szumne jak treść.

Przykłady:
- Łagodnie, więcej pokrycia: `--coverage 2`. Mniej dziur, trochę więcej ziarna.
- Mocno, więcej pokrycia: `--coverage 4`. Praktycznie bez dziur; dokłada podstawianie szumu od 11 kHz.
- Przesadnie, więcej pokrycia: `--coverage 5`. Wszystko wypełnione, szum od 7 kHz.
- Łagodnie, więcej precyzji: `--coverage -2`. Najpierw odchodzą najsłabsze pasma.
- Mocno, więcej precyzji: `--coverage -4`. Dużo dziur, to, co zostaje, jest czyste.
- Przesadnie, więcej precyzji: `--coverage -5`. Znika mniej więcej połowa pasm.

### `--band-knee <dB>`

**Przesuń zapas każdego pasma nad progiem przy krawędzi kodowane/wyzerowane, we wszystkich kanałach.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone.
- Wartości: `-24.0..24.0` dB, `0` = wyłączone
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Wartość dodatnia zatrzymuje pasma do N dB pod progiem, na najgrubszym kroku, zamiast je
zerować. Wartość ujemna zeruje pasma, które przechodzą próg o mniej niż N dB po pierwszym
oszacowaniu bitów, więc drugie oszacowanie oddaje te bity reszcie. Pasm PNS i IS nie
rusza; w pasmach side pary MS wygrywa `--side-knee`.

Przykłady:
- Łagodnie, zatrzymaj więcej: `--band-knee 3`.
- Mocno, zatrzymaj więcej: `--band-knee 6`. Użyteczne maksimum: powyżej około 9 dB uratowane
  grube pasma kosztują tyle bitów, że pętla tnie gdzie indziej.
- Przesadnie, zatrzymaj więcej: `--band-knee 24`. Zmierzone: gorzej niż 6.
- Łagodnie, wyrzuć więcej: `--band-knee -2`.
- Mocno, wyrzuć więcej: `--band-knee -6`.
- Przesadnie, wyrzuć więcej: `--band-knee -24`. Zostają tylko wyraźnie słyszalne pasma.

### `--quant-round <x>`

**Przesuń punkt zaokrąglania kwantyzatora: mniej albo więcej małych linii widma staje się zerem.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (stock 0,405).
- Wartości: `0.0..0.5`
- Wpisywanie: liczba dziesiętna, do 3 miejsc, `.` albo `,`

Kwantyzator zaokrągla każdą linię widma do całego kroku. Stock zaokrągla z przesunięciem
około 0,405, czyli lekko w stronę zera, co oszczędza bity. `0.5` zaokrągla do najbliższej:
mniej małych linii znika, kosztuje to bity. Niższe wartości wysyłają więcej linii do zera i
są tańsze.

Przykłady:
- Łagodnie, dokładniej: `--quant-round 0.45`.
- Mocno, dokładniej: `--quant-round 0.5`. Zaokrąglanie do najbliższej.
- Łagodnie, taniej: `--quant-round 0.35`.
- Mocno, taniej: `--quant-round 0.25`. Ciche szczegóły znikają.
- Przesadnie, taniej: `--quant-round 0.0`. Obcinanie: każda linia zaokrąglona w dół.

### `--pns-above <Hz>`

**Od tej częstotliwości w górę zastąp każde pasmo bloku długiego szumem o właściwym poziomie, bez względu na tonalność.**

- Dotyczy: AAC-LC, bloki długie. Domyślnie wyłączone.
- Wartości: `1000..48000` Hz
- Wpisywanie: liczba całkowita

Górne pasmo „jak SBR, bez SBR”: pasmo zachowuje poziom, a jego treść staje się szumem.
Włącza PNS nawet poniżej jego bramki bitrate; `--pns 0` nadal wygrywa. Ciche pasma zostają
ciche. Przy niskim bitrate używaj razem z `--pns-budget 1` i `--pns-shared 1`, inaczej ramkom
może zabraknąć bitów i pojawi się trzask (patrz `--pns-budget`).

Przykłady:
- Łagodnie: `--pns-above 11000`. Szumem staje się tylko sama góra.
- Mocno: `--pns-above 5000`.
- Przesadnie: `--pns-above 2200`. Większość obszaru prezencji to szum.

### `--thr-hold <n>`

**Mierz model maskowania tylko co n ramek długich.**

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

**Ogranicz, jak daleko od siebie mogą być najdrobniej i najgrubiej kodowane pasmo kanału.**

- Dotyczy: rdzeń AAC. Domyślnie wyłączone (stock 60).
- Wartości: `0..60`, kroki po 1,5 dB
- Wpisywanie: liczba całkowita

Niższe wartości robią szum kwantyzacji bardziej płaskim w częstotliwości, z mniejszym
kształtowaniem psychoakustycznym. Na zwykłym materiale efekt jest bliski zera; widać go przy
małych wartościach na materiale z silnym nachyleniem widma (szum 6 do 14 kHz z 12,4 do 7,1 dB
pod sygnałem przy `0`, 64 kbps bez PNS). Przy 14 kbps nie zmienia budżetu.

Przykłady:
- Łagodnie: `--scf-range 40`.
- Mocno: `--scf-range 20`.
- Przesadnie: `--scf-range 0`. Jeden krok na cały kanał.

### `--bit-stats`

**Policz, na co idą bity zapisanych ramek, i wypisz tabelę na końcu.**

- Dotyczy: każdy profil. Domyślnie wyłączone.
- Wartości: brak, to flaga

Tabela dzieli ramki na nagłówek transportu, nagłówki elementów, maskę MS, dane sekcji,
współczynniki skali, poziomy szumu PNS, TNS, widmo każdego kanału (mid i side dla MS),
SBR/PS i wypełnienie, a do tego kodowane, wyzerowane, PNS i IS pasma na ramkę. Liczy bity
naprawdę zapisane; strumień jest identyczny z przełącznikiem i bez niego, a suma zgadza się z
plikiem ADTS.

Przykład: `fdkaac -p 2 -b 14000 --bit-stats -f 2 -o out.aac in.wav`

### `--enfac-patch <0|1>`

**Wyłącz przeważanie energii płaskich, szumowych ramek długich albo włącz je dla każdej ramki.**

- Dotyczy: rdzeń AAC, bloki długie, CBR i VBR. Domyślnie wyłączone (decyduje stock).
- Wartości: `0` = nigdy, `1` = każda ramka długa

Gdy słyszalne widmo ramki jest płaskie, stockowy FDK przeważa energie pasm (potęgi 3/4, 1/2
albo 1/4) i w ten sposób przesuwa bity między pasmami. `0` traktuje każdą ramkę jak tonalną,
`1` stosuje przeważanie do każdej ramki długiej. W CBR to drobne pokrętło: wyzerowane pasma
zmieniają się najwyżej o 1 punkt, a szum o mniej niż 0,1 dB przy 14 do 32 kbps; `0` na
materiale tonalnym często daje to samo co stock. W VBR (`-m 3`) `1` powiększyło plik z tonem
i szumem o 29%, a `0` zmniejszyło plik z gęstym szumem o 3%.

## F. Przełączanie bloków i kontrola bitrate'u

Sześć przełączników: jeden, który przechyla decyzję o bloku długim albo krótkim, i pięć,
które kontrolują, jak swobodnie bitrate może się ruszać z ramki na ramkę. Te pięć od
bitrate'u ma sens tylko wtedy, gdy wiesz, jak działa silnik pod spodem, więc od tego
zaczynamy.

**Silnik CBR.** Cel wybierasz przez `-b`, a koder mierzy w niego jako w średnią. Poszczególne
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
jak w LAME, więc licz się z lżejszym dotknięciem, niż sugeruje nazwa VBR.

Uwaga o dwóch twardych limitach na ramkę: nie ustawiaj `--max-bits-frame` poniżej średniej
ani `--min-bits-frame` powyżej niej. Oba walczą wtedy z celem, w który pętla dopasowania
bitrate'u próbuje trafić, a wynikiem jest gorsza jakość, nie inny kompromis. `--verbose`
wypisuje blok budżetu rezerwuaru, w tym średnią liczbę bitów na ramkę i sufity, i stamtąd
bierzesz liczby do ustawienia tych przełączników.

### `--block-bias <n>`

**Przechyla decyzję między blokami długimi a krótkimi, nigdy nie wymuszając typu bloku, więc
strumień pozostaje zgodny ze standardem.**

- Dotyczy: każdego profilu, który ma przełączanie bloków, czyli AAC-LC. LD i ELD nie mają
  bloków krótkich, więc nie ma tam czego przechylać.
- Wartości: `0..255`, gdzie `128` to własny próg kodera; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Blok długi daje dobrą rozdzielczość częstotliwościową i złą czasową; blok krótki odwrotnie.
Koder używa detektora ataku, żeby decydować ramka po ramce, którego potrzebuje. Pomyl się w
jednym kierunku, a transjent rozmaże swój szum kwantyzacji wstecz w czasie jako pre-echo;
pomyl się w drugim, a stały fragment tonalny zostanie zakodowany ośmioma krótkimi oknami,
które nie rozdzielą jego składowych, co kosztuje bity i brzmi szorstko.

Ten przełącznik przesuwa próg, z którym detektor się porównuje. Powyżej 128 bloki krótkie
stają się bardziej prawdopodobne, więc koder traktuje materiał jako bardziej transjentowy,
niż jest. Poniżej 128 preferowane są bloki długie, a `0` oznacza praktycznie tylko bloki
długie.

Co ważne, przesuwa decyzję, a nie ją nadpisuje. Wymuszenie typu okna bez przeliczenia układu
pasm i grupowania daje nielegalny strumień, który dekodery odrzucają, i ten przełącznik nigdy
tego nie robi; cokolwiek ustawisz, wyjściem jest poprawny strumień AAC.

Praktyczne odkrycie jest takie, że środek zakresu robi na większości materiału niewiele.
Decyzja na danej ramce zwykle nie jest bliska, więc odwracają ją tylko wartości blisko końców.
W praktyce traktuj to jak przełącznik trójpozycyjny, w którym `0` i `255` są interesującymi
ustawieniami, a środek jest użyteczny tylko na materiale, który naprawdę siedzi blisko granicy
decyzji.

Przykłady:
- Łagodnie, więcej bloków krótkich: `--block-bias 160`. Ramki graniczne idą na krótkie.
  Widoczne tylko na materiale blisko granicy.
- Mocno, więcej bloków krótkich: `--block-bias 220`. Wyraźnie więcej ramek krótkich. Lepsza
  rozdzielczość czasowa na treści perkusyjnej, gorsza rozdzielczość częstotliwościowa i więcej
  bitów wydanych na fragmenty tonalne.
- Przesadnie, więcej bloków krótkich: `--block-bias 255`. Prawie wszystko krótkie. Materiał
  tonalny jest kodowany ośmioma oknami, które nie rozdzielą jego składowych, i brzmi szorstko
  i drogo, co jest dobrym pokazem tego, po co są bloki długie.
- Łagodnie, więcej bloków długich: `--block-bias 100`. Ramki graniczne zostają długie.
- Mocno, więcej bloków długich: `--block-bias 40`. Wyraźnie mniej ramek krótkich. Taniej na
  materiale tonalnym, z pre-echem pojawiającym się na prawdziwych atakach.
- Przesadnie, więcej bloków długich: `--block-bias 0`. Praktycznie tylko bloki długie. Każdy
  transjent musi być niesiony przez blok długi, więc pre-echo staje się dominującym artefaktem
  na materiale perkusyjnym. To też warunek, w którym przełączniki pre-echa z grupy E w końcu
  mają znaczenie w AAC-LC, bo zwykła droga ucieczki do bloków krótkich jest zamknięta.

Współpraca i konflikty: przy `--block-bias 0` `--preecho-rise`, `--preecho-depth` i
`--tns-thresh` z grupy E zostają jedynymi narzędziami przeciw pre-echu, czyli w tej samej
sytuacji, w jakiej LD i ELD są na stałe. `--short-groups` i `--window-shape` mają znaczenie
tylko tam, gdzie bloki krótkie nadal występują, więc bardzo niskie przechylenie czyni je bez
znaczenia. `--attack-decay` i `--attack-floor` zmieniają sam detektor, a nie próg, z którym
się porównuje.

Zmierzone: przy AAC-LC 128 kbps na attacks48 strumień zmieniły tylko `0` i `255`. `64`, `128`
i `192` były identyczne ze stockiem, i to jest podstawa do traktowania środka zakresu jako
bezczynnego na zwykłym materiale.

### `--vbr-reservoir <bits>`

**Ustawia rozmiar rezerwuaru bitów, który decyduje, jak daleko i jak długo ramka może zbaczać
od średniej.**

- Dotyczy: każdego profilu, w trybie CBR. To główne pokrętło quasi-VBR.
- Wartości: bity, od `1` do `6144 * liczba kanałów` minus średnia liczba bitów na ramkę, i
  tam automatycznie przycinane; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Rezerwuar to bufor, z którego koder pożycza i do którego wpłaca. Jego rozmiar jest pamięcią
systemu: mały rezerwuar oznacza, że każde przekroczenie musi być spłacone niemal natychmiast,
co daje przyciemnienie zaraz po kulminacji; duży oznacza, że koder może trzymać się powyżej
średniej przez kilka ramek, a potem wracać stopniowo.

Większy jest zatem kierunkiem dla materiału, który powinien żyć dalej po trudnym fragmencie.
Mniejszy trzyma średnią ciaśniej z chwili na chwilę, i tego chcesz, gdy łącze transportowe ma
wąski budżet chwilowy. Dobry punkt wyjścia to dwa do trzech razy tyle, ile `--verbose`
raportuje jako wartość domyślną.

Dwa zachowania, o których warto wiedzieć. `0` jest traktowane jako nieustawione i nie robi
nic, więc jeśli chcesz ciasnego trzymania średniej, użyj małej wartości dodatniej albo
`--bitres-mode 2`. A każda wartość równa sufitowi albo większa, czyli `6144 * liczba kanałów`
minus średnia liczba bitów na ramkę, zapada się do sufitu, więc nie ma sposobu, by przesadzić,
ani korzyści z próbowania; proszenie o 100000 bitów na pliku stereo daje ten sam strumień, co
proszenie dokładnie o sufit.

Liczby odczytaj z `--verbose`, który wypisuje średnią liczbę bitów na ramkę i sufity w swoim
bloku budżetu rezerwuaru. Ustawianie tego przełącznika na zgadywanie to sposób, w jaki ludzie
przypadkiem kończą na sufice.

Przykłady:
- Łagodnie, więcej oddechu: `--vbr-reservoir 4000`. Nieco szersze wahania przy 128 kbps
  stereo; koder dochodzi do siebie po szczycie przez kilka ramek, a nie przez jedną.
- Mocno, więcej oddechu: `--vbr-reservoir 12000`. Praktyczne ustawienie quasi-VBR przy
  128 kbps stereo. Kulminacje trzymają swoją jakość, a powrót do średniej jest stopniowy.
- Przesadnie, więcej oddechu: `--vbr-reservoir 100000`. Dobrze powyżej sufitu, więc zapada
  się do sufitu. Koder używa maksymalnego oddechu, jaki format pozwala, a karykatura jest
  pouczająca właśnie dlatego, że pokazuje, jak skromne jest to maksimum: lot AAC ogranicza
  limit 6144 bitów na kanał, a nie rezerwuar.
- Łagodnie, ciaśniej: `--vbr-reservoir 2000`. Poniżej zwykłej wartości domyślnej. Wahania
  się zwężają, średnia jest trzymana bliżej.
- Mocno, ciaśniej: `--vbr-reservoir 500`. Bardzo mało buforu. Trudne ramki nie mogą dużo
  pożyczyć, więc trudne fragmenty są kodowane zgrubnie, a nie drogo.
- Przesadnie, ciaśniej: `--vbr-reservoir 1`. Praktycznie brak rezerwuaru. Każda ramka musi
  zmieścić się we własnym przydziale, więc transjenty i gęste fragmenty nie mają gdzie się
  podziać, co brzmi jak gorsza wersja tego, co `--bitres-mode 2` robi celowo.

Współpraca i konflikty: łączy się w parę z `--peak-bitrate`, który podnosi krótkoterminowy
sufit, o jaki rezerwuar może się odbijać; rezerwuar bez podniesionego szczytu ma mniej miejsca
na działanie. `--bitres-mode` może zmniejszyć albo wyłączyć rezerwuar niezależnie od rozmiaru
ustawionego tutaj. `--max-bits-frame` ogranicza poszczególne ramki po wszystkim tym, więc
ciasne ograniczenie czyni duży rezerwuar bezsensownym. Przy `--unlock-bitrate` rezerwuar staje
się ważniejszy, bo przekraczanie jest tam normalnym przypadkiem.

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
pójść. Ustaw go powyżej swojego celu, a koder będzie mógł pozwolić trudnym ramkom chwilowo
osiągnąć ten bitrate, podczas gdy pętla dopasowania bitrate'u dalej steruje średnią do `-b`.

Poniżej celu jest ignorowany, i to celowo: sufit szczytowy pod średnią nie ma sensu i koder nie
działa na tej sprzeczności. Użyteczny zakres jest więc tylko powyżej `-b`, a naturalnym
sposobem wyrażenia tego jest krotność: półtora raza cel jest ostrożne, dwa razy jest hojne.

Sufit 6144 bitów na kanał nadal obowiązuje, więc proszenie o szczyt poza to, co ramka fizycznie
pomieści, daje limit ramki. Przy 48 kHz stereo to ramka warta około 576 kbps (6144 x 2 bity x 48000 / 1024), więc dla
zwykłych celów masz dużo miejsca, zanim limit formatu zacznie wiązać.

Przykłady:
- Łagodnie, wyższy szczyt: `-b 128000 --peak-bitrate 150000`. Niewielki zapas. Kulminacje
  wychodzą nieco mniej sprasowane.
- Mocno, wyższy szczyt: `-b 128000 --peak-bitrate 200000`. Praktyczne ustawienie, zwykle w
  parze z szerokim rezerwuarem. Trudne ramki sięgają dobrze powyżej celu, a średnia jest
  utrzymana.
- Przesadnie, wyższy szczyt: `-b 128000 --peak-bitrate 1100000`. Blisko limitu ramki w
  formacie. Koder może wydać na jedną ramkę prawie całą fizyczną pojemność ramki, po czym pętla
  dopasowania bitrate'u musi wszystko to odebrać, więc dostajesz dramatyczną wersję dokładnie
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
osiągnąć, ten wyraża bezwzględne ograniczenie na ramkę, stosowane później i wygrywające. Jeśli
podasz oba, wiąże wyraźne ograniczenie.

To ograniczenie, a nie pokrętło jakości, i jego głównym zastosowaniem jest spełnienie
zewnętrznego wymagania: transportu, który nie uniesie ramki powyżej pewnego rozmiaru, albo
testu, który potrzebuje ograniczonej ramki maksymalnej. Sensowne ograniczenie to około
półtora raza średnia, co nadal dopuszcza prawdziwy oddech, ograniczając najgorszy przypadek.

Ustaw je za nisko, a głośne ramki zostaną zagłodzone, co jest słyszalne dokładnie jako to
przyciemnienie, którego zwykle chce się uniknąć. Poniżej średniej walczy z celem wprost i
koder nie może jednocześnie uszanować ograniczenia i trafić w bitrate, o który
prosisz.

Przykłady:
- Łagodnie, wyższe ograniczenie: przy 128 kbps stereo (średnio około 2730 bitów na ramkę),
  `--max-bits-frame 8000`. Dobrze powyżej czegokolwiek, co silnik normalnie wyprodukuje, więc
  w praktyce siatka bezpieczeństwa.
- Mocno, wyższe ograniczenie: `--max-bits-frame 12288`. Maksimum formatu dla stereo.
  Ograniczenie jest obecne, ale nigdy nie wiąże, co jest właściwym ustawieniem, gdy chcesz
  mieć przełącznik udokumentowany w wierszu poleceń bez zmiany zachowania.
- Przesadnie, wyższe ograniczenie: `--max-bits-frame 100000`. Powyżej limitu formatu, więc
  koder bierze mniejszą z tej wartości i własnego maksimum rezerwuaru (`6144 * liczba kanałów`),
  więc w stereo strumień jest identyczny jak przy `12288` (zmierzone: 12288, 20000 i 100000 dają
  ten sam strumień ADTS). Nieszkodliwe i przydatne jako przypomnienie, że żaden przełącznik
  tutaj nie podnosi limitu ramki.
- Łagodnie, niższe ograniczenie: `--max-bits-frame 4000` przy 128 kbps stereo. Szczyty
  przycięte, średnia nietknięta; blisko użytecznej wskazówki 1,5x.
- Mocno, niższe ograniczenie: `--max-bits-frame 3000` przy tym samym celu. Głośne ramki są
  wyraźnie zagłodzone, bo ograniczenie siedzi tylko trochę powyżej średniej.
- Przesadnie, niższe ograniczenie: `--max-bits-frame 2000` przy 128 kbps stereo, czyli poniżej
  średniej. Ograniczenie i cel przeczą sobie teraz w każdej ramce; jakość spada na całej linii
  i nic się nie zyskuje, i dlatego reguła mówi, żeby zostać na średniej albo powyżej.

Współpraca i konflikty: stosowany po `--peak-bitrate`, więc gdy podane są oba, wygrywa wyraźne
ograniczenie. Ciasne ograniczenie czyni duży `--vbr-reservoir` bezsensownym, bo bity rezerwuaru
nie mają gdzie zostać wydane. Przed wyborem wartości odczytaj średnią liczbę bitów na ramkę z
bloku budżetu rezerwuaru w `--verbose`.

Zmierzone: AAC-LC 128 kbps na attacks48. Największa ramka w stocku to 3816 bitów ładunku. Przy
`--max-bits-frame 3000` największa staje się 2888; przy `4000` staje się 3440.

### `--min-bits-frame <bits>`

**Twarda podłoga na liczbę bitów, jakich musi użyć każda ramka.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: bity, od `0` do średniej liczby bitów na ramkę; domyślnie: `-1`, wyłączone
- Wpisywanie: liczba całkowita

Lustrzane odbicie sufitu i mniej użyteczna połowa. Podłoga zmusza koder do wydania co najmniej
tyle na każdą ramkę, w tym na ramki, które naprawdę potrzebują mniej, takie jak cisza albo
trzymany cichy fragment. Te bity nie są oszczędzane na później, są wydawane, więc podłoga
zamienia łatwe ramki w zmarnowany budżet.

Powodem jej istnienia jest transport o stałym bitrate i eksperymenty: część sprzętu dalej w
łańcuchu woli przewidywalną minimalną ramkę, a czasem chcesz zobaczyć, co robi kwantyzator,
gdy mu się każe wydać więcej, niż chce.

Trzymaj ją na `0`, chyba że masz taki powód. Powyżej średniej walczy z celem tak samo, jak
zbyt niski sufit, i koder nie może uszanować obu.

Przykłady:
- Łagodnie, wyższa podłoga: `--min-bits-frame 1000` przy 128 kbps stereo. Ciche i bardzo łatwe
  ramki zostają dopełnione; średnią trzeba odrobić na trudniejszych ramkach.
- Mocno, wyższa podłoga: `--min-bits-frame 2000` przy tym samym celu, blisko średniej. Prawie
  każda ramka jest teraz na podłodze, więc silnik traci większość zdolności do redystrybucji.
- Przesadnie, wyższa podłoga: `--min-bits-frame 2730` przy 128 kbps stereo, czyli dokładnie
  średnia. W połączeniu ze średnią jako celem jest to bliskie wymuszeniu sztywnego bitrate'u na
  ramkę, z tą różnicą, że `--bitres-mode 2` robi to porządnie i bez walki z pętlą dopasowania
  bitrate'u.
- Łagodnie, niższa podłoga: `--min-bits-frame 200`. Z trudem ograniczenie; łatwe ramki
  zostają tanie.
- Mocno, niższa podłoga: `--min-bits-frame 1`. Praktycznie brak podłogi, ta sama intencja co
  pozostawienie przełącznika wyłączonego.
- Przesadnie, niższa podłoga: `--min-bits-frame 0`. Udokumentowana wartość wyłączająca podłogę;
  identyczna w efekcie z nieustawieniem jej i ustawienie, do którego wracasz po zakończeniu
  eksperymentu.

Współpraca i konflikty: przeciwny koniec `--max-bits-frame`, a oba omawia się zwykle razem jako
VBR z ograniczeniami. Wysoka podłoga marnuje bity, które `--vbr-reservoir` ma redystrybuować,
więc te dwa działają przeciw sobie. `--bitres-mode 2` to właściwy sposób uzyskania sztywnego
zachowania na ramkę.

Zmierzone: AAC-LC 128 kbps na attacks48. Wyjście stock 97056 bajtów; przy
`--min-bits-frame 1000` 97111 bajtów; przy `2900` 97709 bajtów.

### `--bitres-mode <n>`

**Wybiera, jak duża część mechanizmu rezerwuaru bitów jest aktywna, od pełnego oddechu do
sztywnego bitrate'u na ramkę.**

- Dotyczy: każdego profilu, w trybie CBR.
- Wartości: `0` pełny, `1` zredukowany, `2` wyłączony; domyślnie: `-1`, zostaw to koderowi
- Wpisywanie: liczba całkowita

Tam gdzie `--vbr-reservoir` ustawia rozmiar bufora, ten wybiera politykę. Pełny to normalne
zachowanie, takie samo jak stock. Zredukowany zwęża działanie rezerwuaru. Wyłączony go usuwa,
więc każda ramka musi zmieścić się we własnym przydziale budżetu, a to najbliższa rzecz do
sztywnego, stałego bitrate'u ramka po ramce, jaką ten koder osiąga.

Jedynym wyraźnie użytecznym ustawieniem jest `2`, a przypadkiem użycia jest transport albo
test, który potrzebuje, by każda ramka miała około tego samego rozmiaru. Koszt jest realny:
transjent albo gęsty fragment nie może pożyczyć, więc zostaje zakodowany zgrubnie.

Wartości:
- `-1`: zostaw to koderowi (domyślnie).
- `0`: pełny rezerwuar, to samo co zachowanie stock.
- `1`: zredukowany rezerwuar. Na naszym materiale dało to ten sam strumień co `2`, więc traktuj
  różnicę między tymi dwoma jako nieprzetestowaną, a nie ustaloną.
- `2`: rezerwuar wyłączony. Ramki stają się niemal stałe w rozmiarze, a trudne fragmenty tracą
  swoją drogę ucieczki.

Przykład w kontekście:

    fdkaac -p 2 -b 128000 --bitres-mode 2 -o rigid.m4a in.wav

Współpraca i konflikty: `2` czyni `--vbr-reservoir` i `--peak-bitrate` bez znaczenia, bo nie
ma rezerwuaru, na którym mogłyby działać. `0` to tryb, w którym resztę grupy F należy używać.

Zmierzone: AAC-LC 128 kbps na attacks48. `0` daje stockowy strumień; `1` i `2` dały ten sam
strumień, więc na tym materiale nie zaobserwowaliśmy różnicy między zredukowanym a wyłączonym.

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

DAB+ nie niesie po prostu AAC w innym kontenerze. Różnią się trzy rzeczy. Transformata ma 960
próbek, a nie zwykłe 1024, żeby długość ramki pasowała do siatki czasowej radia. Dźwięk jest
pakowany w superramki o dokładnie 120 ms. A skoro eter jest nieprzyjazny, z zanikami, tunelami
i zakłóceniami, strumień jest chroniony podwójnie: firecode, czyli Fire CRC, pilnuje nagłówka,
a Reed-Solomon RS(120,110) nad GF(256) chroni ładunek, więc odbiornik może odbudować dane,
które przyszły uszkodzone. Koder robi to wszystko.

Wyjściem jest surowy strumień `.dabp`, jedna superramka po drugiej i nic więcej. To nie jest
plik do odtwarzacza multimedialnego; to coś, co pobiera multiplekser DAB+. Zwykły łańcuch to
`out.dabp` do `odr-dabmux`, który łączy kilka stacji w jeden zespół i wypuszcza ETI, a stamtąd
do nadajnika albo do odbiornika programowego, takiego jak welle.io albo dablin, do testów.

Trzy ograniczenia, które narzuca standard, a koder ich pilnuje: częstotliwość próbkowania musi
być 32000 albo 48000 Hz, nic innego nie jest w DAB+ legalne; bitrate musi być wielokrotnością
8 kbps między 8 a 192; mono i stereo oba są w porządku.

Konwencja bitrate'u jest tu własna. Przy `--dab` `-b` to kilobity na sekundę, a koder sam mnoży
przez 1000, więc `-b 96` oznacza 96 kbps. `--unlock-bitrate` tego nie zmienia: kontrola DAB+
i mnożenie przez 1000 dzieją się wcześniej, więc `--dab --unlock-bitrate -b 96` daje ten sam
strumień co bez odblokowania, a `-b 96000` zostaje odrzucone jako niewielokrotność 8 kbps.

Normalnie profilu nie wybierasz. Praktyka DAB+, idąc za narzędziem referencyjnym
`odr-audioenc`, wyprowadza go z bitrate'u i liczby kanałów, tak by niskie bitrate'y
automatycznie dostawały oszczędniejsze narzędzia:

- stereo przy 48 kbps albo niżej, czyli rozmiar podkanału 6 lub mniej, daje HE-AAC v2 z
  parametric stereo;
- mono do 64 kbps albo stereo do 80 kbps daje HE-AAC z SBR;
- cokolwiek wyżej daje AAC-LC.

Zatem `-b 96` na pliku stereo trafia na AAC-LC, `-b 64` na HE-AAC, a `-b 32` na HE-AAC v2
automatycznie. Profil nadal możesz wymusić przez `-p` (`2` dla LC, `5` dla HE-AAC, `29` dla
HE-AAC v2), kiedy wiesz, czego chcesz.

Wartości:
- pominięte: normalne wyjście, a koder zachowuje się dokładnie jak stock.
- `--dab`: wyjście superramek DAB+.

Przykłady w kontekście:

    fdkaac --dab -b 96 -o out.dabp input.wav     # auto AAC-LC
    fdkaac --dab -b 64 -o out.dabp input.wav     # auto HE-AAC
    fdkaac --dab -b 32 -o out.dabp input.wav     # auto HE-AAC v2

Współpraca i konflikty: `--dab-label` robi coś tylko w tym trybie. `-f` i przełączniki kontenera
MP4 nie mają zastosowania, bo nie ma kontenera. `--unlock-bitrate` zmienia sposób odczytu `-b`.
Przełączniki z grup D i F nadal działają na rdzeń AAC wewnątrz superramek, więc pokrętła od
szczegółu i bitrate'u pozostają dostępne.

Zmierzone: strumienie dekodują się niezależnym dekoderem faad2 za dablinem, etykieta DLS
odczytuje się poprawnie, a wyjście AAC-LC jest identyczne bit w bit z tym, co z tego samego
wejścia produkuje referencyjny `odr-audioenc`. Przetestowaliśmy dziewięć kombinacji, 48 i
32 kHz przeciw mono i stereo w trzech profilach, każda niezależnie dekodowalna.

### `--dab-label <text>`

**Niesie statyczną etykietę stacji w superramce, pokazywaną na wyświetlaczu odbiornika DAB+.**

- Dotyczy: wyłącznie trybu `--dab`. Bez `--dab` etykieta jest ignorowana, bo nie ma superramki,
  w której można by ją umieścić.
- Wartości: tekst, do około 48 znaków; domyślnie: brak etykiety
- Wpisywanie: łańcuch tekstowy

Odbiornik DAB+ pokazuje tekst na swoim wyświetlaczu, zwykle nazwę stacji i często tytuł tego,
co leci. Ten tekst podróżuje w kanale zwanym DLS, czyli Dynamic Label Segment, upchniętym w
superramce jako dane X-PAD. Ten koder niesie etykietę statyczną: jeden ustalony łańcuch dla
całego pliku albo strumienia.

Statyczna to słowo kluczowe. Tekst nie zmienia się w trakcie kodowania. Prawdziwie dynamiczna
etykieta, która aktualizuje się utwór po utworze, oraz pokaz slajdów MOT, który wyświetla
okładki, to zadanie `ODR-PadEnc` w łańcuchu OpenDigitalRadio i nie są częścią tego kodera.

Limit długości około 48 znaków wynika z trzech segmentów DLS upakowanych w jeden PAD.
Odbiorniki różnią się tym, ile z długiej etykiety pokazują naraz, więc dla nazwy stacji
użyteczna długość jest krótsza niż limit.

Wartości:
- pominięte: brak etykiety w strumieniu.
- `--dab-label <text>`: podany łańcuch jest niesiony jako DLS. Jeśli zawiera spacje, weź go
  w cudzysłowy.

Przykład w kontekście:

    fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

Współpraca i konflikty: wymaga `--dab`. Niezależny od każdego innego przełącznika, bo dotyka
PAD, a nie dźwięku.

# Dodatki

## Jak czytać `--verbose`

`--verbose` wypisuje wartości, które enkoder faktycznie zainicjalizował, zanim zacznie
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
| `bit reservoir budget` | Średnia liczba bitów na ramkę i sufity rezerwuaru dla mono i stereo. Na tej podstawie dobierz `--vbr-reservoir`. |
| `franken overrides applied` | Przełączniki, które w tym uruchomieniu odbiegają od stocku, z wartością efektywną. Jeśli podanego przełącznika tu nie ma, nie zadziałał. |

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

Bez `-w` FDK wybiera pasmo z tabeli indeksowanej bitrate'em na kanał, a potem wyrównuje
je do granicy pasma współczynnika skali. Zmierzone przez `--verbose` przy 48 kHz (dla
stereo `-b` to suma obu kanałów):

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

Opcjonalny jest cały blok `udta` / `meta` / `ilst`: znacznik identyfikujący enkoder
(`©too`), `iTunSMPB` (opóźnienie enkodera dla odtwarzania bez przerw) i wszystkie znaczniki
tekstowe.

| Przełącznik | Działa na | Co robi |
|---|---|---|
| `--no-tool-tag` | `.m4a` | Pomija znacznik identyfikujący enkoder. Pozostałe znaczniki i dane gapless zostają. |
| `--minimal-moov` | `.m4a` | Najmniejszy legalny plik: pomija cały blok metadanych (znacznik enkodera, `iTunSMPB`, wszystkie znaczniki). Szkielet odtwarzania zostaje. |

Zmierzone na 2 s stereo 128 kbps: 34381 bajtów domyślnie, 34297 z `--no-tool-tag`
(o 84 mniej), 34048 z `--minimal-moov` (o 333 mniej). Narzut to głównie obowiązkowy
szkielet. Bez kontenera w ogóle zapisz surowe ADTS (`-f 2 -o out.aac`), które nie ma
znaczników ani danych gapless.

`--minimal-moov` usuwa `iTunSMPB`, więc przy łączeniu utworów mogą pojawić się drobne
przerwy, bo opóźnienie enkodera nie jest wtedy sygnalizowane. Dla albumów odtwarzanych
bez przerw zostaw ustawienie domyślne.

## Domyślne wyjście a oryginalna binarka

Sprawdzone względem oryginalnej binarki dostarczanej z dBpoweramp R17 (ta sama
libfdk-aac 4.0.1, pakiet 2.0.3). AAC-LC jest identyczny bit w bit (ta sama suma md5).
HE-AAC i HE-AAC v2 różnią się bajtami, ale nie jakością: widmo HE-AAC v2 zgadza się co do
0,000 dB, a błąd HE-AAC względem oryginału jest taki sam jak błąd samego oryginału
(7,05e11 wobec 7,05e11). Różnice bajtów wynikają z innego kompilatora (mingw/gcc zamiast
MSVC) i zaokrągleń arytmetyki stałoprzecinkowej.

# Budowanie i testy

## Budowanie

Repozytorium zawiera tylko źródła. GitHub Actions buduje obie binarki dla Windows na
żądanie (workflow „Build Franken FDK AAC (Windows x64 + x86)”, przycisk „Run workflow”) oraz
przy każdym tagu wydania. Lokalnie, na Linuksie albo w WSL z mingw-w64 i autotools:

    bash tools/build_win.sh both      # albo x64 / x86

Skrypt buduje każdą architekturę ze świeżej kopii źródeł, tak samo jak CI, i zostawia
`fdkaac-franken-x64.exe` oraz `fdkaac-franken-x86.exe` w katalogu projektu. Wszystkie
zmiany w libfdk-aac przechodzą przez jeden moduł, `libAACenc/src/franken.{h,cpp}` (globalny
blok `g_franken`, w którym każda nieustawiona wartość oznacza „stock”); nowe parametry
biblioteki (`AACENC_PARAM`, zakres `0xF0xx`) są zadeklarowane w `aacenc_lib.h`.

## Testy

    make check

uruchamia `tests/check.sh` na zbudowanych binarkach. Sprawdza, że każdy przełącznik jest w
`--help`, że każdy daje strumień przyjmowany przez dekodery bez błędów, że każdy faktycznie
zmienia strumień na materiale, na który powinien działać, że wartości odczytane ze
strumienia zgadzają się z żądanymi (kształt okna, grupy, rozmiary ramek), że dziury i
pre-echo po dekodowaniu zmieniają się w zmierzonym kierunku, oraz że bez nowych
przełączników wynik jest identyczny bajt w bajt ze stockiem. Wymaga ffmpeg i python3; kod
wyjścia 0 oznacza sukces, 1 błędy, 77 brak zależności.

Narzędzia pomiarowe, z których pochodzą liczby w tym dokumencie, są w `tools/`:
`tools/core/` (dziury i pre-echo po dekodowaniu), `tools/invf/` (filtrowanie odwrotne SBR i
progi niskiego bitrate'u), `tools/msis/` (MS i IS odczytane ze strumienia, nagłówki SBR,
dołączanie do strumienia), `tools/dab/` (weryfikacja DAB+).
