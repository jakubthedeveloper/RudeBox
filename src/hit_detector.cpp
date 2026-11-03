#include "hit_detector.h"

#include <math.h>

#include "app_config.h"
#include "audio_io.h"
#include "math_utils.h"

namespace HitDetector {
namespace {

enum class State : uint8_t {
  Idle,
  Validating,
  LockedOut,
};

struct ValidationWindow {
  uint16_t sampleCount;
  uint16_t activeSamples;
  uint16_t maximum;
  uint16_t tailMaximum;
  uint16_t tailActiveSamples;
  uint32_t energy;
  int32_t signedSum;
};

constexpr uint32_t RETRIGGER_HOLDOFF_SAMPLES = static_cast<uint32_t>(
    AudioIo::SAMPLE_RATE * AppConfig::HitDetection::RETRIGGER_HOLDOFF_MS /
    1000.0f);

State state = State::Idle;
ValidationWindow validation = {};
uint32_t lockedOutSamples = 0;

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
    AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE <
        AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES,
    "Trigger tail must start inside the validation window");
static_assert(
    AppConfig::HitDetection::TRIGGER_MIN_TAIL_ACTIVE_SAMPLES <=
        AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES -
            AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE,
    "Required tail samples must fit in the trigger tail");
static_assert(AppConfig::HitDetection::TRIGGER_MIN_TAIL_PEAK_RATIO >= 0.0f &&
                  AppConfig::HitDetection::TRIGGER_MIN_TAIL_PEAK_RATIO <= 1.0f,
              "Trigger tail peak ratio must be normalized");
static_assert(
    AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD <=
        AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD,
    "Follow threshold must not exceed the candidate threshold");
static_assert(AppConfig::HitDetection::RETRIGGER_HOLDOFF_MS >= 0.0f,
              "Retrigger hold-off must not be negative");
static_assert(
    AppConfig::HitDetection::TRIGGER_REARM_THRESHOLD <=
        AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD,
    "Rearm threshold must not exceed the candidate threshold");

uint16_t sampleMagnitude(int16_t sample);
void beginValidation(int16_t sample);
void collectValidationSample(int16_t sample);
bool validationWindowIsComplete();
bool validationWindowIsAccepted();
bool validationWindowMatchesPolarity(TriggerPolarity polarity);
void lockOut();
void countLockedOutSample();
bool canRearm(uint16_t blockPeak);
void copyValidationDiagnostics(Result& result);

}  // namespace

Result process(const int16_t* samples, size_t sampleCount, float sensitivity,
               TriggerPolarity polarity) {
  Result result = {};
  const bool startedLockedOut = state == State::LockedOut;

  for (size_t index = 0; index < sampleCount; ++index) {
    const int16_t sample = samples[index];
    const uint16_t magnitude = sampleMagnitude(sample);
    if (magnitude > result.rawPeak) result.rawPeak = magnitude;

    if (state == State::LockedOut) {
      countLockedOutSample();
      continue;
    }

    if (state == State::Idle) {
      if (magnitude < AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD) continue;

      beginValidation(sample);
      result.candidateStarted = true;
    } else {
      collectValidationSample(sample);
    }

    if (!validationWindowIsComplete()) continue;

    result.validationCompleted = true;
    copyValidationDiagnostics(result);
    if (!validationWindowIsAccepted()) {
      state = State::Idle;
      continue;
    }

    lockOut();
    if (!validationWindowMatchesPolarity(polarity)) {
      result.polarityRejected = true;
      continue;
    }
    result.hitDetected = true;
  }

  if (result.hitDetected) {
    const uint16_t capturedPeak =
        result.rawPeak > validation.maximum ? result.rawPeak
                                            : validation.maximum;
    result.velocity = mapVelocity(capturedPeak, sensitivity);
  }

  if (state == State::Validating && result.candidateStarted &&
      !result.validationCompleted) {
    copyValidationDiagnostics(result);
  }

  if (startedLockedOut && canRearm(result.rawPeak)) state = State::Idle;

  return result;
}

namespace {

uint16_t sampleMagnitude(int16_t sample) {
  const int32_t value = sample;
  return static_cast<uint16_t>(value < 0 ? -value : value);
}

void beginValidation(int16_t sample) {
  validation = {};
  state = State::Validating;
  collectValidationSample(sample);
}

void collectValidationSample(int16_t sample) {
  const uint16_t magnitude = sampleMagnitude(sample);
  const bool isTailSample =
      validation.sampleCount >=
      AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE;
  ++validation.sampleCount;
  if (magnitude >= AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD) {
    ++validation.activeSamples;
    if (isTailSample) ++validation.tailActiveSamples;
  }
  if (magnitude > validation.maximum) validation.maximum = magnitude;
  if (isTailSample && magnitude > validation.tailMaximum) {
    validation.tailMaximum = magnitude;
  }
  validation.energy += magnitude;
  validation.signedSum += sample;
}

bool validationWindowIsComplete() {
  return validation.sampleCount >=
         AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES;
}

bool validationWindowIsAccepted() {
  const bool hasEnoughActiveSamples =
      validation.activeSamples >=
      AppConfig::HitDetection::TRIGGER_MIN_ACTIVE_SAMPLES;
  const bool hasActiveTail =
      validation.tailActiveSamples >=
      AppConfig::HitDetection::TRIGGER_MIN_TAIL_ACTIVE_SAMPLES;
  const bool retainsEnoughTailAmplitude =
      validation.tailMaximum >=
      validation.maximum *
          AppConfig::HitDetection::TRIGGER_MIN_TAIL_PEAK_RATIO;
  return hasEnoughActiveSamples && hasActiveTail &&
         retainsEnoughTailAmplitude;
}

// The window sum follows the dominant half-wave. The first sample is not used
// because the codec's decimation filter rings with alternating signs before a
// sharp edge.
bool validationWindowMatchesPolarity(TriggerPolarity polarity) {
  switch (polarity) {
    case TriggerPolarity::Positive:
      return validation.signedSum > 0;
    case TriggerPolarity::Negative:
      return validation.signedSum < 0;
    case TriggerPolarity::Any:
      break;
  }
  return true;
}

void lockOut() {
  state = State::LockedOut;
  lockedOutSamples = 0;
}

void countLockedOutSample() {
  if (lockedOutSamples < RETRIGGER_HOLDOFF_SAMPLES) ++lockedOutSamples;
}

// The hold-off skips the zero crossing between an AC-coupled edge and its
// opposite-sign rebound, which would otherwise re-arm the detector early.
bool canRearm(uint16_t blockPeak) {
  return lockedOutSamples >= RETRIGGER_HOLDOFF_SAMPLES &&
         blockPeak < AppConfig::HitDetection::TRIGGER_REARM_THRESHOLD;
}

void copyValidationDiagnostics(Result& result) {
  result.validationMax = validation.maximum;
  result.activeSamples = validation.activeSamples;
  result.tailMaximum = validation.tailMaximum;
  result.tailActiveSamples = validation.tailActiveSamples;
  result.windowEnergy = validation.energy;
  result.candidateSign = validation.signedSum < 0 ? -1 : 1;
}

}  // namespace

float mapVelocity(uint16_t peak, float sensitivity) {
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

}  // namespace HitDetector
