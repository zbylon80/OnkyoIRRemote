#pragma once
const char LOG_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Log ESP32</title>
<style>:root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;background:#171717;color:#eeeae2}
main{max-width:1000px;margin:auto;padding:24px 18px}h1{font-size:1.3rem}p{color:#bbb5aa;line-height:1.5}
button,a{font:inherit;color:#eeeae2}button{padding:12px 20px;background:#373737;border:1px solid #777;border-radius:5px}
button:disabled{opacity:.5}a{margin-right:20px}.scroll{overflow:auto}table{width:100%;border-collapse:collapse;font-size:.85rem}
th,td{text-align:left;padding:10px;border-bottom:1px solid #444;white-space:nowrap}#result{min-height:1.5em}
</style></head><body><main><h1>Historia zdarzeń ESP32</h1>
<p>Ostatnie 48 zdarzeń, od najnowszego. Boot oznacza kolejne uruchomienie. Czas przed synchronizacją zegara pokazuje czas działania od startu.</p>
<button id="refresh" type="button">Odśwież log</button> <a href="/logs" download="onkyo-logs.json">Pobierz JSON</a>
<p id="result" role="status"></p><p id="storage"></p><div class="scroll"><table>
<thead><tr><th>Czas / boot</th><th>Zdarzenie</th><th>Szczegóły</th><th>Wi-Fi</th><th>Pamięć / blok</th><th>Pętla</th></tr></thead>
<tbody id="entries"></tbody></table></div>
<p>Log w RAM jest zapisywany we flash co 5 minut; po ważnym zdarzeniu, np. rozłączeniu Wi-Fi, najwcześniej minutę po poprzedniej próbie zapisu. Przed restartem programowym zapis jest wymuszany. Nagłe odłączenie prądu może utracić końcówkę logu. Odczyt logu nie wymusza zapisu.</p>
<p><a href="/status">Diagnostyka</a><a href="/">Pilot</a></p><script>
const button = document.querySelector('#refresh'), result = document.querySelector('#result');
const names = {boot:'Uruchomienie',watchdog_ready:'Watchdog aktywny',watchdog_failed:'Błąd watchdoga',
wifi_connected:'Wi-Fi połączone',wifi_disconnected:'Wi-Fi rozłączone',wifi_retry:'Próba połączenia',radio_reset:'Restart radia',
ota_start:'Start OTA',ota_end:'Koniec OTA',ota_error:'Błąd OTA',manual_restart:'Restart ręczny',daily_restart:'Restart nocny',
clock_ready:'Zegar zsynchronizowany',health:'Stan urządzenia',slow_loop:'Wolny obieg programu',storage_error:'Błąd pamięci logu'};
async function refresh() {
  if (button.disabled) return;
  button.disabled = true; result.textContent = 'Pobieram log…';
  const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 5000);
  try {
    const response = await fetch('/logs', {cache:'no-store',signal:controller.signal});
    if (!response.ok) throw new Error('HTTP');
    const d = await response.json(), rows = document.getElementById('entries'); rows.replaceChildren();
    for (const e of [...d.entries].reverse()) {
      const row = document.createElement('tr');
      const timestamp = e.epochSeconds === null ? 'bez zegara' : new Date(e.epochSeconds*1000).toLocaleString('pl-PL',{timeZone:'Europe/Warsaw'});
      const detail = e.event === 'boot' ? `powód: ${e.resetReason} (${e.detail})` : String(e.detail);
      for (const value of [`${timestamp} · boot ${e.boot} +${e.uptimeSeconds} s`,names[e.event] || e.event,detail,
          e.rssiDbm === null ? '—' : `${e.rssiDbm} dBm`,`${(e.freeBytes/1024).toFixed(1)} / ${(e.largestBlockBytes/1024).toFixed(1)} KiB`,`${e.loopMs} ms`]) {
        const cell = document.createElement('td'); cell.textContent = value; row.appendChild(cell);
      }
      rows.appendChild(row);
    }
    document.getElementById('storage').textContent = d.storageReady ?
      `Zapis flash: ${d.saveFailures} błędów. ${d.pending ? 'Są nowe wpisy oczekujące na zapis.' : 'Log zapisany.'}` : 'Zapis flash niedostępny; nowe wpisy pozostają tylko w RAM.';
    result.textContent = `Odczytano ${d.entries.length} zdarzeń.`;
  } catch (_) { result.textContent = 'Nie udało się pobrać logu. Spróbuj odświeżyć ponownie.'; }
  finally { clearTimeout(timeout); button.disabled = false; }
}
button.addEventListener('click', refresh); refresh();
</script></main></body></html>
)HTML";
