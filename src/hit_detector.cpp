#include "hit_detector.h"

#include <math.h>

#include "app_config.h"
#include "math_utils.h"

namespace HitDetector {
namespace {

enum class State : uint8_t {
  Idle,
  Validating,
  LockedOut,
};

struct ValidationWindow {
  uint8_t sampleCount;
  uint8_t activeSamples;
  uint16_t maximum;
  uint32_t energy;
};

State state = State::Idle;
ValidationWindow validation = {};

static_assert(
    AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY >
            AppConfig::HitDetection::PAD_INPUT_MIN &&
        AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY >
            AppConfig::HitDetection::PAD_INPUT_MIN,
    "Velocity effective maxima must exceed the input minimum");
static_assert(AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES > 0,
              "Trigger validation window must contain at least one sample");
static_assert(
    AppConfig::HitDetection::TRIGGER_MIN_ACTIVE_SAMPLES <=
        AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES,
    "Required active samples must fit in the validation window");
static_assert(
    AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD <=
        AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD,
    "Follow threshold must not exceed the candidate threshold");
static_assert(
    AppConfig::HitDetection::TRIGGER_REARM_THRESHOLD <=
        AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD,
    "Rearm threshold must not exceed the candidate threshold");

void beginValidation(uint16_t magnitude);
void collectValidationSample(uint16_t magnitude);
bool validationWindowIsComplete();
bool validationWindowIsAccepted();
void copyValidationDiagnostics(Result& result);
float velocityFromPeak(uint16_t peak, float sensitivity);

}  // namespace

Result process(const uint16_t* magnitudes, size_t sampleCount,
               float sensitivity) {
  Result result = {};
  const bool startedLockedOut = state == State::LockedOut;

  for (size_t index = 0; index < sampleCount; ++index) {
    const uint16_t magnitude = magnitudes[index];
    if (magnitude > result.rawPeak) result.rawPeak = magnitude;

    if (state == State::LockedOut) continue;

    if (state == State::Idle) {
      if (magnitude < AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD) continue;

      beginValidation(magnitude);
      result.candidateStarted = true;
    } else {
      collectValidationSample(magnitude);
    }

    if (!validationWindowIsComplete()) continue;

    result.validationCompleted = true;
    copyValidationDiagnostics(result);
    if (!validationWindowIsAccepted()) {
      state = State::Idle;
      continue;
    }

    state = State::LockedOut;
    result.hitDetected = true;
    result.velocity = velocityFromPeak(validation.maximum, sensitivity);
  }

  if (state == State::Validating && result.candidateStarted &&
      !result.validationCompleted) {
    copyValidationDiagnostics(result);
  }

  if (startedLockedOut &&
      result.rawPeak < AppConfig::HitDetection::TRIGGER_REARM_THRESHOLD) {
    state = State::Idle;
  }

  return result;
}

namespace {

void beginValidation(uint16_t magnitude) {
  validation = {};
  state = State::Validating;
  collectValidationSample(magnitude);
}

void collectValidationSample(uint16_t magnitude) {
  ++validation.sampleCount;
  if (magnitude >= AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD) {
    ++validation.activeSamples;
  }
  if (magnitude > validation.maximum) validation.maximum = magnitude;
  validation.energy += magnitude;
}

bool validationWindowIsComplete() {
  return validation.sampleCount >=
         AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES;
}

bool validationWindowIsAccepted() {
  return validation.activeSamples >=
         AppConfig::HitDetection::TRIGGER_MIN_ACTIVE_SAMPLES;
}

void copyValidationDiagnostics(Result& result) {
  result.validationMax = validation.maximum;
  result.activeSamples = validation.activeSamples;
  result.windowEnergy = validation.energy;
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
