#pragma once

#include "types.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "cartridge.h"
#include "apu.h"
#include <memory>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Emulator {
public:
    Emulator();
    ~Emulator();

    bool initialize();
    bool loadROM(const std::string& filename);
    void run();
    void shutdown();

private:
    void handleInput();
    void update();
    void render();

    // SDL components
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
    
    // Emulator components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<MMU> m_mmu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Joypad> m_joypad;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<APU> m_apu;
    
    bool m_running;
    u32 m_cyclesThisFrame;
    
    // Frame timing
    u64 m_lastFrameTime;
    const double m_targetFrameTime = 1000.0 / TARGET_FPS;
};

