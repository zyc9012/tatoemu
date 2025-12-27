#include "emulator.h"
#include "config.h"
#include <iostream>
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
        std::cout << "Usage: " << argv_narrow[0] << " <rom_file> [bootrom_file] [options]" << std::endl;
        std::cout << "\nOptions:" << std::endl;
        std::cout << "  --scale <n>           Window scale factor (default: " << Config::Window::Scale << ")" << std::endl;
        std::cout << "  --scale-mode <mode>   Scale mode: linear, nearest (default: linear)" << std::endl;
        std::cout << "  --sample-rate <hz>    Audio sample rate (default: " << Config::Audio::SampleRate << ")" << std::endl;
        std::cout << "  --volume <0.0-1.0>    Audio volume (default: " << Config::Audio::Volume << ")" << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  Arrow Keys  - D-Pad" << std::endl;
        std::cout << "  Z           - A Button" << std::endl;
        std::cout << "  X           - B Button" << std::endl;
        std::cout << "  Enter       - Start" << std::endl;
        std::cout << "  Shift       - Select" << std::endl;
        std::cout << "  F5          - Save State" << std::endl;
        std::cout << "  F9          - Load State" << std::endl;
        std::cout << "  P           - Pause / Resume" << std::endl;
        std::cout << "  ESC         - Quit" << std::endl;
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
                std::cerr << "Warning: Unknown scale mode '" << mode << "', using linear" << std::endl;
            }
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            Config::Audio::SampleRate = std::stoul(argv_narrow[++i]);
        } else if (arg == "--volume" && i + 1 < argc) {
            float volume = std::stof(argv_narrow[++i]);
            if (volume < 0.0f || volume > 1.0f) {
                std::cerr << "Warning: Volume should be between 0.0 and 1.0, clamping to " 
                          << (volume < 0.0f ? 0.0f : 1.0f) << std::endl;
                volume = (volume < 0.0f) ? 0.0f : 1.0f;
            }
            Config::Audio::Volume = volume;
        } else if (arg == "--bootrom" && i + 1 < argc) {
            bootromFile = fs::path(argv_narrow[++i]);
        } else if (arg[0] != '-') {
            // Positional argument (rom file)
            if (romFile.empty()) {
                romFile = fs::path(arg);
            } else {
                std::cerr << "Warning: Unexpected argument: " << arg << std::endl;
            }
        } else {
            std::cerr << "Warning: Unknown option: " << arg << std::endl;
        }
    }

    if (romFile.empty()) {
        std::cerr << "Error: ROM file is required" << std::endl;
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

    std::cout << "Starting emulation..." << std::endl;
    std::cout << "Press ESC to quit" << std::endl;

    emulator.run();

    return 0;
}

