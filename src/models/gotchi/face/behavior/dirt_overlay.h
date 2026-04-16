#ifndef DIRT_OVERLAY_H
#define DIRT_OVERLAY_H

#include <cstdint>

// DirtOverlay — taches de saleté dispersées sur l'écran du Gotchi.
// Positions aléatoires, le joueur frotte pour les enlever (mode wash).

namespace DirtOverlay {

// Générer des taches proportionnel au niveau d'hygiène (0-100).
void setDirty(float hygiene);

// Effacer toute la saleté.
void clear();

// Activer/désactiver le mode frottement.
void setWashMode(bool enabled);
bool isWashMode();

// Appelé à chaque fingerMove en mode wash. Retourne true si nettoyage complet.
bool onFingerMove(float screenX, float screenY);

// Appelé chaque frame pour gérer le cooldown post-nettoyage.
void update(uint32_t dtMs);

// Dessiner les taches dans le framebuffer principal (zone y=130-440).
void drawIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY);

// Dessiner les taches dans le top buffer (zone y=30-130).
void drawIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH);

// Dessiner l'éponge dans le framebuffer principal (alpha blend).
void drawSpongeIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY);

// Dessiner l'éponge dans le top buffer (zone y=30-130).
void drawSpongeIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH);

// --- Mode brossage de dents ---
void setBrushMode(bool enabled);
bool isBrushMode();
bool onBrushFingerMove(float screenX, float screenY);
void drawBrushIntoFB(uint16_t* fb, int16_t fbW, int16_t fbH, int16_t fbX, int16_t fbY);
void drawBrushIntoTopBuf(uint16_t* buf, int16_t bufW, int16_t bufH);

// Y a-t-il de la saleté à l'écran ?
bool isDirty();

} // namespace DirtOverlay

#endif // DIRT_OVERLAY_H
