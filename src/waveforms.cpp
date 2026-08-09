#include "waveforms.h"

namespace Waveforms {

float triangle(float phase) {
  if (phase < 0.25f) return phase * 4.0f;
  if (phase < 0.75f) return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

float shapedPulse(float phase, float shape, float minimumPulseWidth) {
  if (shape <= 0.5f) {
    const float morph = shape * 2.0f;
    const float square = phase < 0.5f ? 1.0f : -1.0f;
    return triangle(phase) * (1.0f - morph) + square * morph;
  }

  const float pulseAmount = (shape - 0.5f) * 2.0f;
  const float pulseWidth =
      0.5f + (minimumPulseWidth - 0.5f) * pulseAmount;
  return phase < pulseWidth ? 1.0f : -1.0f;
}

}  // namespace Waveforms
