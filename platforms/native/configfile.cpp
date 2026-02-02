#include "configfile.h"
#include "emulator.h"
#include "config.h"
#include "nes/config.h"
#include "gb/config.h"
#include "cps/config.h"
#include "neogeo/config.h"
#include "inih/ini.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

static std::string_view sv_or_empty(const char* s) {
    return s ? std::string_view(s) : std::string_view();
}

static std::string_view trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

static bool parse_u32(std::string_view s, u32& out) {
    s = trim(s);
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        unsigned long v = std::stoul(std::string(s), &idx, 0);
        if (idx != s.size()) return false;
        out = static_cast<u32>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parse_float(std::string_view s, float& out) {
    s = trim(s);
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        float v = std::stof(std::string(s), &idx);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

static bool parse_scale_mode(std::string_view s, SDL_ScaleMode& out) {
    s = trim(s);
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "nearest") {
        out = SDL_SCALEMODE_NEAREST;
        return true;
    }
    if (lower == "linear") {
        out = SDL_SCALEMODE_LINEAR;
        return true;
    }
    return false;
}

static bool parse_keycode(std::string_view s, SDL_Keycode& out) {
    s = trim(s);
    SDL_Keycode k = SDL_GetKeyFromName(std::string(s).c_str());
    if (k != SDLK_UNKNOWN) {
        out = k;
        return true;
    }
    return false;
}

static const char* scale_mode_to_string(SDL_ScaleMode mode) {
    return (mode == SDL_SCALEMODE_LINEAR) ? "linear" : "nearest";
}

static const char* neogeo_system_to_string(neogeo::SystemType sys) {
    return (sys == neogeo::SystemType::AES) ? "aes" : "mvs";
}

static void createDefaultIni(const fs::path& iniPath) {
    FILE* out = fopen(iniPath.string().c_str(), "w");
    if (!out) {
        log_error("Warning: Failed to create default config file: %s", iniPath.string().c_str());
        return;
    }
    fprintf(out, "; TatoEmu configuration file\n");
    fprintf(out, "\n");
    fprintf(out, "[Common]\n");
    fprintf(out, "WindowScale=%u ; (0=auto, 1=1x, 2=2x, ...)\n", Config::Window::Scale);
    fprintf(out, "WindowScaleMode=%s ; (nearest, linear)\n", scale_mode_to_string(Config::Window::ScaleMode));
    fprintf(out, "SampleRate=%u\n", Config::Audio::SampleRate);
    fprintf(out, "Volume=%g ; (0.0-1.0)\n", static_cast<double>(Config::Audio::Volume));
    fprintf(out, "Quit=%s\n", SDL_GetKeyName(Config::Key::Quit));
    fprintf(out, "SaveState=%s\n", SDL_GetKeyName(Config::Key::SaveState));
    fprintf(out, "LoadState=%s\n", SDL_GetKeyName(Config::Key::LoadState));
    fprintf(out, "Pause=%s\n", SDL_GetKeyName(Config::Key::Pause));
    fprintf(out, "SpeedUp=%s\n", SDL_GetKeyName(Config::Key::GameSpeedUp));
    fprintf(out, "SpeedDown=%s\n", SDL_GetKeyName(Config::Key::GameSpeedDown));
    fprintf(out, "\n");
    fprintf(out, "[NES]\n");
    fprintf(out, "A=%s\n", SDL_GetKeyName(nes::Config::Key::ButtonA));
    fprintf(out, "B=%s\n", SDL_GetKeyName(nes::Config::Key::ButtonB));
    fprintf(out, "Start=%s\n", SDL_GetKeyName(nes::Config::Key::Start));
    fprintf(out, "SelectPrimary=%s\n", SDL_GetKeyName(nes::Config::Key::SelectPrimary));
    fprintf(out, "SelectSecondary=%s\n", SDL_GetKeyName(nes::Config::Key::SelectSecondary));
    fprintf(out, "Up=%s\n", SDL_GetKeyName(nes::Config::Key::DpadUp));
    fprintf(out, "Down=%s\n", SDL_GetKeyName(nes::Config::Key::DpadDown));
    fprintf(out, "Left=%s\n", SDL_GetKeyName(nes::Config::Key::DpadLeft));
    fprintf(out, "Right=%s\n", SDL_GetKeyName(nes::Config::Key::DpadRight));
    fprintf(out, "\n");
    fprintf(out, "[GB]\n");
    fprintf(out, "A=%s\n", SDL_GetKeyName(gb::Config::Key::ButtonA));
    fprintf(out, "B=%s\n", SDL_GetKeyName(gb::Config::Key::ButtonB));
    fprintf(out, "Start=%s\n", SDL_GetKeyName(gb::Config::Key::Start));
    fprintf(out, "SelectPrimary=%s\n", SDL_GetKeyName(gb::Config::Key::SelectPrimary));
    fprintf(out, "SelectSecondary=%s\n", SDL_GetKeyName(gb::Config::Key::SelectSecondary));
    fprintf(out, "Up=%s\n", SDL_GetKeyName(gb::Config::Key::DpadUp));
    fprintf(out, "Down=%s\n", SDL_GetKeyName(gb::Config::Key::DpadDown));
    fprintf(out, "Left=%s\n", SDL_GetKeyName(gb::Config::Key::DpadLeft));
    fprintf(out, "Right=%s\n", SDL_GetKeyName(gb::Config::Key::DpadRight));
    fprintf(out, "\n");
    fprintf(out, "[CPS]\n");
    fprintf(out, "P1_Up=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Up));
    fprintf(out, "P1_Down=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Down));
    fprintf(out, "P1_Left=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Left));
    fprintf(out, "P1_Right=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Right));
    fprintf(out, "P1_Punch1=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Punch1));
    fprintf(out, "P1_Punch2=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Punch2));
    fprintf(out, "P1_Punch3=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Punch3));
    fprintf(out, "P1_Kick1=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Kick1));
    fprintf(out, "P1_Kick2=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Kick2));
    fprintf(out, "P1_Kick3=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Kick3));
    fprintf(out, "P2_Up=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Up));
    fprintf(out, "P2_Down=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Down));
    fprintf(out, "P2_Left=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Left));
    fprintf(out, "P2_Right=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Right));
    fprintf(out, "P2_Punch1=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Punch1));
    fprintf(out, "P2_Punch2=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Punch2));
    fprintf(out, "P2_Punch3=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Punch3));
    fprintf(out, "P2_Kick1=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Kick1));
    fprintf(out, "P2_Kick2=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Kick2));
    fprintf(out, "P2_Kick3=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Kick3));
    fprintf(out, "P1_Coin=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Coin));
    fprintf(out, "P2_Coin=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Coin));
    fprintf(out, "P1_Start=%s\n", SDL_GetKeyName(cps::Config::Key::P1_Start));
    fprintf(out, "P2_Start=%s\n", SDL_GetKeyName(cps::Config::Key::P2_Start));
    fprintf(out, "Diag=%s\n", SDL_GetKeyName(cps::Config::Key::Diag));
    fprintf(out, "Service=%s\n", SDL_GetKeyName(cps::Config::Key::Service));
    fprintf(out, "\n");
    fprintf(out, "[NeoGeo]\n");
    fprintf(out, "System=%s ; (aes, mvs)\n", neogeo_system_to_string(neogeo::Config::System));
    fprintf(out, "BiosIndex=%d ; (0-34)\n", static_cast<int>(neogeo::Config::BiosIndex));
    fprintf(out, "P1_Up=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Up));
    fprintf(out, "P1_Down=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Down));
    fprintf(out, "P1_Left=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Left));
    fprintf(out, "P1_Right=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Right));
    fprintf(out, "P1_A=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_ButtonA));
    fprintf(out, "P1_B=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_ButtonB));
    fprintf(out, "P1_C=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_ButtonC));
    fprintf(out, "P1_D=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_ButtonD));
    fprintf(out, "P2_Up=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Up));
    fprintf(out, "P2_Down=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Down));
    fprintf(out, "P2_Left=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Left));
    fprintf(out, "P2_Right=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Right));
    fprintf(out, "P2_A=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_ButtonA));
    fprintf(out, "P2_B=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_ButtonB));
    fprintf(out, "P2_C=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_ButtonC));
    fprintf(out, "P2_D=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_ButtonD));
    fprintf(out, "P1_Coin=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Coin));
    fprintf(out, "P2_Coin=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Coin));
    fprintf(out, "P1_Start=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Start));
    fprintf(out, "P2_Start=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Start));
    fprintf(out, "P1_Select=%s\n", SDL_GetKeyName(neogeo::Config::Key::P1_Select));
    fprintf(out, "P2_Select=%s\n", SDL_GetKeyName(neogeo::Config::Key::P2_Select));
    fprintf(out, "Test=%s\n", SDL_GetKeyName(neogeo::Config::Key::Test));
    fprintf(out, "Service=%s\n", SDL_GetKeyName(neogeo::Config::Key::Service));
    fclose(out);
}

static int configHandler(void* /*user*/, const char* section, const char* name, const char* value) {
    const std::string_view sec = trim(sv_or_empty(section));
    const std::string_view key = trim(sv_or_empty(name));
    const std::string_view val = trim(sv_or_empty(value));

    auto set_u32 = [&](u32& target) -> bool {
        u32 v = 0;
        if (!parse_u32(val, v)) return false;
        target = v;
        return true;
    };

    auto set_float = [&](float& target, float minv, float maxv) -> bool {
        float v = 0.0f;
        if (!parse_float(val, v)) return false;
        if (v < minv) v = minv;
        if (v > maxv) v = maxv;
        target = v;
        return true;
    };

    auto set_key = [&](SDL_Keycode& target) -> bool {
        SDL_Keycode k = SDLK_UNKNOWN;
        if (!parse_keycode(val, k)) return false;
        target = k;
        return true;
    };

    auto set_neogeo_sys = [&]() -> bool {
        std::string lower = std::string(val);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "aes") {
            neogeo::Config::System = neogeo::SystemType::AES;
            return true;
        }
        if (lower == "mvs") {
            neogeo::Config::System = neogeo::SystemType::MVS;
            return true;
        }
        return false;
    };

    auto set_neogeo_bios = [&]() -> bool {
        u32 v = 0;
        if (!parse_u32(val, v)) return false;
        if (v > 34) return false;
        neogeo::Config::BiosIndex = static_cast<u8>(v);
        return true;
    };

    if (sec == "Common") {
        if (key == "WindowScale") set_u32(Config::Window::Scale);
        else if (key == "WindowScaleMode") parse_scale_mode(val, Config::Window::ScaleMode);
        else if (key == "SampleRate") set_u32(Config::Audio::SampleRate);
        else if (key == "Volume") set_float(Config::Audio::Volume, 0.0f, 1.0f);
        else if (key == "Quit") set_key(Config::Key::Quit);
        else if (key == "SaveState") set_key(Config::Key::SaveState);
        else if (key == "LoadState") set_key(Config::Key::LoadState);
        else if (key == "Pause") set_key(Config::Key::Pause);
        else if (key == "SpeedUp") set_key(Config::Key::GameSpeedUp);
        else if (key == "SpeedDown") set_key(Config::Key::GameSpeedDown);
    } else if (sec == "NES") {
        if (key == "A") set_key(nes::Config::Key::ButtonA);
        else if (key == "B") set_key(nes::Config::Key::ButtonB);
        else if (key == "Start") set_key(nes::Config::Key::Start);
        else if (key == "SelectPrimary") set_key(nes::Config::Key::SelectPrimary);
        else if (key == "SelectSecondary") set_key(nes::Config::Key::SelectSecondary);
        else if (key == "Up") set_key(nes::Config::Key::DpadUp);
        else if (key == "Down") set_key(nes::Config::Key::DpadDown);
        else if (key == "Left") set_key(nes::Config::Key::DpadLeft);
        else if (key == "Right") set_key(nes::Config::Key::DpadRight);
    } else if (sec == "GB") {
        if (key == "A") set_key(gb::Config::Key::ButtonA);
        else if (key == "B") set_key(gb::Config::Key::ButtonB);
        else if (key == "Start") set_key(gb::Config::Key::Start);
        else if (key == "SelectPrimary") set_key(gb::Config::Key::SelectPrimary);
        else if (key == "SelectSecondary") set_key(gb::Config::Key::SelectSecondary);
        else if (key == "Up") set_key(gb::Config::Key::DpadUp);
        else if (key == "Down") set_key(gb::Config::Key::DpadDown);
        else if (key == "Left") set_key(gb::Config::Key::DpadLeft);
        else if (key == "Right") set_key(gb::Config::Key::DpadRight);
    } else if (sec == "CPS") {
        if (key == "P1_Up") set_key(cps::Config::Key::P1_Up);
        else if (key == "P1_Down") set_key(cps::Config::Key::P1_Down);
        else if (key == "P1_Left") set_key(cps::Config::Key::P1_Left);
        else if (key == "P1_Right") set_key(cps::Config::Key::P1_Right);
        else if (key == "P1_Punch1") set_key(cps::Config::Key::P1_Punch1);
        else if (key == "P1_Punch2") set_key(cps::Config::Key::P1_Punch2);
        else if (key == "P1_Punch3") set_key(cps::Config::Key::P1_Punch3);
        else if (key == "P1_Kick1") set_key(cps::Config::Key::P1_Kick1);
        else if (key == "P1_Kick2") set_key(cps::Config::Key::P1_Kick2);
        else if (key == "P1_Kick3") set_key(cps::Config::Key::P1_Kick3);
        else if (key == "P2_Up") set_key(cps::Config::Key::P2_Up);
        else if (key == "P2_Down") set_key(cps::Config::Key::P2_Down);
        else if (key == "P2_Left") set_key(cps::Config::Key::P2_Left);
        else if (key == "P2_Right") set_key(cps::Config::Key::P2_Right);
        else if (key == "P2_Punch1") set_key(cps::Config::Key::P2_Punch1);
        else if (key == "P2_Punch2") set_key(cps::Config::Key::P2_Punch2);
        else if (key == "P2_Punch3") set_key(cps::Config::Key::P2_Punch3);
        else if (key == "P2_Kick1") set_key(cps::Config::Key::P2_Kick1);
        else if (key == "P2_Kick2") set_key(cps::Config::Key::P2_Kick2);
        else if (key == "P2_Kick3") set_key(cps::Config::Key::P2_Kick3);
        else if (key == "P1_Coin") set_key(cps::Config::Key::P1_Coin);
        else if (key == "P2_Coin") set_key(cps::Config::Key::P2_Coin);
        else if (key == "P1_Start") set_key(cps::Config::Key::P1_Start);
        else if (key == "P2_Start") set_key(cps::Config::Key::P2_Start);
        else if (key == "Diag") set_key(cps::Config::Key::Diag);
        else if (key == "Service") set_key(cps::Config::Key::Service);
    } else if (sec == "NeoGeo") {
        if (key == "System") set_neogeo_sys();
        else if (key == "BiosIndex") set_neogeo_bios();
        else if (key == "P1_Up") set_key(neogeo::Config::Key::P1_Up);
        else if (key == "P1_Down") set_key(neogeo::Config::Key::P1_Down);
        else if (key == "P1_Left") set_key(neogeo::Config::Key::P1_Left);
        else if (key == "P1_Right") set_key(neogeo::Config::Key::P1_Right);
        else if (key == "P1_A") set_key(neogeo::Config::Key::P1_ButtonA);
        else if (key == "P1_B") set_key(neogeo::Config::Key::P1_ButtonB);
        else if (key == "P1_C") set_key(neogeo::Config::Key::P1_ButtonC);
        else if (key == "P1_D") set_key(neogeo::Config::Key::P1_ButtonD);
        else if (key == "P2_Up") set_key(neogeo::Config::Key::P2_Up);
        else if (key == "P2_Down") set_key(neogeo::Config::Key::P2_Down);
        else if (key == "P2_Left") set_key(neogeo::Config::Key::P2_Left);
        else if (key == "P2_Right") set_key(neogeo::Config::Key::P2_Right);
        else if (key == "P2_A") set_key(neogeo::Config::Key::P2_ButtonA);
        else if (key == "P2_B") set_key(neogeo::Config::Key::P2_ButtonB);
        else if (key == "P2_C") set_key(neogeo::Config::Key::P2_ButtonC);
        else if (key == "P2_D") set_key(neogeo::Config::Key::P2_ButtonD);
        else if (key == "P1_Coin") set_key(neogeo::Config::Key::P1_Coin);
        else if (key == "P2_Coin") set_key(neogeo::Config::Key::P2_Coin);
        else if (key == "P1_Start") set_key(neogeo::Config::Key::P1_Start);
        else if (key == "P2_Start") set_key(neogeo::Config::Key::P2_Start);
        else if (key == "P1_Select") set_key(neogeo::Config::Key::P1_Select);
        else if (key == "P2_Select") set_key(neogeo::Config::Key::P2_Select);
        else if (key == "Test") set_key(neogeo::Config::Key::Test);
        else if (key == "Service") set_key(neogeo::Config::Key::Service);
    }

    return 1;
}

void loadConfigFile(const fs::path& exePath) {
    const fs::path iniPath = exePath.parent_path() / "tatoemu.ini";
    if (!fs::exists(iniPath)) {
        createDefaultIni(iniPath);
    }
    if (fs::exists(iniPath)) {
        const int parseResult = ini_parse(iniPath.string().c_str(), configHandler, nullptr);
        if (parseResult != 0) {
            log_error("Warning: Failed to parse %s (line %d)", iniPath.string().c_str(), parseResult);
        }
    }
}
