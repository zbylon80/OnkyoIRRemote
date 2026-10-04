# Test dotyku widżetu

Oddzielna aplikacja testowa, włączana tylko przez `-PtouchProbe` lub
`android/build.ps1 -WithTouchProbe`. Nie jest zależnością aplikacji pilota.

Tryb domyślny testuje bazowy dokument Remote Compose przez prawdziwy
`AppWidgetHostView`, a następnie przez Pixel Launcher. Raport zawiera zdarzenia
naciśnięcia i puszczenia z czasem; przytrzymanie trwa 1,2 sekundy.

Tryb `android/test-touch-probe.ps1 -Production` testuje gotowy widżet pilota
z osobnego UID: przypinanie z adresem, zamkniętą/uśpioną aplikację, krótkie
dotknięcie i przytrzymanie VOL+/VOL− bez otwierania okna. Pozorowany serwer
ESP32 nasłuchuje wyłącznie na `127.0.0.1:8989` wewnątrz emulatora.
Test zostawia przypięte widżety z tym lokalnym adresem na pulpicie emulatora.
Nie uruchamia fizycznego nadajnika IR.

Wyniki i zrzuty znajdują się w ignorowanym `build/reports/touch-probe/`.
Skrypt odmawia pracy na fizycznym telefonie.
