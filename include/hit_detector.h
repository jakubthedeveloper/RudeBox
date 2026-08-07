#pragma once

#include <stddef.h>
#include <stdint.h>

namespace HitDetector {

struct Result {
  bool candidateStarted;
  bool validationCompleted;
  bool hitDetected;
  uint16_t rawPeak;
  uint16_t validationMax;
  uint8_t activeSamples;
  uint32_t windowEnergy;
  float velocity;
};

Result process(const uint16_t* magnitudes, size_t sampleCount,
               float sensitivity);

}  // namespace HitDetector
