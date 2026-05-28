#include "config_storage.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

extern Config config;  // z hlavního .ino

bool configLoad(Config& cfg) {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("Konfigurace neexistuje, používám defaults");
    return false;
  }
  
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println("CHYBA: nelze otevřít config.json");
    return false;
  }
  
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if (err) {
    Serial.printf("CHYBA parsování config.json: %s\n", err.c_str());
    return false;
  }
  
  cfg.wifi_ssid       = doc["wifi_ssid"]     | "";
  cfg.wifi_password   = doc["wifi_password"] | "";
  cfg.mqtt_server     = doc["mqtt_server"]   | "";
  cfg.mqtt_port       = doc["mqtt_port"]     | 1883;
  cfg.mqtt_user       = doc["mqtt_user"]     | "";
  cfg.mqtt_password   = doc["mqtt_password"] | "";
  cfg.mqtt_client_id  = doc["mqtt_client_id"] | "watertank_esp01";
  cfg.device_name     = doc["device_name"]   | "Water Tank";
  cfg.device_id       = doc["device_id"]     | "watertank";
  cfg.tank_distance_full_cm  = doc["tank_distance_full_cm"]  | 40.0f;
  cfg.tank_distance_empty_cm = doc["tank_distance_empty_cm"] | 213.0f;
  cfg.tank_max_volume_l      = doc["tank_max_volume_l"]      | 5300.0f;
  cfg.pulse_count_total_persisted = doc["pulse_count_total_persisted"] | 0;
  
  return true;
}

bool configSave(const Config& cfg) {
  StaticJsonDocument<1024> doc;
  
  doc["wifi_ssid"]     = cfg.wifi_ssid;
  doc["wifi_password"] = cfg.wifi_password;
  doc["mqtt_server"]   = cfg.mqtt_server;
  doc["mqtt_port"]     = cfg.mqtt_port;
  doc["mqtt_user"]     = cfg.mqtt_user;
  doc["mqtt_password"] = cfg.mqtt_password;
  doc["mqtt_client_id"] = cfg.mqtt_client_id;
  doc["device_name"]   = cfg.device_name;
  doc["device_id"]     = cfg.device_id;
  doc["tank_distance_full_cm"]  = cfg.tank_distance_full_cm;
  doc["tank_distance_empty_cm"] = cfg.tank_distance_empty_cm;
  doc["tank_max_volume_l"]      = cfg.tank_max_volume_l;
  doc["pulse_count_total_persisted"] = cfg.pulse_count_total_persisted;
  
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial.println("CHYBA: nelze zapisovat config.json");
    return false;
  }
  
  size_t written = serializeJson(doc, f);
  f.close();
  
  if (written == 0) {
    Serial.println("CHYBA: zápis 0 bajtů");
    return false;
  }
  
  Serial.printf("Konfigurace uložena (%d bajtů)\n", written);
  return true;
}

void configReset(Config& cfg) {
  cfg = Config();  // resetuj na defaults z konstruktoru
  configSave(cfg);
}

void configPersistTotalPulses(uint32_t pulses) {
  // Volá se jednou za hodinu z hlavního loop
  if (config.pulse_count_total_persisted != pulses) {
    config.pulse_count_total_persisted = pulses;
    configSave(config);
  }
}
