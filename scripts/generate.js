#!/usr/bin/env node
/**
 * Génère platformio.ini et les dispatchers à partir de models.yaml
 * Usage: node scripts/generate.js
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
	-DARDUINO_USB_MODE=1
	-DARDUINO_USB_CDC_ON_BOOT=1
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
