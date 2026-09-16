#include <ArduinoOTA.h>
#include <IRremote.hpp>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "WiFiConfig.h"

constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_RECEIVE_PIN = 27;
constexpr uint16_t ONKYO_ADDRESS = 0x6DD2;

struct RemoteFunction {
  const char *id;
  const char *label;
  const char *group;
  const char *preferenceKey;
  uint8_t defaultValue;
  uint8_t learnedValue;
  decode_type_t learnedProtocol = UNKNOWN;
  uint16_t learnedAddress = 0;
  uint16_t learnedCommand = 0;
  uint8_t learnedBits = 0;
};

// Every physical button on the RC-209S. Known Basic values remain available;
// the rest are learned from the original remote and retained in ESP32 NVS.
RemoteFunction REMOTE_FUNCTIONS[] = {
    {"POWER", "POWER", "AMPLITUNER", "irPower", 0x04, 0},
    {"SLEEP", "SLEEP", "AMPLITUNER", "irSleep", 0, 0},
    {"SPK-MAIN", "SPEAKERS MAIN", "AMPLITUNER", "irSpkMain", 0, 0},
    {"SPK-REMOTE", "SPEAKERS REMOTE", "AMPLITUNER", "irSpkRemote", 0, 0},
    {"SIMUL", "SIMUL SOURCE", "AMPLITUNER", "irSimul", 0, 0},
    {"SIMUL-UP", "SIMUL SOURCE UP", "AMPLITUNER", "irSimulUp", 0, 0},
    {"SIMUL-DOWN", "SIMUL SOURCE DOWN", "AMPLITUNER", "irSimulDown", 0, 0},
    {"TUNER", "TUNER", "WEJŚCIA", "irTuner", 0x0B, 0},
    {"PHONO", "PHONO", "WEJŚCIA", "irPhono", 0x0A, 0},
    {"CD", "CD", "WEJŚCIA", "irCd", 0x09, 0},
    {"DIRECT", "DIRECT", "WEJŚCIA", "irDirect", 0, 0},
    {"VIDEO-1", "VIDEO-1", "WEJŚCIA", "irVideo1", 0x0F, 0},
    {"VIDEO-2", "VIDEO-2", "WEJŚCIA", "irVideo2", 0, 0},
    {"TAPE-1", "TAPE-1", "WEJŚCIA", "irTape1", 0x08, 0},
    {"TAPE-2", "TAPE-2", "WEJŚCIA", "irTape2", 0, 0},
    {"CLASS", "CLASS", "TUNER", "irClass", 0, 0},
    {"PRESET-", "PRESET ◀", "TUNER", "irPresetM", 0, 0},
    {"PRESET+", "PRESET ▶", "TUNER", "irPresetP", 0, 0},
    {"DECK-A-REV", "◀", "DECK A", "irDeckARev", 0, 0},
    {"DECK-A-PLAY", "▶", "DECK A", "irDeckAPlay", 0, 0},
    {"DECK-A-REC", "REC / PAUSE", "DECK A", "irDeckARec", 0, 0},
    {"DECK-A-STOP", "STOP", "DECK A", "irDeckAStop", 0, 0},
    {"DECK-A-REW", "REW", "DECK A", "irDeckARew", 0, 0},
    {"DECK-A-FF", "FF", "DECK A", "irDeckAFf", 0, 0},
    {"DECK-B-REV", "◀", "DECK B", "irDeckBRev", 0, 0},
    {"DECK-B-PLAY", "▶", "DECK B", "irDeckBPlay", 0, 0},
    {"DECK-B-REC", "REC / PAUSE", "DECK B", "irDeckBRec", 0, 0},
    {"DECK-B-STOP", "STOP", "DECK B", "irDeckBStop", 0, 0},
    {"DECK-B-REW", "REW", "DECK B", "irDeckBRew", 0, 0},
    {"DECK-B-FF", "FF", "DECK B", "irDeckBFf", 0, 0},
    {"CD-PAUSE", "PAUSE", "CD", "irCdPause", 0, 0},
    {"CD-PLAY", "PLAY", "CD", "irCdPlay", 0, 0},
    {"CD-STOP", "STOP", "CD", "irCdStop", 0, 0},
    {"CD-PREV", "◀◀", "CD", "irCdPrev", 0, 0},
    {"CD-NEXT", "▶▶", "CD", "irCdNext", 0, 0},
    {"CENTER-ON", "CENTER OFF/ON", "DŹWIĘK", "irCenterOn", 0, 0},
    {"CENTER-UP", "CENTER UP", "DŹWIĘK", "irCenterUp", 0, 0},
    {"CENTER-DOWN", "CENTER DOWN", "DŹWIĘK", "irCenterDown", 0, 0},
    {"REAR-UP", "REAR LEVEL UP", "DŹWIĘK", "irRearUp", 0, 0},
    {"REAR-DOWN", "REAR LEVEL DOWN", "DŹWIĘK", "irRearDown", 0, 0},
    {"MUTE", "MUTING", "DŹWIĘK", "irMute", 0x05, 0},
    {"VOL+", "VOLUME UP", "DŹWIĘK", "irVolP", 0x02, 0},
    {"VOL-", "VOLUME DOWN", "DŹWIĘK", "irVolM", 0x03, 0},
    {"SURROUND", "SURROUND MODE", "DŹWIĘK", "irSurround", 0, 0},
    {"DELAY", "DELAY TIME", "DŹWIĘK", "irDelay", 0, 0},
    {"TEST", "TEST", "DŹWIĘK", "irTest", 0, 0},
};

WebServer server(80);
Preferences preferences;

constexpr unsigned long VOLUME_REPEAT_INTERVAL_MS = 110;
constexpr unsigned long VOLUME_WATCHDOG_MS = 400;
constexpr unsigned long LEARN_RECEIVER_SETTLE_MS = 750;
constexpr uint16_t OTA_TIMEOUT_SECONDS = 60;
const char *heldVolumeCommand = nullptr;
const char *learningCommand = nullptr;
bool irTransmissionEnabled = true;
unsigned long learningStartedMs = 0;
unsigned long lastVolumeSignalMs = 0;
unsigned long lastVolumeSendMs = 0;

const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#101010">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <link rel="manifest" href="/manifest.webmanifest">
  <link rel="icon" href="/icon.svg" type="image/svg+xml">
  <link rel="apple-touch-icon" href="/icon.svg">
  <title>Pilot Onkyo</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: #a9a7a2; color: #eeeae2; -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; }
    main { width: min(100%, 430px); min-height: 100vh; padding: 24px 18px 32px; background: linear-gradient(110deg, #121212, #202020 52%, #141414); border: 1px solid #515151; box-shadow: inset 0 0 0 2px #0b0b0b, 0 16px 38px #0008; }
    header { display: flex; justify-content: space-between; align-items: start; margin-bottom: 28px; padding-bottom: 13px; border-bottom: 1px solid #555; }
    h1 { margin: 0; font-size: 1.1rem; letter-spacing: .08em; text-shadow: 0 1px #000; }
    .onkyo-logo { font-family: Georgia, serif; font-size: 1.55rem; font-weight: 900; letter-spacing: -.06em; }
    header p { margin: 4px 0 0; color: #bbb5aa; font-size: .8rem; }
    .connected { color: #91d27a; }
    button { min-height: 56px; border: 1px solid #666; border-radius: 5px; background: linear-gradient(135deg, #3b3b3b, #1d1d1d); box-shadow: inset 0 1px #696969, 0 2px 2px #000; color: inherit; font: inherit; font-size: 1rem; font-weight: 650; cursor: pointer; touch-action: manipulation; }
    button:active { transform: translateY(1px); background: #111; box-shadow: inset 0 2px 3px #000; }
    .power { width: 100%; background: linear-gradient(135deg, #9f3b35, #64231f); border-color: #c27067; font-size: 1.2rem; letter-spacing: .1em; }
    .power:active { background: #55201d; }
    .label { margin: 25px 0 9px; padding-top: 7px; border-top: 1px solid #555; color: #c8c1b6; font-size: .72rem; letter-spacing: .12em; }
    .volume { display: grid; grid-template-columns: 1fr 1fr; gap: 7px; }
    .sources { display: grid; grid-template-columns: repeat(4, 1fr); gap: 7px; }
    .sources .source { grid-column: span 2; }
    .sources .preset-button { grid-column: span 1; }
    .volume button { min-height: 72px; font-size: 2rem; }
    .mute { width: 100%; margin-top: 10px; }
    .advanced-link { display: block; margin-top: 26px; color: #d1c8ba; font-size: .82rem; text-align: center; }
    @media (min-width: 431px) { main { min-height: auto; border-radius: 9px; } }
  </style>
</head>
<body>
  <main>
    <header>
      <div><h1 class="onkyo-logo">ONKYO</h1><p>REMOTE CONTROL TRANSMITTER · RC-209S</p></div>
      <p class="connected">● Connected</p>
    </header>
    <button class="power" type="button" data-command="POWER">POWER</button>
    <p class="label">VOLUME</p>
    <div class="volume" aria-label="Volume controls">
      <button type="button" data-hold-command="VOL-" aria-label="Volume down">−</button>
      <button type="button" data-hold-command="VOL+" aria-label="Volume up">+</button>
    </div>
    <button class="mute" type="button" data-command="MUTE">MUTE</button>
    <p class="label">SOURCE</p>
    <div class="sources" aria-label="Source selection">
      <button class="source" type="button" data-source="TAPE-1">TAPE-1</button>
      <button class="source" type="button" data-source="CD">CD</button>
      <button class="source" type="button" data-source="PHONO">PHONO</button>
      <button class="source" type="button" data-source="TUNER">TUNER</button>
      <button class="source" type="button" data-source="VIDEO-1">VIDEO-1</button>
      <button class="preset-button" type="button" data-command="PRESET-">◀</button>
      <button class="preset-button" type="button" data-command="PRESET+">▶</button>
    </div>
    <a class="advanced-link" href="/advanced">Otwórz pilot Advanced</a>
  </main>
  <script>
    let holdTimer = null;
    let heldVolumeCommand = null;
    let requestInFlight = false;
    function sendCommand(command) {
      if (requestInFlight) return;
      requestInFlight = true;
      fetch('/command', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'name=' + encodeURIComponent(command) })
        .then((response) => { if (!response.ok) throw new Error('Command failed'); })
        .catch(() => {})
        .finally(() => { requestInFlight = false; });
    }
    function postVolume(path, body = '') {
      return fetch(path, { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body });
    }
    function stopHolding() {
      if (holdTimer !== null) { clearInterval(holdTimer); holdTimer = null; }
      if (heldVolumeCommand !== null) {
        heldVolumeCommand = null;
        postVolume('/volume/stop').catch(() => {});
      }
    }
    document.querySelectorAll('[data-command]').forEach((button) => button.addEventListener('click', () => sendCommand(button.dataset.command)));
    document.querySelectorAll('[data-source]').forEach((button) => button.addEventListener('click', () => {
      sendCommand(button.dataset.source);
    }));
    document.querySelectorAll('[data-hold-command]').forEach((button) => {
      button.addEventListener('pointerdown', (event) => {
        event.preventDefault();
        stopHolding();
        const command = button.dataset.holdCommand;
        heldVolumeCommand = command;
        button.setPointerCapture(event.pointerId);
        const direction = command === 'VOL+' ? 'up' : command === 'VOL-' ? 'down' : command === 'TUNING+' ? 'tuning-up' : 'tuning-down';
        postVolume('/volume/start', 'direction=' + direction)
          .then((response) => { if (!response.ok) throw new Error('Volume failed'); })
          .catch(() => { stopHolding(); });
        holdTimer = setInterval(() => postVolume('/volume/keepalive').catch(() => stopHolding()), 150);
      });
      button.addEventListener('contextmenu', (event) => event.preventDefault());
    });
    document.addEventListener('pointerup', stopHolding);
    document.addEventListener('pointercancel', stopHolding);
    window.addEventListener('blur', stopHolding);
    window.addEventListener('pagehide', stopHolding);
    document.addEventListener('visibilitychange', () => { if (document.hidden) stopHolding(); });
  </script>
</body>
</html>
)HTML";

const char PWA_MANIFEST[] PROGMEM = R"JSON(
{
  "name": "Pilot Onkyo",
  "short_name": "Onkyo",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#101010",
  "theme_color": "#101010",
  "icons": [{
    "src": "/icon.svg",
    "sizes": "any",
    "type": "image/svg+xml",
    "purpose": "any maskable"
  }]
}
)JSON";

const char PWA_ICON[] PROGMEM = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 192">
  <rect width="192" height="192" rx="38" fill="#101010"/>
  <rect x="48" y="27" width="96" height="138" rx="14" fill="#242424" stroke="#e0d8ca" stroke-width="5"/>
  <circle cx="96" cy="55" r="10" fill="#d8342b"/>
  <rect x="67" y="80" width="58" height="12" rx="6" fill="#e0d8ca"/>
  <rect x="67" y="105" width="58" height="12" rx="6" fill="#e0d8ca"/>
  <rect x="67" y="130" width="58" height="12" rx="6" fill="#e0d8ca"/>
</svg>
)SVG";

RemoteFunction *findRemoteFunction(const String &id) {
  for (RemoteFunction &function : REMOTE_FUNCTIONS) {
    if (id == function.id) return &function;
  }
  return nullptr;
}

bool hasLearnedCode(const RemoteFunction &function) {
  return function.learnedProtocol != UNKNOWN || function.learnedValue != 0;
}

String remotePageHead(const char *title) {
  String page = F("<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta name=\"theme-color\" content=\"#101010\"><title>");
  page += title;
  page += F("</title><style>:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#101010;color:#f5f2ed}main{max-width:760px;margin:auto;padding:24px 18px 36px;background:#181818}h1{margin:0;font-size:1.15rem;letter-spacing:.07em}h2{margin:28px 0 9px;color:#aaa6a0;font-size:.75rem;letter-spacing:.12em}.hint,#status{line-height:1.45;color:#c2bdb5}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}button{min-height:54px;border:0;border-radius:14px;background:#292929;color:inherit;font:inherit;font-weight:650;cursor:pointer}button:active{transform:scale(.98);background:#393939}.missing{opacity:.52}.learning{background:#806326}.saved:after{content:' ✓';color:#91d27a}a{display:block;margin-top:28px;color:#aaa6a0;text-align:center}@media(min-width:600px){.grid{grid-template-columns:repeat(3,minmax(0,1fr))}main{margin-top:20px;border-radius:28px;box-shadow:0 18px 50px #0008}}</style></head><body><main>");
  return page;
}

String renderRemoteButtons(bool learning) {
  String page;
  const char *currentGroup = nullptr;
  for (const RemoteFunction &function : REMOTE_FUNCTIONS) {
    if (currentGroup == nullptr || strcmp(currentGroup, function.group) != 0) {
      if (currentGroup != nullptr) page += F("</div>");
      currentGroup = function.group;
      page += F("<h2>");
      page += currentGroup;
      page += F("</h2><div class=\"grid\">");
    }
    page += F("<button type=\"button\" data-id=\"");
    page += function.id;
    page += F("\" class=\"");
    if (!learning && function.defaultValue == 0 && !hasLearnedCode(function)) page += F("missing");
    if (learning && (function.defaultValue != 0 || hasLearnedCode(function))) page += F("saved");
    page += F("\">");
    page += function.label;
    page += F("</button>");
  }
  if (currentGroup != nullptr) page += F("</div>");
  return page;
}

String advancedButton(const char *id, bool learning = false) {
  RemoteFunction *function = findRemoteFunction(id);
  if (function == nullptr) return String();
  String button = F("<button type=\"button\" class=\"key ");
  if (learning && (function->defaultValue != 0 || hasLearnedCode(*function))) button += F("saved ");
  if (!learning && function->defaultValue == 0 && !hasLearnedCode(*function)) button += F("unlearned ");
  if (strcmp(id, "POWER") == 0) button += F("power-key ");
  button += F("\" data-id=\"");
  button += id;
  button += F("\">");
  button += function->label;
  button += F("</button>");
  return button;
}

String remoteShellStyle() {
  return F("<style>main{max-width:590px;background:linear-gradient(110deg,#111,#242424 52%,#121212);border:1px solid #585858;box-shadow:inset 0 0 0 2px #090909,0 18px 42px #0009}.remote{max-width:550px;margin:auto;border:1px solid #686868;padding:15px;background:#171717;box-shadow:inset 0 0 18px #000}.remote h1{text-align:center;margin:5px 0 15px;font-family:Georgia,serif;font-size:1.35rem;font-weight:900;letter-spacing:-.04em}.top,.inputs,.tuner-controls,.deck-controls,.cd-controls,.effects{display:grid;gap:6px}.top,.inputs{grid-template-columns:repeat(5,1fr)}.input-box,.tuner-box,.deck,.cd-box,.center-box,.rear-box,.muting-box,.effects-box{border:1px solid #626262;padding:6px}.box-label{text-align:center;font-size:.66rem;letter-spacing:.08em;color:#c7c0b5;margin:0 0 6px}.tuner-line{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin:12px 0}.tuner-box{grid-column:3 / 6}.tuner-controls{grid-template-columns:repeat(3,1fr)}.decks{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}.deck-a{grid-column:1 / 3}.deck-b{grid-column:4 / 6}.deck-controls{grid-template-columns:repeat(2,1fr)}.lower{display:grid;grid-template-columns:2fr 1fr 1fr 1fr;gap:6px;margin-top:12px}.cd-controls{grid-template-columns:repeat(2,1fr)}.cd-stop{grid-column:1 / 3;display:grid;place-items:center}.cd-stop .key{width:48%}.center-box,.rear-box,.muting-box{display:grid;gap:6px;align-content:start}.volume-label{margin-top:5px!important;border-top:1px solid #575757;padding-top:5px}.effects-box{width:61%;margin-top:6px}.effects{grid-template-columns:repeat(3,1fr)}.brand{width:61%;margin-top:16px;padding:13px 5px 3px;border-top:1px solid #555;color:#e4e0d7}.brand-onkyo{font-family:Georgia,serif;font-size:2.15rem;font-weight:900;letter-spacing:-.08em}.brand-ri{float:right;font-family:Georgia,serif;font-size:1.7rem;font-weight:900}.brand small{display:block;letter-spacing:.08em;font-size:.56rem}.brand b{float:right}.key{min-height:52px;padding:5px 3px;border:1px solid #6b6b6b;border-radius:3px;background:linear-gradient(135deg,#3f3f3f,#1c1c1c);box-shadow:inset 0 1px #777,0 2px 2px #000;color:#ece8df;font-size:.72rem;font-weight:700;line-height:1.05}.key:active{transform:translateY(1px);background:#101010;box-shadow:inset 0 2px 3px #000}.power-key{background:linear-gradient(135deg,#984038,#57211e);border-color:#bd746b}.unlearned{opacity:.46;border-style:dashed}.saved:after{content:' ✓';color:#91d27a}.learning{background:#806326;opacity:1}.legend{margin:12px 0 0;text-align:center;font-size:.78rem;color:#c5bfb4}.learn{color:#d7cfbf}.back{margin-top:14px}@media(max-width:420px){main{padding:20px 10px}.remote{padding:10px}.key{min-height:46px;font-size:.61rem}.top,.inputs{gap:4px}}</style>");
}

String renderRemoteShell(bool learning) {
  String page = F("<div class=\"remote\"><div class=\"top\">");
  page += advancedButton("POWER", learning); page += advancedButton("SLEEP", learning); page += advancedButton("SPK-MAIN", learning); page += advancedButton("SPK-REMOTE", learning); page += advancedButton("SIMUL", learning);
  page += F("</div><div class=\"input-box\"><p class=\"box-label\">INPUT SELECTOR</p><div class=\"inputs\">");
  page += advancedButton("TUNER", learning); page += advancedButton("PHONO", learning); page += advancedButton("CD", learning); page += advancedButton("DIRECT", learning); page += advancedButton("SIMUL-UP", learning);
  page += advancedButton("VIDEO-1", learning); page += advancedButton("VIDEO-2", learning); page += advancedButton("TAPE-1", learning); page += advancedButton("TAPE-2", learning); page += advancedButton("SIMUL-DOWN", learning);
  page += F("</div></div><div class=\"tuner-line\"><div></div><div></div><div class=\"tuner-box\"><p class=\"box-label\">TUNER</p><div class=\"tuner-controls\">");
  page += advancedButton("CLASS", learning); page += advancedButton("PRESET-", learning); page += advancedButton("PRESET+", learning);
  page += F("</div></div></div><div class=\"decks\"><div class=\"deck deck-a\"><p class=\"box-label\">DECK-A</p><div class=\"deck-controls\">");
  page += advancedButton("DECK-A-REV", learning); page += advancedButton("DECK-A-PLAY", learning); page += advancedButton("DECK-A-REC", learning); page += advancedButton("DECK-A-STOP", learning); page += advancedButton("DECK-A-REW", learning); page += advancedButton("DECK-A-FF", learning);
  page += F("</div></div><div></div><div class=\"deck deck-b\"><p class=\"box-label\">DECK-B</p><div class=\"deck-controls\">");
  page += advancedButton("DECK-B-REV", learning); page += advancedButton("DECK-B-PLAY", learning); page += advancedButton("DECK-B-REC", learning); page += advancedButton("DECK-B-STOP", learning); page += advancedButton("DECK-B-REW", learning); page += advancedButton("DECK-B-FF", learning);
  page += F("</div></div></div><div class=\"lower\"><div class=\"cd-box\"><p class=\"box-label\">CD</p><div class=\"cd-controls\">");
  page += advancedButton("CD-PAUSE", learning); page += advancedButton("CD-PLAY", learning);
  page += F("<span class=\"cd-stop\">"); page += advancedButton("CD-STOP", learning); page += F("</span>");
  page += advancedButton("CD-PREV", learning); page += advancedButton("CD-NEXT", learning);
  page += F("</div></div><div class=\"center-box\"><p class=\"box-label\">CENTER</p>");
  page += advancedButton("CENTER-ON", learning); page += advancedButton("CENTER-UP", learning); page += advancedButton("CENTER-DOWN", learning);
  page += F("</div><div class=\"rear-box\"><p class=\"box-label\">REAR LEVEL</p>");
  page += advancedButton("REAR-UP", learning); page += advancedButton("REAR-DOWN", learning);
  page += F("</div><div class=\"muting-box\"><p class=\"box-label\">MUTING</p>");
  page += advancedButton("MUTE", learning);
  page += F("<p class=\"box-label volume-label\">VOLUME</p>");
  page += advancedButton("VOL+", learning); page += advancedButton("VOL-", learning);
  page += F("</div></div><div class=\"effects-box\"><div class=\"effects\">");
  page += advancedButton("SURROUND", learning); page += advancedButton("DELAY", learning); page += advancedButton("TEST", learning);
  page += F("</div></div><div class=\"brand\"><span class=\"brand-onkyo\">ONKYO</span><span class=\"brand-ri\">RI</span><small>REMOTE CONTROL TRANSMITTER <b>RC-209S</b></small></div></div>");
  return page;
}

void handleAdvancedPage() {
  String page = remotePageHead("Pilot Onkyo Advanced");
  page += remoteShellStyle();
  page += renderRemoteShell(false);
  page += F("<a class=\"back\" href=\"/\">Wróć do wersji Basic</a><script>document.querySelectorAll('[data-id]').forEach(b=>b.onclick=()=>fetch('/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(b.dataset.id)}));</script></main></body></html>");
  server.send(200, "text/html", page);
}

void handleRemoteLearnPage() {
  String page = remotePageHead("Nauka pilota Onkyo");
  page += remoteShellStyle();
  page += F("<h1>NAUKA PILOTA — RC-209S</h1><p class=\"hint\">Zielony znacznik oznacza przypisany kod. Po wybraniu przycisku nadajnik IR zostaje wyłączony, więc wzmacniacz nie dostanie żadnej komendy. Naciśnij odpowiednik na oryginalnym pilocie.</p><p id=\"status\">Wybierz przycisk do nauczenia.</p>");
  page += renderRemoteShell(true);
  page += F("<a href=\"/advanced\">Wróć do pilota Advanced</a><script>let current=null;const status=document.querySelector('#status');const buttons=[...document.querySelectorAll('[data-id]')];async function poll(){try{const r=await fetch('/learn/status');const d=await r.json();if(d.learning){current=d.name;buttons.forEach(b=>b.classList.toggle('learning',b.dataset.id===current));status.textContent='Czekam na sygnał: '+current}else if(current){buttons.forEach(b=>b.classList.remove('learning'));const b=buttons.find(x=>x.dataset.id===current);if(b)b.classList.add('saved');status.textContent='Kod zapisany: '+current;current=null}}catch(_){status.textContent='Brak połączenia z pilotem.'}}buttons.forEach(b=>b.onclick=async()=>{const r=await fetch('/learn/start',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(b.dataset.id)});if(!r.ok){status.textContent='Nie można rozpocząć nauki.';return}current=b.dataset.id;buttons.forEach(x=>x.classList.toggle('learning',x===b));status.textContent='Czekam na sygnał: '+current});window.addEventListener('pagehide',()=>{if(current)fetch('/learn/cancel',{method:'POST'})});setInterval(poll,600);</script></main></body></html>");
  server.send(200, "text/html", page);
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_PAGE);
}

void handleManifest() {
  server.send_P(200, "application/manifest+json", PWA_MANIFEST);
}

void handleIcon() {
  server.send_P(200, "image/svg+xml", PWA_ICON);
}

void stopVolume() {
  heldVolumeCommand = nullptr;
}

bool sendOnkyoCommand(const String &name) {
  // Never emit IR while the receiver is learning: this prevents the amplifier
  // from receiving a command selected accidentally in the web UI.
  if (!irTransmissionEnabled || learningCommand != nullptr) return false;
  RemoteFunction *function = findRemoteFunction(name);
  if (function != nullptr) {
    if (function->learnedProtocol != UNKNOWN) {
      IRData learnedData{};
      learnedData.protocol = function->learnedProtocol;
      learnedData.address = function->learnedAddress;
      learnedData.command = function->learnedCommand;
      learnedData.numberOfBits = function->learnedBits;
      Serial.print(">>> Sending learned ");
      Serial.println(name);
      return IrSender.write(&learnedData, NO_REPEATS) != 0;
    }
    const uint8_t value = function->learnedValue != 0 ? function->learnedValue : function->defaultValue;
    if (value == 0) return false;
    Serial.print(">>> Sending ");
    Serial.println(name);
    IrSender.sendNEC(ONKYO_ADDRESS, value, 0);
    return true;
  }
  return false;
}

void handleLearnStart() {
  stopVolume();
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  RemoteFunction *function = findRemoteFunction(server.arg("name"));
  if (function == nullptr) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  learningCommand = function->id;
  irTransmissionEnabled = false;
  learningStartedMs = millis();
  digitalWrite(IR_SEND_PIN, LOW);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLearnCancel() {
  learningCommand = nullptr;
  irTransmissionEnabled = true;
  learningStartedMs = 0;
  digitalWrite(IR_SEND_PIN, LOW);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLearnStatus() {
  if (learningCommand != nullptr) {
    String response = "{\"learning\":true,\"name\":\"";
    response += learningCommand;
    response += "\"}";
    server.send(200, "application/json", response);
    return;
  }
  server.send(200, "application/json", "{\"learning\":false}");
}

void processLearnedCommand() {
  if (learningCommand == nullptr || !IrReceiver.decode()) {
    return;
  }

  const IRData &data = IrReceiver.decodedIRData;
  // Discard the tail/repeat frames of the preceding physical button press.
  // A new slot therefore waits for a fresh, deliberate signal.
  if (millis() - learningStartedMs < LEARN_RECEIVER_SETTLE_MS ||
      (data.flags & IRDATA_FLAGS_IS_REPEAT) != 0) {
    IrReceiver.resume();
    return;
  }
  if (data.protocol != UNKNOWN && (data.flags & IRDATA_FLAGS_WAS_OVERFLOW) == 0) {
    RemoteFunction *function = findRemoteFunction(learningCommand);
    if (function != nullptr) {
      function->learnedProtocol = data.protocol;
      function->learnedAddress = data.address;
      function->learnedCommand = data.command;
      function->learnedBits = data.numberOfBits;
      function->learnedValue = data.command == 0 ? 1 : static_cast<uint8_t>(data.command);
      const String keyBase = function->preferenceKey;
      preferences.putUChar((keyBase + "P").c_str(), static_cast<uint8_t>(data.protocol));
      preferences.putUShort((keyBase + "A").c_str(), data.address);
      preferences.putUShort((keyBase + "C").c_str(), data.command);
      preferences.putUChar((keyBase + "B").c_str(), data.numberOfBits);
      Serial.print(">>> Learned ");
      Serial.print(function->id);
      Serial.print(" protocol=");
      Serial.print(getProtocolString(data.protocol));
      Serial.print(" address=0x");
      Serial.print(data.address, HEX);
      Serial.print(" command=0x");
      Serial.println(data.command, HEX);
    }
    learningCommand = nullptr;
    irTransmissionEnabled = true;
    learningStartedMs = 0;
    digitalWrite(IR_SEND_PIN, LOW);
  }
  IrReceiver.resume();
}

void handleCommand() {
  stopVolume();
  if (!server.hasArg("name") || !sendOnkyoCommand(server.arg("name"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeStart() {
  if (!server.hasArg("direction")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  const String direction = server.arg("direction");
  heldVolumeCommand = direction == "up" ? "VOL+" : direction == "down" ? "VOL-"
                      : direction == "tuning-up" ? "TUNING+" : direction == "tuning-down" ? "TUNING-" : nullptr;
  if (heldVolumeCommand == nullptr || !sendOnkyoCommand(heldVolumeCommand)) {
    stopVolume();
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  lastVolumeSignalMs = millis();
  lastVolumeSendMs = lastVolumeSignalMs;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeKeepalive() {
  if (heldVolumeCommand != nullptr) {
    lastVolumeSignalMs = millis();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeStop() {
  stopVolume();
  server.send(200, "application/json", "{\"ok\":true}");
}

void repeatHeldVolume() {
  if (heldVolumeCommand == nullptr) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastVolumeSignalMs > VOLUME_WATCHDOG_MS) {
    stopVolume();
    return;
  }

  if (now - lastVolumeSendMs >= VOLUME_REPEAT_INTERVAL_MS) {
    sendOnkyoCommand(heldVolumeCommand);
    lastVolumeSendMs = now;
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();
  Serial.print("Wi-Fi connected. Open http://");
  Serial.println(WiFi.localIP());
}

void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setTimeout(OTA_TIMEOUT_SECONDS);
  ArduinoOTA.begin();

  Serial.print("OTA ready. Hostname: ");
  Serial.println(OTA_HOSTNAME);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Retain the known-working IR setup: transmitter on GPIO26, receiver on GPIO27.
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);
  preferences.begin("onkyo-remote", false);
  for (RemoteFunction &function : REMOTE_FUNCTIONS) {
    function.learnedValue = preferences.getUChar(function.preferenceKey, 0);
    const String keyBase = function.preferenceKey;
    const uint8_t savedProtocol = preferences.getUChar((keyBase + "P").c_str(), static_cast<uint8_t>(UNKNOWN));
    if (savedProtocol != static_cast<uint8_t>(UNKNOWN)) {
      function.learnedProtocol = static_cast<decode_type_t>(savedProtocol);
      function.learnedAddress = preferences.getUShort((keyBase + "A").c_str(), 0);
      function.learnedCommand = preferences.getUShort((keyBase + "C").c_str(), 0);
      function.learnedBits = preferences.getUChar((keyBase + "B").c_str(), 0);
    }
  }
  // Firmware Basic used these two keys. Migrate them once so yesterday's
  // learned preset buttons remain available in the Advanced remote.
  RemoteFunction *presetPrevious = findRemoteFunction("PRESET-");
  RemoteFunction *presetNext = findRemoteFunction("PRESET+");
  if (presetPrevious != nullptr && presetPrevious->learnedValue == 0) {
    presetPrevious->learnedValue = preferences.getUChar("presetPrev", 0);
    if (presetPrevious->learnedValue != 0) preferences.putUChar(presetPrevious->preferenceKey, presetPrevious->learnedValue);
  }
  if (presetNext != nullptr && presetNext->learnedValue == 0) {
    presetNext->learnedValue = preferences.getUChar("presetNext", 0);
    if (presetNext->learnedValue != 0) preferences.putUChar(presetNext->preferenceKey, presetNext->learnedValue);
  }

  connectToWiFi();
  startOta();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/advanced", HTTP_GET, handleAdvancedPage);
  server.on("/manifest.webmanifest", HTTP_GET, handleManifest);
  server.on("/icon.svg", HTTP_GET, handleIcon);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/volume/start", HTTP_POST, handleVolumeStart);
  server.on("/volume/keepalive", HTTP_POST, handleVolumeKeepalive);
  server.on("/volume/stop", HTTP_POST, handleVolumeStop);
  server.on("/learn/start", HTTP_POST, handleLearnStart);
  server.on("/learn/cancel", HTTP_POST, handleLearnCancel);
  server.on("/learn/status", HTTP_GET, handleLearnStatus);
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  repeatHeldVolume();
  processLearnedCommand();
}
