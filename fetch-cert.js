#!/usr/bin/env node
/**
 * Script de récupération de chaîne TLS pour l'ESP32.
 * - Demande le nom d'hôte (et optionnellement le port).
 * - Ouvre une connexion TLS, récupère le certificat + la chaîne.
 * - Écrit le tout dans le dossier ./certificats/<hote>.pem au format PEM.
 *
 * Usage :
 *   node fetch-cert.js
 *   # puis saisir kidoo-box.com (ou un autre domaine)
 */

const fs = require('fs');
const path = require('path');
const tls = require('tls');
const readline = require('readline');

function prompt(question) {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  });
  return new Promise((resolve) => {
    rl.question(question, (answer) => {
      rl.close();
      resolve(answer.trim());
    });
  });
}

function bufferToPem(buffer) {
  const base64 = buffer.toString('base64');
  const lines = base64.match(/.{1,64}/g) || [];
  return ['-----BEGIN CERTIFICATE-----', ...lines, '-----END CERTIFICATE-----', ''].join('\n');
}

async function main() {
  const hostInput = await prompt('Nom d’hôte (ex: kidoo-box.com) : ');
  if (!hostInput) {
    console.error('❌ Nom d’hôte requis. Abandon.');
    process.exit(1);
  }
  const portInput = await prompt('Port (défaut 443) : ');
  const port = Number(portInput) || 443;

  console.log(`🔐 Connexion à ${hostInput}:${port} ...`);

  const socket = tls.connect(
    {
      host: hostInput,
      port,
      servername: hostInput,
      rejectUnauthorized: false, // On veut récupérer les certifs sans valider la chaîne
    },
    () => {
      try {
        const peerCert = socket.getPeerCertificate(true);
        if (!peerCert || !peerCert.raw) {
          throw new Error('Impossible de récupérer le certificat du serveur.');
        }

        const pemParts = [];
        const seen = new Set();
        let current = peerCert;

        while (current && current.raw) {
          const rawBase64 = current.raw.toString('base64');
          if (seen.has(rawBase64)) {
            break;
          }
          seen.add(rawBase64);
          pemParts.push(bufferToPem(current.raw));

          if (!current.issuerCertificate || current === current.issuerCertificate) {
            break;
          }
          current = current.issuerCertificate;
        }

        if (pemParts.length === 0) {
          throw new Error('Aucun certificat extrait.');
        }

        const certDir = path.resolve(__dirname, 'certificats');
        fs.mkdirSync(certDir, { recursive: true });

        const pemContent = pemParts.join('\n');
        const pemPath = path.join(certDir, 'ota-cert.pem');
        fs.writeFileSync(pemPath, pemContent, 'utf8');

        // Pour l'ESP32: n'utiliser que le certificat racine (dernier) pour réduire la RAM
        // (chaîne complète = "SSL - Memory allocation failed" sur ESP32)
        const rootOnlyPem = pemParts[pemParts.length - 1];

        const headerPath = path.join(certDir, 'ota-cert.h');
        const header = `#pragma once

static const char OTA_CERT_PEM[] = R"EOF(
${rootOnlyPem}
)EOF";
`;
        fs.writeFileSync(headerPath, header, 'utf8');

        console.log(`✅ Chaîne TLS (${pemParts.length} cert.) → ${pemPath}`);
        console.log(`✅ Header ota-cert.h: certificat racine uniquement (économie RAM)`);
        console.log('   Inclusions disponibles : #include "../../../../../certificats/ota-cert.h"');
      } catch (error) {
        console.error('❌ Erreur lors de la récupération du certificat :', error.message);
        process.exitCode = 1;
      } finally {
        socket.end();
      }
    }
  );

  socket.on('error', (err) => {
    console.error('❌ Échec de la connexion TLS :', err.message);
    process.exit(1);
  });
}

main();
