# Onkyo RC-209S Wi-Fi IR Remote

ESP32-based Wi-Fi infrared remote for the Onkyo RC-209S layout. It serves two local web panels:

- **Basic** — everyday controls: power, volume, mute, selected inputs and tuner preset arrows.
- **Advanced** — the complete RC-209S-style layout, including tuner, tape decks, CD and amplifier controls.

The remote works only inside the same local Wi-Fi network as the ESP32. It does not use a cloud service or require an Internet connection.

- [Instrukcja po polsku](#instrukcja-po-polsku)
- [English guide](#english-guide)

---

# Instrukcja po polsku

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

Kody Advanced są specyficzne dla konkretnego pilota. Aktualnie ekran nauki jest celowo ukryty po zakończeniu konfiguracji.

Na nowym ESP32:

1. W `OnkyoRemote.ino` w funkcji `setup()` dodaj tymczasowo:

   ```cpp
   server.on("/remotelearn", HTTP_GET, handleRemoteLearnPage);
   ```

2. Wgraj firmware i otwórz `http://ADRES_ESP32/remotelearn`.
3. Wybierz funkcję na układzie ekranowym, skieruj oryginalny RC-209S na odbiornik IR i naciśnij odpowiadający przycisk.
4. Zielony znacznik oznacza zapisany kod. Kody pozostają w pamięci ESP32 po restarcie.
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
| Funkcja Advanced nie działa | naucz ją ponownie z oryginalnego pilota |

Pełna mapa kodów bieżącej instalacji znajduje się w sekcji [Mapa zapisanych kodów IR](#11-mapa-zapisanych-kodów-ir--saved-ir-code-map).

---

# English guide

## 1. What you need

- an **ESP32 DevKitC / ESP-WROOM-32** board;
- an IR transmitter;
- a 38 kHz IR receiver for learning codes from the original remote;
- jumper wires;
- a USB cable for the first firmware upload;
- a computer with Arduino IDE 2.x and a phone or computer on the same Wi-Fi network.

## 2. Wiring

| Module | Wire | ESP32 pin | Purpose |
| --- | --- | --- | --- |
| IR receiver | red | `3V3` | power |
| IR receiver | black | `GND` | ground |
| IR receiver | yellow | `GPIO27` | received signal |
| IR transmitter | green | `VIN` / `5V` | power |
| IR transmitter | brown | `GND` | ground |
| IR transmitter | orange | `GPIO26` | transmitted signal |

All modules need a common ground. Power the IR receiver from `3V3`, not `5V`.

## 3. Prepare Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install **esp32 by Espressif Systems** from Boards Manager.
3. Install **IRremote** from Library Manager.
4. Open `firmware/OnkyoRemote/OnkyoRemote.ino`.
5. Select **ESP32 Dev Module** and the correct USB port.

## 4. Configure Wi-Fi and OTA

1. Copy `firmware/OnkyoRemote/WiFiConfig.example.h` to `WiFiConfig.h` in the same folder.
2. Enter your local Wi-Fi details and a strong OTA password.
3. Never commit or share `WiFiConfig.h`; it contains passwords.

The file is already excluded by `.gitignore`.

## 5. First upload

1. Connect the ESP32 over USB.
2. Click **Upload** in Arduino IDE.
3. Open Serial Monitor at `115200` baud.
4. After Wi-Fi connects, the firmware prints the device's local IP address.

The IP can change after a router restart. Check Serial Monitor, your router's DHCP list, or create a DHCP reservation for the ESP32.

## 6. Use the remote

Your phone or computer must use the same Wi-Fi network as the ESP32.

| Panel | Address | Purpose |
| --- | --- | --- |
| Basic | `http://ESP32_IP/` | everyday controls |
| Advanced | `http://ESP32_IP/advanced` | complete RC-209S-style layout |

For the current installation, use `http://192.168.1.46/` and `http://192.168.1.46/advanced`.

Point the IR transmitter toward the receiver on the amplifier. In Basic, the `◀` and `▶` buttons beside `VIDEO-1` switch saved tuner stations.

## 7. Install as an app

### Android / Chrome

1. Open the panel in Chrome.
2. Open the `⋮` menu.
3. Choose **Install app** or **Add to Home screen**.

### Windows / Chrome or Edge

1. Open the panel in the browser.
2. Use the install-app icon in the address bar or the browser menu.
3. Optionally pin the installed app to Start or the taskbar.

## 8. Learn Advanced functions on a new device

Advanced codes are specific to the physical remote. The learning page is intentionally hidden after setup.

For a new ESP32:

1. Temporarily add this line in `setup()` in `OnkyoRemote.ino`:

   ```cpp
   server.on("/remotelearn", HTTP_GET, handleRemoteLearnPage);
   ```

2. Upload the firmware and open `http://ESP32_IP/remotelearn`.
3. Select a function on the layout, aim the original RC-209S at the IR receiver, and press the matching physical button.
4. A green mark means the code was saved. Codes persist through restarts.
5. Remove or comment the line again, re-upload, and use the Advanced panel.

The ESP32 IR transmitter is disabled while learning, so the amplifier cannot receive an accidental command.

## 9. Wi-Fi firmware updates (OTA)

After the first USB upload, further firmware updates can be performed over the local Wi-Fi network. The ESP32 must be powered on and connected.

In Arduino IDE, select the network port, usually shown as `onkyo-remote at IP_ADDRESS`, then click **Upload**. Arduino IDE will request the OTA password from local `WiFiConfig.h`.

If OTA fails, restart the ESP32 and try again. USB remains the most reliable recovery method.

## 10. Troubleshooting

| Symptom | Check |
| --- | --- |
| The panel does not open | same Wi-Fi network, current IP address, ESP32 power |
| ESP32 is powered but unreachable | press `EN` / `RESET`, then wait for Wi-Fi reconnect |
| The amplifier does not react | transmitter direction, `GPIO26` wire, common ground |
| The original remote cannot be read | receiver on `GPIO27`, `3V3` power, common ground |
| An Advanced function does not work | learn the function from the original remote again |

The complete code map for this installation is in [Saved IR code map](#11-mapa-zapisanych-kodów-ir--saved-ir-code-map).

## 11. Mapa zapisanych kodów IR / Saved IR code map

Poniższa tabela jest kopią aktualnych przypisań odczytanych z pamięci ESP32. `built-in` oznacza kod wpisany w firmware, `legacy-learned` — kod zapisany przez wcześniejszą wersję nauki, a `learned` — pełny zapis z obecnego trybu nauki. Wartości szesnastkowe mają przedrostek `0x`.

The table below is a copy of the current assignments read from ESP32 storage. `built-in` means a firmware default, `legacy-learned` was saved by an earlier learning version, and `learned` is a full capture from the current learning mode. Hexadecimal values use the `0x` prefix.

| Function | Origin | Protocol | Address | Command | Bits |
| --- | --- | --- | --- | --- | --- |
| POWER | built-in | NEC | `0x6DD2` | `0x04` | 32 |
| SLEEP | legacy-learned | NEC | `0x6DD2` | `0x5D` | 32 |
| SPEAKERS MAIN | legacy-learned | NEC | `0x6DD2` | `0x59` | 32 |
| SPEAKERS REMOTE | legacy-learned | NEC | `0x6DD2` | `0x5A` | 32 |
| SIMUL SOURCE | legacy-learned | NEC | `0x6DD2` | `0xCC` | 32 |
| SIMUL SOURCE UP | legacy-learned | NEC | `0x6DD2` | `0xC2` | 32 |
| SIMUL SOURCE DOWN | legacy-learned | NEC | `0x6DD2` | `0xC3` | 32 |
| TUNER | built-in | NEC | `0x6DD2` | `0x0B` | 32 |
| PHONO | built-in | NEC | `0x6DD2` | `0x0A` | 32 |
| CD (input) | built-in | NEC | `0x6DD2` | `0x09` | 32 |
| DIRECT | legacy-learned | NEC | `0x6DD2` | `0x44` | 32 |
| VIDEO-1 | built-in | NEC | `0x6DD2` | `0x0F` | 32 |
| VIDEO-2 | learned | NEC | `0x6DD2` | `0x0E` | 32 |
| TAPE-1 | built-in | NEC | `0x6DD2` | `0x08` | 32 |
| TAPE-2 | legacy-learned | NEC | `0x6DD2` | `0x07` | 32 |
| CLASS | legacy-learned | NEC | `0x6DD2` | `0x4A` | 32 |
| PRESET ◀ | legacy-learned | NEC | `0x6DD2` | `0x01` | 32 |
| PRESET ▶ | learned | NEC | `0x6DD2` | `0x00` | 32 |
| DECK A ◀ | learned | NEC | `0x6DD2` | `0x4F` | 32 |
| DECK A ▶ | learned | PulseDistance | `0x0000` | `0x0000` | 7 |
| DECK A REC / PAUSE | learned | NEC | `0x6DD2` | `0x50` | 32 |
| DECK A STOP | learned | NEC | `0x6DD2` | `0x4D` | 32 |
| DECK A REW | learned | NEC | `0x6DD2` | `0x52` | 32 |
| DECK A FF | learned | NEC | `0x6DD2` | `0x51` | 32 |
| DECK B ◀ | learned | NEC | `0x6DD2` | `0x16` | 32 |
| DECK B ▶ | learned | NEC | `0x6DD2` | `0x15` | 32 |
| DECK B REC / PAUSE | learned | NEC | `0x6DD2` | `0x18` | 32 |
| DECK B STOP | learned | NEC | `0x6DD2` | `0x13` | 32 |
| DECK B REW | learned | NEC | `0x6DD2` | `0x1A` | 32 |
| DECK B FF | learned | NEC | `0x6DD2` | `0x19` | 32 |
| CD PAUSE | learned | NEC | `0x6DD2` | `0x1F` | 32 |
| CD PLAY | learned | NEC | `0x6DD2` | `0x1B` | 32 |
| CD STOP | learned | NEC | `0x6DD2` | `0x1C` | 32 |
| CD ◀◀ | learned | NEC | `0x6DD2` | `0x1E` | 32 |
| CD ▶▶ | learned | NEC | `0x6DD2` | `0x1D` | 32 |
| CENTER OFF/ON | learned | NEC | `0x6DD2` | `0x98` | 32 |
| CENTER UP | learned | NEC | `0x6DD2` | `0x80` | 32 |
| CENTER DOWN | learned | NEC | `0x6DD2` | `0x81` | 32 |
| REAR LEVEL UP | learned | NEC | `0x6DD2` | `0x42` | 32 |
| REAR LEVEL DOWN | learned | NEC | `0x6DD2` | `0x43` | 32 |
| MUTING | built-in | NEC | `0x6DD2` | `0x05` | 32 |
| VOLUME UP | built-in | NEC | `0x6DD2` | `0x02` | 32 |
| VOLUME DOWN | built-in | NEC | `0x6DD2` | `0x03` | 32 |
| SURROUND MODE | learned | NEC | `0x6DD2` | `0x4C` | 32 |
| DELAY TIME | learned | NEC | `0x6DD2` | `0x53` | 32 |
| TEST | learned | NEC | `0x6DD2` | `0x9A` | 32 |

## Project files

| File or folder | Contents |
| --- | --- |
| `firmware/OnkyoRemote/OnkyoRemote.ino` | firmware and embedded web panels |
| `firmware/OnkyoRemote/WiFiConfig.example.h` | password-free configuration template |
| `diagnostics/OnkyoIRReader/` | simple IR receive diagnostic sketch |
| `diagnostics/OnkyoIRTest/` | IR send/receive test sketch |
