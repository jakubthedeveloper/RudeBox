#include "sensitivity_svg.h"

#include <algorithm>
#include <fstream>
#include <iomanip>

#include "app_config.h"
#include "hit_detector.h"

namespace SensitivitySvg {
namespace {

constexpr int IMAGE_WIDTH = 1280;
constexpr int IMAGE_HEIGHT = 620;
constexpr int PLOT_LEFT = 90;
constexpr int PLOT_TOP = 105;
constexpr int PLOT_WIDTH = 1130;
constexpr int PLOT_HEIGHT = 430;
constexpr int SAMPLE_COUNT = 240;

float plottedPeakMaximum() {
  return std::max(
      AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY,
      AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY);
}

double xCoordinate(float peak) {
  return PLOT_LEFT + peak * PLOT_WIDTH / plottedPeakMaximum();
}

double yCoordinate(float velocity) {
  return PLOT_TOP + (1.0f - velocity) * PLOT_HEIGHT;
}

void writeGrid(std::ofstream& output) {
  for (int tick = 0; tick <= 5; ++tick) {
    const float fraction = tick / 5.0f;
    const double x = PLOT_LEFT + fraction * PLOT_WIDTH;
    const double y = PLOT_TOP + (1.0f - fraction) * PLOT_HEIGHT;
    output << "<line x1=\"" << x << "\" y1=\"" << PLOT_TOP
           << "\" x2=\"" << x << "\" y2=\"" << PLOT_TOP + PLOT_HEIGHT
           << "\" class=\"grid\"/>\n"
           << "<line x1=\"" << PLOT_LEFT << "\" y1=\"" << y
           << "\" x2=\"" << PLOT_LEFT + PLOT_WIDTH << "\" y2=\"" << y
           << "\" class=\"grid\"/>\n"
           << "<text x=\"" << x << "\" y=\"" << PLOT_TOP + PLOT_HEIGHT + 26
           << "\" class=\"tick\" text-anchor=\"middle\">"
           << static_cast<int>(fraction * plottedPeakMaximum()) << "</text>\n"
           << "<text x=\"" << PLOT_LEFT - 12 << "\" y=\"" << y + 5
           << "\" class=\"tick\" text-anchor=\"end\">" << std::fixed
           << std::setprecision(1) << fraction << "</text>\n";
  }
}

void writeCurve(std::ofstream& output, float sensitivity) {
  output << "<polyline class=\"curve\" points=\"";
  for (int sample = 0; sample <= SAMPLE_COUNT; ++sample) {
    const float peak = plottedPeakMaximum() * sample / SAMPLE_COUNT;
    const float velocity = HitDetector::mapVelocity(
        static_cast<uint16_t>(peak), sensitivity);
    output << std::fixed << std::setprecision(2) << xCoordinate(peak) << ','
           << yCoordinate(velocity) << ' ';
  }
  output << "\"/>\n";
}

}  // namespace

bool write(const std::filesystem::path& outputPath, const std::string& title,
           float sensitivity) {
  std::error_code directoryError;
  std::filesystem::create_directories(outputPath.parent_path(), directoryError);
  if (directoryError) return false;

  std::ofstream output(outputPath);
  if (!output) return false;

  output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
         << IMAGE_WIDTH << "\" height=\"" << IMAGE_HEIGHT
         << "\" viewBox=\"0 0 " << IMAGE_WIDTH << ' ' << IMAGE_HEIGHT
         << "\">\n"
         << "<style>.background{fill:#10151c}.plot{fill:#17212b;stroke:#637083}.grid{stroke:#35404d;stroke-width:1}.curve{fill:none;stroke:#43d9a3;stroke-width:3}.title{fill:#f4f7fa;font:700 24px sans-serif}.details{fill:#aebbc9;font:14px sans-serif}.tick{fill:#8e9baa;font:12px sans-serif}.axis{fill:#cbd5df;font:13px sans-serif}</style>\n"
         << "<rect width=\"100%\" height=\"100%\" class=\"background\"/>\n"
         << "<text x=\"" << PLOT_LEFT << "\" y=\"38\" class=\"title\">"
         << title << "</text>\n"
         << "<text x=\"" << PLOT_LEFT << "\" y=\"66\" class=\"details\">Sensitivity: "
         << std::fixed << std::setprecision(2) << sensitivity
         << " | PAD_INPUT_MIN: " << AppConfig::HitDetection::PAD_INPUT_MIN
         << " | PAD_INPUT_MAX: " << AppConfig::HitDetection::PAD_INPUT_MAX
         << "</text>\n"
         << "<rect x=\"" << PLOT_LEFT << "\" y=\"" << PLOT_TOP
         << "\" width=\"" << PLOT_WIDTH << "\" height=\"" << PLOT_HEIGHT
         << "\" class=\"plot\"/>\n";

  writeGrid(output);
  writeCurve(output, sensitivity);

  output << "<text x=\"" << PLOT_LEFT + PLOT_WIDTH / 2 << "\" y=\"590\" class=\"axis\" text-anchor=\"middle\">Captured pad peak</text>\n"
         << "<text x=\"24\" y=\"" << PLOT_TOP + PLOT_HEIGHT / 2
         << "\" class=\"axis\" text-anchor=\"middle\" transform=\"rotate(-90 24 "
         << PLOT_TOP + PLOT_HEIGHT / 2 << ")\">Velocity</text>\n"
         << "</svg>\n";
  return output.good();
}

}  // namespace SensitivitySvg
