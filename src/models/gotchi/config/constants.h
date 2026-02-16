#ifndef MODEL_GOTCHI_CONSTANTS_H
#define MODEL_GOTCHI_CONSTANTS_H

/**
 * Constantes pour le modèle Kidoo Gotchi
 *
 * Fichier généré par kidoo-shared/scripts/generate-esp32-constants.ts
 * Ne pas éditer à la main. Source de vérité : emotions/constants.ts (EATING_VARIANTS, TRIGGER_EFFECTS).
 */

// ============================================
// NFC Keys - Simulation keys for testing
// ============================================

// --- Food items ---
#define NFC_KEY_BOTTLE "BOTTLE"
#define NFC_KEY_SNACK "SNACK"
#define NFC_KEY_APPLE "APPLE"
#define NFC_KEY_CANDY "CANDY"

// --- Hygiene items (future) ---
#define NFC_KEY_TOOTHBRUSH "BRUSH"
#define NFC_KEY_SOAP "SOAP"

// --- Sleep items (future) ---
#define NFC_KEY_BED "BED"

// ============================================
// NFC Key Mapping Structure
// ============================================

struct NFCKeyMapping {
  const char* key;      // Clé NFC (affichage / fallback texte)
  const char* itemId;   // ID de l'objet (correspond aux NFC_ITEM_* dans config.h)
  const char* name;     // Nom lisible de l'objet
  uint8_t variant;      // Code écrit sur le tag (1-4) pour reconnaissance fiable sans corruption
};

// Table de mapping des clés NFC vers les objets
static const NFCKeyMapping NFC_KEY_TABLE[] = {
  {NFC_KEY_BOTTLE, "bottle", "Bottle", 1},
  {NFC_KEY_SNACK, "cake", "Cake", 2},
  {NFC_KEY_APPLE, "apple", "Apple", 3},
  {NFC_KEY_CANDY, "candy", "Candy", 4},
  // Future items (commented out until implemented)
  // {NFC_KEY_TOOTHBRUSH, "toothbrush", "Toothbrush", 0},
  // {NFC_KEY_SOAP, "soap", "Soap", 0},
  // {NFC_KEY_BED, "bed", "Bed", 0},
};

#define NFC_KEY_TABLE_SIZE (sizeof(NFC_KEY_TABLE) / sizeof(NFCKeyMapping))

// ============================================
// Progressive Effect Structure
// ============================================

struct ProgressiveFoodEffect {
  const char* itemId;           // ID de l'item (bottle, cake, apple, candy)
  uint8_t tickHunger;           // Hunger donné par tick
  uint8_t tickHappiness;        // Happiness donné par tick
  uint8_t tickHealth;           // Health donné par tick
  uint8_t tickSickness;         // Maladie donnée par tick (stat 0→augmente)
  unsigned long tickInterval;   // Intervalle entre chaque tick (ms)
  uint8_t totalTicks;           // Nombre total de ticks
};

// Ordre des entrées = ordre des variants (1, 2, 3, 4) dans EATING_VARIANTS
static const ProgressiveFoodEffect PROGRESSIVE_FOOD_EFFECTS[] = {
  { "bottle", 5, 1, 0, 0, 5000UL, 0 },
  { "cake", 1, 5, 0, 1, 8000UL, 2 },
  { "apple", 2, 0, 1, 0, 5000UL, 4 },
  { "candy", 1, 2, 2, 0, 6000UL, 3 },
};

#define PROGRESSIVE_FOOD_EFFECTS_SIZE (sizeof(PROGRESSIVE_FOOD_EFFECTS) / sizeof(ProgressiveFoodEffect))

// ============================================
// Trigger stat effects (instant: hunger, happiness, health, fatigue, hygiene)
// ============================================

struct TriggerStatEffect {
  int8_t hunger;
  int8_t happiness;
  int8_t health;
  int8_t fatigue;
  int8_t hygiene;
};

static const struct { const char* triggerId; struct TriggerStatEffect effect; } TRIGGER_STAT_EFFECTS[] = {
  { "head_caress", { 0, 1, 0, 0, 0 } },
};

#define TRIGGER_STAT_EFFECTS_SIZE 1

#endif // MODEL_GOTCHI_CONSTANTS_H
