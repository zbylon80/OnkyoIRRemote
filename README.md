# Pilot Onkyo — dokumentacja

Sterowany przez Wi-Fi pilot IR dla amplitunera **Onkyo TX-SV919PRO**, zbudowany na **ESP32 DevKitC / ESP-WROOM-32**.

Panel pilota jest dostępny wyłącznie w tej samej domowej sieci Wi-Fi co ESP32. Aktualny adres urządzenia to `http://192.168.1.46`; adres może się zmienić po restarcie routera.

## Co działa

- POWER
- regulacja głośności — przytrzymanie przycisku stale zmienia poziom
- MUTE
- wybór źródeł: TAPE-1, CD, PHONO, TUNER i VIDEO-1
- instalacja panelu jako aplikacji „Pilot Onkyo” na Androidzie oraz Windows
- aktualizacje firmware przez Wi-Fi (OTA)

Sterowanie strojeniem tunera i presetami **nie jest obecnie dostępne**. Główny przycisk `TUNER` nadal przełącza amplituner na radio.

## Podłączenie sprzętowe — ważne

| Moduł | Kolor przewodu | Funkcja | Podłączenie do ESP32 |
| --- | --- | --- | --- |
| Odbiornik IR | czerwony | VCC, zasilanie | `3V3` |
| Odbiornik IR | czarny | GND, masa | `GND` |
| Odbiornik IR | żółty | OUT, sygnał odbierany | `GPIO27` |
| Nadajnik IR | zielony | VCC, zasilanie | `VIN` / `5V` |
| Nadajnik IR | brązowy | GND, masa | `GND` |
| Nadajnik IR | pomarańczowy | DAT, sygnał nadawany | `GPIO26` |

Nie zamieniać zasilania odbiornika IR z zasilaniem nadajnika: odbiornik pracuje z `3V3`, a nadajnik jest podłączony do `VIN` / `5V`.

## Używanie pilota

1. Telefon lub komputer muszą być połączone z tą samą siecią Wi-Fi co ESP32.
2. Otwórz `http://192.168.1.46`.
3. Naciśnij przycisk odpowiadający funkcji amplitunera.
4. Nadajnik IR musi być skierowany na czujnik IR amplitunera.

## Aplikacja na telefonie i komputerze

### Android / Chrome

1. Otwórz adres pilota w Chrome.
2. Naciśnij `⋮`.
3. Wybierz **Zainstaluj aplikację** lub **Dodaj do ekranu głównego**.
4. Potwierdź nazwę „Pilot Onkyo”.

### Windows / Firefox

1. Otwórz adres pilota w Firefoxie.
2. Kliknij ikonę aplikacji internetowej po prawej stronie paska adresu.
3. Firefox doda „Pilot Onkyo” do menu Start i można przypiąć go do paska zadań.

## Aktualizacja przez Wi-Fi (OTA)

Po pierwszym wgraniu przez USB następne aktualizacje można wykonywać bez odłączania układu. ESP32 musi być włączone, połączone z Wi-Fi i dostępne pod swoim aktualnym adresem IP.

Konfiguracja Wi-Fi i hasło OTA znajdują się wyłącznie w lokalnym pliku `firmware/OnkyoRemote/WiFiConfig.h`.

Tego pliku nie wolno dodawać do Git ani udostępniać, ponieważ zawiera hasła. Szablon bez sekretów: `firmware/OnkyoRemote/WiFiConfig.example.h`.

## Pliki projektu

| Plik lub katalog | Zawartość |
| --- | --- |
| `firmware/OnkyoRemote/OnkyoRemote.ino` | działający firmware i panel WWW/PWA |
| `firmware/OnkyoRemote/WiFiConfig.example.h` | szablon konfiguracji Wi-Fi i OTA |
| `diagnostics/OnkyoIRReader/` | odczyt kodów z fizycznego pilota |
| `diagnostics/OnkyoIRTest/` | test nadawania i odbioru IR |

## Szybka diagnostyka

- Panel się nie otwiera: sprawdź, czy telefon/komputer jest w tej samej sieci Wi-Fi i czy adres IP urządzenia się nie zmienił.
- Amplituner nie reaguje: sprawdź kierunek nadajnika IR oraz przewód pomarańczowy (`GPIO26`).
- Nie można odczytać fizycznego pilota: sprawdź żółty przewód odbiornika (`GPIO27`) oraz zasilanie odbiornika `3V3`.
- Po zmianie okablowania zawsze sprawdź przede wszystkim masę: oba moduły muszą mieć wspólny `GND` z ESP32.
