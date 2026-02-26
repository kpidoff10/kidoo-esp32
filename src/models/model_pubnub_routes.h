#ifndef MODEL_PUBNUB_ROUTES_H
#define MODEL_PUBNUB_ROUTES_H

/**
 * Inclusion des routes PubNub spécifiques au modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#ifdef KIDOO_MODEL_DREAM
  #include "dream/pubnub/model_pubnub_routes.h"
  typedef ModelDreamPubNubRoutes ModelPubNubRoutes;
#elif defined(KIDOO_MODEL_GOTCHI)
  #include "gotchi/pubnub/model_pubnub_routes.h"
  typedef ModelGotchiPubNubRoutes ModelPubNubRoutes;
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_*"
#endif

#endif // MODEL_PUBNUB_ROUTES_H
