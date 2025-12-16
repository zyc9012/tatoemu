#include "emulator.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom_file> [bootrom_file]" << std::endl;
        std::cout << "\nAlternatively, set BOOTROM environment variable to bootrom path" << std::endl;
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

    fs::path romFile = fs::path(argv[1]);
    fs::path bootromFile;

    // Check for bootrom from command line or environment variable
    if (argc >= 3) {
        bootromFile = fs::path(argv[2]);
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

