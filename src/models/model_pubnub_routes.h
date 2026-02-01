#ifndef MODEL_PUBNUB_ROUTES_H
#define MODEL_PUBNUB_ROUTES_H

/**
 * Inclusion des routes PubNub spécifiques au modèle
 * 
 * Ce fichier inclut automatiquement le bon fichier de routes
 * selon le modèle compilé (Basic, Mini, etc.)
 */

#ifdef KIDOO_MODEL_BASIC
  #include "basic/pubnub/model_pubnub_routes.h"
  typedef ModelBasicPubNubRoutes ModelPubNubRoutes;
#elif defined(KIDOO_MODEL_MINI)
  #include "mini/pubnub/model_pubnub_routes.h"
  typedef ModelMiniPubNubRoutes ModelPubNubRoutes;
#elif defined(KIDOO_MODEL_DREAM)
  #include "dream/pubnub/model_pubnub_routes.h"
  typedef ModelDreamPubNubRoutes ModelPubNubRoutes;
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_BASIC, KIDOO_MODEL_MINI ou KIDOO_MODEL_DREAM"
#endif

#endif // MODEL_PUBNUB_ROUTES_H
