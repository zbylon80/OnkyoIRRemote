# Onkyo RC-209S Wi-Fi IR Remote — instrukcja po polsku

[English version](README.md)

## Restart z diagnostyki

W panelu **Diagnostyka** przycisk **Restart ESP32** uruchamia ponownie pilot.
Nie wysyła POWER i zachowuje zapisane ustawienia. Po przyjęciu restartu odczekaj
około 15 sekund i wybierz **Odśwież dane**; zegar ponownie synchronizuje się przez
NTP. Restart jest niedostępny podczas OTA i oczekiwania na źródło pobudki.

## Budzik i wyłączenie

Panel **Budzik i wyłączenie** jest dostępny z pilotów Basic/Advanced oraz pod
adresem ESP32 z końcówką `/schedule`. Ikona zegarka w nagłówku strony Basic
otwiera panel WWW. W widżetach Windows i Android od wersji 0.4.0 zegarek w górnym
menu otwiera własny panel aplikacji, bez przeglądarki. Wszystkie klienty odczytują
i zapisują ten sam harmonogram ESP32. Podpowiedź ikony: **Budzik i wyłączenie**.

Panel ma tylko dwie niezależne opcje: **Włącz** — godzina, źródło i **Ustaw**,
oraz **Wyłącz** — godzina i **Ustaw**. Możesz ustawić jedną akcję lub obie.
Każda działa jeden raz: dziś, jeśli wybrana minuta jeszcze się nie rozpoczęła,
albo jutro. Pod przyciskiem pojawia się docelowa data i godzina oraz **×** do
anulowania. Domyślnie żadna akcja nie jest zaplanowana; godziny to 07:00 i 02:00.
Pobudka wybiera źródło 2 sekundy po POWER. Głośność pozostaje bez zmian. Do pobudki
wybierz źródło, które będzie grało, np. TUNER.

Zapis jest wspólny dla wszystkich klientów i pozostaje w pamięci ESP32 po
restarcie. Telefon, komputer i strona nie muszą pozostać włączone. ESP32 musi
mieć zasilanie; po każdym uruchomieniu synchronizuje czas przez NTP. Korzysta
z czasu polskiego i uwzględnia zmianę czasu. Po synchronizacji może wykonywać
harmonogram bez Internetu, dopóki nie zostanie zrestartowany.

POWER przełącza stan amplitunera. Wyłączenie zakłada, że amplituner już gra,
a pobudka — że jest wyłączony. Nie odczytujemy jego rzeczywistego stanu.
Pomijamy przegapione godziny oraz akcje podczas OTA, nauki IR i przytrzymania
przycisku. Po wykonaniu akcja sama się wyłącza i nie powtarza po restarcie lub
następnego dnia. Ponowne **Ustaw** planuje kolejną akcję. Godzina, której nie
ma podczas wiosennej zmiany czasu, zostaje pominięta.

## Wersjonowanie firmware

Numer wersji ma jedno źródło: [FirmwareVersion.h](firmware/OnkyoRemote/FirmwareVersion.h).
Jest widoczny na dole paneli Basic i Advanced, w logu uruchomienia na porcie
szeregowym oraz w odpowiedzi JSON z `GET /version`, np. `{"version":"1.1.0"}`.
Ten adres podaje wersję faktycznie działającą na ESP32.

Stosujemy `MAJOR.MINOR.PATCH`: PATCH zwiększamy przy poprawkach, MINOR przy nowych
zgodnych funkcjach, a MAJOR przy zmianach łamiących zgodność. Każde wydanie opisujemy
w [CHANGELOG.md](CHANGELOG.md). Zmiany przed wydaniem mogą zbierać się pod jednym
przygotowywanym numerem; po wydaniu kolejne zmiany dostają nowy numer.
Zmiana źródeł lub kompilacja nie aktualizuje urządzenia.

## Zabezpieczenie zgłaśniania

Krótkie naciśnięcie wysyła jedną komendę IR. Powtarzanie rusza tylko przy nadal
przytrzymanym przycisku, 350 ms po otrzymaniu odpowiedzi na pierwszą komendę.
W Basic i Advanced ESP32 zatrzymuje każde zgłaśnianie po trzech sekundach od
pierwszej komendy, niezależnie od podtrzymywania przez telefon lub komputer.
Przy osiągnięciu limitu przycisk także wizualnie się zwalnia, mimo trzymania palca.
Aby zgłaśniać dalej, trzeba puścić i ponownie nacisnąć przycisk.
Ściszanie i strojenie nie mają dodatkowego limitu przytrzymania; wszystkie akcje
nadal kończą się po ponad dwóch sekundach bez poprawnego podtrzymania.

Identyfikator akcji blokuje spóźnione rozpoczęcie i stare podtrzymanie; stare
zatrzymanie nie przerywa nowej akcji. Początkowe komendy i podtrzymania nie
gromadzą się przy wolnym połączeniu, a oczekiwanie na odpowiedź jest ograniczone
do dwóch sekund.
Nie ma odpytywania połączenia w tle. To zabezpieczenie ogranicza niekontrolowane
powtarzanie. IR nie przekazuje aktualnej głośności wzmacniacza, więc pilot nie
może narzucić konkretnego maksymalnego poziomu głośności.

Wspólny skrypt paneli znajduje się w
[VolumeControls.h](firmware/OnkyoRemote/VolumeControls.h) i jest udostępniany pod
`/volume.js`. Zasady ESP32 znajdują się w
[VolumeHoldSafety.h](firmware/OnkyoRemote/VolumeHoldSafety.h). `POST /volume/press`
z kierunkiem wysyła pojedynczą komendę i zwraca identyfikator używany następnie
przez `/volume/start`, `/volume/keepalive` i `/volume/stop`. Po wgraniu tej wersji
należy odświeżyć istniejące karty pilota; poprzednie żądania przytrzymania bez
identyfikatora są odrzucane.

## Diagnostyka stabilności

Od wersji 1.1.0 usypianie Wi-Fi jest wyłączone, aby zmniejszyć opóźnienia;
oznacza to większy pobór prądu przez radio. Link **Diagnostyka** pod numerem
wersji otwiera `/status`. Pod `/diagnostics` dostępne są te same dane w JSON.
Strona odczytuje dane po otwarciu i po ręcznym odświeżeniu.

Pokazuje wersję i czas działania, sygnał Wi-Fi w dBm, ustawienie usypiania,
liczbę połączeń i rozłączeń oraz kod i czas ostatniego rozłączenia od startu,
wolną i minimalną wolną pamięć oraz największy wolny blok, powód restartu,
najdłuższy obieg programu, synchronizację zegara i limity głośności.
Brak danych o sygnale lub rozłączeniu jest oznaczony jako `null` w JSON.
Liczniki i maksymalne czasy dotyczą bieżącego uruchomienia, nie trwałej historii.
Powód restartu pochodzi z SDK ESP32: restart programowy nie rozróżnia OTA od
nocnego restartu. Odczyty nie zerują czasu bezczynności, nie ujawniają danych
Wi-Fi ani OTA i nie wysyłają komend do wzmacniacza.

## Nocny restart

Od wersji 1.0.0 firmware szuka momentu na restart w oknie 03:00–05:00 czasu
polskiego, z uwzględnieniem czasu letniego i zimowego. Wymaga pełnych 30 minut
bezczynności i co najmniej 30 minut od uruchomienia. Jeżeli warunek nie zostanie
spełniony przed 05:00, restart tej nocy jest pomijany.

Komendy IR, podtrzymanie aktywnej regulacji głośności, puszczenie aktywnego
przycisku, obsługa nauki kodów i OTA zerują czas bezczynności. Samo otwarcie strony
lub odczyt statusu tego nie robi. Podczas OTA, nauki i aktywnego przytrzymania
restart jest blokowany. Data restartu jest zapisywana przed jego wykonaniem,
więc zaplanowany restart nastąpi najwyżej raz dziennie, również po odłączeniu
zasilania. Błąd zapisu do pamięci uniemożliwia zaplanowany restart.

Zegar synchronizuje się w tle przez `pool.ntp.org` i `time.nist.gov`. Bez poprawnej
synchronizacji po uruchomieniu zaplanowany restart pozostaje wyłączony, a pilot
działa normalnie. Sterowanie nie czeka na odpowiedź serwera czasu.
Stałe harmonogramu znajdują się w [DailyRestart.h](firmware/OnkyoRemote/DailyRestart.h).


## 1. Co jest potrzebne

- płytka **ESP32 DevKitC / ESP-WROOM-32**;
- nadajnik IR;
- odbiornik IR 38 kHz, używany podczas uczenia kodów z oryginalnego pilota;
- przewody połączeniowe;
- kabel USB do pierwszego wgrania firmware'u;
- komputer z Arduino IDE 2.x i telefon lub komputer w tej samej sieci Wi-Fi.

## 2. Podłączenie

| Moduł | Przewód | ESP32 | Funkcja |
| --- | --- | --- | --- |
| Odbiornik IR | czerwony | `3V3` | zasilanie |
| Odbiornik IR | czarny | `GND` | masa |
| Odbiornik IR | żółty | `GPIO27` | sygnał odbierany |
| Nadajnik IR | zielony | `VIN` / `5V` | zasilanie |
| Nadajnik IR | brązowy | `GND` | masa |
| Nadajnik IR | pomarańczowy | `GPIO26` | sygnał nadawany |

Wszystkie elementy muszą mieć wspólną masę (`GND`). Odbiornik IR jest zasilany z `3V3`; nie należy podłączać go do `5V`.

## 3. Przygotowanie Arduino IDE

1. Zainstaluj [Arduino IDE](https://www.arduino.cc/en/software).
2. W Menedżerze płytek zainstaluj pakiet **esp32 by Espressif Systems**.
3. W Menedżerze bibliotek zainstaluj **IRremote**.
4. Otwórz `firmware/OnkyoRemote/OnkyoRemote.ino`.
5. Wybierz płytkę **ESP32 Dev Module** oraz właściwy port USB.

## 4. Konfiguracja Wi-Fi i OTA

1. Skopiuj `firmware/OnkyoRemote/WiFiConfig.example.h` jako `WiFiConfig.h` w tym samym katalogu.
2. Uzupełnij lokalne dane Wi-Fi oraz własne, silne hasło OTA.
3. Nie dodawaj `WiFiConfig.h` do Git i nie wysyłaj go nikomu — zawiera hasła.

Plik jest już ignorowany przez `.gitignore`.

## 5. Pierwsze wgranie

1. Podłącz ESP32 przewodem USB.
2. Kliknij **Upload** w Arduino IDE.
3. Otwórz Monitor portu szeregowego z prędkością `115200` bodów.
4. Po połączeniu z Wi-Fi firmware wyświetli lokalny adres IP urządzenia.

Jeżeli router nadaje nowy adres po restarcie, sprawdź go w Monitorze portu szeregowego lub w panelu routera. Można też ustawić rezerwację DHCP dla ESP32 w routerze.

## 6. Używanie pilota

Telefon lub komputer musi być połączony z tą samą siecią Wi-Fi co ESP32.

| Panel | Adres | Zastosowanie |
| --- | --- | --- |
| Basic | `http://ADRES_ESP32/` | codzienne sterowanie |
| Advanced | `http://ADRES_ESP32/advanced` | pełny układ RC-209S |

Przykład dla obecnej instalacji: `http://192.168.1.46/` oraz `http://192.168.1.46/advanced`.

Skieruj nadajnik IR w stronę czujnika amplitunera. W Basic przyciski `◀` i `▶` obok `VIDEO-1` zmieniają zapisaną stację tunera.

## 7. Instalacja jako aplikacja

### Natywny widżet Android

Projekt [android/](android/README.md) zawiera aplikację z widżetem ekranu głównego
odtwarzającym przyciski Basic. Pozwala sterować ESP32 bez otwierania panelu WWW.
Głośność zmienia się o jeden krok na kliknięcie. Na Androidzie 16 (API 36) lub
nowszym przytrzymanie VOL−/VOL+ uruchamia powtarzanie do puszczenia, z limitem
3 sekund. Android 8–15 obsługuje pojedyncze kliknięcia. Widżet korzysta
z istniejącego protokołu firmware 1.1.0; aktualizacja firmware nie jest potrzebna.
Instrukcja budowy APK i instalacji znajduje się w dokumentacji Androida powyżej.

### Widżet na Windows PC

Projekt [windows/](windows/README.md) zawiera natywny pilot w małym oknie
na pulpicie Windows, z panelem Basic i przytrzymaniem VOL−/VOL+ (limit 3 sekund).
Można przesuwać okno, zmieniać rozmiar i przypiąć je nad innymi oknami.
Podczas działania aplikacji klawisze multimedialne VOL−/VOL+ i MUTE sterują
Onkyo, a PLAY/PAUSE wysyła POWER, także przy zminimalizowanym oknie.
Zamknięcie aplikacji przywraca standardowe funkcje klawiszy Windows.
Korzysta z firmware 1.1.0 bez aktualizacji ESP32. Instrukcja uruchomienia
wersji przenośnej i testów znajduje się w dokumentacji Windows powyżej.

### Android / Chrome

1. Otwórz panel w Chrome.
2. Otwórz menu `⋮`.
3. Wybierz **Zainstaluj aplikację** albo **Dodaj do ekranu głównego**.

### Windows / Chrome lub Edge

1. Otwórz panel w przeglądarce.
2. Użyj ikony instalacji aplikacji na pasku adresu albo menu przeglądarki.
3. Przypnij aplikację do menu Start lub paska zadań, jeśli chcesz.

## 8. Programowanie funkcji Advanced na nowym urządzeniu

Pełny zestaw 46 kodów RC-209S jest wpisany na stałe w firmware. Na nowym lub wyczyszczonym ESP32 wystarczy skonfigurować Wi-Fi i wgrać `OnkyoRemote.ino` — panel Advanced działa od razu, bez oryginalnego pilota i bez procedury nauki.

Tryb nauki pozostaje w kodzie tylko na wypadek przyszłej, świadomej zmiany przypisania. Ekran jest celowo ukryty w zwykłym użyciu. Aby go tymczasowo włączyć:

1. W `OnkyoRemote.ino` w funkcji `setup()` dodaj tymczasowo:

   ```cpp
   server.on("/remotelearn", HTTP_GET, handleRemoteLearnPage);
   ```

2. Wgraj firmware i otwórz `http://ADRES_ESP32/remotelearn`.
3. Wybierz funkcję na układzie ekranowym, skieruj oryginalny RC-209S na odbiornik IR i naciśnij odpowiadający przycisk.
4. Zielony znacznik oznacza zapisany kod. Taki kod nadpisuje odpowiadającą mu wartość wpisaną na stałe w firmware i pozostaje w pamięci ESP32 po restarcie.
5. Usuń lub ponownie zakomentuj wskazaną linię, wgraj firmware ponownie i używaj panelu Advanced.

Podczas nauki nadajnik IR ESP32 jest blokowany, aby amplituner nie otrzymał przypadkowej komendy.

## 9. Aktualizacja przez Wi-Fi (OTA)

Po pierwszym wgraniu przez USB kolejne aktualizacje można wykonywać przez lokalną sieć Wi-Fi. ESP32 musi być włączone i połączone z siecią.

W Arduino IDE wybierz port sieciowy urządzenia, zwykle widoczny jako `onkyo-remote at ADRES_IP`, a następnie kliknij **Upload**. Przy połączeniu OTA Arduino IDE poprosi o hasło zapisane w lokalnym `WiFiConfig.h`.

Jeśli aktualizacja OTA nie powiedzie się, uruchom ESP32 ponownie i spróbuj jeszcze raz. W razie potrzeby użyj USB — to najpewniejsza metoda odzyskania urządzenia.

## 10. Rozwiązywanie problemów

| Objaw | Co sprawdzić |
| --- | --- |
| Panel się nie otwiera | wspólna sieć Wi-Fi, aktualny IP, zasilanie ESP32 |
| ESP32 świeci, ale nie odpowiada | naciśnij `EN` / `RESET`, poczekaj na połączenie Wi-Fi |
| Amplituner nie reaguje | kierunek nadajnika, przewód `GPIO26`, wspólna masa |
| Nie działa odczyt z oryginalnego pilota | odbiornik na `GPIO27`, zasilanie `3V3`, wspólna masa |
| Funkcja Advanced nie działa | wgraj ponownie aktualny firmware; sprawdź też nadajnik IR i jego kierunek |

Pełna mapa kodów bieżącej instalacji znajduje się w sekcji [Mapa zapisanych kodów IR](#11-mapa-zapisanych-kodów-ir--saved-ir-code-map).

---
## 11. Mapa zapisanych kodów IR

Poniższa tabela jest kompletną konfiguracją wpisaną na stałe w firmware. Wartości szesnastkowe mają przedrostek `0x`.

| Function | Protocol | Address | Command | Bits |
| --- | --- | --- | --- | --- |
| POWER | NEC | `0x6DD2` | `0x04` | 32 |
| SLEEP | NEC | `0x6DD2` | `0x5D` | 32 |
| SPEAKERS MAIN | NEC | `0x6DD2` | `0x59` | 32 |
| SPEAKERS REMOTE | NEC | `0x6DD2` | `0x5A` | 32 |
| SIMUL SOURCE | NEC | `0x6DD2` | `0xCC` | 32 |
| SIMUL SOURCE UP | NEC | `0x6DD2` | `0xC2` | 32 |
| SIMUL SOURCE DOWN | NEC | `0x6DD2` | `0xC3` | 32 |
| TUNER | NEC | `0x6DD2` | `0x0B` | 32 |
| PHONO | NEC | `0x6DD2` | `0x0A` | 32 |
| CD (input) | NEC | `0x6DD2` | `0x09` | 32 |
| DIRECT | NEC | `0x6DD2` | `0x44` | 32 |
| VIDEO-1 | NEC | `0x6DD2` | `0x0F` | 32 |
| VIDEO-2 | NEC | `0x6DD2` | `0x0E` | 32 |
| TAPE-1 | NEC | `0x6DD2` | `0x08` | 32 |
| TAPE-2 | NEC | `0x6DD2` | `0x07` | 32 |
| CLASS | NEC | `0x6DD2` | `0x4A` | 32 |
| PRESET ◀ | NEC | `0x6DD2` | `0x01` | 32 |
| PRESET ▶ | NEC | `0x6DD2` | `0x00` | 32 |
| DECK A ◀ | NEC | `0x6DD2` | `0x4F` | 32 |
| DECK A ▶ | PulseDistance | `0x0000` | `0x0000` | 7 |
| DECK A REC / PAUSE | NEC | `0x6DD2` | `0x50` | 32 |
| DECK A STOP | NEC | `0x6DD2` | `0x4D` | 32 |
| DECK A REW | NEC | `0x6DD2` | `0x52` | 32 |
| DECK A FF | NEC | `0x6DD2` | `0x51` | 32 |
| DECK B ◀ | NEC | `0x6DD2` | `0x16` | 32 |
| DECK B ▶ | NEC | `0x6DD2` | `0x15` | 32 |
| DECK B REC / PAUSE | NEC | `0x6DD2` | `0x18` | 32 |
| DECK B STOP | NEC | `0x6DD2` | `0x13` | 32 |
| DECK B REW | NEC | `0x6DD2` | `0x1A` | 32 |
| DECK B FF | NEC | `0x6DD2` | `0x19` | 32 |
| CD PAUSE | NEC | `0x6DD2` | `0x1F` | 32 |
| CD PLAY | NEC | `0x6DD2` | `0x1B` | 32 |
| CD STOP | NEC | `0x6DD2` | `0x1C` | 32 |
| CD ◀◀ | NEC | `0x6DD2` | `0x1E` | 32 |
| CD ▶▶ | NEC | `0x6DD2` | `0x1D` | 32 |
| CENTER OFF/ON | NEC | `0x6DD2` | `0x98` | 32 |
| CENTER UP | NEC | `0x6DD2` | `0x80` | 32 |
| CENTER DOWN | NEC | `0x6DD2` | `0x81` | 32 |
| REAR LEVEL UP | NEC | `0x6DD2` | `0x42` | 32 |
| REAR LEVEL DOWN | NEC | `0x6DD2` | `0x43` | 32 |
| MUTING | NEC | `0x6DD2` | `0x05` | 32 |
| VOLUME UP | NEC | `0x6DD2` | `0x02` | 32 |
| VOLUME DOWN | NEC | `0x6DD2` | `0x03` | 32 |
| SURROUND MODE | NEC | `0x6DD2` | `0x4C` | 32 |
| DELAY TIME | NEC | `0x6DD2` | `0x53` | 32 |
| TEST | NEC | `0x6DD2` | `0x9A` | 32 |

## Pliki projektu

| Plik lub katalog | Zawartość |
| --- | --- |
| `firmware/OnkyoRemote/OnkyoRemote.ino` | firmware oraz wbudowane panele internetowe |
| `firmware/OnkyoRemote/WiFiConfig.example.h` | szablon konfiguracji bez haseł |
| `diagnostics/OnkyoIRReader/` | prosty szkic diagnostyczny odbioru IR |
| `diagnostics/OnkyoIRTest/` | szkic testowy nadawania i odbioru IR |
