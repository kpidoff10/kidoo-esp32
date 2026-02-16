#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <SD.h>

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
 *
 * SYSTÈME ASYNCHRONE (non-bloquant):
 * - update() appelé dans loop() avance d'une frame par cycle
 * - Queue d'animations avec priorités (NORMAL, HIGH)
 * - Interruption possible des animations en cours (HIGH priority)
 * - Buffer PSRAM persistant (128KB) alloué une seule fois
 */

/** État de lecture de l'animation */
enum EmotionPlayState {
  EMOTION_STATE_IDLE,           // Aucune animation en cours
  EMOTION_STATE_PLAYING_INTRO,  // Lecture phase intro
  EMOTION_STATE_PLAYING_LOOP,   // Lecture phase loop
  EMOTION_STATE_PLAYING_EXIT,   // Lecture phase exit
};

/** Priorité de requête d'animation */
enum EmotionPriority {
  EMOTION_PRIORITY_NORMAL = 0,  // Mise en queue, attend fin de boucle courante
  EMOTION_PRIORITY_HIGH = 1,    // Interrompt l'animation courante (saute vers exit)
};

/** Type d'action à exécuter à l'affichage d'une frame (aligné avec FrameAction côté server). */
enum FrameActionType {
  FRAME_ACTION_NONE = 0,
  FRAME_ACTION_LED,   // Couleur LED (r, g, b)
  FRAME_ACTION_VIBRATION  // Réservé pour plus tard
};

/** Action matérielle déclenchée à cette frame (LED, vibration, etc.). */
struct FrameAction {
  FrameActionType type;
  uint8_t r, g, b;  // Pour LED : couleur RGB
  uint8_t vibratorEffect;  // Pour FRAME_ACTION_VIBRATION : 0=short, 1=long, 2=jerky, 3=pulse, 4=doubletap
};

/** Structure d'une frame dans la timeline */
struct TimelineFrame {
  int sourceFrameIndex;  // Index de la frame source dans le MJPEG
  std::vector<FrameAction> actions;  // Actions à exécuter quand cette frame est affichée (LED, etc.)
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
  String trigger;        // Trigger automatique (manual, hunger_low, eating_finished, etc.)
  int variant;           // Variant (1-4) pour plusieurs animations par trigger
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

/** Callback optionnel : retourne false pour passer en phase EXIT (ex. head_caress après 4 s sans touch). */
typedef bool (*LoopContinueConditionFn)();

/** Requête d'émotion pour la queue */
struct EmotionRequest {
  String emotionKey;       // Clé de l'émotion à charger
  int loopCount;           // Nombre de boucles à effectuer
  EmotionPriority priority; // Priorité de la requête
  int variant;             // Variant (1-4) pour key+variant ; 0 = premier qui matche la key
  String requestedTrigger; // Trigger qui a déclenché la requête (ex. hunger_medium) ; vide si manuel/NFC
  LoopContinueConditionFn loopCondition;  // Si non null, utilisée en phase loop pour décider EXIT (ex. head_caress)
};

/** Contexte de lecture (état interne de la state machine) */
struct PlaybackContext {
  EmotionPlayState state;       // État courant de la state machine
  int currentFrameIndex;        // Index dans la timeline de la phase courante
  int currentLoopIteration;     // Itération de loop courante (0-based)
  int totalLoopIterations;      // Nombre total d'itérations de loop demandées
  unsigned long lastFrameTime;  // Timestamp (millis) de la dernière frame affichée
  uint32_t frameDurationMs;     // Durée par frame en ms (1000/fps)
  bool interruptRequested;      // Flag d'interruption (HIGH priority)
  bool frameErrorOccurred;      // Première erreur seek/read frame → log une fois, puis EXIT
  File mjpegFile;               // Handle du fichier MJPEG ouvert
  bool fileOpen;                // true si mjpegFile est ouvert
};

class EmotionManager {
public:
  //================================================================
  // API ASYNCHRONE (Nouveau système non-bloquant)
  //================================================================

  /** Initialiser le gestionnaire (alloue buffer PSRAM) */
  static bool init();

  /** Mettre à jour la state machine (à appeler dans loop()). Non-bloquant. */
  static void update();

  /** Demander la lecture d'une émotion (mise en queue). Non-bloquant.
   * @param variant Variant (1-4) pour sélectionner l'animation key+variant ; 0 = premier qui matche la key
   * @param requestedTrigger Trigger qui a déclenché (ex. hunger_medium) ; utilisé pour le log, vide si manuel/NFC
   * @param loopCondition Si non null, appelée en phase loop pour décider de passer en EXIT (ex. head_caress) */
  static bool requestEmotion(const String& emotionKey, int loopCount = 1,
                             EmotionPriority priority = EMOTION_PRIORITY_NORMAL, int variant = 0,
                             const String& requestedTrigger = "", LoopContinueConditionFn loopCondition = nullptr);

  /** Annuler toutes les animations (vide queue, interruption immédiate → IDLE) */
  static void cancelAll();

  /** Vérifier si une animation est en cours de lecture */
  static bool isPlaying();

  /** Obtenir l'état courant de la state machine */
  static EmotionPlayState getState();

  /** Obtenir la clé de l'émotion en cours de lecture (vide si IDLE) */
  static String getCurrentPlayingKey();

  /** Callback appelé à chaque fin d'itération de loop : retourne true pour continuer, false pour passer en EXIT.
   *  Utilisé ex. pour le biberon : boucle tant que (faim OU tag NFC présent). */
  static void setLoopContinueCondition(LoopContinueConditionFn fn);

  /** Forcer la loop en cours à passer en EXIT au prochain update() (ex. tag NFC retiré → jouer la phase exit). */
  static void requestExitLoop();

  //================================================================
  // API LEGACY (Compatibilité debug/serial)
  //================================================================

  /** Charger une émotion pour inspection (métadonnées uniquement, ne joue pas).
   * @param variant Variant (1-4) pour key+variant ; 0 = premier qui matche la key */
  static bool loadEmotion(const String& emotionKey, int variant = 0);

  /** Obtenir l'émotion actuellement chargée */
  static const EmotionData* getCurrentEmotion();

  /** Vérifier si une émotion est chargée */
  static bool isLoaded();

private:
  //================================================================
  // Membres statiques (état interne)
  //================================================================

  static String _characterId;         // ID du personnage (depuis /config.json)
  static EmotionData _currentEmotion; // Émotion actuellement chargée
  static bool _loaded;                // true si une émotion est chargée

  // Buffer PSRAM : JPEG/MJPEG (legacy) ou données RLE pour .anim
  static uint8_t* _frameBuffer;
  static const size_t FRAME_BUFFER_SIZE = 131072;  // 128 KB

  // Format .anim : palette + double buffer RGB565 pour fluidité
  static bool _useAnimFormat;
  static uint16_t _animPalette[256];
  static uint8_t _animPaletteSize;
  static uint16_t* _animRgbBuffer[2];   // Double buffering PSRAM
  static int _animRgbBufferIndex;        // 0 ou 1, alterné à chaque frame
  static const size_t ANIM_RGB_BUFFER_SIZE = 240 * 280 * 2;  // 134400 bytes par buffer

  // Contexte de lecture (state machine)
  static PlaybackContext _playback;

  // Nouveau : queue d'animations (circular buffer fixe)
  static const int EMOTION_QUEUE_MAX_SIZE = 4;
  static EmotionRequest _queue[EMOTION_QUEUE_MAX_SIZE];
  static int _queueHead;   // Index du prochain élément à dequeue
  static int _queueTail;   // Index du prochain slot libre
  static int _queueCount;  // Nombre d'éléments dans la queue

  /** Si non null, appelé à chaque fin d'itération de loop ; si retourne false → EXIT. */
  static LoopContinueConditionFn _loopContinueCondition;

  //================================================================
  // Méthodes privées
  //================================================================

  /** Charger le characterId depuis /config.json */
  static bool loadCharacterId();

  /** Parser le JSON de config d'une émotion (key + variant optionnel).
   * @param silentIfNotFound Si true, ne pas imprimer en Serial quand la clé est absente (pour fallback eating→FOOD). */
  static bool parseEmotionConfig(const String& jsonPath, const String& emotionKey, int requestedVariant = 0, bool silentIfNotFound = false);

  /** Construire l'index des frames (MJPEG .idx ou .anim en parcourant le fichier) */
  static bool buildFrameIndex();

  /** Construire l'index des frames pour un fichier .anim (header + palette + offsets) */
  static bool buildAnimFrameIndex();

  /** Afficher la frame courante si le timing est respecté. Retourne true si affichée. */
  static bool displayCurrentFrame(const EmotionPhase& phase);

  /** Exécuter les actions de la frame courante (LED, etc.). Appelé après affichage réussi. */
  static void runFrameActions(const EmotionPhase& phase, int frameIndex);

  /** Ouvrir le fichier MJPEG de l'émotion actuellement chargée */
  static bool openMjpegFile();

  /** Fermer le fichier MJPEG s'il est ouvert */
  static void closeMjpegFile();

  /** Ajouter une requête dans la queue. Retourne false si pleine. */
  static bool enqueue(const EmotionRequest& request);

  /** Retirer une requête de la queue. Retourne false si vide. */
  static bool dequeue(EmotionRequest& request);

  /** Vider la queue */
  static void clearQueue();

  /** Obtenir un pointeur vers la phase courante selon l'état */
  static const EmotionPhase* getCurrentPhase();

  /** Transition vers un nouvel état (gère entry/exit logic) */
  static void transitionTo(EmotionPlayState newState);
};

#endif // EMOTION_MANAGER_H
