#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * Configuration à la racine du projet (kidoo-esp32/include/)
 * 
 * Modifier ce fichier pour changer l'URL de l'API sans fouiller dans src/
 * 
 * Surcharge possible via build_flags dans platformio.ini :
 *   -DAPI_BASE_URL=\"http://192.168.1.217:3000\"   pour dev local
 *   -DAPI_BASE_URL=\"https://www.kidoo-box.com\"   pour prod
 */

#ifndef API_BASE_URL
#define API_BASE_URL "https://www.kidoo-box.com"
#endif

#endif // APP_CONFIG_H
