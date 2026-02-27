/**
 * Gestionnaire des requêtes HTTP vers l'API Kidoo
 * Centralise les appels (POST/GET) pour éviter les redéclarations.
 */

#ifndef API_MANAGER_H
#define API_MANAGER_H

#ifdef HAS_WIFI

class ApiManager {
public:
  /**
   * POST JSON vers un endpoint de l'API
   * @param path Chemin relatif (ex: "/api/device/nighttime-alert")
   * @param body Corps JSON (ex: "{\"mac\":\"AA:BB:CC:DD:EE:FF\"}")
   * @param timeoutMs Timeout en ms (défaut: 5000)
   * @return Code HTTP (200 = OK, -1 = erreur, etc.)
   */
  static int postJson(const char* path, const char* body, int timeoutMs = 5000);
};

#endif // HAS_WIFI

#endif // API_MANAGER_H
