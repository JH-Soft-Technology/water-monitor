#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include "sensors.h"
#include "period_stats.h"

// Inicializace MQTT klienta (po WiFi STA připojení)
void mqttInit();

// Volat z hlavního loop pro udržení připojení
void mqttLoop();

// Publikační funkce
void mqttPublishFlow(float lpm);
void mqttPublishMinuteVolumes(float volume_minute, float volume_total);
void mqttPublishHourVolume(float volume_hour);
void mqttPublishTank(const TankMeasurement& tank);

// Period statistics publishing (volá se po každém přechodu nebo periodicky)
void mqttPublishPeriodStats(const PeriodStats& stats);

// Stavová info
bool mqttIsConnected();

#endif // MQTT_HANDLER_H
