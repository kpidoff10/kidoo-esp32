#include "../managers/init/init_manager.h"
#include "../managers/pubnub/pubnub_manager.h"
#include "../managers/wifi/wifi_manager.h"
#include "../../model_config.h"

/**
 * Initialisation du gestionnaire PubNub
 * 
 * Cette fonction initialise le client PubNub et tente de se connecter
 */
bool InitManager::initPubNub() {
#ifndef HAS_PUBNUB
  // PubNub non disponible sur ce modèle
  systemStatus.pubnub = INIT_NOT_STARTED;
  return true;
#else
  if (!HAS_PUBNUB) {
    systemStatus.pubnub = INIT_NOT_STARTED;
    return true;
  }
  systemStatus.pubnub = INIT_IN_PROGRESS;
  Serial.println("[INIT] Initialisation PubNub...");
  
  // Vérifier que les clés sont configurées
  if (strlen(DEFAULT_PUBNUB_SUBSCRIBE_KEY) == 0) {
    Serial.println("[INIT] PubNub: Cles non configurees dans default_config.h");
    systemStatus.pubnub = INIT_FAILED;
    return false;
  }
  
  // Initialiser le client PubNub (même si WiFi n'est pas connecté)
  // Le thread se connectera automatiquement quand le WiFi sera disponible
  if (!PubNubManager::init()) {
    Serial.println("[INIT] PubNub: Echec initialisation");
    systemStatus.pubnub = INIT_FAILED;
    return false;
  }
  
  // Si le WiFi est déjà connecté, tenter de se connecter immédiatement
  if (WiFiManager::isConnected()) {
    if (PubNubManager::connect()) {
      Serial.println("[INIT] PubNub: Connecte");
      systemStatus.pubnub = INIT_SUCCESS;
      return true;
    } else {
      Serial.println("[INIT] PubNub: Echec connexion (retry auto)");
      systemStatus.pubnub = INIT_SUCCESS; // On considère OK car retry auto
      return true;
    }
  } else {
    // WiFi pas encore connecté, mais PubNub est initialisé
    // Il se connectera automatiquement quand le WiFi sera disponible
    Serial.println("[INIT] PubNub: Initialise (en attente WiFi)");
    systemStatus.pubnub = INIT_SUCCESS;
    return true;
  }
#endif
}
