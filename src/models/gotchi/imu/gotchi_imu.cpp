#include "gotchi_imu.h"
#include "../config/config.h"

#include <Wire.h>
#include <Arduino.h>
#include <SensorQMI8658.hpp>
#include <cmath>

namespace {

SensorQMI8658 s_imu;
bool s_ok = false;

// Shake detection
constexpr float SHAKE_THRESHOLD = 0.25f;  // g (tes secousses font 0.3-0.6g)
constexpr uint32_t SHAKE_COOLDOWN = 800;   // ms entre deux events (plus reactif)
constexpr uint32_t SHAKE_MIN_HITS = 2;     // 2 pics suffisent

uint32_t s_lastShakeAt = 0;
uint8_t  s_shakeHits = 0;
uint32_t s_shakeWindow = 0;

// Accumulation direction
float s_accDirX = 0;
float s_accDirY = 0;

} // namespace

namespace GotchiImu {

bool init() {
  if (s_ok) return true;

  s_ok = s_imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!s_ok) {
    s_ok = s_imu.begin(Wire, QMI8658_H_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  }

  if (!s_ok) {
    Serial.println("[IMU] QMI8658 non detecte");
    return false;
  }

  s_imu.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_125Hz
  );
  s_imu.enableAccelerometer();

  Serial.println("[IMU] QMI8658 OK (accel 4G, 125Hz)");
  return true;
}

bool update(uint32_t dtMs, float& shakeX, float& shakeY) {
  if (!s_ok) return false;

  float ax, ay, az;
  if (!s_imu.getDataReady()) return false;
  if (!s_imu.getAccelerometer(ax, ay, az)) return false;

  // Magnitude sans gravite
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float force = fabsf(mag - 1.0f);



  // Fenetre de detection
  s_shakeWindow += dtMs;
  if (s_shakeWindow > 800) {
    s_shakeHits = 0;
    s_shakeWindow = 0;
    s_accDirX = 0;
    s_accDirY = 0;
  }

  if (force > SHAKE_THRESHOLD) {
    s_shakeHits++;
    // Accumuler la direction (ax = gauche/droite, ay = avant/arriere)
    s_accDirX += ax;
    s_accDirY += ay;
  }

  uint32_t now = millis();
  if (s_shakeHits >= SHAKE_MIN_HITS && (now - s_lastShakeAt) > SHAKE_COOLDOWN) {
    s_lastShakeAt = now;

    // Normaliser la direction
    float dirMag = sqrtf(s_accDirX * s_accDirX + s_accDirY * s_accDirY);
    if (dirMag > 0.1f) {
      shakeX = s_accDirX / dirMag;  // -1 gauche, +1 droite
      shakeY = s_accDirY / dirMag;  // -1 haut, +1 bas
    } else {
      shakeX = 0;
      shakeY = 0;
    }

    Serial.printf("[IMU] Shake! dir=(%.1f, %.1f) force=%.1fg\n", shakeX, shakeY, force);

    s_shakeHits = 0;
    s_shakeWindow = 0;
    s_accDirX = 0;
    s_accDirY = 0;
    return true;
  }

  return false;
}

} // namespace GotchiImu
