#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"
#include <filesystem>
#include <string>
#include <vector>

namespace gba {

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    bool load(const fs::path& filename);

    bool isLoaded() const { return m_loaded; }
    const std::string& getTitle() const { return m_title; }

    u8* getROM() { return m_rom.data(); }
    u32 getROMSize() const { return static_cast<u32>(m_rom.size()); }

    SaveType getSaveType() const { return m_saveType; }

    // SRAM/Flash access
    u8 readSave(u32 address) const;
    void writeSave(u32 address, u8 value);

    // Battery save/load
    void saveBattery() const;
    void loadBattery();

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void parseHeader();
    void detectSaveType();

    bool m_loaded = false;
    std::string m_title;
    std::string m_gameCode;
    std::vector<u8> m_rom;

    // Save data
    SaveType m_saveType = SaveType::NONE;
    std::vector<u8> m_sram;
    fs::path m_savePath;

    // Flash state machine
    enum class FlashState { READY, CMD1, CMD2, ERASE, WRITE, BANK_SELECT, ID };
    FlashState m_flashState = FlashState::READY;
    u8 m_flashBank = 0;
    bool m_flashIdMode = false;
};

} // namespace gba
