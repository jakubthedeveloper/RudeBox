#include "user_interface.h"

#include <Arduino.h>

#include "app_config.h"
#include "synth_control_input.h"

namespace UserInterface {
namespace {

uint32_t activityLedStartedMs = 0;
bool activityLedOn = false;

void setActivityLed(bool on) {
  const uint8_t outputLevel = AppConfig::Ui::ACTIVITY_LED_ACTIVE_LOW
                                  ? (on ? LOW : HIGH)
                                  : (on ? HIGH : LOW);
  digitalWrite(AppConfig::Ui::ACTIVITY_LED_PIN, outputLevel);
  activityLedOn = on;
}

void initializeActivityLed() {
  pinMode(AppConfig::Ui::ACTIVITY_LED_PIN, OUTPUT);
  setActivityLed(false);
}

void updateActivityLed(uint32_t now) {
  if (activityLedOn &&
      now - activityLedStartedMs >= AppConfig::Ui::ACTIVITY_LED_PULSE_MS) {
    setActivityLed(false);
  }
}

}  // namespace

bool begin() {
  initializeActivityLed();
  return SynthControlInput::begin();
}

void indicatePadHit() {
  activityLedStartedMs = millis();
  setActivityLed(true);
}

void update() {
  const uint32_t now = millis();
  updateActivityLed(now);
  SynthControlInput::update();
}

SynthControls synthControls() { return SynthControlInput::snapshot(); }

}  // namespace UserInterface
