#ifndef MODEL_GOTCHI_CONSTANTS_H
#define MODEL_GOTCHI_CONSTANTS_H

/**
 * Constantes pour le modèle Kidoo Gotchi
 *
 * Ce fichier contient les constantes NFC pour les objets interactifs :
 * - Clés NFC simulées pour les tests Serial
 * - Mapping des clés vers les types d'objets
 */

// ============================================
// NFC Keys - Simulation keys for testing
// ============================================
// Ces clés sont utilisées pour simuler la lecture d'un badge NFC via Serial
// En production, ces clés seront remplacées par les vraies UIDs des badges NFC

// --- Food items ---
#define NFC_KEY_BOTTLE "BOTTLE"
#define NFC_KEY_SNACK "SNACK"        // variant 2 = gâteau
#define NFC_KEY_APPLE "APPLE"        // variant 3 = pomme (fruit)
#define NFC_KEY_CANDY "CANDY"

// --- Hygiene items (future) ---
#define NFC_KEY_TOOTHBRUSH "BRUSH"
#define NFC_KEY_SOAP "SOAP"

// --- Sleep items (future) ---
#define NFC_KEY_BED "BED"

// ============================================
// NFC Key Mapping Structure
// ============================================
// Structure pour mapper une clé NFC vers un type d'objet

struct NFCKeyMapping {
  const char* key;      // Clé NFC (affichage / fallback texte)
  const char* itemId;   // ID de l'objet (correspond aux NFC_ITEM_* dans config.h)
  const char* name;     // Nom lisible de l'objet
  uint8_t variant;      // Code écrit sur le tag (1-4) pour reconnaissance fiable sans corruption
};

// Table de mapping des clés NFC vers les objets
// variant = octet écrit en bloc 4 (1=Biberon, 2=Gâteau, 3=Pomme, 4=Bonbon) pour éviter corruption lecture texte
static const NFCKeyMapping NFC_KEY_TABLE[] = {
  {NFC_KEY_BOTTLE, "bottle", "Biberon", 1},
  {NFC_KEY_SNACK, "cake", "Gateau", 2},
  {NFC_KEY_APPLE, "apple", "Pomme", 3},
  {NFC_KEY_CANDY, "candy", "Bonbon", 4},

  // Future items (commented out until implemented)
  // {NFC_KEY_TOOTHBRUSH, "toothbrush", "Toothbrush"},
  // {NFC_KEY_SOAP, "soap", "Soap"},
  // {NFC_KEY_BED, "bed", "Bed"},
};

// Nombre d'entrées dans la table
#define NFC_KEY_TABLE_SIZE (sizeof(NFC_KEY_TABLE) / sizeof(NFCKeyMapping))

// ============================================
// Progressive Food Effects (effets progressifs)
// ============================================
// Au lieu de donner tout d'un coup, la nourriture donne ses effets progressivement

// --- Bottle (Biberon) ---
#define BOTTLE_TICK_HUNGER 5
#define BOTTLE_TICK_HAPPINESS 1
#define BOTTLE_TICK_INTERVAL_MS 5000
#define BOTTLE_TOTAL_TICKS 0           // 0 = illimité (faim 100% ou tag retiré)

// --- Cake (Gâteau) : +5 nourriture, 4 ticks, -1 maladie ---
#define CAKE_TICK_HUNGER 1              // +1 par tick × 5 ticks = +5 total
#define CAKE_TICK_HAPPINESS 1
#define CAKE_TICK_HEALTH 1             // -1 maladie = +1 health par tick
#define CAKE_TICK_INTERVAL_MS 8000
#define CAKE_TOTAL_TICKS 5

// --- Candy (Bonbon) : +3 nourriture, 3 ticks, -2 maladie, +4 bonne humeur ---
#define CANDY_TICK_HUNGER 1            // +1 par tick × 3 = +3 total
#define CANDY_TICK_HAPPINESS 2         // +2 par tick × 3 = +6 (ex. +4 bonne humeur)
#define CANDY_TICK_HEALTH 2            // -2 maladie = +2 health par tick × 3 = +6 total
#define CANDY_TICK_INTERVAL_MS 6000
#define CANDY_TOTAL_TICKS 3

// --- Apple (Pomme / fruit) : variant 3 ---
#define APPLE_TICK_HUNGER 2
#define APPLE_TICK_HAPPINESS 0
#define APPLE_TICK_HEALTH 1
#define APPLE_TICK_INTERVAL_MS 5000
#define APPLE_TOTAL_TICKS 4

// ============================================
// Progressive Effect Structure
// ============================================
// Structure pour définir un effet progressif

struct ProgressiveFoodEffect {
  const char* itemId;           // ID de l'item (bottle, cake, apple, candy)
  uint8_t tickHunger;           // Hunger donné par tick
  uint8_t tickHappiness;        // Happiness donné par tick
  uint8_t tickHealth;           // Health donné par tick
  unsigned long tickInterval;   // Intervalle entre chaque tick (ms)
  uint8_t totalTicks;           // Nombre total de ticks
};

// 4 entrées = 4 variants (aligné getVariantLabel : 1 Biberon, 2 Gâteau, 3 Pomme, 4 Bonbon)
static const ProgressiveFoodEffect PROGRESSIVE_FOOD_EFFECTS[] = {
  { "bottle", BOTTLE_TICK_HUNGER, BOTTLE_TICK_HAPPINESS, 0, BOTTLE_TICK_INTERVAL_MS, BOTTLE_TOTAL_TICKS },
  { "cake",   CAKE_TICK_HUNGER,    CAKE_TICK_HAPPINESS,   CAKE_TICK_HEALTH, CAKE_TICK_INTERVAL_MS, CAKE_TOTAL_TICKS },
  { "candy",  CANDY_TICK_HUNGER,   CANDY_TICK_HAPPINESS,  CANDY_TICK_HEALTH, CANDY_TICK_INTERVAL_MS, CANDY_TOTAL_TICKS },
  { "apple",  APPLE_TICK_HUNGER,   APPLE_TICK_HAPPINESS, APPLE_TICK_HEALTH, APPLE_TICK_INTERVAL_MS, APPLE_TOTAL_TICKS },
};

#define PROGRESSIVE_FOOD_EFFECTS_SIZE (sizeof(PROGRESSIVE_FOOD_EFFECTS) / sizeof(ProgressiveFoodEffect))

#endif // MODEL_GOTCHI_CONSTANTS_H
