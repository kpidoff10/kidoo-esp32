#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire OTA (mise à jour firmware par parts).
 * Télécharge partCount parts depuis les URLs renvoyées par l'API,
 * écrit dans la partition OTA puis redémarre.
 * En cas d'échec, publie firmware-update-failed sur MQTT.
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
   * À utiliser depuis Serial ou MQTT pour éviter un stack overflow dans la tâche appelante.
   * @param version Version cible (ex: "1.0.1", max 31 caractères)
   * @return true si la tâche a été créée, false sinon (version invalide ou xTaskCreate échoué)
   */
  static bool startUpdateTask(const char* version);

  /**
   * Indique si une mise à jour OTA est en cours (ressources libérées, MQTT déconnecté).
   * À utiliser pour éviter les reconnexions automatiques MQTT pendant l'OTA.
   */
  static bool isOtaInProgress();

  /**
   * Publie l'erreur OTA stockée en NVS au boot (après init MQTT).
   * À appeler depuis InitManager après l'initialisation complète.
   */
  static void publishLastOtaErrorIfAny();
};

#endif // OTA_MANAGER_H
