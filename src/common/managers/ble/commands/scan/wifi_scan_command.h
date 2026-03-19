#ifndef BLE_WIFI_SCAN_COMMAND_H
#define BLE_WIFI_SCAN_COMMAND_H

#include <Arduino.h>

class BLEWiFiScanCommand {
public:
  static bool execute(const String& jsonData);
  static bool isValid(const String& jsonData);
};

#endif
