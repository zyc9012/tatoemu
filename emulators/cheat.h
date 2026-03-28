#pragma once

#include "types.h"
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ─── Searchable memory region ────────────────────────────────────────────────

struct MemRegion {
    u32 base;
    u32 size;  // bytes
};

// ─── Memory interface ────────────────────────────────────────────────────────
// Implemented by each core to expose its mutable RAM to the cheat subsystem.

class ICheatMemory {
public:
    virtual ~ICheatMemory() = default;

    virtual u8  peek8 (u32 addr) = 0;
    virtual u16 peek16(u32 addr) {
        return static_cast<u16>(peek8(addr)) | (static_cast<u16>(peek8(addr + 1)) << 8);
    }
    virtual u32 peek32(u32 addr) {
        return static_cast<u32>(peek16(addr)) | (static_cast<u32>(peek16(addr + 2)) << 16);
    }
    virtual void poke8 (u32 addr, u8  val) = 0;
    virtual void poke16(u32 addr, u16 val) {
        poke8(addr, static_cast<u8>(val));
        poke8(addr + 1, static_cast<u8>(val >> 8));
    }
    virtual void poke32(u32 addr, u32 val) {
        poke16(addr, static_cast<u16>(val));
        poke16(addr + 2, static_cast<u16>(val >> 16));
    }

    // RAM regions that make sense to search (excludes ROM and MMIO).
    virtual std::vector<MemRegion> getSearchRegions() const = 0;
};

// ─── Cheat code ──────────────────────────────────────────────────────────────

struct CheatCode {
    std::string name;
    u32  address = 0;
    u32  value   = 0;
    u8   width   = 2;    // bytes: 1, 2, or 4
    bool enabled = true;
};

// ─── Cheat engine ────────────────────────────────────────────────────────────

class CheatEngine {
public:
    void setMemory(ICheatMemory* mem) { m_mem = mem; }

    // Apply a single code unconditionally, ignoring the enabled flag.
    void apply(CheatCode& code);

    // Apply all enabled codes — call once per frame.
    void applyAll();

    // Code management.
    void addCode   (const CheatCode& code) { m_codes.push_back(code); }
    void removeCode(size_t index);
    void toggleCode(size_t index);
    void clearCodes()                      { m_codes.clear(); }

    std::vector<CheatCode>&       getCodes()       { return m_codes; }
    const std::vector<CheatCode>& getCodes() const { return m_codes; }
    bool isEmpty() const { return m_codes.empty(); }

private:
    ICheatMemory*          m_mem = nullptr;
    std::vector<CheatCode> m_codes;
};

// ─── Memory searcher ─────────────────────────────────────────────────────────

class MemSearcher {
public:
    enum class Filter  { Equal, NotEqual, Greater, Less, Changed, Unchanged };
    enum class Width   { U8 = 1, U16 = 2, U32 = 4 };

    void setMemory(ICheatMemory* mem) { m_mem = mem; m_initialized = false; }
    void setWidth (Width w)           { m_width = w; }
    Width getWidth() const            { return m_width; }

    // Start a fresh search: seed all aligned candidates from every search
    // region and take the initial snapshot.
    void reset();

    // Narrow the candidate list.
    // Equal / NotEqual compare current memory against `value`.
    // Greater / Less / Changed / Unchanged compare current memory against
    // the snapshot captured by the previous reset() or filter() call.
    // The snapshot is always advanced at the end so the next relative
    // filter is compared to the current state.
    void filter(Filter f, u32 value = 0);

    size_t candidateCount() const              { return m_candidates.size(); }
    const std::vector<u32>& candidates() const { return m_candidates; }
    bool isInitialized() const                 { return m_initialized; }
    u32 readCurrent(u32 addr) const;

private:
    u32    readSnapshot(u32 addr) const;
    void   takeSnapshot();
    size_t addrToSnapshotOffset(u32 addr) const;

    ICheatMemory*          m_mem         = nullptr;
    Width                  m_width       = Width::U16;
    bool                   m_initialized = false;
    std::vector<u32>       m_candidates;
    std::vector<u8>        m_snapshot;
    std::vector<MemRegion> m_regions;    // cached from last reset()
};
