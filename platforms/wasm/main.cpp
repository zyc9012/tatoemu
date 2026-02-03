#include "emulator.h"
#include "config.h"
#include "neogeo/config.h"
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <string>
#include <algorithm>
#include <cstdio>

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

// C functions exported to JavaScript for file loading and configuration
extern "C" {
    // Load ROM from uploaded file
    int loadROMFile(const char* filename) {
        if (!g_emulatorWasm || !g_emulatorWasm->initialized) {
            log_error("Emulator not initialized!");
            return 0;
        }
        
        log_info("Loading ROM: %s", filename);
        
        if (g_emulatorWasm->emulator.loadROM(filename)) {
            log_info("ROM loaded successfully!");
            g_emulatorWasm->romLoaded = true;
            g_emulatorWasm->running = true;
            return 1;
        } else {
            return 0;
        }
    }
    
    // Load bootrom from uploaded file
    int loadBootromFile(const char* filename) {
        if (!g_emulatorWasm || !g_emulatorWasm->initialized) {
            log_error("Emulator not initialized!");
            return 0;
        }
        
        log_info("Loading bootrom: %s", filename);
        
        if (g_emulatorWasm->emulator.loadBootrom(filename)) {
            log_info("Bootrom loaded successfully!");
            return 1;
        } else {
            log_error("Failed to load bootrom");
            return 0;
        }
    }

    // Configuration functions
    void setScaleMode(const char* mode) {
        std::string modeStr = mode;
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);
        if (modeStr == "nearest") {
            Config::Window::ScaleMode = SDL_SCALEMODE_NEAREST;
        } else if (modeStr == "linear") {
            Config::Window::ScaleMode = SDL_SCALEMODE_LINEAR;
        } else {
            log_error("Warning: Unknown scale mode '%s', using linear", modeStr.c_str());
            Config::Window::ScaleMode = SDL_SCALEMODE_LINEAR;
        }
    }

    void setVolume(int volume) {
        Config::Audio::Volume = static_cast<u32>(volume);
    }

    void setNeoSys(const char* sys) {
        std::string sysStr = sys;
        std::transform(sysStr.begin(), sysStr.end(), sysStr.begin(), ::tolower);
        if (sysStr == "aes") {
            neogeo::Config::System = neogeo::SystemType::AES;
        } else if (sysStr == "mvs") {
            neogeo::Config::System = neogeo::SystemType::MVS;
        } else {
            log_error("Warning: Unknown NeoGeo system '%s', using MVS", sysStr.c_str());
            neogeo::Config::System = neogeo::SystemType::MVS;
        }
    }

    void setNeoBios(const char* bios) {
        u8 biosIndex = static_cast<u8>(std::stoul(bios));
        if (biosIndex < 0 || biosIndex > 34) {
            log_error("Warning: NeoGeo BIOS index should be between 0 and 34, using %d", static_cast<int>(neogeo::Config::BiosIndex));
            biosIndex = neogeo::Config::BiosIndex;
        }
        neogeo::Config::BiosIndex = biosIndex;
    }
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
    log_info("TatoEmu (WebAssembly)");
    log_info("");
    log_info("Waiting for ROM file upload...");

    g_emulatorWasm = new EmulatorWasm();
    g_emulatorWasm->initialized = true;
    log_info("Emulator initialized successfully");
    log_info("Please upload a ROM file to begin");

    // Set up the main loop using Emscripten's mechanism
    // 0 = use browser's requestAnimationFrame for timing (recommended)
    // 1 = simulate infinite loop
    emscripten_set_main_loop(emscripten_main_loop, 0, 1);

    // Note: Code after this point won't execute until the main loop is cancelled
    // Cleanup happens when the page is closed
    delete g_emulatorWasm;
    
    return 0;
}
