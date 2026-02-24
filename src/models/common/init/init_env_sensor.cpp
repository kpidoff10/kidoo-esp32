#include "../managers/init/init_manager.h"
#include "../../model_config.h"
#ifdef HAS_ENV_SENSOR
#include "../managers/env_sensor/env_sensor_manager.h"
#endif

/**
 * Initialisation du capteur environnemental AHT20 + BMP280
 * (température, humidité, pression)
 */

#ifdef HAS_ENV_SENSOR

bool InitManager::initEnvSensor() {
  systemStatus.envSensor = INIT_IN_PROGRESS;

  if (!HAS_ENV_SENSOR) {
    systemStatus.envSensor = INIT_NOT_STARTED;
    return true;
  }

  if (EnvSensorManager::init()) {
    systemStatus.envSensor = INIT_SUCCESS;
    return true;
  }

  systemStatus.envSensor = INIT_FAILED;
  return false;
}

#else

bool InitManager::initEnvSensor() {
  systemStatus.envSensor = INIT_NOT_STARTED;
  return true;
}

#endif
