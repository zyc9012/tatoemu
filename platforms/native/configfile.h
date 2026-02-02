#pragma once

#include <filesystem>

namespace fs = std::filesystem;

void loadConfigFile(const fs::path& exePath);
