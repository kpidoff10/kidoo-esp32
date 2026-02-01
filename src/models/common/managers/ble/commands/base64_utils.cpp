/**
 * Utilitaires Base64 pour BLE
 * Fonctions de décodage base64 pour les commandes BLE
 */

#include "base64_utils.h"

// Table de décodage base64
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Décode une string base64
 */
bool decodeBase64(const String& input, char* output, size_t& outputLen) {
  if (input.length() == 0) {
    outputLen = 0;
    return false;
  }
  
  // Table de lookup pour le décodage
  int lookup[256];
  for (int i = 0; i < 256; i++) {
    lookup[i] = -1;
  }
  for (int i = 0; i < 64; i++) {
    lookup[(unsigned char)base64_chars[i]] = i;
  }
  lookup[(unsigned char)'='] = 0;
  
  // Nettoyer la string (enlever les espaces, retours à la ligne, etc.)
  String cleaned = input;
  cleaned.replace(" ", "");
  cleaned.replace("\n", "");
  cleaned.replace("\r", "");
  cleaned.replace("\t", "");
  
  size_t inputLen = cleaned.length();
  if (inputLen == 0) {
    outputLen = 0;
    return false;
  }
  
  // Calculer la taille de sortie
  size_t padding = 0;
  if (inputLen >= 2 && cleaned.charAt(inputLen - 2) == '=') {
    padding = 2;
  } else if (inputLen >= 1 && cleaned.charAt(inputLen - 1) == '=') {
    padding = 1;
  }
  
  size_t decodedLen = (inputLen * 3) / 4 - padding;
  
  if (decodedLen > outputLen) {
    outputLen = decodedLen;
    return false; // Buffer trop petit
  }
  
  // Décoder
  size_t outIdx = 0;
  for (size_t i = 0; i < inputLen; i += 4) {
    if (i + 3 >= inputLen) {
      break;
    }
    
    int enc1 = lookup[(unsigned char)cleaned.charAt(i)];
    int enc2 = lookup[(unsigned char)cleaned.charAt(i + 1)];
    int enc3 = lookup[(unsigned char)cleaned.charAt(i + 2)];
    int enc4 = lookup[(unsigned char)cleaned.charAt(i + 3)];
    
    if (enc1 < 0 || enc2 < 0 || enc3 < 0 || enc4 < 0) {
      // Caractère invalide
      outputLen = outIdx;
      return false;
    }
    
    uint32_t bitmap = (enc1 << 18) | (enc2 << 12) | (enc3 << 6) | enc4;
    
    if (outIdx < decodedLen) {
      output[outIdx++] = (bitmap >> 16) & 0xFF;
    }
    if (outIdx < decodedLen && enc3 != 64) {
      output[outIdx++] = (bitmap >> 8) & 0xFF;
    }
    if (outIdx < decodedLen && enc4 != 64) {
      output[outIdx++] = bitmap & 0xFF;
    }
  }
  
  outputLen = outIdx;
  return true;
}

/**
 * Vérifie si une string est en base64
 */
bool isBase64(const String& str) {
  if (str.length() == 0) {
    return false;
  }
  
  // Vérifier que tous les caractères sont valides pour base64
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (!((c >= 'A' && c <= 'Z') || 
          (c >= 'a' && c <= 'z') || 
          (c >= '0' && c <= '9') || 
          c == '+' || c == '/' || c == '=' ||
          c == ' ' || c == '\n' || c == '\r' || c == '\t')) {
      return false;
    }
  }
  
  // Vérifier la longueur (doit être un multiple de 4, ou proche)
  size_t len = str.length();
  while (len > 0 && (str.charAt(len - 1) == ' ' || str.charAt(len - 1) == '\n' || str.charAt(len - 1) == '\r')) {
    len--;
  }
  
  return len > 0 && (len % 4 == 0 || len % 4 == 1 || len % 4 == 2 || len % 4 == 3);
}
