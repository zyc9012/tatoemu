#include "emulator.h"
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <iostream>
#include <string>

// Emulator wrapper class with public access to necessary methods
class EmulatorWasm {
public:
    Emulator emulator;
    bool running;
    bool initialized;
    bool romLoaded;
    
    EmulatorWasm() : running(false), initialized(false), romLoaded(false) {}
    
    void mainLoop() {
        if (!running || !romLoaded) return;

        emulator.runFrame();
    }
};

// Global emulator instance
EmulatorWasm* g_emulatorWasm = nullptr;

// Main loop callback for Emscripten
void emscripten_main_loop() {
    if (g_emulatorWasm) {
        g_emulatorWasm->mainLoop();
    }
}

// C functions exported to JavaScript for file loading
extern "C" {
    // Load ROM from uploaded file
    int loadROMFile(const char* filename) {
        if (!g_emulatorWasm || !g_emulatorWasm->initialized) {
            std::cerr << "Emulator not initialized!" << std::endl;
            return 0;
        }
        
        std::cout << "Loading ROM: " << filename << std::endl;
        
        if (g_emulatorWasm->emulator.loadROM(filename)) {
            std::cout << "ROM loaded successfully!" << std::endl;
            g_emulatorWasm->romLoaded = true;
            g_emulatorWasm->running = true;
            return 1;
        } else {
            std::cerr << "Failed to load ROM" << std::endl;
            return 0;
        }
    }
    
    // Load bootrom from uploaded file
    int loadBootromFile(const char* filename) {
        if (!g_emulatorWasm || !g_emulatorWasm->initialized) {
            std::cerr << "Emulator not initialized!" << std::endl;
            return 0;
        }
        
        std::cout << "Loading bootrom: " << filename << std::endl;
        
        if (g_emulatorWasm->emulator.loadBootrom(filename)) {
            std::cout << "Bootrom loaded successfully!" << std::endl;
            return 1;
        } else {
            std::cerr << "Failed to load bootrom" << std::endl;
            return 0;
        }
    }
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
    std::cout << "TatoEmu (WebAssembly)" << std::endl;
    std::cout << std::endl;
    std::cout << "Waiting for ROM file upload..." << std::endl;

    g_emulatorWasm = new EmulatorWasm();
    g_emulatorWasm->initialized = true;
    std::cout << "Emulator initialized successfully" << std::endl;
    std::cout << "Please upload a ROM file to begin" << std::endl;

    // Set up the main loop using Emscripten's mechanism
    // 0 = use browser's requestAnimationFrame for timing (recommended)
    // 1 = simulate infinite loop
    emscripten_set_main_loop(emscripten_main_loop, 0, 1);

    // Note: Code after this point won't execute until the main loop is cancelled
    // Cleanup happens when the page is closed
    delete g_emulatorWasm;
    
    return 0;
}
