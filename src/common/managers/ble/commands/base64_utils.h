/**
 * Utilitaires Base64 pour BLE
 * Fonctions de décodage base64 pour les commandes BLE
 */

#ifndef BASE64_UTILS_H
#define BASE64_UTILS_H

#include <Arduino.h>

/**
 * Décode une string base64
 * @param input String base64 à décoder
 * @param output Buffer de sortie (doit être assez grand)
 * @param outputLen Taille du buffer de sortie (sera mis à jour avec la taille réelle)
 * @return true si le décodage a réussi, false sinon
 */
bool decodeBase64(const String& input, char* output, size_t& outputLen);

/**
 * Vérifie si une string est en base64
 * @param str String à vérifier
 * @return true si la string semble être en base64
 */
bool isBase64(const String& str);

#endif // BASE64_UTILS_H
