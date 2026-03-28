#pragma once

#include "cheat.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace fs = std::filesystem;

// ─── CheatConsole ─────────────────────────────────────────────────────────────

class CheatConsole {
public:
    // romPath is used as the default sidecar path for `save`.
    explicit CheatConsole(const fs::path& romPath);
    ~CheatConsole();

    // Start the background stdin reader thread.
    void start();
    // Signal the reader thread to stop and join it.
    void stop();

    // Called from the main thread once per frame. Pops all queued lines,
    // executes them, and prints results to stdout.
    void drain(CheatEngine& engine, MemSearcher& searcher);

private:
    void readLoop();
    void dispatch(const std::string& line, CheatEngine& engine, MemSearcher& searcher);

    // Commands
    void cmdHelp();
    void cmdReset(const std::vector<std::string>& args, MemSearcher& searcher);
    void cmdSearch(const std::vector<std::string>& args, MemSearcher& searcher);
    void cmdApply(const std::vector<std::string>& args, CheatEngine& engine);
    void cmdAdd(const std::vector<std::string>& args, CheatEngine& engine);
    void cmdList(CheatEngine& engine);
    void cmdToggle(const std::vector<std::string>& args, CheatEngine& engine);
    void cmdRemove(const std::vector<std::string>& args, CheatEngine& engine);

    fs::path m_defaultCht;

    std::thread          m_thread;
    std::atomic<bool>    m_running{false};
    std::mutex           m_mutex;
    std::queue<std::string> m_queue;
};
