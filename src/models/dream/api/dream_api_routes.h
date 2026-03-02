/**
 * Routes API spécifiques au modèle Kidoo Dream
 * Appels HTTP vers le serveur (nighttime-alert, etc.)
 */

#ifndef DREAM_API_ROUTES_H
#define DREAM_API_ROUTES_H

class DreamApiRoutes {
public:
  /**
   * Envoyer l'alerte veilleuse au serveur (GET /api/devices/[mac]/nighttime-alert).
   * Le serveur envoie la notification Expo Push.
   * Nécessite HAS_WIFI (no-op sinon).
   * @return true si succès (200), false sinon
   */
  static bool postNighttimeAlert();
};

#endif // DREAM_API_ROUTES_H
