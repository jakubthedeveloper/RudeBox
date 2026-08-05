#include "es8388.h"

#include <Arduino.h>
#include <Wire.h>

#include "app_config.h"

namespace Es8388 {
namespace {

constexpr uint8_t ADDRESS = 0x10;
constexpr int PA_ENABLE_PIN = 21;

TwoWire wire(1);

struct Setting {
  uint8_t reg;
  uint8_t value;
};

void setPowerAmplifierEnabled(bool enabled) {
  digitalWrite(PA_ENABLE_PIN, enabled ? HIGH : LOW);
}

void preparePowerAmplifier() {
  pinMode(PA_ENABLE_PIN, OUTPUT);
  setPowerAmplifierEnabled(false);
}

bool connectToCodec() {
  if (!wire.begin(33, 32, 400000)) return false;

  wire.beginTransmission(ADDRESS);
  return wire.endTransmission() == 0;
}

bool writeAndVerify(uint8_t reg, uint8_t value) {
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

bool configureCodec() {
  const uint8_t gain =
      AppConfig::AudioInput::GAIN_CODE > 4 ? 4
                                           : AppConfig::AudioInput::GAIN_CODE;
  const uint8_t stereoGain = static_cast<uint8_t>((gain << 4) | gain);

  const Setting settings[] = {
      {0x19, 0x04},        // Mute the DAC during setup.
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
      {0x17, 0x18},        // DAC standard I2S, 16-bit.
      {0x18, 0x02},        // DAC MCLK/Fs = 256.
      {0x1A, 0x00},        // Left DAC digital volume: 0 dB.
      {0x1B, 0x00},        // Right DAC digital volume: 0 dB.
      {0x26, 0x00},        // Analog bypass inputs not selected.
      {0x27, 0x90},        // Left DAC routed to left output mixer.
      {0x2A, 0x90},        // Right DAC routed to right output mixer.
      {0x2B, 0x80},        // ADC and DAC share the same LRCK.
      {0x2E, 0x1E},        // LOUT1 analog volume: 0 dB.
      {0x2F, 0x1E},        // ROUT1 analog volume: 0 dB.
      {0x30, 0x1E},        // LOUT2 analog volume: 0 dB.
      {0x31, 0x1E},        // ROUT2 analog volume: 0 dB.
      {0x03, 0x09},        // ADC/line on, microphone bias off.
      {0x04, 0x3C},        // Power both DACs and all L/R output drivers.
      {0x19, 0x00},        // Unmute the DAC.
  };

  for (const Setting& setting : settings) {
    if (!writeAndVerify(setting.reg, setting.value)) return false;
  }
  return true;
}

}  // namespace

bool begin() {
  preparePowerAmplifier();
  if (!connectToCodec()) return false;
  if (!configureCodec()) return false;

  setPowerAmplifierEnabled(true);
  return true;
}

}  // namespace Es8388
