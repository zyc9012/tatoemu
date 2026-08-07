#include "emulator.h"
#include "gb/core.h"
#include "nes/core.h"
#include "cps/core.h"
#include "cps/db.h"
#include "neogeo/core.h"
#include "neogeo/db.h"
#include "gba/core.h"
#include "md/core.h"
#include "md/cartridge.h"
#include "../utilities/zip_reader.h"
#include <SDL3/SDL.h>
#include <algorithm>

SDLVideoDevice::SDLVideoDevice(SDL_Renderer* renderer, SDL_Texture* texture, u16 screenWidth, u16 screenHeight)
    : m_renderer(renderer)
    , m_texture(texture)
    , m_screenWidth(screenWidth)
    , m_displayAspectRatio(static_cast<double>(screenWidth) / static_cast<double>(screenHeight)) {
}

SDLVideoDevice::~SDLVideoDevice() {
}

void SDLVideoDevice::render(u32* buffer) {
    SDL_UpdateTexture(m_texture, nullptr, buffer, m_screenWidth * sizeof(u32));
    present();
}

void SDLVideoDevice::present() {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // Get current window size
    int windowWidth, windowHeight;
    SDL_GetRenderOutputSize(m_renderer, &windowWidth, &windowHeight);
    
    // Calculate aspect ratio preserving destination rectangle
    // Use display aspect ratio (which accounts for non-square pixels on original hardware)
    // rather than pixel aspect ratio
    float targetAspect = static_cast<float>(m_displayAspectRatio);
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

    if (m_overlay) m_overlay(m_renderer);

    // Present
    SDL_RenderPresent(m_renderer);
}

CoreType Emulator::determineCoreType(const fs::path& romFilename) {
    fs::path ext = romFilename.extension();

    // Handle direct file extensions
    if (ext == ".gb" || ext == ".gbc") {
        return CoreType::GB;
    } else if (ext == ".nes") {
        return CoreType::NES;
    } else if (ext == ".gba") {
        return CoreType::GBA;
    } else if (ext == ".gen" || ext == ".md" || ext == ".smd") {
        return CoreType::MD;
    } else if (ext == ".bin" && md::Cartridge::fileHasHeader(romFilename)) {
        return CoreType::MD;
    } else if (ext == ".zip") {
        // Check ZIP contents for GB/GBC/NES files
        util::ZipReader zip;
        if (!zip.open(romFilename)) {
            log_error("Failed to open ZIP file for inspection: %s", romFilename.string().c_str());
            return CoreType::UNKNOWN;
        }

        std::vector<std::string> files = zip.getFileList();
        zip.close();

        // Check if ZIP contains GB/GBC/NES files
        for (const auto& filename : files) {
            fs::path filePath = filename;
            fs::path fileExt = filePath.extension();

            if (fileExt == ".gb" || fileExt == ".gbc") {
                return CoreType::GB;
            } else if (fileExt == ".nes") {
                return CoreType::NES;
            } else if (fileExt == ".gba") {
                return CoreType::GBA;
            } else if (fileExt == ".gen" || fileExt == ".md" || fileExt == ".smd") {
                return CoreType::MD;
            }
        }

        // If no GB/GBC/NES files found, fall back to CPS/NeoGeo
        std::string romSetName = romFilename.stem().string();
        std::transform(romSetName.begin(), romSetName.end(), romSetName.begin(), ::tolower);

        const cps::GameInfo* cpsGameInfo = cps::GameDatabase::findGame(romSetName);
        if (cpsGameInfo) {
            return CoreType::CPS;
        }

        const neogeo::GameInfo* neogeoGameInfo = neogeo::GameDatabase::findGame(romSetName);
        if (neogeoGameInfo) {
            return CoreType::NEOGEO;
        }

        // Last resort: a lone ".bin" entry carrying a Mega Drive header.  This
        // runs after the arcade lookups so arcade sets are never misdetected.
        for (const auto& filename : files) {
            if (fs::path(filename).extension() != ".bin") continue;

            std::vector<u8> data;
            if (zip.open(romFilename) && zip.extractFile(filename, data)) {
                zip.close();
                if (md::Cartridge::hasHeader(data.data(), data.size())) {
                    return CoreType::MD;
                }
            }
        }

        log_error("Unknown game in ZIP: %s", romSetName.c_str());
        return CoreType::UNKNOWN;
    }

    return CoreType::UNKNOWN;
}

SDLAudioDevice::SDLAudioDevice()
    : m_audioStream(nullptr) {
}

SDLAudioDevice::~SDLAudioDevice() {
    shutdown();
}

bool SDLAudioDevice::initialize() {
    SDL_AudioSpec spec;
    spec.freq = Config::Audio::SampleRate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;  // Stereo

    m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (!m_audioStream) {
        log_error("Failed to open audio stream: %s", SDL_GetError());
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
    return -1;
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
    , m_statsTimer(0)
    , m_frameCount(0) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    // Determine which core to create based on ROM file
    CoreType coreType = determineCoreType(m_romFilename);

    // Create core based on determined type
    switch (coreType) {
        case CoreType::GB:
            m_core = std::make_unique<gb::Core>();
            break;
        case CoreType::NES:
            m_core = std::make_unique<nes::Core>();
            break;
        case CoreType::CPS:
            m_core = std::make_unique<cps::Core>();
            break;
        case CoreType::NEOGEO:
            m_core = std::make_unique<neogeo::Core>();
            break;
        case CoreType::GBA:
            m_core = std::make_unique<gba::Core>();
            break;
        case CoreType::MD:
            m_core = std::make_unique<md::Core>();
            break;
        case CoreType::UNKNOWN:
        default:
            log_error("Unsupported ROM file or unknown game: %s", m_romFilename.string().c_str());
            return false;
    }

    if (!m_core) {
        log_error("Failed to create core");
        return false;
    }

    // Initialize core
    if (!m_core->initialize()) {
        log_error("Failed to initialize core");
        return false;
    }

    // Load bootrom/BIOS if provided (optional, GB and GBA)
    if (!m_bootromFilename.empty()) {
        log_info("Loading bootrom: %s", m_bootromFilename.string().c_str());
        m_core->loadBootrom(m_bootromFilename);
    } else if (coreType == CoreType::GB) {
        log_info("No bootrom provided, starting with post-boot state");
    }
    
    if (!m_core->loadROM(m_romFilename)) {
        return false;
    }

    // Get target FPS and screen dimensions
    m_targetFPS = m_core->getTargetFPS();
    u16 screenWidth = m_core->getScreenWidth();
    u16 screenHeight = m_core->getScreenHeight();
    double displayAspectRatio = m_core->getDisplayAspectRatio();
    m_targetFrameTime = 1000.0 / m_targetFPS / m_gameSpeed;

    // The queue ripples by about a frame either way, so hold enough that it neither
    // empties nor grows into audible lag. Min/max only drive the title bar gauge.
    const double audioBytesPerFrame = static_cast<double>(Config::Audio::SampleRate) * 2.0 * sizeof(float) / m_targetFPS;
    m_minAudioBufferSize = static_cast<int>(audioBytesPerFrame * 1.5);
    m_maxAudioBufferSize = static_cast<int>(audioBytesPerFrame * 4.0);
    m_audioTargetSize = static_cast<int>(audioBytesPerFrame * 2.75);
    m_pacer.setAudioTarget(m_audioTargetSize);

    // Initialize SDL
#ifdef __EMSCRIPTEN__
    // Restrict SDL keyboard capture to the canvas only, so HTML input elements work normally
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        log_error("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    // Calculate window size respecting display aspect ratio
    // Scale the height, then calculate width from aspect ratio
    int windowHeight;
    int windowWidth;

    if (Config::Window::Scale == 0) {
        SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(displayID);
        windowHeight = displayMode->h / 2;
        windowWidth = static_cast<int>(windowHeight * displayAspectRatio + 0.5);  // Round to nearest
    } else {
        windowHeight = screenHeight * Config::Window::Scale;
        windowWidth = static_cast<int>(windowHeight * displayAspectRatio + 0.5);  // Round to nearest
    }

    // Create window
    m_window = SDL_CreateWindow(
        "TatoEmu",
        windowWidth,
        windowHeight,
        SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        log_error("Failed to create window: %s", SDL_GetError());
        return false;
    }

    // Create renderer
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        log_error("Failed to create renderer: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(m_renderer, FramePacer::kVSyncInterval);

    // Create texture for framebuffer
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        screenWidth,
        screenHeight
    );

    if (!m_texture) {
        log_error("Failed to create texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(m_texture, Config::Window::ScaleMode);

    // Create video and audio devices
    m_videoDevice = std::make_unique<SDLVideoDevice>(m_renderer, m_texture, screenWidth, screenHeight);
    m_videoDevice->setOverlayCallback(m_overlayCallback);
    m_audioDevice = std::make_unique<SDLAudioDevice>();

    // Initialize audio
    if (!m_audioDevice->initialize()) {
        log_error("Warning: Failed to initialize audio");
        // Continue anyway - emulator can run without audio
    }

    // Set devices
    m_core->setVideoDevice(m_videoDevice.get());
    m_core->setAudioDevice(m_audioDevice.get());
    
    // Set display aspect ratio from core (accounts for non-square pixels on original hardware)
    m_videoDevice->setDisplayAspectRatio(m_core->getDisplayAspectRatio());
    
    m_core->setAudioSampleRate(Config::Audio::SampleRate);

    // Calculate volume
    float volume = std::powf(Config::Audio::Volume / 100.0f, 2.0f);
    m_core->setAudioVolume(volume);
    
    log_info("Emulator initialized successfully");
    return true;
}

bool Emulator::loadBootrom(const fs::path& filename) {
    m_bootromFilename = filename;
    return true;
}

void Emulator::setOverlayCallback(std::function<void(SDL_Renderer*)> draw,
                                  std::function<bool()> needsRedraw) {
    m_overlayCallback = std::move(draw);
    m_overlayNeedsRedraw = std::move(needsRedraw);
    if (m_videoDevice) m_videoDevice->setOverlayCallback(m_overlayCallback);
}

bool Emulator::loadROM(const fs::path& filename) {
    // If already initialized, shutdown first to clean up resources
    if (m_window != nullptr || m_core != nullptr) {
        shutdown();
    }
    
    m_romFilename = filename;
    if (!initialize()) return false;

    // Connect the cheat subsystem to the active core.
    ICheatMemory* cheatMem = m_core->getCheatMemory();
    m_cheatEngine.setMemory(cheatMem);
    m_searcher.setMemory(cheatMem);

    m_lastFrameTime = pacerNow();
    m_pacer.reset(m_lastFrameTime);
    m_statsTimer = SDL_GetTicks();
    m_frameCount = 0;

    return true;
}

void Emulator::runFrame() {
    if (m_paused) {
        handleInput();
        const bool overlayVisible = m_overlayNeedsRedraw && m_overlayNeedsRedraw();
        if (m_videoDevice && (overlayVisible || m_overlayWasVisible)) {
            m_videoDevice->present();
        }
        m_overlayWasVisible = overlayVisible;
        updateWindowStats();
        m_pacer.idle(pacerNow(), m_targetFrameTime);
        return;
    }

    handleInput();

    if (!m_pacer.frameIsDue(pacerNow(), m_targetFrameTime / m_pacer.emulationSpeed())) {
        return;
    }

    m_core->update();
    m_cheatEngine.applyAll();
    if (m_frameCallback) m_frameCallback();

    const double currentTime = pacerNow();
    const double frameTime = currentTime - m_lastFrameTime;

    // Detect if we've been stalled (e.g., window dragging, debugging, hidden tab)
    if (frameTime > m_targetFrameTime * 3.0) {
        // Clear audio buffer to prevent audio glitches
        if (m_audioDevice) {
            m_audioDevice->clearBuffer();
        }
        m_pacer.reset(currentTime);
        m_lastFrameTime = currentTime;
        m_statsTimer = SDL_GetTicks();
        m_frameCount = 0;
        return;
    }

    // Audio-driven synchronization
    if (m_audioDevice) {
        m_pacer.syncToAudio(m_audioDevice->getQueuedSize());
    }

    m_pacer.waitForNextFrame(m_targetFrameTime / m_pacer.emulationSpeed());

    m_lastFrameTime = pacerNow();
    m_frameCount++;

    // Update window title with real-time stats
    updateWindowStats();
}

void Emulator::run() {
    m_running = true;
    m_paused = false;
    m_lastFrameTime = pacerNow();
    m_pacer.reset(m_lastFrameTime);
    m_statsTimer = SDL_GetTicks();
    m_frameCount = 0;
    
    while (m_running) {
        runFrame();
    }
}

bool Emulator::handleKeyInput(SDL_Keycode keycode, bool pressed) {
    if (!m_core) return false;

    SDL_Event event{};
    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = keycode;
    event.key.down = pressed;
    return m_core->handleInput(event);
}

void Emulator::handleInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // The platform overlay (cheat console) gets first refusal on every event.
        if (m_eventCallback && m_eventCallback(event)) {
            continue;
        }

        // Handle input through core first
        if (m_core && m_core->handleInput(event)) {
            continue;
        }

        // Handle common input
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == Config::Key::Quit) {
                    m_running = false;
                }
                break;

            case SDL_EVENT_KEY_UP:
                if (event.key.key == Config::Key::SaveState) { // Save state
                    if (!m_romFilename.empty() && m_core) {
                        fs::path savePath = m_romFilename;
                        savePath.replace_extension(".state");
                        if (m_core->saveState(savePath)) {
                            log_info("State saved to %s", savePath.string().c_str());
                        }
                    }
                } else if (event.key.key == Config::Key::LoadState ||
                           event.key.key == Config::Key::LoadState_Backup1 ||
                           event.key.key == Config::Key::LoadState_Backup2 ||
                           event.key.key == Config::Key::LoadState_Backup3) { // Load state
                    if (!m_romFilename.empty() && m_core) {
                        fs::path savePath = m_romFilename;
                        if (event.key.key == Config::Key::LoadState) {
                            savePath.replace_extension(".state");
                        } else if (event.key.key == Config::Key::LoadState_Backup1) {
                            savePath.replace_extension(".state.bak1");
                        } else if (event.key.key == Config::Key::LoadState_Backup2) {
                            savePath.replace_extension(".state.bak2");
                        } else if (event.key.key == Config::Key::LoadState_Backup3) {
                            savePath.replace_extension(".state.bak3");
                        }
                        if (m_core->loadState(savePath)) {
                            if (m_audioDevice) {
                                m_audioDevice->clearBuffer();
                            }
                            log_info("State loaded from %s", savePath.string().c_str());
                        }
                    }
                } else if (event.key.key == Config::Key::GameSpeedUp) { // Game speed up
                    updateGameSpeed(m_gameSpeed + 0.5);
                } else if (event.key.key == Config::Key::GameSpeedDown) { // Game speed down
                    updateGameSpeed(m_gameSpeed - 0.5);
                } else if (event.key.key == Config::Key::Pause) { // Pause / Resume
                    m_paused = !m_paused;
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
        std::string title = (m_core ? m_core->getGameTitle() : "") + " - " + "Paused";
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
        std::string title = (m_core ? m_core->getGameTitle() : "") + " - ";
        
        // Add stats: FPS, Speed, Audio Buffer
        char stats[50];
        snprintf(stats, sizeof(stats), "%.1f FPS | Speed: %.1f%% | Audio: %d%%", actualFPS, m_pacer.emulationSpeed() * m_gameSpeed * 100.0, bufferPercent);
        title += stats;
        
        // Update window title
        SDL_SetWindowTitle(m_window, title.c_str());
        
        // Reset counters
        m_statsTimer = currentTime;
        m_frameCount = 0;
    }
}

void Emulator::updateGameSpeed(double gameSpeed) {
    if (gameSpeed <= 0) {
        return;
    }
    m_gameSpeed = gameSpeed;
    m_targetFrameTime = 1000.0 / m_targetFPS / m_gameSpeed;
    if (m_core) {
        m_core->updateGameSpeed(gameSpeed);
    }
    log_info("Game speed updated to %.1f", m_gameSpeed);
}
