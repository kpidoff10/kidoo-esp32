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
  
  // IMPORTANT : Ne JAMAIS attendre Serial sur ESP32-C3 avec USB CDC
  // Si USB n'est pas connecté, Serial ne sera jamais disponible et attendre bloque le boot
  // Le système doit démarrer même sans Serial
  
  // Vérifier immédiatement si Serial est disponible (sans attendre)
  // Si USB est connecté, Serial sera disponible immédiatement
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
