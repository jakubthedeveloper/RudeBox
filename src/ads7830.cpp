#include "ads7830.h"

#include <Arduino.h>
#include <Wire.h>

#include "app_config.h"

namespace Ads7830 {
namespace {

constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

TwoWire wire(0);

uint8_t commandForChannel(uint8_t channel) {
  // Single-ended input, external reference and ADC powered during conversion.
  // Channel selection codes are not linear; see Table 2 in the datasheet.
  const uint8_t mux = static_cast<uint8_t>(
      (((channel << 2) | (channel >> 1)) & 0x07) << 4);
  return static_cast<uint8_t>(0x84 | mux);
}

}  // namespace

bool begin() {
  if (!wire.begin(AppConfig::Ui::ADS7830_SDA_PIN,
                  AppConfig::Ui::ADS7830_SCL_PIN,
                  I2C_FREQUENCY_HZ)) {
    return false;
  }

  wire.beginTransmission(AppConfig::Ui::ADS7830_ADDRESS);
  return wire.endTransmission() == 0;
}

bool read(uint8_t channel, uint8_t& value) {
  if (channel >= CHANNEL_COUNT) return false;

  wire.beginTransmission(AppConfig::Ui::ADS7830_ADDRESS);
  wire.write(commandForChannel(channel));
  if (wire.endTransmission() != 0) return false;

  if (wire.requestFrom(AppConfig::Ui::ADS7830_ADDRESS,
                       static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = wire.read();
  return true;
}

bool readAverage(uint8_t channel, uint8_t sampleCount, float& value) {
  if (sampleCount == 0) return false;

  uint32_t sum = 0;
  for (uint8_t sample = 0; sample < sampleCount; ++sample) {
    uint8_t rawValue;
    if (!read(channel, rawValue)) return false;
    sum += rawValue;
  }

  value = static_cast<float>(sum) / sampleCount;
  return true;
}

}  // namespace Ads7830
