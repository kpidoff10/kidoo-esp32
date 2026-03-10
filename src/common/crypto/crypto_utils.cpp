/**
 * Utilitaires cryptographiques pour ESP32
 *
 * Dérivation HKDF + AES-128-CBC pour chiffrer les données sensibles
 * Utilise mbedTLS (inclus dans Arduino ESP32)
 */

#include "crypto_utils.h"
#include "crypto_config.h"
#include <stdint.h>
#include <string.h>

#ifdef HAS_WIFI
#include <mbedtls/aes.h>
#include <mbedtls/cipher.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif

#ifdef HAS_WIFI
/**
 * HMAC-SHA256 manuelle
 * hmac_output doit avoir 32 bytes (SHA256 output size)
 */
static void hmac_sha256(
  const uint8_t* key, size_t keyLen,
  const uint8_t* message, size_t messageLen,
  uint8_t hmac_output[32]
) {
  const uint8_t ipad = 0x36;
  const uint8_t opad = 0x5C;
  const size_t blockSize = 64;  // SHA256 block size

  uint8_t keyPad[blockSize];
  memset(keyPad, 0, blockSize);

  // Si clé > 64 bytes, hash-la d'abord
  if (keyLen > blockSize) {
    mbedtls_sha256((const unsigned char*)key, keyLen, keyPad, 0);
    keyLen = 32;
  } else {
    memcpy(keyPad, key, keyLen);
  }

  // XOR key avec ipad et opad
  uint8_t ipadKey[blockSize];
  uint8_t opadKey[blockSize];
  for (size_t i = 0; i < blockSize; i++) {
    ipadKey[i] = keyPad[i] ^ ipad;
    opadKey[i] = keyPad[i] ^ opad;
  }

  // SHA256((key ^ ipad) || message)
  uint8_t temp[blockSize + 256];  // Max message size
  if (messageLen > 256) return;  // Message trop long

  memcpy(temp, ipadKey, blockSize);
  memcpy(temp + blockSize, message, messageLen);
  uint8_t innerHash[32];
  mbedtls_sha256(temp, blockSize + messageLen, innerHash, 0);

  // SHA256((key ^ opad) || innerHash)
  memcpy(temp, opadKey, blockSize);
  memcpy(temp + blockSize, innerHash, 32);
  mbedtls_sha256(temp, blockSize + 32, hmac_output, 0);
}
#endif

/**
 * Dériver une clé HKDF(HMAC-SHA256) - Implémentation manuelle
 *
 * HKDF = Extract-and-Expand
 * Extract: HMAC-SHA256(salt, IKM) -> PRK
 * Expand: HMAC-SHA256(PRK, info || counter) -> OKM
 */
bool deriveEncryptionKey(
  const uint8_t* macAddress,
  const unsigned char* context,
  size_t contextLen,
  uint8_t* outKey,
  size_t outKeyLen
) {
#ifdef HAS_WIFI
  if (!macAddress || !context || !outKey || outKeyLen == 0) return false;

  // Construire le salt à partir du MAC address
  // Salt = MAC (6 octets) + padding
  uint8_t salt[16] = {0};
  memcpy(salt, macAddress, 6);

  // === ÉTAPE 1: Extract ===
  // PRK = HMAC-SHA256(salt, CRYPTO_MASTER_SECRET)
  uint8_t prk[32];  // SHA256 output = 32 bytes
  hmac_sha256(
    salt, sizeof(salt),
    CRYPTO_MASTER_SECRET, sizeof(CRYPTO_MASTER_SECRET),
    prk
  );

  // === ÉTAPE 2: Expand ===
  // Pour des clés de 16 bytes (AES-128), une itération suffit
  // OKM = HMAC-SHA256(PRK, context || 0x01)
  uint8_t expandInput[256];  // Max: context + counter byte
  uint8_t expandOutput[32];  // HMAC-SHA256 = 32 bytes

  if (contextLen + 1 > 256) return false;
  memcpy(expandInput, context, contextLen);
  expandInput[contextLen] = 0x01;  // Counter byte

  hmac_sha256(
    prk, 32,
    expandInput, contextLen + 1,
    expandOutput
  );

  // Copier la portion de la clé dérivée
  size_t copyLen = (outKeyLen < 32) ? outKeyLen : 32;
  memcpy(outKey, expandOutput, copyLen);

  return true;
#else
  return false;
#endif
}

/**
 * Chiffrer avec AES-128-CBC + padding PKCS7
 */
bool aesEncrypt(
  const uint8_t* plaintext,
  size_t plaintextLen,
  const uint8_t* key,
  const uint8_t* iv,
  uint8_t* ciphertext,
  size_t* ciphertextLen
) {
#ifdef HAS_WIFI
  if (!plaintext || !key || !iv || !ciphertext || !ciphertextLen) return false;

  // Calculer la longueur avec padding PKCS7
  // paddingLen = 16 - (plaintextLen % 16)
  // Si plaintextLen est multiple de 16, paddingLen = 16 (bloc complet de padding)
  uint8_t paddingLen = 16 - (plaintextLen % 16);
  size_t paddedLen = plaintextLen + paddingLen;

  // Préparer le buffer avec padding PKCS7
  uint8_t paddedPlaintext[64];  // Max: 32 + 16 padding
  if (paddedLen > sizeof(paddedPlaintext)) return false;

  memcpy(paddedPlaintext, plaintext, plaintextLen);
  // Remplir le padding avec sa longueur (PKCS7)
  for (int i = 0; i < paddingLen; i++) {
    paddedPlaintext[plaintextLen + i] = paddingLen;
  }

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);

  // AES-128 = 16 bytes key
  int ret = mbedtls_aes_setkey_enc(&ctx, key, 128);
  if (ret != 0) {
    mbedtls_aes_free(&ctx);
    return false;
  }

  // Chiffrer en CBC mode (avec le plaintext padé)
  ret = mbedtls_aes_crypt_cbc(
    &ctx,
    MBEDTLS_AES_ENCRYPT,
    paddedLen,  // Chiffrer la taille totale avec padding
    (unsigned char*)iv,
    paddedPlaintext,
    ciphertext
  );

  mbedtls_aes_free(&ctx);

  if (ret == 0) {
    *ciphertextLen = paddedLen;  // Retourner la taille avec padding
    return true;
  }
  return false;
#else
  return false;
#endif
}

/**
 * Déchiffrer avec AES-128-CBC + remove padding PKCS7
 */
bool aesDecrypt(
  const uint8_t* ciphertext,
  size_t ciphertextLen,
  const uint8_t* key,
  const uint8_t* iv,
  uint8_t* plaintext,
  size_t* plaintextLen
) {
#ifdef HAS_WIFI
  if (!ciphertext || !key || !iv || !plaintext || !plaintextLen) return false;

  // La longueur du ciphertext doit être multiple de 16 (block size AES)
  if (ciphertextLen % 16 != 0) return false;

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);

  // AES-128 = 16 bytes key
  int ret = mbedtls_aes_setkey_dec(&ctx, key, 128);
  if (ret != 0) {
    mbedtls_aes_free(&ctx);
    return false;
  }

  // Déchiffrer en CBC mode
  ret = mbedtls_aes_crypt_cbc(
    &ctx,
    MBEDTLS_AES_DECRYPT,
    ciphertextLen,
    (unsigned char*)iv,
    (unsigned char*)ciphertext,
    plaintext
  );

  mbedtls_aes_free(&ctx);

  if (ret != 0) return false;

  // Vérifier et retirer le padding PKCS7
  // Dernier byte = nombre de bytes de padding
  uint8_t paddingLen = plaintext[ciphertextLen - 1];
  if (paddingLen == 0 || paddingLen > 16) return false;

  // Vérifier que tous les bytes de padding ont la bonne valeur
  for (int i = 0; i < paddingLen; i++) {
    if (plaintext[ciphertextLen - 1 - i] != paddingLen) {
      return false;
    }
  }

  *plaintextLen = ciphertextLen - paddingLen;
  return true;
#else
  return false;
#endif
}

/**
 * Générer un IV aléatoire de 16 octets avec l'RNG mbedtls
 */
bool generateIV(uint8_t iv[16]) {
#ifdef HAS_WIFI
  if (!iv) return false;

  // Utiliser l'RNG de mbedtls seeded par le RNG hardware de l'ESP32
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;

  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
  if (ret != 0) {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return false;
  }

  ret = mbedtls_ctr_drbg_random(&ctr_drbg, iv, 16);

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  return (ret == 0);
#else
  return false;
#endif
}
