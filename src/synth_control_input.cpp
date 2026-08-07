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
};
uint32_t lastScanMs = 0;
uint32_t lastLogMs = 0;

static_assert(AppConfig::Controls::POTENTIOMETER_COUNT == 3,
              "The control mapping expects exactly three potentiometers");
static_assert(AppConfig::Controls::SENSITIVITY_CHANNEL <
                  AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::OSC_PITCH_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT &&
                  AppConfig::Controls::PITCH_DROP_CHANNEL <
                      AppConfig::Controls::POTENTIOMETER_COUNT,
              "Each control channel must be scanned");
static_assert(AppConfig::Controls::SENSITIVITY_CHANNEL !=
                      AppConfig::Controls::OSC_PITCH_CHANNEL &&
                  AppConfig::Controls::SENSITIVITY_CHANNEL !=
                      AppConfig::Controls::PITCH_DROP_CHANNEL &&
                  AppConfig::Controls::OSC_PITCH_CHANNEL !=
                      AppConfig::Controls::PITCH_DROP_CHANNEL,
              "Each control needs a distinct ADS7830 channel");
static_assert(AppConfig::Controls::POT_FILTER_ALPHA > 0.0f &&
                  AppConfig::Controls::POT_FILTER_ALPHA <= 1.0f,
              "The potentiometer EMA alpha must be in (0, 1]");

void scanPotentiometers();
void logControlValues(uint32_t now);
void updatePotentiometer(uint8_t channel, SynthControls& nextControls);
void applyMappedValue(uint8_t channel, float filteredValue,
                      SynthControls& nextControls);
float normalizedKnob(float filteredValue, bool invert);
float mapOscPitch(float knob);
float mapPitchDrop(float knob);

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
      "pots raw=[%u,%u,%u] filtered=[%.2f,%.2f,%.2f] "
      "sensitivity=%.3f oscPitchHz=%.2f pitchDropOct=%.3f\n",
      potentiometers[0].raw, potentiometers[1].raw, potentiometers[2].raw,
      potentiometers[0].filtered, potentiometers[1].filtered,
      potentiometers[2].filtered, controls.sensitivity, controls.oscPitchHz,
      controls.pitchDropOctaves);
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
      nextControls.sensitivity = normalizedKnob(
          filteredValue, AppConfig::Controls::INVERT_SENSITIVITY);
      break;
    case AppConfig::Controls::OSC_PITCH_CHANNEL:
      nextControls.oscPitchHz = mapOscPitch(normalizedKnob(
          filteredValue, AppConfig::Controls::INVERT_OSC_PITCH));
      break;
    case AppConfig::Controls::PITCH_DROP_CHANNEL:
      nextControls.pitchDropOctaves = mapPitchDrop(normalizedKnob(
          filteredValue, AppConfig::Controls::INVERT_PITCH_DROP));
      break;
  }
}

float normalizedKnob(float filteredValue, bool invert) {
  float knob = MathUtils::clamp01(filteredValue / 255.0f);
  if (invert) knob = 1.0f - knob;
  return knob;
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

}  // namespace
}  // namespace SynthControlInput
