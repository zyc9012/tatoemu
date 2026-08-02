#include "cheat_console.h"
#include "config.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ─── layout ──────────────────────────────────────────────────────────────────

namespace {

constexpr float kGlyph      = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;  // 8x8
constexpr float kLineHeight = kGlyph + 2.0f;
constexpr float kPadding    = 6.0f;
constexpr int   kTargetCols = 80;
constexpr int   kCandidateCols = 22;    // width of the live candidate pane
constexpr size_t kMaxScrollback = 512;

// Console occupies this fraction of the window height.
constexpr float kHeightFraction = 0.55f;

} // namespace

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    const char* p = line.c_str();
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        tokens.emplace_back(start, p);
    }
    return tokens;
}

static bool parseValue(const std::string& s, u32& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const char* p = s.c_str();
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        p += 2;
        base = 16;
    }
    unsigned long v = strtoul(p, &end, base);
    if (end == p || *end != '\0') return false;
    out = static_cast<u32>(v);
    return true;
}

static u8 parseWidth(const std::string& s, u8 fallback = 2) {
    if (s == "1" || s == "u8")  return 1;
    if (s == "2" || s == "u16") return 2;
    if (s == "4" || s == "u32") return 4;
    return fallback;
}

// ─── CheatConsole ─────────────────────────────────────────────────────────────

CheatConsole::CheatConsole(CheatEngine& engine, MemSearcher& searcher)
    : m_engine(engine)
    , m_searcher(searcher) {
    pushLine("Cheat console. Type 'help' for commands.");
}

void CheatConsole::pushLine(std::string line) {
    m_lines.push_back(std::move(line));
    while (m_lines.size() > kMaxScrollback) m_lines.pop_front();
    m_scroll = 0;
}

void CheatConsole::print(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    pushLine(buf);
}

void CheatConsole::setOpen(bool open, SDL_WindowID windowID) {
    if (m_open == open) return;
    m_open = open;

    if (open && m_releaseKey) {
        int numKeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numKeys);
        for (int sc = 0; sc < numKeys; ++sc) {
            if (!keys[sc]) continue;
            SDL_Keycode key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(sc),
                                                     SDL_KMOD_NONE, false);
            if (key != SDLK_UNKNOWN) m_releaseKey(key);
        }
    }

    SDL_Window* window = SDL_GetWindowFromID(windowID);
    if (!window) return;
    if (open) SDL_StartTextInput(window);
    else      SDL_StopTextInput(window);
}

// ─── input ───────────────────────────────────────────────────────────────────

bool CheatConsole::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == Config::Key::CheatConsole) {
        setOpen(!m_open, event.key.windowID);
        return true;
    }

    if (!m_open) return false;

    switch (event.type) {
        case SDL_EVENT_TEXT_INPUT:
            m_input.insert(m_cursor, event.text.text);
            m_cursor += strlen(event.text.text);
            return true;

        case SDL_EVENT_MOUSE_WHEEL:
            m_scroll = std::max(0, m_scroll + static_cast<int>(event.wheel.y));
            return true;

        case SDL_EVENT_KEY_DOWN:
            break;

        // Swallow every other key event so held buttons cannot leak to the game.
        case SDL_EVENT_KEY_UP:
            return true;

        default:
            return false;
    }

    const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;

    switch (event.key.key) {
        case SDLK_ESCAPE:
            setOpen(false, event.key.windowID);
            break;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            submit();
            break;

        case SDLK_BACKSPACE:
            if (m_cursor > 0) {
                m_input.erase(m_cursor - 1, 1);
                --m_cursor;
            }
            break;

        case SDLK_DELETE:
            if (m_cursor < m_input.size()) m_input.erase(m_cursor, 1);
            break;

        case SDLK_LEFT:
            if (m_cursor > 0) --m_cursor;
            break;

        case SDLK_RIGHT:
            if (m_cursor < m_input.size()) ++m_cursor;
            break;

        case SDLK_HOME:
            m_cursor = 0;
            break;

        case SDLK_END:
            m_cursor = m_input.size();
            break;

        case SDLK_UP:
            recallHistory(1);
            break;

        case SDLK_DOWN:
            recallHistory(-1);
            break;

        case SDLK_PAGEUP:
            m_scroll += 5;
            break;

        case SDLK_PAGEDOWN:
            m_scroll = std::max(0, m_scroll - 5);
            break;

        case SDLK_U:
            if (ctrl) {
                m_input.erase(0, m_cursor);
                m_cursor = 0;
            }
            break;

        case SDLK_V:
            if (ctrl && SDL_HasClipboardText()) {
                char* text = SDL_GetClipboardText();
                if (text) {
                    m_input.insert(m_cursor, text);
                    m_cursor += strlen(text);
                    SDL_free(text);
                }
            }
            break;

        default:
            break;
    }
    return true;
}

void CheatConsole::submit() {
    std::string line = m_input;
    m_input.clear();
    m_cursor = 0;
    m_historyPos = -1;
    m_stashedInput.clear();

    if (line.empty()) return;
    if (m_history.empty() || m_history.back() != line) m_history.push_back(line);

    print("> %s", line.c_str());
    dispatch(line);
}

void CheatConsole::recallHistory(int delta) {
    if (m_history.empty()) return;

    if (m_historyPos < 0 && delta > 0) m_stashedInput = m_input;

    int pos = std::clamp(m_historyPos + delta, -1, static_cast<int>(m_history.size()) - 1);
    if (pos == m_historyPos) return;
    m_historyPos = pos;

    m_input  = (pos < 0) ? m_stashedInput
                         : m_history[m_history.size() - 1 - static_cast<size_t>(pos)];
    m_cursor = m_input.size();
}

// ─── rendering ───────────────────────────────────────────────────────────────

void CheatConsole::render(SDL_Renderer* renderer) {
    if (!m_open) return;

    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(renderer, &outW, &outH);
    if (outW <= 0 || outH <= 0) return;

    // Scale the 8x8 font up so the console shows roughly kTargetCols columns.
    const float scale = std::max(1.0f, SDL_floorf(static_cast<float>(outW) /
                                                  (kTargetCols * kGlyph)));
    SDL_SetRenderScale(renderer, scale, scale);

    const float w = static_cast<float>(outW) / scale;
    const float h = SDL_floorf(static_cast<float>(outH) / scale * kHeightFraction);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 12, 20, 225);
    SDL_FRect panel{0.0f, 0.0f, w, h};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 170, 255, 255);
    SDL_RenderLine(renderer, 0.0f, h, w, h);

    const bool showPane = m_searcher.isInitialized();
    const float paneX   = w - kPadding - kCandidateCols * kGlyph;
    const float textW   = (showPane ? paneX - kPadding * 2.0f : w - kPadding * 2.0f);
    const size_t maxCols = static_cast<size_t>(std::max(1.0f, textW / kGlyph));

    // Input line sits at the bottom of the panel.
    const float inputY = h - kPadding - kGlyph;

    // Scrollback fills upward from just above the input line.
    const int rows = static_cast<int>((inputY - kPadding * 2.0f) / kLineHeight);
    const int total = static_cast<int>(m_lines.size());
    m_scroll = std::clamp(m_scroll, 0, std::max(0, total - rows));

    SDL_SetRenderDrawColor(renderer, 205, 215, 225, 255);
    for (int row = 0; row < rows; ++row) {
        const int index = total - 1 - m_scroll - row;
        if (index < 0) break;
        const std::string& line = m_lines[static_cast<size_t>(index)];
        const float y = inputY - kPadding - (row + 1) * kLineHeight;
        SDL_RenderDebugText(renderer, kPadding, y,
                            line.size() <= maxCols ? line.c_str()
                                                   : line.substr(0, maxCols).c_str());
    }

    if (showPane) renderCandidatePane(renderer, paneX, h);

    // Prompt, entered text, and a blinking block cursor.
    SDL_SetRenderDrawColor(renderer, 120, 220, 140, 255);
    SDL_RenderDebugText(renderer, kPadding, inputY, ">");

    const float textX = kPadding + 2.0f * kGlyph;
    // Keep the caret on screen on long lines.
    const size_t firstCol = (m_cursor >= maxCols) ? m_cursor - maxCols + 1 : 0;
    SDL_SetRenderDrawColor(renderer, 235, 240, 245, 255);
    SDL_RenderDebugText(renderer, textX, inputY,
                        m_input.substr(firstCol, maxCols).c_str());

    if ((SDL_GetTicks() / 400) % 2 == 0) {
        SDL_FRect caret{textX + (m_cursor - firstCol) * kGlyph, inputY, kGlyph, kGlyph};
        SDL_SetRenderDrawColor(renderer, 235, 240, 245, 160);
        SDL_RenderFillRect(renderer, &caret);
    }

    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

void CheatConsole::renderCandidatePane(SDL_Renderer* renderer, float x, float h) {
    const auto& candidates = m_searcher.candidates();

    SDL_SetRenderDrawColor(renderer, 90, 170, 255, 120);
    SDL_RenderLine(renderer, x - kPadding, kPadding, x - kPadding, h - kPadding);

    char buf[64];
    SDL_SetRenderDrawColor(renderer, 90, 170, 255, 255);
    SDL_snprintf(buf, sizeof(buf), "%zu candidate(s)", candidates.size());
    SDL_RenderDebugText(renderer, x, kPadding, buf);

    // Values are re-read every frame, so the pane tracks memory live.
    const int rows = static_cast<int>((h - kPadding * 3.0f - kLineHeight) / kLineHeight);
    const int shown = std::min<int>(rows, static_cast<int>(candidates.size()));
    SDL_SetRenderDrawColor(renderer, 205, 215, 225, 255);
    for (int i = 0; i < shown; ++i) {
        SDL_snprintf(buf, sizeof(buf), "%08X %u",
                     candidates[static_cast<size_t>(i)],
                     m_searcher.readCurrent(candidates[static_cast<size_t>(i)]));
        SDL_RenderDebugText(renderer, x, kPadding + (i + 2) * kLineHeight, buf);
    }
    if (shown < static_cast<int>(candidates.size())) {
        SDL_SetRenderDrawColor(renderer, 130, 140, 150, 255);
        SDL_snprintf(buf, sizeof(buf), "... %zu more",
                     candidates.size() - static_cast<size_t>(shown));
        SDL_RenderDebugText(renderer, x, kPadding + (shown + 2) * kLineHeight, buf);
    }
}

// ─── dispatch ────────────────────────────────────────────────────────────────

void CheatConsole::dispatch(const std::string& line) {
    auto args = tokenize(line);
    if (args.empty()) return;

    const std::string& cmd = args[0];

    if      (cmd == "help")   cmdHelp();
    else if (cmd == "clear")  m_lines.clear();
    else if (cmd == "reset")  cmdReset(args);
    else if (cmd == "search") cmdSearch(args);
    else if (cmd == "apply")  cmdApply(args);
    else if (cmd == "add")    cmdAdd(args);
    else if (cmd == "list")   cmdList();
    else if (cmd == "toggle") cmdToggle(args);
    else if (cmd == "remove") cmdRemove(args);
    else print("Unknown command '%s'. Type 'help'.", cmd.c_str());
}

// ─── commands ─────────────────────────────────────────────────────────────────

void CheatConsole::cmdHelp() {
    static const char* const kHelp[] = {
        "reset [u8|u16|u32]                  Start a new memory search (default u16)",
        "search eq|ne <value>                Keep candidates (not) equal to value",
        "search gt|lt                        Compare against the previous snapshot",
        "search changed|unchanged            Keep candidates that (did not) change",
        "search list                         Print all candidates and their values",
        "apply <addr> <value> [1|2|4]        Write value to address once",
        "add <name> <addr> <val> [1|2|4]     Add a freeze cheat applied every frame",
        "list                                List cheat codes with index and state",
        "toggle <index>                      Toggle a cheat code on/off",
        "remove <index>                      Remove a cheat code",
        "clear                               Clear the console scrollback",
        "",
        "PgUp/PgDn or wheel scrolls, Up/Down recalls history, Esc closes.",
    };
    for (const char* line : kHelp) pushLine(line);
}

void CheatConsole::cmdReset(const std::vector<std::string>& args) {
    if (args.size() >= 2) {
        const std::string& w = args[1];
        if      (w == "u8"  || w == "1") m_searcher.setWidth(MemSearcher::Width::U8);
        else if (w == "u16" || w == "2") m_searcher.setWidth(MemSearcher::Width::U16);
        else if (w == "u32" || w == "4") m_searcher.setWidth(MemSearcher::Width::U32);
        else {
            print("Unknown width '%s'. Valid: u8, u16, u32.", w.c_str());
            return;
        }
    }

    const char* widthStr = (m_searcher.getWidth() == MemSearcher::Width::U8)  ? "u8"  :
                           (m_searcher.getWidth() == MemSearcher::Width::U16) ? "u16" : "u32";
    m_searcher.reset();
    print("Search reset (%s) - %zu candidates.", widthStr, m_searcher.candidateCount());
}

void CheatConsole::cmdSearch(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print("Usage: search <eq|ne|gt|lt|changed|unchanged|list> [value]");
        return;
    }
    if (!m_searcher.isInitialized()) {
        print("No active search. Run 'reset' first.");
        return;
    }

    const std::string& filter = args[1];

    if (filter == "list") {
        const auto& cands = m_searcher.candidates();
        if (cands.empty()) {
            print("No candidates.");
            return;
        }
        print("%zu candidate(s):", cands.size());
        for (u32 addr : cands) print("  0x%08X = %u", addr, m_searcher.readCurrent(addr));
        return;
    }

    if (filter == "eq" || filter == "ne") {
        if (args.size() < 3) {
            print("Usage: search %s <value>", filter.c_str());
            return;
        }
        u32 value = 0;
        if (!parseValue(args[2], value)) {
            print("Invalid value: %s", args[2].c_str());
            return;
        }
        m_searcher.filter(filter == "eq" ? MemSearcher::Filter::Equal
                                         : MemSearcher::Filter::NotEqual, value);
    } else {
        MemSearcher::Filter f;
        if      (filter == "gt")        f = MemSearcher::Filter::Greater;
        else if (filter == "lt")        f = MemSearcher::Filter::Less;
        else if (filter == "changed")   f = MemSearcher::Filter::Changed;
        else if (filter == "unchanged") f = MemSearcher::Filter::Unchanged;
        else {
            print("Unknown filter '%s'. Valid: eq, ne, gt, lt, changed, unchanged, list.",
                  filter.c_str());
            return;
        }
        m_searcher.filter(f);
    }

    print("%zu candidates remain.", m_searcher.candidateCount());
}

void CheatConsole::cmdApply(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        print("Usage: apply <addr> <val> [width: 1|2|4]");
        return;
    }
    u32 addr = 0, val = 0;
    if (!parseValue(args[1], addr)) {
        print("Invalid address: %s", args[1].c_str());
        return;
    }
    if (!parseValue(args[2], val)) {
        print("Invalid value: %s", args[2].c_str());
        return;
    }

    CheatCode code;
    code.name    = "one-shot";
    code.address = addr;
    code.value   = val;
    code.width   = (args.size() >= 4) ? parseWidth(args[3]) : 2;
    code.enabled = true;
    m_engine.apply(code);
    print("Applied: [0x%08X] = %u (%d byte(s)).", addr, val, code.width);
}

void CheatConsole::cmdAdd(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        print("Usage: add <name> <addr> <val> [width: 1|2|4]");
        return;
    }
    u32 addr = 0, val = 0;
    if (!parseValue(args[2], addr)) {
        print("Invalid address: %s", args[2].c_str());
        return;
    }
    if (!parseValue(args[3], val)) {
        print("Invalid value: %s", args[3].c_str());
        return;
    }

    CheatCode code;
    code.name    = args[1];
    code.address = addr;
    code.value   = val;
    code.width   = (args.size() >= 5) ? parseWidth(args[4]) : 2;
    code.enabled = true;
    m_engine.addCode(code);
    print("Added code #%zu '%s': [0x%08X] = %u (%d byte(s)).",
          m_engine.getCodes().size() - 1, code.name.c_str(), addr, val, code.width);
}

void CheatConsole::cmdList() {
    const auto& codes = m_engine.getCodes();
    if (codes.empty()) {
        print("No cheat codes loaded.");
        return;
    }
    print("%zu code(s):", codes.size());
    for (size_t i = 0; i < codes.size(); ++i) {
        const auto& c = codes[i];
        print("  [%zu] %s  addr=0x%08X  val=%u  width=%d  %s",
              i, c.name.c_str(), c.address, c.value, c.width, c.enabled ? "ON" : "OFF");
    }
}

void CheatConsole::cmdToggle(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print("Usage: toggle <index>");
        return;
    }
    size_t idx = static_cast<size_t>(strtoul(args[1].c_str(), nullptr, 10));
    if (idx >= m_engine.getCodes().size()) {
        print("Index %zu out of range (0-%zu).", idx, m_engine.getCodes().size() - 1);
        return;
    }
    m_engine.toggleCode(idx);
    const auto& c = m_engine.getCodes()[idx];
    print("Code #%zu '%s' is now %s.", idx, c.name.c_str(), c.enabled ? "ON" : "OFF");
}

void CheatConsole::cmdRemove(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        print("Usage: remove <index>");
        return;
    }
    size_t idx = static_cast<size_t>(strtoul(args[1].c_str(), nullptr, 10));
    if (idx >= m_engine.getCodes().size()) {
        print("Index %zu out of range (0-%zu).", idx, m_engine.getCodes().size() - 1);
        return;
    }
    const std::string name = m_engine.getCodes()[idx].name;
    m_engine.removeCode(idx);
    print("Removed code #%zu '%s'.", idx, name.c_str());
}
