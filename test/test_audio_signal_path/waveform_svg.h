#pragma once

#include <stdint.h>

#include <filesystem>
#include <string>
#include <vector>

namespace WaveformSvg {

struct PlotDescription {
  std::string title;
  uint16_t inputPeak;
  float sensitivity;
  float oscillatorPitchHz;
  float pitchDropOctaves;
  float clickLevel;
  float ampVelocity;
  uint32_t sampleRate;
};

bool write(const std::filesystem::path& outputPath,
           const std::vector<int16_t>& samples,
           const PlotDescription& description);

}  // namespace WaveformSvg
