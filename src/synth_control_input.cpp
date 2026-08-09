#include "synth_control_input.h"

#include <Arduino.h>
#include <math.h>

#include "ads7830.h"
#include "app_config.h"
#include "math_utils.h"

namespace SynthControlInput {
namespace {

struct PotentiometerState {
  uint8_t raw = 0;
  float filtered = 0.0f;
  float lastApplied = 0.0f;
  bool initialized = false;
};

PotentiometerState potentiometers[AppConfig::Controls::POTENTIOMETER_COUNT];
SynthControls controls = {
    AppConfig::Controls::DEFAULT_SENSITIVITY,
    AppConfig::Controls::DEFAULT_OSC_PITCH_HZ,
    AppConfig::Controls::DEFAULT_PITCH_DROP_OCTAVES,
    AppConfig::Controls::DEFAULT_CLICK_LEVEL,
    AppConfig::Controls::DEFAULT_AMP_VELOCITY,
    AppConfig::Controls::DEFAULT_SHAPE,
    AppConfig::Controls::DEFAULT_DECAY_MS,
    AppConfig::Controls::DEFAULT_ENV_TO_PITCH_SEMITONES,
};
uint32_t lastScanMs = 0;
uint32_t lastLogMs = 0;

constexpr uint8_t CONTROL_CHANNELS[] = {
    AppConfig::Controls::SENSITIVITY_CHANNEL,
    AppConfig::Controls::OSC_PITCH_CHANNEL,
    AppConfig::Controls::PITCH_DROP_CHANNEL,
    AppConfig::Controls::CLICK_CHANNEL,
    AppConfig::Controls::AMP_VELOCITY_CHANNEL,
    AppConfig::Controls::SHAPE_CHANNEL,
    AppConfig::Controls::DECAY_CHANNEL,
    AppConfig::Controls::ENV_TO_PITCH_CHANNEL,
};

constexpr bool controlChannelsAreDistinct() {
  for (size_t current = 0;
       current < sizeof(CONTROL_CHANNELS) / sizeof(CONTROL_CHANNELS[0]);
       ++current) {
    for (size_t compared = current + 1;
         compared < sizeof(CONTROL_CHANNELS) / sizeof(CONTROL_CHANNELS[0]);
         ++compared) {
      if (CONTROL_CHANNELS[current] == CONTROL_CHANNELS[compared]) return false;
    }
  }
  return true;
}

static_assert(AppConfig::Controls::POTENTIOMETER_COUNT > 0,
              "At least one potentiometer must be configured");
static_assert(AppConfig::Controls::SENSITIVITY_CHANNEL <
                  AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::OSC_PITCH_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::PITCH_DROP_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::CLICK_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::AMP_VELOCITY_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::SHAPE_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::DECAY_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::ENV_TO_PITCH_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT,
              "Each control channel must be scanned");
static_assert(controlChannelsAreDistinct(),
              "Each control needs a distinct ADS7830 channel");
static_assert(AppConfig::Controls::POT_FILTER_ALPHA > 0.0f &&
                  AppConfig::Controls::POT_FILTER_ALPHA <= 1.0f,
              "The potentiometer EMA alpha must be in (0, 1]");

void scanPotentiometers();
void logControlValues(uint32_t now);
void updatePotentiometer(uint8_t channel, SynthControls& nextControls);
void applyMappedValue(uint8_t channel, float filteredValue,
                      SynthControls& nextControls);
float normalizedKnob(float filteredValue);
float mapOscPitch(float knob);
float mapPitchDrop(float knob);
float mapDecay(float knob);

}  // namespace

bool begin() {
  if (!Ads7830::begin()) return false;

  const uint32_t now = millis();
  scanPotentiometers();
  lastScanMs = now;
  lastLogMs = now;
  return true;
}

void update() {
  const uint32_t now = millis();
  if (now - lastScanMs >= AppConfig::Controls::CONTROL_SCAN_INTERVAL_MS) {
    lastScanMs = now;
    scanPotentiometers();
  }
  logControlValues(now);
}

SynthControls snapshot() { return controls; }

namespace {

void scanPotentiometers() {
  SynthControls nextControls = controls;
  for (uint8_t channel = 0;
       channel < AppConfig::Controls::POTENTIOMETER_COUNT; ++channel) {
    updatePotentiometer(channel, nextControls);
  }
  controls = nextControls;
}

void logControlValues(uint32_t now) {
  if (!AppConfig::Controls::LOG_CONTROL_VALUES ||
      now - lastLogMs < AppConfig::Controls::CONTROL_LOG_INTERVAL_MS) {
    return;
  }

  lastLogMs = now;
  Serial.printf(
      "pots filtered=[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]\n"
      "sensitivity=%.3f oscPitchHz=%.2f pitchDropOct=%.3f\n"
      "CLICK=%.3f AMP_VEL=%.3f shape=%.3f decay_ms=%.1f "
      "env_to_pitch=%.1f\n",
      potentiometers[0].filtered,
      potentiometers[1].filtered,
      potentiometers[2].filtered,
      potentiometers[3].filtered,
      potentiometers[4].filtered,
      potentiometers[5].filtered,
      potentiometers[6].filtered,
      potentiometers[7].filtered,
      controls.sensitivity, controls.oscPitchHz, controls.pitchDropOctaves,
      controls.clickLevel, controls.ampVelocity, controls.shapeNormalized,
      controls.decayMs, controls.envToPitchSemitones);
}

void updatePotentiometer(uint8_t channel, SynthControls& nextControls) {
  uint8_t raw;
  if (!Ads7830::read(channel, raw)) return;

  PotentiometerState& state = potentiometers[channel];
  state.raw = raw;
  if (!state.initialized) {
    state.filtered = raw;
    state.lastApplied = raw;
    state.initialized = true;
    applyMappedValue(channel, state.filtered, nextControls);
    return;
  }

  state.filtered += AppConfig::Controls::POT_FILTER_ALPHA *
                    (static_cast<float>(raw) - state.filtered);
  if (fabsf(state.filtered - state.lastApplied) <
      AppConfig::Controls::POT_CHANGE_THRESHOLD) {
    return;
  }

  state.lastApplied = state.filtered;
  applyMappedValue(channel, state.filtered, nextControls);
}

void applyMappedValue(uint8_t channel, float filteredValue,
                      SynthControls& nextControls) {
  switch (channel) {
    case AppConfig::Controls::SENSITIVITY_CHANNEL:
      nextControls.sensitivity = normalizedKnob(filteredValue);
      break;
    case AppConfig::Controls::OSC_PITCH_CHANNEL:
      nextControls.oscPitchHz = mapOscPitch(normalizedKnob(filteredValue));
      break;
    case AppConfig::Controls::PITCH_DROP_CHANNEL:
      nextControls.pitchDropOctaves =
          mapPitchDrop(normalizedKnob(filteredValue));
      break;
    case AppConfig::Controls::CLICK_CHANNEL:
      nextControls.clickLevel = normalizedKnob(filteredValue);
      break;
    case AppConfig::Controls::AMP_VELOCITY_CHANNEL:
      nextControls.ampVelocity = normalizedKnob(filteredValue);
      break;
    case AppConfig::Controls::SHAPE_CHANNEL:
      nextControls.shapeNormalized = normalizedKnob(filteredValue);
      break;
    case AppConfig::Controls::DECAY_CHANNEL:
      nextControls.decayMs = mapDecay(normalizedKnob(filteredValue));
      break;
    case AppConfig::Controls::ENV_TO_PITCH_CHANNEL:
      nextControls.envToPitchSemitones =
          normalizedKnob(filteredValue) *
          AppConfig::Controls::ENV_TO_PITCH_MAX_SEMITONES;
      break;
  }
}

float normalizedKnob(float filteredValue) {
  return MathUtils::clamp01(filteredValue / 255.0f);
}

float mapOscPitch(float knob) {
  return AppConfig::Controls::OSC_PITCH_MIN_HZ *
         powf(AppConfig::Controls::OSC_PITCH_MAX_HZ /
                  AppConfig::Controls::OSC_PITCH_MIN_HZ,
              knob);
}

float mapPitchDrop(float knob) {
  const float shaped = knob * knob;
  return MathUtils::lerp(AppConfig::Controls::PITCH_DROP_MIN_OCTAVES,
                         AppConfig::Controls::PITCH_DROP_MAX_OCTAVES, shaped);
}

float mapDecay(float knob) {
  return AppConfig::Controls::DECAY_MIN_MS *
         powf(AppConfig::Controls::DECAY_MAX_MS /
                  AppConfig::Controls::DECAY_MIN_MS,
              knob);
}

}  // namespace
}  // namespace SynthControlInput
