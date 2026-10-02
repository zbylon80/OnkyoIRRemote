const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const assert = require('node:assert/strict');
const header = fs.readFileSync(path.join(__dirname, '../firmware/OnkyoRemote/DiagnosticsPage.h'), 'utf8');
const script = header.match(/<script>([\s\S]*?)<\/script>/)[1];
const fields = new Map(), requests = [], timers = new Map();
let nextTimer = 0, clicked;
const field = id => {
  if (!fields.has(id)) fields.set(id, {textContent: '', disabled: false,
    addEventListener(type, callback) { if (type === 'click') clicked = callback; }});
  return fields.get(id);
};
const flush = async () => { for (let i = 0; i < 12; ++i) await Promise.resolve(); };
vm.runInNewContext(script, {
  document: {querySelector: selector => field(selector.slice(1)), getElementById: field},
  AbortController,
  setTimeout(callback) { const id = ++nextTimer; timers.set(id, callback); return id; },
  clearTimeout(id) { timers.delete(id); },
  fetch(url, options) {
    assert.equal(url, '/diagnostics'); assert.equal(options.cache, 'no-store');
    return new Promise((resolve, reject) => {
      options.signal.addEventListener('abort', () => reject(new Error('timeout')));
      requests.push({finish(data) { resolve({ok:true,json:async()=>data}); }, reject});
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
  console.log('Actual diagnostics page passed rendering, manual refresh and recovery tests.');
})().catch(error => { console.error(error); process.exitCode = 1; });
