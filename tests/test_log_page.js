const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const header = fs.readFileSync(path.join(__dirname, '../firmware/OnkyoRemote/LogPage.h'), 'utf8');
const script = header.match(/<script>([\s\S]*?)<\/script>/)[1];
const fields = new Map(), requests = [], timers = new Map();
let nextTimer = 0, clicked;
const element = () => ({textContent:'',disabled:false,children:[],
  addEventListener(type, callback) { if (type === 'click') clicked = callback; },
  replaceChildren() { this.children = []; }, appendChild(child) { this.children.push(child); }});
const field = id => { if (!fields.has(id)) fields.set(id, element()); return fields.get(id); };
const flush = async () => { for (let i = 0; i < 12; ++i) await Promise.resolve(); };
vm.runInNewContext(script, {
  document:{querySelector:s => field(s.slice(1)),getElementById:field,createElement:element},
  AbortController, Date,
  setTimeout(callback) { const id = ++nextTimer; timers.set(id, callback); return id; },
  clearTimeout:id => timers.delete(id),
  fetch(url, options) {
    assert.equal(url, '/logs'); assert.equal(options.cache, 'no-store');
    assert.ok(!options.method || options.method === 'GET');
    return new Promise((resolve,reject) => {
      options.signal.addEventListener('abort', () => reject(new Error('timeout')));
      requests.push({finish(data,ok=true) { resolve({ok,json:async()=>data}); }});
    });
  }
});
const boot = {event:'boot',boot:1,uptimeSeconds:1,epochSeconds:null,detail:3,resetReason:'software',
  rssiDbm:null,freeBytes:190000,largestBlockBytes:100000,loopMs:0};
const snapshot = {storageReady:true,saveFailures:0,pending:true,entries:[boot,
  {...boot,event:'wifi_disconnected',detail:201,uptimeSeconds:300,epochSeconds:1791564000,rssiDbm:-60}]};
(async () => {
  assert.equal(requests.length,1); clicked(); assert.equal(requests.length,1);
  requests[0].finish(snapshot); await flush();
  const rows = field('entries').children;
  assert.equal(rows.length,2); assert.equal(rows[0].children[1].textContent,'Wi-Fi rozłączone');
  assert.equal(rows[0].children[2].textContent,'201'); assert.equal(rows[0].children[3].textContent,'-60 dBm');
  assert.ok(rows[1].children[0].textContent.includes('bez zegara'));
  assert.ok(rows[1].children[2].textContent.includes('software (3)'));
  assert.equal(field('refresh').disabled,false); assert.equal(timers.size,0);
  clicked(); requests[1].finish({...snapshot,storageReady:false,entries:[]}); await flush();
  assert.equal(field('entries').children.length,0);
  assert.ok(field('storage').textContent.includes('tylko w RAM'));
  clicked(); [...timers.values()][0](); await flush();
  assert.equal(field('refresh').disabled,false); assert.equal(timers.size,0);
  assert.ok(field('result').textContent.includes('Spróbuj odświeżyć'));
  clicked(); requests[3].finish({...snapshot,saveFailures:2,pending:false}); await flush();
  assert.ok(field('storage').textContent.includes('2 błędów'));
  assert.equal(requests.length,4); assert.equal(timers.size,0); // No polling or writes.
  assert.ok(header.includes('download="onkyo-logs.json"'));
  console.log('Event log page passed rendering, chronology, unavailable storage and timeout recovery checks.');
})().catch(error => { console.error(error); process.exitCode=1; });
