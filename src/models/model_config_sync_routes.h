#ifndef MODEL_CONFIG_SYNC_ROUTES_H
#define MODEL_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Inclusion des routes de synchronisation de configuration spécifiques au modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#ifdef KIDOO_MODEL_DREAM
  #include "dream/config_sync/model_config_sync_routes.h"
  typedef ModelDreamConfigSyncRoutes ModelConfigSyncRoutes;
#elif defined(KIDOO_MODEL_GOTCHI)
  class ModelGotchiConfigSyncRoutes {
  public:
    static void onWiFiConnected() {}
  };
  typedef ModelGotchiConfigSyncRoutes ModelConfigSyncRoutes;
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_*"
#endif

#endif // MODEL_CONFIG_SYNC_ROUTES_H
