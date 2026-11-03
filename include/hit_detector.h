#pragma once

#include <stddef.h>
#include <stdint.h>

namespace HitDetector {

// Required sign of the sample sum over a strike's validation window.
enum class TriggerPolarity : uint8_t { Any, Positive, Negative };

struct Result {
  bool candidateStarted;
  bool polarityRejected;
  bool validationCompleted;
  bool hitDetected;
  int8_t candidateSign;
  uint16_t rawPeak;
  uint16_t validationMax;
  uint16_t activeSamples;
  uint16_t tailMaximum;
  uint16_t tailActiveSamples;
  uint32_t windowEnergy;
  float velocity;
};

// Locks out after every validated impulse. An impulse that does not match the
// polarity is reported as rejected instead of as a hit. Re-arming requires the
// retrigger hold-off to elapse and a quiet input block.
Result process(const int16_t* samples, size_t sampleCount, float sensitivity,
               TriggerPolarity polarity);

// Maps a captured pad peak to normalized velocity for the selected sensitivity.
float mapVelocity(uint16_t peak, float sensitivity);

}  // namespace HitDetector
