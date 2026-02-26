# Générer le .bin et le publier pour l’OTA

## Prérequis : configuration centralisée

Le projet utilise **models.yaml** comme source de vérité. Pour modifier les modèles ou ajouter un nouveau : éditer `models.yaml` puis `npm run generate` (ou `node scripts/generate.js`). Cela régénère `platformio.ini` et les dispatchers `model_*.h`.

### Conventions des chemins d'include

Le projet utilise `-I $PROJECT_DIR/src` et `-I $PROJECT_DIR` dans `platformio.ini`. Tous les includes doivent être **relatifs à ces répertoires** :

| Chemin | Exemple |
|--------|---------|
| `models/` | `#include "models/model_config.h"` |
| `common/` | `#include "common/config/core_config.h"` |
| `color/` (racine) | `#include "color/colors.h"` |
| `certificats/` (racine) | `#include "certificats/ota-cert.h"` |

Éviter les chemins relatifs `../../` pour ces fichiers ; utiliser les chemins absolus ci-dessus.

---

## 0. Premier flash Dream (obligatoire pour que l'OTA fonctionne)

L'OTA Dream utilise la table de partitions **partitions_dream_ota.csv** (ota_0 + ota_1). Si le périphérique a été flashé avec une ancienne table (ex. huge_app, une seule partition app), les mises à jour OTA échouent avec « Update.begin() échoué » ou erreur « begin ».

**Important :** un simple `upload` **ne réécrit pas** la table de partitions. Il faut d'abord **effacer la flash**, puis uploader, pour que la nouvelle table (ota_0 + ota_1) soit écrite.

**Procédure (dans l'ordre) :**

1. **Libérer le port COM** (sinon « Accès refusé » / « port is busy ») :
   - Fermer le **Serial Monitor** PlatformIO (et tout autre terminal série).
   - Débrancher le câble USB du Kidoo, attendre 3 s, rebrancher.
   - Ne pas rouvrir le Serial Monitor avant d'avoir fini erase + upload.

2. **Effacer la flash**, puis **uploader** :

```bash
pio run -e dream -t erase
pio run -e dream -t upload
```

Après ce premier flash (erase + upload), les mises à jour OTA depuis l'app fonctionneront.

**Si le port reste bloqué :** redémarrer le PC, ou utiliser un autre câble/port USB, puis refaire erase puis upload.

---

## 1. Générer le .bin (script recommandé)

Dans le dossier **kidoo-esp32** :

```powershell
.\build-firmware.ps1
```

Le script :

1. **Demande le modèle** (dream / gotchi) si non fourni.
2. **Demande la version** (ex: `1.0.2`) ou utilise la version actuelle du modèle si tu appuies sur Entrée.
3. **Met à jour** `src/models/{model}/config/default_config.h` → `#define FIRMWARE_VERSION "1.0.2"` (chaque modèle a sa propre version).
4. **Lance le build** PlatformIO pour l'environnement choisi.
5. **Découpe** le firmware en parts (max 2 Mo) et crée un zip dans `firmware/{model}/firmware_{version}.zip`.

Avec paramètres :

```powershell
.\build-firmware.ps1 -Model dream -Version 1.0.2
.\build-firmware.ps1 -Model gotchi -Version 1.0.1   # build Gotchi au lieu de Dream
```

---

## Alternative : build manuel (PlatformIO)

```bash
# Dream (ESP32-C3, OTA)
pio run -e dream

# Gotchi (ESP32-S3)
pio run -e gotchi
```

Le binaire est dans :

- **Dream** : `.pio/build/dream/firmware.bin`
- **Gotchi** : `.pio/build/gotchi/firmware.bin`

---

## 2. Publier le firmware sur le serveur (pour l’OTA)

Le serveur stocke le .bin sur R2 et enregistre l’URL en base. Il faut :

1. **Obtenir une URL d’upload** (admin, avec ton token) :

   ```http
   POST /api/admin/firmware/upload-url
   Content-Type: application/json
   Authorization: Bearer <token>

   {
     "model": "dream",
     "version": "1.0.1",
     "fileName": "firmware.bin",
     "fileSize": 1234567
   }
   ```

   Réponse : `{ "data": { "uploadUrl", "path", "publicUrl" } }`.

2. **Envoyer le .bin vers R2** :

   ```http
   PUT <uploadUrl>
   Content-Type: application/octet-stream
   Body: contenu binaire du fichier .pio/build/dream/firmware.bin
   ```

   (avec curl : `curl -X PUT -T .pio/build/dream/firmware.bin "<uploadUrl>"`)

3. **Créer l’entrée firmware en base** (admin) :

   ```http
   POST /api/admin/firmware
   Content-Type: application/json
   Authorization: Bearer <token>

   {
     "model": "dream",
     "version": "1.0.1",
     "url": "<publicUrl de l’étape 1>",
     "path": "<path de l’étape 1>",
     "fileName": "firmware.bin",
     "fileSize": 1234567,
     "changelog": "- Démarrage en violet (test OTA)"
   }
   ```

Après ça, l’app pourra proposer la MAJ et l’ESP32 pourra télécharger le .bin via `GET /api/firmware/download?model=dream&version=1.0.1`.

---

## 3. Script rapide (optionnel)

Tu peux automatiser avec un script qui :

1. Lance `pio run -e dream`.
2. Lit la taille de `.pio/build/dream/firmware.bin`.
3. Appelle `upload-url` puis `PUT` puis `POST /api/admin/firmware` (avec un token admin en env).

Si tu veux, on peut détailler ce script (PowerShell ou Node) étape par étape.
