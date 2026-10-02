#pragma once

const char DIAGNOSTICS_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Diagnostyka pilota</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;background:#171717;color:#eeeae2}
main{max-width:640px;margin:auto;padding:24px 18px}h1{font-size:1.3rem}p{color:#bbb5aa;line-height:1.5}
button{padding:12px 20px;background:#373737;border:1px solid #777;border-radius:5px;color:inherit;font:inherit}
button:disabled{opacity:.5}dl{display:grid;grid-template-columns:1fr 1fr;gap:0 16px}
dt,dd{margin:0;padding:12px 0;border-bottom:1px solid #444}dd{overflow-wrap:anywhere}
a{color:#d1c8ba;margin-right:24px}#result{min-height:1.5em}
</style></head><body><main><h1>Diagnostyka pilota</h1>
<p>Dane dotyczą bieżącego uruchomienia urządzenia. Odświeżają się po otwarciu tej strony lub naciśnięciu przycisku.</p>
<button id="refresh" type="button">Odśwież dane</button><p id="result" role="status"></p>
<dl>
<dt>Firmware</dt><dd id="version">—</dd>
<dt>Czas działania</dt><dd id="uptime">—</dd>
<dt>Połączenie Wi-Fi</dt><dd id="wifi">—</dd>
<dt>Sygnał Wi-Fi</dt><dd id="rssi">—</dd>
<dt>Usypianie Wi-Fi</dt><dd id="sleep">—</dd>
<dt>Połączenia / rozłączenia</dt><dd id="connections">—</dd>
<dt>Ostatnie rozłączenie</dt><dd id="disconnect">—</dd>
<dt>Wolna pamięć</dt><dd id="heap">—</dd>
<dt>Najmniej wolnej pamięci</dt><dd id="minHeap">—</dd>
<dt>Największy wolny blok</dt><dd id="largestBlock">—</dd>
<dt>Powód ostatniego restartu</dt><dd id="reset">—</dd>
<dt>Najdłuższy obieg programu</dt><dd id="loop">—</dd>
<dt>Synchronizacja czasu</dt><dd id="clock">—</dd>
<dt>Limit zgłaśniania</dt><dd id="limit">—</dd>
</dl><p><a href="/">Pilot Basic</a><a href="/advanced">Pilot Advanced</a></p>
<script>
const button = document.querySelector('#refresh');
const result = document.querySelector('#result');
const show = (id, value) => { document.getElementById(id).textContent = value; };
const duration = seconds => `${Math.floor(seconds / 3600)} godz. ${Math.floor(seconds % 3600 / 60)} min ${seconds % 60} s`;
const bytes = value => `${(value / 1024).toFixed(1)} KiB`;
const resetNames = {power_on:'włączenie zasilania',external:'zewnętrzny reset',software:'restart programowy',
  panic:'błąd programu',interrupt_watchdog:'watchdog przerwań',task_watchdog:'watchdog zadania',
  watchdog:'watchdog',deep_sleep:'wybudzenie',brownout:'spadek napięcia',sdio:'reset SDIO',unknown:'inny'};
async function refresh() {
  if (button.disabled) return;
  button.disabled = true; result.textContent = 'Pobieram dane…';
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 3000);
  try {
    const response = await fetch('/diagnostics', {cache:'no-store',signal:controller.signal});
    if (!response.ok) throw new Error('Diagnostics failed');
    const d = await response.json();
    show('version', d.version); show('uptime', duration(d.uptimeSeconds));
    show('wifi', d.wifi.connected ? 'połączony' : 'rozłączony');
    show('rssi', d.wifi.rssiDbm === null ? 'brak danych' : `${d.wifi.rssiDbm} dBm`);
    show('sleep', d.wifi.sleepEnabled ? 'włączone' : 'wyłączone');
    show('connections', `${d.wifi.connections} / ${d.wifi.disconnects}`);
    show('disconnect', d.wifi.lastDisconnectReason === null ? 'brak od uruchomienia' :
      `kod ${d.wifi.lastDisconnectReason}, po ${duration(d.wifi.lastDisconnectUptimeSeconds)}`);
    show('heap', bytes(d.memory.freeBytes)); show('minHeap', bytes(d.memory.minimumFreeBytes));
    show('largestBlock', bytes(d.memory.largestFreeBlockBytes));
    show('reset', `${resetNames[d.reset.reason] || d.reset.reason} (kod ${d.reset.code})`);
    show('loop', `${d.loopMaxMs} ms`); show('clock', d.timeSynchronized ? 'zakończona' : 'oczekuje');
    show('limit', `${d.volumeUpLimitMs / 1000} s`);
    result.textContent = 'Dane odświeżone.';
  } catch (_) {
    result.textContent = 'Nie udało się pobrać danych. Spróbuj odświeżyć ponownie.';
  } finally {
    clearTimeout(timeout); button.disabled = false;
  }
}
button.addEventListener('click', refresh);
refresh();
</script></main></body></html>
)HTML";
