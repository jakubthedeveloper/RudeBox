#include "user_interface.h"

#include <Arduino.h>

#include "ads7830.h"
#include "app_config.h"

namespace UserInterface {
namespace {

uint32_t lastPotentiometerReportMs = 0;
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

void reportPotentiometer(uint8_t channel) {
  float value;
  if (Ads7830::readAverage(channel,
                           AppConfig::Ui::POTENTIOMETER_SAMPLES_PER_READING,
                           value)) {
    Serial.printf("Potentiometer %u: %.2f\n", channel, value);
    return;
  }

  Serial.printf("[WARN] Failed to read potentiometer %u\n", channel);
}

void reportPotentiometers(uint32_t now) {
  if (!AppConfig::Ui::LOG_POTENTIOMETERS ||
      now - lastPotentiometerReportMs <
          AppConfig::Ui::POTENTIOMETER_REPORT_INTERVAL_MS) {
    return;
  }

  lastPotentiometerReportMs = now;
  const uint8_t potentiometerCount =
      AppConfig::Ui::POTENTIOMETER_COUNT < Ads7830::CHANNEL_COUNT
          ? AppConfig::Ui::POTENTIOMETER_COUNT
          : Ads7830::CHANNEL_COUNT;
  for (uint8_t channel = 0; channel < potentiometerCount; ++channel) {
    reportPotentiometer(channel);
  }
}

}  // namespace

bool begin() {
  initializeActivityLed();
  if (!Ads7830::begin()) return false;

  lastPotentiometerReportMs = millis();
  return true;
}

void indicatePadHit() {
  activityLedStartedMs = millis();
  setActivityLed(true);
}

void update() {
  const uint32_t now = millis();
  updateActivityLed(now);
  reportPotentiometers(now);
}

}  // namespace UserInterface
