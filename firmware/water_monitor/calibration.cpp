#include "calibration.h"
#include "config.h"
#include "config_storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

extern Config config;

// Statické pole bodů (max 30)
static CalibrationPoint points[MAX_CALIBRATION_POINTS];
static int pointsCount = 0;

// Pomocná funkce - seřadí body podle level_cm
static void sortPoints() {
  for (int i = 0; i < pointsCount - 1; i++) {
    for (int j = i + 1; j < pointsCount; j++) {
      if (points[i].level_cm > points[j].level_cm) {
        CalibrationPoint tmp = points[i];
        points[i] = points[j];
        points[j] = tmp;
      }
    }
  }
}

bool calibrationLoad() {
  if (!LittleFS.exists(CALIBRATION_FILE)) {
    Serial.println("Kalibrace neexistuje, vytvářím default");
    calibrationReset();
    return true;
  }
  
  File f = LittleFS.open(CALIBRATION_FILE, "r");
  if (!f) {
    Serial.println("CHYBA: nelze otevřít calibration.json");
    calibrationReset();
    return false;
  }
  
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if (err) {
    Serial.printf("CHYBA parsování calibration.json: %s\n", err.c_str());
    calibrationReset();
    return false;
  }
  
  JsonArray arr = doc["points"];
  pointsCount = 0;
  for (JsonObject p : arr) {
    if (pointsCount >= MAX_CALIBRATION_POINTS) break;
    points[pointsCount].level_cm = p["level_cm"];
    points[pointsCount].volume_l = p["volume_l"];
    pointsCount++;
  }
  
  sortPoints();
  return true;
}

bool calibrationSave() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("points");
  
  for (int i = 0; i < pointsCount; i++) {
    JsonObject p = arr.createNestedObject();
    p["level_cm"] = points[i].level_cm;
    p["volume_l"] = points[i].volume_l;
  }
  
  File f = LittleFS.open(CALIBRATION_FILE, "w");
  if (!f) {
    Serial.println("CHYBA: nelze zapisovat calibration.json");
    return false;
  }
  
  size_t written = serializeJson(doc, f);
  f.close();
  
  Serial.printf("Kalibrace uložena (%d bodů, %d bajtů)\n", pointsCount, written);
  return written > 0;
}

int calibrationGetCount() {
  return pointsCount;
}

const CalibrationPoint* calibrationGetPoints() {
  return points;
}

bool calibrationAddPoint(float level_cm, float volume_l) {
  if (pointsCount >= MAX_CALIBRATION_POINTS) {
    Serial.println("CHYBA: max počet bodů");
    return false;
  }
  
  // Pokud bod se stejnou hladinou (±0.5cm) existuje, aktualizuj objem
  for (int i = 0; i < pointsCount; i++) {
    if (abs(points[i].level_cm - level_cm) < 0.5f) {
      points[i].volume_l = volume_l;
      sortPoints();
      calibrationSave();
      return true;
    }
  }
  
  points[pointsCount].level_cm = level_cm;
  points[pointsCount].volume_l = volume_l;
  pointsCount++;
  
  sortPoints();
  calibrationSave();
  return true;
}

bool calibrationRemovePoint(int index) {
  if (index < 0 || index >= pointsCount) return false;
  
  for (int i = index; i < pointsCount - 1; i++) {
    points[i] = points[i + 1];
  }
  pointsCount--;
  
  calibrationSave();
  return true;
}

void calibrationReset() {
  // Default: lineární 3 body podle aktuální konfigurace nádrže
  float maxLevel = config.tank_distance_empty_cm - config.tank_distance_full_cm;
  
  pointsCount = 3;
  points[0] = { 0.0f,            0.0f };
  points[1] = { maxLevel / 2.0f, config.tank_max_volume_l / 2.0f };
  points[2] = { maxLevel,        config.tank_max_volume_l };
  
  calibrationSave();
}

float calibrationLevelToVolume(float level_cm) {
  if (pointsCount == 0) return 0.0f;
  if (pointsCount == 1) return points[0].volume_l;
  
  // Pod minimum
  if (level_cm <= points[0].level_cm) return points[0].volume_l;
  
  // Nad maximum
  if (level_cm >= points[pointsCount - 1].level_cm) {
    return points[pointsCount - 1].volume_l;
  }
  
  // Interpolace mezi dvěma sousedními body
  for (int i = 0; i < pointsCount - 1; i++) {
    if (level_cm >= points[i].level_cm && level_cm <= points[i + 1].level_cm) {
      float ratio = (level_cm - points[i].level_cm) / 
                    (points[i + 1].level_cm - points[i].level_cm);
      return points[i].volume_l + 
             ratio * (points[i + 1].volume_l - points[i].volume_l);
    }
  }
  
  return 0.0f;
}
