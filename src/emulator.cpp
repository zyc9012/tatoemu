#include "emulator.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>

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

    m_currentROMPath = filename;

    // Enable GBC mode if cartridge supports it
    bool isGBC = m_cartridge->isGBC();
    m_cpu->setGBCMode(isGBC);
    m_ppu->setGBCMode(isGBC);
    m_mmu->setGBCMode(isGBC);
    
    if (isGBC) {
        SDL_SetWindowTitle(m_window, ("GameBoy Color - " + m_cartridge->getTitle()).c_str());
    } else {
        SDL_SetWindowTitle(m_window, ("GameBoy - " + m_cartridge->getTitle()).c_str());
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
        
        double frameTime = SDL_GetTicks() - m_lastFrameTime;
        
        // Detect if we've been paused (e.g., window dragging)
        if (frameTime > m_targetFrameTime * 2.0) {
            // Clear audio buffer to prevent desync
            if (m_apu) {
                m_apu->clearAudioBuffer();
            }
            m_lastFrameTime = SDL_GetTicks();
        // Frame rate limiting based on target time
        } else if (frameTime < m_targetFrameTime) {
            double remaining = m_targetFrameTime - frameTime;
            if (remaining > 0) {
                SDL_Delay(static_cast<u32>(remaining));
            }
            if (m_apu) {
                int queuedAudio = m_apu->getQueuedAudioSize();
                if (queuedAudio > m_targetAudioBufferSize) {
                    // Audio buffer is too full, we're running too fast
                    SDL_Delay(2);
                }
            }
            
            m_lastFrameTime = SDL_GetTicks();
        }
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
                    case SDLK_F5: // Save state
                        if (!m_currentROMPath.empty()) {
                            std::string saveFilename = m_currentROMPath.substr(0, m_currentROMPath.find_last_of('.')) + ".sav";
                            saveState(saveFilename);
                            std::cout << "State saved to " << saveFilename << std::endl;
                        }
                        break;
                    case SDLK_F9: // Load state
                        if (!m_currentROMPath.empty()) {
                            std::string saveFilename = m_currentROMPath.substr(0, m_currentROMPath.find_last_of('.')) + ".sav";
                            loadState(saveFilename);
                            std::cout << "State loaded from " << saveFilename << std::endl;
                        }
                        break;
                    case SDLK_F6: // Dump tiles to BMP
                        {
                            std::string bmpFilename = "tiles_dump.bmp";
                            if (m_ppu->dumpTilesToBMP(bmpFilename)) {
                                std::cout << "Tiles dumped to " << bmpFilename << std::endl;
                            } else {
                                std::cerr << "Failed to dump tiles to " << bmpFilename << std::endl;
                            }
                        }
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
    // Close audio first to prevent segfaults
    if (m_apu) {
        m_apu->closeAudio();
    }
    
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

void Emulator::saveState(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return;
    }
    
    // Write a simple header
    const char* header = "GBEMU";
    file.write(header, 5);
    u32 version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Save all component states
    m_cpu->saveState(file);
    m_mmu->saveState(file);
    m_ppu->saveState(file);
    m_timer->saveState(file);
    m_joypad->saveState(file);
    m_apu->saveState(file);
    m_cartridge->saveState(file);
    
    // Save emulator state
    file.write(reinterpret_cast<const char*>(&m_cyclesThisFrame), sizeof(m_cyclesThisFrame));
    
    file.close();
}

void Emulator::loadState(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return;
    }
    
    // Read and verify header
    char header[6] = {0};
    file.read(header, 5);
    if (std::string(header) != "GBEMU") {
        std::cerr << "Invalid save state file format" << std::endl;
        return;
    }
    
    u32 version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        std::cerr << "Unsupported save state version" << std::endl;
        return;
    }
    
    // Load all component states
    m_cpu->loadState(file);
    m_mmu->loadState(file);
    m_ppu->loadState(file);
    m_timer->loadState(file);
    m_joypad->loadState(file);
    m_apu->loadState(file);
    m_cartridge->loadState(file);
    
    // Load emulator state
    file.read(reinterpret_cast<char*>(&m_cyclesThisFrame), sizeof(m_cyclesThisFrame));
    
    file.close();
}

