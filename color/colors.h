#ifndef COLORS_H
#define COLORS_H

/**
 * Définitions des couleurs par défaut
 * 
 * Ce fichier contient les définitions RGB pour les couleurs utilisées
 * dans le système, afin d'éviter de réécrire les codes à chaque fois.
 */

// ============================================
// Couleurs de base (RGB)
// ============================================

// Rouge
#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

// Vert
#define COLOR_GREEN_R 0
#define COLOR_GREEN_G 255
#define COLOR_GREEN_B 0

// Bleu
#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 0
#define COLOR_BLUE_B 255

// Orange
#define COLOR_ORANGE_R 255
#define COLOR_ORANGE_G 102
#define COLOR_ORANGE_B 0

// Jaune
#define COLOR_YELLOW_R 255
#define COLOR_YELLOW_G 255
#define COLOR_YELLOW_B 0

// Blanc
#define COLOR_WHITE_R 255
#define COLOR_WHITE_G 255
#define COLOR_WHITE_B 255

// Noir (éteint)
#define COLOR_BLACK_R 0
#define COLOR_BLACK_G 0
#define COLOR_BLACK_B 0

// Cyan
#define COLOR_CYAN_R 0
#define COLOR_CYAN_G 255
#define COLOR_CYAN_B 255

// Magenta
#define COLOR_MAGENTA_R 255
#define COLOR_MAGENTA_G 0
#define COLOR_MAGENTA_B 255

// Violet
#define COLOR_PURPLE_R 128
#define COLOR_PURPLE_G 0
#define COLOR_PURPLE_B 128

// ============================================
// Macros pour utiliser les couleurs facilement
// ============================================
// Ces macros s'expandent en 3 paramètres (R, G, B) pour setColor()

#define COLOR_RED COLOR_RED_R, COLOR_RED_G, COLOR_RED_B
#define COLOR_GREEN COLOR_GREEN_R, COLOR_GREEN_G, COLOR_GREEN_B
#define COLOR_BLUE COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B
#define COLOR_ORANGE COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B
#define COLOR_YELLOW COLOR_YELLOW_R, COLOR_YELLOW_G, COLOR_YELLOW_B
#define COLOR_WHITE COLOR_WHITE_R, COLOR_WHITE_G, COLOR_WHITE_B
#define COLOR_BLACK COLOR_BLACK_R, COLOR_BLACK_G, COLOR_BLACK_B
#define COLOR_CYAN COLOR_CYAN_R, COLOR_CYAN_G, COLOR_CYAN_B
#define COLOR_MAGENTA COLOR_MAGENTA_R, COLOR_MAGENTA_G, COLOR_MAGENTA_B
#define COLOR_PURPLE COLOR_PURPLE_R, COLOR_PURPLE_G, COLOR_PURPLE_B

// ============================================
// Couleurs pour les états du système
// ============================================

// Couleur d'erreur (rouge)
#define COLOR_ERROR COLOR_RED

// Couleur de succès (vert)
#define COLOR_SUCCESS COLOR_GREEN

// Couleur d'avertissement (orange)
#define COLOR_WARNING COLOR_ORANGE

// Couleur système prêt (orange qui tourne)
#define COLOR_SYSTEM_READY COLOR_ORANGE

#endif // COLORS_H
