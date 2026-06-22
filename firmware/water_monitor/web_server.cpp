#include "web_server.h"
#include "config.h"
#include "config_storage.h"
#include "calibration.h"
#include "sensors.h"
#include "period_stats.h"
#include "mqtt_handler.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

extern Config config;

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;

// ============================================================================
// MIME type pro statické soubory
// ============================================================================
static String getContentType(const String& filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css"))  return "text/css";
  if (filename.endsWith(".js"))   return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".ico"))  return "image/x-icon";
  if (filename.endsWith(".svg"))  return "image/svg+xml";
  return "text/plain";
}

// ============================================================================
// Servírování statických souborů z LittleFS
// ============================================================================
static bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

// ============================================================================
// API: GET /api/status
// Vrací aktuální stav (pro live dashboard)
// ============================================================================
static void handleApiStatus() {
  StaticJsonDocument<512> doc;

  doc["fw_version"] = FW_VERSION;
  doc["uptime_s"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_ssid"] = WiFi.SSID();
  doc["wifi_ip"] = WiFi.localIP().toString();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["mqtt_connected"] = mqttIsConnected();

  doc["flow_lpm"] = sensorsGetLastFlow();
  doc["total_volume_l"] = sensorsGetTotalVolume();
  
  // Kalibrační čítač - počet pulzů od posledního resetu (pro určení K-faktoru)
  uint32_t calibPulses = sensorsGetCalibrationPulses();
  doc["calibration_pulses"] = calibPulses;
  // Odvozený objem (kontrola podle aktuálního K-faktoru):
  doc["calibration_volume_l"] = (float)calibPulses / config.pulses_per_liter;
  doc["pulses_per_liter"] = config.pulses_per_liter;

  TankMeasurement tank = sensorsGetLastTank();
  doc["tank_valid"] = tank.valid;
  doc["tank_distance_cm"] = tank.distance_cm;
  doc["tank_level_cm"] = tank.level_cm;
  doc["tank_volume_l"] = tank.volume_l;
  doc["tank_fill_percent"] = tank.fill_percent;

  // Period statistics summary (jen skaláry; histogram je v /api/stats)
  PeriodStats s = statsGet();
  JsonObject stats = doc.createNestedObject("stats");
  stats["today_l"]    = s.today_l;
  stats["week_l"]     = s.week_l;
  stats["month_l"]    = s.month_l;
  stats["year_l"]     = s.year_l;
  stats["time_valid"] = s.time_valid;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// API: GET /api/config
// ============================================================================
static void handleApiGetConfig() {
  StaticJsonDocument<1024> doc;
  
  doc["wifi_ssid"]     = config.wifi_ssid;
  doc["wifi_password"] = "***";  // skryj heslo
  doc["mqtt_server"]   = config.mqtt_server;
  doc["mqtt_port"]     = config.mqtt_port;
  doc["mqtt_user"]     = config.mqtt_user;
  doc["mqtt_password"] = "***";
  doc["mqtt_client_id"] = config.mqtt_client_id;
  doc["device_name"]   = config.device_name;
  doc["device_id"]     = config.device_id;
  doc["tank_distance_full_cm"]  = config.tank_distance_full_cm;
  doc["tank_distance_empty_cm"] = config.tank_distance_empty_cm;
  doc["tank_max_volume_l"]      = config.tank_max_volume_l;
  doc["pulses_per_liter"]       = config.pulses_per_liter;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// API: POST /api/config
// ============================================================================
static void handleApiSetConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  // Aktualizuj pouze pole, která jsou v JSON (nepřepisuj nezadaná)
  if (doc.containsKey("wifi_ssid"))     config.wifi_ssid     = doc["wifi_ssid"].as<String>();
  if (doc.containsKey("wifi_password")) {
    String pwd = doc["wifi_password"].as<String>();
    if (pwd != "***" && pwd != "") config.wifi_password = pwd;
  }
  if (doc.containsKey("mqtt_server"))   config.mqtt_server   = doc["mqtt_server"].as<String>();
  if (doc.containsKey("mqtt_port"))     config.mqtt_port     = doc["mqtt_port"];
  if (doc.containsKey("mqtt_user"))     config.mqtt_user     = doc["mqtt_user"].as<String>();
  if (doc.containsKey("mqtt_password")) {
    String pwd = doc["mqtt_password"].as<String>();
    if (pwd != "***" && pwd != "") config.mqtt_password = pwd;
  }
  if (doc.containsKey("mqtt_client_id")) config.mqtt_client_id = doc["mqtt_client_id"].as<String>();
  if (doc.containsKey("device_name"))    config.device_name    = doc["device_name"].as<String>();
  if (doc.containsKey("device_id"))      config.device_id      = doc["device_id"].as<String>();
  if (doc.containsKey("tank_distance_full_cm"))  config.tank_distance_full_cm  = doc["tank_distance_full_cm"];
  if (doc.containsKey("tank_distance_empty_cm")) config.tank_distance_empty_cm = doc["tank_distance_empty_cm"];
  if (doc.containsKey("tank_max_volume_l"))      config.tank_max_volume_l      = doc["tank_max_volume_l"];
  if (doc.containsKey("pulses_per_liter")) {
    float ppl = doc["pulses_per_liter"];
    if (ppl > 0.0f) config.pulses_per_liter = ppl;  // ochrana proti dělení nulou
  }
  
  if (configSave(config)) {
    server.send(200, "application/json", "{\"status\":\"ok\",\"restart_required\":true}");
  } else {
    server.send(500, "text/plain", "Save failed");
  }
}

// ============================================================================
// API: GET /api/calibration
// ============================================================================
static void handleApiGetCalibration() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("points");
  
  const CalibrationPoint* points = calibrationGetPoints();
  int count = calibrationGetCount();
  
  for (int i = 0; i < count; i++) {
    JsonObject p = arr.createNestedObject();
    p["index"]    = i;
    p["level_cm"] = points[i].level_cm;
    p["volume_l"] = points[i].volume_l;
  }
  
  doc["max_points"] = MAX_CALIBRATION_POINTS;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// API: POST /api/calibration/add  body: {"level_cm": X, "volume_l": Y}
// ============================================================================
static void handleApiAddCalibrationPoint() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  if (!doc.containsKey("level_cm") || !doc.containsKey("volume_l")) {
    server.send(400, "text/plain", "Missing level_cm or volume_l");
    return;
  }
  
  float level = doc["level_cm"];
  float volume = doc["volume_l"];
  
  if (calibrationAddPoint(level, volume)) {
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(500, "text/plain", "Add failed (max points?)");
  }
}

// ============================================================================
// POST /api/calibration/remove  body: {"index": N}
// ============================================================================
static void handleApiRemoveCalibrationPoint() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  int idx = doc["index"];
  if (calibrationRemovePoint(idx)) {
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "text/plain", "Invalid index");
  }
}

// ============================================================================
// POST /api/calibration/reset
// ============================================================================
static void handleApiResetCalibration() {
  calibrationReset();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ============================================================================
// API: GET /api/stats
// Vrátí kompletní period stats vč. histogramů (pro grafy ve web UI)
// ============================================================================
static void handleApiGetStats() {
  // Velký buffer kvůli histogramům (24 + 7 + 31 + 12 = 74 floatů)
  DynamicJsonDocument doc(2048);

  PeriodStats s = statsGet();
  doc["today_l"]    = s.today_l;
  doc["week_l"]     = s.week_l;
  doc["month_l"]    = s.month_l;
  doc["year_l"]     = s.year_l;
  doc["time_valid"] = s.time_valid;

  doc["active_hour"]    = statsGetActiveHour();
  doc["active_weekday"] = statsGetActiveWeekday();
  doc["active_mday"]    = statsGetActiveMday();
  doc["active_month"]   = statsGetActiveMonth();

  // Histogramy (zaokrouhlené na 1 desetinné místo)
  JsonArray ah = doc.createNestedArray("hourly");
  const float* h = statsGetHourly();
  for (int i = 0; i < 24; i++) ah.add(round(h[i] * 10) / 10.0f);

  JsonArray aw = doc.createNestedArray("daily_week");
  const float* dw = statsGetDailyWeek();
  for (int i = 0; i < 7; i++) aw.add(round(dw[i] * 10) / 10.0f);

  JsonArray am = doc.createNestedArray("daily_month");
  const float* dm = statsGetDailyMonth();
  for (int i = 0; i < 31; i++) am.add(round(dm[i] * 10) / 10.0f);

  JsonArray ay = doc.createNestedArray("monthly");
  const float* mo = statsGetMonthly();
  for (int i = 0; i < 12; i++) ay.add(round(mo[i] * 10) / 10.0f);

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// API: POST /api/stats/reset
// Vynuluje všechny period stats akumulátory a histogramy
// ============================================================================
static void handleApiResetStats() {
  statsResetAll();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// POST /api/pulses/reset
// Vynuluje kalibrační čítač pulzů (pro určení K-faktoru reálným měřením).
// NEovlivní celkový perzistovaný objem (sensorsGetTotalVolume).
// ============================================================================
static void handleApiResetCalibrationPulses() {
  sensorsResetCalibrationPulses();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// POST /api/restart
// ============================================================================
static void handleApiRestart() {
  sensorsPersistTotal();  // ulož total před plánovaným restartem (žádná ztráta)
  server.send(200, "application/json", "{\"status\":\"restarting\"}");
  delay(500);
  ESP.restart();
}

// ============================================================================
// GET /api/wifi/scan - vrátí dostupné WiFi sítě
// ============================================================================
static void handleApiWiFiScan() {
  int n = WiFi.scanNetworks();
  
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("networks");
  
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject net = arr.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["secure"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============================================================================
// Záloha / obnova perzistentních souborů z LittleFS
// ----------------------------------------------------------------------------
// Web UI (app.js, index.html, style.css) a runtime konfigurace sdílí jeden
// LittleFS oddíl. Flash filesystemu (uploadfs / OTA pole "filesystem") přepíše
// celý oddíl, takže config.json, calibration.json i statistiky ZMIZÍ.
//
// /api/backup  (GET)  - stáhne všechny perzistentní soubory jako jeden JSON
// /api/restore (POST) - zapíše je zpět (typicky po flashi FS, nebo zapéci do
//                       data/ před buildem image, aby zařízení zůstalo online)
// ============================================================================
struct BackupFile {
  const char* key;
  const char* path;
};

static const BackupFile BACKUP_FILES[] = {
  {"config",      CONFIG_FILE},
  {"calibration", CALIBRATION_FILE},
  {"stats",       STATS_FILE},
  {"history",     HISTORY_FILE},
};
static const size_t BACKUP_FILE_COUNT = sizeof(BACKUP_FILES) / sizeof(BACKUP_FILES[0]);

static String readFileRaw(const char* path) {
  if (!LittleFS.exists(path)) return String();
  File f = LittleFS.open(path, "r");
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

// GET /api/backup - vrátí {"config":"<raw>", "calibration":"<raw>", ...}
// Pozn.: obsahuje WiFi/MQTT hesla v plaintextu (na rozdíl od /api/config).
static void handleApiBackup() {
  DynamicJsonDocument doc(8192);
  for (size_t i = 0; i < BACKUP_FILE_COUNT; i++) {
    String raw = readFileRaw(BACKUP_FILES[i].path);
    if (raw.length() > 0) {
      doc[BACKUP_FILES[i].key] = raw;  // raw JSON text jako string (zachová formát)
    }
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// POST /api/restore - tělo = výstup z /api/backup; zapíše soubory zpět do LittleFS
static void handleApiRestore() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  int restored = 0;
  for (size_t i = 0; i < BACKUP_FILE_COUNT; i++) {
    if (!doc.containsKey(BACKUP_FILES[i].key)) continue;
    String raw = doc[BACKUP_FILES[i].key].as<String>();
    if (raw.length() == 0) continue;

    File f = LittleFS.open(BACKUP_FILES[i].path, "w");
    if (!f) continue;
    f.print(raw);
    f.close();
    restored++;
  }

  if (restored > 0) {
    StaticJsonDocument<96> resp;
    resp["status"] = "ok";
    resp["restored"] = restored;
    resp["restart_required"] = true;
    String out;
    serializeJson(resp, out);
    server.send(200, "application/json", out);
  } else {
    server.send(400, "text/plain", "Nothing to restore");
  }
}

// ============================================================================
void webServerStart() {
  // API endpoints
  server.on("/api/status",       HTTP_GET,  handleApiStatus);
  server.on("/api/config",       HTTP_GET,  handleApiGetConfig);
  server.on("/api/config",       HTTP_POST, handleApiSetConfig);
  server.on("/api/calibration",  HTTP_GET,  handleApiGetCalibration);
  server.on("/api/calibration/add",    HTTP_POST, handleApiAddCalibrationPoint);
  server.on("/api/calibration/remove", HTTP_POST, handleApiRemoveCalibrationPoint);
  server.on("/api/calibration/reset",  HTTP_POST, handleApiResetCalibration);
  server.on("/api/stats",        HTTP_GET,  handleApiGetStats);
  server.on("/api/stats/reset",  HTTP_POST, handleApiResetStats);
  server.on("/api/pulses/reset", HTTP_POST, handleApiResetCalibrationPulses);
  server.on("/api/backup",       HTTP_GET,  handleApiBackup);
  server.on("/api/restore",      HTTP_POST, handleApiRestore);
  server.on("/api/restart",      HTTP_POST, handleApiRestart);
  server.on("/api/wifi/scan",    HTTP_GET,  handleApiWiFiScan);
  
  // OTA update na /update
  httpUpdater.setup(&server, "/update");

  // Ulož total na začátku OTA přenosu (před zápisem hlavní části firmwaru).
  // onProgress se volá opakovaně; configPersistTotalPulses zapíše jen při
  // změně, takže opakovaná volání flash neopotřebovávají.
  Update.onProgress([](size_t cur, size_t /*total*/) {
    if (cur <= HTTP_UPLOAD_BUFLEN) {   // jen první chunk přenosu
      sensorsPersistTotal();
    }
  });
  
  // Statické soubory z LittleFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "Not found");
    }
  });
  
  server.begin();
}

void webServerHandle() {
  server.handleClient();
}
