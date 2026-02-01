#include "rtc_manager.h"
#include <Wire.h>
#include <time.h>
#include "../wifi/wifi_manager.h"
#include "../../../model_config.h"

// Serveurs NTP
static const char* NTP_SERVER_1 = "pool.ntp.org";
static const char* NTP_SERVER_2 = "time.google.com";
static const char* NTP_SERVER_3 = "time.cloudflare.com";

// Variables statiques
bool RTCManager::initialized = false;
bool RTCManager::available = false;
bool RTCManager::ntpSynced = false;

bool RTCManager::init() {
  if (initialized) {
    return available;
  }
  
  initialized = true;
  available = false;
  
  // Initialiser le bus I2C si pas déjà fait
  // Wire.begin() peut être appelé plusieurs fois sans problème
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  delay(10); // Petit délai pour stabiliser le bus
  
  // Vérifier si le DS3231 répond
  Wire.beginTransmission(DS3231_ADDRESS);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    available = true;
    
    // Vérifier si l'oscillateur s'est arrêté (bit OSF dans le registre status)
    if (hasLostPower()) {
      Serial.println("[RTC] WARNING: Oscillateur arrete, heure non valide");
      // Effacer le flag OSF
      uint8_t status = readRegister(REG_STATUS);
      writeRegister(REG_STATUS, status & ~0x80);
    }
    
    Serial.println("[RTC] DS3231 detecte et initialise");
  } else {
    Serial.print("[RTC] ERREUR: DS3231 non detecte (erreur I2C: ");
    Serial.print(error);
    Serial.println(")");
  }
  
  return available;
}

bool RTCManager::isAvailable() {
  return initialized && available;
}

bool RTCManager::isInitialized() {
  return initialized;
}

uint8_t RTCManager::bcdToDec(uint8_t bcd) {
  return ((bcd / 16) * 10) + (bcd % 16);
}

uint8_t RTCManager::decToBcd(uint8_t dec) {
  return ((dec / 10) * 16) + (dec % 10);
}

uint8_t RTCManager::readRegister(uint8_t reg) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(DS3231_ADDRESS, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

void RTCManager::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

DateTime RTCManager::getDateTime() {
  DateTime dt = {0, 0, 0, 0, 0, 0, 0};
  
  if (!isAvailable()) {
    return dt;
  }
  
  // Lire tous les registres de temps d'un coup
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(REG_SECONDS);
  Wire.endTransmission();
  
  Wire.requestFrom(DS3231_ADDRESS, (uint8_t)7);
  
  if (Wire.available() >= 7) {
    dt.second = bcdToDec(Wire.read() & 0x7F);
    dt.minute = bcdToDec(Wire.read());
    dt.hour = bcdToDec(Wire.read() & 0x3F); // Format 24h
    dt.dayOfWeek = bcdToDec(Wire.read());
    dt.day = bcdToDec(Wire.read());
    dt.month = bcdToDec(Wire.read() & 0x1F);
    dt.year = 2000 + bcdToDec(Wire.read());
  }
  
  return dt;
}

bool RTCManager::setDateTime(const DateTime& dt) {
  if (!isAvailable()) {
    return false;
  }
  
  // Valider les valeurs
  if (dt.year < 2000 || dt.year > 2099) return false;
  if (dt.month < 1 || dt.month > 12) return false;
  if (dt.day < 1 || dt.day > 31) return false;
  if (dt.hour > 23) return false;
  if (dt.minute > 59) return false;
  if (dt.second > 59) return false;
  
  // Calculer le jour de la semaine si non fourni
  uint8_t dow = dt.dayOfWeek;
  if (dow == 0 || dow > 7) {
    dow = calculateDayOfWeek(dt.year, dt.month, dt.day);
  }
  
  // Écrire tous les registres de temps
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(REG_SECONDS);
  Wire.write(decToBcd(dt.second));
  Wire.write(decToBcd(dt.minute));
  Wire.write(decToBcd(dt.hour)); // Format 24h
  Wire.write(decToBcd(dow));
  Wire.write(decToBcd(dt.day));
  Wire.write(decToBcd(dt.month));
  Wire.write(decToBcd(dt.year - 2000));
  uint8_t error = Wire.endTransmission();
  
  return (error == 0);
}

String RTCManager::getTimeString() {
  DateTime dt = getDateTime();
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
  return String(buffer);
}

String RTCManager::getDateString() {
  DateTime dt = getDateTime();
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", dt.day, dt.month, dt.year);
  return String(buffer);
}

String RTCManager::getDateTimeString() {
  DateTime dt = getDateTime();
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d:%02d", 
           dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);
  return String(buffer);
}

uint32_t RTCManager::getUnixTime() {
  DateTime dt = getDateTime();
  
  // Calcul simplifié du timestamp Unix
  // Nombre de jours depuis 1970
  uint16_t year = dt.year;
  uint32_t days = 0;
  
  // Années complètes depuis 1970
  for (uint16_t y = 1970; y < year; y++) {
    days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  }
  
  // Mois de l'année en cours
  static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (uint8_t m = 1; m < dt.month; m++) {
    days += daysInMonth[m - 1];
    if (m == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
      days++; // Février bissextile
    }
  }
  
  // Jours du mois en cours
  days += dt.day - 1;
  
  // Convertir en secondes et ajouter l'heure
  return days * 86400UL + dt.hour * 3600UL + dt.minute * 60UL + dt.second;
}

bool RTCManager::setUnixTime(uint32_t timestamp) {
  // Convertir le timestamp en DateTime
  DateTime dt;
  
  uint32_t remaining = timestamp;
  
  // Calculer l'année
  dt.year = 1970;
  while (true) {
    uint32_t daysInYear = (dt.year % 4 == 0 && (dt.year % 100 != 0 || dt.year % 400 == 0)) ? 366 : 365;
    uint32_t secondsInYear = daysInYear * 86400UL;
    if (remaining < secondsInYear) break;
    remaining -= secondsInYear;
    dt.year++;
  }
  
  // Calculer le mois
  static const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  dt.month = 1;
  while (dt.month <= 12) {
    uint8_t days = daysInMonth[dt.month - 1];
    if (dt.month == 2 && (dt.year % 4 == 0 && (dt.year % 100 != 0 || dt.year % 400 == 0))) {
      days = 29;
    }
    uint32_t secondsInMonth = days * 86400UL;
    if (remaining < secondsInMonth) break;
    remaining -= secondsInMonth;
    dt.month++;
  }
  
  // Calculer le jour
  dt.day = (remaining / 86400UL) + 1;
  remaining %= 86400UL;
  
  // Calculer l'heure
  dt.hour = remaining / 3600UL;
  remaining %= 3600UL;
  
  // Calculer les minutes et secondes
  dt.minute = remaining / 60UL;
  dt.second = remaining % 60UL;
  
  // Calculer le jour de la semaine
  dt.dayOfWeek = calculateDayOfWeek(dt.year, dt.month, dt.day);
  
  return setDateTime(dt);
}

float RTCManager::getTemperature() {
  if (!isAvailable()) {
    return 0.0f;
  }
  
  // Lire les registres de température
  int8_t msb = (int8_t)readRegister(REG_TEMP_MSB);
  uint8_t lsb = readRegister(REG_TEMP_LSB);
  
  // La température est sur 10 bits (8 bits MSB + 2 bits LSB)
  // MSB est signé, LSB contient les 2 bits de fraction (0.25°C par bit)
  float temp = (float)msb + ((lsb >> 6) * 0.25f);
  
  return temp;
}

bool RTCManager::hasLostPower() {
  if (!initialized) {
    return true;
  }
  
  // Bit OSF (Oscillator Stop Flag) dans le registre status
  uint8_t status = readRegister(REG_STATUS);
  return (status & 0x80) != 0;
}

uint8_t RTCManager::calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day) {
  // Algorithme de Zeller simplifié
  // Retourne 1=Lundi, 7=Dimanche
  
  if (month < 3) {
    month += 12;
    year--;
  }
  
  int k = year % 100;
  int j = year / 100;
  
  int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  
  // Convertir de Zeller (0=Samedi) vers notre format (1=Lundi)
  int dow = ((h + 5) % 7) + 1;
  
  return (uint8_t)dow;
}

void RTCManager::printInfo() {
  Serial.println("");
  Serial.println("========== Etat RTC DS3231 ==========");
  Serial.print("[RTC] Initialise: ");
  Serial.println(initialized ? "Oui" : "Non");
  Serial.print("[RTC] Disponible: ");
  Serial.println(available ? "Oui" : "Non");
  
  if (available) {
    Serial.print("[RTC] Date/Heure: ");
    Serial.println(getDateTimeString());
    Serial.print("[RTC] Timestamp Unix: ");
    Serial.println(getUnixTime());
    Serial.print("[RTC] Temperature: ");
    Serial.print(getTemperature(), 2);
    Serial.println(" C");
    Serial.print("[RTC] Perte alimentation: ");
    Serial.println(hasLostPower() ? "Oui (heure non fiable)" : "Non");
  }
  
  Serial.println("=====================================");
}

bool RTCManager::syncWithNTP(long gmtOffsetSec, int daylightOffsetSec) {
  // Vérifier que le WiFi est connecté
  if (!WiFiManager::isConnected()) {
    Serial.println("[RTC] ERREUR: WiFi non connecte pour sync NTP");
    return false;
  }
  
  // Vérifier que le RTC est disponible
  if (!isAvailable()) {
    Serial.println("[RTC] ERREUR: RTC non disponible");
    return false;
  }
  
  Serial.println("[RTC] Synchronisation NTP en cours...");
  
  // Configurer le client NTP
  configTime(gmtOffsetSec, daylightOffsetSec, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  
  // Attendre la synchronisation (max 10 secondes)
  struct tm timeinfo;
  int attempts = 0;
  const int maxAttempts = 20;
  
  while (!getLocalTime(&timeinfo) && attempts < maxAttempts) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
  Serial.println();
  
  if (attempts >= maxAttempts) {
    Serial.println("[RTC] ERREUR: Timeout synchronisation NTP");
    return false;
  }
  
  // Convertir en DateTime et mettre à jour le RTC
  DateTime dt;
  dt.year = timeinfo.tm_year + 1900;
  dt.month = timeinfo.tm_mon + 1;
  dt.day = timeinfo.tm_mday;
  dt.hour = timeinfo.tm_hour;
  dt.minute = timeinfo.tm_min;
  dt.second = timeinfo.tm_sec;
  dt.dayOfWeek = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday; // Dimanche = 7
  
  if (setDateTime(dt)) {
    Serial.print("[RTC] Heure synchronisee: ");
    Serial.println(getDateTimeString());
    return true;
  } else {
    Serial.println("[RTC] ERREUR: Echec mise a jour RTC");
    return false;
  }
}

bool RTCManager::syncWithNTPFrance() {
  // France: GMT+1 (hiver) ou GMT+2 (été)
  // On utilise GMT+1 avec détection automatique de l'heure d'été
  // gmtOffsetSec = 3600 (1 heure)
  // daylightOffsetSec = 3600 (1 heure supplémentaire en été)
  bool result = syncWithNTP(3600, 3600);
  if (result) {
    ntpSynced = true;
  }
  return result;
}

bool RTCManager::isTimeValid() {
  if (!isAvailable()) {
    return false;
  }
  
  DateTime dt = getDateTime();
  
  // L'heure est considérée valide si l'année est >= 2026
  // (on est en 2026, donc toute année avant est invalide)
  if (dt.year < 2026) {
    return false;
  }
  
  // Vérifier aussi si l'heure semble raisonnable (entre 0h et 23h59)
  if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
    return false;
  }
  
  return true;
}

bool RTCManager::hasBeenSynced() {
  return ntpSynced;
}

bool RTCManager::autoSyncIfNeeded() {
  // Si le RTC n'est pas disponible, ne rien faire
  if (!isAvailable()) {
    return false;
  }
  
  // Si le WiFi n'est pas connecté, ne rien faire
  if (!WiFiManager::isConnected()) {
    return false;
  }
  
  // Vérifier si une synchronisation est nécessaire
  bool needsSync = false;
  
  // Cas 1: Le RTC a perdu l'alimentation
  if (hasLostPower()) {
    Serial.println("[RTC] Auto-sync: RTC a perdu l'alimentation");
    needsSync = true;
  }
  // Cas 2: L'heure semble invalide (année < 2026 ou valeurs hors limites)
  else if (!isTimeValid()) {
    Serial.println("[RTC] Auto-sync: Heure invalide detectee");
    needsSync = true;
  }
  // Cas 3: Pas encore synchronisé dans cette session - synchroniser systématiquement
  // Cela garantit que l'heure est toujours à jour après une connexion WiFi
  else if (!ntpSynced) {
    Serial.println("[RTC] Auto-sync: Premiere synchronisation de la session");
    needsSync = true;
  }
  
  if (needsSync) {
    Serial.println("[RTC] Synchronisation NTP automatique...");
    bool result = syncWithNTPFrance();
    if (result) {
      ntpSynced = true;
    }
    return result;
  }
  
  // Pas besoin de sync, marquer comme "synced" pour éviter les vérifications futures
  // (l'heure est déjà valide et synchronisée)
  ntpSynced = true;
  return true;
}
