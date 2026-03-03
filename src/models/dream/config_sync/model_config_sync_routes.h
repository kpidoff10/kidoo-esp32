#ifndef MODEL_DREAM_CONFIG_SYNC_ROUTES_H
#define MODEL_DREAM_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Routes de synchronisation de configuration pour le modèle Dream
 * 
 * Ce fichier définit les fonctions de synchronisation de configuration
 * spécifiques au modèle Dream (bedtime et wakeup)
 */

class ModelDreamConfigSyncRoutes {
public:
  /**
   * Appelé automatiquement après une connexion WiFi réussie
   * Cette fonction récupère la configuration depuis l'API serveur via l'adresse MAC
   */
  static void onWiFiConnected();

  /**
   * Réessayer de récupérer la configuration (utilisé après RTC sync)
   */
  static void retryFetchConfig();

private:
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
