#ifndef SSL_CONFIG_H
#define SSL_CONFIG_H

/**
 * Configuration SSL pour ESP32
 *
 * Chaîne de certificats Let's Encrypt pour www.kidoo-box.com:
 * - R12 (Intermediate) : ../certificats/ssl-api/lets-encrypt-r12.h
 * - ISRG Root X1 (Root) : ../certificats/ssl-api/isrg-root-x1.h
 */

// Certificat intermédiaire Let's Encrypt (R12)
#include "../certificats/ssl-api/lets-encrypt-r12.h"

// Certificat CA racine Let's Encrypt (ISRG Root X1)
#include "../certificats/ssl-api/isrg-root-x1.h"

#endif // SSL_CONFIG_H
