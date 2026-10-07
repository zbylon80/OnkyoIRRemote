# Widżet Onkyo na Windows

Natywna aplikacja 0.4.2 z małym oknem na pulpicie. Odtwarza panel Basic:
POWER, VOL−/VOL+, MUTE, TAPE-1, CD, PHONO, TUNER, VIDEO-1 oraz poprzednia
i następna stacja. Nie zawiera panelu Advanced.

Ikona zegarka w górnym menu (podpowiedź **Budzik i wyłączenie**) otwiera własny
panel w oknie widżetu. **Włącz**: godzina 00–23, minuty, źródło i **Ustaw**;
**Wyłącz**: godzina, minuty i **Ustaw**. Kliknij dużą godzinę, aby otworzyć
ciemny edytor 24-godzinny. Cyfry można wpisać, zmieniać strzałkami lub
kółkiem myszy przy aktywnym polu; **Gotowe** zatwierdza tylko wybór czasu,
**Anuluj** / Esc go cofa. Dopiero **Ustaw** zapisuje harmonogram.
Każda akcja wykona się raz; zapisany termin
potwierdza ESP32, a **×** anuluje wybraną akcję. **← Pilot** wraca do przycisków.
Ustawienia są wspólne ze stroną WWW i Androidem (firmware 1.3.0 lub nowsze).
Harmonogram zapisuje i wykonuje ESP32, więc aplikację Windows można zamknąć.
**Odśwież** odczytuje zmiany z innych klientów. Po błędzie wymaga odświeżenia
przed kolejnym zapisem; nie ponawia żądań automatycznie. Brak synchronizacji
zegara blokuje ustawianie nowych akcji, ale pozwala anulować już zapisane.

## Uruchomienie

1. Rozpakuj `OnkyoRemote-Windows-0.4.2-x64.zip` do wybranego folderu.
2. Uruchom `OnkyoRemote.Windows.exe`. Wersja przenośna zawiera .NET i nie
   wymaga instalacji środowiska ani uprawnień administratora. Jest przeznaczona
   dla Windows 10/11 x64. EXE nie jest podpisany certyfikatem wydawcy.
3. Kliknij koło zębate, wpisz adres ESP32 i wybierz **Sprawdź połączenie**,
   potem **Zapisz i wróć**. Domyślny adres to `http://192.168.1.46`.
   Samo sprawdzenie odczytuje `GET /version` i nie zmienia zapisanego adresu.
4. Przeciągaj okno za napis ONKYO. Zmieniaj rozmiar za krawędź okna.
   Przycisk ◇/◆ włącza i wyłącza wyświetlanie nad innymi oknami.
5. Przycisk − minimalizuje pilot do paska zadań; × zamyka aplikację.

PC i ESP32 muszą mieć dostęp do tej samej sieci lokalnej. Ustawienia adresu,
przypięcia, rozmiaru i pozycji są zapisywane w
`%LOCALAPPDATA%\OnkyoRemote\widget.json`. Pilot nie uruchamia się automatycznie
z Windows. Kolejne otwarcie EXE nie tworzy drugiej instancji.

## Klawisze multimedialne klawiatury

Gdy aplikacja działa, przejmuje cztery standardowe klawisze Windows:

| Klawisz fizyczny | Polecenie Onkyo |
| --- | --- |
| VOL− | ściszanie, również z przytrzymaniem |
| VOL+ | podgłaśnianie, również z przytrzymaniem |
| MUTE | wyciszenie |
| PLAY/PAUSE | POWER — włączenie/wyłączenie wzmacniacza |

Sterowanie działa również po minimalizacji oraz podczas pracy w innych
oknach. Te cztery klawisze nie zmieniają wtedy głośności PC ani odtwarzania
multimediów. STOP, poprzedni i następny utwór zachowują standardowe funkcje.
POWER i MUTE są wysyłane raz na naciśnięcie, także przy przytrzymaniu klawisza.

Zamknięcie aplikacji przez × lub Alt+F4 usuwa przechwytywanie i przywraca
standardowe działanie Windows. Minimalizacja nadal pozostawia sterowanie
Onkyo aktywne. Aplikacja nie dodaje ikony przy zegarze. Jeśli klawisz był
już trzymany podczas otwierania pilota, jego dotychczasowa funkcja pozostaje
aktywna do puszczenia. Błąd sieci nie przełącza samoczynnie klawiszy z powrotem
na PC — decyduje o tym zamknięcie widżetu.

Obsługuje standardowe `VK_VOLUME_*` i `VK_MEDIA_PLAY_PAUSE`. Klawiatura lub
oprogramowanie producenta wysyłające wyłącznie własne komendy HID może
wymagać osobnego dopasowania. Przy uruchomieniu napis na dole potwierdza
przejęcie klawiszy albo pokazuje jego niepowodzenie. Obsługa używa publicznego
[WH_KEYBOARD_LL](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)
na osobnym wątku; callback filtruje tylko te cztery klawisze i kolejkuje ich
obsługę, bez HTTP, logowania czy zapisywania zwykłych naciśnięć.

## Głośność i potwierdzenia

- Kliknięcie VOL−/VOL+ daje jeden krok. Przytrzymanie lewego przycisku myszy
  przez 350 ms włącza powtarzanie. Puszczenie kończy sesję, również poza
  przyciskiem dzięki przechwytywaniu myszy.
- Tab wybiera przycisk. Na przyciskach głośności można przytrzymać spację lub
  Enter; powtarzanie klawiatury nie tworzy kolejnych sesji. Wywołanie przycisku
  przez narzędzie dostępności daje jeden krok.
- Utrata przechwytywania myszy, aktywności okna lub fokusu klawiatury oraz
  minimalizacja kończą lokalne przytrzymanie przycisku widżetu. Globalne
  klawisze głośności działają do puszczenia lub limitu, także w tle.
  Esc w aktywnym oknie i zamknięcie kończą każde przytrzymanie. Zamknięcie
  czeka na zakończenie bieżącego żądania i wysłanie znanego `/volume/stop`.
- Każde przytrzymanie ma limit 3 sekund. Można puścić i nacisnąć ponownie.
  Przy zabiciu procesu lub utracie sieci pozostaje watchdog ESP32: maksymalnie
  2 sekundy od ostatniego sygnału sesji. Nie ma automatycznego wznowienia.
- Korzysta z istniejących `/volume/press`, `/volume/start`, `/volume/keepalive`
  i `/volume/stop` w firmware 1.1.0. Aktualizacja firmware nie jest potrzebna.
  Podczas powtarzania keepalive jest wysyłane co około 500 ms.
- Pozostałe przyciski wysyłają `POST /command`. Nakładające się akcje są
  odrzucane, a przyciski są tymczasowo wyłączone bez przygaszania całego panelu.
  Wizualnie reaguje tylko naciskany przycisk. Nie ma automatycznych
  ponowień, przekierowań HTTP ani odpytywania w tle. Żądania głośności mają
  limit 400 ms, pozostałe 2 sekundy, a odpowiedź maksymalnie 2048 bajtów.
- **ESP32 przyjęło…** potwierdza odpowiedź HTTP 200 i `{"ok":true}`. Nie
  oznacza odczytu stanu amplitunera. Brak potwierdzenia nie dowodzi, że IR
  nie zostało wysłane. Aplikacja nie zawiera danych Wi-Fi ani OTA.

## Budowanie i testy

Potrzebne: Windows i .NET SDK 10. Bez dodatkowych bibliotek aplikacyjnych.
Z katalogu repozytorium:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File windows/build.ps1 -Test
powershell -NoProfile -ExecutionPolicy Bypass -File windows/build.ps1 -Portable
```

Zwykła kompilacja tworzy aplikację korzystającą z zainstalowanego .NET Desktop
Runtime 10 w `OnkyoRemote.Desktop/bin/Release/net10.0-windows/`.
`-Portable` pobiera pakiety środowiska dla `win-x64`, tworzy samodzielny EXE
w `artifacts/portable-0.4.2/` oraz ZIP w `artifacts/`. Pierwsza publikacja wymaga
dostępu do NuGet. Narzędzia lokalne i wyniki budowania są ignorowane przez Git.

`-Test` uruchamia własny runner bez pakietów testowych. Lokalna atrapa ESP32
nasłuchuje wyłącznie na loopback i losowym porcie. Testy nie kontaktują się
z fizycznym ESP32 i nie wysyłają IR. Sprawdzają protokół HTTP, błędy i timeout,
przytrzymanie, puszczenie przed odpowiedzią, limit 3 sekund, blokowanie
nakładających się poleceń oraz okno WPF i zapis ustawień. Testowe okno jest
tworzone na czas sprawdzenia, a potem zamykane; ustawienia testowe są osobne.
Runner sprawdza też mapowanie czterech klawiszy, przepuszczanie pozostałych,
brak wielokrotnego POWER przy trzymaniu, głośność przy zminimalizowanym oknie,
odrzucanie starych zdarzeń, własność sesji i zwolnienie natywnego hooka.
Nie wstrzykuje globalnych naciśnięć do Windows. Fizyczna klawiatura wymaga
sprawdzenia po uruchomieniu. Raport i obrazy okna trafiają do `artifacts/tests/`.
Testy budzika sprawdzają natywny panel w trzech rozmiarach, zakres 24-godzinny,
formularze HTTP, zachowanie drugiej akcji i jej roboczej godziny, anulowanie
bez NTP, blokowanie ręcznych komend podczas zapisu oraz błędy bez ponowień.

Ikona EXE i okna pochodzi z istniejącego `PWA_ICON` w firmware. Plik źródłowy
to `OnkyoRemote.Desktop/Assets/remote.svg`; wielorozmiarowy `remote.ico`
(16–256 px) odtworzysz przez `powershell -NoProfile -STA -File windows/tools/build-icon.ps1`.

Podstawy techniczne: [obsługa wejścia WPF](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/input-overview),
[publikacja pojedynczego pliku .NET](https://learn.microsoft.com/en-us/dotnet/core/deploying/single-file/overview).
