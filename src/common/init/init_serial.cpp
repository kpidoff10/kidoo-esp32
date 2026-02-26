#include "../managers/init/init_manager.h"

bool InitManager::initSerial() {
  systemStatus.serial = INIT_IN_PROGRESS;
  
  // Sur ESP32-C3 avec USB CDC, Serial.begin() peut bloquer si USB n'est pas connecté
  // Solution : Configurer Serial avec timeout très court et ne jamais attendre
  
  // Configurer Serial avec timeout minimal pour éviter les blocages
  Serial.setTimeout(1);  // Timeout de 1ms pour les opérations de lecture
  
  // Démarrer Serial - ne pas attendre qu'il soit disponible
  // Sur ESP32-C3, Serial.begin() peut retourner immédiatement même sans USB
  Serial.begin(SERIAL_BAUD_RATE);
  
  // Sur ESP32-C3 USB CDC : l'énumération USB peut prendre 100-500 ms selon le PC/hôte.
  // Boucle de retry pour attendre que Serial soit disponible (évite "ERREUR" alors que Serial fonctionne).
  const unsigned long USB_ENUMERATION_TIMEOUT_MS = 500;
  const unsigned long RETRY_INTERVAL_MS = 50;
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < USB_ENUMERATION_TIMEOUT_MS)) {
    delay(RETRY_INTERVAL_MS);
  }
  
  // Vérifier si Serial est disponible
  // Si USB est connecté, Serial sera disponible après l'énumération
  // Si USB n'est pas connecté, Serial ne sera pas disponible mais le boot continue
  if (Serial) {
    // Serial disponible (USB connecté)
    // Petite pause pour stabiliser la connexion USB CDC
    delay(50);
    systemStatus.serial = INIT_SUCCESS;
    return true;
  } else {
    // Serial non disponible (USB non connecté) - ne pas bloquer le démarrage
    // Le système peut fonctionner sans Serial
    systemStatus.serial = INIT_FAILED;
    return false;  // Retourne false mais le système continue
  }
}
