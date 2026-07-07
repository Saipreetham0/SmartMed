#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

// Single-page control panel served from PROGMEM. Fully self-contained (no
// external CDNs/fonts) so it works on the offline AP. Talks to /api/v1/*.
static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SmartMed</title>
<style>
  :root{
    --paper:#f3f5f2; --card:#ffffff; --ink:#15302a; --ink2:#3f5750;
    --mut:#6d837b; --line:#e2e8e3; --teal:#0e8a6b; --teal-soft:#e2f1eb;
    --coral:#e4593a; --amber:#c08a12; --shadow:0 1px 2px rgba(20,48,42,.05),0 8px 24px rgba(20,48,42,.06);
    --r:18px;
  }
  @media (prefers-color-scheme:dark){
    :root{--paper:#0d1613;--card:#132420;--ink:#eaf3ef;--ink2:#b7ccc3;
      --mut:#7f978d;--line:#20342d;--teal:#3fc79c;--teal-soft:#16302a;
      --coral:#ff6a49;--amber:#e0b24a;--shadow:0 1px 2px rgba(0,0,0,.3),0 10px 30px rgba(0,0,0,.35)}
  }
  :root[data-theme=light]{--paper:#f3f5f2;--card:#fff;--ink:#15302a;--ink2:#3f5750;
    --mut:#6d837b;--line:#e2e8e3;--teal:#0e8a6b;--teal-soft:#e2f1eb;--coral:#e4593a;--amber:#c08a12;
    --shadow:0 1px 2px rgba(20,48,42,.05),0 8px 24px rgba(20,48,42,.06)}
  :root[data-theme=dark]{--paper:#0d1613;--card:#132420;--ink:#eaf3ef;--ink2:#b7ccc3;
    --mut:#7f978d;--line:#20342d;--teal:#3fc79c;--teal-soft:#16302a;--coral:#ff6a49;--amber:#e0b24a;
    --shadow:0 1px 2px rgba(0,0,0,.3),0 10px 30px rgba(0,0,0,.35)}

  *{box-sizing:border-box}
  html,body{margin:0;max-width:100%;overflow-x:hidden}
  body{background:var(--paper);color:var(--ink);
    font-family:"SF Pro Text",-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,system-ui,sans-serif;
    line-height:1.5;-webkit-font-smoothing:antialiased}
  .wrap{max-width:820px;margin:0 auto;padding:22px 18px 60px}
  .eyebrow{display:flex;align-items:center;gap:9px;font-size:12px;font-weight:700;
    letter-spacing:.16em;text-transform:uppercase;color:var(--mut)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--teal)}
  .dot.warn{background:var(--amber)}
  .brand{margin-left:auto;font-weight:800;letter-spacing:.02em;color:var(--ink)}

  /* Hero */
  .hero{margin:14px 0 20px}
  .clock{font-weight:800;font-size:clamp(52px,15vw,88px);line-height:.95;letter-spacing:-.035em;
    font-variant-numeric:tabular-nums;color:var(--ink)}
  .clock .sec{color:var(--teal);font-size:.5em;vertical-align:.18em;margin-left:.06em}
  .subline{margin-top:8px;color:var(--ink2);font-size:15px}
  .subline b{color:var(--ink)}

  /* Alert takeover */
  .alert{display:none;align-items:center;gap:14px;background:var(--coral);color:#fff;
    border-radius:var(--r);padding:16px 18px;margin:0 0 18px;box-shadow:var(--shadow)}
  .alert.on{display:flex;animation:pulse 1.8s ease-out infinite}
  @keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(228,89,58,.45)}50%{box-shadow:0 0 0 16px rgba(228,89,58,0)}}
  .alert .big{font-weight:800;font-size:20px;line-height:1.1}
  .alert .sm{opacity:.9;font-size:13px}
  .alert button{margin-left:auto;background:#fff;color:var(--coral)}

  /* Toolbar */
  .tools{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:18px}
  button{font:inherit;font-weight:650;font-size:14px;border:0;border-radius:11px;padding:10px 15px;
    background:var(--teal);color:#fff;cursor:pointer;transition:transform .08s,filter .15s}
  button:hover{filter:brightness(1.05)} button:active{transform:translateY(1px)}
  button.ghost{background:transparent;color:var(--ink);border:1px solid var(--line)}
  button.sm{padding:7px 12px;font-size:13px;border-radius:9px}
  :focus-visible{outline:2px solid var(--teal);outline-offset:2px}

  /* Section label */
  .seclabel{font-size:12px;font-weight:700;letter-spacing:.14em;text-transform:uppercase;
    color:var(--mut);margin:6px 2px 12px}

  /* Chamber grid */
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:12px}
  @media (max-width:520px){.grid{grid-template-columns:1fr}}
  .cell{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
    padding:15px;box-shadow:var(--shadow);transition:border-color .2s,box-shadow .2s}
  .cell.alerting{border-color:var(--coral);box-shadow:0 0 0 1px var(--coral),var(--shadow)}
  .chead{display:flex;align-items:center;gap:10px}
  .num{width:30px;height:30px;border-radius:9px;flex:0 0 auto;display:flex;align-items:center;
    justify-content:center;font-weight:800;font-size:14px;color:var(--ink2);
    background:var(--paper);border:1px solid var(--line)}
  .cap{width:34px;height:16px;border-radius:8px;position:relative;overflow:hidden;flex:0 0 auto;
    box-shadow:inset 0 0 0 1px rgba(0,0,0,.14)}
  .cap:before{content:"";position:absolute;top:0;bottom:0;left:0;width:50%;background:var(--cc)}
  .cap:after{content:"";position:absolute;top:0;bottom:0;left:50%;right:0;
    background:color-mix(in srgb,var(--cc) 32%,#fff)}
  .lbl{flex:1;min-width:0;border:1px solid var(--line);background:var(--paper);color:var(--ink);
    border-radius:9px;padding:9px 12px;font:inherit;font-weight:650;font-size:15px}
  .crow{display:flex;align-items:center;gap:10px;margin-top:11px}
  .tm{border:1px solid var(--line);background:var(--paper);color:var(--ink);border-radius:9px;
    padding:9px 11px;font:inherit;font-variant-numeric:tabular-nums}
  .drow{display:flex;gap:10px;margin-top:10px;flex-wrap:wrap}
  .dl{flex:1;min-width:130px;display:flex;flex-direction:column;gap:4px;font-size:11px;
    font-weight:700;letter-spacing:.1em;text-transform:uppercase;color:var(--mut)}
  .dt{border:1px solid var(--line);background:var(--paper);color:var(--ink);border-radius:9px;
    padding:8px 10px;font:inherit;font-size:13.5px;font-variant-numeric:tabular-nums}
  .toggle{position:relative;width:44px;height:26px;flex:0 0 auto;cursor:pointer}
  .toggle input{opacity:0;width:0;height:0;position:absolute}
  .track{position:absolute;inset:0;background:var(--line);border-radius:99px;transition:.2s}
  .knob{position:absolute;top:3px;left:3px;width:20px;height:20px;background:#fff;border-radius:50%;
    transition:.2s;box-shadow:0 1px 2px rgba(0,0,0,.25)}
  .toggle input:checked+.track{background:var(--teal)}
  .toggle input:checked~.knob{transform:translateX(18px)}
  .cmeta{display:flex;align-items:center;gap:14px;margin-top:12px;flex-wrap:wrap}
  .stat{font-size:12.5px;color:var(--mut)} .stat b{color:var(--ink);font-variant-numeric:tabular-nums}
  .pill{padding:3px 9px;border-radius:99px;font-size:11.5px;font-weight:700;letter-spacing:.02em}
  .p-idle{background:var(--teal-soft);color:var(--teal)}
  .p-alert{background:var(--coral);color:#fff}
  .cactions{margin-left:auto;display:flex;gap:7px}

  /* Setup / status card */
  .setup{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
    padding:16px;box-shadow:var(--shadow);margin-bottom:18px}
  .setup h3{margin:0 0 4px;font-size:16px}
  .setup .row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
  .setup input{flex:1;min-width:130px;border:1px solid var(--line);background:var(--paper);
    color:var(--ink);border-radius:9px;padding:9px 11px;font:inherit}
  .scan a{color:var(--teal);text-decoration:none;margin-right:4px}
  .foot{margin-top:24px;text-align:center;color:var(--mut);font-size:12px}

  .toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%) translateY(10px);
    background:var(--ink);color:var(--paper);padding:11px 18px;border-radius:11px;font-size:14px;
    font-weight:600;opacity:0;transition:.25s;pointer-events:none;box-shadow:var(--shadow)}
  .toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
  @media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style>
</head>
<body>
<div class="wrap">
  <div class="eyebrow"><span class="dot" id="sdot"></span><span id="statustext">connecting…</span>
    <span class="brand">◐ SmartMed</span></div>

  <div class="alert" id="alert" role="alert" aria-live="assertive">
    <div><div class="big" id="alertbig">Time to take your medicine</div>
      <div class="sm" id="alertsm"></div></div>
    <button id="alertbtn">Mark taken</button>
  </div>

  <div class="hero">
    <div class="clock" id="clock">--:--<span class="sec">--</span></div>
    <div class="subline" id="subline">—</div>
  </div>

  <div id="setup"></div>

  <div class="tools">
    <button onclick="syncTime()">Sync clock to this device</button>
    <button class="ghost" onclick="save()">Save schedules</button>
    <button class="ghost" onclick="toggleTheme()" id="themebtn">Theme</button>
    <button class="ghost" onclick="setToken()">Token</button>
  </div>

  <div class="seclabel">Chambers</div>
  <div class="grid" id="chambers"></div>

  <div class="foot" id="foot"></div>
</div>
<div class="toast" id="toast"></div>

<script>
const HUES=['#1f9e7a','#3d7dd8','#8a63d2','#d98a2b','#d2607f','#3fa39b'];
let state=null, info=null;
let token=localStorage.getItem('smartmed_token')||'';
const pad=n=>String(n).padStart(2,'0');
function toast(m){const t=document.getElementById('toast');t.textContent=m;
  t.classList.add('show');clearTimeout(t._t);t._t=setTimeout(()=>t.classList.remove('show'),1700)}

// theme
function applyTheme(t){document.documentElement.dataset.theme=t;localStorage.setItem('smartmed_theme',t)}
function toggleTheme(){const cur=document.documentElement.dataset.theme
  ||(matchMedia('(prefers-color-scheme:dark)').matches?'dark':'light');
  applyTheme(cur==='dark'?'light':'dark')}
(function(){applyTheme(localStorage.getItem('smartmed_theme')||'light')})();

async function api(path,opts){opts=opts||{};opts.headers=opts.headers||{};
  if(token)opts.headers['Authorization']='Bearer '+token;
  const r=await fetch(path,opts);
  if(r.status===401)toast('Unauthorized — tap Token to enter it');
  return r.json();}
function setToken(){const t=prompt('Device API token',token);
  if(t!==null){token=t.trim();localStorage.setItem('smartmed_token',token);toast('Token saved');boot();}}
async function ensureToken(){if(token)return;
  try{const r=await fetch('/api/v1/pair');if(r.ok){token=(await r.json()).token;
    localStorage.setItem('smartmed_token',token);}}catch(e){}}

function nextDose(){
  if(!state)return null;
  const p=state.time.split(':').map(Number),now=p[0]*60+p[1];
  let best=null;
  state.chambers.forEach((c,i)=>{if(!c.enabled)return;
    let d=c.hour*60+c.minute-now;if(d<0)d+=1440;
    if(!best||d<best.d)best={i,d,c};});
  return best;}

function renderHero(){
  if(!state)return;
  const p=state.time.split(':');
  document.getElementById('clock').innerHTML=`${p[0]}:${p[1]}<span class="sec">${p[2]||'00'}</span>`;
  const online=state.mode!=='ap';
  document.getElementById('sdot').className='dot'+(online?'':' warn');
  document.getElementById('statustext').textContent=
    (online?'Online':'Setup mode')+' · '+state.date;
  const nd=nextDose();
  const sub=document.getElementById('subline');
  if(nd){const h=Math.floor(nd.d/60),m=nd.d%60;
    sub.innerHTML=`Next dose <b>${nd.c.label||('Chamber '+(nd.i+1))}</b> at <b>${pad(nd.c.hour)}:${pad(nd.c.minute)}</b> · in ${h?h+'h ':''}${m}m`;}
  else sub.innerHTML='No reminders scheduled — turn one on below.';
}

function renderAlert(){
  const el=document.getElementById('alert');
  const a=state&&state.chambers.find(c=>c.alerting);
  if(a){const i=state.chambers.indexOf(a);
    document.getElementById('alertbig').textContent='Take now — Chamber '+(i+1);
    document.getElementById('alertsm').textContent=a.label||'';
    document.getElementById('alertbtn').onclick=()=>dismiss(i);
    el.classList.add('on');}
  else el.classList.remove('on');
}

function renderSetup(){
  const el=document.getElementById('setup');
  if(!info){el.innerHTML='';return;}
  if(info.mode==='ap'){
    el.innerHTML=`<div class="setup"><h3>📶 Connect to your WiFi</h3>
      <div class="stat">Pick your home network so the app and phone can reach the device online.</div>
      <div class="row"><input id="wssid" placeholder="Network name">
        <input id="wpass" placeholder="Password" type="text">
        <button onclick="wifiSave()">Connect</button>
        <button class="ghost sm" onclick="wifiScan()">Scan</button></div>
      <div class="scan stat" id="scanres" style="margin-top:8px"></div></div>`;
  }else el.innerHTML='';
  document.getElementById('foot').textContent=
    `${info.mdns} · IP ${info.ip} · firmware ${info.fw} · device ${info.device_id}`;
}
async function wifiScan(){const j=await api('/api/v1/wifi/scan');
  document.getElementById('scanres').innerHTML=(j.networks||[]).map(n=>
    `<a href="#" onclick="document.getElementById('wssid').value='${(n.ssid||'').replace(/'/g,'')}';return false">${n.ssid}</a>`).join(' · ')||'No networks found';}
async function wifiSave(){const ssid=wssid.value,pass=wpass.value;
  if(!ssid){toast('Enter a network name');return;}
  await api('/api/v1/wifi',{method:'POST',body:JSON.stringify({ssid,pass})});
  toast('Saved — device is reconnecting');}

const esc=s=>(s||'').replace(/"/g,'&quot;');
let built=0;
// Build the editable card structure ONCE (so typing a name isn't wiped by polling).
function buildChambers(){
  const wrap=document.getElementById('chambers');wrap.innerHTML='';
  state.chambers.forEach((c,i)=>{
    const hue=HUES[i%HUES.length];
    const d=document.createElement('div');d.className='cell';d.id='cell'+i;
    d.innerHTML=`
      <div class="chead">
        <div class="num">${i+1}</div>
        <span class="cap" style="--cc:${hue}"></span>
        <input class="lbl" id="lbl${i}" value="${esc(c.label)}" placeholder="Medicine name">
      </div>
      <div class="crow">
        <input class="tm" type="time" id="tm${i}" value="${pad(c.hour)}:${pad(c.minute)}">
        <label class="toggle"><input type="checkbox" id="en${i}" ${c.enabled?'checked':''}>
          <span class="track"></span><span class="knob"></span></label>
        <span class="pill" id="pill${i}"></span>
      </div>
      <div class="drow">
        <label class="dl">From<input class="dt" type="date" id="ds${i}" value="${c.start||''}"></label>
        <label class="dl">Until<input class="dt" type="date" id="de${i}" value="${c.end||''}"></label>
      </div>
      <div class="cmeta" id="meta${i}"></div>`;
    wrap.appendChild(d);
  });
  built=state.chambers.length;
}
// Live-update only the status bits each poll (never the editable inputs).
function updateChambers(){
  state.chambers.forEach((c,i)=>{
    document.getElementById('cell'+i).classList.toggle('alerting',c.alerting);
    const pill=document.getElementById('pill'+i);
    pill.className='pill '+(c.alerting?'p-alert':'p-idle');
    pill.textContent=c.alerting?'take now':(c.enabled?'scheduled':'off');
    document.getElementById('meta'+i).innerHTML=
      `<span class="stat">Taken <b>${c.taken_count}</b></span>
       <span class="stat">Missed <b>${c.missed_count}</b></span>
       <span class="stat">Last <b>${c.last_taken||'never'}</b></span>
       <span class="cactions">${c.alerting?`<button class="sm" onclick="dismiss(${i})">Taken</button>`:''}<button class="ghost sm" onclick="test(${i})">Test</button></span>`;
  });
}

async function refresh(){
  try{info=await(await fetch('/api/v1/info')).json();renderSetup();}catch(e){}
  try{const s=await api('/api/v1/status');if(s&&s.ok!==false){state=s;
    renderHero();renderAlert();
    if(built!==state.chambers.length)buildChambers();
    updateChambers();}}catch(e){}
}
async function save(){
  if(!state)return;
  const body={chambers:state.chambers.map((c,i)=>{const t=document.getElementById('tm'+i).value.split(':');
    return{label:document.getElementById('lbl'+i).value,hour:+t[0],minute:+t[1],
      enabled:document.getElementById('en'+i).checked,
      start:document.getElementById('ds'+i).value,
      end:document.getElementById('de'+i).value};})};
  await api('/api/v1/schedules',{method:'POST',body:JSON.stringify(body)});
  toast('Schedules saved');refresh();}
async function syncTime(){const d=new Date();
  await api('/api/v1/time',{method:'POST',body:JSON.stringify({year:d.getFullYear(),
    month:d.getMonth()+1,day:d.getDate(),hour:d.getHours(),minute:d.getMinutes(),
    second:d.getSeconds(),tz:-d.getTimezoneOffset()*60})});
  toast('Clock updated');refresh();}
async function dismiss(i){await api('/api/v1/dismiss',{method:'POST',body:JSON.stringify({chamber:i})});
  toast('Marked as taken');refresh();}
async function test(i){await api('/api/v1/test',{method:'POST',body:JSON.stringify({chamber:i})});
  toast('Testing chamber '+(i+1));refresh();}
async function boot(){await ensureToken();refresh();}
boot();setInterval(refresh,2000);
</script>
</body>
</html>
)HTMLPAGE";

#endif // WEBUI_H
