#include "mqtt_handler.h"
#include "config.h"
#include "config_storage.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

extern Config config;

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

// MQTT topics (sestavené z device_id)
static String topicFlow;
static String topicVolMin;
static String topicVolHour;
static String topicVolTotal;
static String topicDistance;
static String topicLevel;
static String topicVolume;
static String topicPercent;
static String topicStatus;
// Period statistics
static String topicConsumptionToday;
static String topicConsumptionWeek;
static String topicConsumptionMonth;
static String topicConsumptionYear;

static unsigned long lastReconnectAttempt = 0;

// ============================================================================
// Sestavení MQTT topics
// ============================================================================
static void buildTopics() {
  String base = config.device_id;
  topicFlow     = base + "/flowmeter/flow_lpm";
  topicVolMin   = base + "/flowmeter/volume_minute";
  topicVolHour  = base + "/flowmeter/volume_hour";
  topicVolTotal = base + "/flowmeter/volume_total";
  topicDistance = base + "/tank/distance_cm";
  topicLevel    = base + "/tank/level_cm";
  topicVolume   = base + "/tank/volume_liters";
  topicPercent  = base + "/tank/fill_percent";
  topicStatus   = base + "/status";
  // Period stats - skupina "consumption" (spotřeba)
  topicConsumptionToday = base + "/consumption/today";
  topicConsumptionWeek  = base + "/consumption/week";
  topicConsumptionMonth = base + "/consumption/month";
  topicConsumptionYear  = base + "/consumption/year";
}

// ============================================================================
// Pomocná publikační funkce
// ============================================================================
static void publishFloat(const String& topic, float value, uint8_t decimals = 3) {
  if (!mqttClient.connected()) return;
  
  char buf[16];
  dtostrf(value, 0, decimals, buf);
  mqttClient.publish(topic.c_str(), buf);
}

// ============================================================================
// HA Auto-discovery
// ============================================================================
static void publishHADiscoverySensor(
    const char* objectId,
    const char* friendlyName,
    const String& stateTopic,
    const char* unit,
    const char* deviceClass,
    const char* stateClass,
    const char* icon,
    int precision
) {
  StaticJsonDocument<512> doc;
  
  doc["name"] = friendlyName;
  doc["unique_id"] = config.device_id + "_" + objectId;
  doc["state_topic"] = stateTopic;
  doc["unit_of_measurement"] = unit;
  if (strlen(deviceClass) > 0) doc["device_class"] = deviceClass;
  if (strlen(stateClass) > 0) doc["state_class"] = stateClass;
  if (strlen(icon) > 0) doc["icon"] = icon;
  doc["suggested_display_precision"] = precision;
  doc["availability_topic"] = topicStatus;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = config.device_id;
  device["name"] = config.device_name;
  device["model"] = "ESP8266 + DN32 + JSN-SR04T";
  device["manufacturer"] = "DIY";
  device["sw_version"] = FW_VERSION;
  
  String configTopic = String("homeassistant/sensor/") + config.device_id + "_" + objectId + "/config";
  
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  mqttClient.publish(configTopic.c_str(), buffer, true);  // retained
  
  Serial.printf("HA discovery: %s (%d bytes)\n", objectId, n);
}

static void publishHomeAssistantDiscovery() {
  Serial.println("Publikuji HA discovery...");
  
  // ---- Live data ----
  publishHADiscoverySensor("flow_lpm",      "Průtok aktuální",  topicFlow,
                           "L/min", "", "measurement", "mdi:water-pump", 2);
  publishHADiscoverySensor("volume_minute", "Objem za minutu",  topicVolMin,
                           "L", "water", "total_increasing", "mdi:water", 3);
  publishHADiscoverySensor("volume_hour",   "Objem za hodinu",  topicVolHour,
                           "L", "water", "total_increasing", "mdi:water", 3);
  publishHADiscoverySensor("volume_total",  "Objem celkem",     topicVolTotal,
                           "L", "water", "total_increasing", "mdi:counter", 3);

  // ---- Tank ----
  publishHADiscoverySensor("distance_cm",   "Vzdálenost senzor", topicDistance,
                           "cm", "distance", "measurement", "mdi:arrow-up-down", 1);
  publishHADiscoverySensor("level_cm",      "Hladina v nádrži", topicLevel,
                           "cm", "distance", "measurement", "mdi:waves-arrow-up", 1);
  publishHADiscoverySensor("volume_liters", "Objem v nádrži",   topicVolume,
                           "L", "water", "measurement", "mdi:cup-water", 0);
  publishHADiscoverySensor("fill_percent",  "Naplnění nádrže",  topicPercent,
                           "%", "", "measurement", "mdi:gauge", 1);

  // ---- Period statistics (spotřeba) ----
  // Pozn.: NEpoužíváme device_class "water" + state_class "total_increasing",
  // protože hodnoty se reseují (today/week/month/year) - to by HA energy dashboard
  // matlo. Místo toho state_class "measurement", které je správně pro
  // reseující se akumulátory za období.
  publishHADiscoverySensor("consumption_today", "Spotřeba dnes",      topicConsumptionToday,
                           "L", "", "measurement", "mdi:calendar-today", 1);
  publishHADiscoverySensor("consumption_week",  "Spotřeba tento týden", topicConsumptionWeek,
                           "L", "", "measurement", "mdi:calendar-week",  1);
  publishHADiscoverySensor("consumption_month", "Spotřeba tento měsíc", topicConsumptionMonth,
                           "L", "", "measurement", "mdi:calendar-month", 1);
  publishHADiscoverySensor("consumption_year",  "Spotřeba tento rok",  topicConsumptionYear,
                           "L", "", "measurement", "mdi:calendar",      1);

  Serial.println("HA discovery hotovo.");
}

// ============================================================================
// Reconnect
// ============================================================================
static bool tryConnect() {
  Serial.print("MQTT připojuji... ");
  
  bool connected = mqttClient.connect(
    config.mqtt_client_id.c_str(),
    config.mqtt_user.c_str(),
    config.mqtt_password.c_str(),
    topicStatus.c_str(),
    1, true, "offline"
  );
  
  if (connected) {
    Serial.println("OK");
    mqttClient.publish(topicStatus.c_str(), "online", true);
    publishHomeAssistantDiscovery();
    return true;
  } else {
    Serial.printf("selhalo, rc=%d\n", mqttClient.state());
    return false;
  }
}

// ============================================================================
// Public API
// ============================================================================
void mqttInit() {
  buildTopics();
  mqttClient.setServer(config.mqtt_server.c_str(), config.mqtt_port);
  mqttClient.setBufferSize(1024);  // Větší buffer pro HA discovery
}

void mqttLoop() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      tryConnect();
    }
  } else {
    mqttClient.loop();
  }
}

void mqttPublishFlow(float lpm) {
  publishFloat(topicFlow, lpm, 2);
}

void mqttPublishMinuteVolumes(float volume_minute, float volume_total) {
  publishFloat(topicVolMin, volume_minute, 3);
  publishFloat(topicVolTotal, volume_total, 3);
}

void mqttPublishHourVolume(float volume_hour) {
  publishFloat(topicVolHour, volume_hour, 3);
}

void mqttPublishTank(const TankMeasurement& tank) {
  if (!tank.valid) return;
  publishFloat(topicDistance, tank.distance_cm, 1);
  publishFloat(topicLevel, tank.level_cm, 1);
  publishFloat(topicVolume, tank.volume_l, 0);
  publishFloat(topicPercent, tank.fill_percent, 1);
}

void mqttPublishPeriodStats(const PeriodStats& s) {
  if (!mqttClient.connected()) return;
  publishFloat(topicConsumptionToday, s.today_l, 1);
  publishFloat(topicConsumptionWeek,  s.week_l,  1);
  publishFloat(topicConsumptionMonth, s.month_l, 1);
  publishFloat(topicConsumptionYear,  s.year_l,  1);
}

bool mqttIsConnected() {
  return mqttClient.connected();
}
