/**
 * Gestionnaire des requêtes HTTP vers l'API Kidoo
 * Centralise les appels (POST/GET) pour éviter les redéclarations.
 */

#ifndef API_MANAGER_H
#define API_MANAGER_H

#include <Arduino.h>

#ifdef HAS_WIFI

class ApiManager {
public:
  /**
   * POST JSON vers un endpoint de l'API
   * @param path Chemin relatif (ex: "/api/devices/AABBCCDDEEFF/nighttime-alert")
   * @param body Corps JSON (optionnel pour GET)
   * @param timeoutMs Timeout en ms (défaut: 5000)
   * @return Code HTTP (200 = OK, -1 = erreur, etc.)
   */
  static int postJson(const char* path, const char* body, int timeoutMs = 5000);

  /**
   * GET vers un endpoint avec authentification device (signature Ed25519).
   * Ajoute les headers x-kidoo-timestamp et x-kidoo-signature.
   * @param path Chemin complet (ex: "/api/devices/AABBCCDDEEFF/nighttime-alert")
   * @param timeoutMs Timeout en ms
   * @return Code HTTP
   */
  static int getJsonWithDeviceAuth(const char* path, int timeoutMs = 5000);

  /**
   * GET avec device auth, retourne le corps de la réponse.
   * @param path Chemin complet
   * @param responseBody Buffer pour le corps (optionnel, peut être nullptr)
   * @param timeoutMs Timeout en ms
   * @return Code HTTP
   */
  static int getJsonWithDeviceAuth(const char* path, String* responseBody, int timeoutMs = 5000);
};

#endif // HAS_WIFI

#endif // API_MANAGER_H
