#pragma once

#include "cheat.h"
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

// ─── CheatConsole ─────────────────────────────────────────────────────────────
// Quake-style drop-down console drawn over the emulated frame. Text is entered
// through SDL text input and rendered with SDL_RenderDebugText, so it needs no
// font asset and no terminal.

class CheatConsole {
public:
    CheatConsole(CheatEngine& engine, MemSearcher& searcher);

    bool isOpen() const { return m_open; }

    // Invoked for every held key when the console opens, so the game does not
    // see a button stuck down while the player is typing.
    void setKeyReleaseCallback(std::function<void(SDL_Keycode)> cb) {
        m_releaseKey = std::move(cb);
    }

    // Returns true when the event belongs to the console and must not reach
    // the game. Call before the core sees the event.
    bool handleEvent(const SDL_Event& event);

    // Draws the overlay. Call between the frame blit and the present.
    void render(SDL_Renderer* renderer);

private:
    void setOpen(bool open, SDL_WindowID windowID);

    void renderCandidatePane(SDL_Renderer* renderer, float x, float h);

    void print(SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);
    void pushLine(std::string line);

    void submit();
    void recallHistory(int delta);

    void dispatch(const std::string& line);

    // Commands
    void cmdHelp();
    void cmdReset(const std::vector<std::string>& args);
    void cmdSearch(const std::vector<std::string>& args);
    void cmdApply(const std::vector<std::string>& args);
    void cmdAdd(const std::vector<std::string>& args);
    void cmdList();
    void cmdToggle(const std::vector<std::string>& args);
    void cmdRemove(const std::vector<std::string>& args);

    CheatEngine& m_engine;
    MemSearcher& m_searcher;

    std::function<void(SDL_Keycode)> m_releaseKey;

    bool m_open = false;

    std::deque<std::string> m_lines;    // scrollback, oldest first
    int  m_scroll = 0;                  // lines scrolled up from the bottom

    std::string m_input;
    size_t      m_cursor = 0;

    std::vector<std::string> m_history;
    int m_historyPos = -1;              // -1 means "editing a fresh line"
    std::string m_stashedInput;         // in-progress line parked by history recall
};
