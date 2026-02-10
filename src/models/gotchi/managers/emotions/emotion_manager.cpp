#include "emotion_manager.h"
#include "../../../model_config.h"
#include "../../../common/managers/lcd/lcd_manager.h"

#if defined(HAS_SD)
#include <SD.h>
#include <ArduinoJson.h>
#endif

String EmotionManager::_characterId = "";
EmotionData EmotionManager::_currentEmotion;
bool EmotionManager::_loaded = false;

bool EmotionManager::init() {
  _loaded = false;
  _characterId = "";

#if defined(HAS_SD)
  // Charger le characterId depuis /config.json
  if (!loadCharacterId()) {
    Serial.println("[EMOTION] Erreur: Impossible de charger characterId depuis /config.json");
    return false;
  }

  Serial.printf("[EMOTION] CharacterId charge: %s\n", _characterId.c_str());
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
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[EMOTION] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  if (!doc.containsKey("characterId")) {
    Serial.println("[EMOTION] Erreur: characterId manquant dans /config.json");
    return false;
  }

  _characterId = doc["characterId"].as<String>();
  return true;
#else
  return false;
#endif
}

bool EmotionManager::loadEmotion(const String& emotionKey) {
#if defined(HAS_SD)
  if (_characterId.isEmpty()) {
    Serial.println("[EMOTION] Erreur: characterId non charge, appelez init() d'abord");
    return false;
  }

  // Construire le chemin du fichier de config
  String configPath = "/characters/" + _characterId + "/emotions/config.json";

  Serial.printf("[EMOTION] Chargement emotion '%s' depuis %s\n", emotionKey.c_str(), configPath.c_str());

  if (!parseEmotionConfig(configPath, emotionKey)) {
    Serial.printf("[EMOTION] Erreur: Impossible de charger l'emotion '%s'\n", emotionKey.c_str());
    return false;
  }

  // Construire l'index des frames pour accès direct
  Serial.println("[EMOTION] Construction de l'index des frames...");
  if (!buildFrameIndex()) {
    Serial.println("[EMOTION] Erreur: Impossible de construire l'index des frames");
    return false;
  }

  _loaded = true;
  Serial.printf("[EMOTION] Emotion '%s' chargee: %d frames (intro:%d, loop:%d, exit:%d)\n",
                _currentEmotion.key.c_str(), _currentEmotion.totalFrames,
                _currentEmotion.intro.frames, _currentEmotion.loop.frames, _currentEmotion.exit.frames);
  Serial.printf("[EMOTION] Index construit: %d frames indexees\n", _currentEmotion.frameOffsets.size());

  return true;
#else
  return false;
#endif
}

bool EmotionManager::parseEmotionConfig(const String& jsonPath, const String& emotionKey) {
#if defined(HAS_SD)
  File file = SD.open(jsonPath.c_str(), FILE_READ);
  if (!file) {
    Serial.printf("[EMOTION] Erreur: fichier introuvable: %s\n", jsonPath.c_str());
    return false;
  }

  // Parser le JSON (tableau d'émotions)
  // Taille du document : ajustez selon la taille de votre JSON
  DynamicJsonDocument doc(32768); // 32KB devrait suffire pour la config
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[EMOTION] Erreur parsing JSON: %s\n", error.c_str());
    return false;
  }

  // Le JSON est un tableau, chercher l'émotion avec la bonne clé
  JsonArray emotions = doc.as<JsonArray>();
  JsonObject emotionObj;
  bool found = false;

  for (JsonObject obj : emotions) {
    if (obj["key"].as<String>() == emotionKey) {
      emotionObj = obj;
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.printf("[EMOTION] Erreur: emotion '%s' non trouvee dans le JSON\n", emotionKey.c_str());
    return false;
  }

  // Extraire les données de l'émotion
  _currentEmotion.key = emotionKey;
  _currentEmotion.emotionId = emotionObj["emotionId"].as<String>();

  // Les données de la vidéo sont dans emotion_videos[0]
  if (!emotionObj.containsKey("emotion_videos") || !emotionObj["emotion_videos"].is<JsonArray>()) {
    Serial.println("[EMOTION] Erreur: emotion_videos manquant ou invalide");
    return false;
  }

  JsonObject video = emotionObj["emotion_videos"][0];
  if (!video) {
    Serial.println("[EMOTION] Erreur: aucune video dans emotion_videos");
    return false;
  }

  // Extraire l'emotion_videoId pour construire le chemin
  String emotionVideoId = video["emotion_videoId"].as<String>();
  if (emotionVideoId.isEmpty()) {
    Serial.println("[EMOTION] Erreur: emotion_videoId manquant dans emotion_videos[0]");
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
  _currentEmotion.mjpegPath = "/characters/" + _characterId + "/emotions/" + emotionKey + "/" + emotionVideoId + "/video.mjpeg";

  Serial.printf("[EMOTION] Chemin MJPEG: %s\n", _currentEmotion.mjpegPath.c_str());

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
    Serial.printf("[EMOTION] Chargement index depuis: %s\n", idxPath.c_str());

    // Lire le nombre de frames (4 bytes, little-endian)
    if (idxFile.available() < 4) {
      Serial.println("[EMOTION] Erreur: fichier .idx trop court");
      idxFile.close();
      return false;
    }

    uint32_t frameCount = 0;
    idxFile.read((uint8_t*)&frameCount, 4);

    Serial.printf("[EMOTION] Index contient %u frames\n", frameCount);

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
    Serial.printf("[EMOTION] Index charge: %u frames indexees en %u bytes\n",
                  _currentEmotion.frameOffsets.size(), 4 + frameCount * 8);
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

  Serial.printf("[EMOTION] Index construit: %d frames trouvees\n", _currentEmotion.frameOffsets.size());
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

void EmotionManager::playPhase(const EmotionPhase& phase) {
#if defined(HAS_LCD) && defined(HAS_SD)
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return;
  }

  if (!LCDManager::isAvailable()) {
    Serial.println("[EMOTION] Erreur: LCD non disponible");
    return;
  }

  if (phase.timeline.empty()) {
    Serial.println("[EMOTION] Phase vide, rien a jouer");
    return;
  }

  if (_currentEmotion.frameOffsets.empty()) {
    Serial.println("[EMOTION] Erreur: Index des frames non construit");
    return;
  }

  Serial.printf("[EMOTION] Lecture phase: %d frames dans la timeline\n", phase.timeline.size());

  File f = SD.open(_currentEmotion.mjpegPath.c_str(), FILE_READ);
  if (!f) {
    Serial.printf("[EMOTION] Erreur: fichier MJPEG introuvable: %s\n", _currentEmotion.mjpegPath.c_str());
    return;
  }

  // Allouer un buffer pour la plus grande frame possible (on va réutiliser ce buffer)
  const size_t MAX_FRAME_SIZE = 131072;  // 128 KB max par frame
  uint8_t* frameBuf = (uint8_t*)malloc(MAX_FRAME_SIZE);
  if (!frameBuf) {
    f.close();
    Serial.println("[EMOTION] Erreur allocation memoire");
    return;
  }

  const uint32_t FRAME_MS = 1000 / (_currentEmotion.fps > 0 ? _currentEmotion.fps : 10);
  int playedFrames = 0;

  // Parcourir la timeline et afficher chaque frame
  for (const auto& tf : phase.timeline) {
    int frameIdx = tf.sourceFrameIndex;

    // Vérifier que l'index est valide
    if (frameIdx < 0 || frameIdx >= (int)_currentEmotion.frameOffsets.size()) {
      Serial.printf("[EMOTION] Erreur: Index frame invalide: %d\n", frameIdx);
      continue;
    }

    const FrameIndex& idx = _currentEmotion.frameOffsets[frameIdx];

    // Vérifier la taille de la frame
    if (idx.frameSize > MAX_FRAME_SIZE) {
      Serial.printf("[EMOTION] Erreur: Frame %d trop grande (%d bytes)\n", frameIdx, idx.frameSize);
      continue;
    }

    uint32_t frameStart = millis();

    // Seek à la position de la frame et lire directement
    if (!f.seek(idx.fileOffset)) {
      Serial.printf("[EMOTION] Erreur seek frame %d\n", frameIdx);
      continue;
    }

    int bytesRead = f.read(frameBuf, idx.frameSize);
    if (bytesRead != (int)idx.frameSize) {
      Serial.printf("[EMOTION] Erreur lecture frame %d: lu %d/%d bytes\n", frameIdx, bytesRead, idx.frameSize);
      continue;
    }

    // Afficher la frame
    LCDManager::displayJpegFrame(frameBuf, idx.frameSize);
    playedFrames++;

    // Respecter le timing FPS
    uint32_t elapsed = millis() - frameStart;
    if (elapsed < FRAME_MS) {
      delay(FRAME_MS - elapsed);
    }
  }

  free(frameBuf);
  f.close();

  Serial.printf("[EMOTION] Phase terminee: %d frames jouees\n", playedFrames);
#else
  Serial.println("[EMOTION] LCD ou SD non disponible");
#endif
}

void EmotionManager::playIntro() {
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return;
  }

  Serial.println("[EMOTION] === INTRO START ===");
  playPhase(_currentEmotion.intro);
  Serial.println("[EMOTION] === INTRO END ===");
}

void EmotionManager::playLoop() {
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return;
  }

  Serial.println("[EMOTION] === LOOP START ===");
  playPhase(_currentEmotion.loop);
  Serial.println("[EMOTION] === LOOP END ===");
}

void EmotionManager::playExit() {
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return;
  }

  Serial.println("[EMOTION] === EXIT START ===");
  playPhase(_currentEmotion.exit);
  Serial.println("[EMOTION] === EXIT END ===");
}

void EmotionManager::playAll(int loopCount) {
  if (!_loaded) {
    Serial.println("[EMOTION] Erreur: Aucune emotion chargee");
    return;
  }

  Serial.printf("[EMOTION] === PLAY ALL START (INTRO -> LOOP x%d -> EXIT) ===\n", loopCount);

  // Créer une timeline combinée pour éviter les coupures entre phases
  EmotionPhase combinedPhase;
  combinedPhase.frames = _currentEmotion.intro.frames +
                        (_currentEmotion.loop.frames * loopCount) +
                        _currentEmotion.exit.frames;

  // Ajouter intro
  Serial.println("[EMOTION] === INTRO ===");
  for (const auto& frame : _currentEmotion.intro.timeline) {
    combinedPhase.timeline.push_back(frame);
  }

  // Ajouter loop × N
  for (int i = 0; i < loopCount; i++) {
    Serial.printf("[EMOTION] === LOOP %d/%d ===\n", i + 1, loopCount);
    for (const auto& frame : _currentEmotion.loop.timeline) {
      combinedPhase.timeline.push_back(frame);
    }
  }

  // Ajouter exit
  Serial.println("[EMOTION] === EXIT ===");
  for (const auto& frame : _currentEmotion.exit.timeline) {
    combinedPhase.timeline.push_back(frame);
  }

  // Jouer toute la séquence d'un coup sans interruption
  playPhase(combinedPhase);

  Serial.println("[EMOTION] === PLAY ALL END ===");
}
