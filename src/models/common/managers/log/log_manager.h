#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire de logs avec écriture sur Serial et SD
 * 
 * Ce module gère les logs avec différents niveaux :
 * - INFO : Affiché sur Serial uniquement
 * - DEBUG : Affiché sur Serial uniquement (si activé)
 * - ERROR : Affiché sur Serial ET écrit dans un fichier sur la SD
 */

// Niveaux de log
enum LogLevel {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO = 1,
  LOG_LEVEL_WARNING = 2,
  LOG_LEVEL_ERROR = 3
};

class LogManager {
public:
  /**
   * Initialiser le gestionnaire de logs
   */
  static void init();
  
  /**
   * Définir le niveau de log minimum (les logs en dessous ne seront pas affichés)
   * @param level Niveau minimum (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
   */
  static void setLogLevel(LogLevel level);
  
  /**
   * Activer/désactiver l'écriture des erreurs sur la SD
   * @param enabled true pour activer, false pour désactiver
   */
  static void setSDLoggingEnabled(bool enabled);
  
  /**
   * Logger un message de debug
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void debug(const char* format, ...);
  
  /**
   * Logger un message d'information
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void info(const char* format, ...);
  
  /**
   * Logger un message d'avertissement
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void warning(const char* format, ...);
  
  /**
   * Logger un message d'erreur (écrit aussi sur SD)
   * @param format Format du message (comme printf)
   * @param ... Arguments variables
   */
  static void error(const char* format, ...);
  
  /**
   * Vider le fichier de logs d'erreur sur la SD
   * @return true si réussi, false sinon
   */
  static bool clearErrorLog();
  
  /**
   * Obtenir la taille du fichier de logs d'erreur
   * @return Taille en octets, 0 si erreur
   */
  static size_t getErrorLogSize();

private:
  /**
   * Logger un message avec un niveau spécifique
   * @param level Niveau du log
   * @param prefix Préfixe du log (ex: "[ERROR]")
   * @param format Format du message
   * @param args Arguments variables
   */
  static void log(LogLevel level, const char* prefix, const char* format, va_list args);
  
  /**
   * Écrire un message d'erreur dans le fichier sur la SD
   * @param message Message à écrire
   */
  static void writeErrorToSD(const char* message);
  
  /**
   * Formater un timestamp pour les logs
   * @param buffer Buffer pour stocker le timestamp
   * @param bufferSize Taille du buffer
   */
  static void formatTimestamp(char* buffer, size_t bufferSize);
  
  static bool initialized;
  static LogLevel currentLogLevel;
  static bool sdLoggingEnabled;
  static const char* ERROR_LOG_FILE;
  static const size_t MAX_LOG_LINE_SIZE;
};

#endif // LOG_MANAGER_H
