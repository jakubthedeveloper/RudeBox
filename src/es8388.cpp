#include "es8388.h"

#include <Arduino.h>
#include <Wire.h>

namespace Es8388 {
namespace {

constexpr uint8_t ADDRESS = 0x10;
constexpr uint8_t INPUT_GAIN_CODE = 2;  // +6 dB; valid range: 0..4.

TwoWire wire(1);

struct Setting {
  uint8_t reg;
  uint8_t value;
};

bool write(uint8_t reg, uint8_t value) {
  wire.beginTransmission(ADDRESS);
  wire.write(reg);
  wire.write(value);
  if (wire.endTransmission() != 0) return false;

  wire.beginTransmission(ADDRESS);
  wire.write(reg);
  if (wire.endTransmission(false) != 0 ||
      wire.requestFrom(ADDRESS, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  return wire.read() == value;
}

}  // namespace

bool begin() {
  wire.begin(33, 32, 400000);
  wire.beginTransmission(ADDRESS);
  if (wire.endTransmission() != 0) return false;

  const uint8_t gain = INPUT_GAIN_CODE > 4 ? 4 : INPUT_GAIN_CODE;
  const uint8_t stereoGain = static_cast<uint8_t>((gain << 4) | gain);

  const Setting settings[] = {
      {0x01, 0x50},        // Analog bias profile.
      {0x02, 0x00},        // Digital core and clocks on.
      {0x08, 0x00},        // Codec is the I2S slave.
      {0x04, 0xC0},        // DAC and analog outputs off.
      {0x00, 0x12},        // Play/record clock profile.
      {0x03, 0xFF},        // ADC off while it is configured.
      {0x09, stereoGain},  // Left and right PGA gain.
      {0x0A, 0x50},        // LINPUT2/RINPUT2: board LINE IN.
      {0x0B, 0x02},        // Stereo ADC, data output enabled.
      {0x0C, 0x0C},        // Standard I2S, 16-bit.
      {0x0D, 0x02},        // MCLK/Fs = 256.
      {0x0E, 0x30},        // High-pass filters on.
      {0x0F, 0x00},        // ADC unmuted.
      {0x10, 0x00},        // Left digital volume: 0 dB.
      {0x11, 0x00},        // Right digital volume: 0 dB.
      {0x12, 0x00},        // Automatic level control off.
      {0x16, 0x00},        // Noise gate off.
      {0x03, 0x09},        // ADC/line on, microphone bias off.
  };

  for (const Setting& setting : settings) {
    if (!write(setting.reg, setting.value)) return false;
  }
  return true;
}

}  // namespace Es8388
