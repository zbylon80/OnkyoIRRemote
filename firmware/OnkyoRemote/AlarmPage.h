#pragma once
const char ALARM_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#161616"><title>Onkyo · budzik</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;background:#161616;color:#eeeae2}main{max-width:430px;margin:auto;padding:24px 18px}header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px}h1{font-size:1.35rem;margin:0}a{color:#d1c8ba;text-decoration:none}p{font-size:.8rem;color:#bbb5aa;line-height:1.4}section{padding:16px;margin:16px 0;border:1px solid #555;border-radius:8px;background:#202020}h2{margin:0 0 16px;font-size:1.1rem}.on{border-left:3px solid #8cae86}.off{border-left:3px solid #9e8582}label{display:flex;align-items:center;justify-content:space-between;gap:16px;margin:12px 0;font-size:.9rem}input,select{padding:8px;font:inherit;min-height:44px;background:#151515;border:1px solid #777;border-radius:4px;color:inherit}input{width:118px}select{min-width:0;width:160px}button{min-height:44px;padding:10px 16px;font:inherit;background:#333;color:inherit;border:1px solid #777;border-radius:5px;cursor:pointer}button:disabled,fieldset:disabled{opacity:.6}.actions{display:flex;gap:10px;margin-top:16px}.set{flex:1;background:#465d40;border-color:#8cae86}.off .set{background:#554340;border-color:#9e8582}.status{margin:12px 0 0;min-height:1.2em}#result{min-height:1.2em}[hidden]{display:none!important}button:focus-visible,input:focus-visible,select:focus-visible,a:focus-visible{outline:2px solid #e9d7ab;outline-offset:3px}.footer{display:flex;align-items:center;justify-content:space-between;margin-top:20px}#refresh{padding:6px 10px;font-size:.8rem}
.time-row{display:flex;align-items:center;justify-content:space-between;gap:16px;margin:12px 0;font-size:.9rem}.time-picker{display:flex;align-items:center;justify-content:space-between;gap:6px;width:160px;flex-shrink:0}.time-picker select{width:70px;padding:8px 4px;text-align:center;font-variant-numeric:tabular-nums}
</style></head><body><main>
<header><h1>Budzik</h1><a href="/">← Pilot</a></header>
<p id="clock" role="status">Odczytywanie czasu ESP32…</p>
<fieldset id="fields" disabled style="border:0;margin:0;padding:0">
<form id="on-form"><section class="on"><h2>☀ Włącz</h2><div class="time-row"><span>Godzina</span><div class="time-picker" role="group" aria-label="Czas włączenia, format 24-godzinny"><select id="on-hour" aria-label="Godzina włączenia" required></select><span aria-hidden="true">:</span><select id="on-minute" aria-label="Minuty włączenia" required></select></div></div><label>Źródło<select id="source"><option>TUNER</option><option>CD</option><option>PHONO</option><option>TAPE-1</option><option>TAPE-2</option><option>VIDEO-1</option><option>VIDEO-2</option></select></label><div class="actions"><button id="on-set" class="set" type="submit">Ustaw</button><button id="on-cancel" type="button" hidden aria-label="Anuluj włączenie" title="Anuluj włączenie">×</button></div><p id="on-status" class="status"></p></section></form>
<form id="off-form"><section class="off"><h2>☾ Wyłącz</h2><div class="time-row"><span>Godzina</span><div class="time-picker" role="group" aria-label="Czas wyłączenia, format 24-godzinny"><select id="off-hour" aria-label="Godzina wyłączenia" required></select><span aria-hidden="true">:</span><select id="off-minute" aria-label="Minuty wyłączenia" required></select></div></div><div class="actions"><button id="off-set" class="set" type="submit">Ustaw</button><button id="off-cancel" type="button" hidden aria-label="Anuluj wyłączenie" title="Anuluj wyłączenie">×</button></div><p id="off-status" class="status"></p></section></form>
</fieldset>
<p id="result" role="status" aria-live="polite"></p>
<div class="footer"><span style="font-size:.75rem;color:#aaa">Każda akcja wykona się raz.</span><button id="refresh" type="button">Odśwież</button></div>
<script>
const field=id=>document.getElementById(id);
function setTime(id,time){const [hour,minute]=time.split(':');field(id+'-hour').value=hour;field(id+'-minute').value=minute;}
function readTime(id){return field(id+'-hour').value+':'+field(id+'-minute').value;}
for(const id of ['on','off']){
  for(const [part,count] of [['hour',24],['minute',60]]){
    for(let i=0;i<count;i++){const value=String(i).padStart(2,'0');field(id+'-'+part).add(new Option(value,value));}
  }
  setTime(id,id==='on'?'07:00':'02:00');
}
let busy=false,loaded=false,storageReady=false,clockReady=false;
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
  if(replace==='all'||replace==='on')field('source').value=data.on.source;
  if(!storageReady)field('result').textContent='Pamięć ESP32 niedostępna. Budzik zatrzymany.';
}
function setBusy(value){
  busy=value;field('refresh').disabled=value;field('fields').disabled=value||!loaded||!storageReady;
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
}
field('refresh').addEventListener('click',load);
load();
</script></main></body></html>
)HTML";
