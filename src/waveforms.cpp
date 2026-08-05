#include "waveforms.h"

namespace Waveforms {

float triangle(float phase) {
  if (phase < 0.25f) return phase * 4.0f;
  if (phase < 0.75f) return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

}  // namespace Waveforms
