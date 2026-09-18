# Onkyo RC-209S Wi-Fi IR Remote — instrukcja po polsku

[English version](README.md)


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
