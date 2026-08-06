#include "hit_detector.h"

#include "app_config.h"

namespace HitDetector {
namespace {

bool armed = true;

}  // namespace

bool update(uint16_t peak) {
  if (peak < AppConfig::HitDetection::PAD_TRIGGER_THRESHOLD) {
    armed = true;
    return false;
  }
  if (!armed) return false;

  armed = false;
  return true;
}

}  // namespace HitDetector
