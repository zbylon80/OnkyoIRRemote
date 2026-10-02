#pragma once

// Shared by Basic and Advanced. No background connection polling is needed.
const char VOLUME_SCRIPT[] PROGMEM = R"JS(
(() => {
  let currentHold = null;
  let pressedPointerId = null;
  let pressInFlight = false;
  function post(path, values = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 2000);
    return fetch(path, {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      signal: controller.signal,
      body: new URLSearchParams(values).toString()
    }).then(response => {
      if (!response.ok) throw new Error('Volume request rejected');
      return response;
    }).finally(() => clearTimeout(timeout));
  }
  function stopHolding(hold = currentHold) {
    if (hold === null || hold !== currentHold) return;
    currentHold = null;
    clearTimeout(hold.renewTimer);
    clearTimeout(hold.limitTimer);
    clearTimeout(hold.startTimer);
    if (hold.session !== null) {
      post('/volume/stop', {session: hold.session}).catch(() => {});
    }
  }
  async function renew(hold) {
    if (currentHold !== hold) return;
    try {
      await post('/volume/keepalive', {session: hold.session});
      // At most one renewal is in flight, even on a slow connection.
      if (currentHold === hold) hold.renewTimer = setTimeout(() => renew(hold), 150);
    } catch (_) {
      stopHolding(hold);
    }
  }
  async function startHolding(button, event) {
    if (event.isPrimary === false || (event.button !== undefined && event.button !== 0)) return;
    event.preventDefault();
    // Reaching the limit does not count as releasing the physical button.
    if (pressedPointerId !== null) return;
    pressedPointerId = event.pointerId;
    // Do not accumulate initial presses behind a stalled connection.
    if (pressInFlight) return;
    const command = button.dataset.holdCommand;
    const hold = {session: null, pointerId: event.pointerId, renewTimer: null, limitTimer: null, startTimer: null};
    currentHold = hold;
    button.setPointerCapture(event.pointerId);
    if (command === 'VOL+') {
      hold.limitTimer = setTimeout(() => stopHolding(hold), 2000);
    }
    const direction = command === 'VOL+' ? 'up' : command === 'VOL-' ? 'down'
      : command === 'TUNING+' ? 'tuning-up' : 'tuning-down';
    try {
      // One press sends one IR command. Repeating is armed separately, only
      // while this same gesture is still held after the response and delay.
      pressInFlight = true;
      let prepared;
      try {
        const response = await post('/volume/press', {direction});
        prepared = await response.json();
      } finally {
        pressInFlight = false;
      }
      if (currentHold !== hold) return;
      if (typeof prepared.session !== 'string' || !/^[0-9]+$/.test(prepared.session)) {
        throw new Error('Invalid volume session');
      }
      hold.session = prepared.session;
      hold.startTimer = setTimeout(async () => {
        if (currentHold !== hold) return;
        try {
          await post('/volume/start', {session: hold.session});
          if (currentHold === hold) hold.renewTimer = setTimeout(() => renew(hold), 150);
        } catch (_) {
          stopHolding(hold);
        }
      }, 350);
    } catch (_) {
      stopHolding(hold);
    }
  }
  const release = event => {
    if (pressedPointerId === event.pointerId) {
      pressedPointerId = null;
      stopHolding();
    }
  };
  const releaseAll = () => { pressedPointerId = null; stopHolding(); };
  document.querySelectorAll('[data-hold-command]').forEach(button => {
    button.addEventListener('pointerdown', event => startHolding(button, event));
    button.addEventListener('contextmenu', event => event.preventDefault());
    button.addEventListener('lostpointercapture', release);
  });
  document.addEventListener('pointerup', release);
  document.addEventListener('pointercancel', release);
  window.addEventListener('blur', releaseAll);
  window.addEventListener('pagehide', releaseAll);
  document.addEventListener('visibilitychange', () => { if (document.hidden) releaseAll(); });
})();
)JS";
