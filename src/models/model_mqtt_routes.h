#ifndef MODEL_mqtt_ROUTES_H
#define MODEL_mqtt_ROUTES_H

/**
 * Inclusion des routes mqtt spécifiques au modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#ifdef KIDOO_MODEL_DREAM
  #include "dream/mqtt/model_mqtt_routes.h"
  typedef ModelDreamMqttRoutes ModelMqttRoutes;
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/mqtt/model_mqtt_routes.h"
  typedef ModelGotchiMqttRoutes ModelMqttRoutes;
#elif defined(KIDOO_MODEL_SOUND)
  #include "sound/mqtt/model_mqtt_routes.h"
  typedef ModelSoundMqttRoutes ModelMqttRoutes;
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_*"
#endif

#endif // MODEL_mqtt_ROUTES_H
