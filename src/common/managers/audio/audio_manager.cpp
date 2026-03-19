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

volatile uint8_t AudioManager::currentVolume = 50;  // Volume en % (0-100)
volatile uint8_t AudioManager::pendingVolume = 50;  // Volume en attente (lock-free)
volatile bool AudioManager::volumeChanged = false;  // Flag changement volume
volatile bool AudioManager::stopRequested = false;  // Flag stop demandé
volatile bool AudioManager::pauseRequested = false; // Flag pause demandé
volatile bool AudioManager::resumeRequested = false;// Flag resume demandé
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
    // Traiter les commandes lock-free (pas d'attente mutex)
    if (audioMutex && xSemaphoreTake(audioMutex, 0) == pdTRUE) {
      // Stop a la priorité la plus haute
      if (stopRequested) {
        audio.stopSong();
        currentFile = "";
        paused = false;
        stopRequested = false;
        LogManager::info("[AUDIO] Lecture arretee");
      }
      // Puis Pause
      else if (pauseRequested) {
        audio.pauseResume();
        paused = true;
        pauseRequested = false;
        LogManager::info("[AUDIO] Lecture en pause");
      }
      // Puis Resume
      else if (resumeRequested) {
        audio.pauseResume();
        paused = false;
        resumeRequested = false;
        LogManager::info("[AUDIO] Lecture reprise");
      }

      // Volume peut être appliqué en même temps
      if (volumeChanged) {
        uint8_t newVolume = pendingVolume;
        uint8_t internalVolume = (newVolume * 21) / 100;
        audio.setVolume(internalVolume);
        currentVolume = newVolume;
        volumeChanged = false;
        LogManager::info("[AUDIO] Volume appliqué: %d%% (interne: %d/21)", newVolume, internalVolume);
      }

      xSemaphoreGive(audioMutex);
    }

    if (available && !paused) {
      // Lire l'audio (non-bloquant)
      if (audioMutex && xSemaphoreTake(audioMutex, 0) == pdTRUE) {
        audio.loop();
        xSemaphoreGive(audioMutex);
      }
      // Si mutex occupé, on saute ce tour
    }

    // Courte pause pour équilibrer CPU entre audio et autres tâches (WiFi/BLE/MQTT)
    vTaskDelay(pdMS_TO_TICKS(1)); // ~1ms
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

    // Optimiser pour MP3 192 kbps stéréo 44.1 kHz
    // Augmenter buffers : RAM=32KB, PSRAM=300KB pour meilleure tolérance underruns
    audio.setBufsize(32 * 1024, 300 * 1024);

    audio.setI2SCommFMT_LSB(false);  // Format standard
    audio.setConnectionTimeout(500, 2000);  // Timeout connexion

    // Convertir le volume % en valeur interne (0-21)
    uint8_t internalVolume = (currentVolume * 21) / 100;
    audio.setVolume(internalVolume);

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

  // Approche lock-free : signaler la demande et laisser la task audio l'exécuter
  pauseRequested = true;
  LogManager::info("[AUDIO] Pause demandée");
#endif
}

void AudioManager::resume() {
#ifdef HAS_AUDIO
  if (!available) return;

  // Approche lock-free : signaler la demande et laisser la task audio l'exécuter
  resumeRequested = true;
  LogManager::info("[AUDIO] Resume demandé");
#endif
}

void AudioManager::stop() {
#ifdef HAS_AUDIO
  if (!available) return;

  // Approche lock-free : signaler la demande et laisser la task audio l'exécuter
  stopRequested = true;
  LogManager::info("[AUDIO] Stop demandé");
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

  if (!available) return;

  // Approche lock-free : signaler le changement et laisser la task audio l'appliquer
  // Plus rapide que d'attendre le mutex, idéal pour les changements de volume fréquents
  pendingVolume = percent;
  volumeChanged = true;  // Flag volatile pour la task audio

  LogManager::info("[AUDIO] Volume en attente: %d%%", percent);
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
