#include "waveform_svg.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

namespace WaveformSvg {
namespace {

constexpr int IMAGE_WIDTH = 1280;
constexpr int IMAGE_HEIGHT = 560;
constexpr int PLOT_LEFT = 80;
constexpr int PLOT_TOP = 105;
constexpr int PLOT_WIDTH = 1160;
constexpr int PLOT_HEIGHT = 390;
constexpr int WAVEFORM_POINT_LIMIT = PLOT_WIDTH * 2;

int16_t outputPeak(const std::vector<int16_t>& samples) {
  int32_t peak = 0;
  for (const int16_t sample : samples) {
    const int32_t magnitude =
        sample < 0 ? -static_cast<int32_t>(sample) : sample;
    if (magnitude > peak) peak = magnitude;
  }
  return static_cast<int16_t>(peak);
}

double xCoordinate(size_t sampleIndex, size_t sampleCount) {
  return PLOT_LEFT + static_cast<double>(sampleIndex) * PLOT_WIDTH /
                         static_cast<double>(sampleCount - 1);
}

double yCoordinate(int16_t sample) {
  constexpr double FULL_SCALE = 32767.0;
  const double normalized = static_cast<double>(sample) / FULL_SCALE;
  return PLOT_TOP + (1.0 - normalized) * PLOT_HEIGHT / 2.0;
}

void writeHorizontalGrid(std::ofstream& output) {
  for (int tick = -2; tick <= 2; ++tick) {
    const double normalized = tick / 2.0;
    const double y = PLOT_TOP + (1.0 - normalized) * PLOT_HEIGHT / 2.0;
    output << "<line x1=\"" << PLOT_LEFT << "\" y1=\"" << y
           << "\" x2=\"" << PLOT_LEFT + PLOT_WIDTH << "\" y2=\"" << y
           << "\" class=\"grid\"/>\n";
    output << "<text x=\"" << PLOT_LEFT - 12 << "\" y=\"" << y + 5
           << "\" class=\"tick\" text-anchor=\"end\">" << normalized
           << "</text>\n";
  }
}

void writeVerticalGrid(std::ofstream& output, double durationMs) {
  constexpr int TICK_COUNT = 6;
  for (int tick = 0; tick <= TICK_COUNT; ++tick) {
    const double fraction = static_cast<double>(tick) / TICK_COUNT;
    const double x = PLOT_LEFT + fraction * PLOT_WIDTH;
    output << "<line x1=\"" << x << "\" y1=\"" << PLOT_TOP << "\" x2=\""
           << x << "\" y2=\"" << PLOT_TOP + PLOT_HEIGHT
           << "\" class=\"grid\"/>\n";
    output << "<text x=\"" << x << "\" y=\"" << PLOT_TOP + PLOT_HEIGHT + 26
           << "\" class=\"tick\" text-anchor=\"middle\">"
           << std::fixed << std::setprecision(0) << fraction * durationMs
           << "</text>\n";
  }
}

void writeWaveform(std::ofstream& output,
                   const std::vector<int16_t>& samples) {
  const size_t step = std::max<size_t>(
      1, (samples.size() + WAVEFORM_POINT_LIMIT - 1) /
             WAVEFORM_POINT_LIMIT);

  output << "<polyline class=\"waveform\" points=\"";
  for (size_t sample = 0; sample < samples.size(); sample += step) {
    output << std::fixed << std::setprecision(2)
           << xCoordinate(sample, samples.size()) << ','
           << yCoordinate(samples[sample]) << ' ';
  }
  if ((samples.size() - 1) % step != 0) {
    output << xCoordinate(samples.size() - 1, samples.size()) << ','
           << yCoordinate(samples.back());
  }
  output << "\"/>\n";
}

}  // namespace

bool write(const std::filesystem::path& outputPath,
           const std::vector<int16_t>& samples,
           const PlotDescription& description) {
  if (samples.size() < 2 || description.sampleRate == 0) return false;

  std::error_code directoryError;
  std::filesystem::create_directories(outputPath.parent_path(),
                                      directoryError);
  if (directoryError) return false;

  std::ofstream output(outputPath);
  if (!output) return false;

  const double durationMs =
      samples.size() * 1000.0 / description.sampleRate;

  output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
         << IMAGE_WIDTH << "\" height=\"" << IMAGE_HEIGHT << "\" viewBox=\"0 0 "
         << IMAGE_WIDTH << ' ' << IMAGE_HEIGHT << "\">\n"
         << "<style>"
         << ".background{fill:#10151c}.plot{fill:#17212b;stroke:#637083}"
         << ".grid{stroke:#35404d;stroke-width:1}.waveform{fill:none;stroke:#43d9a3;stroke-width:1.4}"
         << ".title{fill:#f4f7fa;font:700 24px sans-serif}.details{fill:#aebbc9;font:14px sans-serif}"
         << ".tick{fill:#8e9baa;font:12px sans-serif}.axis{fill:#cbd5df;font:13px sans-serif}"
         << "</style>\n"
         << "<rect width=\"100%\" height=\"100%\" class=\"background\"/>\n"
         << "<text x=\"" << PLOT_LEFT << "\" y=\"38\" class=\"title\">"
         << description.title << "</text>\n"
         << "<text x=\"" << PLOT_LEFT << "\" y=\"66\" class=\"details\">"
         << "Input peak: " << description.inputPeak
         << " | Sensitivity: " << std::fixed << std::setprecision(2)
         << description.sensitivity << " | Oscillator: "
         << description.oscillatorPitchHz << " Hz | Pitch drop: "
         << description.pitchDropOctaves << " oct | Click: "
         << description.clickLevel << " | AMP VEL: "
         << description.ampVelocity << " | Output peak: "
         << outputPeak(samples) << "</text>\n"
         << "<rect x=\"" << PLOT_LEFT << "\" y=\"" << PLOT_TOP
         << "\" width=\"" << PLOT_WIDTH << "\" height=\"" << PLOT_HEIGHT
         << "\" class=\"plot\"/>\n";

  writeHorizontalGrid(output);
  writeVerticalGrid(output, durationMs);
  writeWaveform(output, samples);

  output << "<text x=\"" << PLOT_LEFT + PLOT_WIDTH / 2 << "\" y=\"548\" class=\"axis\" text-anchor=\"middle\">Time (ms)</text>\n"
         << "<text x=\"22\" y=\"" << PLOT_TOP + PLOT_HEIGHT / 2
         << "\" class=\"axis\" text-anchor=\"middle\" transform=\"rotate(-90 22 "
         << PLOT_TOP + PLOT_HEIGHT / 2 << ")\">Normalized amplitude</text>\n"
         << "</svg>\n";

  return output.good();
}

}  // namespace WaveformSvg
