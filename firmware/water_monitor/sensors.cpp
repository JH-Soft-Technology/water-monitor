#include "sensors.h"
#include "config.h"
#include "config_storage.h"
#include "calibration.h"
#include <NewPing.h>

extern Config config;

// Pulzní čítače - volatile kvůli ISR
static volatile uint32_t pulseCount = 0;          // reset každou 1 s (debug)
static volatile uint32_t pulseCountMinute = 0;
static volatile uint32_t pulseCountHour = 0;
static volatile uint32_t pulseCountTotal = 0;
static volatile uint32_t pulseCountCalibration = 0;  // kalibrační čítač, reset přes API/UI nebo restart
static volatile uint32_t lastPulseMicros = 0;

// Diagnostický čítač: všechna raw přerušení vč. těch odfiltrovaných debounce
static volatile uint32_t rawInterruptCount = 0;

// Kruhový buffer časových razítek posledních N platných pulzů.
// Průtok se počítá z průměrné periody → funguje i při 1 pulzu za několik sekund.
static volatile uint32_t pulseTimes[FLOW_PERIOD_SAMPLES];
static volatile uint8_t  pulseTimesWr  = 0;   // index dalšího zápisu
static volatile uint8_t  pulseTimesN   = 0;   // počet platných záznamů (0..FLOW_PERIOD_SAMPLES)

// NewPing instance pro ultrazvuk
static NewPing* sonar = nullptr;

// Poslední změřené hodnoty (pro web UI)
static float lastFlowLpm = 0.0f;
static TankMeasurement lastTank = {0, 0, 0, 0, false};

// ============================================================================
// ISR - detekce pulzu z optočlenu
// ============================================================================
static void IRAM_ATTR onPulse() {
  rawInterruptCount++;

  uint32_t now = micros();
  if (now - lastPulseMicros < MIN_PULSE_INTERVAL_US) {
    return;  // Debouncing
  }

  // Pokud byl průtok přerušen déle než FLOW_STALE_US, stará razítka
  // v bufferu jsou neplatná — resetujeme, aby se nepromíchala s novými.
  if (pulseTimesN > 0 && (now - lastPulseMicros) > FLOW_STALE_US) {
    pulseTimesWr = 0;
    pulseTimesN  = 0;
  }

  lastPulseMicros = now;

  pulseTimes[pulseTimesWr] = now;
  pulseTimesWr = (pulseTimesWr + 1) % FLOW_PERIOD_SAMPLES;
  if (pulseTimesN < FLOW_PERIOD_SAMPLES) pulseTimesN++;

  pulseCount++;
  pulseCountMinute++;
  pulseCountHour++;
  pulseCountTotal++;
  pulseCountCalibration++;
}

// ============================================================================
void sensorsInit() {
  // Průtokoměr - vstup s pull-up (PC817 invertuje signál)
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onPulse, FALLING);
  
  // Načti perzistovaný celkový čítač (přežije restart)
  pulseCountTotal = config.pulse_count_total_persisted;
  
  // Ultrazvuk
  sonar = new NewPing(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE_CM);
  
  Serial.printf("Senzory init: total pulses = %u (= %.3f L)\n",
                pulseCountTotal, pulseCountTotal / PULSES_PER_LITER);
}

// ============================================================================
float sensorsCalculateFlow(unsigned long /*elapsed_ms*/) {
  // Zkopíruj volatile data s vypnutými přerušeními
  noInterrupts();
  uint32_t pulses  = pulseCount;
  pulseCount = 0;
  uint32_t rawISR  = rawInterruptCount;
  rawInterruptCount = 0;
  uint8_t  n       = pulseTimesN;
  uint8_t  wr      = pulseTimesWr;
  uint32_t ts[FLOW_PERIOD_SAMPLES];
  for (uint8_t i = 0; i < FLOW_PERIOD_SAMPLES; i++) ts[i] = pulseTimes[i];
  uint32_t lastTs  = lastPulseMicros;
  interrupts();

  if (rawISR > 0 || pulses > 0) {
    Serial.printf("[FLOW] raw ISR: %u  filtrované pulzy: %u\n", rawISR, pulses);
  }

  // Bez pulzu déle než FLOW_STALE_US → průtok = 0
  if (n == 0 || (micros() - lastTs) > FLOW_STALE_US) {
    lastFlowLpm = 0.0f;
    return 0.0f;
  }

  // Jen 1 pulz — odhadni průtok z doby uplynulé od jeho přijetí.
  // Odhad je horní hranicí (klesá čím déle druhý pulz nepřichází).
  if (n < 2) {
    uint32_t elapsed = micros() - lastTs;
    if (elapsed >= 200000UL) {  // počkej aspoň 200 ms
      lastFlowLpm = (1000000.0f / (float)elapsed) / K_FACTOR;
      Serial.printf("[FLOW] 1 pulz, elapsed %lu µs → odhad %.3f L/min\n",
                    elapsed, lastFlowLpm);
    }
    return lastFlowLpm;
  }

  // Nejstarší záznam v bufferu: (wr - n) mod N
  // Nejnovější záznam:          (wr - 1) mod N
  uint8_t  idxOld  = (uint8_t)((wr + FLOW_PERIOD_SAMPLES - n) % FLOW_PERIOD_SAMPLES);
  uint8_t  idxNew  = (uint8_t)((wr + FLOW_PERIOD_SAMPLES - 1) % FLOW_PERIOD_SAMPLES);
  uint32_t span    = ts[idxNew] - ts[idxOld];  // µs; správně i po přetečení uint32

  if (span == 0) return lastFlowLpm;

  // průměrná frekvence = (n-1) intervalů / celkový čas v sekundách
  float freqHz   = (float)(n - 1) * 1000000.0f / (float)span;
  lastFlowLpm    = freqHz / K_FACTOR;

  Serial.printf("[FLOW] %u pulzů, span %lu µs → %.3f Hz → %.3f L/min\n",
                n, span, freqHz, lastFlowLpm);

  return lastFlowLpm;
}

// ============================================================================
void sensorsGetMinuteVolume(float& volume_minute_l, float& volume_total_l) {
  noInterrupts();
  uint32_t pulsesMin = pulseCountMinute;
  pulseCountMinute = 0;
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();
  
  volume_minute_l = (float)pulsesMin / PULSES_PER_LITER;
  volume_total_l  = (float)pulsesTotal / PULSES_PER_LITER;
}

// ============================================================================
float sensorsGetHourVolume() {
  noInterrupts();
  uint32_t pulsesHour = pulseCountHour;
  pulseCountHour = 0;
  interrupts();

  return (float)pulsesHour / PULSES_PER_LITER;
}

// ============================================================================
// Uloží aktuální total do flash. Perzistuje se jen pokud se hodnota změnila
// (configPersistTotalPulses má vlastní ochranu), takže opakované volání
// nezbytečně neopotřebovává flash.
void sensorsPersistTotal() {
  noInterrupts();
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();

  configPersistTotalPulses(pulsesTotal);
}

// ============================================================================
float sensorsGetTotalVolume() {
  noInterrupts();
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();
  
  return (float)pulsesTotal / PULSES_PER_LITER;
}

// ============================================================================
bool sensorsMeasureTank(TankMeasurement& result) {
  result.valid = false;
  
  if (!sonar) return false;
  
  float samples[MEDIAN_SAMPLES];
  uint8_t validCount = 0;
  
  // Rychlost zvuku: 331.4 + 0.6 * T[°C]  [m/s]
  float soundSpeed = 331.4f + 0.6f * SOUND_SPEED_TEMP_C;
  
  for (uint8_t i = 0; i < MEDIAN_SAMPLES; i++) {
    unsigned int us = sonar->ping_median(3);
    if (us > 0) {
      // us → cm:  distance = (čas / 2) * rychlost zvuku v cm/µs
      float distance = (us / 2.0f) * (soundSpeed / 10000.0f);
      samples[validCount++] = distance;
    }
    delay(50);
  }
  
  if (validCount == 0) {
    Serial.println("Tank: žádné platné měření!");
    return false;
  }
  
  // Medián
  for (uint8_t i = 0; i < validCount - 1; i++) {
    for (uint8_t j = i + 1; j < validCount; j++) {
      if (samples[i] > samples[j]) {
        float tmp = samples[i];
        samples[i] = samples[j];
        samples[j] = tmp;
      }
    }
  }
  float distance = samples[validCount / 2];
  
  result.distance_cm = distance;
  result.level_cm = config.tank_distance_empty_cm - distance;
  if (result.level_cm < 0) result.level_cm = 0;
  
  float maxLevel = config.tank_distance_empty_cm - config.tank_distance_full_cm;
  result.fill_percent = (result.level_cm / maxLevel) * 100.0f;
  if (result.fill_percent < 0) result.fill_percent = 0;
  if (result.fill_percent > 100) result.fill_percent = 100;
  
  result.volume_l = calibrationLevelToVolume(result.level_cm);
  result.valid = true;
  
  lastTank = result;
  return true;
}

// ============================================================================
float sensorsGetLastFlow() {
  return lastFlowLpm;
}

TankMeasurement sensorsGetLastTank() {
  return lastTank;
}

// ============================================================================
// Kalibrační čítač pulzů (pro určení K-faktoru reálným měřením)
// ============================================================================
uint32_t sensorsGetCalibrationPulses() {
  noInterrupts();
  uint32_t v = pulseCountCalibration;
  interrupts();
  return v;
}

void sensorsResetCalibrationPulses() {
  noInterrupts();
  pulseCountCalibration = 0;
  interrupts();
}
