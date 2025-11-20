#include "emulator.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

SDLVideoDevice::SDLVideoDevice(SDL_Renderer* renderer, SDL_Texture* texture, u16 screenWidth, u16 screenHeight)
    : m_renderer(renderer)
    , m_texture(texture)
    , m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight) {
}

SDLVideoDevice::~SDLVideoDevice() {
}

void SDLVideoDevice::render(u32* buffer) {
    SDL_UpdateTexture(m_texture, nullptr, buffer, m_screenWidth * sizeof(u32));

    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // Get current window size
    int windowWidth, windowHeight;
    SDL_GetRenderOutputSize(m_renderer, &windowWidth, &windowHeight);
    
    // Calculate aspect ratio preserving destination rectangle
    float targetAspect = static_cast<float>(m_screenWidth) / static_cast<float>(m_screenHeight);
    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    
    SDL_FRect destRect;
    
    if (windowAspect > targetAspect) {
        // Window is wider than target aspect ratio - letterbox on sides
        destRect.h = static_cast<float>(windowHeight);
        destRect.w = destRect.h * targetAspect;
        destRect.x = (windowWidth - destRect.w) / 2.0f;
        destRect.y = 0;
    } else {
        // Window is taller than target aspect ratio - letterbox on top/bottom
        destRect.w = static_cast<float>(windowWidth);
        destRect.h = destRect.w / targetAspect;
        destRect.x = 0;
        destRect.y = (windowHeight - destRect.h) / 2.0f;
    }

    // Render texture
    SDL_RenderTexture(m_renderer, m_texture, nullptr, &destRect);

    // Present
    SDL_RenderPresent(m_renderer);
}

SDLAudioDevice::SDLAudioDevice()
    : m_audioStream(nullptr) {
}

SDLAudioDevice::~SDLAudioDevice() {
    shutdown();
}

bool SDLAudioDevice::initialize() {
    SDL_AudioSpec spec;
    spec.freq = Config::Audio::SAMPLE_RATE;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;  // Stereo

    m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (!m_audioStream) {
        std::cerr << "Failed to open audio stream: " << SDL_GetError() << std::endl;
        return false;
    }

    // Start the audio stream
    SDL_ResumeAudioStreamDevice(m_audioStream);
    return true;
}

void SDLAudioDevice::shutdown() {
    if (m_audioStream) {
        // Pause the audio device before destroying the stream
        SDL_PauseAudioStreamDevice(m_audioStream);
        // Flush any remaining audio data
        SDL_FlushAudioStream(m_audioStream);
        // Now safely destroy the stream
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
}

void SDLAudioDevice::clearBuffer() {
    if (m_audioStream) {
        SDL_ClearAudioStream(m_audioStream);
    }
}

int SDLAudioDevice::getQueuedSize() const {
    if (m_audioStream) {
        return SDL_GetAudioStreamQueued(m_audioStream);
    }
    return 0;
}

void SDLAudioDevice::writeSamples(void* stream, u32 length) {
    if (m_audioStream) {
        SDL_PutAudioStreamData(m_audioStream, stream, length);
    }
}

Emulator::Emulator()
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_texture(nullptr)
    , m_running(false)
    , m_paused(false)
    , m_lastFrameTime(0)
    , m_emulationSpeed(1.0)
    , m_statsTimer(0)
    , m_frameCount(0) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    fs::path ext = m_romFilename.extension();

    // Create core based on ROM file extension
    if (ext == ".gb" || ext == ".gbc") {
        m_core = std::make_unique<gb::Core>();
    } else {
        std::cerr << "Unsupported ROM file extension: " << ext << std::endl;
        return false;
    }

    m_targetFPS = m_core->getTargetFPS();
    m_targetFrameTime = 1000.0 / m_targetFPS / m_gameSpeed;

    // Audio buffer thresholds: maintain 1.5-4 frames worth of audio for smooth playback
    m_minAudioBufferSize = static_cast<int>((Config::Audio::SAMPLE_RATE * 2 * sizeof(float) / static_cast<double>(m_targetFPS)) * 1.5);
    m_maxAudioBufferSize = static_cast<int>((Config::Audio::SAMPLE_RATE * 2 * sizeof(float) / static_cast<double>(m_targetFPS)) * 4.0);

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    m_window = SDL_CreateWindow(
        "GameBoy Emulator",
        m_core->getScreenWidth() * Config::Window::SCALE,
        m_core->getScreenHeight() * Config::Window::SCALE,
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

    // Disable VSync - we'll sync to audio instead for better compatibility
    SDL_SetRenderVSync(m_renderer, 0);

    // Create texture for framebuffer
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_core->getScreenWidth(),
        m_core->getScreenHeight()
    );

    if (!m_texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetTextureScaleMode(m_texture, Config::Window::SCALE_MODE);

    // Create video and audio devices
    m_videoDevice = std::make_unique<SDLVideoDevice>(m_renderer, m_texture, m_core->getScreenWidth(), m_core->getScreenHeight());
    m_audioDevice = std::make_unique<SDLAudioDevice>();

    // Initialize audio
    if (!m_audioDevice->initialize()) {
        std::cerr << "Warning: Failed to initialize audio" << std::endl;
        // Continue anyway - emulator can run without audio
    }

    // Initialize core
    if (!m_core->initialize(m_videoDevice.get(), m_audioDevice.get())) {
        std::cerr << "Failed to initialize core" << std::endl;
        return false;
    }
    m_core->setAudioSampleRate(Config::Audio::SAMPLE_RATE);
    m_core->setAudioVolume(Config::Audio::VOLUME);

    // Load bootrom if provided (optional)
    if (!m_bootromFilename.empty()) {
        std::cout << "Loading bootrom: " << m_bootromFilename << std::endl;
        m_core->loadBootrom(m_bootromFilename);
    } else {
        std::cout << "No bootrom provided, starting with post-boot state" << std::endl;
    }

    if (!m_core->loadROM(m_romFilename)) {
        std::cerr << "Failed to load ROM: " << m_romFilename << std::endl;
        return false;
    }

    std::cout << "Emulator initialized successfully" << std::endl;
    return true;
}

bool Emulator::loadBootrom(const fs::path& filename) {
    m_bootromFilename = filename;
    return true;
}

bool Emulator::loadROM(const fs::path& filename) {
    m_romFilename = filename;
    return initialize();
}

void Emulator::runFrame() {
    if (m_paused) {
        handleInput();
        updateWindowStats();
        SDL_Delay(static_cast<u32>(m_targetFrameTime));
        return;
    }

    handleInput();
    m_core->update();
    
    u64 currentTime = SDL_GetTicks();
    double frameTime = currentTime - m_lastFrameTime;
    
    // Detect if we've been paused (e.g., window dragging, debugging)
    if (frameTime > m_targetFrameTime * 3.0) {
        // Clear audio buffer to prevent audio glitches
        if (m_audioDevice) {
            m_audioDevice->clearBuffer();
        }
        m_lastFrameTime = currentTime;
        m_emulationSpeed = 1.0;
        m_statsTimer = currentTime;  // Reset stats timer
        return;
    }
    
    // Audio-driven synchronization
    if (m_audioDevice) {
        int queuedAudio = m_audioDevice->getQueuedSize();
        
        // Dynamically adjust emulation speed based on audio buffer level
        if (queuedAudio < m_minAudioBufferSize) {
            // Buffer is running low - speed up slightly to catch up
            m_emulationSpeed = 1.05;
        } else if (queuedAudio > m_maxAudioBufferSize) {
            // Buffer is too full - slow down slightly
            m_emulationSpeed = 0.95;
        } else {
            // Buffer is in good range - normalize speed gradually
            if (m_emulationSpeed > 1.0) {
                m_emulationSpeed = 1.0 + (m_emulationSpeed - 1.0) * 0.95;
            } else if (m_emulationSpeed < 1.0) {
                m_emulationSpeed = 1.0 - (1.0 - m_emulationSpeed) * 0.95;
            }
        }
    }
    
    // Calculate target frame time adjusted for emulation speed
    double adjustedFrameTime = m_targetFrameTime / m_emulationSpeed;
    
    // Frame rate limiting with adaptive timing
    if (frameTime < adjustedFrameTime) {
        double remaining = adjustedFrameTime - frameTime;
        if (remaining > 1.0) {
            // Use high-precision delay for longer waits
            SDL_Delay(static_cast<u32>(remaining));
        } else if (remaining > 0.0) {
            // // Busy-wait for sub-millisecond precision
            // while ((SDL_GetTicks() - currentTime) < adjustedFrameTime) {
            //     // Spin
            // }
        }
    }
    
    m_lastFrameTime = SDL_GetTicks();
    m_frameCount++;
    
    // Update window title with real-time stats
    updateWindowStats();
}

void Emulator::run() {
    m_running = true;
    m_paused = false;
    m_lastFrameTime = SDL_GetTicks();
    m_emulationSpeed = 1.0;
    m_statsTimer = SDL_GetTicks();
    m_frameCount = 0;
    
    while (m_running) {
        runFrame();
    }
}

void Emulator::handleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle input through core first
        if (m_core->handleInput(event)) {
            continue;
        }

        // Handle common input
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                    case Config::Key::QUIT:
                        m_running = false;
                        break;
                }
                break;

            case SDL_EVENT_KEY_UP:
                switch (event.key.key) {
                    case Config::Key::SAVE_STATE: // Save state
                        if (!m_romFilename.empty()) {
                            fs::path savePath = m_romFilename;
                            savePath.replace_extension(".state");
                            if (m_core->saveState(savePath)) {
                                std::cout << "State saved to " << savePath.string() << std::endl;
                            }
                        }
                        break;
                    case Config::Key::LOAD_STATE: // Load state
                        if (!m_romFilename.empty()) {
                            fs::path savePath = m_romFilename;
                            savePath.replace_extension(".state");
                            if (m_core->loadState(savePath)) {
                                if (m_audioDevice) {
                                    m_audioDevice->clearBuffer();
                                }
                                std::cout << "State loaded from " << savePath.string() << std::endl;
                            }
                        }
                        break;
                    case Config::Key::GAME_SPEED_UP: // Game speed up
                        updateGameSpeed(m_gameSpeed + 0.5);
                        break;
                    case Config::Key::GAME_SPEED_DOWN: // Game speed down
                        updateGameSpeed(m_gameSpeed - 0.5);
                        break;
                    case Config::Key::PAUSE: // Pause / Resume
                        m_paused = !m_paused;
                        break;
                }
                break;
        }
    }
}

void Emulator::shutdown() {
    // Close audio first to prevent segfaults
    if (m_audioDevice) {
        m_audioDevice->shutdown();
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

void Emulator::updateWindowStats() {
    u64 currentTime = SDL_GetTicks();
    u64 elapsed = currentTime - m_statsTimer;

    if (m_paused) {
        std::string title = m_core->getGameTitle() + " - " + "Paused";
        SDL_SetWindowTitle(m_window, title.c_str());
        return;
    }
    
    // Update window title every second with real-time stats
    if (elapsed >= 1000) {
        double actualFPS = (m_frameCount * 1000.0) / elapsed;
        int queuedAudio = m_audioDevice ? m_audioDevice->getQueuedSize() : 0;
        
        // Calculate buffer percentage (0-100%)
        int bufferRange = m_maxAudioBufferSize - m_minAudioBufferSize;
        int bufferPosition = queuedAudio - m_minAudioBufferSize;
        int bufferPercent = (bufferPosition * 100) / bufferRange;
        
        // Clamp to 0-100%
        if (bufferPercent < 0) bufferPercent = 0;
        if (bufferPercent > 100) bufferPercent = 100;
        
        // Build title with ROM name and stats
        std::string title = m_core->getGameTitle() + " - ";
        
        // Add stats: FPS, Speed, Audio Buffer
        std::ostringstream stats;
        stats.setf(std::ios::fixed);
        stats.precision(1);
        stats << actualFPS
              << " FPS | Speed: "
              << (m_emulationSpeed * m_gameSpeed * 100.0)
              << "% | Audio: "
              << bufferPercent
              << "%";
        
        title += stats.str();
        
        // Update window title
        SDL_SetWindowTitle(m_window, title.c_str());
        
        // Reset counters
        m_statsTimer = currentTime;
        m_frameCount = 0;
    }
}

void Emulator::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
    m_targetFrameTime = 1000.0 / m_targetFPS / m_gameSpeed;
    m_core->updateGameSpeed(gameSpeed);
    std::cout << "Game speed updated to " << m_gameSpeed << std::endl;
}
