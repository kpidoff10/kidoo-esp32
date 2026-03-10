#ifndef CRYPTO_CONFIG_H
#define CRYPTO_CONFIG_H

/**
 * Configuration cryptographique ESP32
 *
 * Secrets pour dériver les clés de chiffrement des données sensibles
 * IMPORTANT: Ne jamais commiter les vrais secrets en production!
 * Ces valeurs sont des placeholders - remplacer en production
 */

// Secret master pour dériver les clés de chiffrement (32 octets = 256 bits)
// Généré avec: openssl rand -hex 32
static const unsigned char CRYPTO_MASTER_SECRET[32] = {
  0x7d, 0xa9, 0xb6, 0x8b, 0x64, 0x41, 0xc4, 0x9f,
  0xbe, 0x9a, 0xb0, 0xce, 0x16, 0x12, 0xb7, 0x85,
  0xc5, 0xa5, 0x9b, 0x89, 0x5f, 0x91, 0xe4, 0x17,
  0xd9, 0x93, 0x84, 0xad, 0x25, 0xfe, 0x04, 0x63
};

// Contexte pour dériver la clé device (identifie le type de clé)
static const unsigned char DEVICE_KEY_CONTEXT[] = "kidoo-device-key";
static const size_t DEVICE_KEY_CONTEXT_LEN = sizeof(DEVICE_KEY_CONTEXT) - 1;

#endif // CRYPTO_CONFIG_H
