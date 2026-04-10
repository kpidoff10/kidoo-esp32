#ifndef GOTCHI_HAPTIC_H
#define GOTCHI_HAPTIC_H

#include <cstdint>

/**
 * Haptique sémantique du Gotchi
 *
 * Petit wrapper non-bloquant au-dessus de VibratorManager qui mappe
 * des effets de jeu et d'émotions à des pulsations courtes.
 *
 * - Toutes les fonctions sont safe à appeler à toute fréquence
 *   (rate-limit interne pour éviter le buzzing continu).
 * - Aucune fonction n'utilise delay() : l'arrêt est géré par update().
 * - Si HAS_VIBRATOR n'est pas défini sur le modèle, les fonctions
 *   sont des no-op (compilation propre sur Dream/Sound).
 */
namespace GotchiHaptic {

// À appeler dans la boucle principale (gère l'auto-stop)
void update();

// --- Jeu : balle ---
void ballBounce();   // Rebond au sol : tap court et sec
void ballThrow();    // Lancer/relâche : pulse plus marqué
void ballCatch();    // Prise au doigt : tap léger

// --- Émotions ---
void joyTap();       // Happy : petite pulsation joyeuse
void angerBurst();   // Tantrum : pulsation forte
void angerShake();   // Tantrum : micro-pulse pendant le tremblement
void chew();         // Eating : pulse de mastication
void cough();        // Sick : pulse faible et lente
void heartBeat();    // (libre) battement de cœur

} // namespace GotchiHaptic

#endif // GOTCHI_HAPTIC_H
