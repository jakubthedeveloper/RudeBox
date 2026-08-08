#include "user_interface.h"

#include <Arduino.h>
#include <math.h>

#include "app_config.h"
#include "math_utils.h"
#include "synth_control_input.h"

namespace UserInterface {
namespace {

constexpr uint32_t PWM_MAX_DUTY =
    (1U << AppConfig::Ui::SENSITIVITY_LED_PWM_RESOLUTION_BITS) - 1U;
constexpr float MIN_VISIBLE_LED_LEVEL = 1.0f / PWM_MAX_DUTY;

static_assert(AppConfig::Ui::SENSITIVITY_LED_PWM_RESOLUTION_BITS > 0 &&
                  AppConfig::Ui::SENSITIVITY_LED_PWM_RESOLUTION_BITS < 32,
              "Sensitivity LED PWM resolution must be between 1 and 31 bits");
static_assert(AppConfig::Ui::SENSITIVITY_LED_UPDATE_INTERVAL_MS > 0,
              "Sensitivity LED update interval must be positive");
static_assert(AppConfig::Ui::SENSITIVITY_LED_FADE_MS > 0,
              "Sensitivity LED fade duration must be positive");

float sensitivityLedLevel = 0.0f;
uint32_t sensitivityLedUpdatedMs = 0;

void writeSensitivityLed() {
  const uint32_t brightness = static_cast<uint32_t>(
      MathUtils::clamp01(sensitivityLedLevel) * PWM_MAX_DUTY);
  ledcWrite(AppConfig::Ui::SENSITIVITY_LED_PIN, brightness);
}

bool initializeSensitivityLed() {
  if (!ledcAttach(AppConfig::Ui::SENSITIVITY_LED_PIN,
                  AppConfig::Ui::SENSITIVITY_LED_PWM_FREQUENCY_HZ,
                  AppConfig::Ui::SENSITIVITY_LED_PWM_RESOLUTION_BITS)) {
    return false;
  }

  sensitivityLedLevel = 0.0f;
  sensitivityLedUpdatedMs = millis();
  writeSensitivityLed();
  return true;
}

void updateSensitivityLed(uint32_t now) {
  const uint32_t elapsedMs = now - sensitivityLedUpdatedMs;
  if (sensitivityLedLevel == 0.0f ||
      elapsedMs < AppConfig::Ui::SENSITIVITY_LED_UPDATE_INTERVAL_MS) {
    return;
  }
  sensitivityLedUpdatedMs = now;

  const float decay = powf(
      MIN_VISIBLE_LED_LEVEL,
      static_cast<float>(elapsedMs) / AppConfig::Ui::SENSITIVITY_LED_FADE_MS);
  sensitivityLedLevel *= decay;
  if (sensitivityLedLevel <= MIN_VISIBLE_LED_LEVEL) {
    sensitivityLedLevel = 0.0f;
  }
  writeSensitivityLed();
}

}  // namespace

bool begin() {
  if (!initializeSensitivityLed()) return false;
  return SynthControlInput::begin();
}

void indicatePadHit(float velocity) {
  sensitivityLedLevel = sqrtf(MathUtils::clamp01(velocity));
  sensitivityLedUpdatedMs = millis();
  writeSensitivityLed();
}

void update() {
  const uint32_t now = millis();
  updateSensitivityLed(now);
  SynthControlInput::update();
}

SynthControls synthControls() { return SynthControlInput::snapshot(); }

}  // namespace UserInterface
