#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include "config.h"   // DEFAULT_PULSES_PER_LITER

// Struktura uchovávající všechny perzistentní nastavení
struct Config {
  // WiFi STA
  String wifi_ssid = "";
  String wifi_password = "";
  
  // MQTT
  String mqtt_server = "";
  uint16_t mqtt_port = 1883;
  String mqtt_user = "";
  String mqtt_password = "";
  String mqtt_client_id = "watertank_esp01";
  
  // Device
  String device_name = "Water Tank";
  String device_id = "watertank";
  
  // Tank geometry
  float tank_distance_full_cm = 40.0f;
  float tank_distance_empty_cm = 213.0f;
  float tank_max_volume_l = 5300.0f;

  // Průtokoměr - K-faktor (pulzů na litr), kalibrovatelný za běhu přes web UI
  float pulses_per_liter = DEFAULT_PULSES_PER_LITER;
  
  // Volume_total perzistence (zachová stav přes restart)
  uint32_t pulse_count_total_persisted = 0;
};

// Načte konfiguraci z LittleFS. Pokud soubor neexistuje, použije defaults.
bool configLoad(Config& cfg);

// Uloží konfiguraci do LittleFS
bool configSave(const Config& cfg);

// Reset konfigurace na defaultní hodnoty
void configReset(Config& cfg);

// Periodické ukládání volume_total (jednou za hodinu) - aby přežil restart
void configPersistTotalPulses(uint32_t pulses);

#endif // CONFIG_STORAGE_H
