/***************************************************************
********************  STC CREATIVE CLUB  ***********************
*  Project Name : STC ESP32 MAX30100 Web + OLED Monitor
*  Board        : ESP32 DevKit V1
*  Sensor       : MAX30100 Pulse Oximeter
*  Display      : 0.96" OLED (SSD1306, 128x64, I2C)
*
* 
*
*  PROJECT BRIEF:
*  ------------------------------------------------------------
*  - ESP32 reads MAX30100 sensor (I2C address 0x57)
*  - Shows Heart Rate (BPM) + SpO2 (%) on:
*        1) OLED display (near patient)
*        2) Web dashboard at 192.168.4.1
*  - ESP32 creates WiFi Access Point:
*        SSID : STC_MAX30100_AP
*        PASS : 12345678
*
*  WIRING (COMMON I2C BUS):
*  ------------------------------------------------------------
*  ESP32 DEVKIT V1        MAX30100        OLED (SSD1306)
*  3.3V (3V3)       --->  VIN / VCC      VCC
*  GND              --->  GND            GND
*  GPIO 21          --->  SDA            SDA
*  GPIO 22          --->  SCL            SCL
*
*  
*
*  SERIAL MONITOR BRANDING:
*  ------------------------------------------------------------
*  - STC_BrandingSerial() prints STC Creative Club header.
***************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MAX30100_PulseOximeter.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====================  OLED CONFIG  ==========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================  STC SERIAL BRANDING  ===================
void STC_BrandingSerial() {
  Serial.println();
  Serial.println("****************************************************");
  Serial.println("*                STC CREATIVE CLUB – NAVSARI       *");
  Serial.println("*     ESP32 + MAX30100 + OLED + WEB MONITOR        *");
  Serial.println("*              Author : PRAVIN SINH RANA           *");
  Serial.println("*              Contact: 9313057803                 *");
  Serial.println("*              Insta  : @S.T.C_CREATIVE_CLUB       *");
  Serial.println("****************************************************");
  Serial.println();
}

// ====================  MAX30100 SENSOR  ======================
PulseOximeter pox;
unsigned long lastRead = 0;

float gSpo2  = 0.0;
float gBpm   = 0.0;
bool  gValid = false;

// ====== HEART BEAT CALLBACK (OPTIONAL) =======================
void onBeatDetected() {
  Serial.println("[STC] ❤️  Heart Beat Detected");
}

// ======================  WIFI / SERVER  ======================
WebServer server(80);

const char* apSsid = "STC_MAX30100_AP";
const char* apPass = "12345678";

IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway (192, 168, 4, 1);
IPAddress subnet  (255, 255, 255, 0);

// ======================  OLED HELPER  ========================
void updateOLED(float spo2, float bpm, bool valid) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("STC Creative Club");

  display.setCursor(0, 10);
  display.println("ESP32 MAX30100 Monitor");

  display.drawLine(0, 20, 127, 20, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 28);
  display.print("SpO2:");
  if (valid) display.print((int)spo2);
  else       display.print("--");

  display.setCursor(0, 48);
  display.print("BPM:");
  if (valid) display.print((int)bpm);
  else       display.print("--");

  display.display();
}

// =====================  HTML DASHBOARD  ======================
// Upgraded hospital-style dashboard based on your reference.
// Uses /data endpoint: { spo2, bpm, valid }


const char INDEX_HTML[] PROGMEM = R"rawlite(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>Patient Vitals Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <style>
    :root {
      --bg: #04151f;
      --card: #071b2e;
      --accent: #16c4a8;
      --accent-soft: #0c7c87;
      --warn: #f97360;
      --text-main: #f9fafb;
      --text-muted: #9ca3af;
      --border: #1f2933;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: radial-gradient(circle at top, #06324a 0, #020617 55%, #000 100%);
      color: var(--text-main);
    }
    .page {
      max-width: 1080px;
      margin: 0 auto;
      padding: 16px;
    }
    .header {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 12px;
      padding: 14px 18px;
      border-radius: 16px;
      background: linear-gradient(135deg, #0b2538 0, #04141f 45%, #062d32 100%);
      border: 1px solid #123047;
      box-shadow: 0 18px 40px rgba(0, 0, 0, 0.6);
    }
    .title-block { min-width: 220px; }
    .app-title {
      font-size: 22px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--accent);
    }
    .subtitle {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 4px;
    }
    .info-line {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 2px;
    }
    .header-right {
      display: flex;
      flex-direction: column;
      gap: 8px;
      align-items: flex-end;
      justify-content: center;
      min-width: 220px;
    }
    .pill {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      border-radius: 999px;
      padding: 4px 10px;
      font-size: 11px;
      border: 1px solid var(--border);
      background: rgba(15, 23, 42, 0.9);
      color: var(--text-muted);
    }
    .pill-dot {
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: #4b5563;
    }
    .pill.online .pill-dot { background: #22c55e; }
    .pill.online {
      color: #bbf7d0;
      border-color: rgba(34, 197, 94, 0.4);
      background: rgba(21, 128, 61, 0.1);
    }
    .pill.offline .pill-dot { background: #f97360; }
    .pill.offline {
      color: #fecaca;
      border-color: rgba(248, 113, 113, 0.4);
      background: rgba(127, 29, 29, 0.15);
    }
    .btn {
      border-radius: 999px;
      border: 1px solid rgba(148, 163, 184, 0.35);
      padding: 6px 14px;
      font-size: 12px;
      background: rgba(15, 23, 42, 0.9);
      color: var(--text-main);
      cursor: pointer;
      transition: all 0.15s ease-out;
    }
    .btn:hover {
      transform: translateY(-1px);
      border-color: var(--accent);
      box-shadow: 0 0 18px rgba(22, 196, 168, 0.35);
    }
    .layout {
      display: grid;
      grid-template-columns: minmax(0, 260px) minmax(0, 1fr);
      gap: 14px;
      margin-top: 14px;
    }
    @media (max-width: 880px) {
      .layout { grid-template-columns: minmax(0, 1fr); }
      .header-right { align-items: flex-start; }
    }
    .card {
      background: radial-gradient(circle at top left, #0b2538 0, #020617 60%);
      border-radius: 16px;
      padding: 12px 14px;
      border: 1px solid rgba(15, 23, 42, 0.95);
      box-shadow: 0 14px 35px rgba(0, 0, 0, 0.7);
    }
    .card-title {
      font-size: 13px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.18em;
      color: var(--text-muted);
      margin-bottom: 10px;
    }
    .device-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .device-item {
      padding: 8px 10px;
      border-radius: 12px;
      border: 1px solid rgba(31, 41, 55, 0.9);
      background: linear-gradient(135deg, rgba(15,23,42,0.95), rgba(15,23,42,0.7));
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 12px;
    }
    .device-main { font-weight: 600; color: #e5e7eb; }
    .device-sub {
      color: var(--text-muted);
      font-size: 11px;
      margin-top: 2px;
    }
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 10px;
    }
    .metric-card {
      border-radius: 12px;
      padding: 10px;
      border: 1px solid rgba(31, 41, 55, 0.9);
      background: radial-gradient(circle at top, rgba(22, 196, 168, 0.15), rgba(15,23,42,0.95));
      position: relative;
      overflow: hidden;
    }
    .metric-label {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.16em;
      color: var(--text-muted);
    }
    .metric-value {
      margin-top: 4px;
      font-size: 26px;
      font-weight: 700;
    }
    .metric-unit {
      font-size: 11px;
      color: var(--text-muted);
      margin-left: 4px;
    }
    .metric-status {
      font-size: 11px;
      margin-top: 4px;
      color: #bbf7d0;
    }
    .metric-status.bad { color: #fecaca; }
    .metric-bg-accent {
      position: absolute;
      right: -18px;
      top: -18px;
      width: 72px;
      height: 72px;
      border-radius: 999px;
      background: radial-gradient(circle, rgba(22,196,168,0.18), transparent 70%);
      opacity: 0.8;
      pointer-events: none;
    }
    .chart-container { margin-top: 14px; }
    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      margin-bottom: 6px;
    }
    .chart-title {
      font-size: 12px;
      color: var(--text-muted);
    }
    .chart-legend {
      display: inline-flex;
      gap: 10px;
      align-items: center;
      font-size: 11px;
      color: var(--text-muted);
    }
    .legend-dot {
      width: 9px;
      height: 9px;
      border-radius: 999px;
    }
    .ld-hr { background: #f97316; }
    .ld-spo2 { background: #22c55e; }
    canvas {
      width: 100%;
      height: 230px;
      border-radius: 12px;
      background: radial-gradient(circle at top, #020617, #020617);
      border: 1px solid rgba(15, 23, 42, 0.9);
    }
    .note {
      margin-top: 10px;
      font-size: 11px;
      color: var(--text-muted);
      line-height: 1.4;
    }
    .note span {
      color: var(--accent);
      font-weight: 500;
    }
  </style>
</head>
<body>
<div class="page">
  <header class="header">
    <div class="title-block">
      <div class="app-title">PATIENT VITALS MONITOR</div>
      <div class="subtitle">Local Wi-Fi Dashboard</div>
      <div class="info-line">
        Connect to Wi-Fi AP <strong>STC_MAX30100_AP</strong> and open
        <strong>http://192.168.4.1/</strong>.
      </div>
    </div>
    <div class="header-right">
      <div class="pill offline" id="pillConnection">
        <span class="pill-dot"></span>
        <span id="pillText">Device Offline</span>
      </div>
      <button class="btn" id="btnRefresh">Refresh now</button>
    </div>
  </header>

  <main class="layout">
    <!-- LEFT: DEVICE INFO -->
    <section class="card">
      <div class="card-title">DEVICE STATUS</div>
      <div class="device-list">
        <div class="device-item">
          <div>
            <div class="device-main">Vital Node</div>
            <div class="device-sub" id="deviceIp">IP: 192.168.4.1</div>
          </div>
          <div class="pill offline" id="pillNode">
            <span class="pill-dot"></span>
            <span id="pillNodeText">Disconnected</span>
          </div>
        </div>
      </div>
      <div class="note">
        This dashboard shows live SpO₂ and BPM from the sensor node.
        If the device is <span>offline</span>, check ESP32 power and wiring.
      </div>
    </section>

    <!-- RIGHT: VITALS + CHART -->
    <section class="card">
      <div class="card-title">LIVE VITALS</div>
      <div class="metrics-grid">
        <div class="metric-card">
          <div class="metric-bg-accent"></div>
          <div class="metric-label">SpO₂</div>
          <div class="metric-value">
            <span id="valSpo2">--</span>
            <span class="metric-unit">%</span>
          </div>
          <div id="statusSpo2" class="metric-status">Waiting for data...</div>
        </div>
        <div class="metric-card">
          <div class="metric-bg-accent"></div>
          <div class="metric-label">Heart Rate</div>
          <div class="metric-value">
            <span id="valHr">--</span>
            <span class="metric-unit">bpm</span>
          </div>
          <div id="statusHr" class="metric-status">Waiting for data...</div>
        </div>
      </div>

      <div class="chart-container">
        <div class="chart-header">
          <div class="chart-title">Recent trend (last few minutes)</div>
          <div class="chart-legend">
            <span class="legend-dot ld-hr"></span> HR
            <span class="legend-dot ld-spo2"></span> SpO₂
          </div>
        </div>
        <canvas id="chart"></canvas>
      </div>

      <div class="note">
        API endpoint used by this page:
        <span>GET /data → {"spo2":97.5,"bpm":78,"valid":true}</span>
      </div>
    </section>
  </main>
</div>

<script>
  const API_URL = "/data";
  const POLL_INTERVAL_MS = 1000;
  const HISTORY_MS = 5 * 60 * 1000;

  let dataBuffer = []; // { t, spo2, hr }
  let lastOk = 0;

  const elSpo2 = document.getElementById("valSpo2");
  const elHr = document.getElementById("valHr");

  const elStatusSpo2 = document.getElementById("statusSpo2");
  const elStatusHr = document.getElementById("statusHr");

  const pillConnection = document.getElementById("pillConnection");
  const pillText = document.getElementById("pillText");
  const pillNode = document.getElementById("pillNode");
  const pillNodeText = document.getElementById("pillNodeText");

  const btnRefresh = document.getElementById("btnRefresh");
  const deviceIp = document.getElementById("deviceIp");

  deviceIp.textContent = "IP: " + (location.hostname || "192.168.4.1");

  function setOnline(isOnline) {
    const clsOn = "pill online";
    const clsOff = "pill offline";
    if (isOnline) {
      pillConnection.className = clsOn;
      pillNode.className = clsOn;
      pillText.textContent = "Device Online";
      pillNodeText.textContent = "Connected";
    } else {
      pillConnection.className = clsOff;
      pillNode.className = clsOff;
      pillText.textContent = "Device Offline";
      pillNodeText.textContent = "Disconnected";
    }
  }

  function classifySpo2(v) {
    if (isNaN(v) || v <= 0) return { text: "Waiting for data...", bad: false };
    if (v < 90) return { text: "Critical low SpO₂", bad: true };
    if (v < 94) return { text: "Below normal, monitor closely", bad: true };
    return { text: "Within normal range", bad: false };
  }

  function classifyHr(v) {
    if (isNaN(v) || v <= 0) return { text: "Waiting for data...", bad: false };
    if (v < 50) return { text: "Low heart rate", bad: true };
    if (v > 120) return { text: "High heart rate", bad: true };
    return { text: "Within normal range", bad: false };
  }

  async function fetchVitals() {
    try {
      const res = await fetch(API_URL, { cache: "no-store" });
      if (!res.ok) throw new Error("HTTP " + res.status);
      const j = await res.json();

      const now = Date.now();
      lastOk = now;

      const spo2 = parseFloat(j.spo2);
      const hr = parseFloat(j.bpm);

      elSpo2.textContent = isNaN(spo2) || spo2 <= 0 ? "--" : spo2.toFixed(1);
      elHr.textContent   = isNaN(hr)   || hr   <= 0 ? "--" : Math.round(hr);

      const sSpo2 = classifySpo2(spo2);
      elStatusSpo2.textContent = sSpo2.text;
      elStatusSpo2.className = "metric-status" + (sSpo2.bad ? " bad" : "");

      const sHr = classifyHr(hr);
      elStatusHr.textContent = sHr.text;
      elStatusHr.className = "metric-status" + (sHr.bad ? " bad" : "");

      setOnline(j.valid === true);

      dataBuffer.push({ t: now, spo2, hr });
      const cutoff = now - HISTORY_MS;
      dataBuffer = dataBuffer.filter(d => d.t >= cutoff);

      drawChart();
    } catch (e) {
      const since = Date.now() - lastOk;
      if (!lastOk || since > 4000) {
        setOnline(false);
      }
    }
  }

  btnRefresh.addEventListener("click", fetchVitals);
  setInterval(fetchVitals, POLL_INTERVAL_MS);
  fetchVitals();

  const canvas = document.getElementById("chart");
  const ctx = canvas.getContext("2d");

  function resizeCanvas() {
    const rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * window.devicePixelRatio;
    canvas.height = rect.height * window.devicePixelRatio;
    ctx.setTransform(window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
  }
  window.addEventListener("resize", () => {
    resizeCanvas();
    drawChart();
  });
  resizeCanvas();

  function drawChart() {
    const w = canvas.width / window.devicePixelRatio;
    const h = canvas.height / window.devicePixelRatio;
    ctx.clearRect(0, 0, w, h);

    if (dataBuffer.length < 2) {
      ctx.fillStyle = "#4b5563";
      ctx.font = "11px system-ui";
      ctx.fillText("Waiting for enough data to draw graph...", 12, h / 2);
      return;
    }

    const tMin = dataBuffer[0].t;
    const tMax = dataBuffer[dataBuffer.length - 1].t;
    const xPad = 16;
    const yPad = 18;

    function tx(t) {
      if (tMax === tMin) return xPad;
      return xPad + (w - 2 * xPad) * (t - tMin) / (tMax - tMin);
    }

    const spo2Vals = dataBuffer.map(d => d.spo2).filter(v => !isNaN(v) && v > 0);
    const hrVals   = dataBuffer.map(d => d.hr).filter(v => !isNaN(v) && v > 0);

    const spo2Min = spo2Vals.length ? Math.min(...spo2Vals, 85) : 90;
    const spo2Max = spo2Vals.length ? Math.max(...spo2Vals, 100) : 100;
    const hrMin   = hrVals.length   ? Math.min(...hrVals, 40)   : 40;
    const hrMax   = hrVals.length   ? Math.max(...hrVals, 140)  : 140;

    function mapY(v, min, max, laneIndex) {
      const lanes = 2;
      const laneH = (h - 2 * yPad) / lanes;
      const top = yPad + laneIndex * laneH;
      const bottom = top + laneH - 10;
      if (max === min) return (top + bottom) / 2;
      const ratio = (v - min) / (max - min);
      return bottom - ratio * (bottom - top);
    }

    ctx.strokeStyle = "rgba(30,64,88,0.9)";
    ctx.lineWidth = 1;
    const midY = yPad + (h - 2 * yPad) / 2;
    ctx.beginPath();
    ctx.moveTo(xPad, midY);
    ctx.lineTo(w - xPad, midY);
    ctx.stroke();

    function drawSeries(getVal, min, max, laneIndex, color) {
      ctx.beginPath();
      let started = false;
      dataBuffer.forEach(d => {
        const v = getVal(d);
        if (isNaN(v) || v <= 0) return;
        const x = tx(d.t);
        const y = mapY(v, min, max, laneIndex);
        if (!started) {
          ctx.moveTo(x, y);
          started = true;
        } else {
          ctx.lineTo(x, y);
        }
      });
      if (started) {
        ctx.strokeStyle = color;
        ctx.lineWidth = 1.7;
        ctx.stroke();
      }
    }

    drawSeries(d => d.hr,   hrMin,   hrMax,   0, "#fb923c");
    drawSeries(d => d.spo2, spo2Min, spo2Max, 1, "#22c55e");
  }
</script>
</body>
</html>
)rawlite";























// ====================  WEB HANDLERS  =========================
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  String json = "{";
  json += "\"spo2\":" + String((int)gSpo2) + ",";
  json += "\"bpm\":"  + String((int)gBpm)  + ",";
  json += "\"valid\":" + String(gValid ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "STC: Resource not found");
}

// ===========================  SETUP  =========================
void setup() {
  Serial.begin(115200);
  delay(1000);
  STC_BrandingSerial();

  Wire.begin(21, 22);
  Serial.println("[STC] I2C started on SDA=21, SCL=22");
  Serial.println("[STC] MAX30100 at 0x57, OLED at 0x3C");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[STC] ❌ OLED init FAILED!");
  } else {
    Serial.println("[STC] ✔ OLED init SUCCESS.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("STC Creative Club");
    display.setCursor(0, 10);
    display.println("MAX30100 + ESP32");
    display.setCursor(0, 20);
    display.println("WiFi: 192.168.4.1");
    display.display();
  }

  Serial.println("[STC] Initializing MAX30100 sensor...");
  if (!pox.begin()) {
    Serial.println("[STC] ❌ MAX30100 init FAILED!");
    while (1) delay(1000);
  }

  Serial.println("[STC] ✔ MAX30100 init SUCCESS!");
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  Serial.println("[STC] Place your finger on the sensor gently...");
  Serial.println("[STC] Web dashboard: http://192.168.4.1");

  updateOLED(0, 0, false);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(apSsid, apPass);

  Serial.println();
  Serial.println("[STC] WiFi Access Point Started:");
  Serial.print ("      SSID : "); Serial.println(apSsid);
  Serial.print ("      PASS : "); Serial.println(apPass);
  Serial.print ("      IP   : "); Serial.println(WiFi.softAPIP());
  Serial.println();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("[STC] Web server running...");
  Serial.println("[STC] Open browser on:  http://192.168.4.1");
  Serial.println("------------------------------------------------");
}

// ============================  LOOP  =========================
void loop() {
  pox.update();
  server.handleClient();

  if (millis() - lastRead > 1000) {
    lastRead = millis();

    float currentBpm  = pox.getHeartRate();
    float currentSpo2 = pox.getSpO2();

    if (currentBpm > 30 && currentBpm < 220 && currentSpo2 > 60 && currentSpo2 <= 100) {
      gBpm   = currentBpm;
      gSpo2  = currentSpo2;
      gValid = true;
    } else {
      gValid = false;
    }

    Serial.println("------ [STC SENSOR DATA] ------");
    Serial.print  ("SpO2 : ");
    if (gValid) Serial.println(gSpo2);
    else        Serial.println("--");
    Serial.print  ("BPM  : ");
    if (gValid) Serial.println(gBpm);
    else        Serial.println("--");
    Serial.println("--------------------------------");

    updateOLED(gSpo2, gBpm, gValid);
  }
}
