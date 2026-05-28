#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

#define MAX_CALIBRATION_POINTS 30

struct CalibrationPoint {
  float level_cm;
  float volume_l;
};

// Načte kalibraci z LittleFS. Pokud neexistuje, vytvoří default (3 body lineární)
bool calibrationLoad();

// Uloží aktuální kalibraci do LittleFS
bool calibrationSave();

// Vrátí počet bodů
int calibrationGetCount();

// Vrátí ukazatel na pole bodů (jen pro čtení)
const CalibrationPoint* calibrationGetPoints();

// Přidá bod (seřadí se automaticky)
bool calibrationAddPoint(float level_cm, float volume_l);

// Smaže bod podle indexu
bool calibrationRemovePoint(int index);

// Smaže všechny body a vytvoří default lineární
void calibrationReset();

// Interpolace: hladina [cm] → objem [l]
float calibrationLevelToVolume(float level_cm);

#endif // CALIBRATION_H
