/**
 * Gestionnaire de clé device Ed25519
 * Génère et stocke une paire de clés sur la SD pour l'authentification device.
 * La clé publique est envoyée à l'app lors du setup BLE, puis au serveur.
 */

#ifndef DEVICE_KEY_MANAGER_H
#define DEVICE_KEY_MANAGER_H

#include <Arduino.h>

#ifdef HAS_SD

class DeviceKeyManager {
public:
  /**
   * Vérifier si la clé device existe déjà (sans créer si manquante).
   * @return true si la clé existe sur la SD, false sinon
   */
  static bool keyExists();

  /**
   * Obtenir ou créer la clé publique en base64.
   * Génère une paire si elle n'existe pas, sinon charge depuis la SD.
   * @param outBuffer Buffer de sortie pour la clé base64
   * @param bufferSize Taille du buffer (au moins 48 pour base64 de 32 bytes)
   * @return true si succès, false si SD indisponible ou erreur
   */
  static bool getOrCreatePublicKeyBase64(char* outBuffer, size_t bufferSize);

  /**
   * Signer un message avec la clé privée.
   * @param message Message à signer (UTF-8)
   * @param messageLen Longueur du message
   * @param signatureOut Buffer de sortie pour la signature (64 bytes)
   * @return true si succès
   */
  static bool signMessage(const uint8_t* message, size_t messageLen, uint8_t signatureOut[64]);

  /**
   * Signer un message et retourner la signature en base64.
   * @param message Message à signer (UTF-8)
   * @param messageLen Longueur du message
   * @param signatureBase64Out Buffer de sortie (au moins 88 chars pour base64 de 64 bytes)
   * @param bufferSize Taille du buffer
   * @return true si succès
   */
  static bool signMessageBase64(const uint8_t* message, size_t messageLen, char* signatureBase64Out, size_t bufferSize);
};

#endif // HAS_SD

#endif // DEVICE_KEY_MANAGER_H
