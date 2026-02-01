#include "../managers/init/init_manager.h"
#include "../managers/sd/sd_manager.h"

bool InitManager::initSD() {
  systemStatus.sd = INIT_IN_PROGRESS;
  
  // Initialiser la carte SD (vérifier que le module est disponible)
  if (!SDManager::init()) {
    systemStatus.sd = INIT_FAILED;
    return false;
  }
  
  // Vérifier que la carte SD est toujours disponible après l'init
  if (!SDManager::isAvailable()) {
    systemStatus.sd = INIT_FAILED;
    return false;
  }
  
  // Récupérer la configuration depuis la carte SD (optionnel)
  SDConfig config = SDManager::getConfig();
  
  // Stocker la configuration globalement pour accès depuis n'importe où
  static SDConfig storedConfig;
  storedConfig = config;
  InitManager::setGlobalConfig(&storedConfig);
  
  systemStatus.sd = INIT_SUCCESS;
  return true;
}
