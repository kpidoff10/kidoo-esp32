#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

/**
 * Configuration des cœurs CPU et de la mémoire pour ESP32
 * 
 * Ce fichier détecte automatiquement le type de chip et adapte la configuration :
 * 
 * ESP32-S3 (Dual-core + PSRAM) - Modèle Basic
 * -------------------------------------------
 * - Core 0 : WiFi stack, BLE, PubNub, WiFi retry (radio et réseau)
 * - Core 1 : loop(), LEDManager (temps-réel isolé)
 * - PSRAM : Buffers LED, caches
 * - FastLED utilise le driver RMT (hardware) pour éviter les conflits
 * 
 * ESP32-C3 (Single-core RISC-V) - Modèle Mini
 * -------------------------------------------
 * - Core 0 : Tout (seul cœur disponible)
 * - Pas de PSRAM
 * - Priorités ajustées pour éviter les conflits
 * 
 * Les priorités FreeRTOS vont de 0 (idle) à configMAX_PRIORITIES-1 (25 sur ESP32).
 * Plus le nombre est élevé, plus la priorité est haute.
 */

#include <Arduino.h>

// ============================================
// Détection automatique du type de chip
// ============================================

// ESP32-C3 : Single-core RISC-V, pas de PSRAM
#if CONFIG_IDF_TARGET_ESP32C3 || defined(ESP32C3)
  #define IS_SINGLE_CORE      true
  #define IS_DUAL_CORE        false
  #define HAS_PSRAM_SUPPORT   false
  #define CHIP_NAME           "ESP32-C3"

// ESP32-S2 : Single-core, PSRAM possible
#elif CONFIG_IDF_TARGET_ESP32S2 || defined(ESP32S2)
  #define IS_SINGLE_CORE      true
  #define IS_DUAL_CORE        false
  #define HAS_PSRAM_SUPPORT   true
  #define CHIP_NAME           "ESP32-S2"

// ESP32-S3 : Dual-core, PSRAM possible (8MB OPI sur DevKitC-1-N16R8)
#elif CONFIG_IDF_TARGET_ESP32S3 || defined(ESP32S3)
  #define IS_SINGLE_CORE      false
  #define IS_DUAL_CORE        true
  #define HAS_PSRAM_SUPPORT   true
  #define CHIP_NAME           "ESP32-S3"

// ESP32 classique : Dual-core, PSRAM possible
#else
  #define IS_SINGLE_CORE      false
  #define IS_DUAL_CORE        true
  #define HAS_PSRAM_SUPPORT   true
  #define CHIP_NAME           "ESP32"
#endif

// ============================================
// Assignation des cœurs
// ============================================

#if IS_SINGLE_CORE
  // ESP32-C3/S2 : Tout sur Core 0 (seul cœur disponible)
  #define CORE_WIFI         0
  #define CORE_PUBNUB       0
  #define CORE_WIFI_RETRY   0
  #define CORE_LED          0
  #define CORE_BLE          0
  #define CORE_AUDIO        0
  #define CORE_MAIN         0
#else
  // ESP32/S3 Dual-core :
  // Core 0 : WiFi stack + Réseau + BLE + LED (tâches moins critiques)
  #define CORE_WIFI         0   // WiFi stack (automatique ESP-IDF)
  #define CORE_PUBNUB       0   // PubNub (HTTP, dépend WiFi)
  #define CORE_WIFI_RETRY   0   // WiFi retry thread
  #define CORE_BLE          0   // BLE sur Core 0 (partage avec WiFi, même radio)
  #define CORE_LED          0   // LEDManager sur Core 0 (FastLED désactive les interruptions)

  // Core 1 : Audio uniquement (temps-réel critique, isolé)
  #define CORE_AUDIO        1   // AudioManager (I2S, DOIT être isolé des LEDs)
  #define CORE_MAIN         1   // loop() Arduino (automatique)
#endif

// ============================================
// Priorités des tâches FreeRTOS
// ============================================

#if IS_SINGLE_CORE
  // Single-core : Priorités espacées pour éviter la famine
  #define PRIORITY_AUDIO      4   // La plus haute acceptable
  #define PRIORITY_LED        3   // Animations fluides sans casser le RTOS
  #define PRIORITY_PUBNUB     2   // Réseau
  #define PRIORITY_BLE_COMMAND 2  // Traitement commandes BLE (même priorité que PubNub)
  #define PRIORITY_WIFI_RETRY 1   // Background
#else
  // Dual-core : Plus de marge car les tâches sont réparties
  // Audio a la priorité maximale pour éviter les claquements
  #define PRIORITY_LED        10  // Moyenne - LEDs moins critiques que l'audio
  #define PRIORITY_AUDIO      23  // Maximale - audio temps-réel (égal à WiFi)
  #define PRIORITY_PUBNUB     2   // Basse - réseau non critique
  #define PRIORITY_BLE_COMMAND 2  // Traitement commandes BLE (même priorité que PubNub)
  #define PRIORITY_WIFI_RETRY 1   // Très basse - retry en background
#endif

// ============================================
// Configuration PSRAM
// ============================================

#if HAS_PSRAM_SUPPORT
  // Activer l'utilisation de la PSRAM pour les gros buffers
  #define USE_PSRAM_FOR_LED_BUFFER    true
#else
  // ESP32-C3 : Pas de PSRAM, tout en heap interne
  #define USE_PSRAM_FOR_LED_BUFFER    false
#endif

// ============================================
// Tailles de stack des tâches (en bytes)
// ============================================

#define STACK_SIZE_LED          4096    // LEDManager
#define STACK_SIZE_AUDIO        16384   // AudioManager (décodage MP3/streaming) - augmenté pour buffer
#define STACK_SIZE_PUBNUB       8192    // PubNubManager (HTTP + JSON)
#define STACK_SIZE_WIFI_RETRY   4096    // WiFi retry
#define STACK_SIZE_BLE_COMMAND  8192    // Tâche de traitement des commandes BLE (JSON parsing, base64, etc.)

// ============================================
// Helpers pour l'allocation mémoire
// ============================================

/**
 * Allouer de la mémoire en PSRAM si disponible, sinon en heap classique
 * @param size Taille en bytes
 * @return Pointeur vers la mémoire allouée, nullptr si échec
 */
inline void* allocatePsram(size_t size) {
  if (psramFound()) {
    void* ptr = ps_malloc(size);
    if (ptr != nullptr) {
      Serial.printf("[PSRAM] Alloue %d bytes en PSRAM\n", size);
      return ptr;
    }
    Serial.println("[PSRAM] Echec allocation PSRAM, fallback heap");
  }
  return malloc(size);
}

/**
 * Allouer de la mémoire alignée en PSRAM (pour DMA)
 * @param size Taille en bytes
 * @param alignment Alignement requis (généralement 4 ou 32)
 * @return Pointeur vers la mémoire allouée, nullptr si échec
 */
inline void* allocatePsramAligned(size_t size, size_t alignment) {
  if (psramFound()) {
    // heap_caps_aligned_alloc avec MALLOC_CAP_SPIRAM pour PSRAM
    void* ptr = heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_SPIRAM);
    if (ptr != nullptr) {
      Serial.printf("[PSRAM] Alloue %d bytes alignes (%d) en PSRAM\n", size, alignment);
      return ptr;
    }
    Serial.println("[PSRAM] Echec allocation PSRAM alignee, fallback heap");
  }
  // Fallback : allocation alignée en heap interne
  return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_8BIT);
}

/**
 * Afficher les statistiques mémoire
 */
inline void printMemoryStats() {
  Serial.println("");
  Serial.println("========== Statistiques Memoire ==========");
  
  // RAM interne
  Serial.print("[MEM] Heap total: ");
  Serial.print(ESP.getHeapSize() / 1024);
  Serial.println(" KB");
  Serial.print("[MEM] Heap libre: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.print("[MEM] Heap utilise: ");
  Serial.print((ESP.getHeapSize() - ESP.getFreeHeap()) / 1024);
  Serial.println(" KB");
  
  // PSRAM
  #if HAS_PSRAM_SUPPORT
  if (psramFound()) {
    Serial.print("[MEM] PSRAM total: ");
    Serial.print(ESP.getPsramSize() / 1024 / 1024);
    Serial.println(" MB");
    Serial.print("[MEM] PSRAM libre: ");
    Serial.print(ESP.getFreePsram() / 1024 / 1024);
    Serial.println(" MB");
    Serial.print("[MEM] PSRAM utilise: ");
    Serial.print((ESP.getPsramSize() - ESP.getFreePsram()) / 1024);
    Serial.println(" KB");
  } else {
    Serial.println("[MEM] PSRAM: Non detectee");
  }
  #else
  Serial.println("[MEM] PSRAM: Non supportee sur ce chip");
  #endif
  
  Serial.println("==========================================");
}

/**
 * Afficher l'architecture CPU détectée
 */
inline void printCoreArchitecture() {
  Serial.println("");
  Serial.println("========== Architecture CPU ==========");
  Serial.print("[CPU] Chip: ");
  Serial.println(CHIP_NAME);
  
  #if IS_SINGLE_CORE
  Serial.println("[CPU] Mode: Single-core");
  Serial.println("[CPU] Core 0: WiFi, BLE, LED, PubNub (tout)");
  #else
  Serial.println("[CPU] Mode: Dual-core");
  Serial.printf("[CPU] Core 0: WiFi, BLE, PubNub (P%d), WiFi-retry (P%d)\n", 
                PRIORITY_PUBNUB, PRIORITY_WIFI_RETRY);
  Serial.printf("[CPU] Core 1: loop(), LED (P%d) [RMT driver]\n", PRIORITY_LED);
  #endif
  
  #if HAS_PSRAM_SUPPORT
  Serial.println("[CPU] PSRAM: Supportee");
  #else
  Serial.println("[CPU] PSRAM: Non supportee");
  #endif
  
  Serial.println("======================================");
}

#endif // CORE_CONFIG_H
