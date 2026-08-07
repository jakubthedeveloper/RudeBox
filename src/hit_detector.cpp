#include "hit_detector.h"

#include <math.h>

#include "app_config.h"
#include "math_utils.h"

namespace HitDetector {
namespace {

bool armed = true;

static_assert(
    AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY >
            AppConfig::HitDetection::PAD_INPUT_MIN &&
        AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY >
            AppConfig::HitDetection::PAD_INPUT_MIN,
    "Velocity effective maxima must exceed the input minimum");

bool detectNewHit(uint16_t peak);
float velocityFromPeak(uint16_t peak, float sensitivity);

}  // namespace

bool detect(uint16_t peak, float sensitivity, float& velocity) {
  if (!detectNewHit(peak)) return false;

  velocity = velocityFromPeak(peak, sensitivity);
  return true;
}

namespace {

bool detectNewHit(uint16_t peak) {
  if (peak < AppConfig::HitDetection::PAD_TRIGGER_THRESHOLD) {
    armed = true;
    return false;
  }
  if (!armed) return false;

  armed = false;
  return true;
}

float velocityFromPeak(uint16_t peak, float sensitivity) {
  const float normalizedSensitivity = MathUtils::clamp01(sensitivity);
  const float effectiveMax = MathUtils::lerp(
      AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY,
      AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY,
      normalizedSensitivity);
  const float linearVelocity = MathUtils::clamp01(
      (static_cast<float>(peak) - AppConfig::HitDetection::PAD_INPUT_MIN) /
      (effectiveMax - AppConfig::HitDetection::PAD_INPUT_MIN));
  const float curveExponent = MathUtils::lerp(
      AppConfig::HitDetection::VELOCITY_CURVE_AT_MIN_SENSITIVITY,
      AppConfig::HitDetection::VELOCITY_CURVE_AT_MAX_SENSITIVITY,
      normalizedSensitivity);
  return MathUtils::clamp01(powf(linearVelocity, curveExponent));
}

}  // namespace

}  // namespace HitDetector
