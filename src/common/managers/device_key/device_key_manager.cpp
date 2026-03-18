/**
 * Gestionnaire de clé device Ed25519
 * Stockage chiffré AES-128-CBC sur SD card
 */

#include "device_key_manager.h"

#ifdef HAS_SD

#include <SD.h>
#include <Ed25519.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "common/managers/sd/sd_manager.h"
#include "common/managers/ble/commands/base64_utils.h"
#include "crypto_utils.h"
#include "crypto_config.h"

static const char* DEVICE_KEY_FILE = "/device_key.bin";

bool DeviceKeyManager::keyExists() {
  if (!SDManager::isAvailable()) {
    if (Serial) Serial.println("[DEVICE-KEY] SD Manager non disponible");
    return false;
  }

  // Vérifier si le fichier de clé existe et est accessible
  if (!SD.exists(DEVICE_KEY_FILE)) {
    if (Serial) Serial.println("[DEVICE-KEY] Fichier /device_key.bin n'existe pas");
    return false;
  }

  if (Serial) Serial.println("[DEVICE-KEY] Fichier existe, tentative d'ouverture...");

  // Vérifier que le fichier peut être ouvert et a une taille valide
  File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
  if (!f) {
    if (Serial) Serial.println("[DEVICE-KEY] Impossible d'ouvrir le fichier");
    return false;
  }

  size_t fileSize = f.size();
  f.close();

  if (Serial) Serial.printf("[DEVICE-KEY] Taille du fichier: %d bytes\n", fileSize);

  // Format: 16-byte IV + 48-byte encrypted = 64 bytes minimum
  bool exists = (fileSize >= 64);
  if (Serial) Serial.printf("[DEVICE-KEY] Clé existe: %s\n", exists ? "OUI" : "NON");
  return exists;
}

bool DeviceKeyManager::getOrCreatePublicKeyBase64(char* outBuffer, size_t bufferSize) {
  if (!outBuffer || bufferSize < 48) return false;
  if (!SDManager::isAvailable()) return false;

  uint8_t privateKey[32];
  uint8_t publicKey[32];
  uint8_t encryptionKey[16];
  uint8_t iv[16];
  uint8_t macAddress[6];
  bool needGenerate = true;

  // Récupérer l'adresse MAC HARDWARE (stable)
  esp_wifi_get_mac(WIFI_IF_STA, macAddress);

  // Dériver la clé de chiffrement depuis le MAC address
  if (!deriveEncryptionKey(macAddress, DEVICE_KEY_CONTEXT, DEVICE_KEY_CONTEXT_LEN, encryptionKey, 16)) {
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de dériver la clé de chiffrement");
    return false;
  }

  // Tenter de lire la clé chiffrée depuis la SD
  // Format: [16-byte IV][48-byte encrypted private key] = 64 bytes total
  if (SD.exists(DEVICE_KEY_FILE)) {
    File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
    if (f && f.size() >= 64) {
      size_t read = f.read(iv, 16);
      if (read == 16) {
        uint8_t encryptedData[48];
        read = f.read(encryptedData, 48);
        f.close();
        if (read == 48) {
          size_t decryptedLen = 0;
          if (aesDecrypt(encryptedData, 48, encryptionKey, iv, privateKey, &decryptedLen)) {
            if (decryptedLen == 32) {
              needGenerate = false;
            }
          }
        }
      } else {
        f.close();
      }
    } else if (f) {
      f.close();
    }
  }

  // Générer une nouvelle paire de clés si nécessaire
  if (needGenerate) {
    // Le RNG hardware de l'ESP32 est utilisé automatiquement par la librairie Ed25519
    Ed25519::generatePrivateKey(privateKey);

    // Générer un IV aléatoire
    if (!generateIV(iv)) {
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de générer l'IV");
      return false;
    }

    // Chiffrer la clé privée avec PKCS7 padding
    uint8_t encryptedData[48];  // 32 bytes + 16 bytes PKCS7 padding
    size_t encryptedLen = 48;
    if (!aesEncrypt(privateKey, 32, encryptionKey, iv, encryptedData, &encryptedLen)) {
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de chiffrer la clé");
      return false;
    }

    if (encryptedLen != 48) {
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: taille de chiffrement incorrecte");
      return false;
    }

    // Sauvegarder IV + données chiffrées sur SD
    // Supprimer l'ancien fichier (FILE_WRITE = append, pas truncate)
    if (SD.exists(DEVICE_KEY_FILE)) {
      if (!SD.remove(DEVICE_KEY_FILE)) {
        if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de supprimer l'ancien fichier");
        return false;
      }
    }

    // Buffer de 64 bytes: 16 IV + 48 encrypted
    uint8_t fileBuffer[64];
    memcpy(fileBuffer, iv, 16);
    memcpy(fileBuffer + 16, encryptedData, 48);

    File f = SD.open(DEVICE_KEY_FILE, FILE_WRITE);
    if (!f) {
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible d'ouvrir le fichier SD");
      return false;
    }

    if (f.write(fileBuffer, 64) != 64) {
      f.close();
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de sauvegarder la clé chiffrée");
      return false;
    }
    f.flush();
    f.close();

    if (Serial) Serial.println("[DEVICE-KEY] Nouvelle paire de cles generee et sauvegardee (chiffree)");
  }

  Ed25519::derivePublicKey(publicKey, privateKey);
  return encodeBase64(publicKey, 32, outBuffer, bufferSize);
}

bool DeviceKeyManager::getPublicKeyBase64(char* outBuffer, size_t bufferSize) {
  if (!outBuffer || bufferSize < 48) return false;
  if (!SDManager::isAvailable()) return false;
  if (!SD.exists(DEVICE_KEY_FILE)) return false;

  uint8_t privateKey[32];
  uint8_t publicKey[32];
  uint8_t encryptionKey[16];
  uint8_t iv[16];
  uint8_t macAddress[6];

  // Récupérer l'adresse MAC HARDWARE (stable)
  esp_wifi_get_mac(WIFI_IF_STA, macAddress);

  // Dériver la clé de chiffrement depuis le MAC address
  if (!deriveEncryptionKey(macAddress, DEVICE_KEY_CONTEXT, DEVICE_KEY_CONTEXT_LEN, encryptionKey, 16)) {
    return false;
  }

  // Lire la clé chiffrée depuis la SD
  File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
  if (!f || f.size() < 64) {
    if (f) f.close();
    return false;
  }

  size_t read = f.read(iv, 16);
  if (read != 16) {
    f.close();
    return false;
  }

  uint8_t encryptedData[48];
  read = f.read(encryptedData, 48);
  f.close();

  if (read != 48) {
    return false;
  }

  // Déchiffrer la clé privée
  size_t decryptedLen = 0;
  if (!aesDecrypt(encryptedData, 48, encryptionKey, iv, privateKey, &decryptedLen)) {
    return false;
  }

  if (decryptedLen != 32) {
    return false;
  }

  // Dériver et encoder la clé publique
  Ed25519::derivePublicKey(publicKey, privateKey);
  return encodeBase64(publicKey, 32, outBuffer, bufferSize);
}

bool DeviceKeyManager::signMessage(const uint8_t* message, size_t messageLen, uint8_t signatureOut[64]) {
  if (!message || !signatureOut) return false;
  if (!SDManager::isAvailable()) return false;

  uint8_t privateKey[32];
  uint8_t publicKey[32];
  uint8_t encryptionKey[16];
  uint8_t iv[16];
  uint8_t macAddress[6];

  if (!SD.exists(DEVICE_KEY_FILE)) return false;

  // Récupérer l'adresse MAC HARDWARE (stable, pas WiFi.macAddress() qui peut varier)
  esp_wifi_get_mac(WIFI_IF_STA, macAddress);
  if (Serial) Serial.printf("[DEVICE-KEY] signMessage MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
    macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);

  // Dériver la clé de chiffrement depuis le MAC address HARDWARE
  if (!deriveEncryptionKey(macAddress, DEVICE_KEY_CONTEXT, DEVICE_KEY_CONTEXT_LEN, encryptionKey, 16)) {
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de dériver la clé de chiffrement");
    return false;
  }

  // Lire et déchiffrer la clé privée
  // Format: [16-byte IV][48-byte encrypted private key] = 64 bytes total
  File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
  if (!f || f.size() < 64) {
    if (f) f.close();
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: fichier clé invalide ou manquant");
    return false;
  }

  size_t read = f.read(iv, 16);
  if (read != 16) {
    f.close();
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de lire l'IV");
    return false;
  }

  uint8_t encryptedData[48];
  read = f.read(encryptedData, 48);
  f.close();
  if (read != 48) {
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de lire les données chiffrées");
    return false;
  }

  // Déchiffrer la clé privée
  size_t decryptedLen = 0;
  if (!aesDecrypt(encryptedData, 48, encryptionKey, iv, privateKey, &decryptedLen)) {
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de déchiffrer la clé");
    return false;
  }
  if (decryptedLen != 32) {
    if (Serial) Serial.println("[DEVICE-KEY] Erreur: longueur de clé invalide après déchiffrement");
    return false;
  }

  Ed25519::derivePublicKey(publicKey, privateKey);
  Ed25519::sign(signatureOut, privateKey, publicKey, message, messageLen);
  return true;
}

bool DeviceKeyManager::signMessageBase64(const uint8_t* message, size_t messageLen, char* signatureBase64Out, size_t bufferSize) {
  if (!signatureBase64Out || bufferSize < 88) return false;

  uint8_t signature[64];
  if (!signMessage(message, messageLen, signature)) return false;

  return encodeBase64(signature, 64, signatureBase64Out, bufferSize);
}

#endif // HAS_SD
