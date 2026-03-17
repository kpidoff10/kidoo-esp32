#!/usr/bin/env node
/**
 * Assistant interactif pour créer un nouveau modèle Kidoo
 * Demande tous les paramètres et appelle generate.js
 * Usage: node scripts/create-model.js
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

function ask(question) {
  return new Promise((resolve) => {
    rl.question(question, (answer) => {
      resolve(answer.trim());
    });
  });
}

async function main() {
  console.log('\n🔧 Assistant de création de modèle Kidoo\n');
  console.log('═'.repeat(50));

  // Nom du modèle
  let modelId = '';
  while (!modelId) {
    modelId = await ask('\n📝 Nom du modèle (ex: sound, music, lite): ');
  }
  modelId = modelId.toLowerCase();

  // Type d'ESP
  console.log('\n📱 Type d\'ESP32:');
  console.log('  1. ESP32-S3 (Dual-core, 16MB, PSRAM) ← Recommandé');
  console.log('  2. ESP32-C3 (Single-core, limité en GPIO)');
  let espChoice = await ask('\nChoisir (1 ou 2, défaut: 1): ');
  espChoice = espChoice || '1';
  const isS3 = espChoice !== '2';
  const board = isS3 ? 'esp32-s3-devkitc-1' : 'esp32-c3-devkitm-1';

  // Fonctionnalités
  console.log('\n✨ Fonctionnalités disponibles:');
  console.log('  • wifi  - Communication WiFi');
  console.log('  • ble   - Bluetooth Low Energy');
  console.log('  • rtc   - Real-Time Clock (RTC DS3231)');
  console.log('  • sd    - Carte SD');
  console.log('  • nfc   - Module NFC PN532');
  const featuresInput = await ask(
    '\nQuelles features activer? (défaut: wifi,ble,rtc,sd)\nEntrer: '
  );

  let enabledFeatures = ['wifi', 'ble', 'rtc', 'sd'];
  if (featuresInput.trim()) {
    enabledFeatures = featuresInput.toLowerCase().split(',').map(f => f.trim());
  }

  rl.close();

  // Afficher le résumé
  console.log('\n' + '═'.repeat(50));
  console.log('\n✓ Configuration:');
  console.log(`  • Modèle: ${modelId}`);
  console.log(`  • Board: ${board}`);
  console.log(`  • Features: ${enabledFeatures.join(', ')}`);
  console.log('\n' + '═'.repeat(50));

  // Appeler le generate.js
  try {
    const cmd = `node scripts/generate.js --create-model --model "${modelId}" --esp ${isS3 ? 's3' : 'c3'} --features "${enabledFeatures.join(',')}"`;
    console.log(`\nExécution: ${cmd}\n`);
    execSync(cmd, { stdio: 'inherit', cwd: path.join(__dirname, '..') });
  } catch (error) {
    console.error('\n❌ Erreur lors de la création du modèle');
    process.exit(1);
  }
}

main().catch(err => {
  console.error('Erreur:', err);
  process.exit(1);
});
