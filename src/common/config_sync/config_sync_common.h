#ifndef CONFIG_SYNC_COMMON_H
#define CONFIG_SYNC_COMMON_H

#include <Arduino.h>

/**
 * Synchronisation de configuration commune à TOUS les modèles Kidoo
 * Récupère les données partagées (cmdTokenSecret, etc.) via /api/devices/[mac]/config
 */

class ModelConfigSyncCommon {
public:
  /**
   * Récupère et sauvegarde le secret de token MQTT depuis le serveur
   * Appelé automatiquement lors de la connexion WiFi
   * Compatible avec tous les modèles (Dream, Sound)
   */
  static void fetchAndSaveCmdTokenSecret();

  /**
   * Récupère la configuration du serveur (implémentation interne)
   */
  static bool fetchConfigFromAPI();
};

#endif // CONFIG_SYNC_COMMON_H
