# Spécifications du Modèle Dream

## Vue d'ensemble

Le modèle **Dream** est une veilleuse intelligente pour enfant qui aide à apprendre un rituel de sommeil via l'application mobile. Elle utilise des LEDs pour créer une ambiance apaisante avec des transitions automatiques basées sur l'heure.

## Architecture

- **Hardware** : ESP32-S3
- **Gestion du temps** : RTC DS3231 (via I2C)
- **Stockage** : Configuration sauvegardée sur carte SD (fichier config JSON)
- **Communication** :
  - **Setup initial** : BLE (Bluetooth Low Energy)
  - **Commandes** : PubNub (via WiFi)

## Fonctionnalités principales

### 1. Heure de coucher

À l'heure de coucher définie dans l'application :
- **Fade-in progressif** : Les LEDs s'allument progressivement (brightness 0 → brightness actuelle)
- **Couleur** : Couleur définie dans l'application mobile
- **Gestion par jour** : Heure et couleur différentes selon les jours de la semaine

### 2. Durée d'allumage

Deux modes disponibles :

#### Option A : Toute la nuit
- Les LEDs restent allumées jusqu'à l'heure de réveil
- La couleur reste la même pendant toute la nuit

#### Option B : Timer avec extinction progressive
- Durée configurable (ex: 1 heure)
- Après la durée définie, extinction progressive (brightness actuelle → 0)
- La couleur reste la même pendant l'extinction
- Les LEDs sont complètement éteintes jusqu'au réveil

### 3. Heure de réveil

15 minutes avant l'heure de réveil définie :

#### Réveil progressif
- Les LEDs se réveillent progressivement (brightness 0 → brightness actuelle)

#### Transition de couleur
- Transition progressive de la couleur de coucher vers la couleur de réveil
- Les deux effets sont combinés (brightness + couleur)

### 4. Configuration par défaut

Si aucune configuration n'est définie :
- **Heure de coucher** : 20h00 (tous les jours)
- **Heure de réveil** : 7h30 (tous les jours)
- **Couleur coucher** : Orange doux (RGB 255, 100, 50)
- **Couleur réveil** : Jaune doux (RGB 255, 200, 100)
- **Durée allumage** : Toute la nuit (0 = mode toute la nuit)
- **Durée fade-in coucher** : 30 secondes
- **Durée extinction progressive** : 5 minutes

## Structure de configuration

### Format JSON (fichier config sur SD card)

```json
{
  "sleepRitual": {
    "enabled": true,
    "currentBrightness": 128,
    "days": {
      "monday": {
        "bedtime": {
          "hour": 20,
          "minute": 0,
          "color": {
            "r": 255,
            "g": 100,
            "b": 50
          }
        },
        "wakeup": {
          "hour": 7,
          "minute": 30,
          "color": {
            "r": 255,
            "g": 200,
            "b": 100
          }
        },
        "duration": 0
      },
      "tuesday": { ... },
      "wednesday": { ... },
      "thursday": { ... },
      "friday": { ... },
      "saturday": { ... },
      "sunday": { ... }
    }
  }
}
```

### Champs de configuration

#### Niveau racine `sleepRitual`
- `enabled` (bool) : Active/désactive le rituel de sommeil
- `currentBrightness` (uint8_t) : Brightness actuelle des LEDs (0-255)

#### Par jour de la semaine
- `bedtime` : Configuration de l'heure de coucher
  - `hour` (uint8_t) : Heure (0-23)
  - `minute` (uint8_t) : Minute (0-59)
  - `color` : Couleur RGB
    - `r` (uint8_t) : Rouge (0-255)
    - `g` (uint8_t) : Vert (0-255)
    - `b` (uint8_t) : Bleu (0-255)
- `wakeup` : Configuration de l'heure de réveil
  - `hour` (uint8_t) : Heure (0-23)
  - `minute` (uint8_t) : Minute (0-59)
  - `color` : Couleur RGB
    - `r` (uint8_t) : Rouge (0-255)
    - `g` (uint8_t) : Vert (0-255)
    - `b` (uint8_t) : Bleu (0-255)
- `duration` (uint16_t) : Durée d'allumage en minutes
  - `0` : Toute la nuit (jusqu'au réveil)
  - `> 0` : Timer en minutes (ex: 60 = 1 heure)

## Comportements détaillés

### Transition de coucher (Fade-in)

1. **Déclenchement** : À l'heure de coucher exacte
2. **Durée** : Transition progressive sur **30 secondes**
3. **Effet** :
   - Brightness : 0 → `currentBrightness` (interpolation linéaire sur 30 secondes)
   - Couleur : Couleur de coucher définie pour le jour
   - Effet LED : `LED_EFFECT_NONE` (couleur fixe)

### Extinction progressive (si timer activé)

1. **Déclenchement** : Après la durée définie depuis l'heure de coucher
2. **Durée** : Transition progressive sur **5 minutes** (300 secondes)
3. **Effet** :
   - Brightness : `currentBrightness` → 0 (interpolation linéaire sur 5 minutes)
   - Couleur : Reste la même (couleur de coucher)
   - Effet LED : `LED_EFFECT_NONE` (couleur fixe)
4. **État final** : LEDs complètement éteintes (brightness 0, couleur noire)

### Transition de réveil

1. **Déclenchement** : 15 minutes avant l'heure de réveil
2. **Durée** : 15 minutes (jusqu'à l'heure de réveil exacte)
3. **Effet combiné** :
   - **Brightness** : 0 → `currentBrightness` (progression linéaire sur 15 minutes)
   - **Couleur** : Transition progressive de la couleur de coucher vers la couleur de réveil
     - Interpolation linéaire RGB sur 15 minutes
   - Effet LED : `LED_EFFECT_NONE` (couleur fixe avec transition)

### Gestion du jour de la semaine

- Le système utilise `RTCManager::getDateTime()` pour obtenir le jour actuel
- `dayOfWeek` : 1 = Lundi, 7 = Dimanche
- La configuration du jour correspondant est utilisée

## Intégration avec les managers existants

### RTCManager
- `RTCManager::getDateTime()` : Obtenir l'heure actuelle et le jour de la semaine
- `RTCManager::isAvailable()` : Vérifier que le RTC est opérationnel

### LEDManager
- `LEDManager::setColor(r, g, b)` : Définir la couleur
- `LEDManager::setBrightness(brightness)` : Définir la brightness (via configuration)
- `LEDManager::setEffect(LED_EFFECT_NONE)` : Pas d'effet animé, couleur fixe
- `LEDManager::wakeUp()` : Réveiller les LEDs (sortir du sleep mode)

### SDManager
- `SDManager::getConfig()` : Lire la configuration depuis le fichier config.json
- `SDManager::saveConfig(config)` : Sauvegarder la configuration

### BLEConfigManager
- Utilisé uniquement pour le setup initial (activation BLE via bouton)
- Une fois configuré, les commandes passent par PubNub

### PubNubManager
- Réception des commandes de configuration depuis l'application mobile
- Commandes possibles :
  - Mise à jour de la configuration du rituel
  - Activation/désactivation du rituel
  - Modification de la brightness actuelle

## États du système

### État : Attente coucher
- LEDs éteintes ou dans l'état précédent
- Vérification périodique de l'heure de coucher

### État : Coucher (fade-in)
- Transition brightness 0 → currentBrightness
- Couleur : Couleur de coucher
- Durée : 30 secondes

### État : Nuit (allumée)
- LEDs allumées à la couleur de coucher
- Brightness : currentBrightness
- Si timer activé : Attente de l'extinction progressive
- Si toute la nuit : Attente du réveil

### État : Nuit (extinction progressive)
- Transition brightness currentBrightness → 0
- Couleur : Couleur de coucher (fixe)
- Durée : 5 minutes
- État final : LEDs éteintes

### État : Attente réveil
- LEDs éteintes
- Vérification périodique de l'heure (15 minutes avant réveil)

### État : Réveil (transition)
- Transition brightness 0 → currentBrightness
- Transition couleur : Couleur coucher → Couleur réveil
- Durée : 15 minutes
- État final : LEDs allumées à la couleur de réveil

### État : Jour
- LEDs dans l'état normal (peuvent être contrôlées manuellement)
- Attente de l'heure de coucher suivante

## Gestion des erreurs

### RTC non disponible
- Le rituel ne peut pas fonctionner sans RTC
- Afficher un message d'erreur
- Désactiver automatiquement le rituel

### Configuration invalide
- Utiliser les valeurs par défaut (20h00 coucher, 7h30 réveil)
- Logger l'erreur

### SD card non disponible
- Le rituel peut fonctionner avec la configuration en mémoire
- Les modifications ne peuvent pas être sauvegardées
- Logger un avertissement

## Commandes PubNub

### Mise à jour de la configuration
```json
{
  "type": "sleepRitual.update",
  "data": {
    "enabled": true,
    "currentBrightness": 128,
    "days": { ... }
  }
}
```

### Activation/désactivation
```json
{
  "type": "sleepRitual.toggle",
  "data": {
    "enabled": true
  }
}
```

### Modification brightness
```json
{
  "type": "sleepRitual.brightness",
  "data": {
    "brightness": 150
  }
}
```

## Notes d'implémentation

### SleepRitualManager

Créer un manager `SleepRitualManager` dans `models/common/managers/sleep_ritual/` :
- Gestion de l'état du rituel
- Vérification périodique de l'heure (dans `update()`)
- Transitions de couleur et brightness
- Chargement/sauvegarde de la configuration

### Fréquence de vérification

- `update()` appelé dans `loop()` (toutes les 10ms environ)
- Vérifier l'heure toutes les secondes (pas besoin de vérifier en continu)
- Utiliser `millis()` pour éviter les appels trop fréquents à `RTCManager::getDateTime()`

### Transitions fluides

- Utiliser des calculs basés sur le temps écoulé
- Interpolation linéaire pour les transitions de couleur RGB
- Interpolation linéaire pour les transitions de brightness

### Gestion du sleep mode LED

- Le rituel doit empêcher le sleep mode automatique des LEDs pendant les transitions
- Utiliser `LEDManager::wakeUp()` si nécessaire
- Après le réveil, les LEDs peuvent entrer en sleep mode normal si inactives

## Valeurs confirmées

1. **Durée du fade-in de coucher** : 30 secondes
2. **Durée de l'extinction progressive** : 5 minutes (300 secondes)
3. **Couleur par défaut coucher** : Orange doux (RGB 255, 100, 50)
4. **Couleur par défaut réveil** : Jaune doux (RGB 255, 200, 100)
5. **Fréquence de vérification** : Toutes les secondes (optimisation avec `millis()`)
