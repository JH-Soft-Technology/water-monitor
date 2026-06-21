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

// ----------------------------------------------------------------------------
// Kalibrační čítač pulzů (od posledního resetu)
// ----------------------------------------------------------------------------
// Slouží pro určení K-faktoru senzoru reálným měřením:
//   1) Resetuj čítač přes /api/pulses/reset
//   2) Odčerpej známé množství vody (např. 10 L do kalibrované konve)
//   3) Přečti počet pulzů z /api/status nebo Kalibrace v UI
//   4) Reálný K-faktor [Hz na L/min] = pulses / (10L * 60s)
// Čítač NENÍ persistován (resetuje se i při restartu ESP).
uint32_t sensorsGetCalibrationPulses();
void     sensorsResetCalibrationPulses();


#endif // SENSORS_H
