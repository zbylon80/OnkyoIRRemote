const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const assert = require('node:assert/strict');
const header = fs.readFileSync(path.join(__dirname, '../firmware/OnkyoRemote/DiagnosticsPage.h'), 'utf8');
const script = header.match(/<script>([\s\S]*?)<\/script>/)[1];
const fields = new Map(), requests = [], timers = new Map();
let nextTimer = 0, clicked, restartClicked;
const field = id => {
  if (!fields.has(id)) fields.set(id, {textContent: '', disabled: false,
    addEventListener(type, callback) { if (type === 'click') { if (id === 'restart') restartClicked = callback; else clicked = callback; } }});
  return fields.get(id);
};
const flush = async () => { for (let i = 0; i < 12; ++i) await Promise.resolve(); };
vm.runInNewContext(script, {
  document: {querySelector: selector => field(selector.slice(1)), getElementById: field},
  AbortController,
  setTimeout(callback) { const id = ++nextTimer; timers.set(id, callback); return id; },
  clearTimeout(id) { timers.delete(id); },
  fetch(url, options) {
    assert.ok(url === '/diagnostics' || url === '/restart'); assert.equal(options.cache, 'no-store');
    return new Promise((resolve, reject) => {
      options.signal.addEventListener('abort', () => reject(new Error('timeout')));
      requests.push({url,options,finish(data,ok=true) { resolve({ok,json:async()=>data}); }, reject});
    });
  }
});
const snapshot = {
  version:'1.1.0',uptimeSeconds:3661,
  wifi:{connected:true,rssiDbm:-64,sleepEnabled:false,connections:2,disconnects:1,
    lastDisconnectReason:201,lastDisconnectUptimeSeconds:90},
  memory:{freeBytes:180000,minimumFreeBytes:160000,largestFreeBlockBytes:100000},
  reset:{code:3,reason:'software'},loopMaxMs:75,timeSynchronized:true,volumeUpLimitMs:3000
};
(async () => {
  assert.equal(requests.length, 1); clicked(); assert.equal(requests.length, 1);
  requests[0].finish(snapshot); await flush();
  assert.equal(field('refresh').disabled, false); assert.equal(timers.size, 0);
  assert.equal(field('limit').textContent, '3 s'); assert.equal(field('rssi').textContent, '-64 dBm');
  assert.equal(field('sleep').textContent, 'wyłączone'); assert.equal(field('connections').textContent, '2 / 1');
  assert.ok(field('disconnect').textContent.includes('201'));
  assert.ok(field('uptime').textContent.includes('1 godz. 1 min 1 s'));
  clicked(); assert.equal(requests.length, 2);
  requests[1].finish({...snapshot,wifi:{...snapshot.wifi,connected:false,rssiDbm:null,lastDisconnectReason:null}});
  await flush(); assert.equal(field('rssi').textContent, 'brak danych');
  assert.equal(field('disconnect').textContent, 'brak od uruchomienia');
  clicked(); const timeout = [...timers.values()][0]; timeout(); await flush();
  assert.equal(field('refresh').disabled, false); assert.equal(timers.size, 0);
  assert.ok(field('result').textContent.includes('Spróbuj odświeżyć ponownie'));
  clicked(); assert.equal(requests.length, 4);
  requests[3].finish(snapshot); await flush();
  assert.equal(timers.size, 0); // Successful reads schedule no background work.
  assert.equal(requests.filter(r => r.url === '/restart').length, 0);
  restartClicked(); restartClicked(); clicked(); assert.equal(requests.length, 5);
  assert.equal(requests[4].url, '/restart'); assert.equal(requests[4].options.method, 'POST');
  assert.equal(requests[4].options.body, 'confirm=restart');
  requests[4].finish({ok:true,restarting:true}); await flush();
  assert.ok(field('result').textContent.includes('15 sekund')); assert.equal(field('restart').disabled, true);
  assert.equal(field('refresh').disabled, false); assert.equal(timers.size, 0);
  restartClicked(); assert.equal(requests.length, 5);
  clicked(); requests[5].finish(snapshot); await flush();
  assert.equal(field('restart').disabled, false);
  restartClicked(); requests[6].finish({ok:false,error:'Trwa aktualizacja'},false); await flush();
  assert.ok(field('result').textContent.includes('Trwa aktualizacja'));
  clicked(); requests[7].finish(snapshot); await flush();
  restartClicked(); [...timers.values()][0](); await flush();
  assert.equal(field('restart').disabled, true); assert.equal(field('refresh').disabled, false);
  assert.equal(requests.length, 9); assert.equal(timers.size, 0); // No restart retry after timeout.
  console.log('Actual diagnostics page passed rendering, manual refresh and recovery tests.');
})().catch(error => { console.error(error); process.exitCode = 1; });
