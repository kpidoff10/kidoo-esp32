#ifndef MODEL_DREAM_CONFIG_SYNC_ROUTES_H
#define MODEL_DREAM_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Routes de synchronisation de configuration pour le modèle Dream
 *
 * Ce fichier définit les fonctions de synchronisation de configuration
 * spécifiques au modèle Dream (bedtime et wakeup)
 *
 * La config-sync s'exécute dans sa propre tâche FreeRTOS avec 16KB de stack
 * pour éviter les débordements lors du parsing JSON
 */

class ModelDreamConfigSyncRoutes {
public:
  /**
   * Appelé automatiquement après une connexion WiFi réussie
   * Lance une tâche FreeRTOS dédiée pour la config-sync
   */
  static void onWiFiConnected();

  /**
   * Réessayer de récupérer la configuration (utilisé après RTC sync)
   * Lance une tâche FreeRTOS dédiée pour la config-sync
   */
  static void retryFetchConfig();

private:
  /**
   * Tâche FreeRTOS pour la config-sync avec stack généreuse (16KB)
   */
  static void configSyncTask(void* param);

  /**
   * Récupérer la configuration depuis l'API serveur via l'adresse MAC
   * @return true si la configuration a été récupérée et sauvegardée avec succès, false sinon
   */
  static bool fetchConfigFromAPI();

  /**
   * Récupérer et appliquer le fuseau horaire depuis l'API serveur
   * Synchronise le RTC avec le fuseau horaire de l'utilisateur
   * @return true si la synchronisation a réussi, false sinon
   */
  static bool fetchAndApplyTimezoneFromAPI();
};

#endif // MODEL_DREAM_CONFIG_SYNC_ROUTES_H
