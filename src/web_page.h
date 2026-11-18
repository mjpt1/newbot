// Control page served by the robot (http://robot.local or http://192.168.4.1)
#pragma once
static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fa" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoboEyes</title>
<style>
:root{--bg:#07090c;--card:rgba(255,255,255,.05);--line:rgba(255,255,255,.09);--text:#e9f5f1;--mute:#8aa39b;--acc:#8cffd7}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:radial-gradient(120% 80% at 50% 0%,#0f1d19 0%,var(--bg) 60%);color:var(--text);
font:15px/1.5 system-ui,-apple-system,"Segoe UI",Tahoma,sans-serif;min-height:100vh}
main{max-width:520px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:22px;letter-spacing:-.02em;margin:6px 0 2px;display:flex;align-items:center;gap:10px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--acc);box-shadow:0 0 12px var(--acc)}
.sub{color:var(--mute);margin:0 0 18px;font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px;margin:12px 0;backdrop-filter:blur(12px)}
.card h2{font-size:13px;color:var(--mute);font-weight:600;margin:0 0 12px;letter-spacing:.02em}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button{font:inherit;color:var(--text);background:rgba(255,255,255,.06);border:1px solid var(--line);border-radius:12px;
padding:11px 6px;cursor:pointer;transition:transform .1s ease-out,background .15s}
button:active{transform:scale(.96)}
button.on{background:color-mix(in srgb,var(--acc) 22%,transparent);border-color:color-mix(in srgb,var(--acc) 60%,transparent)}
.row{display:flex;align-items:center;gap:12px;margin:8px 0}
.row label{flex:0 0 90px;color:var(--mute);font-size:13px}
input[type=range]{flex:1;accent-color:var(--acc)}
#hue{appearance:none;height:10px;border-radius:6px;background:linear-gradient(90deg,red,#ff0,lime,cyan,blue,#f0f,red)}
input[type=text],input[type=password]{width:100%;font:inherit;color:var(--text);background:rgba(0,0,0,.3);border:1px solid var(--line);
border-radius:10px;padding:10px;margin:4px 0 10px}
.toggle{display:flex;justify-content:space-between;align-items:center}
.sw{width:50px;height:30px;border-radius:15px;background:rgba(255,255,255,.12);position:relative;cursor:pointer;transition:background .2s}
.sw::after{content:"";position:absolute;top:3px;right:3px;width:24px;height:24px;border-radius:50%;background:#fff;transition:transform .25s cubic-bezier(.3,1.4,.5,1)}
.sw.on{background:var(--acc)}.sw.on::after{transform:translateX(-20px)}
.mute{color:var(--mute);font-size:12px}
.primary{background:var(--acc);color:#04130e;border:0;font-weight:600;width:100%}
</style></head><body><main>
<h1><span class="dot"></span>RoboEyes</h1>
<p class="sub" id="status">در حال اتصال…</p>

<div class="card"><h2>حالت چهره</h2><div class="grid" id="emos"></div></div>

<div class="card"><h2>ظاهر</h2>
<div class="row"><label>رنگ چشم</label><input id="hue" type="range" min="0" max="359"><button onclick="set('hue',-2)" style="padding:6px 12px">سفید</button></div>
<div class="row"><label>نور صفحه</label><input id="bri" type="range" min="10" max="255"></div>
<div class="toggle" style="margin-top:12px"><span>نمایش ساعت</span><div class="sw" id="clock"></div></div>
</div>

<div class="card"><h2>اتصال به وای‌فای خانه (برای ساعت و آپدیت)</h2>
<input id="ssid" type="text" placeholder="نام شبکه">
<input id="pass" type="password" placeholder="رمز">
<button class="primary" onclick="saveWifi()">ذخیره و ری‌استارت</button>
<p class="mute">ربات همیشه شبکه‌ی خودش (RoboEyes) را هم روشن نگه می‌دارد.</p></div>

<div class="card"><h2>آپدیت برنامه (فایل firmware.bin)</h2>
<input id="fw" type="file" accept=".bin"><button class="primary" onclick="ota()">آپلود</button>
<p class="mute" id="otaMsg"></p></div>
</main>
<script>
const names=[["neutral","معمولی"],["happy","خوشحال"],["sad","ناراحت"],["angry","عصبانی"],["surprised","متعجب"],["sleepy","خواب‌آلود"],["dizzy","گیج"],["love","عاشق"],["curious","کنجکاو"],["wink","چشمک"],["asleep","خواب"],["swear","فحش"],["eat","غذا"],["vomit","استفراغ"]];
const $=id=>document.getElementById(id);
names.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n[1];b.onclick=()=>set('emo',i);b.id='e'+i;$('emos').appendChild(b)});
let t;function set(k,v){fetch(`/api/set?${k}=${v}`).then(r=>r.json()).then(show)}
function deb(k,v){clearTimeout(t);t=setTimeout(()=>set(k,v),60)}
$('hue').oninput=e=>{deb('hue',e.target.value);document.documentElement.style.setProperty('--acc',`hsl(${e.target.value} 100% 77%)`)};
$('bri').oninput=e=>deb('bri',e.target.value);
$('clock').onclick=()=>set('clock',$('clock').classList.contains('on')?0:1);
function show(s){names.forEach((n,i)=>$('e'+i).classList.toggle('on',i==s.emo));
$('hue').value=s.hue;$('bri').value=s.bri;$('clock').classList.toggle('on',!!s.clock);
document.documentElement.style.setProperty('--acc',s.hue<0?'#f2f6f8':`hsl(${s.hue} 100% 77%)`);
$('status').textContent=(s.time?('ساعت '+s.time+' · '):'')+(s.ip?('IP '+s.ip):'فقط شبکه‌ی ربات')+(s.batt?(' · باتری '+s.batt.toFixed(2)+'V'):'');}
function poll(){fetch('/api/state').then(r=>r.json()).then(show).catch(()=>{})}
poll();setInterval(poll,3000);
function saveWifi(){const f=new FormData();f.append('ssid',$('ssid').value);f.append('pass',$('pass').value);
fetch('/api/wifi',{method:'POST',body:f}).then(()=>$('status').textContent='ذخیره شد، ری‌استارت…')}
function ota(){const file=$('fw').files[0];if(!file)return;const f=new FormData();f.append('fw',file);
$('otaMsg').textContent='در حال آپلود…';fetch('/update',{method:'POST',body:f}).then(r=>r.text()).then(x=>$('otaMsg').textContent=x)}
</script></body></html>)HTML";
