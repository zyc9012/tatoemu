#include "cheat_console.h"
#include "config.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

CheatConsole::CheatConsole(const fs::path& romPath) {
    m_defaultCht = romPath;
    m_defaultCht.replace_extension(".cht");
}

CheatConsole::~CheatConsole() {
    stop();
}

void CheatConsole::start() {
    m_running = true;
    m_thread = std::thread(&CheatConsole::readLoop, this);
    log_info("Cheat console ready. Type 'help' for commands.");
    fflush(stdout);
}

void CheatConsole::stop() {
    m_running = false;
    // The background thread may be blocked in getline; closing stdin
    // from another thread is not safe on all platforms. We detach and
    // let the OS clean up when the process exits.
    if (m_thread.joinable()) m_thread.detach();
}

void CheatConsole::readLoop() {
    char buf[512];
    while (m_running && fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
        if (len > 0) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::string(buf, len));
        }
    }
}

void CheatConsole::drain(CheatEngine& engine, MemSearcher& searcher) {
    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(local, m_queue);
    }
    while (!local.empty()) {
        dispatch(local.front(), engine, searcher);
        local.pop();
    }
}

void CheatConsole::dispatch(const std::string& line, CheatEngine& engine, MemSearcher& searcher) {
    auto args = tokenize(line);
    if (args.empty()) return;

    const std::string& cmd = args[0];

    if      (cmd == "help")   cmdHelp();
    else if (cmd == "reset")  cmdReset(args, searcher);
    else if (cmd == "search") cmdSearch(args, searcher);
    else if (cmd == "apply")  cmdApply(args, engine);
    else if (cmd == "add")    cmdAdd(args, engine);
    else if (cmd == "list")   cmdList(engine);
    else if (cmd == "toggle") cmdToggle(args, engine);
    else if (cmd == "remove") cmdRemove(args, engine);
    else {
        log_info("[Cheat] Unknown command '%s'. Type 'help'.", cmd.c_str());
        fflush(stdout);
    }
}

// ─── commands ─────────────────────────────────────────────────────────────────

void CheatConsole::cmdHelp() {
    log_info_nn(
        "\nCheat console commands:\n"
        "  reset [u8|u16|u32]                  Start a new memory search (default: u16)\n"
        "  search eq   <value>                 Keep candidates equal to value\n"
        "  search ne   <value>                 Keep candidates not equal to value\n"
        "  search gt                           Keep candidates greater than previous snapshot\n"
        "  search lt                           Keep candidates less than previous snapshot\n"
        "  search changed                      Keep candidates whose value changed\n"
        "  search unchanged                    Keep candidates whose value did not change\n"
        "  search list                         Print all current candidates and their values\n"
        "  apply <addr> <value> [1|2|4]        Write value to address once (width defaults to 2)\n"
        "  add   <name> <addr> <value> [1|2|4] Add a freeze cheat applied every frame\n"
        "  list                                List all cheat codes with index and state\n"
        "  toggle <index>                      Toggle a cheat code on/off\n"
        "  remove <index>                      Remove a cheat code\n"
        "  help                                Show this help\n\n"
    );
    fflush(stdout);
}

void CheatConsole::cmdReset(const std::vector<std::string>& args, MemSearcher& searcher) {
    if (args.size() >= 2) {
        const std::string& w = args[1];
        if      (w == "u8"  || w == "1") searcher.setWidth(MemSearcher::Width::U8);
        else if (w == "u16" || w == "2") searcher.setWidth(MemSearcher::Width::U16);
        else if (w == "u32" || w == "4") searcher.setWidth(MemSearcher::Width::U32);
        else {
            log_info("[Cheat] Unknown width '%s'. Valid: u8, u16, u32.", w.c_str());
            fflush(stdout);
            return;
        }
    }

    const char* widthStr = (searcher.getWidth() == MemSearcher::Width::U8)  ? "u8"  :
                           (searcher.getWidth() == MemSearcher::Width::U16) ? "u16" : "u32";
    searcher.reset();
    log_info("[Cheat] Search reset (%s) - %zu candidates.",
             widthStr, searcher.candidateCount());
    fflush(stdout);
}

void CheatConsole::cmdSearch(const std::vector<std::string>& args, MemSearcher& searcher) {
    if (args.size() < 2) {
        log_info("[Cheat] Usage: search <eq|ne|gt|lt|changed|unchanged|list> [value]");
        fflush(stdout);
        return;
    }
    if (!searcher.isInitialized()) {
        log_info("[Cheat] No active search. Run 'reset' first.");
        fflush(stdout);
        return;
    }

    const std::string& filter = args[1];

    if (filter == "list") {
        const auto& cands = searcher.candidates();
        if (cands.empty()) {
            log_info("[Cheat] No candidates.");
            fflush(stdout);
            return;
        }
        log_info("[Cheat] %zu candidate(s):", cands.size());
        for (u32 addr : cands)
            log_info("  0x%08X = %u", addr, searcher.readCurrent(addr));
        fflush(stdout);
        return;
    }

    // Filters that need a value.
    if (filter == "eq" || filter == "ne") {
        if (args.size() < 3) {
            log_info("[Cheat] Usage: search %s <value>", filter.c_str());
            fflush(stdout);
            return;
        }
        u32 value = 0;
        if (!parseValue(args[2], value)) {
            log_info("[Cheat] Invalid value: %s", args[2].c_str());
            fflush(stdout);
            return;
        }
        MemSearcher::Filter f = (filter == "eq") ? MemSearcher::Filter::Equal
                                                  : MemSearcher::Filter::NotEqual;
        searcher.filter(f, value);
    } else {
        MemSearcher::Filter f;
        if      (filter == "gt")        f = MemSearcher::Filter::Greater;
        else if (filter == "lt")        f = MemSearcher::Filter::Less;
        else if (filter == "changed")   f = MemSearcher::Filter::Changed;
        else if (filter == "unchanged") f = MemSearcher::Filter::Unchanged;
        else {
            log_info("[Cheat] Unknown filter '%s'. Valid: eq, ne, gt, lt, changed, unchanged, list.",
                     filter.c_str());
            fflush(stdout);
            return;
        }
        searcher.filter(f);
    }

    log_info("[Cheat] %zu candidates remain.", searcher.candidateCount());
    fflush(stdout);
}

void CheatConsole::cmdApply(const std::vector<std::string>& args, CheatEngine& engine) {
    // apply <addr> <val> [width]
    if (args.size() < 3) {
        log_info("[Cheat] Usage: apply <addr> <val> [width: 1|2|4]");
        fflush(stdout);
        return;
    }
    u32 addr = 0, val = 0;
    if (!parseValue(args[1], addr)) {
        log_info("[Cheat] Invalid address: %s", args[1].c_str());
        fflush(stdout);
        return;
    }
    if (!parseValue(args[2], val)) {
        log_info("[Cheat] Invalid value: %s", args[2].c_str());
        fflush(stdout);
        return;
    }
    u8 width = (args.size() >= 4) ? parseWidth(args[3]) : 2;

    CheatCode code;
    code.name    = "one-shot";
    code.address = addr;
    code.value   = val;
    code.width   = width;
    code.enabled = true;
    engine.apply(code);
    log_info("[Cheat] Applied: [0x%08X] = %u (%d byte(s)).", addr, val, width);
    fflush(stdout);
}

void CheatConsole::cmdAdd(const std::vector<std::string>& args, CheatEngine& engine) {
    // add <name> <addr> <val> [width]
    if (args.size() < 4) {
        log_info("[Cheat] Usage: add <name> <addr> <val> [width: 1|2|4]");
        fflush(stdout);
        return;
    }
    u32 addr = 0, val = 0;
    if (!parseValue(args[2], addr)) {
        log_info("[Cheat] Invalid address: %s", args[2].c_str());
        fflush(stdout);
        return;
    }
    if (!parseValue(args[3], val)) {
        log_info("[Cheat] Invalid value: %s", args[3].c_str());
        fflush(stdout);
        return;
    }
    u8 width = (args.size() >= 5) ? parseWidth(args[4]) : 2;

    CheatCode code;
    code.name    = args[1];
    code.address = addr;
    code.value   = val;
    code.width   = width;
    code.enabled = true;
    engine.addCode(code);
    log_info("[Cheat] Added code #%zu '%s': [0x%08X] = %u (%d byte(s)).",
             engine.getCodes().size() - 1, code.name.c_str(), addr, val, width);
    fflush(stdout);
}

void CheatConsole::cmdList(CheatEngine& engine) {
    const auto& codes = engine.getCodes();
    if (codes.empty()) {
        log_info("[Cheat] No cheat codes loaded.");
        fflush(stdout);
        return;
    }
    log_info("[Cheat] %zu code(s):", codes.size());
    for (size_t i = 0; i < codes.size(); ++i) {
        const auto& c = codes[i];
        log_info("  [%zu] %s  addr=0x%08X  val=%u  width=%d  %s",
                 i, c.name.c_str(), c.address, c.value, c.width,
                 c.enabled ? "ON" : "OFF");
    }
    fflush(stdout);
}

void CheatConsole::cmdToggle(const std::vector<std::string>& args, CheatEngine& engine) {
    if (args.size() < 2) {
        log_info("[Cheat] Usage: toggle <index>");
        fflush(stdout);
        return;
    }
    size_t idx = static_cast<size_t>(strtoul(args[1].c_str(), nullptr, 10));
    if (idx >= engine.getCodes().size()) {
        log_info("[Cheat] Index %zu out of range (0-%zu).",
                 idx, engine.getCodes().size() - 1);
        fflush(stdout);
        return;
    }
    engine.toggleCode(idx);
    const auto& c = engine.getCodes()[idx];
    log_info("[Cheat] Code #%zu '%s' is now %s.", idx, c.name.c_str(),
             c.enabled ? "ON" : "OFF");
    fflush(stdout);
}

void CheatConsole::cmdRemove(const std::vector<std::string>& args, CheatEngine& engine) {
    if (args.size() < 2) {
        log_info("[Cheat] Usage: remove <index>");
        fflush(stdout);
        return;
    }
    size_t idx = static_cast<size_t>(strtoul(args[1].c_str(), nullptr, 10));
    if (idx >= engine.getCodes().size()) {
        log_info("[Cheat] Index %zu out of range (0-%zu).",
                 idx, engine.getCodes().size() - 1);
        fflush(stdout);
        return;
    }
    const std::string name = engine.getCodes()[idx].name;
    engine.removeCode(idx);
    log_info("[Cheat] Removed code #%zu '%s'.", idx, name.c_str());
    fflush(stdout);
}
