#ifndef MODEL_CONFIG_SYNC_ROUTES_H
#define MODEL_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Inclusion des routes de synchronisation de configuration spécifiques au modèle
 * 
 * Ce fichier inclut automatiquement le bon fichier de routes
 * selon le modèle compilé (Basic, Mini, Dream, etc.)
 */

#ifdef KIDOO_MODEL_BASIC
  // Classe vide pour Basic (pas de synchronisation de config)
  class ModelBasicConfigSyncRoutes {
  public:
    static void onWiFiConnected() {
      // Rien à faire pour Basic
    }
  };
  typedef ModelBasicConfigSyncRoutes ModelConfigSyncRoutes;
#elif defined(KIDOO_MODEL_MINI)
  // Classe vide pour Mini (pas de synchronisation de config)
  class ModelMiniConfigSyncRoutes {
  public:
    static void onWiFiConnected() {
      // Rien à faire pour Mini
    }
  };
  typedef ModelMiniConfigSyncRoutes ModelConfigSyncRoutes;
#elif defined(KIDOO_MODEL_DREAM)
  #include "dream/config_sync/model_config_sync_routes.h"
  typedef ModelDreamConfigSyncRoutes ModelConfigSyncRoutes;
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_BASIC, KIDOO_MODEL_MINI ou KIDOO_MODEL_DREAM"
#endif

#endif // MODEL_CONFIG_SYNC_ROUTES_H
