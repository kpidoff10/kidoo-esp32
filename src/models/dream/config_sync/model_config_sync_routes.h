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
  
private:
  /**
   * Récupérer la configuration depuis l'API serveur via l'adresse MAC
   * @return true si la configuration a été récupérée et sauvegardée avec succès, false sinon
   */
  static bool fetchConfigFromAPI();
};

#endif // MODEL_DREAM_CONFIG_SYNC_ROUTES_H
