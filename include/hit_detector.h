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
  uint16_t activeSamples;
  uint16_t tailMaximum;
  uint16_t tailActiveSamples;
  uint32_t windowEnergy;
  float velocity;
};

Result process(const uint16_t* magnitudes, size_t sampleCount,
               float sensitivity);

// Maps a captured pad peak to normalized velocity for the selected sensitivity.
float mapVelocity(uint16_t peak, float sensitivity);

}  // namespace HitDetector
