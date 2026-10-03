#pragma once
#include <Arduino.h>

// Übersichtsseite (wird im Access Point automatisch geöffnet)
const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zeiterfassung</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7280;--acc:#1f7a4d;--run:#1f7a4d;--stop:#b42318;--line:#e5e7eb}
@media (prefers-color-scheme:dark){:root{--bg:#111418;--card:#1b2027;--fg:#e8eaed;--mut:#9aa3ad;--acc:#4ec38a;--run:#4ec38a;--stop:#f97066;--line:#2c333c}}
*{box-sizing:border-box}body{margin:0;font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:720px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 14px}h2{font-size:17px;margin:0 0 10px}
.card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.status{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.badge{font-weight:700;font-size:20px}.badge.run{color:var(--run)}.badge.stop{color:var(--mut)}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.kpi{background:var(--bg);border-radius:10px;padding:10px;text-align:center}
.kpi b{display:block;font-size:24px;font-variant-numeric:tabular-nums}.kpi span{color:var(--mut);font-size:13px}
button,.btn{font:inherit;border:0;border-radius:10px;padding:10px 14px;background:var(--acc);color:#fff;cursor:pointer;text-decoration:none;display:inline-block;text-align:center}
.btn.sec,button.sec{background:var(--line);color:var(--fg)}button.del{background:none;color:var(--stop);padding:4px 8px}
.nav{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}
td,th{padding:6px 4px;border-bottom:1px solid var(--line);text-align:left}th{color:var(--mut);font-weight:600;font-size:13px}
td.r,th.r{text-align:right}tr.we td{color:var(--mut)}
.bar{height:8px;background:var(--acc);border-radius:4px;min-width:2px}
.dl{display:flex;flex-wrap:wrap;gap:8px}
.hint{color:var(--mut);font-size:13px;margin-top:8px}
.warn{background:#fff4e5;color:#7a4a00;border-radius:10px;padding:10px;margin-bottom:14px}
@media (prefers-color-scheme:dark){.warn{background:#3a2a10;color:#ffd79a}}
form{display:grid;grid-template-columns:1fr 1fr 1fr auto;gap:8px;align-items:end}
form label{font-size:13px;color:var(--mut)}input{font:inherit;width:100%;padding:8px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg)}
button.x{background:none;color:var(--stop);font-size:20px;font-weight:700;line-height:1;padding:2px 8px}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.45);display:flex;align-items:center;justify-content:center;padding:16px;z-index:10}
.modal[hidden]{display:none}.mbox{background:var(--card);border-radius:14px;padding:20px;max-width:360px;width:100%;text-align:center}
.mbox h3{margin:0 0 8px;font-size:20px}.mbox p{margin:0 0 18px}.mrow{display:flex;gap:10px}.mrow button{flex:1;font-size:18px}
.net{display:flex;align-items:center;gap:10px;padding:10px 4px;border-bottom:1px solid var(--line);cursor:pointer}
.net:hover{background:var(--bg)}.net .n{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sig{font-family:monospace;letter-spacing:-2px;color:var(--acc)}.tag{font-size:12px;color:var(--mut)}
.wform{grid-template-columns:1fr 1fr auto}.ok{color:var(--acc)}.err{color:var(--stop)}
@media (max-width:520px){.grid{grid-template-columns:1fr 1fr 1fr}.kpi b{font-size:19px}form,.wform{grid-template-columns:1fr 1fr}}
</style></head><body><main>
<h1>Zeiterfassung</h1>
<div id="warn"></div>
<div class="card">
 <div class="status"><div><div id="badge" class="badge">…</div><div id="since" class="hint"></div></div>
 <button id="toggle">…</button></div>
 <div class="grid" style="margin-top:14px">
  <div class="kpi"><b id="kToday">–</b><span>Heute</span></div>
  <div class="kpi"><b id="kWeek">–</b><span>Diese Woche</span></div>
  <div class="kpi"><b id="kMonth">–</b><span>Dieser Monat</span></div>
 </div>
 <div id="dev" class="hint"></div>
</div>

<div class="card">
 <div class="nav"><button class="sec" id="prev">&#8249;</button><h2 id="mTitle" style="margin:0"></h2><button class="sec" id="next">&#8250;</button></div>
 <div class="grid" style="grid-template-columns:1fr 1fr;margin-bottom:12px">
  <div class="kpi"><b id="mTotal">–</b><span>Summe Monat</span></div>
  <div class="kpi"><b id="mDays">–</b><span>Arbeitstage</span></div>
 </div>
 <h2>Wochen</h2><table id="weeks"></table>
 <h2 style="margin-top:16px">Tage</h2><table id="days"></table>
</div>

<div class="card"><h2>Einträge</h2><table id="sess"></table>
 <h2 style="margin-top:16px">Eintrag nachtragen</h2>
 <form id="add"><div><label>Datum</label><input type="date" id="aD" required></div>
 <div><label>Beginn</label><input type="time" id="aS" required></div>
 <div><label>Ende</label><input type="time" id="aE" required></div>
 <button>Speichern</button></form>
</div>

<div class="card"><h2>Download</h2>
 <div class="dl"><a class="btn" id="xM" href="#">Excel – Monat</a><a class="btn" href="/export.xlsx">Excel – alles</a>
 <a class="btn sec" id="cM" href="#">CSV – Monat</a></div>
 <p class="hint">Falls der Download im automatisch geöffneten Fenster nicht startet: im Browser
 <b>http://192.168.4.1</b> öffnen.</p>
</div>
<div class="card"><h2>WLAN für Datum &amp; Uhrzeit</h2>
 <div id="wCur" class="hint" style="margin:0 0 8px"></div>
 <table id="wSaved"></table>
 <button class="sec" id="wScan" style="margin-top:12px">Netzwerke in der Nähe suchen</button>
 <div id="wList"></div>
 <form id="wForm" class="wform" style="margin-top:12px">
  <div><label>Netzwerk (SSID)</label><input id="wS" maxlength="32" required autocomplete="off"></div>
  <div><label>Passwort</label><input id="wP" type="password" maxlength="63" autocomplete="off"></div>
  <button>Speichern &amp; verbinden</button>
 </form>
 <label class="hint" style="display:block"><input type="checkbox" id="wShow" style="width:auto"> Passwort anzeigen</label>
 <div id="wMsg" class="hint"></div>
 <p class="hint">Das Gerät verbindet sich automatisch mit dem stärksten bekannten Netz und holt Datum und Uhrzeit.
 Die Verbindung zum Hotspot kann dabei kurz unterbrochen werden – Seite dann neu laden.</p>
</div>
<div class="card"><h2>Zurücksetzen</h2>
 <p class="hint" style="margin-top:0">Löscht <b>alle</b> erfassten Zeiten und beendet eine laufende Erfassung.
 Vorher am besten „Excel – alles“ herunterladen.</p>
 <button id="reset" style="background:var(--stop)">Alle Daten löschen</button>
</div>
</main>
<div id="modal" class="modal" hidden><div class="mbox"><h3 id="mT"></h3><p id="mB"></p>
 <div class="mrow"><button class="sec" id="mNo">Nein</button><button id="mYes" style="background:var(--stop)">Ja</button></div></div></div>
<script>
const $=id=>document.getElementById(id);
const WD=['So','Mo','Di','Mi','Do','Fr','Sa'];
const MN=['Januar','Februar','März','April','Mai','Juni','Juli','August','September','Oktober','November','Dezember'];
const dur=s=>{s=Math.floor(s/60);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')};
const hm=t=>{const d=new Date(t*1000);return String(d.getHours()).padStart(2,'0')+':'+String(d.getMinutes()).padStart(2,'0')};
const dd=t=>{const d=new Date(t*1000);return String(d.getDate()).padStart(2,'0')+'.'+String(d.getMonth()+1).padStart(2,'0')+'.'};
const esc=t=>String(t).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'})[c]);
const ddy=t=>{const d=new Date(t*1000);return dd(t)+d.getFullYear()};
function ask(title,html){return new Promise(res=>{$('mT').textContent=title;$('mB').innerHTML=html;$('modal').hidden=false;
 const done=v=>{$('modal').hidden=true;res(v)};$('mYes').onclick=()=>done(true);$('mNo').onclick=()=>done(false);
 $('modal').onclick=e=>{if(e.target.id=='modal')done(false)}})}
const post=(u,data)=>api(u,{method:'POST',body:new URLSearchParams(data)});
let st=null,cy,cm;
async function api(u,o){const r=await fetch(u,o);if(!r.ok)throw new Error(await r.text());return r.json()}
async function setTime(){st=await api('/api/settime?t='+Math.floor(Date.now()/1000),{method:'POST'});}
async function loadStatus(){
 st=await api('/api/status');
 if(!st.valid){await setTime();}
 const diff=Math.abs(st.now-Date.now()/1000);
 $('warn').innerHTML=diff>120?'<div class="warn">Die Uhr des Geräts weicht ab ('+new Date(st.now*1000).toLocaleString('de-DE')+'). <button id="fix">Uhrzeit vom Handy übernehmen</button></div>':'';
 if($('fix'))$('fix').onclick=async()=>{await setTime();refresh()};
 $('badge').textContent=st.running?'● Läuft':'Pausiert';
 $('badge').className='badge '+(st.running?'run':'stop');
 $('since').textContent=st.running?('seit '+hm(st.since)+' Uhr – '+dur(st.now-st.since)):'';
 $('toggle').textContent=st.running?'Stoppen':'Starten';
 $('toggle').style.background=st.running?'var(--stop)':'var(--acc)';
 $('kToday').textContent=dur(st.today||0);$('kWeek').textContent=dur(st.week||0);$('kMonth').textContent=dur(st.month||0);
 $('dev').textContent='Akku '+st.bat+' % ('+st.batV+' V)'+(st.sta?' · WLAN '+st.staIp:'')+(st.ap?' · Access Point aktiv':'')+' · '+st.count+' Einträge';
}
async function loadMonth(){
 const d=await api('/api/month?y='+cy+'&m='+cm);
 $('mTitle').textContent=MN[cm-1]+' '+cy;
 $('mTotal').textContent=dur(d.total);
 const max=Math.max(1,...d.days);let wd=0,h='';
 d.days.forEach((s,i)=>{if(s>0)wd++;const t=new Date(cy,cm-1,i+1);const w=t.getDay();
  h+='<tr class="'+(w==0||w==6?'we':'')+'"><td>'+WD[w]+' '+String(i+1).padStart(2,'0')+'.</td><td style="width:50%"><div class="bar" style="width:'+(s?Math.max(2,s/max*100):0)+'%"></div></td><td class="r">'+(s?dur(s):'')+'</td></tr>'});
 $('days').innerHTML=h;$('mDays').textContent=wd;
 $('weeks').innerHTML='<tr><th>KW</th><th>ab</th><th class="r">Stunden</th></tr>'+d.weeks.map(w=>'<tr><td>KW '+w.kw+'</td><td>'+dd(w.from)+'</td><td class="r">'+dur(w.sec)+'</td></tr>').join('');
 $('sess').innerHTML='<tr><th>Datum</th><th>Beginn</th><th>Ende</th><th class="r">Dauer</th><th></th></tr>'+(d.sessions.length?d.sessions.slice().reverse().map(s=>
  '<tr><td>'+WD[new Date(s.s*1000).getDay()]+' '+dd(s.s)+'</td><td>'+hm(s.s)+'</td><td>'+(s.run?'läuft':hm(s.e))+'</td><td class="r">'+dur((s.run?st.now:s.e)-s.s)+'</td><td class="r">'+(s.run?'':'<button class="x" title="Löschen" data-s="'+s.s+'" data-e="'+s.e+'">&#10005;</button>')+'</td></tr>').join(''):'<tr><td colspan="5" class="hint">Keine Einträge</td></tr>');
 document.querySelectorAll('#sess button.x').forEach(b=>b.onclick=async()=>{const a=+b.dataset.s,e=+b.dataset.e;
  if(!await ask('Sicher löschen?','<b>'+WD[new Date(a*1000).getDay()]+' '+ddy(a)+'</b><br>'+hm(a)+' – '+hm(e)+' Uhr ('+dur(e-a)+' h)'))return;
  try{await api('/api/delete?s='+a,{method:'POST'})}catch(x){alert(x.message)}refresh()});
 const q='?y='+cy+'&m='+cm;$('xM').href='/export.xlsx'+q;$('cM').href='/export.csv'+q;
}
async function refresh(){try{await loadStatus();await loadMonth();await loadWifi()}catch(e){console.log(e)}}
$('toggle').onclick=async()=>{try{await api('/api/toggle',{method:'POST'})}catch(e){alert(e.message)}refresh()};
$('reset').onclick=async()=>{
 if(!await ask('Sicher löschen?','Wirklich <b>alle</b> erfassten Zeiten löschen?'))return;
 if(prompt('Zur Sicherheit bitte LÖSCHEN eintippen:')!=='LÖSCHEN')return alert('Abgebrochen – nichts gelöscht.');
 try{await api('/api/reset?confirm=1',{method:'POST'});alert('Alle Daten gelöscht.')}catch(e){alert(e.message)}refresh()};
async function loadWifi(){
 const w=await api('/api/wifi');
 $('wCur').innerHTML=w.current?'<span class="ok">● Verbunden mit <b>'+esc(w.current)+'</b></span>':'Zurzeit nicht mit einem WLAN verbunden.';
 let h='<tr><th>Gespeicherte Netzwerke</th><th></th></tr>';
 w.saved.forEach(n=>h+='<tr><td>'+esc(n)+'</td><td class="r"><button class="x" title="Entfernen" data-n="'+esc(n)+'">&#10005;</button></td></tr>');
 if(!w.saved.includes(w.builtin))h+='<tr><td>'+esc(w.builtin)+'</td><td class="r tag">fest eingebaut</td></tr>';
 $('wSaved').innerHTML=h;
 document.querySelectorAll('#wSaved button.x').forEach(b=>b.onclick=async()=>{
  if(!await ask('Sicher löschen?','WLAN <b>'+esc(b.dataset.n)+'</b> entfernen?'))return;
  try{await post('/api/wifi/del',{ssid:b.dataset.n})}catch(x){alert(x.message)}loadWifi()});
}
const bars=r=>r>-55?'▂▄▆█':r>-67?'▂▄▆<span style="opacity:.25">█</span>':r>-78?'▂▄<span style="opacity:.25">▆█</span>':'▂<span style="opacity:.25">▄▆█</span>';
$('wScan').onclick=async()=>{
 $('wScan').disabled=true;$('wList').innerHTML='<p class="hint">Suche läuft …</p>';
 try{let r;for(let i=0;i<20;i++){r=await api('/api/wifi/scan');if(!r.scanning)break;await new Promise(z=>setTimeout(z,1000))}
  if(!r||r.scanning||!r.nets.length){$('wList').innerHTML='<p class="hint">Keine Netzwerke gefunden.</p>'}
  else{$('wList').innerHTML=r.nets.map(n=>'<div class="net" data-n="'+esc(n.ssid)+'"><span class="sig">'+bars(n.rssi)+'</span><span class="n">'+esc(n.ssid)+'</span>'+(n.known?'<span class="tag">gespeichert</span>':'')+(n.secure?'<span>&#128274;</span>':'')+'</div>').join('');
   document.querySelectorAll('#wList .net').forEach(d=>d.onclick=()=>{$('wS').value=d.dataset.n;$('wP').value='';$('wP').focus()})}
 }catch(x){$('wList').innerHTML='<p class="hint err">Suche fehlgeschlagen – bitte nochmal versuchen.</p>'}
 $('wScan').disabled=false};
$('wShow').onchange=()=>$('wP').type=$('wShow').checked?'text':'password';
$('wForm').onsubmit=async e=>{e.preventDefault();const ssid=$('wS').value,before=st?st.lastSync:0;
 try{await post('/api/wifi/add',{ssid,pass:$('wP').value})}catch(x){$('wMsg').innerHTML='<span class="err">'+esc(x.message)+'</span>';return}
 $('wForm').reset();$('wList').innerHTML='';loadWifi();
 $('wMsg').textContent='Gespeichert – verbinde und hole die Uhrzeit …';
 for(let i=0;i<25;i++){await new Promise(z=>setTimeout(z,2000));
  try{await loadStatus();if(st.lastSync&&st.lastSync!=before){$('wMsg').innerHTML='<span class="ok">✓ Uhrzeit gestellt ('+new Date(st.lastSync*1000).toLocaleString('de-DE')+')</span>';loadWifi();return}}catch(x){}}
 $('wMsg').innerHTML='<span class="err">Keine Verbindung möglich – Passwort prüfen oder Netz außer Reichweite.</span>';loadWifi()};
$('prev').onclick=()=>{if(--cm<1){cm=12;cy--}loadMonth()};
$('next').onclick=()=>{if(++cm>12){cm=1;cy++}loadMonth()};
$('add').onsubmit=async e=>{e.preventDefault();
 const s=new Date($('aD').value+'T'+$('aS').value);let en=new Date($('aD').value+'T'+$('aE').value);
 if(en<=s)en=new Date(en.getTime()+864e5);
 try{await api('/api/add?s='+Math.floor(s/1000)+'&e='+Math.floor(en/1000),{method:'POST'});$('add').reset();refresh()}catch(x){alert(x.message)}};
const now=new Date();cy=now.getFullYear();cm=now.getMonth()+1;$('aD').valueAsDate=now;
refresh();setInterval(loadStatus,30000);
</script></body></html>)HTML";
