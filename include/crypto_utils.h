#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * Utilitaires cryptographiques pour ESP32
 *
 * - Dérivation de clé HKDF
 * - Chiffrement/déchiffrement AES-128-CBC
 * - Padding PKCS7
 */

/**
 * Dériver une clé de chiffrement à partir du MAC address et du secret master
 *
 * @param macAddress       Adresse MAC (6 octets)
 * @param context          Contexte pour la dérivation (ex: "kidoo-device-key")
 * @param contextLen       Longueur du contexte
 * @param outKey           Clé dérivée de sortie (16 octets pour AES-128)
 * @param outKeyLen        Longueur de la clé de sortie
 * @return true si succès, false sinon
 */
bool deriveEncryptionKey(
  const uint8_t* macAddress,
  const unsigned char* context,
  size_t contextLen,
  uint8_t* outKey,
  size_t outKeyLen
);

/**
 * Chiffrer un buffer avec AES-128-CBC
 *
 * @param plaintext        Données à chiffrer
 * @param plaintextLen     Longueur des données
 * @param key              Clé AES-128 (16 octets)
 * @param iv               Vecteur d'initialisation (16 octets)
 * @param ciphertext       Buffer de sortie (doit être >= plaintextLen + 16 pour le padding)
 * @param ciphertextLen    Longueur du buffer de sortie, rempli avec la longueur réelle
 * @return true si succès, false sinon
 */
bool aesEncrypt(
  const uint8_t* plaintext,
  size_t plaintextLen,
  const uint8_t* key,
  const uint8_t* iv,
  uint8_t* ciphertext,
  size_t* ciphertextLen
);

/**
 * Déchiffrer un buffer avec AES-128-CBC
 *
 * @param ciphertext       Données chiffrées
 * @param ciphertextLen    Longueur des données chiffrées
 * @param key              Clé AES-128 (16 octets)
 * @param iv               Vecteur d'initialisation (16 octets)
 * @param plaintext        Buffer de sortie
 * @param plaintextLen     Longueur du buffer de sortie, rempli avec la longueur réelle
 * @return true si succès, false sinon
 */
bool aesDecrypt(
  const uint8_t* ciphertext,
  size_t ciphertextLen,
  const uint8_t* key,
  const uint8_t* iv,
  uint8_t* plaintext,
  size_t* plaintextLen
);

/**
 * Générer un vecteur d'initialisation (IV) aléatoire de 16 octets
 *
 * @param iv               Buffer IV de sortie (16 octets)
 * @return true si succès, false sinon
 */
bool generateIV(uint8_t iv[16]);

#endif // CRYPTO_UTILS_H
