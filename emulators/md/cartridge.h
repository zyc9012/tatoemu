#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"
#include <array>
#include <string>
#include <vector>

namespace md {

// ---------------------------------------------------------------------------
// Mega Drive cartridge
//
// Handles ROM loading (.bin/.gen/.md raw images, .smd interleaved images and
// ZIP archives containing either), header parsing, battery-backed SRAM and the
// SSF2/"Super Street Fighter 2" style 512 KB bank mapper exposed through
// 0xA130F3-0xA130FF.
// ---------------------------------------------------------------------------
class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    bool load(const fs::path& filename);
    void reset();

    // ".bin" is shared with other consoles, so callers can sniff the console
    // name in the cartridge header at 0x100 to identify a Mega Drive image.
    static bool hasHeader(const u8* data, size_t size);
    static bool fileHasHeader(const fs::path& filename);

    // 68000 ROM/SRAM area reads (0x000000-0x3FFFFF)
    u8  read8(u32 address) const;
    u16 read16(u32 address) const;
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);

    // Cartridge control registers at 0xA130F0-0xA130FF
    void writeControl(u32 address, u8 value);

    bool isLoaded() const { return m_loaded; }
    const std::string& getTitle() const { return m_title; }
    bool isPAL() const { return m_pal; }

    // Console region the cartridge expects: 0 = Japan, 1 = USA, 2 = Europe.
    // Games that lock themselves out with a "region" screen read this back
    // through the version register at 0xA10001.
    u8 getRegion() const { return m_region; }

    void saveBattery();
    void loadBattery();

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void parseHeader();
    static void deinterleaveSMD(std::vector<u8>& data);
    // Translate a 68000 address into a linear ROM offset through the bank map.
    u32 mapRomOffset(u32 address) const;

    std::vector<u8> m_rom;
    std::vector<u8> m_sram;

    // SSF2 mapper: eight 512 KB windows covering 0x000000-0x3FFFFF.
    std::array<u8, 8> m_banks{};

    bool m_loaded = false;
    bool m_pal = false;
    u8   m_region = 1;

    bool m_hasSram = false;
    bool m_sramEnabled = false;
    // True when the ROM image extends into the declared SRAM window; SRAM then
    // stays hidden until the game selects it through 0xA130F1.
    bool m_sramOverlapsRom = false;
    u32  m_sramStart = 0x200000;
    u32  m_sramEnd   = 0x20FFFF;
    bool m_sramDirty = false;

    std::string m_title;
    fs::path m_savePath;
};

} // namespace md
