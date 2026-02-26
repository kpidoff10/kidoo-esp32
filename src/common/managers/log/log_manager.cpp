#include "log_manager.h"
#include "../sd/sd_manager.h"
#include <SD.h>
#include <cstdarg>
#include <ctime>

// Variables statiques
bool LogManager::initialized = false;
LogLevel LogManager::currentLogLevel = LOG_LEVEL_INFO;
bool LogManager::sdLoggingEnabled = true;
const char* LogManager::ERROR_LOG_FILE = "/error_log.txt";
const size_t LogManager::MAX_LOG_LINE_SIZE = 512;

void LogManager::init() {
  if (initialized) {
    return;
  }
  
  initialized = true;
  currentLogLevel = LOG_LEVEL_INFO;
  sdLoggingEnabled = true;
  
  // Vérifier si la SD est disponible pour le logging
  if (!SDManager::isAvailable()) {
    sdLoggingEnabled = false;
    if (Serial) {
      Serial.println("[LOG] SD non disponible, logging sur SD desactive");
    }
  }
}

void LogManager::setLogLevel(LogLevel level) {
  currentLogLevel = level;
}

void LogManager::setSDLoggingEnabled(bool enabled) {
  sdLoggingEnabled = enabled && SDManager::isAvailable();
}

void LogManager::debug(const char* format, ...) {
  if (currentLogLevel > LOG_LEVEL_DEBUG) {
    return;
  }
  
  va_list args;
  va_start(args, format);
  log(LOG_LEVEL_DEBUG, "[DEBUG]", format, args);
  va_end(args);
}

void LogManager::info(const char* format, ...) {
  if (currentLogLevel > LOG_LEVEL_INFO) {
    return;
  }
  
  va_list args;
  va_start(args, format);
  log(LOG_LEVEL_INFO, "[INFO]", format, args);
  va_end(args);
}

void LogManager::warning(const char* format, ...) {
  if (currentLogLevel > LOG_LEVEL_WARNING) {
    return;
  }
  
  va_list args;
  va_start(args, format);
  log(LOG_LEVEL_WARNING, "[WARNING]", format, args);
  va_end(args);
}

void LogManager::error(const char* format, ...) {
  if (currentLogLevel > LOG_LEVEL_ERROR) {
    return;
  }
  
  va_list args;
  va_start(args, format);
  char message[MAX_LOG_LINE_SIZE];
  vsnprintf(message, MAX_LOG_LINE_SIZE, format, args);
  va_end(args);
  
  // Afficher sur Serial
  va_list args2;
  va_start(args2, format);
  log(LOG_LEVEL_ERROR, "[ERROR]", format, args2);
  va_end(args2);
  
  // Écrire sur SD si activé
  if (sdLoggingEnabled) {
    writeErrorToSD(message);
  }
}

void LogManager::log(LogLevel level, const char* prefix, const char* format, va_list args) {
  if (!Serial) {
    return;
  }
  
  // Formater le timestamp
  char timestamp[32];
  formatTimestamp(timestamp, sizeof(timestamp));
  
  // Afficher le timestamp et le préfixe
  Serial.print(timestamp);
  Serial.print(" ");
  Serial.print(prefix);
  Serial.print(" ");
  
  // Formater et afficher le message
  char buffer[MAX_LOG_LINE_SIZE];
  vsnprintf(buffer, MAX_LOG_LINE_SIZE, format, args);
  Serial.println(buffer);
}

void LogManager::writeErrorToSD(const char* message) {
  if (!SDManager::isAvailable()) {
    return;
  }
  
  // Ouvrir le fichier en mode append
  File logFile = SD.open(ERROR_LOG_FILE, FILE_APPEND);
  if (!logFile) {
    // Si le fichier n'existe pas, le créer
    logFile = SD.open(ERROR_LOG_FILE, FILE_WRITE);
    if (!logFile) {
      return; // Impossible d'écrire
    }
  }
  
  // Formater le timestamp
  char timestamp[32];
  formatTimestamp(timestamp, sizeof(timestamp));
  
  // Écrire la ligne de log
  logFile.print(timestamp);
  logFile.print(" [ERROR] ");
  logFile.println(message);
  
  logFile.close();
}

void LogManager::formatTimestamp(char* buffer, size_t bufferSize) {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  snprintf(buffer, bufferSize, "[%02lu:%02lu:%02lu.%03lu]",
           hours, minutes % 60, seconds % 60, ms % 1000);
}

bool LogManager::clearErrorLog() {
  if (!SDManager::isAvailable()) {
    return false;
  }
  
  // Supprimer le fichier s'il existe
  if (SD.exists(ERROR_LOG_FILE)) {
    return SD.remove(ERROR_LOG_FILE);
  }
  
  return true; // Fichier n'existait pas, considéré comme réussi
}

size_t LogManager::getErrorLogSize() {
  if (!SDManager::isAvailable()) {
    return 0;
  }
  
  if (!SD.exists(ERROR_LOG_FILE)) {
    return 0;
  }
  
  File logFile = SD.open(ERROR_LOG_FILE, FILE_READ);
  if (!logFile) {
    return 0;
  }
  
  size_t size = logFile.size();
  logFile.close();
  
  return size;
}
