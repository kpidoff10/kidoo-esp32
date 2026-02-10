#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <Arduino.h>
#include <vector>

/**
 * Gestionnaire d'émotions basé sur des vidéos MJPEG et configuration JSON
 *
 * Structure des fichiers:
 * - /config.json : contient characterId
 * - /characters/[characterId]/emotions/config.json : config des émotions
 * - /characters/[characterId]/emotions/[emotionKey]/video.mjpeg : vidéo MJPEG
 *
 * Chaque émotion a 3 phases: intro, loop, exit
 * Chaque phase a une timeline de sourceFrameIndex
 */

/** Structure d'une frame dans la timeline */
struct TimelineFrame {
  int sourceFrameIndex;  // Index de la frame source dans le MJPEG
  // Note: actions ignorées pour le moment
};

/** Structure pour l'index d'une frame JPEG dans le fichier MJPEG */
struct FrameIndex {
  size_t fileOffset;  // Position dans le fichier (début SOI)
  size_t frameSize;   // Taille de la frame (SOI à EOI inclus)
};

/** Structure d'une phase (intro, loop, ou exit) */
struct EmotionPhase {
  int frames;                            // Nombre de frames
  std::vector<TimelineFrame> timeline;   // Timeline des frames
};

/** Structure d'une émotion chargée */
struct EmotionData {
  String key;            // Clé de l'émotion (OK, SLEEP, COLD, etc.)
  String emotionId;      // UUID de l'émotion
  int fps;               // FPS de l'animation
  int width;             // Largeur de la vidéo
  int height;            // Hauteur de la vidéo
  int totalFrames;       // Nombre total de frames
  float durationS;       // Durée totale en secondes
  EmotionPhase intro;    // Phase d'intro
  EmotionPhase loop;     // Phase de loop
  EmotionPhase exit;     // Phase d'exit
  String mjpegPath;      // Chemin vers le fichier video.mjpeg
  std::vector<FrameIndex> frameOffsets;  // Index des frames pour accès direct
};

class EmotionManager {
public:
  /** Initialiser le gestionnaire */
  static bool init();

  /** Charger une émotion depuis la SD (par sa clé, ex: "OK", "SLEEP") */
  static bool loadEmotion(const String& emotionKey);

  /** Obtenir l'émotion actuellement chargée */
  static const EmotionData* getCurrentEmotion();

  /** Jouer la phase intro uniquement */
  static void playIntro();

  /** Jouer la phase loop uniquement */
  static void playLoop();

  /** Jouer la phase exit uniquement */
  static void playExit();

  /** Jouer toute l'émotion (intro → loop x loopCount → exit) */
  static void playAll(int loopCount = 1);

  /** Vérifier si une émotion est chargée */
  static bool isLoaded();

private:
  static String _characterId;         // ID du personnage (depuis /config.json)
  static EmotionData _currentEmotion; // Émotion actuellement chargée
  static bool _loaded;                // true si une émotion est chargée

  /** Charger le characterId depuis /config.json */
  static bool loadCharacterId();

  /** Parser le JSON de config d'une émotion */
  static bool parseEmotionConfig(const String& jsonPath, const String& emotionKey);

  /** Construire l'index des frames du MJPEG pour accès direct */
  static bool buildFrameIndex();

  /** Jouer une phase donnée */
  static void playPhase(const EmotionPhase& phase);
};

#endif // EMOTION_MANAGER_H
