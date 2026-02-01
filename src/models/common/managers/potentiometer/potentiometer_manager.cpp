#include "potentiometer_manager.h"
#include "../../../model_config.h"

// ============================================
// Classe Potentiometer (instance)
// ============================================

Potentiometer::Potentiometer(uint8_t pin, const char* name)
  : _pin(pin)
  , _name(name)
  , _initialized(false)
  , _available(false)
  , _lastValue(0)
  , _threshold(3)
  , _callback(nullptr)
{
}

bool Potentiometer::init() {
  if (_initialized) {
    return _available;
  }
  
  _initialized = true;
  _available = false;
  
  // Configurer le pin ADC
  pinMode(_pin, INPUT);
  
  // Configurer l'ADC pour une meilleure précision
  analogReadResolution(12);  // 12 bits (0-4095)
  analogSetAttenuation(ADC_11db);  // Plage 0-3.3V
  
  // Lire la valeur initiale
  _lastValue = readPercent();
  
  _available = true;
  Serial.print("[");
  Serial.print(_name);
  Serial.print("] Potentiometre initialise sur GPIO ");
  Serial.println(_pin);
  Serial.print("[");
  Serial.print(_name);
  Serial.print("] Valeur initiale: ");
  Serial.print(_lastValue);
  Serial.println("%");
  
  return _available;
}

bool Potentiometer::isAvailable() const {
  return _initialized && _available;
}

uint16_t Potentiometer::readRaw() {
  if (!_available) {
    return 0;
  }
  
  // Moyennage de plusieurs lectures pour réduire le bruit
  uint32_t sum = 0;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sum += analogRead(_pin);
    delayMicroseconds(100);
  }
  return sum / SAMPLES;
}

uint8_t Potentiometer::readPercent() {
  uint16_t raw = readRaw();
  // Convertir 0-4095 en 0-100
  return (uint8_t)((raw * 100UL) / ADC_MAX);
}

uint8_t Potentiometer::getLastValue() const {
  return _lastValue;
}

uint8_t Potentiometer::getPin() const {
  return _pin;
}

const char* Potentiometer::getName() const {
  return _name;
}

bool Potentiometer::update() {
  if (!_available) {
    return false;
  }
  
  uint8_t currentValue = readPercent();
  
  // Vérifier si le changement dépasse le seuil (hystérésis)
  int16_t diff = (int16_t)currentValue - (int16_t)_lastValue;
  if (diff < 0) diff = -diff;
  
  if (diff >= _threshold) {
    uint8_t oldValue = _lastValue;
    _lastValue = currentValue;
    
    // Appeler le callback si défini
    if (_callback != nullptr) {
      _callback(currentValue, oldValue);
    }
    
    return true;
  }
  
  return false;
}

void Potentiometer::setCallback(PotentiometerCallback callback) {
  _callback = callback;
}

void Potentiometer::setThreshold(uint8_t threshold) {
  _threshold = threshold;
  if (_threshold < 1) _threshold = 1;
  if (_threshold > 50) _threshold = 50;
}

void Potentiometer::printInfo() {
  Serial.println("");
  Serial.print("========== Etat ");
  Serial.print(_name);
  Serial.println(" ==========");
  Serial.print("[");
  Serial.print(_name);
  Serial.print("] Initialise: ");
  Serial.println(_initialized ? "Oui" : "Non");
  Serial.print("[");
  Serial.print(_name);
  Serial.print("] Disponible: ");
  Serial.println(_available ? "Oui" : "Non");
  
  if (_available) {
    Serial.print("[");
    Serial.print(_name);
    Serial.print("] Valeur brute: ");
    Serial.println(readRaw());
    Serial.print("[");
    Serial.print(_name);
    Serial.print("] Valeur (%): ");
    Serial.print(readPercent());
    Serial.println("%");
    Serial.print("[");
    Serial.print(_name);
    Serial.print("] Derniere valeur: ");
    Serial.print(_lastValue);
    Serial.println("%");
    Serial.print("[");
    Serial.print(_name);
    Serial.print("] Seuil: ");
    Serial.print(_threshold);
    Serial.println("%");
    Serial.print("[");
    Serial.print(_name);
    Serial.print("] Pin: GPIO ");
    Serial.println(_pin);
  }
  
  Serial.println("=========================================");
}

// ============================================
// PotentiometerManager (rétrocompatibilité)
// ============================================

Potentiometer* PotentiometerManager::_defaultPot = nullptr;
bool PotentiometerManager::_initialized = false;

bool PotentiometerManager::init() {
  if (_initialized) {
    return _defaultPot != nullptr && _defaultPot->isAvailable();
  }
  
  _initialized = true;
  
#ifdef POTENTIOMETER_PIN
  // Créer l'instance par défaut
  _defaultPot = new Potentiometer(POTENTIOMETER_PIN, "POT");
  return _defaultPot->init();
#else
  Serial.println("[POT] POTENTIOMETER_PIN non defini");
  return false;
#endif
}

bool PotentiometerManager::isAvailable() {
  return _defaultPot != nullptr && _defaultPot->isAvailable();
}

bool PotentiometerManager::isInitialized() {
  return _initialized;
}

uint16_t PotentiometerManager::readRaw() {
  if (_defaultPot == nullptr) return 0;
  return _defaultPot->readRaw();
}

uint8_t PotentiometerManager::readPercent() {
  if (_defaultPot == nullptr) return 0;
  return _defaultPot->readPercent();
}

uint8_t PotentiometerManager::getLastValue() {
  if (_defaultPot == nullptr) return 0;
  return _defaultPot->getLastValue();
}

bool PotentiometerManager::update() {
  if (_defaultPot == nullptr) return false;
  return _defaultPot->update();
}

void PotentiometerManager::setCallback(PotentiometerCallback callback) {
  if (_defaultPot != nullptr) {
    _defaultPot->setCallback(callback);
  }
}

void PotentiometerManager::setThreshold(uint8_t threshold) {
  if (_defaultPot != nullptr) {
    _defaultPot->setThreshold(threshold);
  }
}

void PotentiometerManager::printInfo() {
  if (_defaultPot != nullptr) {
    _defaultPot->printInfo();
  } else {
    Serial.println("[POT] Potentiometre non initialise");
  }
}

Potentiometer* PotentiometerManager::getDefault() {
  return _defaultPot;
}
