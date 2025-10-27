#include "emulator.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom_file>" << std::endl;
        std::cout << "\nGameBoy Emulator Controls:" << std::endl;
        std::cout << "  Arrow Keys  - D-Pad" << std::endl;
        std::cout << "  Z           - A Button" << std::endl;
        std::cout << "  X           - B Button" << std::endl;
        std::cout << "  Enter       - Start" << std::endl;
        std::cout << "  Shift       - Select" << std::endl;
        std::cout << "  ESC         - Quit" << std::endl;
        return 1;
    }

    std::string romFile = argv[1];

    Emulator emulator;
    
    if (!emulator.initialize()) {
        std::cerr << "Failed to initialize emulator" << std::endl;
        return 1;
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

