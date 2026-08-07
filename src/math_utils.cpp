#include "math_utils.h"

namespace MathUtils {

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float lerp(float start, float end, float amount) {
  return start + (end - start) * amount;
}

}  // namespace MathUtils
