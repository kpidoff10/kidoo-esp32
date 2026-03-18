#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

/**
 * Configuration MQTT à la racine du projet (kidoo-esp32/include/)
 * Centralisée pour tous les modèles (Dream, Gotchi, Sound)
 *
 * Modifier ce fichier pour changer le broker MQTT sans fouiller dans src/
 *
 * Surcharge possible via build_flags dans platformio.ini :
 *   -DMQTT_BROKER_HOST=\"mqtt.example.com\"   pour custom broker
 *   -DMQTT_BROKER_PORT=1883                    pour custom port
 */

#ifndef DEFAULT_MQTT_BROKER_HOST
#define DEFAULT_MQTT_BROKER_HOST "yd6fff17.ala.eu-central-1.emqxsl.com"
#endif

#ifndef DEFAULT_MQTT_BROKER_PORT
#define DEFAULT_MQTT_BROKER_PORT 8883
#endif

#endif // MQTT_CONFIG_H
