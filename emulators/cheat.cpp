#include "cheat.h"
#include "config.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>

// ─── CheatEngine ─────────────────────────────────────────────────────────────

void CheatEngine::apply(CheatCode& code) {
    if (!m_mem) return;
    switch (code.width) {
        case 1: m_mem->poke8 (code.address, static_cast<u8> (code.value)); break;
        case 2: m_mem->poke16(code.address, static_cast<u16>(code.value)); break;
        case 4: m_mem->poke32(code.address, code.value);                   break;
    }
}

void CheatEngine::applyAll() {
    for (auto& code : m_codes)
        if (code.enabled) apply(code);
}

void CheatEngine::removeCode(size_t index) {
    if (index < m_codes.size())
        m_codes.erase(m_codes.begin() + static_cast<ptrdiff_t>(index));
}

void CheatEngine::toggleCode(size_t index) {
    if (index < m_codes.size())
        m_codes[index].enabled = !m_codes[index].enabled;
}

// ─── MemSearcher ─────────────────────────────────────────────────────────────

void MemSearcher::reset() {
    if (!m_mem) return;

    m_regions    = m_mem->getSearchRegions();
    m_candidates.clear();

    const int w = static_cast<int>(m_width);
    for (const auto& r : m_regions) {
        for (u32 off = 0; off + static_cast<u32>(w) <= r.size; off += static_cast<u32>(w))
            m_candidates.push_back(r.base + off);
    }

    takeSnapshot();
    m_initialized = true;

    log_info("[Search] Reset - %zu candidate(s), width=%d byte(s)",
             m_candidates.size(), w);
}

void MemSearcher::filter(Filter f, u32 value) {
    if (!m_initialized || !m_mem) return;

    std::vector<u32> next;
    next.reserve(m_candidates.size());

    for (u32 addr : m_candidates) {
        const u32 cur  = readCurrent(addr);
        const u32 prev = readSnapshot(addr);
        bool keep = false;
        switch (f) {
            case Filter::Equal:     keep = (cur == value); break;
            case Filter::NotEqual:  keep = (cur != value); break;
            case Filter::Greater:   keep = (cur >  prev);  break;
            case Filter::Less:      keep = (cur <  prev);  break;
            case Filter::Changed:   keep = (cur != prev);  break;
            case Filter::Unchanged: keep = (cur == prev);  break;
        }
        if (keep) next.push_back(addr);
    }

    m_candidates = std::move(next);
    takeSnapshot();  // advance so the next relative filter compares from now

    log_info("[Search] Filter - %zu candidate(s) remain", m_candidates.size());
}

// ─── Snapshot helpers ────────────────────────────────────────────────────────

void MemSearcher::takeSnapshot() {
    size_t total = 0;
    for (const auto& r : m_regions) total += r.size;
    m_snapshot.resize(total);

    size_t off = 0;
    for (const auto& r : m_regions) {
        for (u32 i = 0; i < r.size; ++i)
            m_snapshot[off + i] = m_mem->peek8(r.base + i);
        off += r.size;
    }
}

size_t MemSearcher::addrToSnapshotOffset(u32 addr) const {
    size_t off = 0;
    for (const auto& r : m_regions) {
        if (addr >= r.base && addr < r.base + r.size)
            return off + (addr - r.base);
        off += r.size;
    }
    return SIZE_MAX;
}

u32 MemSearcher::readCurrent(u32 addr) const {
    switch (m_width) {
        case Width::U8:  return m_mem->peek8 (addr);
        case Width::U16: return m_mem->peek16(addr);
        case Width::U32: return m_mem->peek32(addr);
    }
    return 0;
}

u32 MemSearcher::readSnapshot(u32 addr) const {
    const size_t off = addrToSnapshotOffset(addr);
    const int    w   = static_cast<int>(m_width);
    if (off == SIZE_MAX || off + static_cast<size_t>(w) > m_snapshot.size()) return 0;
    u32 v = 0;
    for (int i = 0; i < w; ++i)
        v |= static_cast<u32>(m_snapshot[off + i]) << (8 * i);
    return v;
}
