#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// H2SSensor - ELT H2S-SM30-3V + ESP32_MOS_X4
// Initial engineering prototype. Validate against actual hardware before safety use.

namespace HW {
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MOS_LOW_ALARM = 16;   // OUT1
constexpr uint8_t MOS_HIGH_ALARM = 17;  // OUT2
constexpr uint8_t SENSOR_SPAN = 19;      // ELT manual span, active LOW
constexpr uint8_t SENSOR_ZERO = 23;      // ELT manual zero, active LOW
constexpr uint8_t SENSOR_RESET = 18;     // ELT reset, active LOW
constexpr uint8_t BTN_CAL = 32;          // button to GND
constexpr uint8_t BTN_ZERO = 33;         // button to GND
constexpr uint8_t BTN_ENTER = 25;        // button to GND
constexpr uint8_t H2S_ADDR = 0x72;
constexpr uint8_t OLED_ADDR = 0x3C;
}

constexpr char FW_VERSION[] = "H2SSensor-0.1.0";
constexpr uint32_t SENSOR_PERIOD_MS = 1000;
constexpr uint32_t DISPLAY_PERIOD_MS = 500;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t SENSOR_CAL_MS = 60000; // ELT datasheet: manual span/zero ~1 minute
constexpr uint32_t SENSOR_RESET_MS = 300;
constexpr uint32_t SETPOINT_HOLD_MS = 3000;
constexpr uint8_t MAX_FILTER_SAMPLES = 30;

Preferences prefs;
WebServer web(80);
DNSServer dns;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOK = false;

struct Config {
  float lowSP = 5.0f;
  float highSP = 10.0f;
  float hysteresis = 0.5f;
  uint8_t filterSamples = 10;
  float calRawZero = 0.0f;
  float calGain = 1.0f;

  String wifiSsid;
  String wifiPass;

  bool mqttEnabled = false;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPass;
  String baseTopic = "sensors/h2s1";
  uint32_t mqttMinPublishSec = 10;

  bool tagPpm = true;
  bool tagRaw = true;
  bool tagLow = true;
  bool tagHigh = true;
  bool tagSensorOK = true;
  bool tagLowSP = false;
  bool tagHighSP = false;
  String topicPpm = "h2s_ppm";
  String topicRaw = "h2s_raw_ppm";
  String topicLow = "alarm_low";
  String topicHigh = "alarm_high";
  String topicSensorOK = "sensor_ok";
  String topicLowSP = "low_setpoint";
  String topicHighSP = "high_setpoint";
} cfg;

struct SensorState {
  float rawPpm = NAN;
  float calibratedPpm = NAN;
  float filteredPpm = NAN;
  bool ok = false;
  uint32_t goodReads = 0;
  uint32_t badReads = 0;
  uint32_t lastGoodMs = 0;
  uint8_t packet[7] = {0};
  bool lowAlarm = false;
  bool highAlarm = false;
} sensor;

float filterBuf[MAX_FILTER_SAMPLES] = {0};
uint8_t filterCount = 0;
uint8_t filterHead = 0;
float filterSum = 0;

struct ButtonState {
  uint8_t pin;
  bool stable = true; // INPUT_PULLUP; true = released
  bool lastRaw = true;
  uint32_t changedMs = 0;
  uint32_t pressedMs = 0;
};
ButtonState bCal{HW::BTN_CAL}, bZero{HW::BTN_ZERO}, bEnter{HW::BTN_ENTER};

enum class SetpointMode : uint8_t { NONE, LOW, HIGH };
SetpointMode setMode = SetpointMode::NONE;
float editLowSP = 0;
float editHighSP = 0;
bool chordConsumed = false;
uint32_t chordStartMs = 0;

struct SensorAction {
  enum Kind : uint8_t { NONE, SPAN, ZERO, RESET } kind = NONE;
  uint32_t untilMs = 0;
} sensorAction;

uint32_t lastSensorMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttPublishMs = 0;
String apName;

static String htmlEscape(const String &s) {
  String o;
  o.reserve(s.length() + 16);
  for (char c : s) {
    if (c == '&') o += F("&amp;");
    else if (c == '<') o += F("&lt;");
    else if (c == '>') o += F("&gt;");
    else if (c == '\"') o += F("&quot;");
    else o += c;
  }
  return o;
}

static String pageStart(const String &title) {
  String s = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<meta charset='utf-8'><title>");
  s += title;
  s += F("</title><style>body{font-family:Arial,sans-serif;max-width:900px;margin:auto;padding:16px;background:#f5f7f9;color:#182026}"
         "nav a{margin-right:12px} .card{background:white;border:1px solid #ccd3d9;border-radius:10px;padding:16px;margin:12px 0}"
         ".big{font-size:42px;font-weight:700}.ok{color:#16803a}.warn{color:#b26800}.alarm{color:#b00020;font-weight:700}"
         "label{display:block;margin:8px 0 2px}input,select{padding:8px;max-width:100%;box-sizing:border-box}"
         "button{padding:9px 14px;margin:8px 4px 8px 0}table{border-collapse:collapse;width:100%}td,th{padding:6px;border-bottom:1px solid #ddd;text-align:left}"
         "code{background:#eef;padding:2px 4px}</style></head><body><nav>"
         "<a href='/'>Status</a><a href='/wifi'>Wi-Fi</a><a href='/alarms'>Alarms</a><a href='/cal'>Calibration</a>"
         "<a href='/mqtt'>MQTT</a><a href='/tags'>Tags</a><a href='/instructions'>Instructions</a></nav>");
  return s;
}

static String pageEnd() {
  return String(F("<div class='card'><small>")) + FW_VERSION + F("</small></div></body></html>");
}

static String fmtFloat(float v, uint8_t dec = 1) {
  return isfinite(v) ? String(v, dec) : String("--");
}

static bool argBool(const char *name) { return web.hasArg(name); }

void saveConfig() {
  prefs.begin("h2ssensor", false);
  prefs.putFloat("lowSP", cfg.lowSP);
  prefs.putFloat("highSP", cfg.highSP);
  prefs.putFloat("hyst", cfg.hysteresis);
  prefs.putUChar("filtN", cfg.filterSamples);
  prefs.putFloat("rawZero", cfg.calRawZero);
  prefs.putFloat("calGain", cfg.calGain);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);
  prefs.putBool("mqEn", cfg.mqttEnabled);
  prefs.putString("mqHost", cfg.mqttHost);
  prefs.putUShort("mqPort", cfg.mqttPort);
  prefs.putString("mqUser", cfg.mqttUser);
  prefs.putString("mqPass", cfg.mqttPass);
  prefs.putString("base", cfg.baseTopic);
  prefs.putUInt("mqSec", cfg.mqttMinPublishSec);
  prefs.putBool("tPpm", cfg.tagPpm); prefs.putBool("tRaw", cfg.tagRaw);
  prefs.putBool("tLow", cfg.tagLow); prefs.putBool("tHigh", cfg.tagHigh);
  prefs.putBool("tOk", cfg.tagSensorOK); prefs.putBool("tLsp", cfg.tagLowSP); prefs.putBool("tHsp", cfg.tagHighSP);
  prefs.putString("nPpm", cfg.topicPpm); prefs.putString("nRaw", cfg.topicRaw);
  prefs.putString("nLow", cfg.topicLow); prefs.putString("nHigh", cfg.topicHigh);
  prefs.putString("nOk", cfg.topicSensorOK); prefs.putString("nLsp", cfg.topicLowSP); prefs.putString("nHsp", cfg.topicHighSP);
  prefs.end();
}

void loadConfig() {
  prefs.begin("h2ssensor", true);
  cfg.lowSP = prefs.getFloat("lowSP", cfg.lowSP);
  cfg.highSP = prefs.getFloat("highSP", cfg.highSP);
  cfg.hysteresis = prefs.getFloat("hyst", cfg.hysteresis);
  cfg.filterSamples = constrain((int)prefs.getUChar("filtN", cfg.filterSamples), 1, (int)MAX_FILTER_SAMPLES);
  cfg.calRawZero = prefs.getFloat("rawZero", cfg.calRawZero);
  cfg.calGain = prefs.getFloat("calGain", cfg.calGain);
  cfg.wifiSsid = prefs.getString("ssid", "");
  cfg.wifiPass = prefs.getString("wpass", "");
  cfg.mqttEnabled = prefs.getBool("mqEn", cfg.mqttEnabled);
  cfg.mqttHost = prefs.getString("mqHost", "");
  cfg.mqttPort = prefs.getUShort("mqPort", cfg.mqttPort);
  cfg.mqttUser = prefs.getString("mqUser", "");
  cfg.mqttPass = prefs.getString("mqPass", "");
  cfg.baseTopic = prefs.getString("base", cfg.baseTopic);
  cfg.mqttMinPublishSec = max<uint32_t>(1, prefs.getUInt("mqSec", cfg.mqttMinPublishSec));
  cfg.tagPpm = prefs.getBool("tPpm", cfg.tagPpm); cfg.tagRaw = prefs.getBool("tRaw", cfg.tagRaw);
  cfg.tagLow = prefs.getBool("tLow", cfg.tagLow); cfg.tagHigh = prefs.getBool("tHigh", cfg.tagHigh);
  cfg.tagSensorOK = prefs.getBool("tOk", cfg.tagSensorOK); cfg.tagLowSP = prefs.getBool("tLsp", cfg.tagLowSP); cfg.tagHighSP = prefs.getBool("tHsp", cfg.tagHighSP);
  cfg.topicPpm = prefs.getString("nPpm", cfg.topicPpm); cfg.topicRaw = prefs.getString("nRaw", cfg.topicRaw);
  cfg.topicLow = prefs.getString("nLow", cfg.topicLow); cfg.topicHigh = prefs.getString("nHigh", cfg.topicHigh);
  cfg.topicSensorOK = prefs.getString("nOk", cfg.topicSensorOK); cfg.topicLowSP = prefs.getString("nLsp", cfg.topicLowSP); cfg.topicHighSP = prefs.getString("nHsp", cfg.topicHighSP);
  prefs.end();
  if (cfg.highSP <= cfg.lowSP) cfg.highSP = cfg.lowSP + 1.0f;
}

void resetFilter() {
  filterCount = filterHead = 0;
  filterSum = 0;
  sensor.filteredPpm = NAN;
}

void addFilterSample(float v) {
  if (!isfinite(v)) return;
  const uint8_t n = constrain(cfg.filterSamples, (uint8_t)1, MAX_FILTER_SAMPLES);
  if (filterCount < n) {
    filterBuf[filterHead] = v;
    filterSum += v;
    filterHead = (filterHead + 1) % n;
    filterCount++;
  } else {
    filterSum -= filterBuf[filterHead];
    filterBuf[filterHead] = v;
    filterSum += v;
    filterHead = (filterHead + 1) % n;
  }
  sensor.filteredPpm = filterSum / filterCount;
}

bool readH2S(float &ppm) {
  Wire.beginTransmission(HW::H2S_ADDR);
  Wire.write((uint8_t)'R');
  uint8_t rc = Wire.endTransmission(true);
  if (rc != 0) return false;
  delay(2);

  size_t got = Wire.requestFrom((uint8_t)HW::H2S_ADDR, (uint8_t)7, (uint8_t)true);
  if (got != 7) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < 7; ++i) {
    if (!Wire.available()) return false;
    sensor.packet[i] = Wire.read();
    delay(1);
  }

  // ELT documents a 7-byte layout: configuration (1), H2S (2), reserved (4).
  // Prefer big-endian; use little-endian only if it is the sole plausible 0..300 ppm value.
  uint16_t be = (uint16_t(sensor.packet[1]) << 8) | sensor.packet[2];
  uint16_t le = (uint16_t(sensor.packet[2]) << 8) | sensor.packet[1];
  uint16_t chosen = be;
  if (be > 300 && le <= 300) chosen = le;
  ppm = (float)chosen;
  return chosen <= 1000; // accept overload/debug values while rejecting obvious corrupt packets
}

void updateAlarms() {
  if (!sensor.ok || !isfinite(sensor.filteredPpm)) {
    sensor.lowAlarm = false;
    sensor.highAlarm = false;
  } else {
    if (!sensor.lowAlarm && sensor.filteredPpm >= cfg.lowSP) sensor.lowAlarm = true;
    else if (sensor.lowAlarm && sensor.filteredPpm <= cfg.lowSP - cfg.hysteresis) sensor.lowAlarm = false;

    if (!sensor.highAlarm && sensor.filteredPpm >= cfg.highSP) sensor.highAlarm = true;
    else if (sensor.highAlarm && sensor.filteredPpm <= cfg.highSP - cfg.hysteresis) sensor.highAlarm = false;
  }
  digitalWrite(HW::MOS_LOW_ALARM, sensor.lowAlarm ? HIGH : LOW);
  digitalWrite(HW::MOS_HIGH_ALARM, sensor.highAlarm ? HIGH : LOW);
}

void sampleSensor() {
  float raw = NAN;
  if (readH2S(raw)) {
    sensor.ok = true;
    sensor.rawPpm = raw;
    sensor.calibratedPpm = (raw - cfg.calRawZero) * cfg.calGain;
    if (sensor.calibratedPpm < 0) sensor.calibratedPpm = 0;
    addFilterSample(sensor.calibratedPpm);
    sensor.goodReads++;
    sensor.lastGoodMs = millis();
  } else {
    sensor.badReads++;
    if (millis() - sensor.lastGoodMs > 5000) sensor.ok = false;
  }
  updateAlarms();
}

void stopSensorAction() {
  digitalWrite(HW::SENSOR_SPAN, HIGH);
  digitalWrite(HW::SENSOR_ZERO, HIGH);
  digitalWrite(HW::SENSOR_RESET, HIGH);
  sensorAction.kind = SensorAction::NONE;
  sensorAction.untilMs = 0;
}

void startSensorAction(SensorAction::Kind kind) {
  stopSensorAction();
  sensorAction.kind = kind;
  uint32_t duration = (kind == SensorAction::RESET) ? SENSOR_RESET_MS : SENSOR_CAL_MS;
  sensorAction.untilMs = millis() + duration;
  if (kind == SensorAction::SPAN) digitalWrite(HW::SENSOR_SPAN, LOW);
  if (kind == SensorAction::ZERO) digitalWrite(HW::SENSOR_ZERO, LOW);
  if (kind == SensorAction::RESET) digitalWrite(HW::SENSOR_RESET, LOW);
  Serial.printf("[ACTION] sensor %s started for %lu ms\n", kind == SensorAction::SPAN ? "SPAN" : kind == SensorAction::ZERO ? "ZERO" : "RESET", (unsigned long)duration);
}

void serviceSensorAction() {
  if (sensorAction.kind != SensorAction::NONE && (int32_t)(millis() - sensorAction.untilMs) >= 0) {
    Serial.println("[ACTION] sensor action complete");
    stopSensorAction();
  }
}

bool debounce(ButtonState &b) {
  bool raw = digitalRead(b.pin);
  if (raw != b.lastRaw) { b.lastRaw = raw; b.changedMs = millis(); }
  if (millis() - b.changedMs >= 30 && raw != b.stable) {
    b.stable = raw;
    if (!raw) b.pressedMs = millis();
    return true;
  }
  return false;
}

void enterSetpointMode() {
  setMode = SetpointMode::LOW;
  editLowSP = cfg.lowSP;
  editHighSP = cfg.highSP;
  chordConsumed = true;
  Serial.println("[UI] setpoint mode LOW");
}

void handleButtons() {
  bool calChanged = debounce(bCal);
  bool zeroChanged = debounce(bZero);
  bool enterChanged = debounce(bEnter);

  bool calDown = !bCal.stable;
  bool zeroDown = !bZero.stable;

  if (calDown && zeroDown && setMode == SetpointMode::NONE) {
    if (chordStartMs == 0) chordStartMs = millis();
    if (!chordConsumed && millis() - chordStartMs >= SETPOINT_HOLD_MS) enterSetpointMode();
  } else if (!calDown && !zeroDown) {
    chordStartMs = 0;
    if (chordConsumed && setMode == SetpointMode::NONE) chordConsumed = false;
  }

  if (setMode != SetpointMode::NONE) {
    if (calChanged && bCal.stable && !zeroDown) {
      if (setMode == SetpointMode::LOW) editLowSP += 0.5f; else editHighSP += 0.5f;
    }
    if (zeroChanged && bZero.stable && !calDown) {
      if (setMode == SetpointMode::LOW) editLowSP = max(0.0f, editLowSP - 0.5f);
      else editHighSP = max(0.0f, editHighSP - 0.5f);
    }
    if (enterChanged && bEnter.stable) {
      if (setMode == SetpointMode::LOW) {
        if (editHighSP <= editLowSP) editHighSP = editLowSP + 1.0f;
        setMode = SetpointMode::HIGH;
      } else {
        if (editHighSP <= editLowSP) editHighSP = editLowSP + 1.0f;
        cfg.lowSP = editLowSP;
        cfg.highSP = editHighSP;
        saveConfig();
        setMode = SetpointMode::NONE;
        chordConsumed = false;
        Serial.println("[UI] setpoints saved");
      }
    }
    return;
  }

  if (!chordConsumed) {
    if (calChanged && bCal.stable && millis() - bCal.pressedMs < SETPOINT_HOLD_MS) startSensorAction(SensorAction::SPAN);
    if (zeroChanged && bZero.stable && millis() - bZero.pressedMs < SETPOINT_HOLD_MS) startSensorAction(SensorAction::ZERO);
    if (enterChanged && bEnter.stable) startSensorAction(SensorAction::RESET);
  }
}

void drawDisplay() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("H2S ");
  display.print(sensor.ok ? "ONLINE" : "OFFLINE");
  if (WiFi.status() == WL_CONNECTED) display.print(" WIFI");

  if (setMode != SetpointMode::NONE) {
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print(setMode == SetpointMode::LOW ? "LOW SET" : "HIGH SET");
    display.setTextSize(2);
    display.setCursor(0, 38);
    display.print(setMode == SetpointMode::LOW ? editLowSP : editHighSP, 1);
    display.print(" ppm");
    display.display();
    return;
  }

  display.setTextSize(3);
  display.setCursor(0, 14);
  if (isfinite(sensor.filteredPpm)) display.print(sensor.filteredPpm, 1); else display.print("--");
  display.setTextSize(1);
  display.print(" ppm");
  display.setCursor(0, 44);
  display.print("L:"); display.print(cfg.lowSP, 1);
  display.print(" H:"); display.print(cfg.highSP, 1);
  display.setCursor(0, 54);
  if (sensorAction.kind == SensorAction::SPAN) display.print("SPAN CAL ACTIVE");
  else if (sensorAction.kind == SensorAction::ZERO) display.print("ZERO CAL ACTIVE");
  else if (sensor.highAlarm) display.print("*** HIGH ALARM ***");
  else if (sensor.lowAlarm) display.print("** LOW ALARM **");
  else display.print("NORMAL  AP:"); display.print(WiFi.softAPIP());
  display.display();
}

void startWifi() {
  uint64_t mac = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06X", (uint32_t)(mac & 0xFFFFFF));
  apName = String("H2SSensor-") + suffix;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str());
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[WIFI] AP %s IP %s\n", apName.c_str(), WiFi.softAPIP().toString().c_str());

  if (cfg.wifiSsid.length()) {
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    lastWifiAttemptMs = millis();
  }
}

void serviceWifi() {
  dns.processNextRequest();
  if (WiFi.status() != WL_CONNECTED && cfg.wifiSsid.length() && millis() - lastWifiAttemptMs >= WIFI_RETRY_MS) {
    Serial.printf("[WIFI] retry %s\n", cfg.wifiSsid.c_str());
    WiFi.disconnect();
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    lastWifiAttemptMs = millis();
  }
}

String topic(const String &suffix) {
  String base = cfg.baseTopic;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/" + suffix;
}

void mqttPublish(const String &suffix, const String &payload) {
  String t = topic(suffix);
  mqtt.publish(t.c_str(), payload.c_str(), true);
}

void serviceMqtt() {
  if (!cfg.mqttEnabled || !cfg.mqttHost.length() || WiFi.status() != WL_CONNECTED) return;
  mqtt.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
  if (!mqtt.connected()) {
    String id = String("H2SSensor-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    bool ok = cfg.mqttUser.length() ? mqtt.connect(id.c_str(), cfg.mqttUser.c_str(), cfg.mqttPass.c_str()) : mqtt.connect(id.c_str());
    if (!ok) return;
  }
  mqtt.loop();

  if (millis() - lastMqttPublishMs < cfg.mqttMinPublishSec * 1000UL) return;
  lastMqttPublishMs = millis();
  if (cfg.tagPpm && isfinite(sensor.filteredPpm)) mqttPublish(cfg.topicPpm, String(sensor.filteredPpm, 2));
  if (cfg.tagRaw && isfinite(sensor.rawPpm)) mqttPublish(cfg.topicRaw, String(sensor.rawPpm, 2));
  if (cfg.tagLow) mqttPublish(cfg.topicLow, sensor.lowAlarm ? "1" : "0");
  if (cfg.tagHigh) mqttPublish(cfg.topicHigh, sensor.highAlarm ? "1" : "0");
  if (cfg.tagSensorOK) mqttPublish(cfg.topicSensorOK, sensor.ok ? "1" : "0");
  if (cfg.tagLowSP) mqttPublish(cfg.topicLowSP, String(cfg.lowSP, 2));
  if (cfg.tagHighSP) mqttPublish(cfg.topicHighSP, String(cfg.highSP, 2));
}

void redirectHome() {
  web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  web.send(302, "text/plain", "");
}

void handleStatus() {
  String s = pageStart("H2SSensor Status");
  String alarmClass = sensor.highAlarm ? "alarm" : sensor.lowAlarm ? "warn" : "ok";
  String alarmText = sensor.highAlarm ? "HIGH ALARM" : sensor.lowAlarm ? "LOW ALARM" : sensor.ok ? "NORMAL" : "SENSOR OFFLINE";
  s += F("<div class='card'><h1>H2SSensor</h1><div class='big'>"); s += fmtFloat(sensor.filteredPpm); s += F(" ppm</div><div class='"); s += alarmClass; s += "'>" + alarmText + F("</div></div>");
  s += F("<div class='card'><table>");
  s += F("<tr><th>Filtered H2S</th><td>") + fmtFloat(sensor.filteredPpm, 2) + F(" ppm</td></tr>");
  s += F("<tr><th>Calibrated instantaneous</th><td>") + fmtFloat(sensor.calibratedPpm, 2) + F(" ppm</td></tr>");
  s += F("<tr><th>Raw sensor</th><td>") + fmtFloat(sensor.rawPpm, 2) + F(" ppm</td></tr>");
  s += F("<tr><th>Low / High SP</th><td>") + String(cfg.lowSP,1) + " / " + String(cfg.highSP,1) + F(" ppm</td></tr>");
  s += F("<tr><th>Sensor reads</th><td>") + String(sensor.goodReads) + " good / " + String(sensor.badReads) + F(" bad</td></tr>");
  s += F("<tr><th>Station Wi-Fi</th><td>") + (WiFi.status() == WL_CONNECTED ? htmlEscape(WiFi.SSID()) + " / " + WiFi.localIP().toString() : String("not connected")) + F("</td></tr>");
  s += F("<tr><th>Setup AP</th><td>") + apName + " / " + WiFi.softAPIP().toString() + F("</td></tr>");
  s += F("<tr><th>MQTT</th><td>") + String(cfg.mqttEnabled ? (mqtt.connected() ? "connected" : "enabled, not connected") : "disabled") + F("</td></tr></table></div>");
  s += F("<script>setTimeout(()=>location.reload(),3000)</script>") + pageEnd();
  web.send(200, "text/html", s);
}

void handleWifi() {
  if (web.method() == HTTP_POST) {
    if (web.hasArg("standalone")) {
      cfg.wifiSsid = ""; cfg.wifiPass = ""; saveConfig(); WiFi.disconnect(true, false);
    } else {
      cfg.wifiSsid = web.arg("ssid"); cfg.wifiPass = web.arg("pass"); saveConfig();
      WiFi.disconnect(); delay(100); WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str()); lastWifiAttemptMs = millis();
    }
  }
  int n = WiFi.scanNetworks(false, true);
  String s = pageStart("Wi-Fi Setup");
  s += F("<div class='card'><h2>Wi-Fi setup</h2><p>The setup AP stays available for standalone use.</p><form method='post'><label>Network</label><select name='ssid'>");
  if (cfg.wifiSsid.length()) s += "<option selected>" + htmlEscape(cfg.wifiSsid) + "</option>";
  for (int i=0;i<n;i++) s += "<option>" + htmlEscape(WiFi.SSID(i)) + "</option>";
  s += F("</select><label>Password</label><input name='pass' type='password'><br><button>Save & connect</button></form>"
         "<form method='post'><button name='standalone' value='1'>Use standalone / forget Wi-Fi</button></form></div>");
  s += pageEnd(); web.send(200, "text/html", s); WiFi.scanDelete();
}

void handleAlarms() {
  String msg;
  if (web.method() == HTTP_POST) {
    float low = web.arg("low").toFloat(); float high = web.arg("high").toFloat();
    float hyst = web.arg("hyst").toFloat(); int fn = web.arg("filter").toInt();
    if (low >= 0 && high > low && hyst >= 0 && fn >= 1 && fn <= MAX_FILTER_SAMPLES) {
      cfg.lowSP=low; cfg.highSP=high; cfg.hysteresis=hyst; cfg.filterSamples=fn; saveConfig(); resetFilter(); msg="Saved.";
    } else msg="Invalid values: HIGH must be greater than LOW.";
  }
  String s=pageStart("Alarm Setup"); s+=F("<div class='card'><h2>Alarm and filter setup</h2><p>")+msg+F("</p><form method='post'>");
  s+=F("<label>Low alarm ppm</label><input name='low' type='number' step='0.1' value='")+String(cfg.lowSP,1)+F("'>");
  s+=F("<label>High alarm ppm</label><input name='high' type='number' step='0.1' value='")+String(cfg.highSP,1)+F("'>");
  s+=F("<label>Clear hysteresis ppm</label><input name='hyst' type='number' step='0.1' value='")+String(cfg.hysteresis,1)+F("'>");
  s+=F("<label>Rolling-average samples (1-30; one sample/sec)</label><input name='filter' type='number' min='1' max='30' value='")+String(cfg.filterSamples)+F("'><br><button>Save</button></form></div>")+pageEnd();
  web.send(200,"text/html",s);
}

void handleCalibration() {
  String msg;
  if (web.method() == HTTP_POST) {
    String action=web.arg("action");
    if (action=="swzero" && isfinite(sensor.rawPpm)) { cfg.calRawZero=sensor.rawPpm; saveConfig(); resetFilter(); msg="Software zero captured."; }
    else if (action=="swspan" && isfinite(sensor.rawPpm)) {
      float ref=web.arg("reference").toFloat(); float delta=sensor.rawPpm-cfg.calRawZero;
      if (ref>0 && fabsf(delta)>0.001f) { cfg.calGain=ref/delta; saveConfig(); resetFilter(); msg="Software span saved."; } else msg="Invalid span reference/current raw value.";
    } else if (action=="resetsoft") { cfg.calRawZero=0; cfg.calGain=1; saveConfig(); resetFilter(); msg="Software calibration reset."; }
    else if (action=="sensorzero") { startSensorAction(SensorAction::ZERO); msg="ELT zero sequence started (60 s)."; }
    else if (action=="sensorspan") { startSensorAction(SensorAction::SPAN); msg="ELT span sequence started (60 s)."; }
    else if (action=="sensorreset") { startSensorAction(SensorAction::RESET); msg="ELT reset pulse started."; }
  }
  String s=pageStart("Calibration"); s+=F("<div class='card'><h2>Calibration</h2><p><b>Warning:</b> only calibrate with the correct zero/span gas and procedure for your exact ELT sensor revision.</p><p>")+msg+F("</p>");
  s+=F("<p>Current raw: <b>")+fmtFloat(sensor.rawPpm,2)+F(" ppm</b>; software zero raw=")+String(cfg.calRawZero,2)+F(", gain=")+String(cfg.calGain,6)+F("</p>");
  s+=F("<form method='post'><button name='action' value='swzero'>Capture software ZERO now</button></form>"
       "<form method='post'><label>Known span concentration (ppm)</label><input name='reference' type='number' step='0.1' value='50'><button name='action' value='swspan'>Apply software SPAN</button></form>"
       "<form method='post'><button name='action' value='resetsoft'>Reset software calibration 1:1</button></form></div>");
  s+=F("<div class='card'><h3>ELT sensor hardware controls</h3><p>These drive the module's active-low manual calibration/reset pins.</p>"
       "<form method='post'><button name='action' value='sensorzero'>Start ELT ZERO (60 s)</button><button name='action' value='sensorspan'>Start ELT SPAN (60 s)</button><button name='action' value='sensorreset'>Reset ELT sensor</button></form></div>")+pageEnd();
  web.send(200,"text/html",s);
}

void handleMqtt() {
  if (web.method()==HTTP_POST) {
    cfg.mqttEnabled=argBool("enabled"); cfg.mqttHost=web.arg("host"); cfg.mqttPort=constrain(web.arg("port").toInt(),1,65535);
    cfg.mqttUser=web.arg("user"); if (web.arg("pass").length()) cfg.mqttPass=web.arg("pass"); cfg.baseTopic=web.arg("base");
    cfg.mqttMinPublishSec=max(1,web.arg("sec").toInt()); saveConfig(); mqtt.disconnect();
  }
  String s=pageStart("MQTT Setup"); s+=F("<div class='card'><h2>MQTT setup</h2><form method='post'><label><input type='checkbox' name='enabled' "); if(cfg.mqttEnabled)s+="checked"; s+=F("> Enable MQTT</label>");
  s+=F("<label>Broker host/IP</label><input name='host' value='")+htmlEscape(cfg.mqttHost)+F("'><label>Port</label><input name='port' type='number' value='")+String(cfg.mqttPort)+F("'>");
  s+=F("<label>User</label><input name='user' value='")+htmlEscape(cfg.mqttUser)+F("'><label>Password (leave blank to keep)</label><input name='pass' type='password'>");
  s+=F("<label>Base topic</label><input name='base' value='")+htmlEscape(cfg.baseTopic)+F("'><label>Minimum publish interval, seconds</label><input name='sec' type='number' min='1' value='")+String(cfg.mqttMinPublishSec)+F("'><br><button>Save</button></form></div>")+pageEnd();
  web.send(200,"text/html",s);
}

void tagRow(String &s,const char *label,const char *checkName,bool enabled,const char *topicName,const String &topicVal) {
  s += "<tr><td>" + String(label) + "</td><td><input type='checkbox' name='" + checkName + "' "; if(enabled)s+="checked"; s += "></td><td><input name='" + String(topicName) + "' value='" + htmlEscape(topicVal) + "'></td></tr>";
}

void handleTags() {
  if(web.method()==HTTP_POST) {
    cfg.tagPpm=argBool("ePpm"); cfg.tagRaw=argBool("eRaw"); cfg.tagLow=argBool("eLow"); cfg.tagHigh=argBool("eHigh"); cfg.tagSensorOK=argBool("eOk"); cfg.tagLowSP=argBool("eLsp"); cfg.tagHighSP=argBool("eHsp");
    cfg.topicPpm=web.arg("nPpm"); cfg.topicRaw=web.arg("nRaw"); cfg.topicLow=web.arg("nLow"); cfg.topicHigh=web.arg("nHigh"); cfg.topicSensorOK=web.arg("nOk"); cfg.topicLowSP=web.arg("nLsp"); cfg.topicHighSP=web.arg("nHsp"); saveConfig();
  }
  String s=pageStart("MQTT Tags"); s+=F("<div class='card'><h2>MQTT tag setup</h2><p>Topic suffixes are appended to the configured base topic.</p><form method='post'><table><tr><th>Tag</th><th>Publish</th><th>Suffix</th></tr>");
  tagRow(s,"Filtered H2S ppm","ePpm",cfg.tagPpm,"nPpm",cfg.topicPpm); tagRow(s,"Raw H2S ppm","eRaw",cfg.tagRaw,"nRaw",cfg.topicRaw); tagRow(s,"Low alarm","eLow",cfg.tagLow,"nLow",cfg.topicLow); tagRow(s,"High alarm","eHigh",cfg.tagHigh,"nHigh",cfg.topicHigh); tagRow(s,"Sensor OK","eOk",cfg.tagSensorOK,"nOk",cfg.topicSensorOK); tagRow(s,"Low setpoint","eLsp",cfg.tagLowSP,"nLsp",cfg.topicLowSP); tagRow(s,"High setpoint","eHsp",cfg.tagHighSP,"nHsp",cfg.topicHighSP);
  s+=F("</table><button>Save tags</button></form></div>")+pageEnd(); web.send(200,"text/html",s);
}

void handleInstructions() {
  String s=pageStart("Instructions");
  s+=F("<div class='card'><h2>Quick instructions</h2><ol><li>Power the ESP32_MOS_X4 and H2S-SM30-3V with the required supplies and common ground.</li>"
       "<li>Join the H2SSensor setup AP and open <code>192.168.4.1</code>.</li><li>Confirm Sensor ONLINE and verify the reading with known gas/reference equipment.</li>"
       "<li>Configure LOW/HIGH setpoints and rolling filter.</li><li>Optional: configure Wi-Fi and MQTT.</li>"
       "<li>Hold CAL + ZERO for 3 seconds to adjust setpoints locally. CAL raises, ZERO lowers, RESET/ENTER accepts.</li></ol>"
       "<p>Normal CAL starts the ELT span sequence; ZERO starts ELT zero; RESET pulses the sensor reset input. Do not perform calibration in an unknown atmosphere.</p></div>")+pageEnd();
  web.send(200,"text/html",s);
}

void handleApiStatus() {
  String j="{";
  j += "\"firmware\":\""+String(FW_VERSION)+"\",";
  j += "\"sensorOk\":"+String(sensor.ok?"true":"false")+",";
  j += "\"rawPpm\":"+(isfinite(sensor.rawPpm)?String(sensor.rawPpm,3):String("null"))+",";
  j += "\"ppm\":"+(isfinite(sensor.filteredPpm)?String(sensor.filteredPpm,3):String("null"))+",";
  j += "\"lowAlarm\":"+String(sensor.lowAlarm?"true":"false")+",";
  j += "\"highAlarm\":"+String(sensor.highAlarm?"true":"false")+",";
  j += "\"lowSP\":"+String(cfg.lowSP,2)+",\"highSP\":"+String(cfg.highSP,2)+",";
  j += "\"wifiConnected\":"+String(WiFi.status()==WL_CONNECTED?"true":"false")+",";
  j += "\"mqttConnected\":"+String(mqtt.connected()?"true":"false")+",";
  j += "\"goodReads\":"+String(sensor.goodReads)+",\"badReads\":"+String(sensor.badReads)+",";
  j += "\"packet\":["; for(uint8_t i=0;i<7;i++){if(i)j+=",";j+=String(sensor.packet[i]);} j += "]}";
  web.send(200,"application/json",j);
}

void startWeb() {
  web.on("/", HTTP_GET, handleStatus);
  web.on("/wifi", HTTP_ANY, handleWifi);
  web.on("/alarms", HTTP_ANY, handleAlarms);
  web.on("/cal", HTTP_ANY, handleCalibration);
  web.on("/mqtt", HTTP_ANY, handleMqtt);
  web.on("/tags", HTTP_ANY, handleTags);
  web.on("/instructions", HTTP_GET, handleInstructions);
  web.on("/api/status", HTTP_GET, handleApiStatus);
  web.on("/generate_204", HTTP_ANY, redirectHome);
  web.on("/hotspot-detect.html", HTTP_ANY, redirectHome);
  web.on("/connecttest.txt", HTTP_ANY, redirectHome);
  web.onNotFound(redirectHome);
  web.begin();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n%s boot\n", FW_VERSION);

  pinMode(HW::MOS_LOW_ALARM, OUTPUT); pinMode(HW::MOS_HIGH_ALARM, OUTPUT);
  digitalWrite(HW::MOS_LOW_ALARM, LOW); digitalWrite(HW::MOS_HIGH_ALARM, LOW);
  pinMode(HW::SENSOR_SPAN, OUTPUT); pinMode(HW::SENSOR_ZERO, OUTPUT); pinMode(HW::SENSOR_RESET, OUTPUT);
  stopSensorAction();
  pinMode(HW::BTN_CAL, INPUT_PULLUP); pinMode(HW::BTN_ZERO, INPUT_PULLUP); pinMode(HW::BTN_ENTER, INPUT_PULLUP);

  loadConfig();
  Wire.begin(HW::SDA_PIN, HW::SCL_PIN, 100000);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, HW::OLED_ADDR);
  if (oledOK) { display.clearDisplay(); display.display(); }
  Serial.printf("[I2C] OLED=%s, H2S address=0x%02X\n", oledOK?"OK":"NOT FOUND", HW::H2S_ADDR);

  startWifi();
  startWeb();
}

void loop() {
  web.handleClient();
  serviceWifi();
  serviceSensorAction();
  handleButtons();

  uint32_t now=millis();
  if(now-lastSensorMs>=SENSOR_PERIOD_MS){ lastSensorMs=now; sampleSensor(); }
  if(now-lastDisplayMs>=DISPLAY_PERIOD_MS){ lastDisplayMs=now; drawDisplay(); }
  serviceMqtt();
  delay(2);
}
