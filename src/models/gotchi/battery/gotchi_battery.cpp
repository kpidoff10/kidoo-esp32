#include "gotchi_battery.h"
#include "../config/config.h"
#include <Wire.h>
#include <Arduino.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

namespace {

XPowersAXP2101 s_pmu;
bool s_available = false;
int8_t s_percent = -1;
bool s_charging = false;
bool s_pluggedIn = false;
uint32_t s_timer = 0;

constexpr uint32_t READ_INTERVAL_MS = 30000;

void readBattery() {
  s_pluggedIn = s_pmu.isVbusIn();
  if (s_pmu.isBatteryConnect()) {
    s_percent = (int8_t)s_pmu.getBatteryPercent();
    s_charging = s_pmu.isCharging();
  } else {
    s_percent = -1;
    s_charging = false;
  }
}

} // namespace

namespace GotchiBattery {

void init() {
  // Wire deja init par RTC sur IIC_SDA/IIC_SCL
  s_available = s_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);

  if (s_available) {
    // Detection batterie — OBLIGATOIRE pour que le fuel gauge fonctionne
    s_pmu.enableBattDetection();

    // Activer les ADC
    s_pmu.enableBattVoltageMeasure();
    s_pmu.enableSystemVoltageMeasure();
    s_pmu.enableVbusVoltageMeasure();

    // Reset fuel gauge pour forcer recalcul depuis le voltage actuel
    s_pmu.setRegisterBit(0x17, 2);
    delay(10);
    s_pmu.clrRegisterBit(0x17, 2);

    delay(100);  // laisser le fuel gauge se recalculer
    readBattery();
    Serial.printf("[BAT] AXP2101 OK — SOC=%d%% charging=%d vbat=%umV\n",
                  s_percent, s_charging, s_pmu.getBattVoltage());
  } else {
    Serial.println("[BAT] AXP2101 non detecte");
  }
}

void update(uint32_t dtMs) {
  if (!s_available) return;
  s_timer += dtMs;
  if (s_timer >= READ_INTERVAL_MS) {
    s_timer = 0;
    readBattery();
  }
}

bool isAvailable() { return s_available; }
int8_t getPercent() { return s_percent; }
bool isCharging() { return s_charging; }
bool isPluggedIn() { return s_pluggedIn; }

} // namespace GotchiBattery
