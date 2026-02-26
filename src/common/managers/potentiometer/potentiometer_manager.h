#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire de potentiomètre analogique (WH148)
 * 
 * Ce module gère un potentiomètre analogique 3 pins via ADC.
 * Utilisé pour contrôler le volume, la luminosité, etc.
 * 
 * Supporte plusieurs instances pour gérer plusieurs potentiomètres.
 * 
 * Fonctionnalités:
 * - Lecture de la valeur brute (0-4095 sur ESP32)
 * - Conversion en pourcentage (0-100%)
 * - Détection de changement avec hystérésis
 * - Callback sur changement de valeur
 * 
 * Exemple d'utilisation:
 *   Potentiometer potVolume(34, "Volume");
 *   Potentiometer potBrightness(35, "Brightness");
 *   
 *   potVolume.init();
 *   potVolume.setCallback([](uint8_t newVal, uint8_t oldVal) {
 *     // Gérer le changement de volume
 *   });
 */

class Potentiometer;

// Type de callback pour les changements de valeur
typedef void (*PotentiometerCallback)(uint8_t newValue, uint8_t oldValue);

/**
 * Classe Potentiometer - Instance unique pour chaque potentiomètre
 */
class Potentiometer {
public:
  /**
   * Constructeur
   * @param pin Le pin GPIO ADC à utiliser
   * @param name Nom du potentiomètre (pour les logs)
   */
  Potentiometer(uint8_t pin, const char* name = "POT");
  
  /**
   * Initialiser le potentiomètre
   * @return true si l'initialisation est réussie
   */
  bool init();
  
  /**
   * Vérifier si le potentiomètre est disponible
   * @return true si disponible
   */
  bool isAvailable() const;
  
  /**
   * Lire la valeur brute (0-4095)
   * @return Valeur ADC brute
   */
  uint16_t readRaw();
  
  /**
   * Lire la valeur en pourcentage (0-100)
   * @return Valeur en pourcentage
   */
  uint8_t readPercent();
  
  /**
   * Obtenir la dernière valeur lue (en pourcentage)
   * @return Dernière valeur en pourcentage
   */
  uint8_t getLastValue() const;
  
  /**
   * Obtenir le pin GPIO utilisé
   * @return Numéro du pin GPIO
   */
  uint8_t getPin() const;
  
  /**
   * Obtenir le nom du potentiomètre
   * @return Nom du potentiomètre
   */
  const char* getName() const;
  
  /**
   * Mettre à jour et vérifier si la valeur a changé
   * @return true si la valeur a changé significativement
   */
  bool update();
  
  /**
   * Définir le callback appelé lors d'un changement de valeur
   * @param callback Fonction à appeler (newValue, oldValue)
   */
  void setCallback(PotentiometerCallback callback);
  
  /**
   * Définir le seuil de changement (hystérésis)
   * @param threshold Seuil en pourcentage (défaut: 3%)
   */
  void setThreshold(uint8_t threshold);
  
  /**
   * Afficher les informations du potentiomètre
   */
  void printInfo();

private:
  uint8_t _pin;
  const char* _name;
  bool _initialized;
  bool _available;
  uint8_t _lastValue;
  uint8_t _threshold;
  PotentiometerCallback _callback;
  
  static const uint16_t ADC_MAX = 4095;
  static const uint8_t SAMPLES = 5;
};

/**
 * PotentiometerManager - Gestionnaire global (rétrocompatibilité)
 * 
 * Gère le potentiomètre par défaut défini dans config.h
 * Pour plusieurs potentiomètres, utiliser directement la classe Potentiometer
 */
class PotentiometerManager {
public:
  static bool init();
  static bool isAvailable();
  static bool isInitialized();
  static uint16_t readRaw();
  static uint8_t readPercent();
  static uint8_t getLastValue();
  static bool update();
  static void setCallback(PotentiometerCallback callback);
  static void setThreshold(uint8_t threshold);
  static void printInfo();
  
  // Accès à l'instance par défaut
  static Potentiometer* getDefault();

private:
  static Potentiometer* _defaultPot;
  static bool _initialized;
};

#endif // POTENTIOMETER_MANAGER_H
