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
#define NFC_KEY_BOTTLE "BOTTLE_001"   // Clé pour le biberon
#define NFC_KEY_SNACK "SNACK_001"     // Clé pour le snack
#define NFC_KEY_WATER "WATER_001"     // Clé pour l'eau

// --- Hygiene items (future) ---
#define NFC_KEY_TOOTHBRUSH "BRUSH_001"  // Clé pour la brosse à dents
#define NFC_KEY_SOAP "SOAP_001"         // Clé pour le savon

// --- Sleep items (future) ---
#define NFC_KEY_BED "BED_001"           // Clé pour le lit

// ============================================
// NFC Key Mapping Structure
// ============================================
// Structure pour mapper une clé NFC vers un type d'objet

struct NFCKeyMapping {
  const char* key;      // Clé NFC (simulée ou réelle UID)
  const char* itemId;   // ID de l'objet (correspond aux NFC_ITEM_* dans config.h)
  const char* name;     // Nom lisible de l'objet
};

// Table de mapping des clés NFC vers les objets
// Cette table sera utilisée par le Serial command pour valider les clés
static const NFCKeyMapping NFC_KEY_TABLE[] = {
  // Food items
  {NFC_KEY_BOTTLE, "bottle", "Bottle"},
  {NFC_KEY_SNACK, "snack", "Snack"},
  {NFC_KEY_WATER, "water", "Water"},

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
#define BOTTLE_TICK_HUNGER 5           // +5 hunger par tick
#define BOTTLE_TICK_HAPPINESS 1        // +1 happiness par tick (total +5 sur 5 ticks)
#define BOTTLE_TICK_INTERVAL_MS 5000   // Intervalle entre chaque tick (5 secondes)
#define BOTTLE_TOTAL_TICKS 8           // Nombre total de ticks (8 ticks * 5 hunger = 40 total)

// --- Snack ---
#define SNACK_TICK_HUNGER 2            // +2 hunger par tick
#define SNACK_TICK_HAPPINESS 1         // +1 happiness par tick
#define SNACK_TICK_INTERVAL_MS 10000   // Intervalle entre chaque tick (10 secondes)
#define SNACK_TOTAL_TICKS 8            // Nombre total de ticks (8 ticks * 2 hunger = 16 total)

// --- Water (Eau) ---
#define WATER_TICK_HUNGER 2            // +2 hunger par tick
#define WATER_TICK_HEALTH 1            // +1 health par tick
#define WATER_TICK_INTERVAL_MS 5000    // Intervalle entre chaque tick (5 secondes)
#define WATER_TOTAL_TICKS 5            // Nombre total de ticks (5 ticks * 2 hunger = 10 total)

// ============================================
// Progressive Effect Structure
// ============================================
// Structure pour définir un effet progressif

struct ProgressiveFoodEffect {
  const char* itemId;           // ID de l'item (bottle, snack, water)
  uint8_t tickHunger;           // Hunger donné par tick
  uint8_t tickHappiness;        // Happiness donné par tick
  uint8_t tickHealth;           // Health donné par tick
  unsigned long tickInterval;   // Intervalle entre chaque tick (ms)
  uint8_t totalTicks;           // Nombre total de ticks
};

// Table des effets progressifs
static const ProgressiveFoodEffect PROGRESSIVE_FOOD_EFFECTS[] = {
  {
    "bottle",                    // itemId
    BOTTLE_TICK_HUNGER,          // +5 hunger par tick
    BOTTLE_TICK_HAPPINESS,       // +1 happiness par tick
    0,                           // pas d'effet sur health
    BOTTLE_TICK_INTERVAL_MS,     // 5 secondes
    BOTTLE_TOTAL_TICKS           // 8 ticks total
  },
  {
    "snack",                     // itemId
    SNACK_TICK_HUNGER,           // +2 hunger par tick
    SNACK_TICK_HAPPINESS,        // +1 happiness par tick
    0,                           // pas d'effet sur health
    SNACK_TICK_INTERVAL_MS,      // 10 secondes
    SNACK_TOTAL_TICKS            // 8 ticks total
  },
  {
    "water",                     // itemId
    WATER_TICK_HUNGER,           // +2 hunger par tick
    0,                           // pas d'effet sur happiness
    WATER_TICK_HEALTH,           // +1 health par tick
    WATER_TICK_INTERVAL_MS,      // 5 secondes
    WATER_TOTAL_TICKS            // 5 ticks total
  }
};

#define PROGRESSIVE_FOOD_EFFECTS_SIZE (sizeof(PROGRESSIVE_FOOD_EFFECTS) / sizeof(ProgressiveFoodEffect))

#endif // MODEL_GOTCHI_CONSTANTS_H
