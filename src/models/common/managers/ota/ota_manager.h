#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire OTA (Over-The-Air) pour mise à jour firmware
 *
 * - Récupère l'URL du .bin via GET /api/firmware/download?model=&version=
 * - Télécharge le binaire et l'écrit en partition OTA
 * - Pendant le téléchargement : effet LED arc-en-ciel (comme au setup)
 * - À la fin : publie un message PubNub "firmware-update-done" puis redémarre
 * - En cas d'erreur : publie "firmware-update-failed" et ne redémarre pas
 */

class OtaManager {
public:
  /**
   * Démarrer la mise à jour OTA (lance une tâche FreeRTOS)
   * @param model Modèle Kidoo ("dream", "basic", "mini")
   * @param version Version cible (ex: "1.0.1")
   * @return true si la tâche a été créée, false sinon
   */
  static bool start(const char* model, const char* version);
};

#endif // OTA_MANAGER_H
