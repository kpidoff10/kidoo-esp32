# Gotchi Audio — Guide d'intégration

## Architecture

```
assets/sounds/              ← Fichiers WAV/MP3 source
tools/wav_to_header.py      ← Script de conversion
src/models/gotchi/audio/
  ├── gotchi_speaker_test.h ← API publique (playSound, playSoundAsync, etc.)
  ├── gotchi_speaker_test.cpp
  ├── es8311.c/h/reg.h      ← Driver Waveshare (NE PAS MODIFIER)
  └── sounds/
      ├── sound_sneeze.h    ← PCM embarqué (auto-généré)
      └── sound_eating.h
```

## Ajouter un nouveau son

### 1. Mettre le fichier dans `assets/sounds/`
N'importe quel format (WAV, MP3, OGG). Nommage : `sneeze.wav`, `hiccup.mp3`, etc.

### 2. Convertir en header C
```bash
python tools/wav_to_header.py assets/sounds/ -o src/models/gotchi/audio/sounds/
```
Génère `sound_sneeze.h` avec les constantes :
- `SNEEZE_PCM[]` — données PCM (16-bit LE, mono, 16kHz)
- `SNEEZE_PCM_LEN` — taille en bytes
- `SNEEZE_DURATION_MS` — durée en ms
- `SNEEZE_SAMPLE_RATE` — 16000

### 3. Jouer le son dans le code

**Depuis une animation/behavior (non-bloquant) :**
```cpp
#include "../../../audio/gotchi_speaker_test.h"
#include "../../../audio/sounds/sound_sneeze.h"

// Dans la fonction d'animation :
GotchiSpeakerTest::playSoundAsync(SNEEZE_PCM, SNEEZE_PCM_LEN);
```

**Depuis une commande serial (bloquant OK) :**
```cpp
#include "../audio/gotchi_speaker_test.h"
#include "../audio/sounds/sound_sneeze.h"

GotchiSpeakerTest::playSound(SNEEZE_PCM, SNEEZE_PCM_LEN);
```

### 4. Commande serial pour tester
Dans `model_serial_commands.cpp` :
```cpp
#include "../audio/sounds/sound_sneeze.h"

if (command == "speaker sneeze") {
  GotchiSpeakerTest::playSound(SNEEZE_PCM, SNEEZE_PCM_LEN);
  return true;
}
```

## API

| Fonction | Bloquant | Usage |
|---|---|---|
| `playSound(pcm, len)` | Oui | Serial commands, tests |
| `playSoundAsync(pcm, len)` | Non | Animations, behaviors |
| `playTone(freq, dur, vol)` | Oui | Test tones |
| `playMelody()` | Oui | Test Do-Ré-Mi |

## Volume

- Stocké dans `config.json` → `speaker_volume` (0-100, défaut 80)
- Lu automatiquement par `codecInit()` à chaque playback
- Commandes serial : `speaker vol` (lire), `speaker vol 50` (écrire + test)

## Contraintes techniques

- **Format PCM** : 16-bit signed LE, mono, 16kHz (le script convertit auto)
- **Taille** : ~32KB/seconde de son. Un son de 2s = ~64KB en flash
- **Flash dispo** : ~4MB libre, donc ~100 sons de 1s possible
- **PSRAM** : Le driver I2S ne supporte PAS la PSRAM → `--wrap=heap_caps_calloc` dans platformio.ini
- **Stack async** : La task utilise 16KB (I2S init + codec + JSON config)
- **I2S** : Initialisé/détruit à chaque playback (pas persistant)
- **Pins** : MCLK=42, BCK=9, WS=45, DOUT=10, PA=46

## Nommage des fichiers

| Animation | Fichier source | Header généré | Constante |
|---|---|---|---|
| Éternuement | `sneeze.wav` | `sound_sneeze.h` | `SNEEZE_PCM` |
| Hoquet | `hiccup.wav` | `sound_hiccup.h` | `HICCUP_PCM` |
| Manger | `eating.wav` | `sound_eating.h` | `EATING_PCM` |
| Danse | `dance.wav` | `sound_dance.h` | `DANCE_PCM` |
| Bâillement | `yawn.wav` | `sound_yawn.h` | `YAWN_PCM` |
| Toc-toc | `knock.wav` | `sound_knock.h` | `KNOCK_PCM` |
| Content | `happy.wav` | `sound_happy.h` | `HAPPY_PCM` |
| Triste | `sad.wav` | `sound_sad.h` | `SAD_PCM` |
