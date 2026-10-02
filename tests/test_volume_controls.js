// Runs the actual served script with deferred HTTP replies and manual timers.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const header = fs.readFileSync(path.join(__dirname, '../firmware/OnkyoRemote/VolumeControls.h'), 'utf8');
const script = header.match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const flush = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };

function environment(command = 'VOL+') {
  const listeners = {}, buttonEvents = {}, windowEvents = {}, timers = new Map(), requests = [];
  let timerId = 0;
  const button = {dataset: {holdCommand: command}, setPointerCapture() {},
    addEventListener(type, fn) { buttonEvents[type] = fn; }};
  const document = {hidden: false, querySelectorAll: () => [button],
    addEventListener(type, fn) { listeners[type] = fn; }};
  vm.runInNewContext(script, {
    document, URLSearchParams, AbortController,
    window: {addEventListener(type, fn) { windowEvents[type] = fn; }},
    setTimeout(fn, delay) { const id = ++timerId; timers.set(id, {fn, delay}); return id; },
    clearTimeout(id) { timers.delete(id); },
    fetch(url, options) {
      let resolve, reject;
      const result = new Promise((yes, no) => { resolve = yes; reject = no; });
      options.signal.addEventListener('abort', () => reject(new Error('Request timeout')));
      requests.push({url, values: Object.fromEntries(new URLSearchParams(options.body)),
        finish(value = {}) { resolve({ok: true, json: async () => value}); }, reject});
      return result;
    }
  });
  const event = {pointerId: 1, button: 0, isPrimary: true, preventDefault() {}};
  return {requests, timers, document, windowEvents,
    down() { buttonEvents.pointerdown(event); }, up() { listeners.pointerup(event); },
    loseCapture() { buttonEvents.lostpointercapture(event); },
    async tick(delay) {
      const found = [...timers].find(([, timer]) => timer.delay === delay);
      assert.ok(found, `Expected timer ${delay}`);
      timers.delete(found[0]); found[1].fn(); await flush();
    }, hasTimer(delay) { return [...timers.values()].some(timer => timer.delay === delay); }
  };
}

async function arm(env, session = '123') {
  env.down(); assert.equal(env.requests.at(-1).url, '/volume/press');
  env.requests.at(-1).finish({session}); await flush();
  await env.tick(350); assert.equal(env.requests.at(-1).url, '/volume/start');
  env.requests.at(-1).finish(); await flush();
}

(async () => {
  // Delayed initial reply after a tap never starts any repetition.
  let env = environment(); env.down(); env.up();
  env.requests[0].finish({session: '1'}); await flush();
  assert.equal(env.requests.length, 1); assert.equal(env.timers.size, 0);

  // Stop can be sent before the start reply; a late success creates no renewal.
  env = environment(); env.down(); env.requests[0].finish({session: '2'}); await flush();
  await env.tick(350); const pendingStart = env.requests.at(-1);
  env.up(); assert.equal(env.requests.at(-1).url, '/volume/stop');
  assert.equal(env.requests.at(-1).values.session, '2');
  pendingStart.finish(); env.requests.at(-1).finish(); await flush();
  assert.equal(env.timers.size, 0);

  // Up stops at the local cap without needing pointerup, and never auto-restarts.
  env = environment(); await arm(env); await env.tick(2000);
  assert.equal(env.requests.at(-1).url, '/volume/stop');
  env.requests.at(-1).finish(); await flush();
  assert.equal(env.timers.size, 0);
  const capped = env.requests.length;
  env.down(); assert.equal(env.requests.length, capped);
  env.up();
  env.down(); assert.equal(env.requests.at(-1).url, '/volume/press');

  // Duplicate pointerdown while held cannot mint a replacement session.
  env = environment(); await arm(env); const held = env.requests.length;
  env.down(); assert.equal(env.requests.length, held);

  // Down has no extra cap; slow HTTP permits only one in-flight renewal.
  env = environment('VOL-'); await arm(env);
  assert.equal(env.hasTimer(2000), false);
  await env.tick(150); const pendingRenewal = env.requests.at(-1);
  assert.equal(pendingRenewal.url, '/volume/keepalive');
  assert.equal(env.hasTimer(150), false);
  pendingRenewal.finish(); await flush(); assert.equal(env.hasTimer(150), true);
  env.loseCapture(); assert.equal(env.requests.at(-1).url, '/volume/stop');

  // A failure from the old gesture cannot stop the new one.
  env = environment(); await arm(env, '3'); await env.tick(150);
  const oldRenewal = env.requests.at(-1); env.up(); await arm(env, '4');
  const before = env.requests.length; oldRenewal.reject(new Error('late failure')); await flush();
  assert.equal(env.requests.length, before); assert.equal(env.hasTimer(150), true);
  env.windowEvents.blur(); assert.equal(env.requests.at(-1).values.session, '4');

  // Multiple taps do not accumulate presses while the initial request stalls.
  env = environment(); env.down(); env.up(); env.down(); env.up();
  assert.equal(env.requests.length, 1);
  env.requests[0].finish({session: '5'}); await flush(); env.down();
  assert.equal(env.requests.length, 2);

  // A hung initial request times out, so the next press can recover.
  env = environment(); env.down(); env.up(); await env.tick(2000);
  env.down(); assert.equal(env.requests.length, 2);

  // 64-bit tokens stay strings; no JavaScript number precision is lost.
  env = environment(); await arm(env, '18446744073709551615');
  assert.equal(env.requests.at(-1).values.session, '18446744073709551615');
  env.document.hidden = true;
  env.windowEvents.pagehide();
  assert.equal(env.requests.at(-1).url, '/volume/stop');

  console.log('Actual browser volume script passed delayed-network and gesture tests.');
})().catch(error => { console.error(error); process.exitCode = 1; });
