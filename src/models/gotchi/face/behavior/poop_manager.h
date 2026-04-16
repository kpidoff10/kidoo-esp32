#ifndef POOP_MANAGER_H
#define POOP_MANAGER_H

#include <cstdint>

// PoopManager — système ambiant de caca pour le Gotchi.
// Les cacas apparaissent aléatoirement, le joueur doit taper dessus pour les nettoyer.
// Trop de cacas ou des cacas ignorés dégradent hygiène et santé.

namespace PoopManager {

void init();

// Appeler dans la boucle principale (BehaviorEngine::update)
void update(uint32_t dtMs);

// Tester si un touch (x,y) touche un caca. Retourne true si un caca a été nettoyé.
bool onTouchAt(int16_t x, int16_t y);

// Nettoyer tous les cacas (appelé par face clean)
void cleanAll();

// Forcer l'apparition d'un caca (debug/serial)
void spawnOne();

// Nombre de cacas actuellement à l'écran
int getCount();

} // namespace PoopManager

#endif // POOP_MANAGER_H
