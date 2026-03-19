/**
 * JWT Verifier Implementation pour ESP32
 */

#include "jwt_verifier.h"
#include <cstring>
#include <cstdlib>
#include <mbedtls/md.h>
#include <ArduinoJson.h>
#include "common/managers/log/log_manager.h"
#include "common/managers/rtc/rtc_manager.h"

// Lookup table pour base64url decoding
static const uint8_t BASE64URL_DECODE_TABLE[256] = {
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255,
   52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255, 255, 255, 255,
  255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
   15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255,  63,
  255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
   41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

bool JwtVerifier::base64urlDecode(const char* input, size_t inputLen,
                                   uint8_t* output, size_t& outputLen) {
  if (!input || inputLen == 0 || !output) return false;

  size_t outIdx = 0;
  uint32_t buffer = 0;
  int bufBits = 0;

  for (size_t i = 0; i < inputLen; i++) {
    uint8_t c = (uint8_t)input[i];
    if (c == '=' || c == '.') break;  // Stop at padding or delimiter

    uint8_t val = BASE64URL_DECODE_TABLE[c];
    if (val == 255) return false;  // Invalid character

    buffer = (buffer << 6) | val;
    bufBits += 6;

    if (bufBits >= 8) {
      bufBits -= 8;
      if (outIdx >= outputLen) return false;  // Buffer overflow
      output[outIdx++] = (buffer >> bufBits) & 0xFF;
    }
  }

  outputLen = outIdx;
  return true;
}

bool JwtVerifier::verifyHmac(const char* headerPayload, size_t headerPayloadLen,
                              const char* signatureB64, size_t signatureLenB64,
                              const char* secretBase64) {
  if (!headerPayload || !signatureB64 || !secretBase64) return false;

  // Décoder la clé de base64 en bytes binaires
  uint8_t secretBinary[32];
  size_t secretLen = sizeof(secretBinary);
  if (!base64urlDecode(secretBase64, strlen(secretBase64), secretBinary, secretLen)) {
    LogManager::warning("[JWT] Erreur décodage base64 de la clé secrète");
    return false;
  }

  if (secretLen != 32) {
    LogManager::warning("[JWT] Clé décodée invalide (len=%u, expected 32)", secretLen);
    return false;
  }

  // Calculer HMAC-SHA256(secret, headerPayload)
  uint8_t computedHmac[32];  // SHA256 = 32 bytes
  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!mdInfo) return false;

  int ret = mbedtls_md_hmac(mdInfo, secretBinary, secretLen,
                             (const uint8_t*)headerPayload, headerPayloadLen,
                             computedHmac);
  if (ret != 0) return false;

  // Décoder la signature en base64url
  uint8_t providedSignature[32];
  size_t providedSigLen = sizeof(providedSignature);
  if (!base64urlDecode(signatureB64, signatureLenB64, providedSignature, providedSigLen)) {
    return false;
  }

  // Vérifier que la longueur correspond
  if (providedSigLen != 32) return false;

  // Comparaison en temps constant (timing-safe comparison)
  int cmpResult = 0;
  for (size_t i = 0; i < 32; i++) {
    cmpResult |= (computedHmac[i] ^ providedSignature[i]);
  }

  return cmpResult == 0;
}

bool JwtVerifier::extractClaims(const char* payload, CmdTokenClaims& claims) {
  if (!payload) return false;

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    LogManager::warning("[JWT] Erreur parsing JSON payload: %s", error.c_str());
    return false;
  }

  // Extraire kidooMac
  if (!doc["kidooMac"].is<const char*>()) return false;
  const char* mac = doc["kidooMac"];
  strncpy(claims.kidooMac, mac, sizeof(claims.kidooMac) - 1);
  claims.kidooMac[sizeof(claims.kidooMac) - 1] = '\0';

  // Extraire userId
  if (!doc["userId"].is<const char*>()) return false;
  const char* userId = doc["userId"];
  strncpy(claims.userId, userId, sizeof(claims.userId) - 1);
  claims.userId[sizeof(claims.userId) - 1] = '\0';

  // Extraire action
  if (!doc["action"].is<const char*>()) return false;
  const char* action = doc["action"];
  strncpy(claims.action, action, sizeof(claims.action) - 1);
  claims.action[sizeof(claims.action) - 1] = '\0';

  // Extraire exp (expiration timestamp)
  if (!doc["exp"].is<uint32_t>()) return false;
  claims.exp = doc["exp"];

  return true;
}

bool JwtVerifier::verify(const char* token, const char* secret, CmdTokenClaims& claims) {
  if (!token || !secret) return false;

  LogManager::debug("[JWT] Vérification du token...");

  // Trouver les deux points séparant header.payload.signature
  const char* firstDot = strchr(token, '.');
  if (!firstDot) return false;

  const char* secondDot = strchr(firstDot + 1, '.');
  if (!secondDot) return false;

  // Extraire les tailles
  size_t headerPayloadLen = secondDot - token;
  const char* signatureStart = secondDot + 1;

  // Vérifier la signature HMAC
  if (!verifyHmac(token, headerPayloadLen, signatureStart, strlen(signatureStart), secret)) {
    LogManager::warning("[JWT] Signature HMAC invalide");
    return false;
  }

  // Décoder le payload (partie entre les deux points)
  const char* payloadStart = firstDot + 1;
  size_t payloadLen = secondDot - payloadStart;

  uint8_t payloadBytes[512];
  size_t payloadBytesLen = sizeof(payloadBytes);
  if (!base64urlDecode(payloadStart, payloadLen, payloadBytes, payloadBytesLen)) {
    LogManager::warning("[JWT] Erreur base64url decode du payload");
    return false;
  }

  // Ajouter null terminator
  if (payloadBytesLen >= sizeof(payloadBytes)) payloadBytesLen = sizeof(payloadBytes) - 1;
  payloadBytes[payloadBytesLen] = '\0';

  // Extraire les claims
  if (!extractClaims((const char*)payloadBytes, claims)) {
    LogManager::warning("[JWT] Erreur extraction des claims");
    return false;
  }

  // Vérifier que le token n'est pas expiré
  uint32_t now = RTCManager::getUnixTime();
  if (now > claims.exp) {
    LogManager::warning("[JWT] Token expiré (exp=%lu, now=%lu)", claims.exp, now);
    return false;
  }

  LogManager::debug("[JWT] Token valide pour %s (action: %s)", claims.kidooMac, claims.action);
  return true;
}
