#pragma once

#include "types.h"
#include "consts.h"
#include "eeprom.h"
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
    
    // EEPROM access (called by DMA when accessing ROM2_EX region)
    u16 readEEPROM();
    void writeEEPROM(u16 value, u32 writeSize);
    bool hasEEPROM() const { return m_saveType == SaveType::EEPROM_512 || m_saveType == SaveType::EEPROM_8K; }
    EEPROM* getEEPROM() { return &m_eeprom; }

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
    enum class FlashState { READY, CMD1, CMD2, WRITE, BANK_SELECT };
    FlashState m_flashState = FlashState::READY;
    u8 m_flashBank = 0;
    bool m_flashIdMode = false;
    bool m_flashEraseMode = false;
    
    // EEPROM
    EEPROM m_eeprom;
};

} // namespace gba
