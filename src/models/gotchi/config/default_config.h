#ifndef MODEL_GOTCHI_DEFAULT_CONFIG_H
#define MODEL_GOTCHI_DEFAULT_CONFIG_H

/**
 * Configuration par défaut du modèle Kidoo Gotchi
 */

// ============================================
// Configuration par défaut - Gotchi
// ============================================

#define DEFAULT_DEVICE_NAME "Kidoo-Gotchi"

#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

#define DEFAULT_LED_BRIGHTNESS 128

// ============================================
// Configuration MQTT - Gotchi
// ============================================

#define FIRMWARE_VERSION "1.0.0"

// MQTT broker URL is fetched from server via /api/devices/{MAC}/mqtt-token
// No fallback - server is the source of truth

#endif // MODEL_GOTCHI_DEFAULT_CONFIG_H
