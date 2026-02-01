#include "ble_command_handler.h"
#include <ArduinoJson.h>
#include "setup/setup_command.h"
#include "base64_utils.h"
#include "../ble_manager.h"
#include "../../../../model_config.h"
#include "../../wifi/wifi_manager.h"
#include "../../../utils/uuid_utils.h"
#include "../../../utils/mac_utils.h"
#include "../../led/led_manager.h"
#include "../../ble_config/ble_config_manager.h"
#include "../../init/init_manager.h"
#include "../../sd/sd_manager.h"  // Pour la définition complète de SDConfig
#include "../../../config/default_config.h"  // Pour FIRMWARE_VERSION
#include <ESP.h>

#ifdef HAS_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Référence à la caractéristique TX pour envoyer des réponses
static BLECharacteristic* pTxCharacteristic = nullptr;

/**
 * Fonction utilitaire pour faire clignoter 2 fois en vert avec fade (dégradé de luminosité)
 * @param numBlinks Nombre de clignotements (par défaut 2)
 * @param fadeDurationMs Durée du fade en millisecondes (par défaut 200ms)
 */
void blinkGreenWithFade(int numBlinks = 2, int fadeDurationMs = 200) {
  #ifdef HAS_LED
  if (!LEDManager::isInitialized()) {
    return;
  }
  
  // Sauvegarder la luminosité actuelle
  uint8_t savedBrightness = LEDManager::getCurrentBrightness();
  
  // Étape 1: Arrêter l'effet RAINBOW d'abord et attendre qu'il soit complètement arrêté
  // Cela évite le flash bleu/arc-en-ciel pendant la transition
  Serial.println("[BLE-COMMAND] Arret de l'effet RAINBOW...");
  LEDManager::setEffect(LED_EFFECT_NONE);
  delay(300);  // Attendre suffisamment longtemps pour que RAINBOW soit complètement arrêté
  
  // Étape 2: S'assurer que toutes les LEDs sont éteintes avant de définir la couleur verte
  LEDManager::clear();
  delay(100);  // Laisser le temps au thread LED de traiter
  
  // Étape 3: Définir la couleur verte maintenant que RAINBOW est complètement arrêté
  Serial.println("[BLE-COMMAND] Definition de la couleur verte...");
  LEDManager::setColor(0, 255, 0);  // Vert
  delay(150);  // Laisser le temps au thread LED de traiter et d'appliquer la couleur
  
  // Étape 4: Mettre la luminosité à 0 pour commencer le fade in
  Serial.println("[BLE-COMMAND] Debut du clignotement vert avec fade...");
  LEDManager::setBrightness(0);
  delay(100);  // Attendre que la luminosité soit bien à 0
  
  // Faire clignoter avec fade en modifiant la luminosité
  for (int i = 0; i < numBlinks; i++) {
    // Fade in (augmenter la luminosité progressivement)
    for (int brightness = 0; brightness <= 255; brightness += 5) {
      LEDManager::setBrightness(brightness);
      delay(fadeDurationMs / 50);  // Diviser par 50 pour avoir ~50 étapes
    }
    
    // Maintenir à la luminosité maximale brièvement
    delay(100);
    
    // Fade out (diminuer la luminosité progressivement)
    for (int brightness = 255; brightness >= 0; brightness -= 5) {
      LEDManager::setBrightness(brightness);
      delay(fadeDurationMs / 50);
    }
    
    // Pause entre les clignotements (sauf pour le dernier)
    if (i < numBlinks - 1) {
      delay(150);
    }
  }
  
  // Restaurer la luminosité originale
  LEDManager::setBrightness(savedBrightness);
  
  // Éteindre les LEDs
  LEDManager::clear();
  Serial.println("[BLE-COMMAND] LEDs eteintes apres clignotement vert");
  #endif
}

void BLECommandHandler::sendResponse(bool success, const String& message) {
  if (pTxCharacteristic == nullptr) {
    Serial.println("[BLE-COMMAND] Erreur: Caracteristique TX non initialisee");
    return;
  }
  
  // Créer la réponse JSON
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<256> doc;
  #pragma GCC diagnostic pop
  doc["success"] = success;
  doc["message"] = message;
  
  String response;
  serializeJson(doc, response);
  
  // Envoyer la réponse via BLE
  pTxCharacteristic->setValue(response.c_str());
  pTxCharacteristic->notify();
  
  Serial.print("[BLE-COMMAND] Reponse envoyee: ");
  Serial.println(response);
}

bool BLECommandHandler::handleCommand(const String& data) {
  Serial.println("[BLE-COMMAND] ========================================");
  Serial.println("[BLE-COMMAND] >>> handleCommand APPELE <<<");
  Serial.print("[BLE-COMMAND] Longueur des donnees: ");
  Serial.println(data.length());
  
  if (data.length() == 0) {
    Serial.println("[BLE-COMMAND] Erreur: Donnees vides");
    sendResponse(false, "Donnees vides");
    return false;
  }
  
  Serial.println("[BLE-COMMAND] >>> TRAITEMENT DE LA COMMANDE <<<");
  Serial.print("[BLE-COMMAND] Donnees recues (");
  Serial.print(data.length());
  Serial.println(" caracteres):");
  Serial.println(data);
  
  // Décoder le base64 si nécessaire
  String jsonData = data;
  if (isBase64(data)) {
    Serial.println("[BLE-COMMAND] Detection: donnees en base64, decodage...");
    
    char decodedBuffer[512];
    size_t decodedLen = sizeof(decodedBuffer);
    
    if (decodeBase64(data, decodedBuffer, decodedLen)) {
      jsonData = String(decodedBuffer, decodedLen);
      Serial.print("[BLE-COMMAND] Donnees decodees (");
      Serial.print(decodedLen);
      Serial.println(" octets):");
      Serial.println(jsonData);
    } else {
      Serial.println("[BLE-COMMAND] ERREUR: Impossible de decoder le base64");
      sendResponse(false, "Erreur decodage base64");
      return false;
    }
  } else {
    Serial.println("[BLE-COMMAND] Detection: donnees en JSON direct");
  }
  
  // Parser le JSON pour identifier la commande
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<512> doc;
  #pragma GCC diagnostic pop
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.print("[BLE-COMMAND] ERREUR parsing JSON: ");
    Serial.println(error.c_str());
    Serial.println("[BLE-COMMAND] ========================================");
    sendResponse(false, "JSON invalide");
    return false;
  }
  
  // Vérifier que le champ "command" existe
  if (!doc["command"].is<String>()) {
    Serial.println("[BLE-COMMAND] Erreur: Champ 'command' manquant");
    sendResponse(false, "Champ 'command' manquant");
    return false;
  }
  
  String command = doc["command"] | "";
  command.toLowerCase();
  command.trim();
  
  Serial.print("[BLE-COMMAND] Commande identifiee: '");
  Serial.print(command);
  Serial.println("'");
  
  // Router vers le handler approprié
  if (command == "setup") {
    Serial.println("[BLE-COMMAND] Routage vers BLESetupCommand...");
    if (BLESetupCommand::isValid(jsonData)) {
      Serial.println("[BLE-COMMAND] Commande 'setup' valide, execution...");
      bool success = BLESetupCommand::execute(jsonData);
      
      // Vérifier le statut WiFi après l'exécution
      #ifdef HAS_WIFI
      bool wifiConnected = WiFiManager::isConnected();
      #else
      bool wifiConnected = false;
      #endif
      
      if (success) {
        Serial.println("[BLE-COMMAND] Commande 'setup' executee avec succes");
        Serial.print("[BLE-COMMAND] WiFi connecte: ");
        Serial.println(wifiConnected ? "Oui" : "Non");
        Serial.println("[BLE-COMMAND] ========================================");
        
        // Envoyer la réponse avec le statut WiFi et l'UUID du device
        // Si la configuration est sauvegardée mais que WiFi n'est pas connecté, c'est un échec partiel
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<512> responseDoc;
        #pragma GCC diagnostic pop
        
        // Générer un UUID v4 basé sur l'identifiant unique de l'ESP32 (MAC address)
        char uuid[37];
        if (!generateUUIDv4(uuid, sizeof(uuid))) {
          Serial.println("[BLE-COMMAND] ERREUR: Impossible de generer l'UUID");
          // En cas d'erreur, utiliser un UUID par défaut (ne devrait jamais arriver)
          strcpy(uuid, "00000000-0000-4000-8000-000000000000");
        }
        
        // Récupérer la configuration depuis la SD card
        const SDConfig& config = InitManager::getConfig();
        
        // Récupérer la version du firmware Kidoo (définie dans default_config.h)
        String firmwareVersion = FIRMWARE_VERSION;
        
        // Récupérer l'adresse MAC WiFi (utilisée pour PubNub)
        // Sur ESP32-C3, BLE et WiFi ont des adresses MAC différentes
        // IMPORTANT: Utiliser EXACTEMENT la même méthode que PubNub pour garantir la cohérence
        #ifdef HAS_WIFI
        char macStr[18];
        if (!getMacAddressString(macStr, sizeof(macStr), ESP_MAC_WIFI_STA)) {
          strcpy(macStr, "00:00:00:00:00:00"); // Valeur par défaut en cas d'erreur
        }
        Serial.print("[BLE-COMMAND] Adresse MAC WiFi (pour PubNub): ");
        Serial.println(macStr);
        #else
        const char* macStr = "";
        #endif
        
        if (wifiConnected) {
          // Succès complet : configuration sauvegardée ET WiFi connecté
          responseDoc["success"] = true;
          responseDoc["message"] = "Configuration WiFi sauvegardee et connexion reussie";
        } else {
          // Échec : configuration sauvegardée mais WiFi non connecté
          responseDoc["success"] = false;
          responseDoc["message"] = "Configuration WiFi sauvegardee mais connexion echouee";
        }
        responseDoc["wifiConnected"] = wifiConnected;
        responseDoc["deviceId"] = uuid; // UUID unique du device
        #ifdef HAS_WIFI
        responseDoc["macAddress"] = macStr; // Adresse MAC WiFi (utilisée pour PubNub)
        #endif
        
        // Ajouter les informations de configuration depuis la SD card
        // Brightness en pourcentage (0-100) au lieu de 0-255
        uint8_t brightnessPercent = (config.led_brightness * 100 + 127) / 255; // Arrondi correct
        responseDoc["brightness"] = brightnessPercent;
        responseDoc["sleepTimeout"] = config.sleep_timeout_ms;
        responseDoc["firmwareVersion"] = firmwareVersion;
        
        String responseJson;
        serializeJson(responseDoc, responseJson);
        
        if (pTxCharacteristic != nullptr) {
          pTxCharacteristic->setValue(responseJson.c_str());
          pTxCharacteristic->notify();
          Serial.print("[BLE-COMMAND] Reponse envoyee: ");
          Serial.println(responseJson);
          
          // Maintenant que la réponse est envoyée, gérer les LEDs et désactiver le BLE si WiFi connecté
          #ifdef HAS_LED
          if (LEDManager::isInitialized()) {
            if (wifiConnected) {
              // Succès : clignoter 2 fois en vert avec fade puis éteindre
              // La fonction blinkGreenWithFade va arrêter RAINBOW et gérer tout
              Serial.println("[BLE-COMMAND] Clignotement vert (succes)");
              blinkGreenWithFade(2, 200);  // 2 clignotements, fade de 200ms
              
              // Désactiver le BLE immédiatement après un setup réussi
              // Le WiFi est connecté, le BLE n'est plus nécessaire
              #ifdef HAS_BLE
              #ifdef BLE_CONFIG_BUTTON_PIN
              if (BLEConfigManager::isInitialized() && BLEConfigManager::isBLEEnabled()) {
                Serial.println("[BLE-COMMAND] Setup reussi - Desactivation du BLE");
                // Petit délai pour s'assurer que la réponse BLE est bien envoyée avant de désactiver
                delay(500);
                BLEConfigManager::disableBLE();
              }
              #endif
              #endif
            } else {
              // Échec : arrêter RAINBOW et afficher rouge
              Serial.println("[BLE-COMMAND] Effet respiration rouge (echec WiFi)");
              LEDManager::setEffect(LED_EFFECT_NONE);
              delay(50);  // Laisser le temps au thread LED de traiter
              LEDManager::setColor(255, 0, 0);  // Rouge
              delay(50);  // Laisser le temps au thread LED de traiter
              LEDManager::setEffect(LED_EFFECT_PULSE);  // Effet de respiration
            }
          }
          #endif
        }
      } else {
        Serial.println("[BLE-COMMAND] ERREUR: Echec de l'execution de 'setup'");
        Serial.println("[BLE-COMMAND] ========================================");
        sendResponse(false, "Erreur lors de la configuration WiFi");
        
        // Arrêter RAINBOW et afficher rouge en cas d'erreur
        #ifdef HAS_LED
        if (LEDManager::isInitialized()) {
          LEDManager::setEffect(LED_EFFECT_NONE);
          LEDManager::setColor(255, 0, 0);  // Rouge
          LEDManager::setEffect(LED_EFFECT_PULSE);  // Effet de respiration
          Serial.println("[BLE-COMMAND] Effet respiration rouge (echec)");
        }
        #endif
      }
      return success && wifiConnected; // Retourner true seulement si tout est OK
    } else {
      Serial.println("[BLE-COMMAND] ERREUR: Commande 'setup' invalide");
      Serial.println("[BLE-COMMAND] ========================================");
      sendResponse(false, "Commande 'setup' invalide");
      return false;
    }
  } else {
    Serial.print("[BLE-COMMAND] ERREUR: Commande inconnue '");
    Serial.print(command);
    Serial.println("'");
    Serial.println("[BLE-COMMAND] ========================================");
    sendResponse(false, "Commande inconnue: " + command);
    return false;
  }
}

// Fonction pour initialiser le handler (appelée depuis BLEManager)
void BLECommandHandler::init(void* txCharacteristic) {
  pTxCharacteristic = static_cast<BLECharacteristic*>(txCharacteristic);
}

#else
// Stubs si BLE non disponible
void BLECommandHandler::sendResponse(bool success, const String& message) {
  // Rien à faire
}

bool BLECommandHandler::handleCommand(const String& data) {
  return false;
}

void BLECommandHandler::init(void* txCharacteristic) {
  // Rien à faire
}
#endif
