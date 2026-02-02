#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire OTA (mise à jour firmware par parts).
 * Télécharge partCount parts depuis les URLs renvoyées par l'API,
 * écrit dans la partition OTA puis redémarre.
 * En cas d'échec, publie firmware-update-failed sur PubNub.
 */

class OTAManager {
public:
  /**
   * Lance une mise à jour OTA vers la version donnée.
   * Appelle l'API download (partCount + urls), télécharge chaque part, Update.write(), Update.end(true).
   * @param version Version cible (ex: "1.0.1")
   * @return true si la mise à jour a démarré et va redémarrer (ne retourne pas), false en cas d'erreur (firmware-update-failed publié)
   */
  static bool performUpdate(const char* version);

  /**
   * Lance l'OTA dans une tâche dédiée (pile 12 Ko).
   * À utiliser depuis Serial ou PubNub pour éviter un stack overflow dans la tâche appelante.
   * @param version Version cible (ex: "1.0.1", max 31 caractères)
   * @return true si la tâche a été créée, false sinon (version invalide ou xTaskCreate échoué)
   */
  static bool startUpdateTask(const char* version);
};

#endif // OTA_MANAGER_H
