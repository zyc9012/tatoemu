#include "emulator.h"
#include "config.h"
#include "nes/config.h"
#include "gb/config.h"
#include "gba/config.h"
#include "cps/config.h"
#include "neogeo/config.h"
#include "md/config.h"
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
    const char* getCoreType(const char* filename) {
        if (!filename) return "unknown";

        switch (Emulator::determineCoreType(filename)) {
            case CoreType::GB: return "gb";
            case CoreType::GBA: return "gba";
            case CoreType::NES: return "nes";
            case CoreType::CPS: return "cps";
            case CoreType::NEOGEO: return "neogeo";
            case CoreType::MD: return "md";
            case CoreType::UNKNOWN:
            default: return "unknown";
        }
    }

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

    int virtualKey(const char* keyName, int pressed) {
        if (!g_emulatorWasm || !g_emulatorWasm->romLoaded || !keyName) return 0;

        SDL_Keycode keycode = SDL_GetKeyFromName(keyName);
        if (keycode == SDLK_UNKNOWN) {
            log_error("Unknown virtual key name: %s", keyName);
            return 0;
        }

        return g_emulatorWasm->emulator.handleKeyInput(keycode, pressed != 0) ? 1 : 0;
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

    // ── Cheat engine ─────────────────────────────────────────────────────────

    void cheatApply(uint32_t address, uint32_t value, int width) {
        if (!g_emulatorWasm || !g_emulatorWasm->romLoaded) return;
        CheatCode code;
        code.name    = "one-shot";
        code.address = address;
        code.value   = value;
        code.width   = static_cast<u8>(width);
        code.enabled = true;
        g_emulatorWasm->emulator.getCheatEngine().apply(code);
    }

    void cheatAddCode(const char* name, uint32_t address, uint32_t value, int width) {
        if (!g_emulatorWasm || !g_emulatorWasm->romLoaded) return;
        CheatCode code;
        code.name    = name ? name : "";
        code.address = address;
        code.value   = value;
        code.width   = static_cast<u8>(width);
        code.enabled = true;
        g_emulatorWasm->emulator.getCheatEngine().addCode(code);
    }

    void cheatRemoveCode(int index) {
        if (!g_emulatorWasm) return;
        g_emulatorWasm->emulator.getCheatEngine().removeCode(static_cast<size_t>(index));
    }

    void cheatToggleCode(int index) {
        if (!g_emulatorWasm) return;
        g_emulatorWasm->emulator.getCheatEngine().toggleCode(static_cast<size_t>(index));
    }

    void cheatClearCodes() {
        if (!g_emulatorWasm) return;
        g_emulatorWasm->emulator.getCheatEngine().clearCodes();
    }

    static char s_jsonBuf[65536];

    static char* jsonWriteString(char* p, const char* end, const char* s) {
        *p++ = '"';
        for (; *s && p < end - 1; ++s) {
            if (*s == '"' || *s == '\\') *p++ = '\\';
            *p++ = *s;
        }
        *p++ = '"';
        return p;
    }

    const char* cheatGetCodesJson() {
        char* p   = s_jsonBuf;
        char* end = s_jsonBuf + sizeof(s_jsonBuf) - 4;
        *p++ = '[';
        if (g_emulatorWasm) {
            const auto& codes = g_emulatorWasm->emulator.getCheatEngine().getCodes();
            for (size_t i = 0; i < codes.size() && p < end; ++i) {
                if (i > 0) *p++ = ',';
                const auto& c = codes[i];
                p += snprintf(p, end - p, "{\"name\":");
                p  = jsonWriteString(p, end, c.name.c_str());
                p += snprintf(p, end - p,
                    ",\"address\":%u,\"value\":%u,\"width\":%d,\"enabled\":%s}",
                    c.address, c.value, static_cast<int>(c.width),
                    c.enabled ? "true" : "false");
            }
        }
        *p++ = ']'; *p = '\0';
        return s_jsonBuf;
    }

    // ── Memory searcher ───────────────────────────────────────────────────────

    void searchReset(int width) {
        if (!g_emulatorWasm || !g_emulatorWasm->romLoaded) return;
        MemSearcher::Width w = MemSearcher::Width::U16;
        if (width == 1) w = MemSearcher::Width::U8;
        else if (width == 4) w = MemSearcher::Width::U32;
        auto& searcher = g_emulatorWasm->emulator.getSearcher();
        searcher.setWidth(w);
        searcher.reset();
    }

    void searchFilter(const char* filterStr, uint32_t value) {
        if (!g_emulatorWasm || !g_emulatorWasm->romLoaded) return;
        auto& searcher = g_emulatorWasm->emulator.getSearcher();
        if (!searcher.isInitialized()) return;
        std::string f(filterStr);
        MemSearcher::Filter filter;
        if      (f == "eq")        filter = MemSearcher::Filter::Equal;
        else if (f == "ne")        filter = MemSearcher::Filter::NotEqual;
        else if (f == "gt")        filter = MemSearcher::Filter::Greater;
        else if (f == "lt")        filter = MemSearcher::Filter::Less;
        else if (f == "changed")   filter = MemSearcher::Filter::Changed;
        else if (f == "unchanged") filter = MemSearcher::Filter::Unchanged;
        else return;
        searcher.filter(filter, value);
    }

    int searchCandidateCount() {
        if (!g_emulatorWasm) return 0;
        return static_cast<int>(g_emulatorWasm->emulator.getSearcher().candidateCount());
    }

    const char* searchGetCandidatesJson(int maxResults) {
        char* p   = s_jsonBuf;
        char* end = s_jsonBuf + sizeof(s_jsonBuf) - 4;
        *p++ = '[';
        if (g_emulatorWasm) {
            const auto& searcher = g_emulatorWasm->emulator.getSearcher();
            const auto& cands    = searcher.candidates();
            int count = 0;
            for (size_t i = 0; i < cands.size() && p < end && count < maxResults; ++i, ++count) {
                if (i > 0) *p++ = ',';
                u32 addr = cands[i];
                u32 val  = searcher.readCurrent(addr);
                p += snprintf(p, end - p, "{\"address\":%u,\"value\":%u}", addr, val);
            }
        }
        *p++ = ']'; *p = '\0';
        return s_jsonBuf;
    }

    // ── Key bindings ──────────────────────────────────────────────────────────

    void setKeyBinding(const char* section, const char* key, const char* value) {
        SDL_Keycode kc = SDL_GetKeyFromName(value);
        if (kc == SDLK_UNKNOWN) {
            log_error("Unknown key name: %s", value);
            return;
        }

        std::string sec(section);
        std::string k(key);

        if (sec == "Common") {
            if (k == "Quit") Config::Key::Quit = kc;
            else if (k == "SaveState") Config::Key::SaveState = kc;
            else if (k == "LoadState") Config::Key::LoadState = kc;
            else if (k == "LoadState_Backup1") Config::Key::LoadState_Backup1 = kc;
            else if (k == "LoadState_Backup2") Config::Key::LoadState_Backup2 = kc;
            else if (k == "LoadState_Backup3") Config::Key::LoadState_Backup3 = kc;
            else if (k == "Pause") Config::Key::Pause = kc;
            else if (k == "SpeedUp") Config::Key::GameSpeedUp = kc;
            else if (k == "SpeedDown") Config::Key::GameSpeedDown = kc;
        } else if (sec == "GB") {
            if (k == "A") gb::Config::Key::ButtonA = kc;
            else if (k == "B") gb::Config::Key::ButtonB = kc;
            else if (k == "Start") gb::Config::Key::Start = kc;
            else if (k == "Select") gb::Config::Key::Select = kc;
            else if (k == "Up") gb::Config::Key::DpadUp = kc;
            else if (k == "Down") gb::Config::Key::DpadDown = kc;
            else if (k == "Left") gb::Config::Key::DpadLeft = kc;
            else if (k == "Right") gb::Config::Key::DpadRight = kc;
        } else if (sec == "GBA") {
            if (k == "A") gba::Config::Key::ButtonA = kc;
            else if (k == "B") gba::Config::Key::ButtonB = kc;
            else if (k == "L") gba::Config::Key::ButtonL = kc;
            else if (k == "R") gba::Config::Key::ButtonR = kc;
            else if (k == "Start") gba::Config::Key::Start = kc;
            else if (k == "Select") gba::Config::Key::Select = kc;
            else if (k == "Up") gba::Config::Key::DpadUp = kc;
            else if (k == "Down") gba::Config::Key::DpadDown = kc;
            else if (k == "Left") gba::Config::Key::DpadLeft = kc;
            else if (k == "Right") gba::Config::Key::DpadRight = kc;
        } else if (sec == "NES") {
            if (k == "A") nes::Config::Key::ButtonA = kc;
            else if (k == "B") nes::Config::Key::ButtonB = kc;
            else if (k == "Start") nes::Config::Key::Start = kc;
            else if (k == "Select") nes::Config::Key::Select = kc;
            else if (k == "Up") nes::Config::Key::DpadUp = kc;
            else if (k == "Down") nes::Config::Key::DpadDown = kc;
            else if (k == "Left") nes::Config::Key::DpadLeft = kc;
            else if (k == "Right") nes::Config::Key::DpadRight = kc;
        } else if (sec == "CPS") {
            if (k == "P1_Up") cps::Config::Key::P1_Up = kc;
            else if (k == "P1_Down") cps::Config::Key::P1_Down = kc;
            else if (k == "P1_Left") cps::Config::Key::P1_Left = kc;
            else if (k == "P1_Right") cps::Config::Key::P1_Right = kc;
            else if (k == "P1_Punch1") cps::Config::Key::P1_Punch1 = kc;
            else if (k == "P1_Punch2") cps::Config::Key::P1_Punch2 = kc;
            else if (k == "P1_Punch3") cps::Config::Key::P1_Punch3 = kc;
            else if (k == "P1_Kick1") cps::Config::Key::P1_Kick1 = kc;
            else if (k == "P1_Kick2") cps::Config::Key::P1_Kick2 = kc;
            else if (k == "P1_Kick3") cps::Config::Key::P1_Kick3 = kc;
            else if (k == "P2_Up") cps::Config::Key::P2_Up = kc;
            else if (k == "P2_Down") cps::Config::Key::P2_Down = kc;
            else if (k == "P2_Left") cps::Config::Key::P2_Left = kc;
            else if (k == "P2_Right") cps::Config::Key::P2_Right = kc;
            else if (k == "P2_Punch1") cps::Config::Key::P2_Punch1 = kc;
            else if (k == "P2_Punch2") cps::Config::Key::P2_Punch2 = kc;
            else if (k == "P2_Punch3") cps::Config::Key::P2_Punch3 = kc;
            else if (k == "P2_Kick1") cps::Config::Key::P2_Kick1 = kc;
            else if (k == "P2_Kick2") cps::Config::Key::P2_Kick2 = kc;
            else if (k == "P2_Kick3") cps::Config::Key::P2_Kick3 = kc;
            else if (k == "P1_Coin") cps::Config::Key::P1_Coin = kc;
            else if (k == "P2_Coin") cps::Config::Key::P2_Coin = kc;
            else if (k == "P1_Start") cps::Config::Key::P1_Start = kc;
            else if (k == "P2_Start") cps::Config::Key::P2_Start = kc;
            else if (k == "Diag") cps::Config::Key::Diag = kc;
            else if (k == "Service") cps::Config::Key::Service = kc;
        } else if (sec == "NeoGeo") {
            if (k == "P1_Up") neogeo::Config::Key::P1_Up = kc;
            else if (k == "P1_Down") neogeo::Config::Key::P1_Down = kc;
            else if (k == "P1_Left") neogeo::Config::Key::P1_Left = kc;
            else if (k == "P1_Right") neogeo::Config::Key::P1_Right = kc;
            else if (k == "P1_A") neogeo::Config::Key::P1_ButtonA = kc;
            else if (k == "P1_B") neogeo::Config::Key::P1_ButtonB = kc;
            else if (k == "P1_C") neogeo::Config::Key::P1_ButtonC = kc;
            else if (k == "P1_D") neogeo::Config::Key::P1_ButtonD = kc;
            else if (k == "P2_Up") neogeo::Config::Key::P2_Up = kc;
            else if (k == "P2_Down") neogeo::Config::Key::P2_Down = kc;
            else if (k == "P2_Left") neogeo::Config::Key::P2_Left = kc;
            else if (k == "P2_Right") neogeo::Config::Key::P2_Right = kc;
            else if (k == "P2_A") neogeo::Config::Key::P2_ButtonA = kc;
            else if (k == "P2_B") neogeo::Config::Key::P2_ButtonB = kc;
            else if (k == "P2_C") neogeo::Config::Key::P2_ButtonC = kc;
            else if (k == "P2_D") neogeo::Config::Key::P2_ButtonD = kc;
            else if (k == "P1_Coin") neogeo::Config::Key::P1_Coin = kc;
            else if (k == "P2_Coin") neogeo::Config::Key::P2_Coin = kc;
            else if (k == "P1_Start") neogeo::Config::Key::P1_Start = kc;
            else if (k == "P2_Start") neogeo::Config::Key::P2_Start = kc;
            else if (k == "P1_Select") neogeo::Config::Key::P1_Select = kc;
            else if (k == "P2_Select") neogeo::Config::Key::P2_Select = kc;
            else if (k == "Test") neogeo::Config::Key::Test = kc;
            else if (k == "Service") neogeo::Config::Key::Service = kc;
        } else if (sec == "MD") {
            if (k == "Up") md::Config::Key::DpadUp = kc;
            else if (k == "Down") md::Config::Key::DpadDown = kc;
            else if (k == "Left") md::Config::Key::DpadLeft = kc;
            else if (k == "Right") md::Config::Key::DpadRight = kc;
            else if (k == "A") md::Config::Key::ButtonA = kc;
            else if (k == "B") md::Config::Key::ButtonB = kc;
            else if (k == "C") md::Config::Key::ButtonC = kc;
            else if (k == "X") md::Config::Key::ButtonX = kc;
            else if (k == "Y") md::Config::Key::ButtonY = kc;
            else if (k == "Z") md::Config::Key::ButtonZ = kc;
            else if (k == "Start") md::Config::Key::Start = kc;
            else if (k == "Mode") md::Config::Key::Mode = kc;
        }
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
