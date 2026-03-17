#!/usr/bin/env node
/**
 * Génère platformio.ini et les dispatchers à partir de models.yaml
 * Peut aussi créer la structure d'un nouveau modèle
 *
 * Usage:
 *   node scripts/generate.js                    # Génère les dispatchers
 *   node scripts/generate.js --create-model <id> <board> [display-name]
 *   Example: node scripts/generate.js --create-model sound esp32-s3-devkitc-1 Sound
 *
 * Requis: Node.js 18+ (pour fs et path built-in)
 *
 * Include paths (build_flags -I) :
 *   - $PROJECT_DIR/src  → models/, common/
 *   - $PROJECT_DIR      → color/, certificats/, include/
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const MODELS_YAML = path.join(ROOT, 'models.yaml');
const SRC_MODELS = path.join(ROOT, 'src', 'models');

function loadConfig() {
  const content = fs.readFileSync(MODELS_YAML, 'utf8');
  // Essayer avec require('yaml') - si pas dispo, on va créer un JSON
  let config;
  try {
    const yaml = require('yaml');
    config = yaml.parse(content);
  } catch (e) {
    if (e.code === 'MODULE_NOT_FOUND') {
      console.log('Dépendance yaml manquante. Création de package.json...');
      const pkgPath = path.join(ROOT, 'package.json');
      if (!fs.existsSync(pkgPath)) {
        fs.writeFileSync(pkgPath, JSON.stringify({
          name: 'kidoo-esp32',
          private: true,
          scripts: { generate: 'node scripts/generate.js' },
          devDependencies: { yaml: '^2.6.0' }
        }, null, 2));
      }
      console.log('Exécutez: npm install');
      console.log('Puis: node scripts/generate.js');
      process.exit(1);
    }
    throw e;
  }
  return config;
}

function generateModelConfig(config) {
  const models = config.models;
  const modelIds = Object.keys(models);

  const errorMacros = modelIds.map(id => `!defined(${models[id].macro})`).join(' && ');
  const branches = modelIds.map((id, i) => {
    const m = models[id];
    const cond = i === 0 ? `#ifdef ${m.macro}` : `#elif defined(${m.macro})`;
    return `${cond}
  #include "${id}/config/config.h"
  #include "${id}/config/default_config.h"
  #define KIDOO_MODEL_NAME "${m.display_name}"`;
  }).join('\n') + '\n#endif';

  return `#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

/**
 * Fichier central pour inclure la configuration du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if ${errorMacros}
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

#include "common/config/default_config.h"

${branches}

#endif // MODEL_CONFIG_H
`;
}

function generateModelInit(config) {
  const models = config.models;
  const modelIds = Object.keys(models);

  const errorMacros = modelIds.map(id => `!defined(${models[id].macro})`).join(' && ');
  const branches = modelIds.map((id, i) => {
    const m = models[id];
    const cond = i === 0 ? `#ifdef ${m.macro}` : `#elif defined(${m.macro})`;
    return `${cond}
  #include "${id}/init/init_model.h"
  #define InitModel InitModel${m.display_name}`;
  }).join('\n') + '\n#endif';

  return `#ifndef MODEL_INIT_H
#define MODEL_INIT_H

/**
 * Fichier central pour inclure l'initialisation du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if ${errorMacros}
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

${branches}

#endif // MODEL_INIT_H
`;
}

function generateModelPubnubRoutes(config) {
  const models = config.models;
  const modelIds = Object.keys(models);

  const branches = modelIds.map((id, i) => {
    const m = models[id];
    const cond = i === 0 ? `#ifdef ${m.macro}` : `#elif defined(${m.macro})`;
    return `${cond}
  #include "${id}/pubnub/model_pubnub_routes.h"
  typedef Model${m.display_name}PubNubRoutes ModelPubNubRoutes;`;
  }).join('\n') + `
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_*"
#endif`;

  return `#ifndef MODEL_PUBNUB_ROUTES_H
#define MODEL_PUBNUB_ROUTES_H

/**
 * Inclusion des routes PubNub spécifiques au modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

${branches}

#endif // MODEL_PUBNUB_ROUTES_H
`;
}

function generateModelSerialCommands(config) {
  const models = config.models;
  const modelIds = Object.keys(models);

  const errorMacros = modelIds.map(id => `!defined(${models[id].macro})`).join(' && ');
  const branches = modelIds.map((id, i) => {
    const m = models[id];
    const cond = i === 0 ? `#ifdef ${m.macro}` : `#elif defined(${m.macro})`;
    return `${cond}
  #include "${id}/serial/model_serial_commands.h"
  #define ModelSerialCommands Model${m.display_name}SerialCommands`;
  }).join('\n') + '\n#endif';

  return `#ifndef MODEL_SERIAL_COMMANDS_H
#define MODEL_SERIAL_COMMANDS_H

/**
 * Fichier central pour inclure les commandes Serial du modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

#if ${errorMacros}
  #error "Aucun modele Kidoo defini. Definir KIDOO_MODEL_* dans platformio.ini"
#endif

${branches}

#endif // MODEL_SERIAL_COMMANDS_H
`;
}

function generateModelConfigSyncRoutes(config) {
  const models = config.models;
  const modelIds = Object.keys(models);
  const dispConfig = config.dispatcher_config?.config_sync || {};

  const branches = modelIds.map((id, i) => {
    const m = models[id];
    const cond = i === 0 ? `#ifdef ${m.macro}` : `#elif defined(${m.macro})`;
    const cfg = dispConfig[id] || { type: 'inline' };
    if (cfg.type === 'include') {
      return `${cond}
  #include "${cfg.path}"
  typedef ${cfg.typedef} ModelConfigSyncRoutes;`;
    } else {
      return `${cond}
  class Model${m.display_name}ConfigSyncRoutes {
  public:
    static void onWiFiConnected() {}
  };
  typedef Model${m.display_name}ConfigSyncRoutes ModelConfigSyncRoutes;`;
    }
  }).join('\n') + `
#else
  #error "Aucun modele Kidoo defini! Definissez KIDOO_MODEL_*"
#endif`;

  return `#ifndef MODEL_CONFIG_SYNC_ROUTES_H
#define MODEL_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Inclusion des routes de synchronisation de configuration spécifiques au modèle
 * Généré par: node scripts/generate.js
 * Source: models.yaml
 */

${branches}

#endif // MODEL_CONFIG_SYNC_ROUTES_H
`;
}


function createDirectory(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
    return true;
  }
  return false;
}

function createModelStructure(modelId, board, displayName) {
  console.log(`\nCréation de la structure pour le modèle '${modelId}'...`);

  const modelDir = path.join(SRC_MODELS, modelId);
  const dirs = ['config', 'init', 'pubnub', 'serial', 'config_sync', 'managers', 'utils'];

  // Créer les répertoires
  dirs.forEach(dir => {
    const dirPath = path.join(modelDir, dir);
    if (createDirectory(dirPath)) {
      console.log(`  ✓ ${dir}/`);
    }
  });

  const capitalizedId = modelId.charAt(0).toUpperCase() + modelId.slice(1);
  const displayNameFormatted = displayName || capitalizedId;

  // config/config.h - Détecter le type de board pour les pins par défaut
  const isS3 = board.includes('esp32-s3');
  const defaultRtcSda = isS3 ? 8 : 8;
  const defaultRtcScl = isS3 ? 9 : 9;
  const defaultSdMosi = isS3 ? 11 : 6;
  const defaultSdMiso = isS3 ? 13 : 5;
  const defaultSdSck = isS3 ? 12 : 4;
  const defaultSdCs = isS3 ? 10 : 7;

  fs.writeFileSync(path.join(modelDir, 'config', 'config.h'), `#ifndef CONFIG_${modelId.toUpperCase()}_H
#define CONFIG_${modelId.toUpperCase()}_H

#include <Arduino.h>

/**
 * Configuration matérielle du modèle ${displayNameFormatted}
 * Pins GPIO pour SD, RTC, etc.
 */

// ============================================
// Configuration RTC DS3231 (I2C)
// ============================================

#define RTC_SDA_PIN ${defaultRtcSda}
#define RTC_SCL_PIN ${defaultRtcScl}
#define RTC_I2C_ADDRESS 0x68

// ============================================
// Configuration de la carte SD (SPI)
// ============================================

#define SD_MOSI_PIN ${defaultSdMosi}
#define SD_MISO_PIN ${defaultSdMiso}
#define SD_SCK_PIN ${defaultSdSck}
#define SD_CS_PIN ${defaultSdCs}

// ============================================
// Composants disponibles sur ce modèle
// ============================================

#define HAS_SD_CARD true
#define HAS_RTC true
#define HAS_BLE true
#define HAS_PUBNUB true

#endif // CONFIG_${modelId.toUpperCase()}_H
`);
  console.log(`  ✓ config/config.h`);

  // config/default_config.h
  const capitalizedDisplayName = displayNameFormatted.charAt(0).toUpperCase() + displayNameFormatted.slice(1);
  fs.writeFileSync(path.join(modelDir, 'config', 'default_config.h'), `#ifndef DEFAULT_CONFIG_${modelId.toUpperCase()}_H
#define DEFAULT_CONFIG_${modelId.toUpperCase()}_H

/**
 * Configuration par défaut du modèle Kidoo ${capitalizedDisplayName}
 *
 * Ces valeurs sont utilisées lorsque :
 * - Le fichier config.json n'existe pas sur la carte SD
 * - Le fichier config.json est invalide
 * - Première utilisation du Kidoo ${capitalizedDisplayName}
 */

// ============================================
// Configuration par défaut - ${capitalizedDisplayName}
// ============================================

// Nom du dispositif par défaut (utilisé pour le Bluetooth)
#define DEFAULT_DEVICE_NAME "Kidoo-${capitalizedDisplayName}"

// Configuration WiFi par défaut (vide = non configuré)
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Luminosité LED par défaut (0-255)
#define DEFAULT_LED_BRIGHTNESS 128

// ============================================
// Configuration PubNub - ${capitalizedDisplayName}
// ============================================

// Version du firmware Kidoo (spécifique au modèle)
#define FIRMWARE_VERSION "1.0.0"

// Clés PubNub (créer un compte gratuit sur https://www.pubnub.com/)
// Subscribe Key (obligatoire pour recevoir des messages)
#define DEFAULT_PUBNUB_SUBSCRIBE_KEY "sub-c-5f6c027d-31ec-4d2d-96f4-f7a63fa5e747"

// Publish Key (obligatoire pour envoyer des messages)
#define DEFAULT_PUBNUB_PUBLISH_KEY "pub-c-54932998-4a9f-44da-acd1-dface15b1cb7"

#endif // DEFAULT_CONFIG_${modelId.toUpperCase()}_H
`);
  console.log(`  ✓ config/default_config.h`);

  // init/init_model.h
  fs.writeFileSync(path.join(modelDir, 'init', 'init_model.h'), `#ifndef INIT_MODEL_${displayNameFormatted.toUpperCase()}_H
#define INIT_MODEL_${displayNameFormatted.toUpperCase()}_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo ${displayNameFormatted}
 */

class InitModel${displayNameFormatted} {
public:
  static bool init();
  static bool configure();
  static void update();
};

#endif // INIT_MODEL_${displayNameFormatted.toUpperCase()}_H
`);
  console.log(`  ✓ init/init_model.h`);

  // init/init_model.cpp
  fs.writeFileSync(path.join(modelDir, 'init', 'init_model.cpp'), `#include "init_model.h"

bool InitModel${displayNameFormatted}::init() {
  // TODO: Initialisation du modèle ${displayNameFormatted}
  return true;
}

bool InitModel${displayNameFormatted}::configure() {
  // TODO: Configuration du modèle ${displayNameFormatted}
  return true;
}

void InitModel${displayNameFormatted}::update() {
  // TODO: Boucle update du modèle ${displayNameFormatted}
}
`);
  console.log(`  ✓ init/init_model.cpp`);

  // pubnub/model_pubnub_routes.h
  fs.writeFileSync(path.join(modelDir, 'pubnub', 'model_pubnub_routes.h'), `#ifndef MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H
#define MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H

#include <Arduino.h>

/**
 * Routes PubNub pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}PubNubRoutes {
public:
  // TODO: Ajouter les handlers PubNub spécifiques
};

#endif // MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H
`);
  console.log(`  ✓ pubnub/model_pubnub_routes.h`);

  // pubnub/model_pubnub_routes.cpp
  fs.writeFileSync(path.join(modelDir, 'pubnub', 'model_pubnub_routes.cpp'), `#include "model_pubnub_routes.h"

// Implémentation des routes PubNub pour ${displayNameFormatted}
`);
  console.log(`  ✓ pubnub/model_pubnub_routes.cpp`);

  // serial/model_serial_commands.h
  fs.writeFileSync(path.join(modelDir, 'serial', 'model_serial_commands.h'), `#ifndef MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H
#define MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}SerialCommands {
public:
  // TODO: Ajouter les commandes Serial spécifiques
};

#endif // MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H
`);
  console.log(`  ✓ serial/model_serial_commands.h`);

  // serial/model_serial_commands.cpp
  fs.writeFileSync(path.join(modelDir, 'serial', 'model_serial_commands.cpp'), `#include "model_serial_commands.h"

// Implémentation des commandes Serial pour ${displayNameFormatted}
`);
  console.log(`  ✓ serial/model_serial_commands.cpp`);

  // config_sync/model_config_sync_routes.h
  fs.writeFileSync(path.join(modelDir, 'config_sync', 'model_config_sync_routes.h'), `#ifndef MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H
#define MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Routes de synchronisation de configuration pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}ConfigSyncRoutes {
public:
  static void onWiFiConnected();
};

#endif // MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H
`);
  console.log(`  ✓ config_sync/model_config_sync_routes.h`);

  // config_sync/model_config_sync_routes.cpp
  fs.writeFileSync(path.join(modelDir, 'config_sync', 'model_config_sync_routes.cpp'), `#include "model_config_sync_routes.h"

void Model${displayNameFormatted}ConfigSyncRoutes::onWiFiConnected() {
  // TODO: Synchronisation de la config au WiFi connect pour ${displayNameFormatted}
}
`);
  console.log(`  ✓ config_sync/model_config_sync_routes.cpp`);

  // SPECIFICATIONS.md
  fs.writeFileSync(path.join(modelDir, 'SPECIFICATIONS.md'), `# Spécifications du modèle ${displayNameFormatted}

## Vue d'ensemble
Boîte à musique ${displayNameFormatted}

## Hardware
- **Board**: ${board}
- **CPU**: ESP32
- **RAM**: TBD
- **Flash**: TBD
- **Features**:
  - WiFi
  - SD Card

## Architecture

### Managers
- TODO: Ajouter les managers spécifiques

### Config
- Fichiers de configuration: \`config/\`
- Synchronisation: \`config_sync/\`

### Communication
- PubNub: \`pubnub/\`
- Serial: \`serial/\`
`);
  console.log(`  ✓ SPECIFICATIONS.md`);

  return true;
}

function addModelToYaml(modelId, board, displayName) {
  console.log(`\nAjout du modèle à models.yaml...`);

  const content = fs.readFileSync(MODELS_YAML, 'utf8');
  const lines = content.split('\n');

  // Trouver la fin de la section models (avant dispatcher_config)
  let insertIndex = -1;
  for (let i = lines.length - 1; i >= 0; i--) {
    if (lines[i].trim().startsWith('# Config pour les dispatchers')) {
      insertIndex = i - 1;
      break;
    }
  }

  if (insertIndex === -1) {
    console.error('Impossible de trouver où insérer le modèle dans models.yaml');
    return false;
  }

  const capitalizedId = modelId.charAt(0).toUpperCase() + modelId.slice(1);
  const displayNameFormatted = displayName || capitalizedId;

  const modelEntry = `
  ${modelId}:
    macro: KIDOO_MODEL_${modelId.toUpperCase()}
    id: ${modelId}
    display_name: ${displayNameFormatted}
    board: ${board}
    board_config:
      partitions: default_16MB.csv
      flash_mode: qio
      upload_flash_size: 16MB
      memory_type: qio_opi
      upload_speed: 460800
    build_flags:
      - KIDOO_MODEL_${modelId.toUpperCase()}
      - 'KIDOO_MODEL_ID=\\"${modelId}\\"'
      - HAS_WIFI
      - HAS_SD
      - BOARD_HAS_PSRAM
    lib_deps: []
    lib_ignore: []
`;

  lines.splice(insertIndex + 1, 0, modelEntry, '');

  // Ajouter aussi dans dispatcher_config.config_sync
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes(`gotchi: { type: inline }`)) {
      lines[i] += `\n    ${modelId}: { type: include, path: "${modelId}/config_sync/model_config_sync_routes.h", typedef: "Model${displayNameFormatted}ConfigSyncRoutes" }`;
      break;
    }
  }

  fs.writeFileSync(MODELS_YAML, lines.join('\n'));
  console.log(`  ✓ Modèle ajouté à models.yaml`);
  return true;
}

function generatePlatformioIni(config) {
  const models = config.models;
  const modelIds = Object.keys(models);

  const commonFilterBase = `	+<main.cpp>
	+<common/**>
	+<models/model_config.h>
	+<models/model_init.h>
	+<models/model_pubnub_routes.h>
	+<models/model_serial_commands.h>
	+<models/model_config_sync_routes.h>`;

  let envs = '';
  for (const [id, m] of Object.entries(models)) {
    const macro = m.macro;
    const displayName = m.display_name;
    const board = m.board;
    const bc = m.board_config || {};
    const buildFlags = (m.build_flags || []).map(f => 
      f.startsWith('-') ? `	${f}` : `	-D${f}`
    ).join('\n');
    const libDeps = (m.lib_deps || []).map(l => `	${l}`).join('\n');
    const libIgnore = (m.lib_ignore || []).map(l => `	${l}`).join('\n');

    let boardLines = [];
    if (bc.partitions) boardLines.push(`board_build.partitions = ${bc.partitions}`);
    if (bc.flash_mode) boardLines.push(`board_build.flash_mode = ${bc.flash_mode}`);
    if (bc.upload_flash_size) boardLines.push(`board_upload.flash_size = ${bc.upload_flash_size}`);
    if (bc.memory_type) boardLines.push(`board_build.arduino.memory_type = ${bc.memory_type}`);
    if (bc.upload_speed) boardLines.push(`upload_speed = ${bc.upload_speed}`);
    if (bc.before_reset) boardLines.push(`board_upload.before_reset = ${bc.before_reset}`);

    const buildFilter = commonFilterBase + `\n	+<models/${id}/**>`;

    envs += `
; ============================================
; Environnement ${displayName}
; ============================================

[env:${id}]
board = ${board}
${boardLines.join('\n')}

build_src_filter = 
${buildFilter}

build_flags = 
	\${env.build_flags}
${buildFlags}

`;
    if (libDeps.length > 0) {
      envs += `lib_deps = 
	\${env.lib_deps}
${libDeps}

`;
    }
    if (libIgnore.length > 0) {
      envs += `lib_ignore = 
${libIgnore}

`;
    }
  }

  const header = `; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html
;
; ============================================
; Architecture multi-plateforme ESP32
; ============================================
; Ce projet supporte plusieurs plateformes ESP32 avec un seul code source :
; - ESP32-S3 (Gotchi) : Dual-core + PSRAM
; - ESP32-C3 (Dream)  : Single-core RISC-V
;
; La détection du chip est automatique via core_config.h
; Les pins GPIO sont définis dans models/{model}/config/config.h
;
; ============================================
; FICHIER GÉNÉRÉ - Ne pas éditer manuellement
; ============================================
; Généré par: node scripts/generate.js
; Source: models.yaml
; Pour modifier les envs, éditer models.yaml puis relancer la génération.
;
; ============================================
; SD / FatFs (ff.h) - Compatibilité ESP32-S3 et ESP32-C3
; ============================================

[platformio]
default_envs = dream

; ============================================
; Configuration commune (valeurs par défaut)
; ============================================

[env]
platform = espressif32 @ 6.4.0
framework = arduino
platform_packages = tool-esptoolpy@~1.40501.0

build_flags =
	-I $PROJECT_DIR/src
	-I $PROJECT_DIR
	-DCORE_DEBUG_LEVEL=0
	-Os
	-ffunction-sections
	-fdata-sections
	-Wl,--gc-sections
	-DFASTLED_ESP32_FLASH_LOCK=1

lib_deps = 
	adafruit/Adafruit NeoPixel@^1.12.4
	bblanchon/ArduinoJson@^7.0.0

monitor_speed = 115200
monitor_dtr = 0
monitor_rts = 0
upload_speed = 921600
`;

  return header + envs;
}

// Point d'entrée
const readline = require('readline');
const args = process.argv.slice(2);

function prompt(question) {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });
  return new Promise((resolve) => {
    rl.question(question, (answer) => {
      rl.close();
      resolve(answer.trim());
    });
  });
}

function interactiveCreateModel() {
  console.log('\n🔧 Assistant de création de modèle Kidoo\n');

  // Récupérer les arguments passés
  const argv = process.argv.slice(2);
  let modelId = null;
  let isS3 = true;
  let enabledFeatures = ['rtc', 'wifi', 'sd', 'ble']; // par défaut

  // Chercher les arguments
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--model' && argv[i + 1]) {
      modelId = argv[i + 1];
    }
    if (argv[i] === '--esp' && argv[i + 1]) {
      isS3 = argv[i + 1] !== 'c3';
    }
    if (argv[i] === '--features' && argv[i + 1]) {
      enabledFeatures = argv[i + 1].toLowerCase().split(',').map(f => f.trim());
    }
  }

  // S'il manque des infos, on demande à l'utilisateur
  if (!modelId) {
    const modelIdArg = process.argv.slice(2).find((_, i, arr) => arr[i - 1] === '--model');
    if (!modelIdArg) {
      console.error('Usage: node scripts/generate.js --create-model [--model <name>] [--esp s3|c3] [--features rtc,ble,wifi,sd]');
      console.error('Example: node scripts/generate.js --create-model --model sound --esp s3 --features rtc,ble,wifi,sd');
      process.exit(1);
    }
  }

  const displayName = modelId.charAt(0).toUpperCase() + modelId.slice(1);

  // Créer la structure
  console.log(`\n🚀 Création du modèle '${modelId}' sur ${board}...\n`);
  if (!createModelStructure(modelId, board, displayName, enabledFeatures)) {
    console.error('Erreur lors de la création de la structure du modèle');
    process.exit(1);
  }

  // Ajouter à models.yaml
  if (!addModelToYaml(modelId, board, displayName)) {
    console.error('Erreur lors de l\'ajout à models.yaml');
    process.exit(1);
  }

  // Générer les dispatchers
  console.log('\n⚙️ Génération des dispatchers...');
  const config = loadConfig();

  fs.writeFileSync(path.join(ROOT, 'platformio.ini'), generatePlatformioIni(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config.h'), generateModelConfig(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_init.h'), generateModelInit(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_pubnub_routes.h'), generateModelPubnubRoutes(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_serial_commands.h'), generateModelSerialCommands(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config_sync_routes.h'), generateModelConfigSyncRoutes(config));

  console.log('\n✅ Modèle créé avec succès!\n');
  console.log('📦 Fichiers créés:');
  console.log(`  • src/models/${modelId}/config/`);
  console.log(`  • src/models/${modelId}/init/`);
  console.log(`  • src/models/${modelId}/pubnub/`);
  console.log(`  • src/models/${modelId}/serial/`);
  console.log(`  • src/models/${modelId}/config_sync/`);
  console.log(`  • platformio.ini [env:${modelId}]`);
  console.log('\n📝 Prochaines étapes:');
  console.log(`  1. Implémenter src/models/${modelId}/init/init_model.cpp`);
  console.log(`  2. Ajouter les managers spécifiques en src/models/${modelId}/managers/`);
  console.log(`  3. Configurer les pins dans src/models/${modelId}/config/config.h si besoin`);
}

// Patch createModelStructure pour supporter features
const originalCreateModelStructure = createModelStructure;
function createModelStructure(modelId, board, displayName, features = ['rtc', 'wifi', 'sd', 'ble']) {
  console.log(`Création de la structure pour le modèle '${modelId}'...`);

  const modelDir = path.join(SRC_MODELS, modelId);
  const dirs = ['config', 'init', 'pubnub', 'serial', 'config_sync', 'managers', 'utils'];

  dirs.forEach(dir => {
    const dirPath = path.join(modelDir, dir);
    if (createDirectory(dirPath)) {
      console.log(`  ✓ ${dir}/`);
    }
  });

  const capitalizedId = modelId.charAt(0).toUpperCase() + modelId.slice(1);
  const displayNameFormatted = displayName || capitalizedId;
  const isS3 = board.includes('esp32-s3');

  // Déterminer les features habilitées
  const hasRTC = features.includes('rtc');
  const hasSD = features.includes('sd');
  const hasNFC = features.includes('nfc');
  const hasBLE = features.includes('ble');
  const hasWiFi = features.includes('wifi');

  // config/config.h
  const defaultRtcSda = isS3 ? 8 : 8;
  const defaultRtcScl = isS3 ? 9 : 9;
  const defaultSdMosi = isS3 ? 11 : 6;
  const defaultSdMiso = isS3 ? 13 : 5;
  const defaultSdSck = isS3 ? 12 : 4;
  const defaultSdCs = isS3 ? 10 : 7;

  let configH = `#ifndef CONFIG_${modelId.toUpperCase()}_H
#define CONFIG_${modelId.toUpperCase()}_H

#include <Arduino.h>

/**
 * Configuration matérielle du modèle ${displayNameFormatted}
 * Pins GPIO pour SD, RTC, etc.
 */
`;

  if (hasRTC) {
    configH += `
// ============================================
// Configuration RTC DS3231 (I2C)
// ============================================

#define RTC_SDA_PIN ${defaultRtcSda}
#define RTC_SCL_PIN ${defaultRtcScl}
#define RTC_I2C_ADDRESS 0x68
`;
  }

  if (hasSD) {
    configH += `
// ============================================
// Configuration de la carte SD (SPI)
// ============================================

#define SD_MOSI_PIN ${defaultSdMosi}
#define SD_MISO_PIN ${defaultSdMiso}
#define SD_SCK_PIN ${defaultSdSck}
#define SD_CS_PIN ${defaultSdCs}
`;
  }

  if (hasNFC) {
    configH += `
// ============================================
// Configuration NFC PN532 (I2C)
// ============================================

#define NFC_SDA_PIN ${defaultRtcSda}
#define NFC_SCL_PIN ${defaultRtcScl}
#define NFC_I2C_ADDRESS 0x24
`;
  }

  configH += `
// ============================================
// Composants disponibles sur ce modèle
// ============================================
`;

  if (hasSD) configH += '\n#define HAS_SD_CARD true';
  if (hasRTC) configH += '\n#define HAS_RTC true';
  if (hasNFC) configH += '\n#define HAS_NFC true';
  if (hasBLE) configH += '\n#define HAS_BLE true';
  if (hasWiFi) configH += '\n#define HAS_WIFI true';
  configH += '\n#define HAS_PUBNUB true';

  configH += `

#endif // CONFIG_${modelId.toUpperCase()}_H
`;

  fs.writeFileSync(path.join(modelDir, 'config', 'config.h'), configH);
  console.log(`  ✓ config/config.h`);

  // Reste des fichiers (inchangé)
  fs.writeFileSync(path.join(modelDir, 'config', 'default_config.h'), `#ifndef DEFAULT_CONFIG_${modelId.toUpperCase()}_H
#define DEFAULT_CONFIG_${modelId.toUpperCase()}_H

/**
 * Configuration par défaut pour ${displayNameFormatted}
 * Valeurs initiales pour le stockage SD
 */

#endif // DEFAULT_CONFIG_${modelId.toUpperCase()}_H
`);
  console.log(`  ✓ config/default_config.h`);

  // init/init_model.h
  fs.writeFileSync(path.join(modelDir, 'init', 'init_model.h'), `#ifndef INIT_MODEL_${displayNameFormatted.toUpperCase()}_H
#define INIT_MODEL_${displayNameFormatted.toUpperCase()}_H

#include <Arduino.h>

/**
 * Initialisation spécifique au modèle Kidoo ${displayNameFormatted}
 */

class InitModel${displayNameFormatted} {
public:
  static bool init();
  static bool configure();
  static void update();
};

#endif // INIT_MODEL_${displayNameFormatted.toUpperCase()}_H
`);
  console.log(`  ✓ init/init_model.h`);

  // init/init_model.cpp
  fs.writeFileSync(path.join(modelDir, 'init', 'init_model.cpp'), `#include "init_model.h"

bool InitModel${displayNameFormatted}::init() {
  // TODO: Initialisation du modèle ${displayNameFormatted}
  return true;
}

bool InitModel${displayNameFormatted}::configure() {
  // TODO: Configuration du modèle ${displayNameFormatted}
  return true;
}

void InitModel${displayNameFormatted}::update() {
  // TODO: Boucle update du modèle ${displayNameFormatted}
}
`);
  console.log(`  ✓ init/init_model.cpp`);

  // pubnub/model_pubnub_routes.h
  fs.writeFileSync(path.join(modelDir, 'pubnub', 'model_pubnub_routes.h'), `#ifndef MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H
#define MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H

#include <Arduino.h>

/**
 * Routes PubNub pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}PubNubRoutes {
public:
  // TODO: Ajouter les handlers PubNub spécifiques
};

#endif // MODEL_${modelId.toUpperCase()}_PUBNUB_ROUTES_H
`);
  console.log(`  ✓ pubnub/model_pubnub_routes.h`);

  // pubnub/model_pubnub_routes.cpp
  fs.writeFileSync(path.join(modelDir, 'pubnub', 'model_pubnub_routes.cpp'), `#include "model_pubnub_routes.h"

// Implémentation des routes PubNub pour ${displayNameFormatted}
`);
  console.log(`  ✓ pubnub/model_pubnub_routes.cpp`);

  // serial/model_serial_commands.h
  fs.writeFileSync(path.join(modelDir, 'serial', 'model_serial_commands.h'), `#ifndef MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H
#define MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}SerialCommands {
public:
  // TODO: Ajouter les commandes Serial spécifiques
};

#endif // MODEL_${modelId.toUpperCase()}_SERIAL_COMMANDS_H
`);
  console.log(`  ✓ serial/model_serial_commands.h`);

  // serial/model_serial_commands.cpp
  fs.writeFileSync(path.join(modelDir, 'serial', 'model_serial_commands.cpp'), `#include "model_serial_commands.h"

// Implémentation des commandes Serial pour ${displayNameFormatted}
`);
  console.log(`  ✓ serial/model_serial_commands.cpp`);

  // config_sync/model_config_sync_routes.h
  fs.writeFileSync(path.join(modelDir, 'config_sync', 'model_config_sync_routes.h'), `#ifndef MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H
#define MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H

#include <Arduino.h>

/**
 * Routes de synchronisation de configuration pour ${displayNameFormatted}
 */

class Model${displayNameFormatted}ConfigSyncRoutes {
public:
  static void onWiFiConnected();
};

#endif // MODEL_${modelId.toUpperCase()}_CONFIG_SYNC_ROUTES_H
`);
  console.log(`  ✓ config_sync/model_config_sync_routes.h`);

  // config_sync/model_config_sync_routes.cpp
  fs.writeFileSync(path.join(modelDir, 'config_sync', 'model_config_sync_routes.cpp'), `#include "model_config_sync_routes.h"

void Model${displayNameFormatted}ConfigSyncRoutes::onWiFiConnected() {
  // TODO: Synchronisation de la config au WiFi connect pour ${displayNameFormatted}
}
`);
  console.log(`  ✓ config_sync/model_config_sync_routes.cpp`);

  // SPECIFICATIONS.md
  fs.writeFileSync(path.join(modelDir, 'SPECIFICATIONS.md'), `# Spécifications du modèle ${displayNameFormatted}

## Vue d'ensemble
Modèle ${displayNameFormatted}

## Hardware
- **Board**: ${board}
- **CPU**: ESP32
- **RAM**: TBD
- **Flash**: TBD
- **Features**:
${hasWiFi ? '  - WiFi\n' : ''}${hasSD ? '  - SD Card\n' : ''}${hasRTC ? '  - RTC\n' : ''}${hasBLE ? '  - Bluetooth\n' : ''}${hasNFC ? '  - NFC\n' : ''}
## Architecture

### Managers
- TODO: Ajouter les managers spécifiques

### Config
- Fichiers de configuration: \`config/\`
- Synchronisation: \`config_sync/\`

### Communication
- PubNub: \`pubnub/\`
- Serial: \`serial/\`
`);
  console.log(`  ✓ SPECIFICATIONS.md`);

  return true;
}

// Fonction pour parser les arguments
function parseCreateModelArgs() {
  const argv = process.argv.slice(2);
  const args = { model: null, esp: 's3', features: ['wifi', 'ble', 'rtc', 'sd'] };

  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--model' && argv[i + 1]) {
      args.model = argv[i + 1];
    }
    if (argv[i] === '--esp' && argv[i + 1]) {
      args.esp = argv[i + 1];
    }
    if (argv[i] === '--features' && argv[i + 1]) {
      args.features = argv[i + 1].split(',').map(f => f.trim());
    }
  }
  return args;
}

// Point d'entrée
const argv = process.argv.slice(2);

if (argv.includes('--create-model')) {
  // Mode création de modèle
  const args = parseCreateModelArgs();

  if (!args.model) {
    console.error('Usage: node scripts/generate.js --create-model --model <name> [--esp s3|c3] [--features rtc,ble,wifi,sd]');
    console.error('Example: node scripts/generate.js --create-model --model sound --esp s3 --features rtc,ble,wifi,sd');
    process.exit(1);
  }

  const modelId = args.model;
  const isS3 = args.esp !== 'c3';
  const board = isS3 ? 'esp32-s3-devkitc-1' : 'esp32-c3-devkitm-1';
  const displayName = modelId.charAt(0).toUpperCase() + modelId.slice(1);

  // Créer la structure
  console.log(`🚀 Création du modèle '${modelId}' sur ${board}...\n`);
  if (!createModelStructure(modelId, board, displayName, args.features)) {
    console.error('Erreur lors de la création de la structure du modèle');
    process.exit(1);
  }

  // Ajouter à models.yaml
  if (!addModelToYaml(modelId, board, displayName)) {
    console.error('Erreur lors de l\'ajout à models.yaml');
    process.exit(1);
  }

  // Générer les dispatchers
  console.log('\n⚙️  Génération des dispatchers...');
  const config = loadConfig();

  fs.writeFileSync(path.join(ROOT, 'platformio.ini'), generatePlatformioIni(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config.h'), generateModelConfig(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_init.h'), generateModelInit(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_pubnub_routes.h'), generateModelPubnubRoutes(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_serial_commands.h'), generateModelSerialCommands(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config_sync_routes.h'), generateModelConfigSyncRoutes(config));

  console.log('\n✅ Modèle créé avec succès!\n');
  console.log('📦 Fichiers créés:');
  console.log(`  • src/models/${modelId}/config/`);
  console.log(`  • src/models/${modelId}/init/`);
  console.log(`  • src/models/${modelId}/pubnub/`);
  console.log(`  • src/models/${modelId}/serial/`);
  console.log(`  • src/models/${modelId}/config_sync/`);
  console.log(`  • platformio.ini [env:${modelId}]`);
  console.log('\n📝 Prochaines étapes:');
  console.log(`  1. Implémenter src/models/${modelId}/init/init_model.cpp`);
  console.log(`  2. Ajouter les managers spécifiques en src/models/${modelId}/managers/`);
  console.log(`  3. Configurer les pins dans src/models/${modelId}/config/config.h si besoin`);
} else {
  // Mode normal: juste générer les dispatchers
  console.log('Chargement models.yaml...');
  const config = loadConfig();

  fs.writeFileSync(path.join(ROOT, 'platformio.ini'), generatePlatformioIni(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config.h'), generateModelConfig(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_init.h'), generateModelInit(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_pubnub_routes.h'), generateModelPubnubRoutes(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_serial_commands.h'), generateModelSerialCommands(config));
  fs.writeFileSync(path.join(SRC_MODELS, 'model_config_sync_routes.h'), generateModelConfigSyncRoutes(config));

  console.log('Génération terminée:');
  console.log('  - platformio.ini');
  console.log('  - src/models/model_config.h');
  console.log('  - src/models/model_init.h');
  console.log('  - src/models/model_pubnub_routes.h');
  console.log('  - src/models/model_serial_commands.h');
  console.log('  - src/models/model_config_sync_routes.h');
}
