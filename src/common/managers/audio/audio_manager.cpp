// audio_manager.cpp (corrigé + optimisé pour éviter les micro-coupures)
// - Audio.loop() tourne dans une task dédiée
// - Mutex utilisé aussi dans la task (non bloquant) pour éviter les races avec connecttoFS/stopSong
// - Timeout mutex réduit (5ms) pour ne pas créer de “trous” audio
// - flags partagés en volatile
// - isPlaying() thread-safe (try-lock court)

#include "audio_manager.h"
#include "models/model_config.h"
#include "common/config/core_config.h"
#include "common/managers/sd/sd_manager.h"
#include "common/managers/log/log_manager.h"
#include <SD.h>

#ifdef HAS_AUDIO
#include "Audio.h"

// Instance Audio globale (ESP32-audioI2S)
static Audio audio;
#endif

// =========================
// Variables statiques
// =========================
bool AudioManager::initialized = false;

// Partagées entre tasks -> volatile
volatile bool AudioManager::available = false;
volatile bool AudioManager::paused = false;
volatile bool AudioManager::threadRunning = false;

uint8_t AudioManager::currentVolume = 50;  // Volume en % (0-100)
String AudioManager::currentFile = "";

TaskHandle_t AudioManager::audioTaskHandle = nullptr;
SemaphoreHandle_t AudioManager::audioMutex = nullptr;

// Réglages sync
static constexpr TickType_t MUTEX_TIMEOUT_SHORT = pdMS_TO_TICKS(5);  // court, évite les trous
static constexpr TickType_t MUTEX_TIMEOUT_READ  = pdMS_TO_TICKS(1);  // ultra court

#ifdef HAS_AUDIO
// =========================
// Task audio dédiée
// =========================
void AudioManager::audioTask(void* parameter) {
  (void)parameter;

  LogManager::info("[AUDIO] Thread demarre sur Core %d, Priorite %d",
                xPortGetCoreID(), uxTaskPriorityGet(nullptr));

  threadRunning = true;

  while (true) {
    if (available && !paused) {
      // IMPORTANT : éviter course entre audio.loop() et connecttoFS/stopSong/pauseResume/setVolume
      if (audioMutex && xSemaphoreTake(audioMutex, 0) == pdTRUE) { // non-bloquant
        audio.loop();
        xSemaphoreGive(audioMutex);
      }
      // si mutex occupé, une commande (play/stop/etc) est en cours -> on saute ce tour
    }

    // Yield régulier pour stabilité avec WiFi/BLE/SD
    vTaskDelay(1); // ~1ms
  }
}
#endif

// =========================
// API
// =========================
bool AudioManager::init() {
  if (initialized) return available;

  initialized = true;
  available = false;
  paused = false;
  threadRunning = false;

#ifdef HAS_AUDIO
  LogManager::info("[AUDIO] Initialisation du gestionnaire audio...");

  // Mutex pour synchroniser l'accès à l'objet audio
  audioMutex = xSemaphoreCreateMutex();
  if (!audioMutex) {
    LogManager::error("[AUDIO] Impossible de creer le mutex");
    return false;
  }

  // Vérifier la SD
  if (!SDManager::isAvailable()) {
    LogManager::error("[AUDIO] Carte SD non disponible");
    return false;
  }

  // Pins I2S
  LogManager::info("[AUDIO] Pins I2S: BCLK=%d, LRC=%d, DOUT=%d",
                I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);

  // Config audio (protégée)
  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    
    // Augmenter le buffer I2S pour éviter les claquements
    // Plus de buffer = plus de tolérance aux micro-pauses SD/SPI
    audio.setI2SCommFMT_LSB(false);  // Format standard
    audio.setConnectionTimeout(500, 2000);  // Timeout connexion
    
    // Convertir le volume % en valeur interne (0-21)
    uint8_t internalVolume = (currentVolume * 21) / 100;
    audio.setVolume(internalVolume);
    
    LogManager::info("[AUDIO] Buffer d'entree: %d octets", audio.getInBufferSize());
    xSemaphoreGive(audioMutex);
  } else {
    Serial.println("[AUDIO] ERREUR: Timeout mutex pendant init");
    return false;
  }

  // Marquer dispo avant de lancer la task
  available = true;

  // Lancer la task audio (core/prio via core_config.h)
  BaseType_t result = xTaskCreatePinnedToCore(
      audioTask,
      "AudioTask",
      STACK_SIZE_AUDIO,
      nullptr,
      PRIORITY_AUDIO,
      &audioTaskHandle,
      CORE_AUDIO);

  if (result != pdPASS) {
    LogManager::error("[AUDIO] Impossible de creer le thread audio");
    available = false;
    return false;
  }

  LogManager::info("[AUDIO] Gestionnaire audio initialise avec thread dedie");
  LogManager::info("[AUDIO] Volume: %d%%, Core: %d, Priorite: %d",
                currentVolume, CORE_AUDIO, PRIORITY_AUDIO);
#else
  LogManager::info("[AUDIO] Audio non disponible sur ce modele");
#endif

  return available;
}

bool AudioManager::isAvailable() {
  return available;
}

void AudioManager::loop() {
  // Rien : le thread audio gère tout
}

bool AudioManager::play(const char* path) {
#ifdef HAS_AUDIO
  if (!available) {
    LogManager::error("[AUDIO] Audio non initialise");
    return false;
  }
  if (!path || strlen(path) == 0) {
    LogManager::error("[AUDIO] Chemin de fichier invalide");
    return false;
  }
  if (!SD.exists(path)) {
    Serial.printf("[AUDIO] ERREUR: Fichier non trouve: %s\n", path);
    return false;
  }

  // On évite de “geler” l’audio trop longtemps : timeout court
  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.stopSong();
    LogManager::info("[AUDIO] Lecture: %s", path);

    bool success = audio.connecttoFS(SD, path);

    if (success) {
      currentFile = path;
      paused = false;
      LogManager::info("[AUDIO] Lecture demarree");
    } else {
      currentFile = "";
      LogManager::error("[AUDIO] Impossible de lire le fichier");
    }

    xSemaphoreGive(audioMutex);
    return success;
  } else {
    LogManager::error("[AUDIO] Mutex occupe (play), reessaye");
    return false;
  }
#else
  return false;
#endif
}

void AudioManager::pause() {
#ifdef HAS_AUDIO
  if (!available) return;
  if (!isPlaying()) return;

  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.pauseResume();
    paused = true;
    xSemaphoreGive(audioMutex);
    LogManager::info("[AUDIO] Lecture en pause");
  } else {
    LogManager::warning("[AUDIO] Mutex occupe (pause)");
  }
#endif
}

void AudioManager::resume() {
#ifdef HAS_AUDIO
  if (!available) return;
  if (!paused) return;

  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.pauseResume();
    paused = false;
    xSemaphoreGive(audioMutex);
    Serial.println("[AUDIO] Lecture reprise");
  } else {
    Serial.println("[AUDIO] ERREUR: Mutex occupe (resume)");
  }
#endif
}

void AudioManager::stop() {
#ifdef HAS_AUDIO
  if (!available) return;

  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.stopSong();
    currentFile = "";
    paused = false;
    xSemaphoreGive(audioMutex);
    LogManager::info("[AUDIO] Lecture arretee");
  } else {
    LogManager::warning("[AUDIO] Mutex occupe (stop)");
  }
#endif
}

bool AudioManager::isPlaying() {
#ifdef HAS_AUDIO
  if (!available) return false;

  bool running = false;

  // Lecture thread-safe : try-lock très court
  if (audioMutex && xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_READ) == pdTRUE) {
    running = audio.isRunning();
    xSemaphoreGive(audioMutex);
  } else {
    // si mutex occupé, on se base sur l’état "paused" et le fait qu’un fichier est en cours
    // (c’est un fallback, évite de bloquer)
    running = (currentFile.length() > 0);
  }

  return running && !paused;
#else
  return false;
#endif
}

bool AudioManager::isPaused() {
  return paused;
}

void AudioManager::setVolume(uint8_t percent) {
#ifdef HAS_AUDIO
  // Limiter à 0-100%
  if (percent > 100) percent = 100;
  currentVolume = percent;

  if (!available) return;

  // Convertir le pourcentage en valeur interne (0-21)
  uint8_t internalVolume = (percent * 21) / 100;

  if (xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_SHORT) == pdTRUE) {
    audio.setVolume(internalVolume);
    xSemaphoreGive(audioMutex);
    LogManager::info("[AUDIO] Volume: %d%% (interne: %d/21)", percent, internalVolume);
  } else {
    LogManager::error("[AUDIO] Mutex occupe (setVolume)");
  }
#endif
}

uint8_t AudioManager::getVolume() {
  return currentVolume;  // Retourne le volume en %
}

void AudioManager::volumeUp() {
  // Augmenter de 5%
  uint8_t newVolume = currentVolume + 5;
  if (newVolume > 100) newVolume = 100;
  setVolume(newVolume);
}

void AudioManager::volumeDown() {
  // Diminuer de 5%
  if (currentVolume >= 5) {
    setVolume(currentVolume - 5);
  } else {
    setVolume(0);
  }
}

String AudioManager::getCurrentFile() {
  return currentFile;
}

uint32_t AudioManager::getDuration() {
#ifdef HAS_AUDIO
  if (!available) return 0;

  uint32_t d = 0;
  if (audioMutex && xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_READ) == pdTRUE) {
    d = audio.getAudioFileDuration();
    xSemaphoreGive(audioMutex);
  }
  return d;
#else
  return 0;
#endif
}

uint32_t AudioManager::getPosition() {
#ifdef HAS_AUDIO
  if (!available) return 0;

  uint32_t p = 0;
  if (audioMutex && xSemaphoreTake(audioMutex, MUTEX_TIMEOUT_READ) == pdTRUE) {
    p = audio.getAudioCurrentTime();
    xSemaphoreGive(audioMutex);
  }
  return p;
#else
  return 0;
#endif
}

void AudioManager::printStatus() {
  LogManager::info("");
  LogManager::info("========================================");
  LogManager::info("        STATUT AUDIO I2S");
  LogManager::info("========================================");

#ifdef HAS_AUDIO
  LogManager::info("  Disponible: %s", available ? "Oui" : "Non");
  LogManager::info("  Thread: %s", threadRunning ? "Actif" : "Inactif");
  LogManager::info("  Volume: %d%%", currentVolume);
  LogManager::info("  Core: %d, Priorite: %d", CORE_AUDIO, PRIORITY_AUDIO);

  if (available) {
    LogManager::info("  Pins I2S: BCLK=%d, LRC=%d, DOUT=%d",
                  I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);

    if (currentFile.length() > 0) {
      LogManager::info("  Fichier: %s", currentFile.c_str());
      LogManager::info("  Etat: %s", paused ? "En pause" : (isPlaying() ? "Lecture" : "Arrete"));

      uint32_t duration = getDuration();
      uint32_t position = getPosition();
      if (duration > 0) {
        LogManager::info("  Position: %lu/%lu sec", (unsigned long)position, (unsigned long)duration);
      }
    } else {
      LogManager::info("  Aucun fichier en lecture");
    }
  }
#else
  LogManager::info("  Audio non disponible sur ce modele");
#endif

  LogManager::info("========================================");
}
