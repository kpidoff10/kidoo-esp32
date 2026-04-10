#ifndef GOTCHI_BATTERY_H
#define GOTCHI_BATTERY_H

#include <cstdint>

namespace GotchiBattery {

void init();                  // Detecte AXP2101 sur I2C
void update(uint32_t dtMs);   // Relecture toutes les 30s
bool isAvailable();            // AXP2101 detecte ?
int8_t getPercent();           // 0-100, -1 si indisponible
bool isCharging();             // En cours de charge
bool isPluggedIn();            // USB branche (VBUS present)

} // namespace GotchiBattery

#endif
