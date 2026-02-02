#include "emulator.h"
#include "config.h"
#include "neogeo/config.h"
#include <string>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <vector>

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>

// Helper function to convert wide string to narrow string
std::string wstring_to_string(const wchar_t* wstr) {
    if (!wstr) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> buffer(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buffer.data(), size_needed, nullptr, nullptr);
    return std::string(buffer.data());
}

int wmain(int argc, wchar_t* argv[]) {
    // Convert wide string arguments to narrow strings for easier parsing
    std::vector<std::string> args;
    std::vector<const char*> arg_ptrs;
    for (int i = 0; i < argc; i++) {
        args.push_back(wstring_to_string(argv[i]));
        arg_ptrs.push_back(args.back().c_str());
    }
    // Create a new argv-like array
    const char** argv_narrow = arg_ptrs.data();
#else
int main(int argc, char* argv[]) {
    const char** argv_narrow = const_cast<const char**>(argv);
#endif
    if (argc < 2) {
        log_info("Usage: %s <rom_file> [bootrom_file] [options]", argv_narrow[0]);
        log_info("Options:");
        log_info("  --scale <n>           Window scale factor (default: 0, auto)");
        log_info("  --scale-mode <mode>   Scale mode: linear, nearest (default: linear)");
        log_info("  --sample-rate <hz>    Audio sample rate (default: %d)", Config::Audio::SampleRate);
        log_info("  --volume <0.0-1.0>    Audio volume (default: %.1f)", Config::Audio::Volume);
        log_info("  --neo-sys <system>    NeoGeo system: aes, mvs (default: mvs)");
        log_info("  --neo-bios <index>    NeoGeo BIOS index: 0 ~ 34 (default: %d)", static_cast<int>(neogeo::Config::BiosIndex));
        return 1;
    }

    fs::path romFile;
    fs::path bootromFile;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv_narrow[i];
        
        if (arg == "--scale" && i + 1 < argc) {
            Config::Window::Scale = std::stoul(argv_narrow[++i]);
        } else if (arg == "--scale-mode" && i + 1 < argc) {
            std::string mode = argv_narrow[++i];
            std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
            if (mode == "nearest") {
                Config::Window::ScaleMode = SDL_SCALEMODE_NEAREST;
            } else if (mode == "linear") {
                Config::Window::ScaleMode = SDL_SCALEMODE_LINEAR;
            } else {
                log_error("Warning: Unknown scale mode '%s', using linear", mode.c_str());
            }
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            Config::Audio::SampleRate = std::stoul(argv_narrow[++i]);
        } else if (arg == "--volume" && i + 1 < argc) {
            float volume = std::stof(argv_narrow[++i]);
            if (volume < 0.0f || volume > 1.0f) {
                log_error("Warning: Volume should be between 0.0 and 1.0, clamping to %.1f", (volume < 0.0f ? 0.0f : 1.0f));
                volume = (volume < 0.0f) ? 0.0f : 1.0f;
            }
            Config::Audio::Volume = volume;
        } else if (arg == "--bootrom" && i + 1 < argc) {
            bootromFile = fs::path(argv[++i]);
        } else if (arg == "--neo-sys" && i + 1 < argc) {
            std::string system = argv_narrow[++i];
            std::transform(system.begin(), system.end(), system.begin(), ::tolower);
            if (system == "aes") {
                neogeo::Config::System = neogeo::SystemType::AES;
            } else if (system == "mvs") {
                neogeo::Config::System = neogeo::SystemType::MVS;
            } else {
                log_error("Warning: Unknown NeoGeo system '%s', using MVS", system.c_str());
            }
        } else if (arg == "--neo-bios" && i + 1 < argc) {
            u8 biosIndex = static_cast<u8>(std::stoul(argv_narrow[++i]));
            if (biosIndex < 0 || biosIndex > 34) {
                log_error("Warning: NeoGeo BIOS index should be between 0 and 34, using %d", static_cast<int>(neogeo::Config::BiosIndex));
                biosIndex = neogeo::Config::BiosIndex;
            }
            neogeo::Config::BiosIndex = biosIndex;
        } else if (arg[0] != '-') {
            // Positional argument (rom file)
            if (romFile.empty()) {
                romFile = fs::path(argv[i]);
            } else {
                log_error("Warning: Unexpected argument: %s", arg.c_str());
            }
        } else {
            log_error("Warning: Unknown option: %s", arg.c_str());
        }
    }

    if (romFile.empty()) {
        log_error("Error: ROM file is required");
        return 1;
    }

    Emulator emulator;
    
    // Load bootrom if provided (optional)
    if (!bootromFile.empty()) {
        emulator.loadBootrom(bootromFile);
    }

    if (!emulator.loadROM(romFile)) {
        return 1;
    }

    log_info("Starting emulation...");
    log_info("Press ESC to quit");

    emulator.run();

    return 0;
}

