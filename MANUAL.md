# Franken FDK AAC — Manual dla dźwiękowca

Kompletny przewodnik po kodowaniu dźwięku w AAC / MPEG-4 oraz po wszystkich
opcjach tego enkodera. Napisany dla osoby, która zna się na dźwięku i biegle
posługuje się komputerem, ale nie musi być programistą ani matematykiem.

Wersja dokumentu: lipiec 2026. Enkoder: libfdk-aac 2.0.3 + frontend nu774, z
rozszerzeniami "Frankenstein".

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski.
Zbudowane na oprogramowaniu open source: libfdk-aac (Fraunhofer IIS) oraz
frontend nu774/fdkaac.

---

# Część I. Jak w ogóle działa AAC

## 1. Po co komu kodek stratny

Nagranie PCM (to, co jest na płycie CD albo w pliku WAV) zapisuje każdą próbkę
dźwięku dokładnie. Przy 44,1 kHz i 16 bitach na kanał stereo to około 1411
kilobitów na sekundę. To dużo. Kodek stratny, taki jak AAC czy MP3, potrafi
oddać ten sam materiał przy 128, 96, a nawet 64 kilobitach tak, że ucho w
codziennych warunkach różnicy nie usłyszy — albo usłyszy bardzo niewielką.

Sztuczka nie polega na "ściśnięciu" dźwięku jak plik ZIP. Polega na WYRZUCENIU
tego, czego i tak nie usłyszysz, i zapisaniu tylko reszty. Cała inteligencja
kodeka leży w tym, żeby dobrze zgadnąć, co jest nieistotne. To jest model
psychoakustyczny — matematyczny opis tego, jak działa ludzki słuch.

## 2. Trzy filary psychoakustyki

Enkoder podejmuje decyzje w oparciu o kilka zjawisk, które warto rozumieć,
bo połowa opcji tego programu właśnie nimi steruje.

MASKOWANIE JEDNOCZESNE. Głośny dźwięk o jakiejś częstotliwości sprawia, że
przestajesz słyszeć cichsze dźwięki tuż obok niego w widmie. Jeśli gra głośne
1000 Hz, to cichy sygnał 1100 Hz jest "zamaskowany" — ucho go nie wychwyci.
Enkoder może więc zapisać ten zamaskowany fragment bardzo niedbale (mało bitów)
albo w ogóle. To jest najważniejszy mechanizm oszczędzania bitów w AAC.

MASKOWANIE CZASOWE. Tuż przed i tuż po głośnym uderzeniu (np. werbla) ucho jest
"ogłuszone" i nie słyszy cichych detali. Enkoder to wykorzystuje.

PRÓG SŁYSZALNOŚCI (ATH, Absolute Threshold of Hearing). Istnieje granica ciszy,
poniżej której ucho po prostu nie rejestruje dźwięku — i granica ta jest różna
dla różnych częstotliwości (najlepiej słyszymy 2–5 kHz, gorzej skraje pasma).
Cokolwiek jest poniżej tego progu, można wyrzucić.

Kiedy słyszysz, że kodek "zabrał powietrze" albo "spłaszczył blachy", to prawie
zawsze znaczy, że model psychoakustyczny uznał coś za niesłyszalne, a Twoje ucho
się z nim nie zgodziło. Duża część tego manuala to nauka, jak przesuwać te
decyzje w stronę Twojego ucha.

## 3. Pasma, MDCT i współczynniki skali

AAC nie pracuje na próbkach w czasie, tylko na WIDMIE. Sygnał jest cięty na
ramki (1024 próbki) i przekształcany do dziedziny częstotliwości (transformata
MDCT). Powstałe współczynniki grupowane są w PASMA WSPÓŁCZYNNIKÓW SKALI
(scale factor bands, w skrócie SFB). Numeracja SFB idzie OD DOŁU: SFB 0 to
najniższe częstotliwości (basy), a im wyższy numer, tym wyżej w widmie.

To ważne, bo wiele opcji tego enkodera operuje właśnie na numerach pasm SFB.
Gdy mówimy "MS na 5 najwyższych pasmach", chodzi o pasma o najwyższych numerach.

W każdym paśmie enkoder decyduje, jak dokładnie (ile bitów) zapisać
współczynniki. Im mniej bitów, tym większy błąd kwantyzacji — słyszalny jako
szum albo "dziura" w widmie. Model psychoakustyczny mówi, gdzie ten błąd się
schowa pod maskowaniem, a gdzie będzie słyszalny.

## 4. Rodziny AAC: LC, HE-AAC, HE-AAC v2

AAC to nie jeden format, tylko rodzina profili. W tym enkoderze wybierasz je
przełącznikiem `-p`:

AAC-LC (Low Complexity, profil 2). Podstawowy, najczęściej używany wariant.
Koduje pełne pasmo tradycyjnie. Sensowny od około 96 kbps stereo w górę.
Bezpieczny wybór do muzyki w dobrej jakości.

HE-AAC (High Efficiency, profil 5, zwany też AAC+ albo aacPlus). Do LC dokłada
SBR — Spectral Band Replication. Rdzeń AAC koduje tylko dolną część pasma
(np. do 8–13 kHz), a górę SBR "odtwarza sztucznie" na podstawie dolnej i
niewielkiego opisu. Ogromna oszczędność bitów przy zachowaniu wrażenia
szerokiego pasma. Sensowny w okolicach 32–80 kbps stereo.

HE-AAC v2 (profil 29). Do HE-AAC dokłada PS — Parametric Stereo. Zamiast kodować
dwa kanały, koduje jeden (mono) plus garść parametrów opisujących, jak z niego
odtworzyć stereo. Sensowny w bardzo niskich bitrate'ach, 16–48 kbps stereo.

Zasada praktyczna: im niższy bitrate, tym więcej "protez" (SBR, PS). Przy 320
kbps te protezy tylko przeszkadzają — używaj wtedy LC.

## 5. Co to jest SBR (górne pasmo sztuczne)

Warto zrozumieć SBR, bo steruje nim wiele opcji. SBR NIE koduje górnego pasma
próbka po próbce. Zamiast tego wysyła OBWIEDNIE (envelopes) — informację "w tym
kawałku czasu i w tym zakresie częstotliwości energia ma taki a taki kształt" —
plus wskazówki, z których fragmentów dolnego pasma "skopiować" materiał w górę,
i ile dołożyć szumu. Dekoder rekonstruuje górę na tej podstawie.

Dlatego górne pasmo w HE-AAC brzmi inaczej niż oryginał: to inteligentna
rekonstrukcja, nie kopia. Opcje SBR w tym enkoderze pozwalają sterować
gęstością obwiedni w czasie, ich rozdzielczością częstotliwości, ilością
wstrzykiwanego szumu i tak zwanym filtrowaniem odwrotnym (o tym dalej).

## 6. Co to jest Parametric Stereo (PS)

PS idzie jeszcze dalej niż SBR w oszczędzaniu. Wysyła jeden kanał dźwięku i
parametry opisujące różnice między lewym a prawym:

- IID (Inter-channel Intensity Difference) — różnica głośności między kanałami.
- ICC (Inter-channel Coherence) — jak bardzo kanały są do siebie podobne/spójne.
- IPD/OPD (różnice fazy) — te w enkoderze FDK NIE są wspierane (zawsze zero);
  o tym uczciwie w części o opcjach PS.

Dekoder z tego jednego kanału i tych parametrów "rozkłada" dźwięk z powrotem na
stereo. Przy 24 kbps to jedyny sposób, żeby stereo w ogóle miało sens. Przy
wyższych bitrate'ach PS zwykle przeszkadza i się go nie używa.

---
# Część II. Bitrate, rezerwuar bitów i sterowanie jakością

## 7. CBR, VBR i co się naprawdę dzieje w środku

CBR (Constant Bitrate) to stały bitrate: każda sekunda dźwięku zajmuje mniej
więcej tyle samo miejsca. Wygodne do streamingu, przewidywalne. Wybierasz go
przełącznikiem `-b` (np. `-b 128000` = 128 kbit/s).

VBR (Variable Bitrate) to bitrate zmienny: fragmenty trudne (dużo detali,
transjenty) dostają więcej bitów, ciche i proste mniej. Teoretycznie lepsza
jakość na bit. W FDK VBR wybiera się przełącznikiem `-m` (tryby 1–5), ale — i to
trzeba powiedzieć wprost — VBR w FDK jest słaby. Presety są zachowawcze i nie
dają tej swobody, co np. LAME w MP3.

Kluczowa rzecz do zrozumienia: NAWET w trybie CBR bitrate lokalnie "oddycha".
Służy do tego REZERWUAR BITÓW (bit reservoir). To bufor: gdy ramka jest łatwa,
enkoder zapisuje ją oszczędnie i odkłada zaoszczędzone bity do rezerwuaru; gdy
przychodzi ramka trudna, pożycza z rezerwuaru i wydaje więcej, niż wynikałoby ze
średniej. Średnia zostaje zachowana, ale chwilowo bitrate skacze w górę i w dół.
To dlatego "CBR" w AAC nie jest idealnie płaski.

Ten enkoder wystawia sterowanie rezerwuarem na zewnątrz, i to jest podstawa
naszego własnego, lepszego VBR — patrz następny rozdział.

## 8. Przepis: VBR, który nie tnie ostatnich ramek "na siłę"

To jest dokładnie sytuacja, o którą często chodzi: masz trudny fragment (np.
wejście całej sekcji dętej albo talerze), enkoder wypuścił kilka ramek grubo
powyżej średniej — i zamiast pozwolić tej jakości "żyć", zaczyna dusić kolejne
ramki, żeby tylko wrócić do średniej. Efekt: słyszalne przygaszenie zaraz po
głośnym momencie.

Nasze rozwiązanie to quasi-CVBR: bierzemy silnik CBR (który trzyma sensowną
średnią), ale ROZSZERZAMY rezerwuar i widełki bitów, żeby enkoder mógł oddychać
znacznie mocniej i nie musiał natychmiast "oddawać długu". Cztery przełączniki:

`--vbr-reservoir <bity>` — rozmiar rezerwuaru. Większy = enkoder może dłużej
utrzymać podwyższoną jakość po trudnym fragmencie, zanim wróci do średniej.

`--peak-bitrate <bps>` — dozwolony chwilowy szczyt. Pozwala trudnym ramkom
sięgnąć wysoko, przy zachowaniu średniej.

`--max-bits-frame <n>` i `--min-bits-frame <n>` — twarde widełki bitów na ramkę.
To rdzeń "constrained VBR": mówisz "nie mniej niż X, nie więcej niż Y na ramkę".

`--bitres-mode <0|1|2>` — tryb rezerwuaru: 0 pełny, 1 zredukowany, 2 wyłączony
(sztywny bitrate ramka po ramce).

PRZEPIS PRAKTYCZNY dla materiału, który ma "żyć" po trudnych fragmentach
(~128 kbps stereo, muzyka akustyczna/orkiestrowa):

    fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 200000 --vbr-reservoir 12000 -o out.m4a in.wav

Co to robi: średnia zostaje w okolicach 128, ale gdy przychodzi kulminacja,
ramki mogą sięgnąć 200 kbps, a szeroki rezerwuar (12000 bitów) sprawia, że
enkoder NIE dusi natychmiast kolejnych ramek — pozwala jakości opaść łagodnie.
To jest dokładnie ten efekt "nie tnij na siłę, pozwól temu żyć".

Chcesz mocniej: podnieś `--peak-bitrate` do 256000 i `--vbr-reservoir` do 18000.
Chcesz twardziej trzymać średnią (limit łącza): zmniejsz rezerwuar do 4000.

WAŻNE OGRANICZENIE, uczciwie: to nadal działa na silniku CBR + rezerwuar, nie na
prawdziwej alokacji "po zawartości" jak w LAME. Wahania są umiarkowane (rzędu
±20–50%), bo AAC ma twardy limit 6144 bity na kanał na ramkę. Ale ten "lekki
lot" to właśnie to, co zwykle chcesz — i jest w pełni kontrolowany.

## 9. Skrajne bitrate: bardzo nisko i bardzo wysoko

BARDZO NISKO. Standardowo FDK nie pozwoli zejść poniżej pewnej podłogi. Jeśli
naprawdę chcesz 8 kbps HE-AAC stereo albo 6 kbps LC (choćby dla eksperymentu),
użyj flagi `--unlock-bitrate`. UWAGA na jeden szczegół: normalnie ten frontend
traktuje małe liczby przy `-b` jako kilobity (czyli `-b 96` = 96 kbps). W trybie
`--unlock-bitrate` bierze `-b` DOSŁOWNIE jako bity na sekundę — czyli `-b 8000`
to 8000 bps = 8 kbps. Poniżej mniej więcej 10 kbps jest naturalna podłoga
wynikająca z samych nagłówków AAC — niżej się nie da bez łamania formatu.

    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

DWA WAŻNE ZASTRZEŻENIA do bardzo niskich bitrate'ów:

Po pierwsze — `--unlock-bitrate` obniża podłogę tylko dla AAC-LC (profil 2).
HE-AAC i HE-AAC v2 (profile 5 i 29) mają WŁASNĄ, twardą podłogę około 16 kbps,
wynikającą z tego, że SBR nie potrafi skonfigurować się poniżej — enkoder albo
podniesie bitrate do 16 kbps, albo w ogóle odmówi startu. Dlatego jeśli pod
HE-AAC wpiszesz `-b 5` albo `-b 8000`, dostaniesz 16 kbps (program Cię o tym
ostrzeże). Chcesz naprawdę niżej niż 16 kbps? Użyj AAC-LC: `-p 2 -b 8000
--unlock-bitrate`.

Po drugie — UWAGA na interpretację `-b` w trybie unlock. Normalnie ten frontend
traktuje małe liczby jako kilobity: `-b 128` = 128 kbps, `-b 1152` = 1152 kbps.
Ale z `--unlock-bitrate` liczba jest brana DOSŁOWNIE jako bity na sekundę:
`-b 8000` = 8000 bps = 8 kbps. Gdybyś przez pomyłkę napisał `-b 1152
--unlock-bitrate` (myśląc "1152 kbps"), dostałbyś 1152 bps — czyli praktycznie
nic. Program ostrzeże, gdy wartość wygląda na taką pomyłkę. Reguła: w trybie
unlock zawsze podawaj pełną liczbę bitów (np. `-b 6000`, nie `-b 6`).

BARDZO WYSOKO. Górny sufit to 6144 bity na kanał na ramkę. Przy 48 kHz to
wychodzi około 576 kbps na kanał; przy stereo realne ograniczenie pojawia się
w okolicach 600 kbps sumarycznie. Jeśli chcesz 1000+ kbps, musisz podnieść
częstotliwość próbkowania powyżej 48 kHz (źródło 96 kHz) — wtedy sufit rośnie i
osiągniesz 1024, a nawet 1152 kbps. To ograniczenie samego standardu AAC.

Uwaga o jednoznaczności: przy tak wysokich bitrate'ach (1152) NIE używasz
`--unlock-bitrate` (on służy do schodzenia w dół), więc `-b 1152` znaczy tam
jednoznacznie 1152 kbps. Kolizja interpretacji dotyczy wyłącznie trybu unlock,
gdzie i tak operujesz małymi liczbami bps. Praktycznie te dwa światy się nie
stykają.

## 9a. Czy warto robić "afterburner plus" — więcej iteracji przy skrajnym bitrate

Skoro optymalizacja przy bardzo wysokim bitrate to Twój konik, jedno trzeba
powiedzieć wprost, bo inaczej stracisz czas na ślepą uliczkę. W środku enkodera
jest pętla, która iteracyjnie dobiera kwantyzację (parametr wewnętrzny
"maxIterations"; afterburner włączony daje 5 prób, wyłączony 2). Kuszące jest
myślenie "dam 20 iteracji, będzie dokładniej". Ale ta pętla to mechanizm
RATUNKOWY na wypadek NIEDOBORU bitów — uruchamia się dopiero, gdy ramka nie
mieści się w budżecie i trzeba ją docisnąć. Przy skrajnie WYSOKIM bitrate budżet
jest ogromny, ramka mieści się od razu, więc pętla prawie nigdy nie wchodzi w
kolejne iteracje. Zwiększanie limitu nic tam nie da — to nie jest "więcej
polerowania", tylko "wyższy limit awaryjnego dociskania", którego i tak nie
używasz. Sam afterburner (`--afterburner 1`) już wybiera dokładniejszą tabelę
strojenia (i to jest włączone domyślnie) — a wyżej ten konkretny mechanizm nie
sięga, bo w kodeku nie ma dodatkowych poziomów.

Co NAPRAWDĘ daje więcej detalu przy wysokim bitrate, to nie liczba iteracji, ale
OBNIŻENIE PROGÓW MASKOWANIA, żeby enkoder w ogóle uznał więcej rzeczy za warte
zakodowania: `--ath-scale` poniżej 256, `--spread-mask` poniżej 256 i `--side-bias` powyżej 0
(więcej bitów w szerokość stereo). To są dźwignie, które faktycznie zamieniają
nadmiarowe bity na zachowany detal (patrz rozdział 18). Iteracje kwantyzacji nie.

BARDZO WYSOKO (ciąg dalszy właściwej optymalizacji) — patrz rozdział 18.

---

# Część III. Opcje standardowe (nie tylko Frankenstein)

To są przełączniki dostępne w każdej wersji tego frontendu. Znać je warto, bo
to fundament, na którym budujesz resztę.

## 10. Wybór profilu i podstawy

`-p <n>` — profil (Audio Object Type). Najważniejsze: `2` = AAC-LC, `5` =
HE-AAC, `29` = HE-AAC v2, `23` = AAC-LD (low delay, do komunikacji), `39` =
AAC-ELD. Do muzyki: 2 dla wysokich bitrate, 5 dla średnich, 29 dla najniższych.

`-b <n>` — bitrate w bitach na sekundę dla CBR (np. `-b 192000`). Małe liczby
frontend mnoży ×1000 (`-b 192` = 192 kbps) — poza trybem `--unlock-bitrate`.

`-m <n>` — tryb VBR 1–5 (1 najniższa jakość, 5 najwyższa). Jak wspomniano,
w FDK słaby; do poważnej pracy użyj CBR + rozdział 8.

`-o <plik>` — plik wyjściowy. Rozszerzenie `.m4a` daje kontener MP4;
`-f 2` przełącza na surowy strumień ADTS (`.aac`).

`-w <n>` — szerokość pasma (bandwidth) rdzenia w Hz. Uwaga: pod SBR to nie
działa tak, jak się spodziewasz — do sterowania pasmem rdzenia pod HE-AAC użyj
`--core-cutoff` (część IV).

## 11. Afterburner i sygnalizacja

`--afterburner <0|1>` — afterburner to dodatkowy, wolniejszy algorytm
optymalizacji kwantyzacji. `1` = włączony (lepsza jakość, wolniej), i to jest
domyślne oraz zalecane. Wyłączaj tylko gdy zależy Ci na szybkości.

`-s <n>` / signaling — sposób sygnalizowania SBR/PS w strumieniu (kompatybilność
ze starszymi dekoderami). Domyślne "auto" jest w większości przypadków dobre.

## 12. Kontener i metadane

`--moov-before-mdat` — umieszcza indeks MP4 na początku pliku (przydatne do
streamingu progresywnego). Standardowe tagi (tytuł, artysta itd.) ustawia się
przełącznikami `--title`, `--artist`, `--album` i pokrewnymi.

---
# Część IV. Opcje Frankenstein — sterowanie wnętrzem enkodera

To jest sedno tego enkodera: przełączniki, które w zwykłym FDK są zaszyte na
sztywno, a tutaj wystawione na zewnątrz. Każdy opisany jest praktycznie — co
robi dla ucha, kiedy go użyć, i jakie wartości mają sens.

## 13. Stereo w rdzeniu: MS i Intensity

MS STEREO (Mid/Side). Zamiast kodować lewy i prawy kanał osobno, koduje sumę
(mid = L+R) i różnicę (side = L−R). Gdy kanały są podobne (a w muzyce zwykle
są), różnica jest mała i tania w zapisie. To niemal darmowa oszczędność.

`--msmask <0|1>` — wymuś: `0` = wszędzie osobne L/R (pełna niezależność kanałów),
`1` = MS na wszystkich pasmach. Domyślnie enkoder decyduje sam per pasmo.

`--msbands <n>` — ogranicz MS do pasm o numerze mniejszym niż n. Czyli bierze
N NAJNIŻSZYCH pasm (pamiętaj: pasma numerowane są od dołu, 0 = basy). Powyżej —
czyste L/R. Przykład: `--msbands 6` = MS tylko na 6 najniższych pasmach.

`--msbands-lo <lo>` i `--msbands-hi <hi>` — to para "OD–DO". Podajesz OBA
przełączniki i wyznaczasz przedział numerów pasm, w którym MS jest dozwolone;
poza tym przedziałem idzie czyste L/R. To pozwala umieścić MS GDZIEKOLWIEK, w tym
na samej górze widma — czego `--msbands` (zawsze od dołu) nie potrafi.

Jak to zapamiętać: `--msbands` = "od dołu do numeru N"; `--msbands-lo/-hi` =
"od numeru LO do numeru HI". Przykłady (zakładając około 49 pasm w LC stereo):

- MS tylko na 5 NAJWYŻSZYCH pasmach (scalić szum w górze, dół zostawić w pełnym
  stereo): najwyższe pasma to numery 44–48, więc `--msbands-lo 44 --msbands-hi 48`.
- MS tylko w środku pasma: `--msbands-lo 10 --msbands-hi 30`.
- MS na 6 najniższych: prościej `--msbands 6` (to samo co `--msbands-lo 0
  --msbands-hi 5`).

Ile pasm masz realnie dla danego trybu i częstotliwości pokazuje `--verbose`
(pole "active SFBs") — sprawdź tam górny numer, zanim ustawisz `--msbands-hi`.

STEROWANIE BALANSEM STEREO (główne pokrętło). `--side-bias <dB>` to przełącznik, po który
sięgasz, gdy chcesz decydować, ile budżetu bitów pójdzie w szerokość stereo. Przesuwa próg
maskowania kanału SIDE (L−R) na pasmach kodowanych w MS, dokładnie w tym miejscu, gdzie FDK
decyduje, czy pasmo skali (SFB) zostanie zakodowane, czy wyzerowane (`energia > próg`
w `sf_estim.cpp`). Znak jest efektem. Wartość DODATNIA kieruje WIĘCEJ bitów do kanału side:
próg spada, mniej pasm side jest zerowanych, a te, które przetrwają, są kwantyzowane dokładniej
— dostajesz czystszą szerokość stereo, ogony pogłosu i ambience — kosztem kanału mid. Wartość
UJEMNA celowo degraduje side: podnosi próg, pasma side wypadają, a obraz zwęża się w stronę
mono. Nie dzieje się tu nic egzotycznego; to dokładnie ta sama zależność energia-vs-próg, której
LAME używa do alokacji bitów, a MusePack steruje przez `--ms`. To tradeoff przy stałym budżecie,
więc przy niskim bitrate usłyszysz, jak kanał mid oddaje część bitów, żeby nakarmić side — to
oczekiwane zachowanie, nie usterka. Rozsądny, ludzki zakres: **+3 do +9** dla „szerzej i bardziej
przestrzennie"; na ujemne schodź tylko wtedy, gdy świadomie chcesz skrajnego, artystycznego
niszczenia szerokości przy niskim bitrate. Wartość jest w decybelach (dziesiętnie), a `0`
oznacza off — bit-identycznie ze stockowym FDK.

KSZTAŁTOWANIE ODCIĘCIA SIDE. `--side-knee <dB>` steruje tym, JAK OSTRO pasmo side przełącza się
między „kodowane" a „wyzerowane" na tym progu. Stock FDK to twardy klif: w chwili gdy energia
pasma spadnie do progu lub poniżej, całe pasmo jest wyrzucane. Wartość DODATNIA daje MIĘKKIE
kolano — pasma leżące do N dB *poniżej* progu są nadal zachowane (kodowane na najzgrubszym
współczynniku skali) zamiast wyrzucane, więc side gaśnie stopniowo zamiast wyłączać się nagle;
pogłos i „powietrze" wybrzmiewają łagodniej. Wartość UJEMNA daje TWARDE kolano — pasma, które
ledwo przekraczają próg (do N dB *nad* nim), są mimo to zerowane, odcinając side wcześniej, dla
chudszego, agresywniejszego efektu. Jest ortogonalny do `--side-bias` i łączy się z nim: bias
ustawia, *gdzie* leży próg, a knee — *jak miękka jest jego krawędź*. Rozsądny zakres **+3 do +6**.
`0` = off.

STROJENIE CICHYCH PASM GLOBALNIE. `--mask-slope <dB>` to globalna (mid ORAZ side) regulacja
Masking-Slope-Adaptation FDK — heurystyki NIE-maskującej (`adj_thr.cpp`), która rozluźnia
wymagany SNR dla pasm skali o energii dużo poniżej średniej ramki (stock: ponad około 10 dB
poniżej). Mówiąc wprost, FDK celowo głodzi bardzo ciche pasma, żeby oszczędzić bity, a to
pokrętło przesuwa próg „jak daleko poniżej średniej, zanim przestanę się przejmować". Wartość
DODATNIA go podnosi, więc mniej cichych pasm jest głodzonych — więcej detalu w cichych
fragmentach, ogonach pogłosu i wybrzmieniach, kosztem bitów. Wartość UJEMNA go obniża, więc ciche
pasma są głodzone mocniej — chudziej i bardziej pusto, za to więcej bitów zostaje na głośny
materiał. To ta sama rodzina co `--side-bias`, ale stosowana do obu kanałów i zakotwiczona na
energii-vs-średnia zamiast progu MS. Subtelny na gęstym materiale (rusza tylko najcichsze pasma)
i najbardziej słyszalny na rzadkiej lub pogłosowej treści. Rozsądny zakres **±6 do ±12**.
`0` = off.

`--ms-precision <n>` — BARDZO EKSPERYMENTALNY i dziś prawie na pewno chcesz zamiast tego
`--side-bias`. Skaluje precyzję pasm MS globalnie (mid i side razem), w skali Q8, w stylu LAME
`-q`: 256 = bez zmian, 384 ≈ 1,5×, 512 ≈ 2×. W praktyce jego zasięg jest ograniczony — powyżej
mniej więcej 600–800 próg uderza w twardą podłogę FDK, a przy CBR bity są jedynie przetasowywane
między pasmami, więc brzmienie przestaje się zmieniać. Do strojenia stereo został zastąpiony
przez `--side-bias`/`--side-knee`, które działają per-kanał dokładnie tam, gdzie trzeba.
Zostawiony dla kompletności.

`--mid-bias <n>` — również BARDZO EKSPERYMENTALNY i raczej niepotrzebny. Powyżej 256 PODNOSI próg
kanału mid (L+R) po motylku MS, celowo uwalniając bity dla kanału side. Czystszym, lepiej
zmierzonym sposobem przesunięcia balansu mid↔side jest `--side-bias`, który sięga po ten sam
budżet od strony side. 256 = off.

INTENSITY STEREO (IS). Bardziej agresywna technika: dla wysokich pasm, gdzie ucho
słabo lokalizuje kierunek, enkoder wysyła jedną obwiednię energii zamiast dwóch
kanałów. Oszczędza dużo, ale może zwęzić scenę.

`--is <0|1>` — globalnie włącz/wyłącz IS.

`--is-aggression <0..100>` — KONSUMENCKI suwak agresywności IS. To jest to,
czego zwykle chcesz używać. 0 = zachowanie domyślne FDK (bardzo ostrożne),
100 = maksymalnie agresywne intensity. Dlaczego to potrzebne: FDK ma trzy
niezależne "bramki" wpuszczające IS (stosunek bitrate do pasma, próg korelacji
kanałów, minimalna liczba ciągłych pasm) i muszą być spełnione JEDNOCZEŚNIE.
Ruszenie jednego progu nic nie da, bo blokuje inny. Ten suwak rusza wszystkie
naraz. Zacznij od 40–70 i słuchaj.

Zaawansowane progi IS (dla eksperymentujących — działają PO przejściu przez
suwak agresywności): `--is-min-sfbs <n>` (minimalna liczba ciągłych pasm),
`--is-corr-thresh <n>` (próg korelacji kanałów, Q8), `--is-lr-ratio <n>` (próg
stosunku L/R, Q8). Uwaga praktyczna: te progi działają NIEINTUICYJNIE — niższa
wartość progu korelacji oznacza AGRESYWNIEJSZE IS, nie odwrotnie.

Umiejscowienie IS w pasmach. `--is-lo <sfb>` i `--is-hi <sfb>` pozwalają OGRANICZYĆ
intensity stereo do zakresu pasm: IS jest dozwolone dopiero od `--is-lo` w górę
i tylko do `--is-hi` włącznie. To nigdy nie wymusza IS — jedynie ogranicza, gdzie
FDK może go użyć. W praktyce przy niskim bitrate IS ląduje na NISKICH pasmach, więc
skanuj małe wartości, żeby w ogóle zobaczyć efekt. Jeśli chcesz pójść dalej i
WYMUSIĆ IS, `--is-force-lo <sfb>` i `--is-force-hi <sfb>` wpychają intensity stereo
na cały zakres z pominięciem bramek korelacji, min-sfbs i głośności. To tryb
laboratoryjny: ponieważ IS jest stratne i kierunkowe (prawy kanał zostaje
wyzerowany, zostaje tylko współczynnik panoramy), wymuszenie może celowo rozbić
obraz stereo. Strumień i tak pozostaje zgodny ze standardem.

## 14. Pasmo i cutoff (w tym audiofilskie pełne pasmo)

`--core-cutoff <Hz>` — górna granica pasma kodowanego przez rdzeń, w Hz. Działa
także pod SBR (w przeciwieństwie do `-w`). Np. `--core-cutoff 7500` pod HE-AAC v2
oddaje rdzeniowi 7,5 kHz, resztę robi SBR.

Uwaga o realnym cutoffie w `--verbose`: widmo AAC jest podzielone na granice SFB,
więc enkoder nie potrafi uciąć pasma w dowolnym miejscu w Hz — zakotwicza cutoff
na najbliższej granicy SFB. Dlatego `--verbose` pokazuje REALNY cutoff, który może
różnić się od podanej wartości `-w`/`--core-cutoff` (np. `-w 17300` wyświetla się
jako `17915 Hz (SFB-anchored)`). To nie jest błąd — to prawdziwe pasmo, które
trafia do strumienia.

`--uncap-bandwidth` — audiofilskie. FDK ma zaszyty twardy limit pasma rdzenia:
minimum z 20 kHz i połowy częstotliwości próbkowania. Oznacza to, że NAWET przy
źródle 96 kHz i wysokim bitrate realnie nic powyżej 20 kHz nie było kodowane.
Ta flaga zdejmuje limit — `--core-cutoff` może wtedy sięgnąć aż do połowy
częstotliwości próbkowania. Dla materiału 96 kHz i słuchaczy, którzy chcą pełne
pasmo:

    fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

Zmierzono: bez tej flagi powyżej 20 kHz jest praktycznie zero energii; z nią
realnie kodowane jest pasmo do 40 kHz.

## 15. SBR — sterowanie górnym pasmem

Nadpisuje ustawienia z wewnętrznej tabeli tuningowej SBR.

`--sbr-start <n>` / `--sbr-stop <n>` — indeksy początku i końca pasma SBR.
UWAGA: FDK waliduje kombinację — zła para da "encoder initialization failed".
Dobieraj sprawdzone pary (np. dla 64k stereo działa start=5 stop=9).

`--sbr-freqscale <n>` — grupowanie częstotliwości (0 liniowe, wyżej = drobniejsze
logarytmiczne). `--sbr-noise-bands <n>` — gęstość opisu szumu SBR.
`--sbr-amp-res <0|1>` — rozdzielczość amplitudy obwiedni (0 = 1,5 dB, 1 = 3 dB).

`--sbr-num-env <1|2|4>` — liczba obwiedni na ramkę = rozdzielczość czasowa SBR.
Więcej obwiedni = lepiej oddane zmiany w czasie w górnym paśmie, kosztem bitów.
WAŻNE: ta opcja WYMUSZA statyczną siatkę czasową (wyłącza detektor transientów),
więc na materiale z ostrymi atakami może być gorzej. Dobre do stabilnych,
wybrzmiewających dźwięków. (Wartość 8 przekracza standardową siatkę i jest
odrzucana.)

`--sbr-freqres-fixfix <0|1>` — rozdzielczość częstotliwości obwiedni.

`--sbr-stereo-mode <0..3>` — tryb stereo w warstwie SBR (osobny od MS rdzenia!):
0 = mono, 1 = LR (pełna, niezależna separacja lewego i prawego w górnym paśmie),
2 = coupling (oszczędny: wspólna obwiednia + poziom/balans), 3 = switch (domyślnie
enkoder wybiera per ramkę). Wymuś 1 dla maksymalnej separacji stereo w górze
(audiofilsko), 2 dla oszczędności bitów przy niskim bitrate.

`--sbr-invf <0..3>` — filtrowanie odwrotne (inverse filtering). To mechanizm
sterowany przez ESTYMATOR TONALNOŚCI: SBR analizuje, czy górne pasmo powinno być
bardziej tonalne czy bardziej szumowe, i odpowiednio "wybiela" skopiowany
materiał. 0 = wyłączone, 1 = słabe, 2 = średnie, 3 = mocne. Mocniejsze =
mniej "metalicznego" dzwonienia w górze, kosztem części detalu. Zwykle steruje
tym automat; wymuś ręcznie, gdy słyszysz metaliczność.

`--sbr-noise-floor-offset <n>` — offset poziomu szumu wstrzykiwanego przez SBR.
Większe wartości = więcej szumu wypełniającego rekonstrukcję.

`--sbr-header-period <n>` — co ile ramek zapisywane są nagłówki SBR, co decyduje,
jak szybko górne pasmo SBR "wchodzi", gdy dekoder podłącza się do strumienia
HE-AAC NA ŻYWO (Icecast/Shoutcast). Haczyk jest taki: cała konfiguracja SBR
mieszka w okresowym nagłówku, a nie w każdej ramce. Słuchacz, który wpina się w
środek transmisji, słyszy najpierw sam rdzeń AAC (przytłumiony, bez góry), aż
nadejdzie kolejny nagłówek. Ustaw `1`, a nagłówek zapisze się w każdej ramce, więc
dekoder złapie SBR niemal natychmiast (~23 ms) — to właściwy wybór dla strumienia,
do którego ludzie dołączają w losowych momentach. Większe wartości wydłużają ten
moment "sam rdzeń". Domyślnie FDK daje około 10 ramek (~0,23 s przy HE dual-rate,
~0,46 s przy LC), a FDK kapuje okres do najwyżej raz na sekundę, więc bardzo duże
wartości są przycinane (np. 40 staje się 21 ramkami przy 44,1 kHz). `--verbose`
wypisuje efektywny okres w milisekundach.

## 16. Parametric Stereo (HE-AAC v2)

`--ps <0|1>` — wymuś wysyłanie parametru IID (różnica głośności): 0 = nigdy
(spłaszcza obraz stereo do mono-podobnego), 1 = zawsze.

`--ps-iid-quant <0|1>` — dokładność kwantyzacji IID: 0 gruba, 1 dokładna.

`--ps-icc <0|1>` — wymuś ICC (Inter-channel Coherence — spójność/podobieństwo
kanałów) on/off. `--ps-icc-mode <0|1>` — tryb rotacji ICC (ROT_A / ROT_B).

UCZCIWIE o IPD/OPD (różnice fazy): enkoder FDK ich NIE liczy. W kodzie jest to
jawnie oznaczone jako "nie wspierane" — pola fazy są zawsze zerowane. Wystawienie
ich wymagałoby napisania od zera analizy fazy międzykanałowej, co jest dużym,
ryzykownym zadaniem. Dlatego tych parametrów tu nie ma i nie da się ich włączyć.

## 17. TNS, PNS i afterburner

`--tns-mask <n>` / `--tns-order <n>` — TNS (Temporal Noise Shaping) kształtuje
szum kwantyzacji w czasie, żeby chował się pod transjentami (ważne dla ostrych
ataków, np. perkusji). Maska to bitowa mapa aktywnych filtrów, rząd to długość
filtra.

`--pns <0|1>` — PNS (Perceptual Noise Substitution) zastępuje szumowe pasma
opisem "tu jest szum o takiej energii" zamiast kodować go dokładnie. Duża
oszczędność na szumowym materiale.

`--pns-start <Hz>` — od jakiej częstotliwości PNS może działać.

`--force-pns` — obejdź bramkę niskiego bitrate dla PNS. WYJAŚNIENIE: FDK ma
tabelę, która przy bardzo niskich bitrate'ach (poniżej około 28 kbps) całkowicie
wyłącza PNS — i wtedy `--pns-start` jest ignorowane, bo PNS w ogóle nie działa.
To dlatego przy 24 kbps słyszałeś artefakty jak z MP3, a przy 64 kbps PNS
działał. Ta flaga omija tabelę i włącza PNS mimo niskiego bitrate.

Słowo o jednostkach, zanim przejdziemy do poszczególnych pokręteł, bo dzięki temu reszta
nabiera sensu. FDK trzyma te parametry detekcji PNS wewnętrznie jako liczby stałoprzecinkowe,
a nasze przełączniki przyjmują zwykły dziesiętny MNOŻNIK: to, co wpiszesz, jest mnożone przez
100 i użyte do przeskalowania fabrycznej wartości FDK. Czyli `1.0` znaczy „×1,0 = zostaw
fabryczną wartość", `1.5` znaczy „×1,5", `0.5` znaczy „×0,5", a `-1` znaczy, że przełącznik jest
wyłączony. Ta jedna konwencja (`wartość × 100`, `1.0` = bez zmian) dotyczy każdego skalującego
pokrętła `--pns-*` poniżej.

`--pns-gain <x>` — GŁOŚNOŚĆ dorabianego szumu PNS i pokrętło PNS, po które sięgniesz najczęściej.
Gdy PNS uzna pasmo za „sam szum", wyrzuca faktyczne linie widmowe i zapisuje tylko jedną liczbę:
energię szumu tego pasma. Przy odtwarzaniu dekoder odtwarza losowy szum wyskalowany do tej
energii. `--pns-gain` skaluje tę zapisaną energię wprost. `1.0` = bez zmian (odtworzony szum
niesie energię oryginalnego pasma); `>1.0` czyni szum głośniejszym niż oryginał (przydatne, gdy
podstawiony szum brzmi zbyt nieśmiało i miks robi się głuchy); `<1.0` czyni go cichszym (gdy szum
za mocno syczy). Traktuj to jak pokrętło „jak głośny jest podstawiony szum". Wejście dziesiętne,
`-1` = off.

Pozostałe pokrętła PNS rządzą tym, KTÓRE pasma w ogóle zostaną zamienione w szum — etapem
detekcji — a nie tym, jak głośny jest wynik. `--pns-tonality <x>` skaluje próg detekcji
tonalności. PNS odpala się tylko na pasmach, które enkoder uzna za „szumopodobne" (niska
tonalność); podniesienie tego progu pozwala większej liczbie pasm — nawet nieco tonalnym —
zakwalifikować się jako szum, więc podstawianie szumu robi się SZERSZE (więcej widma zastąpione
tanim szumem, większa oszczędność, ale i większe ryzyko rozmycia prawdziwych tonów). Obniżenie
czyni PNS bardziej wybrednym. `1.0` = domyślnie.

`--pns-refpower <x>` skaluje próg mocy referencyjnej detekcji. PNS porównuje moc każdego pasma
z referencją, zanim uzna je za kandydata na szum; to pokrętło przesuwa tę bramkę mocy.
`1.0` = domyślnie; działa w parze z tonalnością — oba warunki muszą się zgodzić, żeby pasmo
stało się PNS.

`--pns-gapfill <x>` skaluje próg wypełniania luk. Gdy PNS oznaczył pasma po obu stronach małej
luki, ta heurystyka może „domknąć lukę" i oznaczyć jako PNS także pasmo pomiędzy nimi, żeby nie
zostawić zakodowanej wyspy uwięzionej między dwoma pasmami szumu. Zaawansowane i subtelne —
rzadko słyszalne samo w sobie. `1.0` = domyślnie.

`--pns-min-width <n>` ustala minimalną szerokość SFB, w liniach widmowych, jaką musi mieć pasmo,
zanim PNS będzie mógł na nim zadziałać. To jedyny z tej grupy, który jest surową liczbą linii,
a nie mnożnikiem `×100`. Jest skuteczny dopiero powyżej wbudowanej domyślnej (LC = 16 linii);
podbicie do 32 albo 64 ogranicza PNS tylko do szerszych pasm, powstrzymując enkoder przed
podstawianiem szumu na wąskich niskich pasmach, gdzie byłoby to bardziej słyszalne.
`-1` = off (użyj domyślnej FDK).

`--ath-scale <n>` — skalowanie progu słyszalności (ATH), w Q8 (256 = ×1,0).
To jest GLOBALNY regulator maskowania. Powyżej 256 = podnosi progi = enkoder
uznaje więcej rzeczy za niesłyszalne = agresywniej, mniej bitów. Poniżej 256 =
obniża progi = enkoder zachowuje więcej detalu = więcej bitów. To najprostszy
sposób powiedzieć enkoderowi "bądź czystszy" albo "bądź oszczędniejszy".

---
# Część V. Detale w wysokich bitrate i dostrajanie do mowy

## 18. "Więcej bitów = więcej detalu": jak to naprawdę wycisnąć

Pytanie, które często pada: skoro daję 400 kbps, czy enkoder naprawdę wykorzysta
je na lepszy opis detali, czy część się marnuje? W AAC nie ma tak zaawansowanych,
osobnych regulatorów jak w MusePack (signal-to-mask ratio, tone-masks-noise,
noise-masks-tone jako oddzielne pokrętła). Ale efekt "opisz dokładniej, mniej
maskuj" osiągamy dwoma narzędziami, które razem robią dokładnie to samo.

`--ath-scale <n>` PONIŻEJ 256 — obniża globalny próg słyszalności. Enkoder
przestaje zakładać, że coś jest niesłyszalne, i zaczyna to dokładnie kodować.
To najbliższy odpowiednik obniżenia progu maskowania z MusePacka. Przy 320–400
kbps spróbuj 200 albo 180.

`--spread-mask <n>` PONIŻEJ 256 — steruje ROZLEWANIEM maskowania między
sąsiednimi pasmami. W psychoakustyce głośne pasmo "rozlewa" swoją zdolność
maskowania na sąsiadów (to właśnie tone-masks-noise). Zmniejszając ten parametr,
mówisz enkoderowi: mniej zakładaj, że sąsiednie pasma się nawzajem maskują —
przez co więcej pasm jest traktowanych jako słyszalne i dokładnie kodowanych.
Największy efekt tam, gdzie bity są ograniczeniem (96–192 kbps); przy bardzo
wysokim bitrate enkoder i tak ma nadmiar bitów, więc zmiana bywa mała.

PRZEPIS audiofilski (maksimum detalu, wysoki bitrate, materiał z bogatą górą):

    fdkaac-franken-x64.exe -p 2 -b 400000 --ath-scale 190 --spread-mask 128 -o out.m4a in.wav

Połączenie: obniżony globalny próg (ath-scale 190) + zmniejszone rozlewanie
maskowania (spread-mask 128). Enkoder zachowuje znacznie więcej mikrodetalu,
co w porównaniu fazowym z oryginałem daje mniejsze różnice. To jest w duchu tego,
co robił MusePack przy 1000 kbps — w granicach tego, co AAC pozwala wystawić bez
przepisywania całego modelu.

Dla materiału, gdzie chcesz więcej budżetu w szerokości stereo, dołóż `--side-bias 6` —
skierowanie bitów do kanału side to kolejne miejsce, gdzie wysoki bitrate daje realnie lepszy
dźwięk. (Stare pokrętło `--ms-precision` robiło coś podobnego globalnie, ale jest bardzo
eksperymentalne i w dużej mierze zastąpione; `--side-bias` to teraz właściwe narzędzie.)

Poza ath-scale jest jeszcze bardziej bezpośrednia dźwignia i warto zrozumieć jej jednostki. Te
pokrętła min-SNR działają w skali stałoprzecinkowej Q8, gdzie 256 oznacza 1,0 (więc 256 =
„zostaw w spokoju", 128 = ×0,5, 512 = ×2,0); `-1` oznacza, że przełącznik jest wyłączony.
`--minsnr-scale <n>` (Q8, 256 = off) to najbliższy FDK-owy odpowiednik gałek TMN/NMT z
MusePacka: skaluje WYMAGANY per-pasmo SNR kodowania (`sfbMinSnrLdData`), czyli minimalny stosunek
sygnału do szumu, jaki enkoder upiera się osiągnąć w każdym paśmie skali. Wartości PONIŻEJ 256
wymagają WYŻSZEGO SNR — enkoder musi kodować każde pasmo dokładniej, więc więcej detalu i więcej
bitów — a powyżej 256 rozluźniają wymóg i kodują zgrubniej. Jest skuteczniejsza niż `--ath-scale`,
bo to właśnie do min-SNR logika „avoid holes" (unikania dziur) cofa progi; samo ruszenie kopii
progu (jak robi ath-scale) jest częściowo cofane dalej w łańcuchu, ale podłoga min-SNR już nie.

Dwa pokrętła clampu przesuwają twarde granice, jakie FDK narzuca wokół tego wymaganego SNR.
Wewnętrznie FDK nigdy nie pozwala pasmu żądać więcej niż sufit MAX_SNR (około −1 dB) ani mniej
niż podłoga MIN_SNR (około −25 dB), więc nawet agresywny `--minsnr-scale` jest przez nie
ograniczony. `--minsnr-clamp-hi <n>` (Q8, 256 = off) skaluje sufit MAX_SNR, pozwalając
wymagającym pasmom prosić o WIĘCEJ niż fabryczny cap — to podnosisz, gdy sam `--minsnr-scale`
zdaje się „kończyć miejsce" u góry. `--minsnr-clamp-lo <n>` (Q8, 256 = off) skaluje podłogę
MIN_SNR na drugim końcu. Razem poszerzają fabryczne okno FDK, żeby `--minsnr-scale` miał dokąd
jeszcze pchać.

Na koniec `--reduce-clamp 0` zdejmuje sufit „29 dB Ratio" redukcji progów wewnątrz kwantyzatora
CBR. Gdy pętli alokacji bitów CBR brakuje bitów, podnosi (rozluźnia) progi, ale stock FDK nie
pozwala zredukować progu o więcej niż około 29 dB względem referencji — to clamp bezpieczeństwa.
Ustawienie `--reduce-clamp 0` zdejmuje ten clamp, pozwalając enkoderowi wepchnąć progi głębiej
i wlać bity w najbardziej wymagające pasma. Łączy się naturalnie z `--minsnr-scale` dla
ekstremalnego detalu i dotyczy tylko CBR (VBR chodzi inną ścieżką). Domyślnie jest `1` (clamp
włączony, zachowanie stockowe).

## 19. Dostrajanie do mowy ludzkiej

Tak, w HE-AAC (a konkretnie w warstwie SBR) istnieje osobny tryb strojenia pod
mowę. W zwykłym FDK jest zaszyty na sztywno jako wyłączony — tutaj wystawiliśmy
go flagą:

`--speech` — włącza strojenie SBR pod mowę ludzką. Zmienia progi filtrowania
odwrotnego, poziom szumu i wyłącza kodowanie parametryczne górnego pasma —
wszystko po to, żeby mowa (podcast, audiobook, dialog) w niskim bitrate brzmiała
naturalniej, bez "bulgotania" w górze. Dotyczy HE-AAC/HE-AAC v2 (bo działa w
SBR).

    fdkaac-franken-x64.exe -p 5 -b 32000 --speech -o mowa.m4a podcast.wav

Uczciwie o LC: czysty AAC-LC (bez SBR) NIE ma osobnego, oddzielnego "trybu mowy"
w FDK. Model psychoakustyczny LC jest ten sam dla mowy i muzyki. Dla mowy w LC
najlepiej po prostu zejść z pasmem (`--core-cutoff` np. 12000–14000 Hz, bo mowa
nie ma dużo powyżej) i ewentualnie włączyć `--pns` z `--force-pns` przy bardzo
niskich bitrate'ach. Ale dedykowanego przełącznika "speech" dla LC nie ma, bo
nie ma go w samym kodeku — i nie chcę udawać, że jest.

---

# Część VI. Gotowe przepisy (od skrajności do skrajności)

## 20. Zestaw praktycznych receptur

MUZYKA, WYSOKA JAKOŚĆ ARCHIWALNA (przezroczyste stereo):

    fdkaac-franken-x64.exe -p 2 -b 256000 --afterburner 1 -o out.m4a in.wav

MUZYKA, "ODDYCHAJĄCY" QUASI-VBR (nie tnie po kulminacjach):

    fdkaac-franken-x64.exe -p 2 -b 160000 --peak-bitrate 256000 --vbr-reservoir 16000 -o out.m4a in.wav

AUDIOFIL, PEŁNE PASMO 96 kHz + maksimum detalu:

    fdkaac-franken-x64.exe -p 2 -b 400000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 190 --spread-mask 128 -o out.m4a in96k.wav

STREAMING MUZYKI, ŚREDNI BITRATE:

    fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-stereo-mode 3 -o out.m4a in.wav

PODCAST / MOWA, NISKI BITRATE:

    fdkaac-franken-x64.exe -p 5 -b 32000 --speech --core-cutoff 12000 -o out.m4a mowa.wav

SKRAJNIE NISKO (eksperyment, 8 kbps stereo):

    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

RATOWANIE STEREO PRZY NISKIM BITRACIE (agresywne intensity):

    fdkaac-franken-x64.exe -p 2 -b 96000 --is 1 --is-aggression 70 -o out.m4a in.wav

MS TYLKO NA GÓRZE (dół pełne stereo, góra scalona):

    fdkaac-franken-x64.exe -p 2 -b 128000 --msbands-lo 44 --msbands-hi 48 -o out.m4a in.wav

## 21. Jak podejść do własnych eksperymentów

Zmieniaj JEDEN parametr naraz i słuchaj (albo porównuj widmo/fazę z oryginałem).
Enkoder bez żadnych flag Frankenstein zachowuje się dokładnie jak oryginalny FDK
— to Twój punkt odniesienia. Każda flaga to świadome odchylenie od domyślnego,
sprawdzonego zachowania.

Kolejność, w jakiej warto próbować przy problemach:
- "za mało detalu / za płasko" → `--ath-scale` w dół, potem `--spread-mask` w dół.
- "stereo za wąskie w górze" → `--sbr-stereo-mode 1` (HE-AAC) albo mniej agresywne IS.
- "metaliczne wysokie" → `--sbr-invf 2` lub 3.
- "tnie po głośnych fragmentach" → `--peak-bitrate` + `--vbr-reservoir` (rozdział 8).
- "artefakty jak MP3 przy bardzo niskim bitrate" → `--force-pns`.
- "chcę pełne pasmo z 96 kHz" → `--uncap-bandwidth --core-cutoff 40000`.

## 22. Uwaga końcowa o rzetelności

Wszystkie opisane tu zachowania zostały sprawdzone pomiarowo (dekodowalność
strumienia, realny rozrzut bitrate, zawartość widma), a nie tylko założone.
Tam, gdzie coś ma ograniczenie (IPD/OPD nieobsługiwane, brak trybu mowy w LC,
sufit 6144 bity na kanał, rezydualna podłoga bitrate ~10 kbps), jest to
powiedziane wprost, zamiast obiecywać rzeczy, których kodek nie potrafi.

Enkoder bez flag = czysty FDK. Flagi = Twoje świadome decyzje. Miłego strojenia.


# Część VII. Tabele referencyjne

Trzy tabele orientacyjne wyliczone z tablic strojenia w kodzie FDK. Są
PRZYBLIŻONE (siatka pasm jest schodkowa), ale pokazują właściwy rząd wielkości i
pomagają świadomie dobrać ustawienia bez zgadywania.

## 23. Tabela 1 — górna częstotliwość pasma (SFB)

Widmo jest podzielone na pasma (scale factor bands, SFB), numerowane od dołu:
pasmo 0 to najniższy bas, im wyżej numer, tym wyżej w częstotliwości. Kiedy
ograniczasz coś do "N pasm" (`--msbands`, `--isbands`), ta tabela mówi, jakiej
częstotliwości to mniej więcej odpowiada. Pokazano co czwarte pasmo; ostatni
wiersz to łączna liczba pasm i częstotliwość Nyquista (połowa próbkowania).

| SFB | 16 kHz | 22,05 kHz | 32 kHz | 44,1 kHz | 48 kHz | 96 kHz |
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
| liczba pasm / Nyquist | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

Przykład odczytu: przy 44,1 kHz pasmo nr 40 kończy się około 15,2 kHz. Jeśli
chcesz, żeby MS działało tylko poniżej ~15 kHz, ustaw `--msbands 40`.

## 24. Tabela 2 — SBR: indeks startu a częstotliwość przejścia

W HE-AAC dolną część pasma koduje rdzeń AAC, a górną dorabia SBR. `--sbr-start`
(indeks 0..15) decyduje, od jakiej częstotliwości zaczyna się SBR — niższy indeks
oznacza, że SBR wchodzi niżej, więc rdzeń jest węższy. "core" to częstotliwość
próbkowania rdzenia; przy trybie dual-rate wyjście jest dwa razy wyższe niż rdzeń.

| indeks startu | core 16 kHz | core 24 kHz | core 32 kHz | core 44,1 kHz | core 48 kHz |
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

Górną granicę SBR ustawia `--sbr-stop` (0..13) — wyższy indeks to SBR sięgające
wyżej. Domyślnie biblioteka dobiera oba indeksy pod bitrate; te wartości podawaj
tylko, gdy świadomie chcesz przesunąć punkt przejścia.

## 25. Tabela 3 — AAC-LC: automatyczne odcięcie pasma wg bitrate

Gdy nie podasz `-w`, koder sam dobiera szerokość pasma z tej tablicy — według
bitrate NA KANAŁ (stereo 128 kbps to 64 kbps na kanał). Tabela jest wspólna dla
32/44,1/48 kHz i wyższych; próbkowanie wpływa tylko na górny limit (Nyquista).

| bitrate na kanał | pasmo mono | pasmo stereo i więcej |
|----:|----:|----:|
| do 12 kbps | 3700  | 5000  |
| 20 kbps    | 6900  | 9640  |
| 28 kbps    | 9600  | 13050 |
| 40 kbps    | 12060 | 14260 |
| 56 kbps    | 13950 | 15500 |
| 72 kbps    | 14200 | 16120 |
| 96 kbps i więcej | 17000 | 17000 |

Wniosek praktyczny: w czystym AAC-LC (bez SBR) automat i tak zatrzyma się około
17 kHz. Podawanie `-w` wyżej ma sens tylko przy dużym zapasie bitrate; pełne pasmo
powyżej 20 kHz wymaga `--uncap-bandwidth` i próbkowania co najmniej 96 kHz.

## 26. Jak czytać `--verbose`

`--verbose` wypisuje surowe wartości bez podpowiedzi w nawiasach, żeby odczyt był
czysty. Oto co oznaczają te mniej oczywiste:

- AOT (profil): 2 = AAC-LC, 5 = HE-AAC, 29 = HE-AAC v2, 23 = AAC-LD, 39 = AAC-ELD.
- bitrate-mode: 0 = CBR, 1 do 5 = VBR (wyżej = lepsza jakość).
- channel-mode: 1 = mono, 2 = stereo. W HE-AAC v2 rdzeń jest mono, a stereo
  odtwarza się z parametrów (Parametric Stereo).
- core bandwidth: górna częstotliwość rdzenia AAC. W nawiasie podane jest ŹRÓDŁO:
  "from -w" (podałeś ręcznie), "from --core-cutoff" albo "auto". Pod SBR to tylko
  rdzeń — SBR gra powyżej tej wartości.
- final BW (AAC+SBR): pokazywane tylko przy aktywnym SBR — orientacyjna górna
  częstotliwość CAŁEGO sygnału (rdzeń plus SBR), wyliczona z indeksu stopu SBR.
  To dopełnienie "core bandwidth": tamto mówi dokąd sięga sam rdzeń, a to dokąd
  sięga cały HE-AAC po dołożeniu SBR.
- sbr-ratio: 1 = single-rate (downsampled), 2 = dual-rate (rdzeń na połowie
  częstotliwości wyjścia).
- sbr amp res: 0 = rozdzielczość 1,5 dB, 1 = 3,0 dB.
- codec delay: opóźnienie kodeka w próbkach na kanał — istotne przy gapless.
- IS corr threshold, IS L/R ratio: progi w skali Q8, gdzie 256 = 1,0. Niższy próg
  korelacji oznacza, że intensity stereo włącza się CHĘTNIEJ (odwrotnie do
  intuicji).
- franken overrides applied: lista przełączników, które w tym uruchomieniu
  odbiegają od czystego FDK (albo "none", gdy nic nie zmieniałeś).

Wartości w skali Q8 (jak progi IS, `--ms-precision`, `--ms-bias`, `--ath-scale`,
`--spread-mask`) to liczby, gdzie 256 znaczy 1,0 — na przykład 243 to około 0,95.


# Część VIII. Słownik pojęć

Dla szybkiego przypomnienia — wszystkie terminy z tego manuala, wyjaśnione
jednym–dwoma zdaniami, językiem dźwiękowca.

AAC (Advanced Audio Coding). Rodzina stratnych kodeków dźwięku z rodziny MPEG,
następca MP3. Lepsza jakość na bit niż MP3, zwłaszcza w niskich bitrate'ach.

AAC-LC (Low Complexity). Podstawowy profil AAC. Koduje pełne pasmo klasycznie.
Wybór do wysokich bitrate'ów i archiwizacji.

Afterburner. Dodatkowy, wolniejszy algorytm optymalizujący kwantyzację w FDK.
Włączony daje nieco lepszą jakość. Domyślnie i zalecanie włączony.

ATH (Absolute Threshold of Hearing). Próg słyszalności — granica ciszy zależna od
częstotliwości, poniżej której ucho nie rejestruje dźwięku. Enkoder wyrzuca to,
co jest poniżej. W tym enkoderze regulowany przez `--ath-scale`.

Bitrate. Ilość danych na sekundę dźwięku, w kilobitach (kbps) lub bitach (bps).
Wyższy = więcej miejsca na detale = lepsza jakość (do pewnej granicy).

Bit reservoir (rezerwuar bitów). Bufor pozwalający pożyczać bity między ramkami.
Dzięki niemu nawet CBR lokalnie "oddycha". Podstawa naszego quasi-VBR.

CBR (Constant Bitrate). Stały bitrate. Przewidywalny rozmiar, wygodny do
streamingu. W AAC i tak lekko zmienny dzięki rezerwuarowi.

Coupling (SBR). Tryb stereo w warstwie SBR, w którym oba kanały dzielą wspólną
obwiednię plus opis poziomu — oszczędny, ale węższa scena w górnym paśmie.

Cutoff. Górna granica pasma kodowanego przez rdzeń. Powyżej niej albo cisza,
albo (w HE-AAC) robotę przejmuje SBR.

HE-AAC (High Efficiency). AAC-LC + SBR. Rdzeń koduje dół pasma, SBR odtwarza
górę. Do średnich i niskich bitrate'ów.

HE-AAC v2. HE-AAC + Parametric Stereo. Do najniższych bitrate'ów stereo.

ICC (Inter-channel Coherence). Parametr PS opisujący, jak bardzo kanały są do
siebie podobne/spójne. Sterowany przez `--ps-icc`.

IID (Inter-channel Intensity Difference). Parametr PS opisujący różnicę głośności
między kanałami. Podstawowy nośnik obrazu stereo w PS.

Intensity Stereo (IS). Technika, w której wysokie pasma kodowane są jako jedna
obwiednia energii zamiast dwóch kanałów. Oszczędna, ale może zwęzić scenę.
Sterowana suwakiem `--is-aggression`.

Inverse filtering (filtrowanie odwrotne). Mechanizm SBR "wybielający" skopiowany
materiał górnego pasma, sterowany estymatorem tonalności. Mocniejsze = mniej
metaliczności. Regulowane `--sbr-invf`.

IPD/OPD (różnice fazy w PS). W enkoderze FDK NIE obsługiwane (zawsze zero).

Kwantyzacja. Zaokrąglanie współczynników widma do skończonej dokładności. Im
mniej bitów, tym większy błąd (szum/dziury). Serce stratnej kompresji.

Maskowanie. Zjawisko, w którym głośny dźwięk sprawia, że nie słyszysz cichszych
tuż obok (w widmie lub w czasie). Główne źródło oszczędności bitów w AAC.

MDCT. Transformata przenosząca ramkę dźwięku z czasu do dziedziny częstotliwości.
Na jej wyniku pracuje cały model psychoakustyczny.

MS Stereo (Mid/Side). Kodowanie sumy (mid) i różnicy (side) zamiast L i R.
Prawie darmowa oszczędność, gdy kanały są podobne.

PNS (Perceptual Noise Substitution). Zastępuje szumowe pasma opisem "tu jest
szum o takiej energii" zamiast kodować je dokładnie. Sterowany `--pns`,
`--force-pns`.

Parametric Stereo (PS). Koduje jeden kanał + parametry (IID, ICC), z których
dekoder odtwarza stereo. Do najniższych bitrate'ów.

Profil (AOT, Audio Object Type). Wariant AAC wybierany przez `-p` (LC, HE, v2,
LD, ELD).

Ramka. Porcja dźwięku (1024 próbki), którą enkoder przetwarza naraz.

SBR (Spectral Band Replication). Rekonstrukcja górnego pasma z dolnego plus
niewielki opis. Serce HE-AAC.

SFB (Scale Factor Band). Pasmo współczynników skali. Grupa sąsiednich
współczynników widma, dla której enkoder podejmuje wspólną decyzję o dokładności.
Numerowane od dołu (0 = basy).

Spreading (rozlewanie maskowania). Rozprzestrzenianie zdolności maskowania na
sąsiednie pasma. Zmniejszane przez `--spread-mask` dla większego detalu.

Tabela tuningowa. Zestaw domyślnych ustawień SBR/PS dobieranych przez FDK wg
bitrate i częstotliwości. Nasze flagi ją nadpisują.

TNS (Temporal Noise Shaping). Kształtuje szum kwantyzacji w czasie, żeby chował
się pod transjentami. Ważne dla perkusji i ostrych ataków.

Transjent. Ostry, krótki dźwięk (uderzenie, atak). Trudny dla kodeka, bo szum
kwantyzacji może "wyprzedzić" atak (pre-echo).

VBR (Variable Bitrate). Zmienny bitrate zależny od trudności materiału. W FDK
słaby; nasz quasi-VBR (rozdział 8) jest lepszą alternatywą.

---
Koniec słownika.
