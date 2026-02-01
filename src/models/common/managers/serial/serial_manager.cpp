#include "serial_manager.h"
#include "../log/log_manager.h"
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <cstdarg>

// Variables statiques
bool SerialManager::initialized = false;

void SerialManager::init() {
  if (initialized) {
    return;
  }
  
  initialized = true;
  
  // Afficher les informations système au démarrage
  printSystemInfo();
}

bool SerialManager::isAvailable() {
  return Serial && Serial.availableForWrite() > 0;
}

void SerialManager::reboot(uint32_t delayMs) {
  if (delayMs > 0) {
    Serial.print("[SERIAL] Redemarrage dans ");
    Serial.print(delayMs);
    Serial.println(" ms...");
    delay(delayMs);
  }
  
  Serial.println("[SERIAL] Redemarrage de l'ESP32...");
  Serial.flush();
  
  // Redémarrer l'ESP32
  esp_restart();
}

void SerialManager::deepSleep(uint32_t delayMs) {
  if (delayMs > 0) {
    Serial.print("[SERIAL] Deep sleep dans ");
    Serial.print(delayMs);
    Serial.println(" ms...");
    delay(delayMs);
  }
  
  Serial.println("[SERIAL] Passage en deep sleep...");
  Serial.flush();
  
  // Mettre en deep sleep (se réveillera après 1 seconde)
  // Note: Pour un deep sleep permanent, utiliser esp_deep_sleep_start()
  esp_deep_sleep(1000000ULL); // 1 seconde en microsecondes
}

void SerialManager::printSystemInfo() {
  if (!isAvailable()) {
    return;
  }
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("     INFORMATIONS SYSTEME");
  Serial.println("========================================");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.println("========================================");
  Serial.println("");
}

void SerialManager::printMemoryInfo() {
  if (!isAvailable()) {
    return;
  }
  
  uint32_t freeHeap = getFreeHeap();
  uint32_t totalHeap = getTotalHeap();
  uint32_t minFreeHeap = getMinFreeHeap();
  uint32_t usedHeap = totalHeap - freeHeap;
  uint32_t usagePercent = (usedHeap * 100) / totalHeap;
  
  printTimestamp();
  Serial.println("[MEMORY] Informations memoire:");
  Serial.print("  Heap libre: ");
  Serial.print(freeHeap);
  Serial.print(" octets (");
  Serial.print(freeHeap / 1024);
  Serial.println(" KB)");
  Serial.print("  Heap utilise: ");
  Serial.print(usedHeap);
  Serial.print(" octets (");
  Serial.print(usedHeap / 1024);
  Serial.println(" KB)");
  Serial.print("  Heap total: ");
  Serial.print(totalHeap);
  Serial.print(" octets (");
  Serial.print(totalHeap / 1024);
  Serial.println(" KB)");
  Serial.print("  Heap minimum atteint: ");
  Serial.print(minFreeHeap);
  Serial.print(" octets (");
  Serial.print(minFreeHeap / 1024);
  Serial.println(" KB)");
  Serial.print("  Utilisation: ");
  Serial.print(usagePercent);
  Serial.println("%");
  
  // Détails par type de mémoire
  Serial.println("");
  Serial.println("  [Details par type]");
  
  // DRAM (mémoire interne rapide)
  uint32_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t totalDRAM = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.print("  DRAM interne: ");
  Serial.print(freeDRAM / 1024);
  Serial.print(" KB libre / ");
  Serial.print(totalDRAM / 1024);
  Serial.println(" KB total");
  
  // PSRAM (si disponible)
  uint32_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  uint32_t totalPSRAM = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  if (totalPSRAM > 0) {
    Serial.print("  PSRAM externe: ");
    Serial.print(freePSRAM / 1024);
    Serial.print(" KB libre / ");
    Serial.print(totalPSRAM / 1024);
    Serial.println(" KB total");
  } else {
    Serial.println("  PSRAM externe: Non disponible");
  }
  
  // Plus grand bloc contigu disponible
  uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  Serial.print("  Plus grand bloc libre: ");
  Serial.print(largestBlock / 1024);
  Serial.println(" KB");
  
  // Avertissement si mémoire critique
  if (usagePercent > 85) {
    Serial.println("");
    Serial.println("  [!] ATTENTION: Memoire critique!");
    Serial.println("  Causes possibles:");
    Serial.println("    - WiFi + BLE actifs simultanement (~70-100 KB)");
    #ifdef HAS_PUBNUB
    Serial.println("    - PubNub connecte (~20-30 KB)");
    #endif
    Serial.println("    - Gros documents JSON en memoire");
  }
}

uint32_t SerialManager::getFreeHeap() {
  return ESP.getFreeHeap();
}

uint32_t SerialManager::getTotalHeap() {
  return ESP.getHeapSize();
}

uint32_t SerialManager::getMinFreeHeap() {
  return ESP.getMinFreeHeap();
}

void SerialManager::printTimestamp() {
  if (!isAvailable()) {
    return;
  }
  
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  Serial.print("[");
  if (hours < 10) Serial.print("0");
  Serial.print(hours);
  Serial.print(":");
  if (minutes % 60 < 10) Serial.print("0");
  Serial.print(minutes % 60);
  Serial.print(":");
  if (seconds % 60 < 10) Serial.print("0");
  Serial.print(seconds % 60);
  Serial.print(".");
  if (ms % 1000 < 100) Serial.print("0");
  if (ms % 1000 < 10) Serial.print("0");
  Serial.print(ms % 1000);
  Serial.print("] ");
}

void SerialManager::log(const char* format, ...) {
  if (!isAvailable()) {
    return;
  }
  
  printTimestamp();
  
  va_list args;
  va_start(args, format);
  
  // Utiliser vsnprintf pour formater le message
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  Serial.println(buffer);
  
  va_end(args);
}

void SerialManager::logError(const char* format, ...) {
  if (!isAvailable()) {
    return;
  }
  
  va_list args;
  va_start(args, format);
  
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  
  // Utiliser LogManager pour logger l'erreur (écrit aussi sur SD)
  LogManager::error("%s", buffer);
  
  va_end(args);
}

void SerialManager::logDebug(const char* format, ...) {
  if (!isAvailable()) {
    return;
  }
  
  printTimestamp();
  Serial.print("[DEBUG] ");
  
  va_list args;
  va_start(args, format);
  
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  Serial.println(buffer);
  
  va_end(args);
}
