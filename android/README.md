# Widżet Onkyo na Androida

Aplikacja 0.1.0 udostępnia natywny widżet ekranu głównego z przyciskami
panelu Basic. Wymaga Androida 8.0 lub nowszego. Ustawienia są po polsku,
oznaczenia klawiszy odpowiadają panelowi WWW.

## Instalacja

1. Przenieś `app/build/outputs/apk/debug/app-debug.apk` na telefon i otwórz plik.
   Jest to lokalny APK testowy podpisany kluczem debug tego komputera.
   Android może poprosić o zezwolenie na instalację z aplikacji otwierającej plik.
2. Otwórz **Pilot Onkyo**, wpisz adres ESP32 i wybierz **Sprawdź połączenie**.
   Domyślny adres tej instalacji to `http://192.168.1.46`.
3. Wybierz **Dodaj widżet do ekranu głównego**. Można też przytrzymać puste
   miejsce na ekranie głównym i wybrać **Widżety → Pilot Onkyo**.
4. W konfiguracji widżetu zatwierdź adres przez **Zapisz widżet**.
   Później zmień go ikoną koła zębatego na widżecie.

Telefon i ESP32 muszą mieć dostęp do tej samej sieci lokalnej. Można wpisać
sam IP, nazwę urządzenia lub adres HTTP z portem. Każdy widżet zapamiętuje
własny adres; zapisanie domyślnego adresu w aplikacji nie zmienia istniejących
widżetów.

Układ pionowy: POWER, VOLUME −/+, MUTE, TAPE-1/CD, PHONO/TUNER,
VIDEO-1 i ◀/▶ stacji tunera. Docelowy rozmiar to 4 × 5 pól, zależnie od programu
ekranu głównego. Minimum 250 × 500 dp zapewnia dostęp do wszystkich przycisków.
Widżet można powiększyć. Nie ma klawiszy ani linku Advanced.

## Działanie

- Przyciski wysyłają `POST /command` z polem formularza `name`, np.
  `name=VOL%2B`, bez otwierania aplikacji.
- Jedno kliknięcie głośności wysyła jedną komendę. Widżet nie uruchamia
  powtarzania; długie naciśnięcie służy Androidowi do obsługi widżetu.
- Podczas żądania przyciski widżetów tego samego urządzenia są wyłączone.
  Identyfikator bieżącego układu odrzuca starsze kliknięcia, także dostarczone
  przez Androida dopiero po zakończeniu poprzedniego żądania.
- Błąd, brak odpowiedzi i niepoprawny JSON pokazują **Brak potwierdzenia**.
  Nie ma ponowień: ESP32 mogło wysłać IR przed utratą odpowiedzi.
- **ESP32 przyjęło…** oznacza HTTP 200 i JSON `{"ok":true}`, a nie odczyt
  stanu amplitunera. IR nie przekazuje informacji o jego stanie ani głośności.
- Test połączenia odczytuje wyłącznie `GET /version`. Nie ma cyklicznego
  odpytywania ani stale działającej usługi w tle.
- Lokalny HTTP jest zgodny z obecnym firmware; nie wymaga jego zmiany.
  Aplikacja nie zawiera danych Wi-Fi ani OTA.

## Budowanie

Potrzebne: JDK 17–21, Android SDK z platformą 36 i Build Tools 35.0.0.
Projekt ma wrapper Gradle 8.14.3 i Android Gradle Plugin 8.11.0.
Pierwsza kompilacja wymaga pobrania zależności. Nie używa dodatkowych bibliotek
aplikacyjnych.

Z katalogu repozytorium na Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File android/build.ps1
```

Skrypt korzysta z `JAVA_HOME` albo JDK dołączonego do Android Studio.
`local.properties` tworzy z `ANDROID_HOME` albo SDK z `%LOCALAPPDATA%/Android/Sdk`.
Lokalna konfiguracja i wyniki budowy są ignorowane przez Git.
Po pobraniu zależności można użyć `-Offline`.

Na innych systemach skonfiguruj Android SDK oraz JDK i uruchom:

```sh
cd android
./gradlew assembleDebug lintDebug
```

Do dystrybucji wariantu release potrzebny jest własny trwały klucz podpisu.

## Testy

Uruchom emulator Androida, następnie:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File android/build.ps1 -WithTests
powershell -NoProfile -ExecutionPolicy Bypass -File android/test-widget.ps1 -DeviceSerial emulator-5560
```

Drugi skrypt instaluje APK i testy wyłącznie na wskazanym emulatorze. Tymczasowo
zezwala aplikacji na powiązanie testowego widżetu, a na końcu cofa zezwolenie.
Runner tworzy rzeczywisty `AppWidgetHost` i lokalny pozorowany serwer ESP32;
nie wysyła IR do fizycznego urządzenia.

Testuje 11 przypisań przycisków, minimalny i powiększony układ, odczyt wersji,
odrzucanie opóźnionych kliknięć, błąd HTTP, negatywny JSON, przekroczenie czasu
odpowiedzi i kolejną poprawną komendę. Wyniki i zrzuty ekranu trafiają
do `app/build/reports/widget-smoke/`.

Zweryfikowano w emulatorze Androida 16 (API 36.1) i przez `lintDebug`.
Fizyczny telefon i reakcja amplitunera wymagają sprawdzenia po instalacji.
Dokumentacja Androida: [widżety](https://developer.android.com/develop/ui/views/appwidgets/overview),
[konfiguracja](https://developer.android.com/develop/ui/views/appwidgets/configuration).
