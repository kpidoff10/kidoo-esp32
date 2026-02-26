#ifndef COMMON_DEFAULT_CONFIG_H
#define COMMON_DEFAULT_CONFIG_H

/**
 * Configuration par défaut commune à tous les modèles
 * 
 * Ce fichier contient les valeurs par défaut qui sont
 * communes à tous les modèles Kidoo
 */

// ============================================
// Configuration par défaut commune
// ============================================

// Timeout pour le sleep mode (en millisecondes)
// 0 = désactivé, sinon temps d'inactivité avant extinction des LEDs
// Minimum: 5000 ms (5 secondes)
#define DEFAULT_SLEEP_TIMEOUT_MS 10000   // 10 secondes par défaut
#define MIN_SLEEP_TIMEOUT_MS 5000        // Minimum: 5 secondes
#define SLEEP_FADE_DURATION_MS 1000      // Durée de l'animation de fade-out (1 seconde)

// Logs verbeux (processCommand, effet sauvegardé, etc.) - 0 = désactivé, 1 = activé
#define ENABLE_VERBOSE_LOGS 0

// URL de base de l'API serveur - définie dans include/app_config.h (racine du projet)
#include "app_config.h"

#endif // COMMON_DEFAULT_CONFIG_H
