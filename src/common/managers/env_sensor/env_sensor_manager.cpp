// Inclure la config du modèle avant le header pour que HAS_ENV_SENSOR soit défini
#include "models/model_config.h"
#ifdef HAS_ENV_SENSOR
#include "env_sensor_manager.h"
#include <Wire.h>

#ifndef ENV_BMP280_I2C_ADDR
#define ENV_BMP280_I2C_ADDR 0x76
#endif

bool EnvSensorManager::initialized = false;
bool EnvSensorManager::aht20Available = false;
bool EnvSensorManager::bmp280Available = false;
EnvSensorData EnvSensorManager::lastData = { NAN, NAN, NAN, false, false };

// BMP280 calibration (signés/non signés selon datasheet)
static uint16_t dig_T1 = 0;
static int16_t dig_T2 = 0, dig_T3 = 0;
static uint16_t dig_P1 = 0;
static int16_t dig_P2 = 0, dig_P3 = 0, dig_P4 = 0, dig_P5 = 0, dig_P6 = 0, dig_P7 = 0, dig_P8 = 0, dig_P9 = 0;
static int32_t t_fine = 0;

uint8_t EnvSensorManager::bmp280Addr() {
#ifdef ENV_BMP280_I2C_ADDR
  return ENV_BMP280_I2C_ADDR;
#else
  return 0x76;
#endif
}

bool EnvSensorManager::init() {
  if (initialized) {
    return aht20Available || bmp280Available;
  }
  initialized = true;
  aht20Available = initAHT20();
  bmp280Available = initBMP280();
  if (Serial && !aht20Available && !bmp280Available) {
    Serial.println("[ENV] ERREUR: Aucun capteur AHT20/BMP280 detecte");
  }
  return aht20Available || bmp280Available;
}

bool EnvSensorManager::isInitialized() { return initialized; }
bool EnvSensorManager::isAvailable() { return aht20Available || bmp280Available; }

bool EnvSensorManager::initAHT20() {
  Wire.beginTransmission(AHT20_ADDR);
  if (Wire.endTransmission() != 0) return false;
  // Soft reset
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  // Init
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  return true;
}

bool EnvSensorManager::readAHT20(float& tempC, float& humPercent) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(80);
  if (Wire.requestFrom((uint8_t)AHT20_ADDR, (uint8_t)6) != 6) return false;
  uint8_t b0 = Wire.read();
  uint8_t b1 = Wire.read();
  uint8_t b2 = Wire.read();
  uint8_t b3 = Wire.read();
  uint8_t b4 = Wire.read();
  uint8_t b5 = Wire.read();
  if (b0 & 0x80) return false; // busy
  uint32_t hraw = ((uint32_t)b1 << 12) | ((uint32_t)b2 << 4) | (b3 >> 4);
  uint32_t traw = ((uint32_t)(b3 & 0x0F) << 16) | ((uint32_t)b4 << 8) | b5;
  humPercent = (float)hraw * 100.0f / 1048576.0f;
  tempC = (float)traw * 200.0f / 1048576.0f - 50.0f;
  return true;
}

void EnvSensorManager::readBMP280Calibration() {
  uint8_t addr = bmp280Addr();
  Wire.beginTransmission(addr);
  Wire.write(0x88);
  if (Wire.endTransmission() != 0) return;
  if (Wire.requestFrom(addr, (uint8_t)24) != 24) return;
  dig_T1 = Wire.read() | (Wire.read() << 8);
  dig_T2 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_T3 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P1 = Wire.read() | (Wire.read() << 8);
  dig_P2 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P3 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P4 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P5 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P6 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P7 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P8 = (int16_t)(Wire.read() | (Wire.read() << 8));
  dig_P9 = (int16_t)(Wire.read() | (Wire.read() << 8));
}

bool EnvSensorManager::initBMP280() {
  uint8_t addr = bmp280Addr();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(addr);
  Wire.write(0xD0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  if (Wire.read() != 0x58) return false; // BMP280 chip ID
  readBMP280Calibration();
  // Mode forcé, oversampling temp x2, pressure x2
  Wire.beginTransmission(addr);
  Wire.write(0xF4);
  Wire.write(0x25);
  if (Wire.endTransmission() != 0) return false;
  return true;
}

// Lecture brute BMP280 : déclenche une mesure forcée puis lit les 6 octets (press + temp)
static bool readBMP280Raw(uint8_t addr, int32_t& rawTemp, int32_t& rawPress) {
  Wire.beginTransmission(addr);
  Wire.write(0xF4);
  Wire.write(0x25); // forced, temp x2, press x2
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  Wire.beginTransmission(addr);
  Wire.write(0xF7);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)6) != 6) return false;
  uint8_t p0 = Wire.read(), p1 = Wire.read(), p2 = Wire.read();
  uint8_t t0 = Wire.read(), t1 = Wire.read(), t2 = Wire.read();
  rawPress = (int32_t)p0 << 12 | (int32_t)p1 << 4 | (p2 >> 4);
  rawTemp = (int32_t)t0 << 12 | (int32_t)t1 << 4 | (t2 >> 4);
  return true;
}

int32_t EnvSensorManager::readBMP280RawTemp() {
  int32_t rT = 0, rP = 0;
  if (!readBMP280Raw(bmp280Addr(), rT, rP)) return 0;
  return rT;
}

int32_t EnvSensorManager::readBMP280RawPressure() {
  int32_t rT = 0, rP = 0;
  if (!readBMP280Raw(bmp280Addr(), rT, rP)) return 0;
  return rP;
}

float EnvSensorManager::compensateTempBMP280(int32_t raw) {
  int32_t var1 = ((((raw >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  int32_t var2 = (((((raw >> 4) - ((int32_t)dig_T1)) * ((raw >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  return (float)((t_fine * 5 + 128) >> 8) / 100.0f;
}

float EnvSensorManager::compensatePressureBMP280(int32_t raw) {
  int64_t var1 = (int64_t)t_fine - 128000;
  int64_t var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + ((int64_t)dig_P4 << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = ((((int64_t)1 << 47) + var1) * (int64_t)dig_P1) >> 33;
  if (var1 == 0) return 0;
  int64_t p = 1048576 - raw;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)dig_P8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);
  return (float)p;
}

bool EnvSensorManager::readBMP280(float& tempC, float& pressurePa) {
  int32_t rT = 0, rP = 0;
  if (!readBMP280Raw(bmp280Addr(), rT, rP)) return false;
  tempC = compensateTempBMP280(rT);
  pressurePa = compensatePressureBMP280(rP);
  return true;
}

bool EnvSensorManager::read(EnvSensorData& out) {
  out.temperatureC = NAN;
  out.humidityPercent = NAN;
  out.pressurePa = NAN;
  out.aht20Ok = false;
  out.bmp280Ok = false;
  if (!initialized) return false;

  float tA = NAN, hA = NAN, tB = NAN, pB = NAN;
  if (aht20Available && readAHT20(tA, hA)) {
    out.temperatureC = tA;
    out.humidityPercent = hA;
    out.aht20Ok = true;
  }
  if (bmp280Available && readBMP280(tB, pB)) {
    if (!out.aht20Ok) out.temperatureC = tB;
    out.pressurePa = pB;
    out.bmp280Ok = true;
  }
  lastData = out;
  return out.aht20Ok || out.bmp280Ok;
}

float EnvSensorManager::getTemperatureC() {
  EnvSensorData d;
  read(d);
  return d.temperatureC;
}
float EnvSensorManager::getHumidityPercent() {
  EnvSensorData d;
  read(d);
  return d.humidityPercent;
}
float EnvSensorManager::getPressurePa() {
  EnvSensorData d;
  read(d);
  return d.pressurePa;
}

void EnvSensorManager::printInfo() {
  if (!Serial) return;
  Serial.println("");
  Serial.println("========== Capteur env (AHT20+BMP280) ==========");
  if (!initialized) {
    Serial.println("[ENV] Non initialise");
    Serial.println("===============================================");
    return;
  }
  Serial.printf("[ENV] AHT20: %s\n", aht20Available ? "OK" : "Non detecte");
  Serial.printf("[ENV] BMP280: %s\n", bmp280Available ? "OK" : "Non detecte");
  EnvSensorData d;
  if (read(d)) {
    if (!isnan(d.temperatureC))
      Serial.printf("[ENV] Temperature: %.1f °C\n", d.temperatureC);
    if (!isnan(d.humidityPercent))
      Serial.printf("[ENV] Humidite: %.1f %%\n", d.humidityPercent);
    if (!isnan(d.pressurePa))
      Serial.printf("[ENV] Pression: %.0f Pa (%.1f hPa)\n", d.pressurePa, d.pressurePa / 100.0f);
  } else {
    Serial.println("[ENV] Lecture impossible");
  }
  Serial.println("===============================================");
}

#endif // HAS_ENV_SENSOR
