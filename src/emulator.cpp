#include "emulator.h"
#include <SDL3/SDL.h>
#include <iostream>

Emulator::Emulator()
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_texture(nullptr)
    , m_running(false)
    , m_cyclesThisFrame(0)
    , m_lastFrameTime(0) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    m_window = SDL_CreateWindow(
        "GameBoy Emulator",
        SCREEN_WIDTH * 4,
        SCREEN_HEIGHT * 4,
        SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetRenderVSync(m_renderer, 1);

    // Create texture for framebuffer
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!m_texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create emulator components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_mmu = std::make_unique<MMU>();
    m_ppu = std::make_unique<PPU>();
    m_joypad = std::make_unique<Joypad>();
    m_timer = std::make_unique<Timer>();
    m_apu = std::make_unique<APU>();

    // Wire up components
    m_mmu->setCartridge(m_cartridge.get());
    m_mmu->setPPU(m_ppu.get());
    m_mmu->setJoypad(m_joypad.get());
    m_mmu->setTimer(m_timer.get());
    m_mmu->setAPU(m_apu.get());
    
    m_cpu->setMMU(m_mmu.get());
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setMMU(m_mmu.get());
    m_joypad->setCPU(m_cpu.get());
    m_timer->setCPU(m_cpu.get());
    m_apu->setCPU(m_cpu.get());
    
    // Initialize audio
    if (!m_apu->initializeAudio()) {
        std::cerr << "Warning: Failed to initialize audio" << std::endl;
        // Continue anyway - emulator can run without audio
    }

    std::cout << "Emulator initialized successfully" << std::endl;
    return true;
}

bool Emulator::loadROM(const std::string& filename) {
    if (!m_cartridge->load(filename)) {
        return false;
    }

    // Reset CPU after loading ROM
    m_cpu->reset();
    m_ppu->reset();
    m_timer->reset();
    m_apu->reset();

    return true;
}

void Emulator::run() {
    m_running = true;
    m_cyclesThisFrame = 0;
    m_lastFrameTime = SDL_GetTicks();

    while (m_running) {
        handleInput();
        update();
        render();
        
        // Frame rate limiting - sleep if we're running too fast
        double frameTime = SDL_GetTicks() - m_lastFrameTime;
        if (frameTime < m_targetFrameTime) {
            SDL_Delay(static_cast<u32>(m_targetFrameTime - frameTime));
        }
        
        m_lastFrameTime = SDL_GetTicks();
    }
}

void Emulator::handleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        m_running = false;
                        break;
                    case SDLK_Z: // A button
                        m_joypad->pressButton(BUTTON_A);
                        break;
                    case SDLK_X: // B button
                        m_joypad->pressButton(BUTTON_B);
                        break;
                    case SDLK_RETURN: // Start
                        m_joypad->pressButton(BUTTON_START);
                        break;
                    case SDLK_RSHIFT: // Select
                    case SDLK_LSHIFT:
                        m_joypad->pressButton(BUTTON_SELECT);
                        break;
                    case SDLK_UP:
                        m_joypad->pressButton(BUTTON_UP);
                        break;
                    case SDLK_DOWN:
                        m_joypad->pressButton(BUTTON_DOWN);
                        break;
                    case SDLK_LEFT:
                        m_joypad->pressButton(BUTTON_LEFT);
                        break;
                    case SDLK_RIGHT:
                        m_joypad->pressButton(BUTTON_RIGHT);
                        break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (event.key.key) {
                    case SDLK_Z: // A button
                        m_joypad->releaseButton(BUTTON_A);
                        break;
                    case SDLK_X: // B button
                        m_joypad->releaseButton(BUTTON_B);
                        break;
                    case SDLK_RETURN: // Start
                        m_joypad->releaseButton(BUTTON_START);
                        break;
                    case SDLK_RSHIFT: // Select
                    case SDLK_LSHIFT:
                        m_joypad->releaseButton(BUTTON_SELECT);
                        break;
                    case SDLK_UP:
                        m_joypad->releaseButton(BUTTON_UP);
                        break;
                    case SDLK_DOWN:
                        m_joypad->releaseButton(BUTTON_DOWN);
                        break;
                    case SDLK_LEFT:
                        m_joypad->releaseButton(BUTTON_LEFT);
                        break;
                    case SDLK_RIGHT:
                        m_joypad->releaseButton(BUTTON_RIGHT);
                        break;
                }
                break;
        }
    }
}

void Emulator::update() {
    // Run until we've executed enough cycles for one frame
    while (m_cyclesThisFrame < CYCLES_PER_FRAME) {
        u32 cycles = m_cpu->step();
        
        m_ppu->step(cycles);
        m_timer->step(cycles);
        m_apu->step(cycles);
        
        m_cyclesThisFrame += cycles;
    }
    
    m_cyclesThisFrame -= CYCLES_PER_FRAME;
}

void Emulator::render() {
    if (m_ppu->isFrameReady()) {
        // Update texture with framebuffer
        SDL_UpdateTexture(
            m_texture,
            nullptr,
            m_ppu->getFramebuffer(),
            SCREEN_WIDTH * sizeof(u32)
        );
        
        m_ppu->clearFrameReady();
    }

    // Clear screen
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // Render texture
    SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);

    // Present
    SDL_RenderPresent(m_renderer);
}

void Emulator::shutdown() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

