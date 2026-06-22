/*
 * ============================================================================
 * Water Monitor v2.4 - Wemos D1 Mini
 * ============================================================================
 *
 * Kompletní řešení monitoringu vody:
 *  - Měření průtoku (DN32 + PC817 optočlen)
 *  - Měření hladiny v nádrži (JSN-SR04T)
 *  - Statistika spotřeby za den/týden/měsíc/rok (NTP CET/CEST)
 *  - MQTT publikace + Home Assistant auto-discovery
 *  - Webové UI pro konfiguraci, kalibraci, grafy a OTA update
 *  - Backup/restore konfigurace (/api/backup, /api/restore) - ochrana dat
 *    při flashi filesystemu (jinak by se config.json/kalibrace smazaly)
 *  - Duální režim: STA (běžný provoz) + AP (vždy dostupný setup)
 *  - LittleFS perzistentní úložiště konfigurace a statistik
 * 
 * Hardware (v2.1 12V architektura):
 *  - Wemos D1 Mini (ESP8266) + DC Power Shield (7-24V vstup)
 *  - 12V/1A spínaný zdroj jako jediný napájecí zdroj systému
 *  - Průtokoměr DN32 napájený 12V (charakteristika F = 4.5 * Q)
 *  - Optočlen PC817 (galvanické oddělení signálu na 30m UTP)
 *  - JSN-SR04T ultrazvukový senzor IP67 (napájen 5V lokálně přes mini step-down)
 * 
 * Piny:
 *  - D2 (GPIO4)  - vstup z optočlenu (signál průtokoměru, FALLING)
 *  - D5 (GPIO14) - TRIG ultrazvuku
 *  - D6 (GPIO12) - ECHO ultrazvuku (PŘÍMO bez děliče - signál je 5V)
 *  - Svorka +/- shieldu - 12V napájecí vstup
 * 
 * První spuštění:
 *  1. Nahrát firmware + filesystem (Tools → ESP8266 LittleFS Data Upload)
 *  2. ESP se spustí v duálním režimu STA+AP
 *  3. Najít WiFi "WaterMonitor-Setup", připojit se (bez hesla)
 *  4. Otevřít http://192.168.4.1 a nakonfigurovat WiFi a MQTT
 *  5. Restart, ESP se připojí k tvé domácí WiFi
 *  6. Najdi IP v routeru, otevři, dokonči kalibraci
 * 
 * Knihovny (Arduino IDE → Manage Libraries):
 *  - PubSubClient (Nick O'Leary)
 *  - ArduinoJson (Benoit Blanchon) ≥ 6.x
 *  - NewPing (Tim Eckel)
 *  - ESP8266WiFi, ESP8266WebServer, ESP8266HTTPUpdateServer, LittleFS, time
 *    (součást ESP8266 board package)
 * 
 * Pro LittleFS upload: nainstalovat plugin
 *  https://github.com/earlephilhower/arduino-littlefs-upload
 * ============================================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include "config.h"
#include "config_storage.h"
#include "sensors.h"
#include "calibration.h"
#include "period_stats.h"
#include "mqtt_handler.h"
#include "web_server.h"

// Globální konfigurace (načte se z LittleFS při startu)
Config config;

// Časovače
unsigned long lastFlowCalc = 0;
unsigned long lastMinutePublish = 0;
unsigned long lastHourPublish = 0;
unsigned long lastTankMeasure = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastPersist = 0;
unsigned long lastStatsPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== Water Monitor v2.4 ===");
  
  // 1. Inicializace LittleFS
  if (!LittleFS.begin()) {
    Serial.println("CHYBA: LittleFS nelze připojit, formátuji...");
    LittleFS.format();
    LittleFS.begin();
  }
  Serial.println("LittleFS OK");
  
  // 2. Načtení konfigurace
  configLoad(config);
  Serial.printf("Konfigurace načtena. WiFi SSID: '%s'\n", config.wifi_ssid.c_str());
  
  // 3. Načtení kalibrační tabulky
  calibrationLoad();
  Serial.printf("Kalibrace načtena: %d bodů\n", calibrationGetCount());
  
  // 4. Inicializace senzorů (před WiFi, ať ISR funguje hned)
  sensorsInit();
  Serial.println("Senzory inicializovány");
  
  // 5. Start duálního WiFi režimu (AP + STA)
  WiFi.mode(WIFI_AP_STA);
  
  // AP - vždy dostupný setup režim
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("AP režim: '%s', IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // STA - připojení k domácí WiFi (pokud je nakonfigurováno)
  if (config.wifi_ssid.length() > 0) {
    Serial.printf("Připojuji k WiFi '%s'...\n", config.wifi_ssid.c_str());
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("STA OK, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("STA selhalo - pokračuji jen v AP režimu");
    }
  } else {
    Serial.println("WiFi neni nakonfigurovana - pouze AP rezim");
  }
  
  // 6. Spuštění webserveru (běží vždy)
  webServerStart();
  Serial.println("Web server běží na portu 80");
  
  // 7. Period statistics (NTP) - jen pokud máme STA spojení
  if (WiFi.status() == WL_CONNECTED) {
    statsInit();
  } else {
    Serial.println("Period stats: přeskočeno (nemáme STA)");
  }

  // 8. MQTT - jen pokud máme STA spojení a MQTT je nakonfigurováno
  if (WiFi.status() == WL_CONNECTED && config.mqtt_server.length() > 0) {
    mqttInit();
    Serial.println("MQTT inicializováno");
  }
  
  // Inicializace časovačů
  unsigned long now = millis();
  lastFlowCalc = now;
  lastMinutePublish = now;
  lastHourPublish = now;
  lastTankMeasure = now - TANK_MEASURE_INTERVAL + 5000;  // První měření za 5s
  lastWiFiCheck = now;
  lastPersist = now;
  lastStatsPublish = now;

  Serial.println("=== Setup hotov, monitoring zahájen ===\n");
}

void loop() {
  unsigned long now = millis();
  
  // Web server obsluhuje requests vždy
  webServerHandle();
  
  // Period stats loop (kontroluje NTP rollover)
  statsLoop();

  // Kontrola WiFi STA každých 30 s (reconnect při výpadku)
  if (now - lastWiFiCheck >= 30000) {
    lastWiFiCheck = now;
    if (config.wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi STA odpadla, reconnect...");
      WiFi.reconnect();
    }
  }
  
  // MQTT loop - jen pokud je STA připojené
  if (WiFi.status() == WL_CONNECTED && config.mqtt_server.length() > 0) {
    mqttLoop();
  }
  
  // ---- Každou 1s: aktuální průtok ----
  if (now - lastFlowCalc >= FLOW_CALC_INTERVAL) {
    unsigned long elapsed = now - lastFlowCalc;
    lastFlowCalc = now;
    
    float flowLpm = sensorsCalculateFlow(elapsed);
    mqttPublishFlow(flowLpm);
  }
  
  // ---- Každou minutu: objem minutový + total + přidat do period stats ----
  if (now - lastMinutePublish >= MINUTE_INTERVAL) {
    lastMinutePublish = now;
    
    float volumeMinute, volumeTotal;
    sensorsGetMinuteVolume(volumeMinute, volumeTotal);
    mqttPublishMinuteVolumes(volumeMinute, volumeTotal);

    // Přidat minutový objem do period statistics
    // (distribuuje se do today/week/month/year + histogramů)
    if (volumeMinute > 0.0f) {
      statsAddVolume(volumeMinute);
    }
  }
  
  // ---- Každou hodinu: objem hodinový ----
  if (now - lastHourPublish >= HOUR_INTERVAL) {
    lastHourPublish = now;
    
    float volumeHour = sensorsGetHourVolume();
    mqttPublishHourVolume(volumeHour);
  }

  // ---- Každých PERIOD_STATS_PUBLISH_INTERVAL: publikuj period stats na MQTT ----
  if (now - lastStatsPublish >= PERIOD_STATS_PUBLISH_INTERVAL) {
    lastStatsPublish = now;

    if (WiFi.status() == WL_CONNECTED && config.mqtt_server.length() > 0) {
      PeriodStats s = statsGet();
      mqttPublishPeriodStats(s);
    }
  }

  // ---- Každých 30s: měření hladiny ----
  if (now - lastTankMeasure >= TANK_MEASURE_INTERVAL) {
    lastTankMeasure = now;

    TankMeasurement tank;
    sensorsMeasureTank(tank);   // při neúspěchu nastaví tank.valid = false
    mqttPublishTank(tank);      // publikuje hodnoty, nebo "offline" do tank/status
  }

  // ---- Každých 10 min: perzistence celkového čítače (ochrana proti výpadku) ----
  if (now - lastPersist >= PERSIST_INTERVAL) {
    lastPersist = now;
    sensorsPersistTotal();
  }

  delay(10);
}
