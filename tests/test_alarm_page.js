const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const html=fs.readFileSync(path.join(__dirname,'../firmware/OnkyoRemote/AlarmPage.h'),'utf8');
const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
assert.ok(!html.includes('type="checkbox"')&&!html.includes('Codziennie'));
assert.ok(!html.includes('type="time"'));
const fields=new Map(),requests=[],timers=new Map();let timerId=0;
function field(id){if(!fields.has(id))fields.set(id,{disabled:false,value:'',hidden:false,textContent:'',options:[],handlers:{},add(option){this.options.push(option);},addEventListener(name,fn){this.handlers[name]=fn;}});return fields.get(id);}
const flush=async()=>{for(let i=0;i<15;i++)await Promise.resolve();};
vm.runInNewContext(source,{document:{getElementById:field},AbortController,URLSearchParams,Option:class{constructor(text,value){this.text=text;this.value=value;}},
  setTimeout(fn){const id=++timerId;timers.set(id,fn);return id;},clearTimeout(id){timers.delete(id);},
  fetch(url,options){assert.equal(url,'/alarms');return new Promise((resolve,reject)=>{options.signal.addEventListener('abort',()=>reject(Error('timeout')));requests.push({options,reject,finish(data,ok=true){resolve({ok,json:async()=>data});}});});}
});
const snapshot={clockReady:true,storageReady:true,localTime:'2026-10-07 12:00:00',off:{enabled:false,time:'02:00',date:''},on:{enabled:false,time:'07:00',date:'',source:'TUNER'}};
const submit=id=>field(id+'-form').handlers.submit({preventDefault(){}}),refresh=()=>field('refresh').handlers.click();
const body=index=>Object.fromEntries(new URLSearchParams(requests[index].options.body));
(async()=>{
  for(const id of ['on','off']){
    assert.equal(field(id+'-hour').options.length,24);assert.equal(field(id+'-hour').options[0].value,'00');assert.equal(field(id+'-hour').options[23].value,'23');
    assert.equal(field(id+'-minute').options.length,60);assert.equal(field(id+'-minute').options[0].value,'00');assert.equal(field(id+'-minute').options[59].value,'59');
  }
  assert.equal(requests.length,1);assert.equal(requests[0].options.method,'GET');submit('on');assert.equal(requests.length,1);
  requests[0].finish(snapshot);await flush();assert.equal(field('on-set').disabled,false);assert.equal(field('on-cancel').hidden,true);
  field('on-hour').value='23';field('on-minute').value='59';field('source').value='CD';field('off-hour').value='00';field('off-minute').value='00';
  submit('on');submit('off');assert.equal(requests.length,2);
  assert.deepEqual(body(1),{action:'on',onTime:'23:59',source:'CD'});
  const onSaved={...snapshot,on:{enabled:true,time:'23:59',date:'2026-10-07',source:'CD'}};
  requests[1].finish(onSaved);await flush();
  assert.equal(field('off-hour').value,'00');assert.equal(field('off-minute').value,'00');assert.equal(field('on-cancel').hidden,false);assert.ok(field('on-status').textContent.includes('07.10 o 23:59'));
  submit('off');assert.deepEqual(body(2),{action:'off',offTime:'00:00'});
  const both={...onSaved,off:{enabled:true,time:'00:00',date:'2026-10-08'}};
  requests[2].finish(both);await flush();assert.equal(field('off-cancel').hidden,false);
  field('on-cancel').handlers.click();assert.deepEqual(body(3),{action:'cancelOn'});
  requests[3].finish({...both,on:{...both.on,enabled:false}});await flush();assert.equal(field('on-cancel').hidden,true);assert.equal(field('off-cancel').hidden,false);
  submit('on');requests[4].reject(Error('connection lost'));await flush();assert.equal(field('fields').disabled,true);
  submit('on');assert.equal(requests.length,5);refresh();requests[5].finish({...both,storageReady:false});await flush();assert.equal(field('fields').disabled,true);
  refresh();requests[6].finish({...both,clockReady:false});await flush();assert.equal(field('on-set').disabled,true);assert.equal(field('fields').disabled,false);
  submit('on');assert.equal(requests.length,7);field('off-cancel').handlers.click();assert.deepEqual(body(7),{action:'cancelOff'});
  requests[7].finish({...both,clockReady:false,off:{...both.off,enabled:false}});await flush();
  refresh();[...timers.values()][0]();await flush();assert.equal(field('fields').disabled,true);assert.equal(field('refresh').disabled,false);assert.equal(timers.size,0);
  console.log('Minimal alarm panel: independent Set/Cancel, no checkbox, preserved draft, errors and NTP/storage guards passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
