#pragma once
const char ALARM_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#161616"><title>Onkyo · budzik</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;background:#161616;color:#eeeae2}main{max-width:430px;margin:auto;padding:24px 18px}header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px}h1{font-size:1.35rem;margin:0}a{color:#d1c8ba;text-decoration:none}p{font-size:.8rem;color:#bbb5aa;line-height:1.4}section{padding:16px;margin:16px 0;border:1px solid #555;border-radius:8px;background:#202020}h2{margin:0 0 16px;font-size:1.1rem}.on{border-left:3px solid #8cae86}.off{border-left:3px solid #9e8582}label{display:flex;align-items:center;justify-content:space-between;gap:16px;margin:12px 0;font-size:.9rem}input,select{padding:8px;font:inherit;min-height:44px;background:#151515;border:1px solid #777;border-radius:4px;color:inherit}input{width:118px}select{min-width:0;width:160px}button{min-height:44px;padding:10px 16px;font:inherit;background:#333;color:inherit;border:1px solid #777;border-radius:5px;cursor:pointer}button:disabled,fieldset:disabled{opacity:.6}.actions{display:flex;gap:10px;margin-top:16px}.set{flex:1;background:#465d40;border-color:#8cae86}.off .set{background:#554340;border-color:#9e8582}.status{margin:12px 0 0;min-height:1.2em}#result{min-height:1.2em}[hidden]{display:none!important}button:focus-visible,input:focus-visible,select:focus-visible,a:focus-visible{outline:2px solid #e9d7ab;outline-offset:3px}.footer{display:flex;align-items:center;justify-content:space-between;margin-top:20px}#refresh{padding:6px 10px;font-size:.8rem}
.time-row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:12px 0;font-size:.9rem}.picker{width:156px;flex-shrink:0;display:flex;align-items:center;justify-content:space-between;background:#292929;border-color:#555;border-radius:8px;padding:8px 12px}.picker::after{content:'';width:7px;height:7px;border-right:1.6px solid #8cae86;border-bottom:1.6px solid #8cae86;transform:rotate(45deg);margin-left:12px;margin-top:-4px}.time-button{font:28px ui-monospace,Consolas,monospace;font-variant-numeric:tabular-nums}.picker:hover{border-color:#8cae86}button:hover{border-color:#8cae86}dialog{width:min(calc(100% - 32px),340px);max-height:calc(100vh - 32px);max-height:calc(100dvh - 32px);overflow:auto;padding:20px;border:1px solid #65655d;border-radius:12px;background:#202020;color:#eeeae2}dialog::backdrop{background:#0008}dialog h2{font-size:1.1rem;margin:0 0 4px}dialog p{margin:0 0 16px}.digits{display:grid;grid-template-columns:1fr 32px 1fr;align-items:center;margin-top:16px}.part{display:flex;flex-direction:column;gap:6px}.part span{text-align:center;font-size:.7rem;color:#bbb5aa;letter-spacing:.04em;margin-bottom:4px}.part button{background:#292929;border-color:#555;border-radius:8px;min-height:48px}.part input{width:100%;height:72px;border:0;border-bottom:2px solid #555;border-radius:5px 5px 0 0;background:#151515;text-align:center;font:42px ui-monospace,Consolas,monospace;min-width:0;padding:0;color:#eeeae2}.part input:focus{outline:0;border-bottom-color:#e9d7ab}.part input::selection{background:#6c8764}.colon{text-align:center;font-size:38px;color:#bbb5aa}.dialog-actions{display:flex;gap:10px;margin-top:12px}.dialog-actions button{flex:1;border-radius:8px;background:#292929}.dialog-actions .done{background:#465d40;border-color:#8cae86}#time-error{color:#e7aaa1;margin:10px 0 0;min-height:1.2em}.source-option{display:block;width:100%;text-align:left;margin:8px 0 0;background:#292929;border-color:#555;border-radius:8px}.source-option[aria-pressed=true]{background:#465d40;border-color:#8cae86}
</style></head><body><main>
<header><h1>Budzik</h1><a href="/">← Pilot</a></header>
<p id="clock" role="status">Odczytywanie czasu ESP32…</p>
<fieldset id="fields" disabled style="border:0;margin:0;padding:0">
<form id="on-form"><section class="on"><h2>☀ Włącz</h2><div class="time-row"><span>Godzina</span><button id="on-time" type="button" class="picker time-button">07:00</button><input id="on-hour" type="hidden"><input id="on-minute" type="hidden"></div><div class="time-row"><span>Źródło</span><button id="source-button" type="button" class="picker" aria-label="Źródło pobudki">TUNER</button><input id="source" type="hidden" value="TUNER"></div><div class="actions"><button id="on-set" class="set" type="submit">Ustaw</button><button id="on-cancel" type="button" hidden aria-label="Anuluj włączenie" title="Anuluj włączenie">×</button></div><p id="on-status" class="status"></p></section></form>
<form id="off-form"><section class="off"><h2>☾ Wyłącz</h2><div class="time-row"><span>Godzina</span><button id="off-time" type="button" class="picker time-button">02:00</button><input id="off-hour" type="hidden"><input id="off-minute" type="hidden"></div><div class="actions"><button id="off-set" class="set" type="submit">Ustaw</button><button id="off-cancel" type="button" hidden aria-label="Anuluj wyłączenie" title="Anuluj wyłączenie">×</button></div><p id="off-status" class="status"></p></section></form>
</fieldset>
<p id="result" role="status" aria-live="polite"></p>
<div class="footer"><span style="font-size:.75rem;color:#aaa">Każda akcja wykona się raz.</span><button id="refresh" type="button">Odśwież</button></div>
<dialog id="time-dialog" aria-labelledby="time-title"><h2 id="time-title">Godzina włączenia</h2><p>Format 24-godzinny</p><div class="digits">
<div class="part"><span>GODZINA</span><button id="time-hour-up" type="button" aria-label="Zmniejsz godzinę">▲</button><input id="time-hour" type="text" inputmode="numeric" maxlength="2" autocomplete="off" aria-label="Godzina, od 00 do 23"><button id="time-hour-down" type="button" aria-label="Zwiększ godzinę">▼</button></div><span class="colon" aria-hidden="true">:</span>
<div class="part"><span>MINUTA</span><button id="time-minute-up" type="button" aria-label="Zmniejsz minutę">▲</button><input id="time-minute" type="text" inputmode="numeric" maxlength="2" autocomplete="off" aria-label="Minuta, od 00 do 59"><button id="time-minute-down" type="button" aria-label="Zwiększ minutę">▼</button></div></div>
<p id="time-error" role="status" aria-live="polite"></p><div class="dialog-actions"><button id="time-cancel" type="button">Anuluj</button><button id="time-done" type="button" class="done">Gotowe</button></div></dialog>
<dialog id="source-dialog" aria-labelledby="source-title"><h2 id="source-title">Źródło pobudki</h2>
<button id="source-TUNER" type="button" class="source-option">TUNER</button><button id="source-CD" type="button" class="source-option">CD</button><button id="source-PHONO" type="button" class="source-option">PHONO</button><button id="source-TAPE-1" type="button" class="source-option">TAPE-1</button><button id="source-TAPE-2" type="button" class="source-option">TAPE-2</button><button id="source-VIDEO-1" type="button" class="source-option">VIDEO-1</button><button id="source-VIDEO-2" type="button" class="source-option">VIDEO-2</button><div class="dialog-actions"><button id="source-cancel" type="button">Anuluj</button></div></dialog>
<script>
const field=id=>document.getElementById(id);
function setTime(id,time){const [hour,minute]=time.split(':');field(id+'-hour').value=hour;field(id+'-minute').value=minute;field(id+'-time').textContent=time;field(id+'-time').setAttribute('aria-label',(id==='on'?'Godzina włączenia: ':'Godzina wyłączenia: ')+time);}
function readTime(id){return field(id+'-hour').value+':'+field(id+'-minute').value;}
for(const id of ['on','off']){
  setTime(id,id==='on'?'07:00':'02:00');
}
let busy=false,loaded=false,storageReady=false,clockReady=false;
let editing='on';
const sources=['TUNER','CD','PHONO','TAPE-1','TAPE-2','VIDEO-1','VIDEO-2'];
function setSource(value){field('source').value=value;field('source-button').textContent=value;field('source-button').setAttribute('aria-label','Źródło pobudki: '+value);for(const source of sources)field('source-'+source).setAttribute('aria-pressed',String(source===value));}
function number(part){const value=field('time-'+part).value;const count=part==='hour'?24:60;return /^[0-9]{1,2}$/.test(value)&&Number(value)<count?Number(value):-1;}
function step(part,change){const count=part==='hour'?24:60;let value=number(part);if(value<0)value=Number(field(editing+'-'+part).value);field('time-'+part).value=String((value+change+count)%count).padStart(2,'0');field('time-error').textContent='';}
function openTime(id){if(busy||!loaded||!storageReady)return;editing=id;field('time-title').textContent=id==='on'?'Godzina włączenia':'Godzina wyłączenia';field('time-hour').value=field(id+'-hour').value;field('time-minute').value=field(id+'-minute').value;field('time-error').textContent='';field('time-dialog').showModal();}
function doneTime(){const hour=number('hour'),minute=number('minute');if(hour<0||minute<0){field('time-error').textContent='Wpisz godzinę 00–23 i minuty 00–59.';return;}setTime(editing,String(hour).padStart(2,'0')+':'+String(minute).padStart(2,'0'));field('time-dialog').close();}
for(const part of ['hour','minute']){
  field('time-'+part+'-up').addEventListener('click',()=>step(part,-1));field('time-'+part+'-down').addEventListener('click',()=>step(part,1));
  field('time-'+part).addEventListener('focus',()=>field('time-'+part).select());
  field('time-'+part).addEventListener('keydown',event=>{if(event.key==='ArrowDown'||event.key==='ArrowUp'){event.preventDefault();step(part,event.key==='ArrowDown'?1:-1);}else if(event.key==='Enter'){event.preventDefault();doneTime();}});
  field('time-'+part).addEventListener('wheel',event=>{if(document.activeElement!==field('time-'+part)||event.deltaY===0)return;event.preventDefault();step(part,event.deltaY>0?1:-1);},{passive:false});
}
field('time-done').addEventListener('click',doneTime);field('time-cancel').addEventListener('click',()=>field('time-dialog').close());
field('source-button').addEventListener('click',()=>{if(!busy&&loaded&&storageReady)field('source-dialog').showModal();});
field('source-cancel').addEventListener('click',()=>field('source-dialog').close());
for(const source of sources)field('source-'+source).addEventListener('click',()=>{setSource(source);field('source-dialog').close();});
setSource('TUNER');
async function request(method,body){
  const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),3000);
  try{
    const response=await fetch('/alarms',{method,cache:'no-store',signal:controller.signal,...(body?{headers:{'Content-Type':'application/x-www-form-urlencoded'},body}:{})});
    const data=await response.json();
    if(!response.ok)throw new Error(data.error||'ESP32 odrzuciło żądanie');
    return data;
  }finally{clearTimeout(timer);}
}
function display(data,replace='all'){
  clockReady=data.clockReady; storageReady=data.storageReady; loaded=true;
  field('clock').textContent=clockReady?'Czas ESP32: '+data.localTime.slice(11)+' · Polska':'Zegar ESP32 czeka na synchronizację.';
  for(const id of ['on','off']){
    const slot=data[id];
    if(replace==='all'||replace===id)setTime(id,slot.time);
    const date=slot.date?slot.date.slice(8,10)+'.'+slot.date.slice(5,7):'';
    field(id+'-status').textContent=slot.enabled?'Ustawiono: '+date+' o '+slot.time+'.':'Nie ustawiono.';
    field(id+'-cancel').hidden=!slot.enabled;
  }
  if(replace==='all'||replace==='on')setSource(data.on.source);
  if(!storageReady)field('result').textContent='Pamięć ESP32 niedostępna. Budzik zatrzymany.';
}
function setBusy(value){
  busy=value;field('refresh').disabled=value;field('fields').disabled=value||!loaded||!storageReady;
  if(value)for(const id of ['time-dialog','source-dialog'])if(field(id).open)field(id).close();
  for(const id of ['on','off'])field(id+'-set').disabled=value||!loaded||!storageReady||!clockReady;
}
async function load(){
  if(busy)return;setBusy(true);field('result').textContent='Odczytywanie…';
  try{display(await request('GET'));if(storageReady)field('result').textContent='';}
  catch(error){loaded=false;field('result').textContent='Nie można odczytać ustawień. '+error.message;}
  finally{setBusy(false);}
}
async function save(id,cancel){
  if(busy||!loaded||!storageReady||(!cancel&&!clockReady))return;
  const body={action:cancel?(id==='on'?'cancelOn':'cancelOff'):id};
  if(!cancel){body[id+'Time']=readTime(id);if(id==='on')body.source=field('source').value;}
  setBusy(true);field('result').textContent='Zapisywanie…';
  try{display(await request('POST',new URLSearchParams(body).toString()),id);field('result').textContent=cancel?'Anulowano.':'Ustawione. Możesz wrócić do pilota.';}
  catch(error){loaded=false;field('result').textContent='Brak potwierdzenia. '+error.message+' Odśwież ustawienia.';}
  finally{setBusy(false);}
}
for(const id of ['on','off']){
  field(id+'-form').addEventListener('submit',event=>{event.preventDefault();save(id,false);});
  field(id+'-cancel').addEventListener('click',()=>save(id,true));
  field(id+'-time').addEventListener('click',()=>openTime(id));
}
field('refresh').addEventListener('click',load);
load();
</script></main></body></html>
)HTML";
