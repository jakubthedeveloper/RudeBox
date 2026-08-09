#pragma once

#include <filesystem>
#include <string>

namespace SensitivitySvg {

bool write(const std::filesystem::path& outputPath, const std::string& title,
           float sensitivity);

}  // namespace SensitivitySvg
