#ifndef GOTCHI_IMU_H
#define GOTCHI_IMU_H

#include <cstdint>

namespace GotchiImu {

bool init();

/// Appeler chaque frame. Retourne true si une secousse est detectee.
/// shakeX/shakeY : direction normalisee (-1 a 1) de la secousse
bool update(uint32_t dtMs, float& shakeX, float& shakeY);

} // namespace GotchiImu

#endif
