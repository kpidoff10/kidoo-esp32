/**
 * Utilitaires UUID
 * Implémentation des fonctions pour générer des UUID v4 valides
 */

#include "uuid_utils.h"
#include <ESP.h>

bool generateUUIDv4(char* uuid, size_t uuidSize) {
  if (uuid == nullptr || uuidSize < 37) {
    return false;
  }
  
  // Récupérer l'identifiant unique de l'ESP32 (48 bits = 6 bytes de MAC address)
  uint64_t chipId = ESP.getEfuseMac();
  
  // Générer un UUID v4 basé sur le chipId
  // Format UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  // où y est 8, 9, A, ou B (variant bits)
  
  // Utiliser les 48 bits du chipId pour remplir le node (12 derniers hex digits)
  // Les autres parties sont générées de manière déterministe avec version 4 et variant
  
  // time_low: 32 bits basés sur chipId
  uint32_t timeLow = (uint32_t)(chipId & 0xFFFFFFFF);
  
  // time_mid: 16 bits basés sur chipId
  uint16_t timeMid = (uint16_t)((chipId >> 32) & 0xFFFF);
  
  // time_hi_and_version: 16 bits avec version 4 (0x4xxx)
  // Les 4 bits de version (bits 12-15) doivent être 0100 (4)
  // Utiliser les 12 bits inférieurs du chipId pour les bits 0-11
  uint16_t timeHiAndVersion = ((uint16_t)(chipId >> 16) & 0x0FFF) | 0x4000;
  
  // clock_seq_hi_and_reserved: 8 bits avec variant (0x8x-0xBx)
  // Les 2 bits de variant (bits 6-7) doivent être 10
  // Utiliser les 6 bits inférieurs du chipId pour les bits 0-5
  uint8_t clockSeqHiAndReserved = ((uint8_t)(chipId >> 8) & 0x3F) | 0x80;
  
  // clock_seq_low: 8 bits basés sur chipId
  uint8_t clockSeqLow = (uint8_t)(chipId & 0xFF);
  
  // node: 48 bits = chipId (MAC address)
  // Diviser en deux parties pour le format UUID
  uint16_t nodeHigh = (uint16_t)((chipId >> 32) & 0xFFFF);
  uint32_t nodeLow = (uint32_t)(chipId & 0xFFFFFFFF);
  
  // Formater l'UUID
  snprintf(uuid, uuidSize,
    "%08X-%04X-%04X-%02X%02X-%04X%08X",
    timeLow,
    timeMid,
    timeHiAndVersion,
    clockSeqHiAndReserved,
    clockSeqLow,
    nodeHigh,
    nodeLow
  );
  
  return true;
}

String generateUUIDv4String() {
  char uuid[37];
  if (generateUUIDv4(uuid, sizeof(uuid))) {
    return String(uuid);
  }
  return String(""); // Retourner une string vide en cas d'erreur
}
