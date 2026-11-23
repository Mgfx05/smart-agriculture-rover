#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// SMART AGRICULTURE ROVER
// Controller: ESP8266MOD NodeMCU
// Live Sensors + 16x2 LCD + Liquid Glass Web Dashboard
// =====================================================

// ---------------- WIFI CREDENTIALS -------------------
const char* ssid     = "esp32";
const char* password = "sg123456";

// ---------------- PIN CONNECTIONS --------------------
#define LCD_SDA  D2
#define LCD_SCL  D1

#define DHT_PIN  D5
#define DHT_TYPE DHT22

// ---------------- LCD & SERVER OBJECTS ---------------
LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP8266WebServer server(80);

Adafruit_ADS1115 ads;
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;

// ---------------- PROJECT SETTINGS ------------------
const float RAIN_ALERT_PERCENT = 40.0;
const unsigned long LCD_PAGE_INTERVAL = 3000;
const unsigned long SENSOR_INTERVAL   = 2000;

// ---------------- CALIBRATION VALUES -----------------
int SOIL1_DRY_RAW = 25000;
int SOIL1_WET_RAW = 12000;

int SOIL2_DRY_RAW = 25000;
int SOIL2_WET_RAW = 12000;

int RAIN_DRY_RAW  = 25000;
int RAIN_WET_RAW  = 12000;

// ---------------- GLOBAL VARIABLES ------------------
float soil1Percent = 0;
float soil2Percent = 0;
float averageSoil  = 0;

float temperature  = 0;
float humidity     = 0;
float lightLux     = 0;

float rainPercent  = 0;
bool rainDetected  = false;

bool adsAvailable   = false;
bool lightAvailable = false;

unsigned long lastSensorRead = 0;
unsigned long lastLCDPage    = 0;
int lcdPage = 0;

// =====================================================
// LOCAL WEB DASHBOARD (COMPACT & ESP8266 MEMORY-SAFE)
// =====================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Smart Agriculture Rover — Soil Survey</title>
  <style>
    :root {
      --glass: rgba(255, 255, 255, 0.08);
      --border: rgba(255, 255, 255, 0.25);
      --text: #f3faf8;
      --sub: #9ec0b9;
      --green: #10b981;
      --cyan: #38bdf8;
      --red: #f43f5e;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: radial-gradient(circle at 15% 15%, #0c3b33 0%, #061b1d 45%, #040d14 100%);
      color: var(--text);
      min-height: 100vh;
      padding: 18px;
    }
    .wrap { max-width: 1280px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
    .glass {
      background: linear-gradient(145deg, rgba(255,255,255,0.11), rgba(255,255,255,0.04));
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      border-radius: 20px;
      border-top: 1px solid var(--border);
      border-left: 1px solid rgba(255,255,255,0.18);
      border-right: 1px solid rgba(255,255,255,0.08);
      border-bottom: 1px solid rgba(255,255,255,0.08);
      padding: 18px;
      box-shadow: 0 12px 32px rgba(0,0,0,0.4);
    }
    .topbar { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 12px; }
    h1 { font-size: 1.2rem; }
    .sub { font-size: 0.78rem; color: var(--sub); }
    .controls { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
    .pill {
      display: flex; align-items: center; gap: 6px;
      background: rgba(0,0,0,0.3); border: 1px solid rgba(255,255,255,0.16);
      border-radius: 999px; padding: 6px 12px; font-size: 0.8rem;
    }
    .pill input { background: transparent; border: none; outline: none; color: #fff; width: 155px; font-family: monospace; }
    .btn {
      cursor: pointer; border: none; border-radius: 999px; padding: 8px 14px;
      font-size: 0.8rem; font-weight: 600; color: #fff;
      background: rgba(255,255,255,0.14); border-top: 1px solid rgba(255,255,255,0.3);
    }
    .btn:hover { background: rgba(255,255,255,0.22); }
    .btn-green { background: rgba(16,185,129,0.8); }
    .btn-red { background: rgba(244,63,94,0.65); }
    .btn-cyan { background: rgba(14,165,233,0.75); }
    .btn.active { background: linear-gradient(135deg, #10b981, #0ea5e9); }
    .dot { width: 9px; height: 9px; border-radius: 50%; display: inline-block; background: #10b981; }
    .dot.demo { background: #38bdf8; }
    .dot.err { background: #f43f5e; }
    .grid7 { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; }
    .card { padding: 14px; border-radius: 16px; }
    .lbl { font-size: 0.72rem; color: var(--sub); text-transform: uppercase; margin-bottom: 6px; }
    .val { font-size: 1.65rem; font-weight: 700; }
    .unit { font-size: 0.85rem; color: var(--sub); }
    .split { display: grid; grid-template-columns: 1.3fr 1fr; gap: 16px; }
    @media (max-width: 960px) { .split { grid-template-columns: 1fr; } }
    .toolbar { display: flex; flex-wrap: wrap; gap: 6px; margin: 10px 0; }
    canvas { width: 100%; height: 360px; border-radius: 14px; background: #041517; border: 1px solid rgba(255,255,255,0.2); cursor: crosshair; display: block; }
    .zones { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-top: 10px; font-size: 0.8rem; }
    .zbox { background: rgba(255,255,255,0.06); padding: 10px; border-radius: 12px; }
    .dir { background: rgba(0,0,0,0.25); padding: 10px 12px; border-radius: 12px; border-left: 3px solid var(--green); margin-bottom: 8px; font-size: 0.82rem; }
    .dir.warn { border-left-color: var(--red); }
    table { width: 100%; border-collapse: collapse; font-size: 0.8rem; margin-top: 10px; }
    th, td { padding: 8px 10px; border-bottom: 1px solid rgba(255,255,255,0.1); text-align: left; }
    th { color: var(--sub); font-size: 0.72rem; text-transform: uppercase; }
    @media print {
      body { background: #fff; color: #000; }
      .no-print { display: none !important; }
      .glass { background: #fff; border: 1px solid #ccc; box-shadow: none; color: #000; }
    }
  </style>
</head>
<body>
<div class="wrap">
  <div class="glass topbar">
    <div>
      <h1>🌱 Smart Agriculture Rover — Soil Survey</h1>
      <div class="sub">ESP8266MOD Real-Time Telemetry &amp; Spatial IDW Field Analysis</div>
    </div>
    <div class="controls no-print">
      <div class="pill">
        <span>IP:</span>
        <input id="ipInput" type="text" value="" placeholder="Auto (same host)" />
      </div>
      <button id="liveBtn" class="btn btn-green" onclick="setModeLive(true)">Live: ON</button>
      <button id="demoBtn" class="btn" onclick="toggleDemo()">Demo Mode: OFF</button>
      <span class="pill"><span id="sDot" class="dot"></span><span id="sTxt">Live (2s)</span></span>
      <button class="btn btn-cyan" onclick="window.print()">📄 Print A4 PDF</button>
    </div>
  </div>

  <div class="grid7">
    <div class="glass card"><div class="lbl">Soil Moisture 1</div><div class="val"><span id="s1">--</span><span class="unit">%</span></div></div>
    <div class="glass card"><div class="lbl">Soil Moisture 2</div><div class="val"><span id="s2">--</span><span class="unit">%</span></div></div>
    <div class="glass card"><div class="lbl">Average Soil</div><div class="val"><span id="savg">--</span><span class="unit">%</span></div></div>
    <div class="glass card"><div class="lbl">Temperature</div><div class="val"><span id="temp">--</span><span class="unit">°C</span></div></div>
    <div class="glass card"><div class="lbl">Humidity</div><div class="val"><span id="hum">--</span><span class="unit">%</span></div></div>
    <div class="glass card"><div class="lbl">Light Intensity</div><div class="val"><span id="lux">--</span><span class="unit">lx</span></div></div>
    <div class="glass card"><div class="lbl">Rain Status</div><div class="val" id="rain" style="font-size:1.3rem;">Dry</div></div>
  </div>

  <div class="split">
    <div class="glass">
      <div style="display:flex; justify-content:space-between; align-items:center;">
        <strong>🗺️ Interactive Field IDW Heatmap</strong>
        <span class="sub">Area: <input id="areaIn" type="number" value="450" style="width:60px; background:rgba(0,0,0,0.3); color:#fff; border:1px solid #444; border-radius:6px; padding:2px 6px;" onchange="updateAll()" /> m²</span>
      </div>
      <div class="toolbar no-print">
        <button id="mLog" class="btn active" onclick="setTool('log')">📍 Log Point</button>
        <button id="mEdit" class="btn" onclick="setTool('edit')">📐 Reshape Plot</button>
        <button class="btn active" data-l="soil" onclick="setLayer('soil', this)">💧 Moisture</button>
        <button class="btn" data-l="temp" onclick="setLayer('temp', this)">🌡️ Temp</button>
        <button class="btn" data-l="lux" onclick="setLayer('lux', this)">☀️ Light</button>
      </div>
      <canvas id="cv" width="760" height="360"></canvas>
      <div class="zones">
        <div class="zbox">🔴 Dry (&lt;35%): <strong id="zDry">0%</strong></div>
        <div class="zbox">🟢 Optimal (35–65%): <strong id="zOpt">0%</strong></div>
        <div class="zbox">🔵 Wet (&gt;65%): <strong id="zWet">0%</strong></div>
      </div>
    </div>

    <div class="glass">
      <strong>💧 Automated &amp; Annual Irrigation Engine</strong>
      <div id="advisory" style="margin-top:10px;"></div>
    </div>
  </div>

  <div class="glass">
    <div style="display:flex; justify-content:space-between; align-items:center;">
      <strong>📋 Survey Waypoints (<span id="wpCnt">0</span>)</strong>
      <div class="controls no-print">
        <button class="btn btn-green" onclick="logCenter()">+ Capture Live Point</button>
        <button class="btn" onclick="exportCSV()">⬇️ CSV</button>
        <button class="btn btn-red" onclick="clearWPs()">Clear</button>
      </div>
    </div>
    <table>
      <thead>
        <tr><th>#</th><th>Time</th><th>X, Y</th><th>Soil 1</th><th>Soil 2</th><th>Avg</th><th>Temp</th><th>Hum</th><th>Light</th><th>Rain</th><th class="no-print">Del</th></tr>
      </thead>
      <tbody id="wpBody"></tbody>
    </table>
  </div>
</div>

<script>
  let demoMode = false;
  let liveOn = true;
  let tool = 'log';
  let layer = 'soil';
  let dragIdx = -1;
  let tele = { soil1: 0, soil2: 0, avgSoil: 0, temp: 0, hum: 0, lux: 0, rainPct: 0, rain: false };
  let poly = [{x:0.15,y:0.15},{x:0.85,y:0.18},{x:0.82,y:0.84},{x:0.18,y:0.80}];
  let wps = [];
  let nextId = 1;

  const cv = document.getElementById('cv');
  const ctx = cv.getContext('2d');

  function setModeLive(state) {
    liveOn = state;
    if (liveOn) demoMode = false;
    syncButtons();
    poll();
  }

  function toggleDemo() {
    demoMode = !demoMode;
    liveOn = !demoMode;
    syncButtons();
    poll();
  }

  function syncButtons() {
    const lBtn = document.getElementById('liveBtn');
    const dBtn = document.getElementById('demoBtn');
    const dot = document.getElementById('sDot');
    const txt = document.getElementById('sTxt');

    lBtn.textContent = liveOn ? 'Live: ON' : 'Connect Live';
    lBtn.className = liveOn ? 'btn btn-green' : 'btn';
    dBtn.textContent = demoMode ? 'Demo Mode: ON' : 'Demo Mode: OFF';
    dBtn.className = demoMode ? 'btn active' : 'btn';

    if (demoMode) {
      dot.className = 'dot demo';
      txt.textContent = 'Demo Simulation (2s)';
    } else if (liveOn) {
      dot.className = 'dot';
      txt.textContent = 'Live Rover (2s)';
    } else {
      dot.className = 'dot err';
      txt.textContent = 'Paused';
    }
  }

  async function poll() {
    if (demoMode) {
      tele.soil1 = Math.max(10, Math.min(90, (tele.soil1 || 28) + (Math.random() - 0.5) * 4));
      tele.soil2 = Math.max(10, Math.min(90, (tele.soil2 || 31) + (Math.random() - 0.5) * 4));
      tele.avgSoil = (tele.soil1 + tele.soil2) / 2;
      tele.temp = Number((27.5 + (Math.random() - 0.5) * 1.2).toFixed(1));
      tele.hum = Math.round(46 + (Math.random() - 0.5) * 4);
      tele.lux = Math.round(420 + (Math.random() - 0.5) * 80);
      tele.rain = false;
      tele.rainPct = 0;
      renderTele();
      return;
    }
    if (!liveOn) return;
    try {
      const customIp = document.getElementById('ipInput').value.trim().replace(/\/$/, '');
      const url = customIp ? customIp + '/data' : '/data';
      const r = await fetch(url);
      const d = await r.json();
      tele = d;
      document.getElementById('sDot').className = 'dot';
      document.getElementById('sTxt').textContent = 'Live Connected';
      renderTele();
    } catch (e) {
      document.getElementById('sDot').className = 'dot err';
      document.getElementById('sTxt').textContent = 'Offline / Retrying...';
    }
  }

  function renderTele() {
    document.getElementById('s1').textContent = Number(tele.soil1).toFixed(1);
    document.getElementById('s2').textContent = Number(tele.soil2).toFixed(1);
    document.getElementById('savg').textContent = Number(tele.avgSoil).toFixed(1);
    document.getElementById('temp').textContent = Number(tele.temp).toFixed(1);
    document.getElementById('hum').textContent = Math.round(tele.hum);
    document.getElementById('lux').textContent = Math.round(tele.lux);
    const rEl = document.getElementById('rain');
    rEl.textContent = tele.rain ? 'RAIN (' + Math.round(tele.rainPct) + '%)' : 'Dry';
    rEl.style.color = tele.rain ? '#f43f5e' : '#10b981';
    updateAll();
  }

  function inPoly(x, y) {
    let inside = false;
    for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
      const xi = poly[i].x, yi = poly[i].y, xj = poly[j].x, yj = poly[j].y;
      if (((yi > y) !== (yj > y)) && (x < (xj - xi) * (y - yi) / ((yj - yi) || 1e-9) + xi)) inside = !inside;
    }
    return inside;
  }

  function idw(nx, ny) {
    if (!wps.length) return null;
    let num = 0, den = 0;
    for (const w of wps) {
      const d = Math.hypot(nx - w.x, ny - w.y);
      const v = layer === 'soil' ? w.avgSoil : (layer === 'temp' ? w.temp : w.lux);
      if (d < 0.001) return v;
      const weight = 1 / (d * d);
      num += weight * v;
      den += weight;
    }
    return num / den;
  }

  function drawMap() {
    const W = cv.width, H = cv.height;
    ctx.clearRect(0, 0, W, H);
    if (wps.length) {
      ctx.save();
      ctx.beginPath();
      poly.forEach((p, i) => i ? ctx.lineTo(p.x * W, p.y * H) : ctx.moveTo(p.x * W, p.y * H));
      ctx.closePath();
      ctx.clip();
      for (let y = 0; y < H; y += 12) {
        for (let x = 0; x < W; x += 12) {
          const nx = (x + 6) / W, ny = (y + 6) / H;
          if (!inPoly(nx, ny)) continue;
          const v = idw(nx, ny);
          let t = layer === 'soil' ? v / 100 : (layer === 'temp' ? (v - 15) / 30 : v / 1000);
          t = Math.max(0, Math.min(1, t));
          ctx.fillStyle = layer === 'soil'
            ? `hsla(${Math.round(t * 210)}, 85%, 50%, 0.65)`
            : `hsla(${Math.round((1 - t) * 220)}, 85%, 52%, 0.65)`;
          ctx.fillRect(x, y, 13, 13);
        }
      }
      ctx.restore();
    }
    ctx.beginPath();
    poly.forEach((p, i) => i ? ctx.lineTo(p.x * W, p.y * H) : ctx.moveTo(p.x * W, p.y * H));
    ctx.closePath();
    ctx.strokeStyle = '#10b981';
    ctx.lineWidth = 2.5;
    ctx.stroke();

    poly.forEach((p) => {
      ctx.beginPath();
      ctx.arc(p.x * W, p.y * H, tool === 'edit' ? 7 : 4, 0, Math.PI * 2);
      ctx.fillStyle = '#38bdf8';
      ctx.fill();
    });

    wps.forEach((w) => {
      ctx.beginPath();
      ctx.arc(w.x * W, w.y * H, 11, 0, Math.PI * 2);
      ctx.fillStyle = '#0f172a';
      ctx.fill();
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.fillStyle = '#fff';
      ctx.font = 'bold 10px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(w.id, w.x * W, w.y * H);
    });
  }

  cv.addEventListener('mousedown', (e) => {
    const r = cv.getBoundingClientRect();
    const nx = (e.clientX - r.left) / r.width;
    const ny = (e.clientY - r.top) / r.height;
    if (tool === 'edit') {
      dragIdx = poly.findIndex(p => Math.hypot(p.x - nx, p.y - ny) < 0.04);
      return;
    }
    if (inPoly(nx, ny)) addWP(nx, ny);
  });
  cv.addEventListener('mousemove', (e) => {
    if (tool === 'edit' && dragIdx >= 0) {
      const r = cv.getBoundingClientRect();
      poly[dragIdx] = { x: (e.clientX - r.left) / r.width, y: (e.clientY - r.top) / r.height };
      updateAll();
    }
  });
  window.addEventListener('mouseup', () => dragIdx = -1);

  function setTool(t) {
    tool = t;
    document.getElementById('mLog').className = t === 'log' ? 'btn active' : 'btn';
    document.getElementById('mEdit').className = t === 'edit' ? 'btn active' : 'btn';
    drawMap();
  }
  function setLayer(l, b) {
    layer = l;
    document.querySelectorAll('[data-l]').forEach(x => x.classList.remove('active'));
    b.classList.add('active');
    drawMap();
  }
  function addWP(x, y) {
    wps.push({
      id: nextId++,
      time: new Date().toTimeString().slice(0, 8),
      x: Number(x.toFixed(2)),
      y: Number(y.toFixed(2)),
      soil1: Number(tele.soil1),
      soil2: Number(tele.soil2),
      avgSoil: Number(tele.avgSoil),
      temp: Number(tele.temp),
      hum: Number(tele.hum),
      lux: Math.round(tele.lux),
      rain: tele.rain ? 'Rain' : 'Dry'
    });
    updateAll();
  }
  function logCenter() { addWP(0.5, 0.5); }
  function delWP(id) { wps = wps.filter(w => w.id !== id); updateAll(); }
  function clearWPs() { wps = []; nextId = 1; updateAll(); }

  function exportCSV() {
    const rows = ['ID,Time,X,Y,Soil1,Soil2,AvgSoil,Temp,Hum,Lux,Rain',
      ...wps.map(w => `${w.id},${w.time},${w.x},${w.y},${w.soil1},${w.soil2},${w.avgSoil},${w.temp},${w.hum},${w.lux},${w.rain}`)];
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([rows.join('\n')], { type: 'text/csv' }));
    a.download = 'rover_survey.csv';
    a.click();
  }

  function updateAll() {
    drawMap();
    document.getElementById('wpCnt').textContent = wps.length;
    document.getElementById('wpBody').innerHTML = wps.map(w =>
      `<tr><td>#${w.id}</td><td>${w.time}</td><td>${w.x}, ${w.y}</td><td>${w.soil1.toFixed(1)}%</td><td>${w.soil2.toFixed(1)}%</td><td><strong>${w.avgSoil.toFixed(1)}%</strong></td><td>${w.temp.toFixed(1)}°C</td><td>${Math.round(w.hum)}%</td><td>${w.lux}</td><td>${w.rain}</td><td class="no-print"><button class="btn btn-red" style="padding:3px 8px;" onclick="delWP(${w.id})">✕</button></td></tr>`
    ).join('');

    let dry = 0, opt = 0, wet = 0, tot = 0;
    if (wps.length) {
      const prev = layer; layer = 'soil';
      for (let y = 0.05; y < 1; y += 0.05) {
        for (let x = 0.05; x < 1; x += 0.05) {
          if (!inPoly(x, y)) continue;
          const v = idw(x, y); tot++;
          if (v < 35) dry++; else if (v > 65) wet++; else opt++;
        }
      }
      layer = prev;
    }
    const dPct = tot ? Math.round((dry / tot) * 100) : (tele.avgSoil < 35 ? 100 : 0);
    const wPct = tot ? Math.round((wet / tot) * 100) : (tele.avgSoil > 65 ? 100 : 0);
    const oPct = Math.max(0, 100 - dPct - wPct);
    document.getElementById('zDry').textContent = dPct + '%';
    document.getElementById('zOpt').textContent = oPct + '%';
    document.getElementById('zWet').textContent = wPct + '%';

    const area = Number(document.getElementById('areaIn').value) || 450;
    const liters = tele.rain ? 0 : Math.round((dPct / 100) * area * 6.5);
    document.getElementById('advisory').innerHTML = `
      <div class="dir ${tele.rain ? 'warn' : ''}">
        <strong>${tele.rain ? '🌧️ RAIN-DELAY LOCK ACTIVE' : '✅ Immediate Zone Action'}</strong><br/>
        ${tele.rain ? 'Valves held OFF while rain is detected.' : `Irrigate Dry Zone (${dPct}% area) with <strong>${liters} Liters</strong> (6.5 L/m²).`}
      </div>
      <div class="dir"><strong>🌱 Monsoon (Jun–Sep):</strong> Rain-fed; supplemental 2.5 L/m² only after 5 dry days.</div>
      <div class="dir"><strong>🌾 Rabi (Oct–Jan):</strong> Irrigate 4.5 L/m² every 4 days at dawn.</div>
      <div class="dir"><strong>☀️ Summer (Feb–May):</strong> Split 7.0 L/m² dawn/dusk every 48 hrs with mulching.</div>
    `;
  }

  syncButtons();
  poll();
  setInterval(poll, 2000);
</script>
</body>
</html>
)rawliteral";

// =====================================================
// CONVERT ADC RAW VALUE TO PERCENTAGE
// =====================================================
float calculatePercentage(int raw, int dryValue, int wetValue) {
  if (dryValue == wetValue) {
    return 0;
  }

  float percentage =
      ((float)(dryValue - raw) * 100.0) /
      (float)(dryValue - wetValue);

  return constrain(percentage, 0.0, 100.0);
}

// =====================================================
// READ SOIL AND RAIN SENSORS
// =====================================================
void readAnalogSensors() {
  int soil1Raw = ads.readADC_SingleEnded(0);
  int soil2Raw = ads.readADC_SingleEnded(1);
  int rainRaw  = ads.readADC_SingleEnded(2);

  soil1Percent = calculatePercentage(soil1Raw, SOIL1_DRY_RAW, SOIL1_WET_RAW);
  soil2Percent = calculatePercentage(soil2Raw, SOIL2_DRY_RAW, SOIL2_WET_RAW);
  averageSoil  = (soil1Percent + soil2Percent) / 2.0;

  rainPercent  = calculatePercentage(rainRaw, RAIN_DRY_RAW, RAIN_WET_RAW);
  rainDetected = (rainPercent >= RAIN_ALERT_PERCENT);

  Serial.println("\n----- ANALOG SENSOR DATA -----");
  Serial.print("Soil 1 Raw: ");   Serial.println(soil1Raw);
  Serial.print("Soil 2 Raw: ");   Serial.println(soil2Raw);
  Serial.print("Rain Raw: ");     Serial.println(rainRaw);
  Serial.print("Soil 1: ");       Serial.print(soil1Percent); Serial.println("%");
  Serial.print("Soil 2: ");       Serial.print(soil2Percent); Serial.println("%");
  Serial.print("Average Soil: "); Serial.print(averageSoil);  Serial.println("%");
  Serial.print("Rain: ");         Serial.print(rainPercent);  Serial.println("%");
}

// =====================================================
// READ DHT22
// =====================================================
void readTemperatureHumidity() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  } else {
    Serial.println("ERROR: DHT22 reading failed");
  }
}

// =====================================================
// READ BH1750 LIGHT SENSOR
// =====================================================
void readLightSensor() {
  if (lightAvailable) {
    float lux = lightMeter.readLightLevel();
    if (lux >= 0) {
      lightLux = lux;
    }
  }
}

// =====================================================
// LCD DISPLAY PAGES (3 PAGES ONLY)
// =====================================================
void displayPage1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print((int)soil1Percent);
  lcd.print("% S2:");
  lcd.print((int)soil2Percent);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Avg Soil:");
  lcd.print((int)averageSoil);
  lcd.print("%");
}

void displayPage2() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(temperature, 1);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

void displayPage3() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Light:");
  lcd.print(lightLux, 0);
  lcd.print(" lx");

  lcd.setCursor(0, 1);
  if (rainDetected) {
    lcd.print("RAIN DETECTED");
  } else {
    lcd.print("Rain: No");
  }
}

void updateLCD() {
  if (millis() - lastLCDPage < LCD_PAGE_INTERVAL) {
    return;
  }

  lastLCDPage = millis();

  switch (lcdPage) {
    case 0: displayPage1(); break;
    case 1: displayPage2(); break;
    case 2: displayPage3(); break;
  }

  lcdPage++;
  if (lcdPage > 2) {
    lcdPage = 0;
  }
}

// =====================================================
// WEB SERVER HANDLERS
// =====================================================
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"soil1\":"   + String(soil1Percent, 1) + ",";
  json += "\"soil2\":"   + String(soil2Percent, 1) + ",";
  json += "\"avgSoil\":" + String(averageSoil, 1)  + ",";
  json += "\"temp\":"    + String(temperature, 1)  + ",";
  json += "\"hum\":"     + String(humidity, 1)     + ",";
  json += "\"lux\":"     + String(lightLux, 0)     + ",";
  json += "\"rainPct\":" + String(rainPercent, 1)  + ",";
  json += "\"rain\":"    + String(rainDetected ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// =====================================================
// SERIAL MONITOR OUTPUT
// =====================================================
void printAllReadings() {
  Serial.println("\n========== AGRICULTURE ROVER ==========");
  Serial.print("Temperature: ");   Serial.print(temperature); Serial.println(" C");
  Serial.print("Humidity: ");      Serial.print(humidity);    Serial.println(" %");
  Serial.print("Light: ");         Serial.print(lightLux);    Serial.println(" lux");
  Serial.print("Average Soil: ");  Serial.print(averageSoil); Serial.println(" %");
  Serial.print("Rain Detected: "); Serial.println(rainDetected ? "YES" : "NO");
  Serial.println("=======================================");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // I2C: SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  Wire.begin(LCD_SDA, LCD_SCL);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Smart Agri Rover");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  adsAvailable = ads.begin(0x48);
  if (adsAvailable) {
    ads.setGain(GAIN_ONE);
    Serial.println("ADS1115 connected");
  } else {
    Serial.println("ERROR: ADS1115 not found");
  }

  dht.begin();

  lightAvailable = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (lightAvailable) {
    Serial.println("BH1750 connected");
  } else {
    Serial.println("ERROR: BH1750 not found");
  }

  // Connect to Wi-Fi for Local Web Dashboard
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Open this URL in browser: http://");
  Serial.println(WiFi.localIP());

  lcd.clear();
  if (!adsAvailable) {
    lcd.setCursor(0, 0);
    lcd.print("ADS1115 ERROR");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Dashboard IP:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  delay(3000);

  lastSensorRead = millis();
  lastLCDPage = millis();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  server.handleClient();

  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();

    if (adsAvailable) {
      readAnalogSensors();
    }

    readTemperatureHumidity();
    readLightSensor();
    printAllReadings();
  }

  updateLCD();
}