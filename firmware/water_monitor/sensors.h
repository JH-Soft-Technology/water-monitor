#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

struct TankMeasurement {
  float distance_cm;     // raw vzdálenost senzor → hladina
  float level_cm;        // hladina od dna
  float volume_l;        // objem v litrech (z kalibrace)
  float fill_percent;    // % naplnění
  bool valid;
};

// Inicializace pinů, interrupt, NewPing
void sensorsInit();

// Spočte aktuální průtok za uplynulé období (volá se každou 1s)
float sensorsCalculateFlow(unsigned long elapsed_ms);

// Vrátí akumulovaný objem za minutu + celkový (volá se každou minutu)
void sensorsGetMinuteVolume(float& volume_minute_l, float& volume_total_l);

// Vrátí akumulovaný objem za hodinu (volá se každou hodinu)
float sensorsGetHourVolume();

// Vrátí kumulativní celkový objem od startu/persistence
float sensorsGetTotalVolume();

// Uloží aktuální celkový čítač pulzů do flash (volá se periodicky a před restartem/OTA)
void sensorsPersistTotal();

// Změří hladinu v nádrži (medián, teplotní korekce)
bool sensorsMeasureTank(TankMeasurement& result);

// Vrátí poslední změřené hodnoty (pro webové UI)
float sensorsGetLastFlow();
TankMeasurement sensorsGetLastTank();


#endif // SENSORS_H
