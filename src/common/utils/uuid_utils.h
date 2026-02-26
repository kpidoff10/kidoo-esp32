/**
 * Utilitaires UUID
 * Fonctions pour générer des UUID v4 valides basés sur l'identifiant unique de l'ESP32
 */

#ifndef UUID_UTILS_H
#define UUID_UTILS_H

#include <Arduino.h>

/**
 * Génère un UUID v4 basé sur l'identifiant unique de l'ESP32 (MAC address)
 * L'UUID est déterministe pour chaque ESP32 (même UUID à chaque appel)
 * 
 * Format UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 * où y est 8, 9, A, ou B (variant bits)
 * 
 * @param uuid Buffer de sortie pour l'UUID (doit être au moins de 37 caractères)
 * @param uuidSize Taille du buffer (doit être >= 37)
 * @return true si l'UUID a été généré avec succès, false sinon
 */
bool generateUUIDv4(char* uuid, size_t uuidSize);

/**
 * Génère un UUID v4 et le retourne comme String
 * 
 * @return String contenant l'UUID v4 généré
 */
String generateUUIDv4String();

#endif // UUID_UTILS_H
