/**
 * Gestionnaire de clé device Ed25519
 */

#include "device_key_manager.h"

#ifdef HAS_SD

#include <SD.h>
#include <RNG.h>
#include <Ed25519.h>
#include "common/managers/sd/sd_manager.h"
#include "common/managers/ble/commands/base64_utils.h"

static const char* DEVICE_KEY_FILE = "/device_key.bin";

bool DeviceKeyManager::getOrCreatePublicKeyBase64(char* outBuffer, size_t bufferSize) {
  if (!outBuffer || bufferSize < 48) return false;
  if (!SDManager::isAvailable()) return false;

  uint8_t privateKey[32];
  uint8_t publicKey[32];
  bool needGenerate = true;

  if (SD.exists(DEVICE_KEY_FILE)) {
    File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
    if (f && f.size() >= 32) {
      size_t read = f.read(privateKey, 32);
      f.close();
      if (read == 32) {
        needGenerate = false;
      }
    }
  }

  if (needGenerate) {
    RNG.begin("ESP32");
    Ed25519::generatePrivateKey(privateKey);
    File f = SD.open(DEVICE_KEY_FILE, FILE_WRITE);
    if (!f || f.write(privateKey, 32) != 32) {
      if (Serial) Serial.println("[DEVICE-KEY] Erreur: impossible de sauvegarder la cle");
      return false;
    }
    f.close();
    if (Serial) Serial.println("[DEVICE-KEY] Nouvelle paire de cles generee et sauvegardee");
  }

  Ed25519::derivePublicKey(publicKey, privateKey);
  return encodeBase64(publicKey, 32, outBuffer, bufferSize);
}

bool DeviceKeyManager::signMessage(const uint8_t* message, size_t messageLen, uint8_t signatureOut[64]) {
  if (!message || !signatureOut) return false;
  if (!SDManager::isAvailable()) return false;

  uint8_t privateKey[32];
  uint8_t publicKey[32];

  if (!SD.exists(DEVICE_KEY_FILE)) return false;
  File f = SD.open(DEVICE_KEY_FILE, FILE_READ);
  if (!f || f.size() < 32) {
    if (f) f.close();
    return false;
  }
  size_t read = f.read(privateKey, 32);
  f.close();
  if (read != 32) return false;

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
