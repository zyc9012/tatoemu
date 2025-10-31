#include "emulator.h"
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom_file> [bootrom_file]" << std::endl;
        std::cout << "\nAlternatively, set BOOTROM environment variable to bootrom path" << std::endl;
        std::cout << "\nGameBoy Emulator Controls:" << std::endl;
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

    std::string romFile = argv[1];
    std::string bootromFile;

    // Check for bootrom from command line or environment variable
    if (argc >= 3) {
        bootromFile = argv[2];
    } else {
        const char* bootromEnv = std::getenv("BOOTROM");
        if (bootromEnv) {
            bootromFile = bootromEnv;
        }
    }

    Emulator emulator;
    
    if (!emulator.initialize()) {
        std::cerr << "Failed to initialize emulator" << std::endl;
        return 1;
    }

    // Load bootrom if provided (optional)
    if (!bootromFile.empty()) {
        std::cout << "Loading bootrom: " << bootromFile << std::endl;
        emulator.loadBootrom(bootromFile);
    } else {
        std::cout << "No bootrom provided, starting with post-boot state" << std::endl;
    }

    if (!emulator.loadROM(romFile)) {
        std::cerr << "Failed to load ROM: " << romFile << std::endl;
        return 1;
    }

    std::cout << "Starting emulation..." << std::endl;
    std::cout << "Press ESC to quit" << std::endl;

    emulator.run();

    return 0;
}

