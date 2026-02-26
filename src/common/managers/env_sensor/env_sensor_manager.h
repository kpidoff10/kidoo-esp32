#ifndef ENV_SENSOR_MANAGER_H
#define ENV_SENSOR_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire capteur environnemental AHT20 + BMP280
 *
 * Module combiné température / humidité (AHT20) et pression (BMP280) sur I2C.
 * - AHT20 : température + humidité relative (adresse 0x38)
 * - BMP280 : température + pression atmosphérique (adresse 0x76 ou 0x77)
 *
 * Utilise le même bus I2C que le RTC (Wire). Initialiser le RTC avant ou
 * s'assurer que Wire.begin() a été appelé.
 *
 * Activé via HAS_ENV_SENSOR dans la config du modèle.
 * Adresse BMP280 optionnelle : ENV_BMP280_I2C_ADDR (défaut 0x76).
 */

#ifdef HAS_ENV_SENSOR

struct EnvSensorData {
  float temperatureC;   // °C (priorité AHT20 si dispo, sinon BMP280)
  float humidityPercent; // % (AHT20 uniquement)
  float pressurePa;     // Pascal (BMP280 uniquement)
  bool aht20Ok;         // AHT20 détecté et lecture OK
  bool bmp280Ok;        // BMP280 détecté et lecture OK
};

class EnvSensorManager {
public:
  /**
   * Initialiser le gestionnaire (détecte AHT20 et BMP280 sur I2C)
   * @return true si au moins un capteur est détecté
   */
  static bool init();

  /**
   * Vérifier si le gestionnaire est initialisé
   */
  static bool isInitialized();

  /**
   * Vérifier si au moins un capteur est disponible
   */
  static bool isAvailable();

  /**
   * Lire la dernière mesure (température, humidité, pression)
   * Lance une nouvelle acquisition si besoin (BMP280 en mode forcé)
   * @param out Structure à remplir
   * @return true si au moins une valeur valide a été lue
   */
  static bool read(EnvSensorData& out);

  /**
   * Température en °C (AHT20 en priorité, sinon BMP280)
   * @return Température ou NAN si indisponible
   */
  static float getTemperatureC();

  /**
   * Humidité relative en %
   * @return Humidité ou NAN si indisponible
   */
  static float getHumidityPercent();

  /**
   * Pression en Pascal
   * @return Pression ou NAN si indisponible
   */
  static float getPressurePa();

  /**
   * Afficher le statut et les dernières valeurs sur Serial
   */
  static void printInfo();

private:
  static bool initialized;
  static bool aht20Available;
  static bool bmp280Available;
  static EnvSensorData lastData;

  // AHT20
  static const uint8_t AHT20_ADDR = 0x38;
  static bool initAHT20();
  static bool readAHT20(float& tempC, float& humPercent);

  static int32_t readBMP280RawTemp();
  static int32_t readBMP280RawPressure();
  // BMP280
  static bool initBMP280();
  static bool readBMP280(float& tempC, float& pressurePa);
  static void readBMP280Calibration();
  static float compensateTempBMP280(int32_t raw);
  static float compensatePressureBMP280(int32_t raw);

  static uint8_t bmp280Addr();
};

#endif // HAS_ENV_SENSOR

#endif // ENV_SENSOR_MANAGER_H
