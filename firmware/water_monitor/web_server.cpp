#include "web_server.h"
#include "config.h"
#include "config_storage.h"
#include "calibration.h"
#include "sensors.h"
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
  
  TankMeasurement tank = sensorsGetLastTank();
  doc["tank_valid"] = tank.valid;
  doc["tank_distance_cm"] = tank.distance_cm;
  doc["tank_level_cm"] = tank.level_cm;
  doc["tank_volume_l"] = tank.volume_l;
  doc["tank_fill_percent"] = tank.fill_percent;
  
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
// POST /api/restart
// ============================================================================
static void handleApiRestart() {
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
void webServerStart() {
  // API endpoints
  server.on("/api/status",       HTTP_GET,  handleApiStatus);
  server.on("/api/config",       HTTP_GET,  handleApiGetConfig);
  server.on("/api/config",       HTTP_POST, handleApiSetConfig);
  server.on("/api/calibration",  HTTP_GET,  handleApiGetCalibration);
  server.on("/api/calibration/add",    HTTP_POST, handleApiAddCalibrationPoint);
  server.on("/api/calibration/remove", HTTP_POST, handleApiRemoveCalibrationPoint);
  server.on("/api/calibration/reset",  HTTP_POST, handleApiResetCalibration);
  server.on("/api/restart",      HTTP_POST, handleApiRestart);
  server.on("/api/wifi/scan",    HTTP_GET,  handleApiWiFiScan);
  
  // OTA update na /update
  httpUpdater.setup(&server, "/update");
  
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
