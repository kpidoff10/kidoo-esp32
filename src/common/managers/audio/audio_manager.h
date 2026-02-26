#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire Audio I2S avec thread FreeRTOS dédié
 * 
 * Ce module gère la lecture audio depuis la carte SD via I2S.
 * L'audio tourne dans un thread FreeRTOS séparé à haute priorité
 * pour éviter les grésillements et garantir un flux audio continu.
 * 
 * Utilise la bibliothèque ESP32-audioI2S
 */

class AudioManager {
public:
  /**
   * Initialiser le gestionnaire audio et démarrer le thread
   * @return true si l'initialisation a réussi
   */
  static bool init();
  
  /**
   * Vérifier si l'audio est disponible
   */
  static bool isAvailable();
  
  /**
   * Mettre à jour le gestionnaire audio
   * Note: Ne fait plus rien car le thread gère tout
   */
  static void loop();
  
  // ============================================
  // Contrôle de lecture
  // ============================================
  
  /**
   * Lire un fichier audio depuis la carte SD
   * @param path Chemin du fichier (ex: "/music/song.mp3")
   * @return true si la lecture a démarré
   */
  static bool play(const char* path);
  
  /**
   * Mettre en pause la lecture
   */
  static void pause();
  
  /**
   * Reprendre la lecture
   */
  static void resume();
  
  /**
   * Arrêter la lecture
   */
  static void stop();
  
  /**
   * Vérifier si une lecture est en cours
   */
  static bool isPlaying();
  
  /**
   * Vérifier si la lecture est en pause
   */
  static bool isPaused();
  
  // ============================================
  // Contrôle du volume (en pourcentage 0-100%)
  // ============================================
  
  /**
   * Définir le volume en pourcentage
   * @param percent Niveau de volume (0 = muet, 100 = max)
   */
  static void setVolume(uint8_t percent);
  
  /**
   * Obtenir le volume actuel en pourcentage
   * @return Volume actuel (0-100%)
   */
  static uint8_t getVolume();
  
  /**
   * Augmenter le volume de 5%
   */
  static void volumeUp();
  
  /**
   * Diminuer le volume de 5%
   */
  static void volumeDown();
  
  // ============================================
  // Informations
  // ============================================
  
  /**
   * Obtenir le fichier en cours de lecture
   * @return Chemin du fichier ou chaîne vide
   */
  static String getCurrentFile();
  
  /**
   * Obtenir la durée totale du fichier (en secondes)
   */
  static uint32_t getDuration();
  
  /**
   * Obtenir la position actuelle (en secondes)
   */
  static uint32_t getPosition();
  
  /**
   * Afficher le statut audio sur Serial
   */
  static void printStatus();

private:
  // Thread FreeRTOS
  static void audioTask(void* parameter);
  static TaskHandle_t audioTaskHandle;
  static volatile bool threadRunning;  // Partagé entre tasks -> volatile
  
  // État
  static bool initialized;
  static volatile bool available;      // Partagé entre tasks -> volatile
  static uint8_t currentVolume;
  static String currentFile;
  static volatile bool paused;         // Partagé entre tasks -> volatile
  
  // Mutex pour la synchronisation thread-safe
  static SemaphoreHandle_t audioMutex;
};

#endif // AUDIO_MANAGER_H
