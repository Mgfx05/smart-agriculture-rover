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
// Live Sensors + 16x2 LCD + Local Web Dashboard
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
// Rain threshold percentage for status alert
const float RAIN_ALERT_PERCENT = 40.0;

// LCD page switching interval
const unsigned long LCD_PAGE_INTERVAL = 3000;

// Sensor reading interval
const unsigned long SENSOR_INTERVAL = 2000;

// ---------------- CALIBRATION VALUES -----------------
// Replace these with your actual ADS1115 readings from dry and wet conditions.
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
// LOCAL WEB DASHBOARD (HTML + CSS + JS)
// =====================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Agriculture Rover</title>
  <style>
    body { font-family: Arial, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; text-align: center; }
    h1 { color: #38bdf8; margin-bottom: 5px; }
    .sub { color: #94a3b8; font-size: 0.9rem; margin-bottom: 20px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; max-width: 850px; margin: 0 auto; }
    .card { background: #1e293b; border-radius: 12px; padding: 18px; box-shadow: 0 4px 10px rgba(0,0,0,0.3); }
    .label { font-size: 0.9rem; color: #94a3b8; margin-bottom: 8px; }
    .val { font-size: 1.8rem; font-weight: bold; color: #38bdf8; }
    .ok { color: #4ade80; }
    .warn { color: #f87171; }
  </style>
</head>
<body>
  <h1>Smart Agriculture Rover</h1>
  <div class="sub">Real-Time Sensor Telemetry &bull; Auto-updates every 2s</div>
  <div class="grid">
    <div class="card"><div class="label">Soil Moisture 1</div><div class="val"><span id="s1">--</span>%</div></div>
    <div class="card"><div class="label">Soil Moisture 2</div><div class="val"><span id="s2">--</span>%</div></div>
    <div class="card"><div class="label">Average Soil</div><div class="val"><span id="savg">--</span>%</div></div>
    <div class="card"><div class="label">Temperature</div><div class="val"><span id="temp">--</span> &deg;C</div></div>
    <div class="card"><div class="label">Humidity</div><div class="val"><span id="hum">--</span>%</div></div>
    <div class="card"><div class="label">Light Intensity</div><div class="val"><span id="lux">--</span> lx</div></div>
    <div class="card"><div class="label">Rain Status</div><div class="val" id="rain">--</div></div>
  </div>

  <script>
    async function fetchData() {
      try {
        const res = await fetch('/data');
        const d = await res.json();
        document.getElementById('s1').textContent = d.soil1.toFixed(0);
        document.getElementById('s2').textContent = d.soil2.toFixed(0);
        document.getElementById('savg').textContent = d.avgSoil.toFixed(0);
        document.getElementById('temp').textContent = d.temp.toFixed(1);
        document.getElementById('hum').textContent = d.hum.toFixed(0);
        document.getElementById('lux').textContent = d.lux.toFixed(0);

        const rainEl = document.getElementById('rain');
        rainEl.textContent = d.rain ? 'RAIN (' + d.rainPct.toFixed(0) + '%)' : 'Dry';
        rainEl.className = 'val ' + (d.rain ? 'warn' : 'ok');
      } catch (e) {
        console.log('Fetch error:', e);
      }
    }
    fetchData();
    setInterval(fetchData, 2000);
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