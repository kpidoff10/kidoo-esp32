#include "emotion_manager.h"
#include "../../../model_config.h"
#include "../../../common/managers/lcd/lcd_manager.h"

// Mettre à 1 pour debug détaillé (attention: flood Serial, accès console difficile)
#define EMOTION_DEBUG_SERIAL 0

// Forcer les FPS des vidéos pour test (20 = 20 fps, 0 = utiliser le FPS de la config)
#define FORCE_EMOTION_FPS 20

#if defined(HAS_SD)
#include <SD.h>
#include <ArduinoJson.h>
#endif
#if defined(ESP32)
#include <esp_random.h>
#endif

// Membres statiques - état existant
String EmotionManager::_characterId = "";
EmotionData EmotionManager::_currentEmotion;
bool EmotionManager::_loaded = false;

// Membres statiques - nouveau système asynchrone
uint8_t* EmotionManager::_frameBuffer = nullptr;
PlaybackContext EmotionManager::_playback = {
  EMOTION_STATE_IDLE, // state
  0,                  // currentFrameIndex
  0,                  // currentLoopIteration
  0,                  // totalLoopIterations
  0,                  // lastFrameTime
  100,                // frameDurationMs (défaut 10 FPS)
  false,              // interruptRequested
  false,              // frameErrorOccurred
  File(),             // mjpegFile
  false               // fileOpen
};
EmotionRequest EmotionManager::_queue[EMOTION_QUEUE_MAX_SIZE];
int EmotionManager::_queueHead = 0;
int EmotionManager::_queueTail = 0;
int EmotionManager::_queueCount = 0;
EmotionManager::LoopContinueConditionFn EmotionManager::_loopContinueCondition = nullptr;

bool EmotionManager::init() {
  _loaded = false;
  _characterId = "";

  // Initialiser la state machine
  _playback.state = EMOTION_STATE_IDLE;
  _playback.currentFrameIndex = 0;
  _playback.currentLoopIteration = 0;
  _playback.totalLoopIterations = 0;
  _playback.lastFrameTime = 0;
  _playback.frameDurationMs = 100;
  _playback.interruptRequested = false;
  _playback.frameErrorOccurred = false;
  _playback.fileOpen = false;

  // Initialiser la queue
  _queueHead = 0;
  _queueTail = 0;
  _queueCount = 0;

#if defined(HAS_SD)
  // Charger le characterId depuis /config.json
  if (!loadCharacterId()) {
    Serial.println("[EMOTION] Erreur: Impossible de charger characterId depuis /config.json");
    return false;
  }

  Serial.printf("[EMOTION] CharacterId charge: %s\n", _characterId.c_str());

  // Allouer le buffer PSRAM persistant (128KB)
  _frameBuffer = (uint8_t*)heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  if (!_frameBuffer) {
    Serial.println("[EMOTION] ERREUR: Impossible d'allouer buffer frame en PSRAM");
    Serial.println("[EMOTION] Tentative allocation en RAM interne...");
    _frameBuffer = (uint8_t*)malloc(FRAME_BUFFER_SIZE);
    if (!_frameBuffer) {
      Serial.println("[EMOTION] ERREUR CRITIQUE: Impossible d'allouer buffer frame");
      return false;
    }
    Serial.println("[EMOTION] Buffer alloue en RAM interne (fallback)");
  } else {
    Serial.printf("[EMOTION] Buffer frame alloue en PSRAM: %d bytes\n", FRAME_BUFFER_SIZE);
  }

  return true;
#else
  Serial.println("[EMOTION] SD non disponible");
  return false;
#endif
}

bool EmotionManager::loadCharacterId() {
#if defined(HAS_SD)
  File file = SD.open("/config.json", FILE_READ);
  if (!file) {
    Serial.println("[EMOTION] Erreur: /config.json introuvable");
    return false;
  }

  // Parser le JSON pour extraire characterId
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[EMOTION] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  if (doc["characterId"].isNull()) {
    Serial.println("[EMOTION] Erreur: characterId manquant dans /config.json");
    return false;
  }

  _characterId = doc["characterId"].as<String>();
  return true;
#else
  return false;
#endif
}

bool EmotionManager::loadEmotion(const String& emotionKey, int variant) {
#if defined(HAS_SD)
  // Garde: ne pas charger pendant la lecture
  if (isPlaying()) {
    Serial.println("[EMOTION] Erreur: Impossible de charger pendant la lecture. Utilisez cancelAll() d'abord.");
    return false;
  }

  if (_characterId.isEmpty()) {
    Serial.println("[EMOTION] Erreur: characterId non charge, appelez init() d'abord");
    return false;
  }

  // Construire le chemin du fichier de config
  String configPath = "/characters/" + _characterId + "/emotions/config.json";

  if (!parseEmotionConfig(configPath, emotionKey, variant, (emotionKey == "eating"))) {
    // Rétrocompat: ancienne config utilisait "FOOD" pour manger (clé "eating" pas encore sync)
    if (emotionKey == "eating" && parseEmotionConfig(configPath, "FOOD", variant)) {
      // Chargé depuis FOOD, chemins vidéo restent .../FOOD/...
    } else {
      Serial.printf("[EMOTION] Erreur: Impossible de charger l'emotion '%s'\n", emotionKey.c_str());
      return false;
    }
  }

  // Construire l'index des frames pour accès direct
  if (!buildFrameIndex()) {
    Serial.println("[EMOTION] Erreur: Impossible de construire l'index des frames");
    return false;
  }

  _loaded = true;
#if EMOTION_DEBUG_SERIAL
  Serial.printf("[EMOTION] Emotion '%s' chargee: %d frames (intro:%d, loop:%d, exit:%d)\n",
                _currentEmotion.key.c_str(), _currentEmotion.totalFrames,
                _currentEmotion.intro.frames, _currentEmotion.loop.frames, _currentEmotion.exit.frames);
#endif
  return true;
#else
  return false;
#endif
}

bool EmotionManager::parseEmotionConfig(const String& jsonPath, const String& emotionKey, int requestedVariant, bool silentIfNotFound) {
#if defined(HAS_SD)
  static String _lastMissingConfigPath;
  static bool _loggedMissingConfig = false;
  File file = SD.open(jsonPath.c_str(), FILE_READ);
  if (!file) {
    if (!_loggedMissingConfig || _lastMissingConfigPath != jsonPath) {
      _loggedMissingConfig = true;
      _lastMissingConfigPath = jsonPath;
      Serial.printf("[EMOTION] Erreur: fichier introuvable: %s (sync emotions ou export config depuis l'admin)\n", jsonPath.c_str());
    }
    return false;
  }

  // Parser le JSON (tableau d'émotions)
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[EMOTION] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  // Le JSON est un tableau, chercher l'émotion avec la bonne clé (et variant si demandé)
  // Pour "OK" (attente/idle), on ne prend que les entrées avec trigger "manual" (pas de trigger humeur)
  JsonArray emotions = doc.as<JsonArray>();
  JsonObject emotionObj;
  bool found = false;
  int objVariant = 0;

  // Pour "OK" avec variant 0 : collecter tous les indices correspondants puis tirer au sort
  const bool okRandomPick = (emotionKey == "OK" && requestedVariant == 0);
  std::vector<size_t> okMatchIndices;

  for (size_t i = 0; i < emotions.size(); i++) {
    JsonObject obj = emotions[i].as<JsonObject>();
    if (obj["key"].as<String>() != emotionKey) {
      continue;
    }
    if (emotionKey == "OK") {
      String t = obj["trigger"].as<String>();
      if (t.isEmpty()) t = "manual";
      if (t != "manual") continue;  // OK = uniquement trigger "manual"
    }
    objVariant = obj["variant"] | 1;
    if (requestedVariant == 0 || objVariant == requestedVariant) {
      if (okRandomPick) {
        okMatchIndices.push_back(i);
      } else {
        emotionObj = obj;
        found = true;
        break;
      }
    }
  }

  if (okRandomPick && !okMatchIndices.empty()) {
    // RNG matériel sur ESP32 pour éviter toujours la même animation (random() non seedé)
    size_t idx;
#if defined(ESP32)
    idx = (size_t)(esp_random() % (uint32_t)okMatchIndices.size());
#else
    idx = (size_t)random(0, (int)okMatchIndices.size());
#endif
    size_t randomIdx = okMatchIndices[idx];
    emotionObj = emotions[randomIdx].as<JsonObject>();
    found = true;
    // Log: N entrées OK (peuvent toutes avoir variant=1) -> on joue l'entrée #idx (animation différente si emotionId différent)
    int variantInConfig = emotionObj["variant"] | 1;
    Serial.printf("[EMOTION] OK: %zu entree(s) -> joue entree #%zu (variant config=%d)\n",
                  okMatchIndices.size(), idx + 1, variantInConfig);
  }

  if (!found) {
    if (!silentIfNotFound) {
      Serial.printf("[EMOTION] Erreur: emotion '%s' (variant=%d) non trouvee dans le JSON\n", emotionKey.c_str(), requestedVariant);
    }
    return false;
  }

  // Extraire les données de l'émotion
  _currentEmotion.key = emotionKey;
  _currentEmotion.emotionId = emotionObj["emotionId"].as<String>();
  _currentEmotion.trigger = emotionObj["trigger"].as<String>();
  _currentEmotion.variant = emotionObj["variant"] | 1;  // Par défaut: 1

  // Si trigger est vide, mettre "manual" par défaut
  if (_currentEmotion.trigger.isEmpty()) {
    _currentEmotion.trigger = "manual";
  }

  // Choisir une vidéo dans emotion_videos (aléatoire si plusieurs, pour varier les OK)
  if (!emotionObj["emotion_videos"].is<JsonArray>()) {
    Serial.println("[EMOTION] Erreur: emotion_videos manquant ou invalide");
    return false;
  }
  JsonArray videosArray = emotionObj["emotion_videos"].as<JsonArray>();
  size_t videoCount = videosArray.size();
  if (videoCount == 0) {
    Serial.println("[EMOTION] Erreur: aucune video dans emotion_videos");
    return false;
  }
  size_t videoIndex = 0;
  if (videoCount > 1) {
#if defined(ESP32)
    videoIndex = (size_t)(esp_random() % (uint32_t)videoCount);
#else
    videoIndex = (size_t)random(0, (int)videoCount);
#endif
  }
  JsonObject video = videosArray[videoIndex].as<JsonObject>();
  if (!video) {
    Serial.println("[EMOTION] Erreur: video invalide dans emotion_videos");
    return false;
  }

  // Extraire l'emotion_videoId pour construire le chemin
  String emotionVideoId = video["emotion_videoId"].as<String>();
  if (emotionVideoId.isEmpty()) {
    Serial.println("[EMOTION] Erreur: emotion_videoId manquant dans emotion_videos");
    return false;
  }

  _currentEmotion.fps = video["fps"] | 10;
  _currentEmotion.width = video["width"] | 280;
  _currentEmotion.height = video["height"] | 240;
  _currentEmotion.totalFrames = video["totalFrames"] | 0;
  _currentEmotion.durationS = video["durationS"] | 0.0f;

  // Parser les phases
  JsonObject phases = video["phases"];
  if (!phases) {
    Serial.println("[EMOTION] Erreur: phases manquantes");
    return false;
  }

  // Parser intro
  JsonObject introObj = phases["intro"];
  if (introObj) {
    _currentEmotion.intro.frames = introObj["frames"] | 0;
    _currentEmotion.intro.timeline.clear();

    JsonArray introTimeline = introObj["timeline"];
    for (JsonObject frame : introTimeline) {
      TimelineFrame tf;
      tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
      if (tf.sourceFrameIndex >= 0) {
        _currentEmotion.intro.timeline.push_back(tf);
      }
    }
  }

  // Parser loop
  JsonObject loopObj = phases["loop"];
  if (loopObj) {
    _currentEmotion.loop.frames = loopObj["frames"] | 0;
    _currentEmotion.loop.timeline.clear();

    JsonArray loopTimeline = loopObj["timeline"];
    for (JsonObject frame : loopTimeline) {
      TimelineFrame tf;
      tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
      if (tf.sourceFrameIndex >= 0) {
        _currentEmotion.loop.timeline.push_back(tf);
      }
    }
  }

  // Parser exit
  JsonObject exitObj = phases["exit"];
  if (exitObj) {
    _currentEmotion.exit.frames = exitObj["frames"] | 0;
    _currentEmotion.exit.timeline.clear();

    JsonArray exitTimeline = exitObj["timeline"];
    for (JsonObject frame : exitTimeline) {
      TimelineFrame tf;
      tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
      if (tf.sourceFrameIndex >= 0) {
        _currentEmotion.exit.timeline.push_back(tf);
      }
    }
  }

  // Construire le chemin du fichier MJPEG avec l'emotion_videoId
  String mjpegPath = "/characters/" + _characterId + "/emotions/" + emotionKey + "/" + emotionVideoId + "/video.mjpeg";

#if defined(HAS_SD)
  // Si la vidéo choisie n'est pas sur la SD (ex: 2e OK pas encore sync), repasser à la première pour éviter écran noir
  if (videoCount > 1 && !SD.exists(mjpegPath.c_str())) {
    Serial.println("[EMOTION] Video choisie absente sur SD, fallback video 0");
    videoIndex = 0;
    video = videosArray[0].as<JsonObject>();
    emotionVideoId = video["emotion_videoId"].as<String>();
    if (emotionVideoId.isEmpty()) {
      Serial.println("[EMOTION] Erreur: emotion_videoId manquant (fallback video 0)");
      return false;
    }
    mjpegPath = "/characters/" + _characterId + "/emotions/" + emotionKey + "/" + emotionVideoId + "/video.mjpeg";
    _currentEmotion.fps = video["fps"] | 10;
    _currentEmotion.width = video["width"] | 280;
    _currentEmotion.height = video["height"] | 240;
    _currentEmotion.totalFrames = video["totalFrames"] | 0;
    _currentEmotion.durationS = video["durationS"] | 0.0f;
    JsonObject phases0 = video["phases"];
    if (phases0) {
      _currentEmotion.intro.timeline.clear();
      _currentEmotion.loop.timeline.clear();
      _currentEmotion.exit.timeline.clear();
      JsonObject introObj2 = phases0["intro"];
      if (introObj2) {
        JsonArray introTimeline = introObj2["timeline"];
        for (JsonObject frame : introTimeline) {
          TimelineFrame tf;
          tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
          if (tf.sourceFrameIndex >= 0) _currentEmotion.intro.timeline.push_back(tf);
        }
      }
      JsonObject loopObj2 = phases0["loop"];
      if (loopObj2) {
        JsonArray loopTimeline = loopObj2["timeline"];
        for (JsonObject frame : loopTimeline) {
          TimelineFrame tf;
          tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
          if (tf.sourceFrameIndex >= 0) _currentEmotion.loop.timeline.push_back(tf);
        }
      }
      JsonObject exitObj2 = phases0["exit"];
      if (exitObj2) {
        JsonArray exitTimeline = exitObj2["timeline"];
        for (JsonObject frame : exitTimeline) {
          TimelineFrame tf;
          tf.sourceFrameIndex = frame["sourceFrameIndex"] | -1;
          if (tf.sourceFrameIndex >= 0) _currentEmotion.exit.timeline.push_back(tf);
        }
      }
    }
  }
#endif

  _currentEmotion.mjpegPath = mjpegPath;

  return true;
#else
  return false;
#endif
}

bool EmotionManager::buildFrameIndex() {
#if defined(HAS_SD)
  _currentEmotion.frameOffsets.clear();

  // Essayer de charger l'index pré-calculé depuis video.idx
  String idxPath = _currentEmotion.mjpegPath;
  idxPath.replace(".mjpeg", ".idx");

  File idxFile = SD.open(idxPath.c_str(), FILE_READ);
  if (idxFile) {
    // Lire le nombre de frames (4 bytes, little-endian)
    if (idxFile.available() < 4) {
      Serial.println("[EMOTION] Erreur: fichier .idx trop court");
      idxFile.close();
      return false;
    }

    uint32_t frameCount = 0;
    idxFile.read((uint8_t*)&frameCount, 4);

    // Lire tous les offsets et sizes (8 bytes par frame)
    for (uint32_t i = 0; i < frameCount; i++) {
      if (idxFile.available() < 8) {
        Serial.printf("[EMOTION] Erreur: fichier .idx incomplet a la frame %u\n", i);
        idxFile.close();
        return false;
      }

      uint32_t offset = 0;
      uint32_t size = 0;
      idxFile.read((uint8_t*)&offset, 4);
      idxFile.read((uint8_t*)&size, 4);

      FrameIndex idx;
      idx.fileOffset = offset;
      idx.frameSize = size;
      _currentEmotion.frameOffsets.push_back(idx);
    }

    idxFile.close();
    return true;
  }

  // Pas de fichier .idx : fallback sur le calcul manuel
  Serial.println("[EMOTION] Fichier .idx non trouve, calcul de l'index...");

  File f = SD.open(_currentEmotion.mjpegPath.c_str(), FILE_READ);
  if (!f) {
    Serial.printf("[EMOTION] Erreur: fichier MJPEG introuvable: %s\n", _currentEmotion.mjpegPath.c_str());
    return false;
  }

  const size_t CHUNK = 8192;
  uint8_t* buffer = (uint8_t*)malloc(CHUNK);
  if (!buffer) {
    f.close();
    Serial.println("[EMOTION] Erreur allocation memoire pour l'indexation");
    return false;
  }

  size_t filePos = 0;
  size_t bufLen = 0;
  size_t bufStart = 0;

  while (true) {
    // Lire plus de données
    if (bufLen < CHUNK / 2) {
      memmove(buffer, buffer + bufStart, bufLen);
      bufStart = 0;

      int n = f.read(buffer + bufLen, CHUNK - bufLen);
      if (n > 0) {
        bufLen += n;
      } else if (bufLen == 0) {
        break;  // Fin du fichier
      }
    }

    if (bufLen < 2) break;

    // Chercher SOI (0xFF 0xD8)
    size_t soiPos = (size_t)-1;
    for (size_t i = 0; i + 1 < bufLen; i++) {
      if (buffer[bufStart + i] == 0xFF && buffer[bufStart + i + 1] == 0xD8) {
        soiPos = i;
        break;
      }
    }

    if (soiPos == (size_t)-1) {
      // Pas de SOI trouvé, avancer
      size_t skip = (bufLen > 1) ? bufLen - 1 : 0;
      filePos += skip;
      bufStart += skip;
      bufLen -= skip;
      continue;
    }

    // Chercher EOI (0xFF 0xD9) après SOI
    size_t eoiPos = (size_t)-1;
    for (size_t i = soiPos + 2; i + 1 < bufLen; i++) {
      if (buffer[bufStart + i] == 0xFF && buffer[bufStart + i + 1] == 0xD9) {
        eoiPos = i + 2;  // Inclure le 0xD9
        break;
      }
    }

    if (eoiPos == (size_t)-1) {
      // EOI pas encore dans le buffer, besoin de plus de données
      filePos += soiPos;
      bufStart += soiPos;
      bufLen -= soiPos;
      continue;
    }

    // Frame complète trouvée!
    FrameIndex idx;
    idx.fileOffset = filePos + soiPos;
    idx.frameSize = eoiPos - soiPos;
    _currentEmotion.frameOffsets.push_back(idx);

    // Avancer après cette frame
    filePos += eoiPos;
    bufStart += eoiPos;
    bufLen -= eoiPos;
  }

  free(buffer);
  f.close();

  return true;
#else
  return false;
#endif
}

const EmotionData* EmotionManager::getCurrentEmotion() {
  return _loaded ? &_currentEmotion : nullptr;
}

bool EmotionManager::isLoaded() {
  return _loaded;
}

// ================================================================
// Ancien code bloquant - SUPPRIMÉ et remplacé par le système asynchrone
// Les anciennes méthodes playPhase(), playIntro(), playLoop(), playExit(), playAll()
// ont été remplacées par la state machine update() + requestEmotion()
// ================================================================

//================================================================
// NOUVEAU SYSTÈME ASYNCHRONE - State Machine
//================================================================

bool EmotionManager::enqueue(const EmotionRequest& request) {
  if (_queueCount >= EMOTION_QUEUE_MAX_SIZE) {
    return false;  // Queue pleine
  }

  _queue[_queueTail] = request;
  _queueTail = (_queueTail + 1) % EMOTION_QUEUE_MAX_SIZE;
  _queueCount++;
  return true;
}

bool EmotionManager::dequeue(EmotionRequest& request) {
  if (_queueCount == 0) {
    return false;  // Queue vide
  }

  request = _queue[_queueHead];
  _queueHead = (_queueHead + 1) % EMOTION_QUEUE_MAX_SIZE;
  _queueCount--;
  return true;
}

void EmotionManager::clearQueue() {
  _queueHead = 0;
  _queueTail = 0;
  _queueCount = 0;
#if EMOTION_DEBUG_SERIAL
  Serial.println("[EMOTION] Queue videe");
#endif
}

void EmotionManager::closeMjpegFile() {
  if (_playback.fileOpen && _playback.mjpegFile) {
    _playback.mjpegFile.close();
    _playback.fileOpen = false;
    // Ne pas remplir noir ici : la dernière frame reste affichée jusqu'à la prochaine animation (évite coupure brutale)
  }
}

bool EmotionManager::openMjpegFile() {
#if defined(HAS_SD)
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return false;
  }

  // Fermer si déjà ouvert
  closeMjpegFile();

  _playback.mjpegFile = SD.open(_currentEmotion.mjpegPath.c_str(), FILE_READ);
  if (!_playback.mjpegFile) {
    Serial.printf("[EMOTION] Erreur: Impossible d'ouvrir %s\n", _currentEmotion.mjpegPath.c_str());
    return false;
  }

  _playback.fileOpen = true;
  return true;
#else
  return false;
#endif
}

void EmotionManager::setLoopContinueCondition(LoopContinueConditionFn fn) {
  _loopContinueCondition = fn;
}

void EmotionManager::requestExitLoop() {
  _loopContinueCondition = nullptr;
  _playback.interruptRequested = true;
}

void EmotionManager::transitionTo(EmotionPlayState newState) {
  // Exit logic pour l'état courant
  switch (_playback.state) {
    case EMOTION_STATE_PLAYING_EXIT:
      // Sortie de la phase exit = fin de l'émotion
      closeMjpegFile();
      break;
    default:
      break;
  }
  if (newState == EMOTION_STATE_PLAYING_EXIT || newState == EMOTION_STATE_IDLE) {
    _loopContinueCondition = nullptr;
  }

  // Entrée dans le nouvel état
  _playback.state = newState;
  _playback.currentFrameIndex = 0;

  if (newState == EMOTION_STATE_PLAYING_INTRO) {
    Serial.printf("[EMOTION] Animation: key=%s variant=%d trigger=%s\n",
                  _currentEmotion.key.c_str(), _currentEmotion.variant, _currentEmotion.trigger.c_str());
  }
}

const EmotionPhase* EmotionManager::getCurrentPhase() {
  switch (_playback.state) {
    case EMOTION_STATE_PLAYING_INTRO:
      return &_currentEmotion.intro;
    case EMOTION_STATE_PLAYING_LOOP:
      return &_currentEmotion.loop;
    case EMOTION_STATE_PLAYING_EXIT:
      return &_currentEmotion.exit;
    default:
      return nullptr;
  }
}

bool EmotionManager::displayCurrentFrame(const EmotionPhase& phase) {
#if defined(HAS_LCD) && defined(HAS_SD)
  // Vérifier le timing
  unsigned long now = millis();
  if (now - _playback.lastFrameTime < _playback.frameDurationMs) {
    return false;  // Pas encore le moment
  }

  if (_playback.currentFrameIndex >= (int)phase.timeline.size()) {
    Serial.printf("[EMOTION] Erreur: Index frame hors limites: %d >= %d\n",
                  _playback.currentFrameIndex, phase.timeline.size());
    return false;
  }

  int frameIdx = phase.timeline[_playback.currentFrameIndex].sourceFrameIndex;
  const int maxFrame = (int)_currentEmotion.frameOffsets.size() - 1;

  if (maxFrame < 0) {
    _playback.lastFrameTime = now;
    return true;  // Pas de frames
  }
  if (frameIdx < 0 || frameIdx > maxFrame) {
#if EMOTION_DEBUG_SERIAL
    Serial.printf("[EMOTION] Index frame clampe: %d -> %d (max %d)\n", frameIdx, (frameIdx < 0 ? 0 : maxFrame), maxFrame);
#endif
    frameIdx = (frameIdx < 0) ? 0 : maxFrame;
  }

  const FrameIndex& idx = _currentEmotion.frameOffsets[frameIdx];

  if (idx.frameSize > FRAME_BUFFER_SIZE) {
    Serial.printf("[EMOTION] Erreur: Frame %d trop grande (%d bytes)\n", frameIdx, idx.frameSize);
    _playback.lastFrameTime = now;
    return true;  // Sauter
  }

  // Seek et lecture depuis le fichier déjà ouvert
  if (!_playback.mjpegFile.seek(idx.fileOffset)) {
    if (!_playback.frameErrorOccurred) {
      _playback.frameErrorOccurred = true;
      Serial.printf("[EMOTION] Erreur seek frame %d (index MJPEG incoherent?); passage en EXIT\n", frameIdx);
    }
    _playback.lastFrameTime = now;
    return true;
  }

  int bytesRead = _playback.mjpegFile.read(_frameBuffer, idx.frameSize);
  if (bytesRead != (int)idx.frameSize) {
    if (!_playback.frameErrorOccurred) {
      _playback.frameErrorOccurred = true;
      Serial.printf("[EMOTION] Erreur lecture frame %d: %d/%d bytes; passage en EXIT\n", frameIdx, bytesRead, (int)idx.frameSize);
    }
    _playback.lastFrameTime = now;
    return true;
  }

  // Afficher la frame
  bool displaySuccess = LCDManager::displayJpegFrame(_frameBuffer, idx.frameSize);
  _playback.lastFrameTime = now;

  if (!displaySuccess) {
    Serial.printf("[EMOTION] ERREUR: Echec affichage frame %d (%d bytes)\n", frameIdx, idx.frameSize);
#if defined(HAS_LCD)
    LCDManager::fillScreen(LCDManager::COLOR_BLACK);  // Effacer frame corrompue
#endif
    // Enchaîner sur une animation OK pour ne pas rester sur écran noir
    EmotionManager::requestEmotion("OK", 1, EMOTION_PRIORITY_NORMAL, 0);
    return false;  // Signaler l'échec → passage EXIT puis IDLE, l'OK jouera
  }

  return true;
#else
  return false;
#endif
}

void EmotionManager::update() {
  switch (_playback.state) {

    case EMOTION_STATE_IDLE: {
      // Vérifier la queue
      EmotionRequest req;
      if (!dequeue(req)) {
        return;  // Queue vide, rien à faire
      }

      // Charger l'émotion (avec variant si spécifié)
      if (!loadEmotion(req.emotionKey, req.variant)) {
        Serial.printf("[EMOTION] Echec chargement '%s' (variant=%d), ignoré\n", req.emotionKey.c_str(), req.variant);
        return;  // Réessaiera au prochain update()
      }
      // Pour le log : afficher le trigger qui a réellement déclenché (pas seulement celui du clip en config)
      if (!req.requestedTrigger.isEmpty()) {
        _currentEmotion.trigger = req.requestedTrigger;
      }

      _loopContinueCondition = nullptr;  // Ne pas garder une condition d'une requête précédente

      // Configurer le contexte de lecture
      _playback.totalLoopIterations = req.loopCount;
      _playback.currentLoopIteration = 0;
      // Ajuster le timing: ajouter 20ms de marge pour le décodage JPEG
      int fpsToUse = (FORCE_EMOTION_FPS > 0) ? FORCE_EMOTION_FPS : (_currentEmotion.fps > 0 ? _currentEmotion.fps : 10);
      uint32_t baseDuration = 1000 / fpsToUse;
      _playback.frameDurationMs = baseDuration + 20;  // Ex: 24 fps → ~42ms + 20ms = 62ms par frame
      _playback.interruptRequested = false;

      // Ouvrir le fichier MJPEG
      if (!openMjpegFile()) {
        Serial.printf("[EMOTION] Echec ouverture MJPEG: %s\n", _currentEmotion.mjpegPath.c_str());
        return;
      }

      _playback.frameErrorOccurred = false;
      transitionTo(EMOTION_STATE_PLAYING_INTRO);
      // Pas de break, on essaie d'afficher la première frame immédiatement
      [[fallthrough]];
    }

    case EMOTION_STATE_PLAYING_INTRO: {
      const EmotionPhase& phase = _currentEmotion.intro;

      // Phase vide ?
      if (phase.timeline.empty()) {
#if EMOTION_DEBUG_SERIAL
        Serial.println("[EMOTION] INTRO timeline vide, saut vers LOOP");
#endif
        transitionTo(EMOTION_STATE_PLAYING_LOOP);
        return;
      }

      // Interruption demandée ?
      if (_playback.interruptRequested) {
        transitionTo(EMOTION_STATE_PLAYING_EXIT);
        return;
      }

      // Afficher la frame courante si timing OK
      if (!displayCurrentFrame(phase)) {
        return;  // Pas encore le moment
      }
      if (_playback.frameErrorOccurred) {
        _playback.frameErrorOccurred = false;
        transitionTo(EMOTION_STATE_PLAYING_EXIT);
        return;
      }

      // Passer à la frame suivante
      _playback.currentFrameIndex++;
      if (_playback.currentFrameIndex >= (int)phase.timeline.size()) {
        // Intro terminé
        transitionTo(EMOTION_STATE_PLAYING_LOOP);
      }
      break;
    }

    case EMOTION_STATE_PLAYING_LOOP: {
      const EmotionPhase& phase = _currentEmotion.loop;

      // Condition externe (ex. biberon) : vérifier à chaque frame pour sortir dès que tag retiré / rassasié
      if (_loopContinueCondition != nullptr) {
        if (!(*_loopContinueCondition)()) {
          _loopContinueCondition = nullptr;
          transitionTo(EMOTION_STATE_PLAYING_EXIT);
          return;
        }
      }

      // Phase vide ou 0 itérations ?
      if (phase.timeline.empty() || _playback.totalLoopIterations <= 0) {
#if EMOTION_DEBUG_SERIAL
        Serial.printf("[EMOTION] LOOP timeline vide ou iterations=0 (timeline.size=%d, iterations=%d), saut vers EXIT\n",
                      phase.timeline.size(), _playback.totalLoopIterations);
#endif
        transitionTo(EMOTION_STATE_PLAYING_EXIT);
        return;
      }

      // Interruption demandée ?
      if (_playback.interruptRequested) {
        transitionTo(EMOTION_STATE_PLAYING_EXIT);
        return;
      }

      // Afficher la frame courante si timing OK
      if (!displayCurrentFrame(phase)) {
        return;  // Pas encore le moment
      }
      if (_playback.frameErrorOccurred) {
        _playback.frameErrorOccurred = false;
        transitionTo(EMOTION_STATE_PLAYING_EXIT);
        return;
      }

      // Passer à la frame suivante
      _playback.currentFrameIndex++;
      if (_playback.currentFrameIndex >= (int)phase.timeline.size()) {
        // Une itération de loop complète
        _playback.currentLoopIteration++;
        _playback.currentFrameIndex = 0;

        // Condition externe (ex. biberon : boucle tant que faim ou tag NFC)
        if (_loopContinueCondition != nullptr) {
          if (!(*_loopContinueCondition)()) {
            _loopContinueCondition = nullptr;
            transitionTo(EMOTION_STATE_PLAYING_EXIT);
          }
          // Sinon on continue la loop (currentFrameIndex déjà à 0)
          break;
        }

        // Vérifier si on doit sortir
        bool shouldExit;
        if (_playback.totalLoopIterations == 0) {
          // Loop infini: sortir seulement si interrupt ou queue non-vide
          shouldExit = _playback.interruptRequested || (_queueCount > 0);
        } else {
          shouldExit = (_playback.currentLoopIteration >= _playback.totalLoopIterations);
        }

        // Ou si la queue a de nouveaux éléments
        bool queueHasItems = (_queueCount > 0);

        if (shouldExit || queueHasItems) {
          transitionTo(EMOTION_STATE_PLAYING_EXIT);
        }
        // Sinon, continue loop (currentFrameIndex déjà reset à 0)
      }
      break;
    }

    case EMOTION_STATE_PLAYING_EXIT: {
      const EmotionPhase& phase = _currentEmotion.exit;

      // Phase vide ?
      if (phase.timeline.empty()) {
        Serial.println("[EMOTION] EXIT timeline vide, saut vers IDLE");
        transitionTo(EMOTION_STATE_IDLE);
        return;
      }

      // Afficher la frame courante si timing OK
      if (!displayCurrentFrame(phase)) {
        return;  // Pas encore le moment
      }
      if (_playback.frameErrorOccurred) {
        _playback.frameErrorOccurred = false;
        transitionTo(EMOTION_STATE_IDLE);
        return;
      }

      // Passer à la frame suivante
      _playback.currentFrameIndex++;
      if (_playback.currentFrameIndex >= (int)phase.timeline.size()) {
        // Exit terminé
        transitionTo(EMOTION_STATE_IDLE);
      }
      break;
    }

    default:
      break;
  }
}

bool EmotionManager::requestEmotion(const String& emotionKey, int loopCount,
                                     EmotionPriority priority, int variant,
                                     const String& requestedTrigger) {
  EmotionRequest req;
  req.emotionKey = emotionKey;
  req.loopCount = loopCount;
  req.priority = priority;
  req.variant = variant;
  req.requestedTrigger = requestedTrigger;

  if (priority == EMOTION_PRIORITY_HIGH) {
    // Haute priorité: vider la queue, insérer en tête, set interrupt flag
    clearQueue();
    if (!enqueue(req)) {
      Serial.println("[EMOTION] Erreur: Impossible d'enqueuer (queue pleine après clear ?!)");
      return false;
    }
    _playback.interruptRequested = true;
#if EMOTION_DEBUG_SERIAL
    Serial.printf("[EMOTION] HIGH priority request: '%s' (interrupt set)\n", emotionKey.c_str());
#endif
  } else {
    if (!enqueue(req)) {
      Serial.println("[EMOTION] Erreur: Queue pleine, requete ignoree");
      return false;
    }
#if EMOTION_DEBUG_SERIAL
    Serial.printf("[EMOTION] Requete mise en queue: '%s' (loops=%d)\n", emotionKey.c_str(), loopCount);
#endif
  }

  return true;
}

void EmotionManager::cancelAll() {
  clearQueue();
  closeMjpegFile();
  _playback.state = EMOTION_STATE_IDLE;
  _playback.interruptRequested = false;
  _playback.frameErrorOccurred = false;
  _playback.currentFrameIndex = 0;
#if EMOTION_DEBUG_SERIAL
  Serial.println("[EMOTION] Tout annule, etat -> IDLE");
#endif
}

bool EmotionManager::isPlaying() {
  return _playback.state != EMOTION_STATE_IDLE;
}

EmotionPlayState EmotionManager::getState() {
  return _playback.state;
}

String EmotionManager::getCurrentPlayingKey() {
  if (_playback.state == EMOTION_STATE_IDLE || !_loaded) {
    return "";
  }
  return _currentEmotion.key;
}
