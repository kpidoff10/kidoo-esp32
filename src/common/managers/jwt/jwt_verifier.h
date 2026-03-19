/**
 * JWT Verifier pour ESP32
 * Vérifie les tokens MQTT commands signés avec HMAC-SHA256 par le serveur
 * Token format: header.payload.signature (JWT standard)
 * Signature: HMAC-SHA256(secret, "header.payload")
 */

#ifndef JWT_VERIFIER_H
#define JWT_VERIFIER_H

#include <cstdint>
#include <cstddef>

/**
 * Claims extraits du payload JWT
 */
struct CmdTokenClaims {
  char kidooMac[16];    // MAC adresse du device (ex: "80B54ED96148")
  char userId[64];      // ID utilisateur
  char action[64];      // Action autorisée
  uint32_t exp;         // Expiration (Unix timestamp)
};

/**
 * Classe pour vérifier les tokens JWT MQTT
 */
class JwtVerifier {
public:
  /**
   * Vérifie un token JWT et extrait les claims
   * @param token - Token JWT complet (header.payload.signature)
   * @param secret - Secret HMAC-SHA256
   * @param claims - Struct pour retourner les claims extraits
   * @return true si le token est valide et non expiré, false sinon
   */
  static bool verify(const char* token, const char* secret, CmdTokenClaims& claims);

private:
  /**
   * Décode un string base64url en bytes
   * @param input - String base64url (sans padding)
   * @param output - Buffer pour les bytes décodés
   * @param outputLen - Longueur du buffer, retourne la longueur décódée
   * @return true si succès, false sinon
   */
  static bool base64urlDecode(const char* input, size_t inputLen, uint8_t* output, size_t& outputLen);

  /**
   * Vérifie la signature HMAC-SHA256 d'un token
   * @param headerPayload - "header.payload" (avant la signature)
   * @param signatureB64 - Signature encodée en base64url
   * @param secret - Secret HMAC-SHA256
   * @return true si la signature est valide, false sinon
   */
  static bool verifyHmac(const char* headerPayload, size_t headerPayloadLen,
                         const char* signatureB64, size_t signatureLenB64,
                         const char* secret);

  /**
   * Extrait les claims du payload JSON
   * @param payload - Payload JSON décodé (null-terminated)
   * @param claims - Struct pour retourner les claims
   * @return true si extraction réussie, false sinon
   */
  static bool extractClaims(const char* payload, CmdTokenClaims& claims);
};

#endif  // JWT_VERIFIER_H
