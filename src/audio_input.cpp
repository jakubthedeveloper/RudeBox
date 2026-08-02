#include "audio_input.h"

#include <Arduino.h>
#include <driver/i2s.h>

namespace AudioInput {
namespace {

constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t BLOCK_FRAMES = 256;

int16_t buffer[BLOCK_FRAMES * 2];

uint16_t magnitude(int16_t sample) {
  const int32_t value = sample;
  return static_cast<uint16_t>(value < 0 ? -value : value);
}

}  // namespace

bool begin() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = BLOCK_FRAMES,
      .use_apll = true,
      .tx_desc_auto_clear = false,
      .fixed_mclk = SAMPLE_RATE * 256,
  };

  const i2s_pin_config_t pins = {
      .mck_io_num = 0,
      .bck_io_num = 27,
      .ws_io_num = 25,
      .data_out_num = 26,
      .data_in_num = 35,
  };

  if (i2s_driver_install(PORT, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(PORT, &pins) == ESP_OK) return true;

  i2s_driver_uninstall(PORT);
  return false;
}

bool readPeak(Channel channel, uint16_t& peak) {
  size_t bytesRead = 0;
  if (i2s_read(PORT, buffer, sizeof(buffer), &bytesRead, pdMS_TO_TICKS(100)) != ESP_OK) {
    return false;
  }

  const size_t frames = bytesRead / (2 * sizeof(int16_t));
  if (frames == 0) return false;

  const size_t slot = channel == Channel::Left ? 0 : 1;
  peak = 0;
  for (size_t frame = 0; frame < frames; ++frame) {
    const uint16_t value = magnitude(buffer[frame * 2 + slot]);
    if (value > peak) peak = value;
  }
  return true;
}

}  // namespace AudioInput
