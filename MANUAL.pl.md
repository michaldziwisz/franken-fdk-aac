# Franken FDK AAC: podręcznik dla realizatora dźwięku

Kompletny przewodnik po kodowaniu dźwięku w AAC / MPEG-4 oraz po wszystkich opcjach tego
kodera. Napisany dla osoby, która zna się na dźwięku i biegle posługuje się komputerem, ale
nie musi być programistą ani matematykiem.

Wersja dokumentu: lipiec 2026. Koder: libfdk-aac 2.0.3 + frontend nu774, z rozszerzeniami
"Frankenstein".

Autor: Michał Dziwisz. Konsultant merytoryczny: Patryk Faliszewski. Zbudowane na
oprogramowaniu open source: libfdk-aac (Fraunhofer IIS) oraz frontend nu774/fdkaac.

---

# Część I. Jak w ogóle działa AAC

## 1. Po co komu kodek stratny

Nagranie PCM (to, co jest na płycie CD albo w pliku WAV) zapisuje każdą próbkę dźwięku
dokładnie. Przy 44,1 kHz i 16 bitach na kanał stereo to około 1411 kilobitów na sekundę. To
dużo. Kodek stratny, taki jak AAC czy MP3, potrafi oddać ten sam materiał przy 128, 96, a
nawet 64 kilobitach tak, że ucho w codziennych warunkach różnicy nie usłyszy albo usłyszy
bardzo niewielką.

Sztuczka nie polega na "ściśnięciu" dźwięku jak plik ZIP. Polega na WYRZUCENIU tego, czego i
tak nie słychać, i zapisaniu tylko reszty. Cała inteligencja kodeka leży w tym, żeby dobrze
zgadnąć, co jest nieistotne. Służy do tego model psychoakustyczny, czyli matematyczny opis
tego, jak działa ludzki słuch.

## 2. Trzy filary psychoakustyki

Koder podejmuje decyzje na podstawie kilku zjawisk, które warto rozumieć, bo połowa opcji
tego programu właśnie nimi steruje.

MASKOWANIE JEDNOCZESNE. Głośny dźwięk o jakiejś częstotliwości sprawia, że cichsze dźwięki
tuż obok niego w widmie przestają być słyszalne. Taki głośny dźwięk to sygnał maskujący
(masker). Jeśli gra głośne 1000 Hz, to cichy sygnał 1100 Hz jest dźwiękiem maskowanym: ucho
go nie wychwyci. Koder może więc zapisać ten fragment bardzo niedbale (mało bitów) albo
pominąć go w całości. To najważniejszy mechanizm oszczędzania bitów w AAC.

MASKOWANIE CZASOWE. Tuż przed i tuż po głośnym uderzeniu (np. werbla) ucho jest "ogłuszone"
i nie słyszy cichych detali. Koder to wykorzystuje.

PRÓG SŁYSZALNOŚCI (ATH, Absolute Threshold of Hearing). Istnieje granica ciszy, poniżej
której ucho po prostu nie rejestruje dźwięku, a granica ta jest różna dla różnych
częstotliwości (najlepiej słyszymy 2-5 kHz, gorzej skraje pasma). Cokolwiek jest poniżej
tego progu, można wyrzucić.

Gdy słychać, że kodek "zabrał powietrze" albo "spłaszczył blachy", prawie zawsze znaczy to,
że model psychoakustyczny uznał coś za niesłyszalne, a ucho się z nim nie zgodziło. Duża
część tego manuala to nauka, jak przesuwać te decyzje w stronę własnego ucha.

## 3. Pasma, MDCT i współczynniki skali

AAC nie pracuje na próbkach w czasie, tylko na WIDMIE. Sygnał jest cięty na ramki (1024
próbki) i przekształcany do dziedziny częstotliwości (transformata MDCT). Powstałe
współczynniki grupowane są w PASMA WSPÓŁCZYNNIKÓW SKALI (scale factor bands, w skrócie SFB).
Numeracja SFB idzie OD DOŁU: SFB 0 to najniższe częstotliwości (basy), a im wyższy numer,
tym wyżej w widmie.

To ważne, bo wiele opcji tego kodera operuje właśnie na numerach pasm SFB. Gdy mówimy "MS na
5 najwyższych pasmach", chodzi o pasma o najwyższych numerach.

W każdym paśmie koder decyduje, jak dokładnie (ile bitów) zapisać współczynniki. Im mniej
bitów, tym większy błąd kwantyzacji, słyszalny jako szum albo "dziura" w widmie. Model
psychoakustyczny mówi, gdzie ten błąd się schowa pod maskowaniem, a gdzie będzie słyszalny.

## 4. Rodziny AAC: LC, HE-AAC, HE-AAC v2

AAC to nie jeden format, tylko rodzina profili. W tym koderze wybiera się je przełącznikiem
`-p`:

AAC-LC (Low Complexity, profil 2). Podstawowy, najczęściej używany wariant. Koduje pełne
pasmo tradycyjnie. Sensowny od około 96 kbps stereo w górę. Bezpieczny wybór do muzyki w
dobrej jakości.

HE-AAC (High Efficiency, profil 5, zwany też AAC+ albo aacPlus). Do LC dokłada SBR, czyli
Spectral Band Replication. Rdzeń AAC koduje tylko dolną część pasma (np. do 8-13 kHz), a
górę SBR "odtwarza sztucznie" na podstawie dolnej i niewielkiego opisu. Ogromna oszczędność
bitów przy zachowaniu wrażenia szerokiego pasma. Sensowny w okolicach 32-80 kbps stereo.

HE-AAC v2 (profil 29). Do HE-AAC dokłada PS, czyli Parametric Stereo. Zamiast kodować dwa
kanały, koduje jeden (mono) plus garść parametrów opisujących, jak z niego odtworzyć stereo.
Sensowny w bardzo niskich bitrate'ach, 16-48 kbps stereo.

Zasada praktyczna: im niższy bitrate, tym więcej "protez" (SBR, PS). Przy 320 kbps te
protezy tylko przeszkadzają, dlatego należy wtedy użyć LC.

## 5. Co to jest SBR (górne pasmo sztuczne)

Warto zrozumieć SBR, bo steruje nim wiele opcji. SBR NIE koduje górnego pasma próbka po
próbce. Zamiast tego wysyła OBWIEDNIE (envelopes), czyli informację "w tym kawałku czasu i w
tym zakresie częstotliwości energia ma taki a taki kształt", plus wskazówki, z których
fragmentów dolnego pasma "skopiować" materiał w górę i ile dołożyć szumu. Dekoder
rekonstruuje górę na tej podstawie.

Dlatego górne pasmo w HE-AAC brzmi inaczej niż oryginał: to inteligentna rekonstrukcja, nie
kopia. Opcje SBR w tym koderze pozwalają sterować gęstością obwiedni w czasie, ich
rozdzielczością częstotliwości, ilością wstrzykiwanego szumu i tak zwanym filtrowaniem
odwrotnym (o tym dalej).

## 6. Co to jest Parametric Stereo (PS)

PS idzie jeszcze dalej niż SBR w oszczędzaniu. Wysyła jeden kanał dźwięku i parametry
opisujące różnice między lewym a prawym:

- IID (Inter-channel Intensity Difference): różnica głośności między kanałami.
- ICC (Inter-channel Coherence): jak bardzo kanały są do siebie podobne i spójne.
- IPD/OPD (różnice fazy): fabryczny FDK ich nie wysyła; ten build potrafi, przez
  `--ps-ipd 1` i `--ps-opd 1` (rozdział 16).

Dekoder z tego jednego kanału i tych parametrów "rozkłada" dźwięk z powrotem na stereo. Przy
24 kbps to jedyny sposób, żeby stereo w ogóle miało sens. Przy wyższych bitrate'ach PS
zwykle przeszkadza i się go nie używa.

---
# Część II. Bitrate, rezerwuar bitów i sterowanie jakością

## 7. CBR, VBR i co się naprawdę dzieje w środku

CBR (Constant Bitrate) to stały bitrate: każda sekunda dźwięku zajmuje mniej więcej tyle
samo miejsca. Wygodne do streamingu, przewidywalne. Wybiera się go przełącznikiem `-b` (np.
`-b 128000` = 128 kbit/s).

VBR (Variable Bitrate) to bitrate zmienny: fragmenty trudne (dużo detali, transjenty)
dostają więcej bitów, ciche i proste mniej. Teoretycznie lepsza jakość na bit. W FDK VBR
wybiera się przełącznikiem `-m` (tryby 1-5), trzeba jednak powiedzieć wprost: VBR w FDK jest
słaby. Presety są zachowawcze i nie dają tej swobody, co np. LAME w MP3.

Kluczowa rzecz do zrozumienia: NAWET w trybie CBR bitrate lokalnie "oddycha". Służy do tego
REZERWUAR BITÓW (bit reservoir). To bufor: gdy ramka jest łatwa, koder zapisuje ją
oszczędnie i odkłada zaoszczędzone bity do rezerwuaru; gdy przychodzi ramka trudna, pożycza
z rezerwuaru i wydaje więcej, niż wynikałoby ze średniej. Średnia zostaje zachowana, ale
chwilowo bitrate skacze w górę i w dół. To dlatego "CBR" w AAC nie jest idealnie płaski.

Ten koder wystawia sterowanie rezerwuarem na zewnątrz i to jest podstawa naszego własnego,
lepszego VBR (patrz następny rozdział).

## 8. Przepis: VBR, który nie tnie ostatnich ramek "na siłę"

To jest dokładnie sytuacja, o którą często chodzi: w trudnym fragmencie (np. wejście całej
sekcji dętej albo talerze) koder wypuścił kilka ramek grubo powyżej średniej, a potem,
zamiast pozwolić tej jakości "żyć", zaczyna dusić kolejne ramki, żeby tylko wrócić do
średniej. Efekt: słyszalne przygaszenie zaraz po głośnym momencie.

Nasze rozwiązanie to quasi-CVBR: bierzemy silnik CBR (który trzyma sensowną średnią), ale
ROZSZERZAMY rezerwuar i widełki bitów, żeby koder mógł oddychać znacznie mocniej i nie
musiał natychmiast "oddawać długu". Cztery przełączniki:

`--vbr-reservoir <bity>`: rozmiar rezerwuaru. Większy oznacza, że koder może
dłużej utrzymać podwyższoną jakość po trudnym fragmencie, zanim wróci do średniej.

`--peak-bitrate <bps>`: dozwolony chwilowy szczyt. Pozwala trudnym ramkom
sięgnąć wysoko, przy zachowaniu średniej.

`--max-bits-frame <n>` i `--min-bits-frame <n>`: twarde widełki bitów na ramkę.
To rdzeń "constrained VBR", czyli reguła "nie mniej niż X, nie więcej niż Y na
ramkę".

`--bitres-mode <0|1|2>`: tryb rezerwuaru, gdzie 0 oznacza pełny, 1 zredukowany,
a 2 wyłączony (sztywny bitrate ramka po ramce).

PRZEPIS PRAKTYCZNY dla materiału, który ma "żyć" po trudnych fragmentach (~128 kbps stereo,
muzyka akustyczna/orkiestrowa):

    fdkaac-franken-x64.exe -p 2 -b 128000 --peak-bitrate 200000 --vbr-reservoir 12000 -o out.m4a in.wav

Co to robi: średnia zostaje w okolicach 128, ale gdy przychodzi kulminacja, ramki mogą
sięgnąć 200 kbps, a szeroki rezerwuar (12000 bitów) sprawia, że koder NIE dusi natychmiast
kolejnych ramek, lecz pozwala jakości opaść łagodnie. To jest dokładnie ten efekt "nie tnij
na siłę, pozwól temu żyć".

Dla mocniejszego działania podnieś `--peak-bitrate` do 256000, a `--vbr-reservoir` do 18000.
Gdy średnia ma być trzymana twardziej (limit łącza), zmniejsz rezerwuar do 4000.

WAŻNE OGRANICZENIE: to nadal działa na silniku CBR plus rezerwuar, nie na prawdziwej
alokacji "po zawartości" jak w LAME. Wahania są umiarkowane (rzędu ±20-50%), bo AAC ma
twardy limit 6144 bity na kanał na ramkę. Ten "lekki lot" to jednak zwykle dokładnie to, o
co chodzi, i jest w pełni kontrolowany.

## 9. Skrajne bitrate: bardzo nisko i bardzo wysoko

BARDZO NISKO. Standardowo FDK nie pozwoli zejść poniżej pewnej dolnej granicy.
Aby uzyskać 6 kbps HE-AAC stereo, 4 kbps HE-AAC v2 albo 3 kbps LC (choćby dla
eksperymentu), trzeba użyć flagi `--unlock-bitrate`. UWAGA na jeden szczegół:
normalnie ten frontend traktuje małe liczby przy `-b` jako kilobity (czyli
`-b 96` = 96 kbps). W trybie `--unlock-bitrate` bierze `-b` DOSŁOWNIE jako bity
na sekundę, czyli `-b 8000` to 8000 bps = 8 kbps.

    fdkaac-franken-x64.exe -p 29 -b 6000 --unlock-bitrate -o out.m4a in48k.wav

DWA WAŻNE ZASTRZEŻENIA do bardzo niskich bitrate'ów:

Po pierwsze: skąd w ogóle biorą się te dolne granice i co flaga zdejmuje. Popularne
przekonanie, że HE-AAC ma twarde minimum 16 kbps, jest nieprawdziwe; powtarzała je także ta
instrukcja. 16 kbps to tylko najniższy wiersz tabeli strojenia SBR dla 48 kHz stereo. Dla
mono i niższych częstotliwości tabela zaczyna się od 8..12 kbps, dla HE-AAC v2 od 8 kbps.
Fabryczny FDK podnosi wszystko, co leży niżej, do tego wiersza. Z `--unlock-bitrate` koder
bierze najniższy wiersz i zostawia zadany bitrate: siatka SBR jest ta sama, rdzeń AAC
dostaje po prostu mniej bitów. W LD i ELD flaga zdejmuje dodatkowo regułę 8 kbps na kanał i
208-bitowy zapas na transport (prawdziwy narzut nagłówków nadal jest sprawdzany), a poniżej
tabeli pasm bierze najwęższe pasmo. Najniższe `-b`, przy którym kodowanie przy 48 kHz się
kończy (w nawiasie najniższy dozwolony poziom domyślny, do którego koder podnosił): HE-AAC
stereo 5050 (16008), HE-AAC mono 2505 (12008), HE-AAC v2 3122 (12008), LD 8291 (19500), ELD
9062 (19500), ELD z SBR około 10500..11300 (36008..60008), LC 2659 (9750). Przy 24 i 32 kHz
wartości są jeszcze niższe. Prawdziwa twarda granica to szacunek ładunku samego SBR (około
2,5 kbps na kanał plus PS): poniżej niego rdzeń nie ma ani bitu i koder kończy czystym
komunikatem „encoder initialization failed”.

Przy takich wartościach licz się z tym, że gęste fragmenty wyjdą powyżej `-b`: ramka, która
nie mieści nawet własnych nagłówków i informacji pobocznej (side information), przekracza
budżet, a rezerwuar odrabia to na kolejnych ramkach. Przykład: `-b 6000` dla HE-AAC stereo
dał realnie 7290 bps na muzyce. Trzeba sprawdzać rozmiar pliku, a nie samą deklarację w
`--verbose`. Do odsłuchu takich strumieni LD i ELD służy dekoder Fraunhofera; ffmpeg 6.1 ma
z nimi kłopot nawet przy koderze fabrycznym.

Po drugie, UWAGA na interpretację `-b` w trybie unlock. Normalnie ten frontend
traktuje małe liczby jako kilobity: `-b 128` = 128 kbps, `-b 1152` = 1152 kbps.
Z `--unlock-bitrate` liczba jest jednak brana DOSŁOWNIE jako bity na sekundę:
`-b 8000` = 8000 bps = 8 kbps. Pomyłkowe `-b 1152
--unlock-bitrate` (w zamiarze "1152 kbps") daje 1152 bps, czyli praktycznie
nic. Program ostrzeże, gdy wartość wygląda na taką pomyłkę. Reguła: w trybie
unlock zawsze podawaj pełną liczbę bitów (np. `-b 6000`, nie `-b 6`).

BARDZO WYSOKO. Górny sufit to 6144 bity na kanał na ramkę. Przy 48 kHz to wychodzi około 576
kbps na kanał; przy stereo realne ograniczenie pojawia się w okolicach 600 kbps sumarycznie.
Aby przekroczyć 1000 kbps, trzeba podnieść częstotliwość próbkowania powyżej 48 kHz (źródło
96 kHz): wtedy sufit rośnie i pozwala osiągnąć 1024, a nawet 1152 kbps. To ograniczenie
samego standardu AAC.

Uwaga o jednoznaczności: przy tak wysokich bitrate'ach (1152) nie używa się
`--unlock-bitrate` (ta flaga służy do schodzenia w dół), więc `-b 1152` znaczy
tam jednoznacznie 1152 kbps. Kolizja interpretacji dotyczy wyłącznie trybu
unlock, gdzie i tak w grze są małe liczby bps. Praktycznie te dwa światy się nie
stykają.

## 9a. Czy warto robić "afterburner plus": więcej iteracji przy skrajnym bitrate

Przy optymalizacji bardzo wysokiego bitrate'u jedno trzeba powiedzieć wprost, bo inaczej
łatwo stracić czas na ślepą uliczkę. W środku kodera jest pętla, która iteracyjnie dobiera
kwantyzację (parametr wewnętrzny "maxIterations"; afterburner włączony daje 5 prób,
wyłączony 2). Kuszące jest myślenie "20 iteracji da dokładniejszy wynik". Ta pętla jest
jednak mechanizmem RATUNKOWYM na wypadek NIEDOBORU bitów: uruchamia się dopiero, gdy ramka
nie mieści się w budżecie i trzeba ją docisnąć. Przy skrajnie WYSOKIM bitrate budżet jest
ogromny, ramka mieści się od razu, więc pętla prawie nigdy nie wchodzi w kolejne iteracje.
Zwiększanie limitu nic tam nie da. To nie "więcej polerowania", tylko "wyższy limit
awaryjnego dociskania", który w takich warunkach nie jest w ogóle używany. Sam afterburner
(`--afterburner 1`) wybiera dokładniejszą tabelę strojenia i jest włączony domyślnie, a
wyżej ten konkretny mechanizm nie sięga, bo w kodeku nie ma dodatkowych poziomów.

Co NAPRAWDĘ daje więcej detalu przy wysokim bitrate, to nie liczba iteracji, ale OBNIŻENIE
PROGÓW MASKOWANIA, żeby koder w ogóle uznał więcej rzeczy za warte zakodowania:
`--ath-scale` poniżej 256, `--spread-mask` poniżej 256 i `--side-bias` powyżej 0 (więcej
bitów w szerokość stereo). To są dźwignie, które faktycznie zamieniają nadmiarowe bity na
zachowany detal (patrz rozdział 18). Iteracje kwantyzacji nie.

BARDZO WYSOKO (ciąg dalszy właściwej optymalizacji): patrz rozdział 18.

---

# Część III. Opcje standardowe (nie tylko Frankenstein)

To są przełączniki dostępne w każdej wersji tego frontendu. Warto je znać, bo to fundament,
na którym opiera się reszta.

## 10. Wybór profilu i podstawy

`-p <n>`: profil (Audio Object Type). Najważniejsze: `2` = AAC-LC, `5` =
HE-AAC, `29` = HE-AAC v2, `23` = AAC-LD (low delay, do komunikacji), `39` =
AAC-ELD. Do muzyki: 2 dla wysokich bitrate, 5 dla średnich, 29 dla najniższych.

`-b <n>`: bitrate w bitach na sekundę dla CBR (np. `-b 192000`). Małe liczby
frontend mnoży ×1000 (`-b 192` = 192 kbps), poza trybem `--unlock-bitrate`.

`-m <n>`: tryb VBR 1-5 (1 najniższa jakość, 5 najwyższa). Jak wspomniano,
w FDK słaby; do poważnej pracy lepiej użyć CBR z rozdziału 8.

`-o <plik>`: plik wyjściowy. Rozszerzenie `.m4a` daje kontener MP4;
`-f 2` przełącza na surowy strumień ADTS (`.aac`).

`-w <n>`: szerokość pasma rdzenia w Hz. Pod SBR jest ignorowane: rdzeń kończy się
wtedy tam, gdzie zaczyna się SBR, a przesuwa go `--sbr-start` (rozdział 15).

## 11. Afterburner i sygnalizacja

`--afterburner <0|1>`: afterburner to dodatkowy, wolniejszy algorytm
optymalizacji kwantyzacji. `1` = włączony (lepsza jakość, wolniej), i to jest
ustawienie domyślne oraz zalecane. Warto go wyłączać tylko dla szybkości.

`-s <n>` (signaling): sposób sygnalizowania SBR/PS w strumieniu (kompatybilność
ze starszymi dekoderami). Domyślne "auto" jest w większości przypadków dobre.

## 12. Kontener i metadane

`--moov-before-mdat`: umieszcza indeks MP4 na początku pliku (przydatne do
streamingu progresywnego). Standardowe tagi (tytuł, artysta itd.) ustawia się
przełącznikami `--title`, `--artist`, `--album` i pokrewnymi.

---
# Część IV. Opcje Frankenstein: sterowanie wnętrzem kodera

To jest sedno tego kodera: przełączniki, które w fabrycznym FDK są zaszyte na sztywno, a
tutaj wystawione na zewnątrz. Każdy opisany jest praktycznie: co robi dla ucha, kiedy go
użyć i jakie wartości mają sens.

## 13. Stereo w rdzeniu: MS i Intensity

Stereo MS (mid/side). Zamiast kodować lewy i prawy kanał osobno, koder w każdym paśmie
koduje ich sumę (mid, L+R) i różnicę (side, L-R). Gdy kanały są podobne, a w muzyce zwykle
są, różnica jest mała i tania w zapisie. Sam obrót jest bezstratny: o tym, jak obraz stereo
przechodzi przez koder, decyduje podział bitów między mid i side, a przede wszystkim to,
które pasma side w ogóle zostaną zakodowane. Każde pasmo side wyzerowane w ramce to kawałek
obrazu przesunięty na środek.

Tę pracę wykonują dwa przełączniki i większości osób wystarczą tylko one.

`--side-bias <dB>` przesuwa granicę, przy której pasmo side zostaje albo wypada. Wartość
dodatnia obniża próg side: mniej pasm side jest zerowanych, pozostałe są kodowane
dokładniej, a pogłos i dźwięk pomieszczenia zostają na swoim miejscu. Bity zabiera mid, więc
przy niskim bitrate'ie płaci środek. Wartość ujemna podnosi próg i zwęża obraz w stronę
mono, ze skokiem przy około -0,97 dB, gdzie razem wypadają pasma, które FDK trzymało przy
życiu tylko dzięki ratunkowi przed dziurami. Na muzyce akustycznej przy 96 kbps `+6` daje
side czystszy o 1,3 dB i mid gorszy o 1,3 dB; przy 48 kbps zysk przychodzi dopiero od
około +6. Użyteczny zakres: od +3 do +9 dla szerokości, od 0 do -0,95 dla łagodnego
zwężania.

`--side-knee <dB>` stopniuje pasma side leżące przy tej granicy według ich własnego
poziomu. Gdy ramce brakuje bitów, FDK utrzymuje słabe pasma najgrubszym krokiem, około
0,97 dB ponad progiem, więc mniej więcej co dziesiąte pasmo side ląduje na tej jednej
wartości, niezależnie od rzeczywistego poziomu. Knee czyta zapas, którego koder chciał
przed tym ratunkiem, i znów stopniuje te pasma. Wartość dodatnia dodaje dokładności przy
granicy, ujemna odbiera ją najpierw najsłabszym pasmom, bez skoku. Przy 96 kbps liczba
kodowanych pasm side na ramkę spada równomiernie z 30,2 (bez przełącznika) przez 25,4 (-1)
i 18,1 (-6) do 1,1 (-24). Za ten sam zysk w side knee bierze mniej z mid niż bias (`+6`:
side -12,9 dB i mid -19,4 dB wobec -12,7 i -19,0). Użyteczny zakres: od -6 do +6. Oba
przełączniki się łączą: bias ustawia granicę, knee łagodzi jej krawędź.

`--side-cutoff <Hz>` oddaje side powyżej zadanej częstotliwości: każde pasmo stereo jest
tam kodowane jako czysty mid z zachowanym poziomem, a bity trafiają do mid. Stereo poniżej
granicy jest nietknięte. Przy 48 kbps granica 5 kHz oszczędziła 16 bitów informacji
pobocznej na ramkę, a mid był czystszy o 0,7 dB. Przy 14 kbps zysk jest mały, bo FDK
i tak zeruje tam większość side.

Pozostałe przełączniki MS to laboratorium. `--msmask 0|1` wymusza czyste L/R albo MS w
każdym paśmie; `--msbands`, `--msbands-lo` i `--msbands-hi` zamykają MS w zakresie pasm
(liczonych od 0 przy basie; ile ich jest, pokazuje `--verbose`). Służą do przypadków
technicznych, na przykład pliku, który pójdzie później przez usuwanie środka i musi zachować
nietknięte L/R; każde zmierzone ograniczenie pogarszało oba kanały przy tym samym
bitrate'ie. `--ms-bias`, `--ms-precision` i `--mid-bias` to liczby Q8 (256 = 1,0), które
przesuwają wprost decyzję MS albo progi pasm MS. Przy CBR pętla dopasowania odkręca je
niemal całkowicie. Przy VBR `--ms-precision` działa jak podniesienie jakości opłacone
rozmiarem pliku (512: plik większy o 5%, błąd niższy o około 0,8 dB), a `--mid-bias` tylko
pogrubia mid. Każdy z nich opisuje opis przełączników.

INTENSITY STEREO (IS). Bardziej agresywna technika: dla wysokich pasm, gdzie ucho słabo
lokalizuje kierunek, koder wysyła jedną obwiednię energii zamiast dwóch kanałów. Oszczędza
dużo, ale może zwęzić scenę.

`--is <0|1>`: globalnie włącza albo wyłącza IS.

`--is-aggression <0..100>`: KONSUMENCKI suwak agresywności IS. To zwykle
najlepszy wybór. 0 = zachowanie domyślne FDK (bardzo ostrożne), 100 =
maksymalnie agresywne intensity. Jest potrzebny, bo FDK ma trzy niezależne
warunki dopuszczenia (gate) wpuszczające IS (stosunek bitrate do pasma, próg
korelacji kanałów, minimalna liczba ciągłych pasm), a muszą być spełnione
JEDNOCZEŚNIE. Ruszenie jednego progu nic nie da, bo blokuje inny. Ten suwak
rusza wszystkie naraz. Zacznij od 40-70 i słuchaj.

Zaawansowane progi IS (dla eksperymentujących, działają PO przejściu przez
suwak agresywności): `--is-min-sfbs <n>` (minimalna liczba ciągłych pasm),
`--is-corr-thresh <n>` (próg korelacji kanałów, Q8), `--is-lr-ratio <n>` (próg
stosunku L/R, Q8). Uwaga praktyczna: te progi działają NIEINTUICYJNIE, bo niższa
wartość progu korelacji oznacza AGRESYWNIEJSZE IS, nie odwrotnie.

Umiejscowienie IS w pasmach. `--is-lo <sfb>` i `--is-hi <sfb>` pozwalają OGRANICZYĆ
intensity stereo do zakresu pasm: IS jest dozwolone dopiero od `--is-lo` w górę i tylko do
`--is-hi` włącznie. To nigdy nie wymusza IS, a jedynie ogranicza, gdzie FDK może go użyć. W
praktyce przy niskim bitrate IS ląduje na NISKICH pasmach, więc trzeba skanować małe
wartości, żeby w ogóle zobaczyć efekt. Do WYMUSZENIA IS służą `--is-force-lo <sfb>` i
`--is-force-hi <sfb>`, które wpychają intensity stereo na cały zakres z pominięciem warunku
korelacji, warunku min-sfbs i warunku głośności. To tryb laboratoryjny: ponieważ IS jest
stratne i kierunkowe (prawy kanał zostaje wyzerowany, zostaje tylko współczynnik panoramy),
wymuszenie może celowo rozbić obraz stereo. Strumień i tak pozostaje zgodny ze standardem.

## 14. Pasmo i odcięcie (w tym audiofilskie pełne pasmo)

`-w <Hz>` ustawia górną krawędź pasma kodowanego przez rdzeń. Bez SBR `--core-cutoff <Hz>`
robi dokładnie to samo: we wszystkich zmierzonych profilach bez SBR oba dały strumienie
identyczne co do bitu. Pod SBR oba ustępują startowi SBR: rdzeń kończy się tam, gdzie
zaczyna się SBR, a ten punkt przesuwa `--sbr-start` (rozdział 15). `--core-cutoff` pod SBR
może tylko ściągnąć rdzeń poniżej startu SBR, co zostawia między nimi dziurę, albo
przesunąć go powyżej, gdzie dekoder i tak nadpisuje to, co wysłał rdzeń, własnym SBR.
Żadne z tych zastosowań nie jest przepisem; oba to eksperymenty.

Uwaga o rzeczywistym odcięciu w `--verbose`: widmo AAC jest podzielone na pasma
współczynników skali (scalefactor bands, SFB), więc koder nie potrafi uciąć pasma w
dowolnym miejscu w Hz; wyrównuje odcięcie do najbliższej granicy pasma SFB. Dlatego
`--verbose` pokazuje rzeczywiste odcięcie, które może różnić się od wpisanej wartości
(`-w 17300` wyświetla się jako `17915 Hz (SFB-anchored)`). To nie błąd, tylko pasmo,
które naprawdę trafia do strumienia.

`--uncap-bandwidth`, dla materiału pełnopasmowego. FDK ogranicza rdzeń do mniejszej z
wartości: 20 kHz i połowa częstotliwości próbkowania, więc nawet przy źródle 96 kHz i
wysokim bitrate'ie nic powyżej 20 kHz nie było kodowane. Ta flaga zdejmuje limit i `-w`
albo `--core-cutoff` mogą wtedy sięgnąć połowy częstotliwości próbkowania:

    fdkaac-franken-x64.exe -p 2 -b 320000 --uncap-bandwidth --core-cutoff 40000 -o out.m4a in96k.wav

Zmierzone: bez flagi powyżej 20 kHz praktycznie nie ma energii; z nią pasmo do 40 kHz jest
naprawdę kodowane (`--verbose` podaje 42000 Hz, najbliższą granicę pasma SFB).

## 15. SBR: sterowanie górnym pasmem

Nadpisuje ustawienia z wewnętrznej tabeli tuningowej SBR.

`--sbr-start <n>` / `--sbr-stop <n>`: indeksy początku i końca pasma SBR. Start to
najużyteczniejszy przełącznik całej grupy SBR, bo rdzeń idzie za nim: podniesienie startu
o jeden indeks daje rdzeniowi prawdziwie kodowane widmo aż do następnego progu (przy
HE-AAC 64 kbps z 48 kHz przejście z 14 na 15 poszerza rdzeń z 10,5 do 12 kHz), a
obniżenie oddaje SBR większą część góry i zostawia rdzeniowi więcej bitów na to, co
niżej. Start warto zmieniać o jeden indeks i słuchać; Tabela 2 pokazuje, gdzie wypada
każdy indeks. Stop przesuwa tylko górną krawędź pliku i nie rusza rdzenia.
FDK sprawdza parę: zła daje "encoder initialization failed". Dozwolone pary tworzą ukośny
pas i zależą od bitrate'u oraz częstotliwości próbkowania. Na przykład przy HE-AAC
64 kbps stereo z 48 kHz start 0..4 jest niedozwolony z każdym stopem, a 8/9, 8/14 i 14/12
działają. Po każdej zmianie warto przeczytać `--verbose`.

`--sbr-freqscale <n>`: grupowanie częstotliwości (0 liniowe, wyżej = drobniejsze
logarytmiczne). `--sbr-noise-bands <0..3>`: gęstość opisu szumu SBR:
0 to jedno pasmo szumu, 1, 2 i 3 to około 1, 2 i 3 pasm na oktawę. W nagłówku SBR to pole
ma 2 bity, więc większych wartości nie da się przesłać.
`--sbr-amp-res <0|1>`: rozdzielczość amplitudy obwiedni (0 = 1,5 dB, 1 = 3 dB).

`--sbr-num-env <1|2|4>`: liczba obwiedni na ramkę, czyli rozdzielczość czasowa SBR.
Więcej obwiedni to lepiej oddane zmiany w czasie w górnym paśmie, kosztem bitów.
WAŻNE: ta opcja WYMUSZA statyczną siatkę czasową (wyłącza detektor transientów),
więc na materiale z ostrymi atakami może być gorzej. Dobre do stabilnych,
wybrzmiewających dźwięków. (Wartość 8 przekracza standardową siatkę i jest
odrzucana.)

`--sbr-freqres-fixfix <0|1>`: rozdzielczość częstotliwości obwiedni.

`--sbr-stereo-mode <1..3>`: tryb stereo warstwy SBR, niezależny od MS rdzenia.
1 = LR (niezależne górne pasmo dla każdego kanału), 2 = coupling (wspólna obwiednia oraz
poziom i balans), 3 = switch (LR albo coupling w każdej ramce). Bez przełącznika FDK
używa 3 poniżej 60 kbps stereo i 1 od 60 kbps. Wymuszenie 1 poniżej 60 kbps daje pełną
separację na górze, 2 oszczędza bity przy niskim bitrate'ie. Wejście mono nie wymaga
przełącznika.

`--sbr-invf <0..3>`: filtrowanie odwrotne (inverse filtering). To mechanizm
sterowany przez ESTYMATOR TONALNOŚCI: SBR analizuje, czy górne pasmo powinno być
bardziej tonalne czy bardziej szumowe, i odpowiednio "wybiela" skopiowany
materiał. 0 = wyłączone, 1 = słabe, 2 = średnie, 3 = mocne. Mocniejsze daje
mniej "metalicznego" dzwonienia w górze, kosztem części detalu. Zwykle steruje
tym automat, a ręczne wymuszenie ma sens, gdy słychać metaliczność.

Wymuszenie ma jedną wadę: jeden poziom obowiązuje wtedy w całym pliku, także w pasmach i
momentach, w których automat wybrałby inaczej. Drugi sposób to zostawić decyzję automatowi i
zmienić tylko jego reguły. Decyzja zapada nadal osobno dla każdego pasma i każdej ramki.

`--sbr-softness <0..7>`: jedno pokrętło od „twardo, metalicznie” do „bardzo
miękko”. `2` to dokładnie estymatory fabryczne dla muzyki, `4` estymatory
fabryczne dla mowy (bez reszty zmian z `--speech`, czyli bez innego crossovera).
Poziomy 0..1 dają mniej wybielania, 3 trzyma co najmniej słabe, 5..7 dokładają
szumu i wybielania. Pomiar z bitstreamu: średni poziom filtrowania rośnie z 0 do
około 3, a płaskość zdekodowanego pasma SBR z 0,37 do 0,42 na mowie i z 0,40 do
0,52 na materiale tonalnym. Przełączanie między ramkami zostaje na poziomie
domyślnym.

Pokrętła dla eksperta, każde wygrywa z makrem:
`--sbr-invf-min` i `--sbr-invf-max` ograniczają wybór automatu od dołu i od
góry (np. `--sbr-invf-max 2` = nigdy mocne). `--sbr-invf-bias <-3..3>` przesuwa
każdą decyzję o tyle stopni. `--sbr-invf-table 0|1` wybiera tabelę decyzji
muzyki albo mowy niezależnie od `--speech`. `--sbr-invf-hyst` (0..2, fabrycznie 1)
to histereza granic: mniejsza daje szybszą reakcję i częstsze przełączanie.
`--sbr-invf-sbr-thr` i `--sbr-invf-orig-thr` przesuwają granice tonalności pasma
odtworzonego przez SBR i oryginalnego, mniej więcej w dB. Uwaga: przesunięcie granic potrafi
wywołać migotanie decyzji między ramkami, dlatego makro ich nie używa.
`--sbr-invf-smooth 0..2` mówi, ile ramek uśrednia estymator.

Szum: `--sbr-nf-weight 0..1` to waga różnicy tonalności oryginału i pasma
odtworzonego przez SBR w poziomie szumu (muzyka 0,25, mowa 1,0),
`--sbr-nf-weight-from 1..3` mówi, od którego poziomu filtrowania pasma dostają tę
wagę (fabrycznie tylko mocne), a `--sbr-nf-smooth 0` wyłącza wygładzanie szumu w
czasie. Ważna rzecz z pomiaru: szum ogranicza sufit `--sbr-noise-max`. Z sufitem
z tabeli strojenia (0,125) około 95% pasm już na nim siedzi, więc sama waga
często nic nie zmienia. Trzeba ją łączyć z `--sbr-noise-max 3` albo `6`.

    fdkaac-franken-x64.exe -p 5 -b 32000 --sbr-softness 6 -o out.m4a in.wav
    fdkaac-franken-x64.exe -p 5 -b 32000 --sbr-invf-max 2 --sbr-invf-hyst 1.5 -o out.m4a in.wav

`--sbr-noise-floor-offset <0..11>`: offset poziomu szumu SBR (noise floor), czyli
minimalnego poziomu szumu wstrzykiwanego przez SBR.
Większe wartości to więcej szumu wypełniającego rekonstrukcję. FDK przyjmuje tylko 0..11;
inne wartości są odrzucane z błędem.

`--sbr-header-period <n>`: co ile ramek zapisywane są nagłówki SBR, co decyduje,
jak szybko górne pasmo SBR "wchodzi", gdy dekoder podłącza się do strumienia
HE-AAC NA ŻYWO (Icecast/Shoutcast). Haczyk jest taki: cała konfiguracja SBR
mieszka w okresowym nagłówku, a nie w każdej ramce. Słuchacz, który wpina się w
środek transmisji, słyszy najpierw sam rdzeń AAC (przytłumiony, bez góry), aż
nadejdzie kolejny nagłówek. Po ustawieniu `1` nagłówek zapisuje się w każdej
ramce, więc dekoder łapie SBR niemal natychmiast (~23 ms). To właściwy wybór dla
strumienia, do którego ludzie dołączają w losowych momentach. Większe wartości
wydłużają ten moment "sam rdzeń". Domyślnie FDK daje 10 ramek, czyli około 0,46 s
przy 44,1 kHz (ramka HE-AAC to 2048 próbek wyjściowych). Wartość jest brana
dosłownie, bez sufitu jednej sekundy, który ma fabryczny FDK.

`--sbr-header-time <s>`: to samo, ale w sekundach, z częścią dziesiętną (0,01 do
600, np. `3.5`). Wygrywa z `--sbr-header-period`. Przydaje się, gdy operator
strumienia wymaga konkretnego odstępu między poprawnymi nagłówkami SBR, np. 3,5 s.
Działa w HE-AAC i HE-AAC v2 (nagłówek SBR w ramce, każdy transport) oraz w AAC-ELD
z SBR (`-p 39 -L 1`). ELD nie ma nagłówka SBR w ramce: jego konfiguracja SBR jest
tylko w konfiguracji strumienia, więc przełącznik ustawia wtedy okres powtarzania
StreamMuxConfig w LATM/LOAS (`-f 6`, `-f 7`, `-f 10`). Przy ELD w ADTS albo w M4A
konfiguracja leży tylko w kontenerze i nie ma czego powtarzać.

`--verbose` wypisuje efektywny okres w ramkach i milisekundach. Pomiar po stronie
dekodera przy `--sbr-header-time 3.5`: najdłuższe czekanie na pasmo SBR po wejściu
w środek strumienia wyniosło 3,41 s (HE-AAC), 3,46 s (HE-AAC v2) i 3,52 s (ELD).

## 16. Parametric Stereo (HE-AAC v2)

`--ps <0|1>`: wymusza wysyłanie parametru IID (różnica głośności), gdzie 0 =
nigdy (spłaszcza obraz stereo do mono-podobnego), 1 = zawsze.

`--ps-iid-quant <0|1>`: dokładność kwantyzacji IID, gdzie 0 gruba, 1 dokładna.

`--ps-icc <0|1>`: włącza albo wyłącza ICC (Inter-channel Coherence, czyli
spójność i podobieństwo kanałów). `--ps-icc-mode <0|1>` to tryb rotacji ICC
(ROT_A / ROT_B). Warto pamiętać, że tryb rotacji to *wyłącznie sygnalizacja*:
oba tryby opisują tę samą macierz stereo, dekoder po prostu wylicza ją inaczej.
To pokrętło kompatybilności, nie jakości.

### Rozdzielczość PS: pasma (częstotliwość) i obwiednie (czas)

Dwa pokrętła, które realnie zmieniają jakość stereo, to te decydujące o tym, *ile*
parametrów stereo w ogóle zostanie wysłanych. Baseline MPEG-4 PS dopuszcza dwie
rozdzielczości pasmowe i do czterech obwiedni parametrów na ramkę, a fabryczny FDK wybiera
jedno i drugie wyłącznie na podstawie bitrate, więc powyżej 36 kbps wszystko jest zamknięte
w 20 pasmach i 4 obwiedniach, bez możliwości usłyszenia alternatyw.

`--ps-bands <10|20>`: rozdzielczość częstotliwościowa parametrów stereo.
Dwadzieścia pasm opisuje obraz stereo z drobniejszym podziałem widma; dziesięć
opisuje go zgrubniej, ale zużywa mniej bitów. Przy bardzo niskich przepływnościach
zgrubniejsza siatka potrafi brzmieć wręcz spokojniej, bo każde pasmo jest
uśredniane po większym wycinku widma i parametry mniej „drgają" między ramkami.

`--ps-env <1|2|4>`: rozdzielczość czasowa. Jedna obwiednia oznacza, że cała ramka
dostaje jedną migawkę stereo, a cztery, że panorama jest próbkowana cztery razy w
ramce. To właśnie decyduje, czy jadąca panorama albo transjent utrzyma swoje
położenie, czy rozmyje się na całą ramkę.

`--ps-env-reduce <0|1>` jest pokrętłem najważniejszym, z powodu pewnej pułapki.
Żądanie czterech obwiedni nie oznacza, że faktycznie je dostaniemy. FDK uruchamia
automatyczną pętlę redukcji, która połowi liczbę obwiedni (cztery do dwóch do
jednej) tak długo, jak sąsiednie obwiednie wyglądają podobnie według zaszytego w
kodzie progu błędu. Na zwykłej muzyce ten próg spełnia się bardzo często, więc
koder po cichu wraca do jednej migawki na ramkę. Ustawienie `--ps-env-reduce 0`
wyłącza tę pętlę i sprawia, że `--ps-env` znaczy to, co mówi.

Pomiar jest tu wyjątkowo czytelny. Na czterosekundowej próbce z panoramą celowo przeciąganą
tam i z powrotem z częstotliwością 0,25 Hz oraz naprzemiennymi transjentami lewo/prawo,
kodowanej jako HE-AAC v2 przy 48 kbps, porównując trajektorię panoramy zdekodowanego wyniku
z oryginałem: fabryczny koder wychodzi na 0,177 RMS błędu panoramy (korelacja 0,9655). Samo
żądanie czterech obwiedni nie zmienia *absolutnie nic*, bo wyjście jest bit w bit identyczne
z domyślnym: pętla redukcji natychmiast wycofuje żądanie. Po dodaniu `--ps-env-reduce 0`
błąd spada do 0,117 (korelacja 0,9944) przy praktycznie tym samym rozmiarze pliku: o jedną
trzecią mniejszy błąd panoramy, za darmo. Jeśli z tego rozdziału ma zostać jedno ustawienie,
niech to będzie `--ps-env 4 --ps-env-reduce 0`.

`--ps-noenv-skip <0|1>` to pokrewny fragment ukrytego zachowania. Poza
przerzedzaniem obwiedni FDK potrafi też pominąć parametry stereo *całkowicie* na
pewnym odcinku ramek: gdy kolejne zestawy IID/ICC wyglądają podobnie, może wysłać
do dziesięciu kolejnych ramek bez żadnej informacji stereo, pozostawiając dekoder
na ostatnich wartościach, jakie dostał. Na materiale, gdzie obraz dryfuje wolno,
słychać to jako chwilowe spłaszczenie obrazu stereo i powrót. `--ps-noenv-skip 0`
tego zabrania i wymusza parametry w każdej ramce.

### IPD: wysyłanie fazy (`--ps-ipd 1`)

Fabryczny FDK nigdy nie wysyła fazy międzykanałowej. Pola są zaszyte na zero z komentarzem
„IPD OPD not supported right now", ale ten komentarz zawyża dystans do działającej
implementacji. Koder już teraz akumuluje zarówno część rzeczywistą, jak i urojoną widma
skrośnego lewy/prawy, dla każdego pasma i każdej obwiedni; używa tylko modułu, do policzenia
ICC, a kąt wyrzuca. Ten kąt *jest* IPD. Tablice Huffmana i writer bitstreamu też już były na
miejscu. Brakującym elementem nigdy nie była więc „analiza fazy od zera", lecz wzięcie
`atan2` z dwóch liczb leżących już w tej samej pętli, zaokrąglenie do 8 zdefiniowanych
kroków fazy i włączenie writera.

Dlaczego to ma znaczenie: bez fazy PS umieszcza dźwięk w przestrzeni wyłącznie różnicami
poziomu i korelacją. To obejmuje wszystko, co spanoramowano poziomem, ale nie źródło
umiejscowione *czasem*. Realne opóźnienie międzykanałowe, a tak właśnie działa w naturze
lokalizacja w dole pasma, jest dla IID i ICC niewidzialne, bo oba kanały niosą ten sam
sygnał na tym samym poziomie i różnią się tylko momentem dotarcia.

Pomiar korzysta z próbek zbudowanych dokładnie w ten sposób: identyczny sygnał w obu
kanałach, równy poziom, różnica wyłącznie w opóźnieniu. Porównanie profilu fazy
zdekodowanego materiału z oryginałem, w zakresie, który IPD realnie pokrywa (60-690 Hz):

| Opóźnienie międzykanałowe | błąd fazy, IPD wył. | błąd fazy, IPD wł. |
|---|---|---|
| 0,25 ms | 35,0° | **14,9°** |
| 0,4 ms | 56,1° | **24,9°** |
| 0,7 ms | 67,4° | 71,8° |

Przy ćwierci i czterech dziesiątych milisekundy błąd fazy spada o ponad połowę, z
identycznym wynikiem przy 32 i 48 kbps. Przy 0,7 ms korzyść znika, bo to reprezentacja
dochodząca do swojej granicy, a nie usterka. Na górnym krańcu zasięgu IPD opóźnienie 0,7 ms
przebiega już niemal całe plus-minus 180 stopni, więc siatka co 45 stopni nie potrafi
jednoznacznie powiedzieć, gdzie ono jest. To ten sam sufit niejednoznaczności fazowej, przez
który słuch człowieka korzysta z przesłanek czasowych głównie na dole.

Jedna uwaga praktyczna, zanim przejdziemy do drugiego parametru: IPD sięga tylko mniej
więcej najniższych 690 Hz, bo jego jedenaście pasm parametrycznych mieści się w pierwszych
trzech pasmach QMF. Słuchanie albo mierzenie powyżej tego zakresu nie pokaże zupełnie nic.

### OPD: druga połowa pary

Samo IPD to nie cała historia i warto rozpisać, dlaczego. IPD to faza lewego kanału
względem prawego; **OPD to faza lewego kanału względem monofonicznego downmiksu.**
Dekoder używa obu: ścieżki karmione downmiksem obraca o `OPD`, a pozostałe o
`OPD - IPD`. Po odjęciu jednego od drugiego `OPD` się skraca, więc międzykanałowa
*różnica* zawsze wychodzi równa IPD, niezależnie od tego, co mówi OPD. To, gdzie
ten obrót leży względem downmiksu, jest już jednak wyłącznie sprawą OPD.

Przy OPD równym zeru lewy kanał zostaje przypięty do fazy downmiksu, a cały obrót ląduje na
prawym. Różnica jest nadal poprawna i właśnie dlatego łatwo to przeoczyć: każdy pomiar
`arg(L) - arg(R)` jest na to ślepy. Wystarczy zmierzyć zamiast tego fazę bezwzględną każdego
kanału względem oryginału, w zasięgu IPD, i asymetria staje się oczywista, podobnie jak to,
że OPD ją usuwa:

| Opóźnienie międzykanałowe | OPD = 0 (L / P) | OPD liczone (L / P) |
|---|---|---|
| 0,25 ms | 14,7 / 20,1° | 14,4 / **17,3°** |
| 0,4 ms | 23,0 / 28,0° | **15,5** / **17,8°** |
| 0,7 ms | 48,1 / 56,2° | **40,9** / **36,3°** |

Przy `OPD = 0` oba kanały nigdy się nie zgadzają: 14,7 wobec 20,1, potem 48,1 wobec 56,2. Z
policzonym OPD zbiegają się, a korzyść rośnie wraz z opóźnieniem: średnio +6,4 stopnia, przy
0,7 ms +13. Warto zauważyć, że to właśnie ten przypadek, w którym *zwykły* pomiar IPD nie
pokazywał nic: przy 0,7 ms różnica międzykanałowa przekraczała już to, co rozstrzyga siatka
45 stopni, a jednak faza bezwzględna każdego kanału i tak wyraźnie się poprawiła.

A wyprowadzenie nic nie kosztuje. Downmiks to `L + R`, więc

    sum(L * conj(L+R)) = sum(|L|²) + sum(L * conj(R))

czego część rzeczywista to `pwrL + pwrCr`, a urojona to `pwrCi`, czyli te same
akumulatory, które karmią już IID, ICC i IPD. Zatem
`OPD = atan2(pwrCi, pwrL + pwrCr)`: jeszcze jeden `atan2` na liczbach, które i tak
leżały w tej pętli, i nie trzeba przekazywać sygnału downmiksu skądkolwiek indziej.
Dla równych poziomów sprowadza się to analitycznie do `IPD/2`, bo
`arg(1 + e^{-i·φ}) = -φ/2`. To wygodny sprawdzian, choć postać ogólna powyżej jest
poprawna także wtedy, gdy kanały różnią się poziomem, a `IPD/2` już nie.

Wcześniejsza wersja tego kodera wysyłała OPD jako zero i twierdziła, że pomiar wykazał, iż
`IPD/2` nie pomaga. Ten pomiar był zakresowany na `arg(L) - arg(R)`, a to, jak wyjaśniono
powyżej, w ogóle nie widzi OPD. Werdykt był błędny i został poprawiony; `--ps-opd 0`
pozostaje dostępne do porównań A/B.

Jedna praktyczna konsekwencja, wyciągnięta z błędu i naprawiona w v1.3.1: rozszerzenie fazy
NIE niesie liczby pasm fazy. Dekoder wylicza ją z `iid_mode`, i to wyłącznie z nagłówka,
który sygnalizuje również IID. FDK decyduje o wysłaniu IID na podstawie heurystyki różnicy
głośności, więc na materiale o niemal równych poziomach kanałów, a przede wszystkim na
treści bliskiej przeciwfazie, legalnie nie sygnalizował IID, a wtedy dekoder odczytywał inną
liczbę bitów niż zapisał koder i fazę wyrzucał. Dlatego włączenie `--ps-ipd` powoduje teraz
również zasygnalizowanie IID. To parametr zwykły, tani i zawsze legalny, a przy tym jedyna
droga, którą liczba pasm dociera do dekodera.

Kompatybilność jest bezpieczna z samej konstrukcji. Dane siedzą w rozszerzeniu z prefiksem
długości, więc dekoder, który nie implementuje syntezy fazy (łącznie z własnym baseline'owym
dekoderem PS w FDK), odczytuje liczbę bajtów i je pomija, na co standard wprost pozwala.
Zweryfikowane w praktyce: przy wyłączonym przełączniku wyjście jest bit-identyczne z
poprzednim, a przy włączonym zarówno ffmpeg, jak i faad dekodują czysto przy 24, 32, 48 i 64
kbps.

### Detektory SBR: ataki i dorabiane harmoniczne

O tym, jak realnie brzmi odtwarzane pasmo górne, decydują w SBR głównie dwa detektory, a
żadnego z nich nie dało się do tej pory dotknąć z linii poleceń.

**Detektor transjentów** decyduje, czy atak dostanie własną obwiednię, czy
zostanie uśredniony na dłuższym odcinku. Kiedy przegapi atak, energia uderzenia
rozlewa się w tył w czasie: słychać krótki wysypek szumu wysokiego tuż *przed*
hi-hatem, nie po nim. To jest pre-echo i w paśmie SBR należy do bardziej
rozpoznawalnych artefaktów kodowania.

`--sbr-tran-thr <n>` skaluje główny próg decydujący o tym, jak łatwo detektor
się odpala (value*100, więc 100 to brak zmiany; poniżej 100 odpala chętniej, powyżej
100 rzadziej). Okazało się najskuteczniejszym
pokrętłem w tej grupie. Pomiar na sygnale z 24 atakami perkusyjnymi i tonami
wygaszającymi się między 8 a 13 kHz, licząc energię pasma górnego pojawiającą się
przed atakiem, a nieobecną w źródle: ustawienie fabryczne daje 1,610 przy 32 kbps,
a obniżenie progu do 40 zbija to do 0,041, czyli pre-echo praktycznie znika. To
samo dzieje się przy 64 kbps (1,186 do 0,059). Efekt saturuje poniżej 60 i zanika
zupełnie od 100 w górę, gdzie wyjście jest bit-identyczne z domyślnym. Na materiale perkusyjnym
`--sbr-tran-thr 40` to miejsce, od którego warto zacząć.

`--sbr-tran-peak <n>` steruje tym, jak *szpilkowy* musi być kandydat, żeby liczyć
się jako atak: to surowa stała 0,90, wpisywana jako 90. `--sbr-tran-split <n>`
rządzi tym, jak chętnie ramka bez wykrytego transjentu jest i tak dzielona na
dwie obwiednie, co kupuje rozdzielczość czasową kosztem bitów. Dwa kolejne
pokrętła, `--sbr-tran-quiet` i `--sbr-tran-dom`, siedzą w szybkim detektorze
transjentów, który działa tylko w AAC-ELD z SBR (`-p 39 -L 1`; ELD i LD wymagają M4A,
LATM albo LOAS, nie ADTS). `--sbr-tran-quiet` zmienia strumień ELD+SBR przy wartościach
poniżej 100; dla `--sbr-tran-dom` nie zaobserwowaliśmy zmiany na naszym materiale.

**Detektor missing harmonics** zajmuje się problemem odwrotnym. Gdy SBR kopiuje
pasmo dolne w górę, ton obecny w oryginale może trafić w złe miejsce albo zniknąć.
Detektor to zauważa i dorabia syntetyczną harmoniczną, żeby go przywrócić.
Zbyt gorliwe ustawienie daje gwizdy i metaliczne artefakty, których w nagraniu
nigdy nie było, a zbyt zachowawcze sprawia, że dzwonki, glockenspiel i talerze
matowieją, bo ich partiale po cichu znikają.

`--sbr-mh-tone <n>` skaluje to, jak tonalne musi być pasmo, zanim harmoniczna
zostanie dorobiona, i jest tu głównym sterem. `--sbr-mh-diff <n>` skaluje
*różnicę* tonalności między oryginałem a pasmem odtworzonym, która uruchamia
kompensację. `--sbr-mh-decay-orig` i `--sbr-mh-decay-diff` zmieniają to, jak
długo znaleziony już ton jest dalej śledzony przy wygasaniu, a to właśnie słychać
na wybrzmieniach talerzy i dzwonków. `--sbr-mh-sfm-sbr` i `--sbr-mh-sfm-orig`
przesuwają granicę płaskości widmowej między „to jest ton" a „to jest szum",
odpowiednio dla pasma odtworzonego i oryginalnego. `--sbr-mh-maxcomp` ogranicza
kompensację obwiedni wokół dorobionej harmonicznej, co wpływa na to, ile szumu
siedzi tuż obok niej.

Na koniec `--sbr-noise-max <6|3|-3>` ustawia sufit tego, jak głośny może być szum
wstrzykiwany przez SBR: odpowiednio 1,0, 0,5 albo 0,125. To najbardziej bezpośrednia
kontrola nad „powietrzem kontra syczeniem" na samej górze. W FDK było to już polem
konfiguracyjnym, wybieranym z tabeli tuningu po bitrate i nigdy nie wystawionym.

Wszystkie te pokrętła są opcjonalne: przy wartościach neutralnych wyjście jest
bit-identyczne z fabrycznym, a zestaw testów tego pilnuje.

## 17. TNS, PNS i afterburner

`--tns-mask <n>` i `--tns-order <n>`: TNS (Temporal Noise Shaping) kształtuje
szum kwantyzacji w czasie, żeby chował się pod transjentami (ważne dla ostrych
ataków, np. perkusji). Maska to bitowa mapa aktywnych filtrów, rząd to długość
filtra.

`--pns <0|1>`: PNS (Perceptual Noise Substitution) zastępuje szumowe pasma
opisem "tu jest szum o takiej energii" zamiast kodować go dokładnie. Duża
oszczędność na szumowym materiale.

`--pns-start <Hz>`: od jakiej częstotliwości PNS może działać.

`--force-pns`: omija warunek niskiego bitrate'u dla PNS. WYJAŚNIENIE: FDK ma
tabelę, która przy bardzo niskich bitrate'ach (poniżej około 28 kbps) całkowicie
wyłącza PNS, a wtedy `--pns-start` jest ignorowane, bo PNS w ogóle nie działa.
To dlatego przy 24 kbps słychać artefakty jak z MP3, a przy 64 kbps PNS
działa. Ta flaga omija tabelę i włącza PNS mimo niskiego bitrate.

Słowo o jednostkach, zanim przejdziemy do poszczególnych pokręteł, bo dzięki temu reszta
nabiera sensu. FDK trzyma te parametry detekcji PNS wewnętrznie jako liczby
stałoprzecinkowe, a nasze przełączniki przyjmują zwykły dziesiętny MNOŻNIK: wpisana wartość
jest mnożona przez 100 i użyta do przeskalowania fabrycznej wartości FDK. Czyli `1.0` znaczy
„×1,0 = zostaw fabryczną wartość", `1.5` znaczy „×1,5", `0.5` znaczy „×0,5", a `-1` znaczy,
że przełącznik jest wyłączony. Ta jedna konwencja (`wartość × 100`, `1.0` = bez zmian)
dotyczy każdego skalującego pokrętła `--pns-*` poniżej.

`--pns-gain <x>`: GŁOŚNOŚĆ dorabianego szumu PNS i pokrętło PNS używane najczęściej.
Gdy PNS uzna pasmo za „sam szum", wyrzuca faktyczne linie widmowe i zapisuje tylko jedną liczbę:
energię szumu tego pasma. Przy odtwarzaniu dekoder odtwarza losowy szum wyskalowany do tej
energii. `--pns-gain` skaluje tę zapisaną energię wprost. `1.0` = bez zmian (odtworzony szum
niesie energię oryginalnego pasma); `>1.0` czyni szum głośniejszym niż oryginał (przydatne, gdy
podstawiony szum brzmi zbyt nieśmiało i miks robi się głuchy); `<1.0` czyni go cichszym (gdy szum
za mocno syczy). To w praktyce pokrętło „jak głośny jest podstawiony szum". Wejście dziesiętne,
`-1` = off.

Pozostałe pokrętła PNS rządzą tym, KTÓRE pasma w ogóle zostaną zamienione w szum, czyli
etapem detekcji, a nie tym, jak głośny jest wynik. `--pns-tonality <x>` skaluje próg
detekcji tonalności. PNS odpala się tylko na pasmach, które koder uzna za „szumopodobne"
(niska tonalność); podniesienie tego progu pozwala większej liczbie pasm, nawet nieco
tonalnym, zakwalifikować się jako szum, więc podstawianie szumu robi się SZERSZE (więcej
widma zastąpione tanim szumem, większa oszczędność, ale i większe ryzyko rozmycia
prawdziwych tonów). Obniżenie czyni PNS bardziej wybrednym. `1.0` = domyślnie.

`--pns-refpower <x>` skaluje próg mocy referencyjnej detekcji. PNS porównuje moc każdego pasma
z referencją, zanim uzna je za kandydata na szum; to pokrętło przesuwa ten warunek mocy.
`1.0` = domyślnie; działa w parze z tonalnością, bo oba warunki muszą się zgodzić, żeby pasmo
stało się PNS.

`--pns-gapfill <x>` skaluje próg wypełniania luk. Gdy PNS oznaczył pasma po obu stronach małej
luki, ta heurystyka może „domknąć lukę" i oznaczyć jako PNS także pasmo pomiędzy nimi, żeby nie
zostawić zakodowanej wyspy uwięzionej między dwoma pasmami szumu. Zaawansowane i subtelne,
rzadko słyszalne samo w sobie. `1.0` = domyślnie.

`--pns-min-width <n>` ustala minimalną szerokość SFB, w liniach widmowych, jaką musi mieć pasmo,
zanim PNS będzie mógł na nim zadziałać. To jedyny z tej grupy, który jest surową liczbą linii,
a nie mnożnikiem `×100`. Jest skuteczny dopiero powyżej wbudowanej domyślnej (LC = 16 linii);
podbicie do 32 albo 64 ogranicza PNS tylko do szerszych pasm, powstrzymując koder przed
podstawianiem szumu na wąskich niskich pasmach, gdzie byłoby to bardziej słyszalne.
`-1` = off (użyj domyślnej FDK).

### Transjenty, pre-echo i dziury w rdzeniu (LC, LD, ELD)

Przy niskim bitrate z szerokim pasmem (np. LC stereo 32 kbps i `-w 17000`) rdzeń nie ma
bitów na wszystkie pasma. Słychać to na dwa sposoby: szum rozmazany przed uderzeniem
(pre-echo) i bulgocące, migające pasma, które co chwilę znikają (dziury). Te pokrętła
sięgają do obu mechanizmów. Bez nich koder działa jak fabryczny FDK.

DZIURY. `--hole-ladder <1..8>` to najmocniejsze pokrętło. Gdy ramki nie stać na wszystko,
FDK zeruje pasma w 8 krokach, od najcichszych w górę. Mniej kroków znaczy, że wolno zerować
tylko najcichsze, a resztę oszczędności koder robi grubszą kwantyzacją. Zmierzone przy 32
kbps i 17 kHz: wyzerowanych pasm ubywa z 37% do 8% (`2`), przy 40 kbps z 37% do 1,7%. W
zamian pozostałe pasma mają trochę więcej szumu, czyli zamiast dziur pojawia się nieco
bardziej ziarniste wypełnienie. Warto zacząć od `2`. `--avoid-holes 1` włącza ochronę
wąskich głośnych pasm, którą FDK wyłącza poniżej 40 kbps stereo. `--hole-start <sfb>` chroni
dolne pasma przed zerowaniem (wyżej = więcej chronionych), `--hole-start-short` to samo dla
bloków short, a `--hole-spread <dB>` to precyzyjna korekta tego, kiedy pasmo uznajemy za
przykryte przez sąsiadów.

PRE-ECHO. Najmocniej działa `--window-shape 1`: wszystkie bloki, także krótkie, dostają okno
KBD, które słabiej przepuszcza szum daleko od dźwięku. Zmierzone: pre-echo średnio niższe o
3,4 do 6,1 dB przy 24, 40 i 128 kbps. `--window-shape 0` daje odwrotnie, sinus wszędzie.
`--attack-decay <x>` (fabrycznie 0,7) ustawia pamięć detektora uderzeń: wyżej porównuje atak
z dłuższą średnią, w pomiarze `0,9` obniżyło najgorszy przypadek pre-echa o 2,9 dB.
`--attack-floor <dB>` w dół sprawia, że także ciche ataki, np. szczotki, przełączają na
bloki krótkie. `--short-groups <0..3>` mówi, jak 8 krótkich okien dzieli się na grupy: `1`
jedna grupa (najtaniej), `2` równe pary, `3` ciasno wokół uderzenia. `--preecho-rise <dB>` i
`--preecho-depth <dB>` pilnują pre-echa wewnątrz długich bloków; w LC działają słabo, bo
prawdziwe ataki i tak trafiają do bloków krótkich, ale w LD i ELD, które bloków krótkich nie
mają, są jednym z niewielu narzędzi. Tam drugim jest `--tns-thresh <x>`: poniżej 1 TNS
włącza się częściej i kształtuje szum w czasie. `--tns-start <Hz>` ustala, jak nisko sięga
dolny filtr TNS.

Na start dla LC stereo 24 do 50 kbps z szerokim pasmem:

    fdkaac-franken-x64.exe -p 2 -b 32000 -w 17000 --hole-ladder 2 --avoid-holes 1 --window-shape 1 -o out.m4a in.wav

`--ath-scale <n>`: skalowanie progu słyszalności (ATH), w Q8 (256 = ×1,0).
To GLOBALNY regulator maskowania. Powyżej 256 podnosi progi, więc koder uznaje
więcej rzeczy za niesłyszalne, działa agresywniej i zużywa mniej bitów. Poniżej
256 obniża progi, więc koder zachowuje więcej detalu za cenę większej liczby
bitów. To najprostszy sposób powiedzieć koderowi "bądź czystszy" albo "bądź
oszczędniejszy".

---
# Część V. Detale w wysokich bitrate i dostrajanie do mowy

## 18. "Więcej bitów = więcej detalu": jak to naprawdę wycisnąć

Pytanie, które często pada: czy przy 400 kbps koder naprawdę wykorzysta te bity na lepszy
opis detali, czy część się marnuje? W AAC nie ma tak zaawansowanych, osobnych regulatorów
jak w MusePack (signal-to-mask ratio, tone-masks-noise, noise-masks-tone jako oddzielne
pokrętła). Efekt "opisz dokładniej, mniej maskuj" osiągamy jednak dwoma narzędziami, które
razem robią dokładnie to samo.

`--ath-scale <n>` PONIŻEJ 256 obniża globalny próg słyszalności. Koder
przestaje zakładać, że coś jest niesłyszalne, i zaczyna to dokładnie kodować.
To najbliższy odpowiednik obniżenia progu maskowania z MusePacka. Przy 320-400
kbps warto spróbować 200 albo 180.

`--spread-mask <n>` PONIŻEJ 256 steruje ROZLEWANIEM maskowania między
sąsiednimi pasmami. W psychoakustyce sygnał maskujący "rozlewa" swoją zdolność
maskowania na sąsiadów (to właśnie tone-masks-noise). Zmniejszenie tego parametru
każe koderowi mniej zakładać, że sąsiednie pasma się nawzajem maskują, przez co
więcej pasm jest traktowanych jako słyszalne i dokładnie kodowanych.
Największy efekt tam, gdzie bity są ograniczeniem (96-192 kbps); przy bardzo
wysokim bitrate koder i tak ma nadmiar bitów, więc zmiana bywa mała.

PRZEPIS audiofilski (maksimum detalu, wysoki bitrate, materiał z bogatą górą):

    fdkaac-franken-x64.exe -p 2 -b 400000 --ath-scale 190 --spread-mask 128 -o out.m4a in.wav

Połączenie: obniżony globalny próg (ath-scale 190) oraz zmniejszone rozlewanie maskowania
(spread-mask 128). Koder zachowuje znacznie więcej mikrodetalu, co w porównaniu fazowym z
oryginałem daje mniejsze różnice. To jest w duchu tego, co robił MusePack przy 1000 kbps, w
granicach tego, co AAC pozwala wystawić bez przepisywania całego modelu.

Gdy w materiale więcej budżetu ma pójść w szerokość stereo, warto dołożyć `--side-bias 6` albo
`--side-knee 6`: skierowanie bitów do kanału side to kolejne miejsce, w którym wysoki
bitrate daje naprawdę lepszy dźwięk.

Poza ath-scale jest jeszcze bardziej bezpośrednia dźwignia i warto zrozumieć jej jednostki. Te
pokrętła min-SNR działają w skali stałoprzecinkowej Q8, gdzie 256 oznacza 1,0 (więc 256 =
„zostaw w spokoju", 128 = ×0,5, 512 = ×2,0); `-1` oznacza, że przełącznik jest wyłączony.
`--minsnr-scale <n>` (Q8, 256 = off) to najbliższy FDK-owy odpowiednik gałek TMN/NMT z
MusePacka: skaluje WYMAGANY per-pasmo SNR kodowania (`sfbMinSnrLdData`), czyli minimalny stosunek
sygnału do szumu, jaki koder upiera się osiągnąć w każdym paśmie skali. Wartości PONIŻEJ 256
wymagają WYŻSZEGO SNR, czyli koder musi kodować każde pasmo dokładniej, co daje więcej detalu
za więcej bitów, a powyżej 256 rozluźniają wymóg i kodują zgrubniej. Jest
skuteczniejsza niż `--ath-scale`, bo to właśnie do min-SNR logika „avoid holes"
(unikania dziur) cofa progi; samo ruszenie kopii progu (jak robi ath-scale) jest
częściowo cofane dalej w łańcuchu, ale dolna granica min-SNR już nie.

Dwa przełączniki okna przesuwają końce przedziału, w którym FDK trzyma wymagany SNR.
Domyślnie żadne pasmo nie może żądać więcej niż około 25 dB SNR i żadne nie dostaje mniej
niż około 1 dB. `--minsnr-clamp-lo <n>` (Q8, 256 = domyślnie) przesuwa koniec wymagający:
poniżej 256 pasmo może żądać więcej, i dzięki temu niskie `--minsnr-scale` dalej działa.
`--minsnr-clamp-hi <n>` (Q8, 1..320) przesuwa koniec zgrubny: poniżej 256 nawet najmniej
ważne pasmo ma zagwarantowany wyższy SNR. Przy CBR pętla dopasowania odkręca oba prawie
całkowicie; przy VBR widać je w jakości i w rozmiarze pliku.

Na koniec `--reduce-clamp 0` zdejmuje sufit „29 dB Ratio" redukcji progów wewnątrz
kwantyzatora CBR. Gdy pętli alokacji bitów CBR brakuje bitów, podnosi (rozluźnia) progi, ale
fabryczny FDK nie pozwala zredukować progu o więcej niż około 29 dB względem referencji, bo
to clamp bezpieczeństwa. Ustawienie `--reduce-clamp 0` zdejmuje ten clamp, pozwalając
koderowi wepchnąć progi głębiej i wlać bity w najbardziej wymagające pasma. Łączy się
naturalnie z `--minsnr-scale` dla ekstremalnego detalu i dotyczy tylko CBR (VBR chodzi inną
ścieżką). Domyślnie jest `1` (clamp włączony, zachowanie fabryczne).

## 19. Dostrajanie do mowy ludzkiej

W HE-AAC, a konkretnie w warstwie SBR, istnieje osobny tryb strojenia pod mowę. W fabrycznym
FDK jest zaszyty na sztywno jako wyłączony, a tutaj został wystawiony flagą:

`--speech`: włącza strojenie SBR pod mowę ludzką. Zmienia progi filtrowania
odwrotnego, poziom szumu i wyłącza kodowanie parametryczne górnego pasma,
wszystko po to, żeby mowa (podcast, audiobook, dialog) w niskim bitrate brzmiała
naturalniej, bez "bulgotania" w górze. Dotyczy HE-AAC i HE-AAC v2, bo działa w
SBR.

    fdkaac-franken-x64.exe -p 5 -b 32000 --speech -o mowa.m4a podcast.wav

`--speech` zmienia kilka rzeczy naraz. Jego część dotyczącą estymatorów SBR
można teraz brać osobno: `--sbr-invf-table 1` daje samą tabelę decyzji mowy,
`--sbr-nf-weight 1.0` samą wagę szumu mowy, a `--sbr-softness 4` obie naraz, bez
zmiany crossovera i bez wyłączania kodowania parametrycznego.

Uczciwie o LC: czysty AAC-LC (bez SBR) NIE ma osobnego, oddzielnego "trybu mowy" w FDK.
Model psychoakustyczny LC jest ten sam dla mowy i muzyki. Dla mowy w LC zwykle pomaga węższe
pasmo (`-w 12000` do `14000`, bo mowa ma niewiele powyżej), a przy bardzo niskim bitrate'ie
`--pns 1` z `--force-pns`. Osobnego przełącznika mowy dla LC nie ma, bo nie ma go w samym
kodeku.

---

# Część VI. Gotowe przepisy (od skrajności do skrajności)

## 20. Zestaw praktycznych receptur

MUZYKA, WYSOKA JAKOŚĆ ARCHIWALNA (przezroczyste stereo):

    fdkaac-franken-x64.exe -p 2 -b 256000 --afterburner 1 -o out.m4a in.wav

MUZYKA, "ODDYCHAJĄCY" QUASI-VBR (nie tnie po kulminacjach):

    fdkaac-franken-x64.exe -p 2 -b 160000 --peak-bitrate 256000 --vbr-reservoir 16000 -o out.m4a in.wav

AUDIOFIL, PEŁNE PASMO 96 kHz + maksimum detalu:

    fdkaac-franken-x64.exe -p 2 -b 400000 --uncap-bandwidth --core-cutoff 40000 --ath-scale 190 --spread-mask 128 -o out.m4a in96k.wav

STREAMING MUZYKI, ŚREDNI BITRATE (prawdziwie kodowany rdzeń do 12 kHz zamiast 10,5):

    fdkaac-franken-x64.exe -p 5 -b 64000 --sbr-start 15 -o out.m4a in.wav

PODCAST / MOWA, NISKI BITRATE (strojenie pod mowę stawia SBR na 5,5 kHz; o indeks wyżej to 7 kHz):

    fdkaac-franken-x64.exe -p 5 -b 32000 --speech --sbr-start 10 -o out.m4a mowa.wav

SKRAJNIE NISKO (eksperyment, 8 kbps stereo):

    fdkaac-franken-x64.exe -p 29 -b 8000 --unlock-bitrate -o out.m4a in48k.wav

RATOWANIE STEREO PRZY NISKIM BITRACIE (agresywne intensity):

    fdkaac-franken-x64.exe -p 2 -b 96000 --is 1 --is-aggression 70 -o out.m4a in.wav

SZERSZE STEREO PRZY ŚREDNIM BITRATE'IE (przy 64 kbps side czystszy o 1,2 dB, mid gorszy o 1,1 dB):

    fdkaac-franken-x64.exe -p 2 -b 64000 --side-knee 4 -o out.m4a in.wav

## 21. Jak podejść do własnych eksperymentów

Zmieniaj JEDEN parametr naraz i słuchaj (albo porównuj widmo i fazę z oryginałem). Koder bez
żadnych flag Frankenstein zachowuje się dokładnie jak oryginalny FDK i to jest punkt
odniesienia. Każda flaga to świadome odchylenie od domyślnego, sprawdzonego zachowania.

Kolejność, w jakiej warto próbować przy problemach:
- "za mało detalu / za płasko" → przy VBR `--minsnr-scale` albo `--ath-scale` poniżej
  256 (płaci się rozmiarem pliku); przy CBR najpierw wyższy bitrate, te pokrętła ruszają
  tam niewiele.
- "stereo za wąskie, przestrzeń zapada się do środka" → `--side-knee 3..6` albo
  `--side-bias 3..9` (LC), `--sbr-stereo-mode 1` (górne pasmo HE-AAC), mniej agresywne IS.
- "metaliczne wysokie" → `--sbr-softness 4..6` albo `--sbr-invf-min 1`; wymuszone
  `--sbr-invf` ustala jeden poziom dla całego pliku i służy do diagnozy.
- "tnie po głośnych fragmentach" → `--peak-bitrate` + `--vbr-reservoir` (rozdział 8).
- "artefakty jak MP3 przy bardzo niskim bitrate" → `--force-pns`.
- "potrzebne pełne pasmo z 96 kHz" → `--uncap-bandwidth -w 40000` (albo `--core-cutoff`).
- "rdzeń pod SBR brzmi wąsko" → `--sbr-start` o jeden indeks wyżej, nie `--core-cutoff`.

## 22. Uwaga końcowa o rzetelności

Wszystkie opisane tu zachowania zostały sprawdzone pomiarowo (dekodowalność strumienia,
realny rozrzut bitrate, zawartość widma), a nie tylko założone. Tam, gdzie coś ma
ograniczenie (brak trybu mowy w LC, sufit 6144 bity na kanał, progi bitrate'u poszczególnych
profili, które zdejmuje dopiero `--unlock-bitrate`), jest to powiedziane wprost, zamiast
obiecywać rzeczy, których kodek nie potrafi.

Koder bez flag to czysty FDK. Flagi to świadome decyzje realizatora. Miłego strojenia.


# Część VII. Tabele referencyjne

Trzy tabele orientacyjne wyliczone z tablic strojenia w kodzie FDK. Są PRZYBLIŻONE (siatka
pasm jest schodkowa), ale pokazują właściwy rząd wielkości i pomagają świadomie dobrać
ustawienia bez zgadywania.

## 23. Tabela 1: górna częstotliwość pasma (SFB)

Widmo jest podzielone na pasma (scale factor bands, SFB), numerowane od dołu: pasmo 0 to
najniższy bas, im wyżej numer, tym wyżej w częstotliwości. Przy ograniczaniu czegoś do "N
pasm" (`--msbands`, `--isbands`) ta tabela mówi, jakiej częstotliwości to mniej więcej
odpowiada. Pokazano co czwarte pasmo; ostatni wiersz to łączna liczba pasm i częstotliwość
Nyquista (połowa próbkowania).

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
| 44  | brak | 9647 | 13000| 17916 | 19500 | brak  |
| 48  | brak | brak | 15000| 22050 | 24000 | brak  |
| liczba pasm / Nyquist | 43 / 8000 | 47 / 11025 | 51 / 16000 | 49 / 22050 | 49 / 24000 | 41 / 48000 |

Przykład odczytu: przy 44,1 kHz pasmo nr 40 kończy się około 15,2 kHz. Gdy MS ma działać
tylko poniżej ~15 kHz (przypadek techniczny, zobacz rozdział 13), należy ustawić
`--msbands 40`.

## 24. Tabela 2: indeks startu SBR a pasmo rdzenia

W HE-AAC dolną część pasma koduje rdzeń AAC, a górną odtwarza SBR. `--sbr-start` (indeks
0..15) decyduje, gdzie zaczyna się SBR, a rdzeń idzie za nim: niższy indeks daje węższy
rdzeń. Tabela podaje pasmo rdzenia, które wypisuje `--verbose` przy HE-AAC 32 kbps stereo
dla trzech częstotliwości wejścia. "x" oznacza, że koder nie zainicjalizował się z domyślnym
stopem. Indeks przechodzi przez tablicę, a nie wzór, więc sąsiednie indeksy mogą dawać tę
samą wartość.

| indeks startu | wejście 32 kHz | wejście 44,1 kHz | wejście 48 kHz |
|----:|----:|----:|----:|
| 0 | x | x | x |
| 1 | x | x | x |
| 2 | 3562 | 3919 | x |
| 3 | 3843 | 4263 | 4265 |
| 4 | 4156 | 4651 | 4640 |
| 5 | 4468 | 5038 | 5062 |
| 6 | 4812 | 5469 | 5484 |
| 7 | 4812 | 5943 | 5953 |
| 8 | 5187 | 5943 | 6468 |
| 9 | 5593 | 6459 | 6468 |
| 10 | 5593 | 7019 | 7031 |
| 11 | 6031 | 7579 | 7640 |
| 12 | 6500 | 8268 | 8250 |
| 13 | 7000 | 8957 | 9000 |
| 14 | 7500 | 9646 | 10500 |
| 15 | 8000 | 11025 | 12000 |

To, które indeksy są dozwolone, zależy też od bitrate'u: przy 64 kbps z 48 kHz inicjalizuje
się tylko górna część zakresu (zobacz kartę `--sbr-start` w opisie przełączników). Górną
granicę SBR ustawia `--sbr-stop` (0..13); wyższy indeks to SBR sięgające wyżej. Domyślnie
biblioteka dobiera oba indeksy pod bitrate.

## 25. Tabela 3: AAC-LC, automatyczne odcięcie pasma według bitrate'u

Bez podanego `-w` koder sam dobiera szerokość pasma z tej tablicy, według bitrate NA KANAŁ
(stereo 128 kbps to 64 kbps na kanał). Tabela jest wspólna dla 32/44,1/48 kHz i wyższych;
próbkowanie wpływa tylko na górny limit (Nyquista).

| bitrate na kanał | pasmo mono | pasmo stereo i więcej |
|----:|----:|----:|
| do 12 kbps | 3700  | 5000  |
| 20 kbps    | 6900  | 9640  |
| 28 kbps    | 9600  | 13050 |
| 40 kbps    | 12060 | 14260 |
| 56 kbps    | 13950 | 15500 |
| 72 kbps    | 14200 | 16120 |
| 96 kbps i więcej | 17000 | 17000 |

Wniosek praktyczny: w czystym AAC-LC (bez SBR) automat i tak zatrzyma się około 17 kHz.
Podawanie `-w` wyżej ma sens tylko przy dużym zapasie bitrate; pełne pasmo powyżej 20 kHz
wymaga `--uncap-bandwidth` i próbkowania co najmniej 96 kHz.

## 26. Jak czytać `--verbose`

`--verbose` wypisuje surowe wartości bez podpowiedzi w nawiasach, żeby odczyt był
czysty. Oto co oznaczają te mniej oczywiste:

- AOT (profil): 2 = AAC-LC, 5 = HE-AAC, 29 = HE-AAC v2, 23 = AAC-LD, 39 = AAC-ELD.
- bitrate-mode: 0 = CBR, 1 do 5 = VBR (wyżej = lepsza jakość).
- channel-mode: 1 = mono, 2 = stereo. W HE-AAC v2 rdzeń jest mono, a stereo
  odtwarza się z parametrów (Parametric Stereo).
- core bandwidth: górna częstotliwość rdzenia AAC. W nawiasie podane jest ŹRÓDŁO:
  "from -w" (wartość podana ręcznie), "from --core-cutoff" albo "auto". Pod SBR to
  tylko rdzeń, bo SBR gra powyżej tej wartości.
- final BW (AAC+SBR): pokazywane tylko przy aktywnym SBR, orientacyjna górna
  częstotliwość CAŁEGO sygnału (rdzeń plus SBR), wyliczona z indeksu stopu SBR.
  To dopełnienie "core bandwidth": tamto mówi dokąd sięga sam rdzeń, a to dokąd
  sięga cały HE-AAC po dołożeniu SBR.
- sbr-ratio: 1 = single-rate (downsampled), 2 = dual-rate (rdzeń na połowie
  częstotliwości wyjścia).
- sbr amp res: 0 = rozdzielczość 1,5 dB, 1 = 3,0 dB.
- codec delay: opóźnienie kodeka w próbkach na kanał, istotne przy gapless.
- IS corr threshold, IS L/R ratio: progi w skali Q8, gdzie 256 = 1,0. Niższy próg
  korelacji oznacza, że intensity stereo włącza się CHĘTNIEJ (odwrotnie do
  intuicji).
- franken overrides applied: lista przełączników, które w tym uruchomieniu
  odbiegają od czystego FDK (albo "none", gdy nic nie zostało zmienione).

Wartości w skali Q8 (jak progi IS, `--ms-precision`, `--ms-bias`, `--ath-scale`,
`--spread-mask`) to liczby, gdzie 256 znaczy 1,0, a na przykład 243 to około 0,95.


# Część VIII. Jak wycisnąć najwięcej ze skrajnego bitrate

## 27. Na co idą bity i co da się odzyskać

Przy 14 do 24 kbps ramka jest tak mała, że to, co otacza muzykę, waży tyle co sama muzyka.
`--bit-stats` wypisuje na końcu kodowania, jak dzielą się zapisane ramki. Dla stereo LC przy
14 kbps i 48 kHz ramka ma około 307 bitów: nagłówek ADTS zajmuje 56 (LOAS 41 do 44), nagłówek
pary kanałów i wyrównanie około 47, sekcje 12 do 16%, współczynniki skali 8 do 12%, maska MS 2
bity albo 22 do 27, gdy jest zapisywana pasmo po paśmie. Informacja poboczna razem to 35 do 47%
ramki; na widmo zostaje tylko 35 do 46%. Tabela nie zmienia strumienia, więc można ją dopisać do
każdego polecenia.

Z tego pomiaru wynikają trzy dźwignie:

- Intensity stereo jest przy tych prędkościach drogie. Wymusza maskę MS pasmo po paśmie i
  dokłada pozycje IS. `--is-admit 0` nie pozwala FDK go użyć (fabrycznie jest
  dopuszczone, gdy bitrate podzielony przez pasmo jest mniejszy niż 5); na gęstym
  materiale przy 14 kbps udział
  wyzerowanych pasm spadł z 85% do 70%.
- `--side-cutoff <Hz>` zamienia górne pasma w czysty mid. Opłaca się bardziej przy średnich
  prędkościach (48 kbps) niż przy 14 kbps, gdzie side i tak w większości znika.
- Wygładzanie współczynników skali się nie opłaca: kosztują już 1,3 do 1,5 bitu na pasmo, a
  nawet przy wszystkich różnicach równych zeru zysk wyniósłby 7 do 13 bitów na ramkę.

## 28. Szum zamiast dziur: PNS, który naprawdę oszczędza

Podstawianie szumu (PNS) zastępuje pasmo szumem o właściwym poziomie. W parze stereo poziom
szumu jest zapisywany dla każdego kanału osobno; format nie ma wspólnego poziomu. Para może
współdzielić sam szum: flaga na masce MS mówi dekoderowi, żeby w obu kanałach użył tego
samego szumu.

Wymuszony PNS przy niskim bitrate (na przykład `--pns-above 2200` przy 24 kbps) trzeszczał w
rytmie ramek. Przyczyną było księgowanie bitów FDK: planuje ono pasmo PNS jako darmowe, choć
każdy poziom szumu kosztuje bity w strumieniu. Część ramek przekraczała wtedy budżet, a
awaryjne cięcie zdejmowało pasma od góry, przez obszar szumu i dalej w kodowane widmo, aż do
około 1,5 kHz. Trzy przełączniki to naprawiają:

- `--pns-budget 1` księguje poziomy szumu w celu bitowym i pozwala awaryjnemu cięciu odzyskać
  ich bity. W opisanym przypadku liczba obciętych ramek spadła z 42 na 766 do 1.
- `--pns-shared 1` używa jednego wspólnego szumu we wszystkich pasmach, które oba kanały kodują
  jako PNS, więc maska MS może wrócić do postaci 2-bitowej (z 28 do 2 bitów na ramkę na
  materiale skorelowanym). `0` daje zamiast tego niezależny szum w każdym kanale.
- `--pns-hold <dB>` pozwala sąsiednim pasmom szumu dzielić jeden poziom, więc obwiednia szumu
  kosztuje mniej (z 94 do 70 bitów na ramkę przy 6 dB).

Z `--pns-budget 1 --pns-shared 1` modulacja pasma szumu w rytmie ramek spadła z 19,7 dB do
8,3 dB, poniżej 11,6 dB tego samego pliku bez PNS. Dobry punkt wyjścia dla każdego
wymuszonego PNS przy niskim bitrate to `--pns-budget 1 --pns-shared 1 --pns-hold 3`. ffmpeg
6.1.1 gra wspólny szum jako niezależny; dekoder Fraunhofera gra go jako wspólny.

## 29. Pokrycie albo precyzja

`--coverage -5..5` przesuwa jedną równowagę jedną liczbą: plus koduje każde pasmo grubo i wypełnia
wszystko, minus koduje mniej pasm czysto i dopuszcza dziury. Steruje `--band-knee`,
`--quant-round`, `--thr-hold`, `--scf-range`, a przy +4 i +5 także `--pns-above`; każdy z nich
można też ustawić osobno i wtedy wygrywa z makrem. Przy LC 32 kbps i 17 kHz `-5` zostawiło puste
51% pasm, a reszta była o 4,75 dB czystsza, ustawienie fabryczne 11,7% i 1,96 dB, `+4` 0,2% dziur.
`--enfac-patch 0|1` wyłącza przeważanie płaskich, szumowych ramek w FDK albo włącza je dla każdej
ramki; w CBR to drobne pokrętło (najwyżej 1 punkt dziur), w VBR zmienia rozmiar pliku nawet o 29%.

## 30. Częstotliwość próbkowania jako ustawienie jakości: `--resample`

Przy niskim bitrate i wąskim paśmie częstotliwość próbkowania rdzenia sama jest ustawieniem
jakości. Rdzeń 48 kHz dzieli widmo na pasma współczynników skali pomyślane pod dźwięk do 20
kHz; poniżej odcięcia 9,5 kHz tylko część z nich cokolwiek niesie, każde kosztuje informację
poboczną, a ramka trwa 21 ms. Przy 32 kHz to samo odcięcie pokrywa więcej węższych pasm, a
ramka trwa 32 ms; przy 24 kHz 43 ms. `--resample <częstotliwość>` przelicza wejście, zanim
zobaczy je koder, więc cały koder pracuje w nowej częstotliwości.

Zmierzone przy AAC-LC 32 kbps stereo z `-w 9500` (błąd w 200..9000 Hz względem oryginału,
niżej znaczy czyściej), 48 / 32 / 24 kHz: długa muzyka -7,3 / -8,2 / -9,0 dB przy dziurach
5,6 / 0,1 / 0%, muzyka mieszana -11,1 / -14,8 / -16,8 dB, szerokie stereo -7,6 / -8,4 / -9,1
dB, gęsty materiał -8,3 / -7,7 / -8,6 dB. Ceną jest rozdzielczość czasowa: blok długi przy
24 kHz obejmuje 85 ms, a pre-echo przed ostrymi atakami wzrosło z -33,9 dB przy 48 kHz do
-28,4 dB przy 32 kHz i -28,1 dB przy 24 kHz. Dla muzyki z perkusją zwykle lepszym
kompromisem jest 32 kHz, dla mowy i spokojnego materiału 24 kHz. Koder nigdy nie wybiera
częstotliwości sam; decyzję podejmuje realizator.

    fdkaac -p 2 -b 32000 -w 9500 --resample 32 -o out.m4a in48k.wav

Przelicznik nie dodaje opóźnienia ani słyszalnego śladu (płasko do 95% niższej
częstotliwości Nyquista, tłumienie 120 dB, zniekształcenia około -136 dB), działa na potoku
i na surowym wejściu, a materiał 44,1 kHz doprowadza też do `--dab`, które przyjmuje tylko
32 i 48 kHz.

# Część IX. Radio cyfrowe DAB+

## 31. Wyjście DAB+ (`--dab`, `--dab-label`)

Dotąd mowa była o plikach: MP4/M4A dla odtwarzaczy, ADTS do streamingu. DAB+ to coś innego,
bo to format naziemnego radia cyfrowego (standard ETSI TS 102 563), a ten koder potrafi
wyprodukować gotowy strumień DAB+ bezpośrednio, bez żadnego zewnętrznego przepakowywania.

Dlaczego nie można po prostu wziąć zwykłego pliku AAC i wysłać go w eter? Bo DAB+ niesie AAC
na swój własny sposób. Po pierwsze, transformata jest krótsza (960 próbek zamiast typowych
1024), żeby długość ramki pasowała do siatki czasowej radia. Po drugie, dźwięk pakowany jest
w **super-ramkę** trwającą dokładnie 120 milisekund. Po trzecie, i najważniejsze, eter to
środowisko wrogie: sygnał zanika, samochody wjeżdżają w tunele, zakłócenia przychodzą i
odchodzą. Dlatego DAB+ otula dźwięk dwiema warstwami ochrony: **firecode'em** (CRC Fire)
strzegącym nagłówka oraz kodowaniem korekcyjnym **Reed-Solomon**, konkretnie RS(120,110) nad
ciałem Galois GF(256), które pozwala odbiornikowi odtworzyć dane nawet wtedy, gdy część
sygnału dotrze uszkodzona. To wszystko koder robi sam.

Wychodzi z tego surowy strumień `.dabp`: jedna super-ramka po drugiej, nic więcej. To nie
plik do odtwarzacza multimediów, lecz surowiec, który połyka multiplekser DAB+. W praktyce
łańcuch wygląda tak:

    out.dabp → odr-dabmux → ETI → nadajnik (albo programowy odbiornik: welle.io, dablin)

`odr-dabmux` to multiplekser, który łączy kilka stacji w jeden zespół (ensemble) i
produkuje strumień ETI, a ten idzie potem albo do prawdziwego nadajnika, albo do
programowego odbiornika do testów.

### Włączenie: `--dab`

    fdkaac --dab -b 96 -o out.dabp input.wav

Przełącznik `--dab` przestawia koder w tryb wyjścia DAB+. Kilka reguł, które narzuca
standard, a których koder pilnuje:

- Częstotliwość próbkowania MUSI wynosić 32000 lub 48000 Hz. Nic innego nie jest
  w DAB+ legalne.
- Bitrate musi być wielokrotnością 8 kbps, od 8 do 192 kbps.
- Mono i stereo są dozwolone.

Ciekawe jest to, że profilu zwykle NIE wybiera się samodzielnie. Praktyka DAB+ (i narzędzie
referencyjne `odr-audioenc`) polega na wyprowadzeniu profilu AAC z bitrate i liczby kanałów,
tak aby niskie bitrate automatycznie sięgały po oszczędniejsze narzędzia:

- stereo przy 48 kbps lub niżej → **HE-AAC v2** (z Parametric Stereo),
- mono do 64k, albo stereo do 80k → **HE-AAC** (z SBR),
- powyżej → **AAC-LC** (zwykły, pełnopasmowy profil).

Więc `-b 96` na pliku stereo trafi na AAC-LC, `-b 64` na HE-AAC, a `-b 32` na HE-AAC v2, bez
potrzeby zajmowania się tym ręcznie. Profil nadal można wymusić przełącznikiem `-p` (2 = LC,
5 = HE-AAC, 29 = HE-AAC v2), tak samo jak w każdym innym trybie tego kodera. Wymuszony
profil daje ten sam strumień co dobór automatyczny dla tego profilu. Inne wartości są
odrzucane, tak samo `-p 29` na pliku mono.

    fdkaac --dab -b 64 -o out.dabp input.wav    # → HE-AAC automatycznie
    fdkaac --dab -b 32 -o out.dabp input.wav    # → HE-AAC v2 automatycznie

### Etykieta stacji: `--dab-label`

Odbiornik DAB+ pokazuje na wyświetlaczu tekst: nazwę stacji, a często i tytuł tego, co gra.
W DAB+ ten tekst podróżuje kanałem zwanym DLS (Dynamic Label Segment), wetkniętym jako dane
X-PAD wewnątrz super-ramki. Ten koder potrafi nieść etykietę STATYCZNĄ, czyli jeden stały
napis przez cały plik:

    fdkaac --dab -b 96 --dab-label "Radio DHT" -o out.dabp input.wav

Mieści się do około 48 znaków (trzy segmenty upakowane w jeden PAD). "Statyczna" to słowo
klucz: tekst nie zmienia się w trakcie pliku. Prawdziwie dynamiczna etykieta, aktualizowana
utwór po utworze, oraz pokaz slajdów MOT wrzucający okładkę albumu na ekran (zadanie
`ODR-PadEnc` w świecie OpenDigitalRadio) są w planach, ale jeszcze ich tu nie ma. I uwaga:
`--dab-label` bez `--dab` nic nie robi, bo etykieta jest ignorowana: nie ma super-ramki, do
której można by ją wstawić.

### Słowo o zaufaniu

Wszystko powyżej sprawdzono niezależnymi narzędziami, nie tylko na zasadzie "się
skompilowało". Strumienie dekodują się czysto w faad2 (dekoder stojący za dablin),
etykieta DLS odczytuje się poprawnie po stronie odbiornika, a wyjście AAC-LC jest
bit-identyczne z tym, co z tego samego wejścia produkuje referencyjny
`odr-audioenc`. Przetestowano dziewięć kombinacji (48 i 32 kHz, mono i stereo, we
wszystkich trzech profilach) i każda dekoduje się samodzielnie. Jak wszędzie w
tym koderze: bez `--dab` nic się nie zmienia, a zwykłe wyjście do pliku pozostaje
bit-identyczne z fabrycznym FDK.

---

# Część X. Słownik pojęć

Dla szybkiego przypomnienia: wszystkie terminy z tego manuala, wyjaśnione jednym lub dwoma
zdaniami, językiem dźwiękowca.

AAC (Advanced Audio Coding). Rodzina stratnych kodeków dźwięku z rodziny MPEG, następca MP3.
Lepsza jakość na bit niż MP3, zwłaszcza w niskich bitrate'ach.

AAC-LC (Low Complexity). Podstawowy profil AAC. Koduje pełne pasmo klasycznie. Wybór do
wysokich bitrate'ów i archiwizacji.

Afterburner. Dodatkowy, wolniejszy algorytm optymalizujący kwantyzację w FDK. Włączony daje
nieco lepszą jakość. Domyślnie i zalecanie włączony.

ATH (Absolute Threshold of Hearing). Próg słyszalności, czyli granica ciszy zależna od
częstotliwości, poniżej której ucho nie rejestruje dźwięku. Koder wyrzuca to, co jest
poniżej. W tym koderze regulowany przez `--ath-scale`.

Bitrate. Ilość danych na sekundę dźwięku, w kilobitach (kbps) lub bitach (bps). Wyższy daje
więcej miejsca na detale, czyli lepszą jakość (do pewnej granicy).

Bit reservoir (rezerwuar bitów). Bufor pozwalający pożyczać bity między ramkami. Dzięki
niemu nawet CBR lokalnie "oddycha". Podstawa naszego quasi-VBR.

CBR (Constant Bitrate). Stały bitrate. Przewidywalny rozmiar, wygodny do streamingu. W AAC i
tak lekko zmienny dzięki rezerwuarowi.

Coupling (SBR). Tryb stereo w warstwie SBR, w którym oba kanały dzielą wspólną obwiednię
plus opis poziomu: oszczędny, ale daje węższą scenę w górnym paśmie.

Cutoff. Górna granica pasma kodowanego przez rdzeń. Powyżej niej albo cisza, albo (w HE-AAC)
robotę przejmuje SBR.

HE-AAC (High Efficiency). AAC-LC plus SBR. Rdzeń koduje dół pasma, SBR odtwarza górę. Do
średnich i niskich bitrate'ów.

HE-AAC v2. HE-AAC plus Parametric Stereo. Do najniższych bitrate'ów stereo.

ICC (Inter-channel Coherence). Parametr PS opisujący, jak bardzo kanały są do siebie podobne
i spójne. Sterowany przez `--ps-icc`.

IID (Inter-channel Intensity Difference). Parametr PS opisujący różnicę głośności między
kanałami. Podstawowy nośnik obrazu stereo w PS.

Intensity Stereo (IS). Technika, w której wysokie pasma kodowane są jako jedna obwiednia
energii zamiast dwóch kanałów. Oszczędna, ale może zwęzić scenę. Sterowana suwakiem
`--is-aggression`.

Inverse filtering (filtrowanie odwrotne). Mechanizm SBR "wybielający" skopiowany materiał
górnego pasma, sterowany estymatorem tonalności. Mocniejsze daje mniej metaliczności.
Regulowane `--sbr-invf`.

IPD/OPD (różnice fazy w PS). Fabryczny FDK ich nie wysyła; ten build wysyła je na żądanie
(`--ps-ipd`, `--ps-opd`).

Kwantyzacja. Zaokrąglanie współczynników widma do skończonej dokładności. Im mniej bitów,
tym większy błąd (szum/dziury). Serce stratnej kompresji.

Maskowanie. Zjawisko, w którym sygnał maskujący sprawia, że cichsze dźwięki tuż obok (w
widmie lub w czasie) stają się niesłyszalne, czyli maskowane. Główne źródło oszczędności
bitów w AAC.

MDCT. Transformata przenosząca ramkę dźwięku z czasu do dziedziny częstotliwości. Na jej
wyniku pracuje cały model psychoakustyczny.

MS Stereo (Mid/Side). Kodowanie sumy (mid) i różnicy (side) zamiast L i R. Prawie darmowa
oszczędność, gdy kanały są podobne.

PNS (Perceptual Noise Substitution). Zastępuje szumowe pasma opisem "tu jest
szum o takiej energii" zamiast kodować je dokładnie. Sterowany `--pns`,
`--force-pns`.

Parametric Stereo (PS). Koduje jeden kanał + parametry (IID, ICC), z których dekoder
odtwarza stereo. Do najniższych bitrate'ów.

Profil (AOT, Audio Object Type). Wariant AAC wybierany przez `-p` (LC, HE, v2, LD, ELD).

Ramka. Porcja dźwięku (1024 próbki), którą koder przetwarza naraz.

SBR (Spectral Band Replication). Rekonstrukcja górnego pasma z dolnego plus niewielki opis.
Serce HE-AAC.

SFB (Scale Factor Band). Pasmo współczynników skali. Grupa sąsiednich współczynników widma,
dla której koder podejmuje wspólną decyzję o dokładności. Numerowane od dołu (0 = basy).

Spreading (rozlewanie maskowania). Rozprzestrzenianie zdolności maskowania na sąsiednie
pasma. Zmniejszane przez `--spread-mask` dla większego detalu.

Tabela tuningowa. Zestaw domyślnych ustawień SBR i PS dobieranych przez FDK według bitrate i
częstotliwości. Nasze flagi ją nadpisują.

TNS (Temporal Noise Shaping). Kształtuje szum kwantyzacji w czasie, żeby chował się pod
transjentami. Ważne dla perkusji i ostrych ataków.

Transjent. Ostry, krótki dźwięk (uderzenie, atak). Trudny dla kodeka, bo szum kwantyzacji
może "wyprzedzić" atak (pre-echo).

VBR (Variable Bitrate). Zmienny bitrate zależny od trudności materiału. W FDK słaby; nasz
quasi-VBR (rozdział 8) jest lepszą alternatywą.

---
Koniec słownika.
