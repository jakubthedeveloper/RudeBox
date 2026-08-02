#include "hit_detector.h"

#include "app_config.h"

namespace HitDetector {
namespace {

bool armed = true;

}  // namespace

bool update(uint16_t peak, float& velocity) {
  if (peak < AppConfig::MIN_HIT_PEAK) {
    armed = true;
    return false;
  }
  if (!armed) return false;

  armed = false;
  const uint16_t limitedPeak =
      peak > AppConfig::MAX_HIT_PEAK ? AppConfig::MAX_HIT_PEAK : peak;
  velocity = static_cast<float>(limitedPeak - AppConfig::MIN_HIT_PEAK) /
             (AppConfig::MAX_HIT_PEAK - AppConfig::MIN_HIT_PEAK);
  return true;
}

}  // namespace HitDetector
