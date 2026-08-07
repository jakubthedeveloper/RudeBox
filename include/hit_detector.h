#pragma once

#include <stdint.h>

namespace HitDetector {

bool detect(uint16_t peak, float sensitivity, float& velocity);

}  // namespace HitDetector
