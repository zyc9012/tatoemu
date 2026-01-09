#include "cartridge.h"
#include "../cpu.h"
#include "ppu.h"
#include "zip_reader.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>

namespace cps1 {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_title("Unknown CPS1 Game")
    , m_romSetName("")
    , m_gameInfo(nullptr)
    , m_programRomSize(0)
    , m_graphicsRomSize(0)
    , m_soundRomSize(0)
    , m_programByteswap(false) {
}

void Cartridge::setPPU(cps::PPUBase* ppu) {
    m_ppu = static_cast<PPU*>(ppu);
}

u8 Cartridge::getDIPSwitch(u8 index) const {
    if (!m_gameInfo || index > 3) {
        return 0xFF;  // Return default if no game loaded
    }
    
    switch (index) {
        case 0: return m_gameInfo->dipSwitch1;
        case 1: return m_gameInfo->dipSwitch2;
        case 2: return m_gameInfo->dipSwitch3;
        case 3: return m_gameInfo->dipSwitch4;
        default: return 0xFF;
    }
}

BoardConfig Cartridge::getBoardConfig() const {
    if (!m_gameInfo) {
        // Return default CPS_B_11 config
        return GameDatabase::getBoardConfig(CPSBoard::CPS_B_11);
    }
    return GameDatabase::getBoardConfig(m_gameInfo->board);
}

CPSBoard Cartridge::getBoardType() const {
    if (!m_gameInfo) {
        return CPSBoard::CPS_B_11;  // Default
    }
    return m_gameInfo->board;
}

CPSMapper Cartridge::getMapper() const {
    if (!m_gameInfo) {
        return CPSMapper::MAPPER_STF29;  // Default
    }
    return m_gameInfo->mapper;
}

bool Cartridge::load(const fs::path& filename) {
    // Check if it's a ZIP file
    fs::path ext = filename.extension();
    if (ext != ".zip") {
        std::cerr << "CPS1 ROMs must be in ZIP format" << std::endl;
        return false;
    }
    
    // Extract ROM set name from filename (without .zip extension)
    m_romSetName = filename.stem().string();
    
    // Look up game in database
    m_gameInfo = GameDatabase::findGame(m_romSetName);
    if (!m_gameInfo) {
        std::cerr << "Unsupported CPS1 game: " << m_romSetName << std::endl;
        std::cerr << "Only games in the database are supported." << std::endl;
        return false;
    }
    
    m_title = m_gameInfo->name;
    m_programByteswap = m_gameInfo->programByteswap;
    
    // Open and extract ZIP file
    ZipReader zip;
    if (!zip.open(filename)) {
        std::cerr << "Failed to open ZIP file: " << filename << std::endl;
        return false;
    }
    
    // Extract all ROM files from ZIP
    std::map<std::string, std::vector<u8>> romFiles;
    if (!zip.extractAll(romFiles)) {
        std::cerr << "Failed to extract files from ZIP" << std::endl;
        return false;
    }
    
    // Load ROMs using game database
    if (!loadROMsFromDatabase(romFiles)) {
        return false;
    }
    
    std::cout << "Loaded CPS1 ROM: " << m_title << std::endl;
    std::cout << "  ROM Set: " << m_romSetName << std::endl;
    std::cout << "  Board Type: " << static_cast<int>(m_gameInfo->board) << std::endl;
    std::cout << "  Graphics Mapper: " << static_cast<int>(m_gameInfo->mapper) << std::endl;
    std::cout << "  Program ROM: " << (m_programRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Graphics ROM: " << (m_graphicsRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Sound ROM: " << (m_soundRomSize / 1024) << " KB" << std::endl;
    
    return true;
}

bool Cartridge::loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles) {
    if (!m_gameInfo) {
        return false;
    }
    
    // Clear existing ROMs
    m_programRom.clear();
    m_graphicsRom.clear();
    m_soundRom.clear();
    
    // Track which ROMs we've found
    std::vector<bool> foundROMs(m_gameInfo->romCount, false);
    std::vector<std::string> missingROMs;
    std::vector<std::string> invalidROMs;
    
    // Temporary storage for program ROMs (need to interleave them)
    std::vector<std::vector<u8>> programRomChips;
    
    // Load ROMs in database order
    for (u32 i = 0; i < m_gameInfo->romCount; i++) {
        const ROMEntry& entry = m_gameInfo->roms[i];
        
        // Skip optional ROMs (PLDs) for now
        if (entry.type == ROMType::PLD && (entry.flags & ROM_FLAG_OPTIONAL)) {
            continue;
        }
        
        // Find the ROM file (case-insensitive)
        bool found = false;
        for (const auto& pair : romFiles) {
            if (GameDatabase::validateROM(pair.first, pair.second, entry)) {
                found = true;
                foundROMs[i] = true;
                
                // Add to appropriate ROM bank
                switch (entry.type) {
                    case ROMType::PROGRAM:
                        // Store program ROMs separately for interleaving
                        std::cout << "Loading program: " << entry.filename << std::endl;
                        programRomChips.push_back(pair.second);
                        break;
                    case ROMType::GRAPHICS:
                        std::cout << "Loading graphics: " << entry.filename << std::endl;
                        m_graphicsRom.insert(m_graphicsRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::SOUND_PROGRAM:
                    case ROMType::SOUND_SAMPLE:
                        std::cout << "Loading sound: " << entry.filename << std::endl;
                        m_soundRom.insert(m_soundRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::PLD:
                        // Skip PLD files
                        break;
                    default:
                        break;
                }
                break;
            }
        }
        
        if (!found) {
            if (entry.flags & ROM_FLAG_OPTIONAL) {
                // Optional ROM missing - just warn
                std::cerr << "Warning: Optional ROM missing: " << entry.filename << std::endl;
            } else {
                missingROMs.push_back(entry.filename);
            }
        }
    }
    
    // Handle program ROM chips
    // Some games (SF2) use pairs of ROMs that need interleaving
    // Other games (SF2CE) use larger ROMs that just need concatenation
    if (!programRomChips.empty()) {
        bool needsInterleaving = m_programByteswap;
        
        if (needsInterleaving) {
            // FBNeo loads ROMs in the exact order they appear in the ROM list
            // No reordering needed - use database order directly
            // Calculate total size
            u32 totalSize = 0;
            for (const auto& chip : programRomChips) {
                totalSize += static_cast<u32>(chip.size());
            }
            m_programRom.resize(totalSize);
            
            // Interleave in pairs: even chip, odd chip, even chip, odd chip...
            u32 offset = 0;
            for (size_t i = 0; i < programRomChips.size(); i += 2) {
                const auto& evenChip = programRomChips[i];
                const auto& oddChip = programRomChips[i + 1];
                
                if (evenChip.size() != oddChip.size()) {
                    std::cerr << "Error: ROM chip size mismatch in pair " << (i/2) << std::endl;
                    return false;
                }
                
                // Interleave bytes: odd byte from second chip, even byte from first chip
                // CPS1 ROM naming: "e" suffix = odd address bytes, "f" suffix = even address bytes
                for (size_t j = 0; j < evenChip.size(); j++) {
                    m_programRom[offset++] = oddChip[j];  // "f" chip has even address bytes
                    m_programRom[offset++] = evenChip[j];  // "e" chip has odd address bytes
                }
            }
        } else {
            // For SF2CE and similar games, simple concatenation (NO byte-swapping needed)
            // Per FBNeo: CPS1_68K_PROGRAM_NO_BYTESWAP just concatenates ROMs in order
            u32 totalSize = 0;
            for (const auto& chip : programRomChips) {
                totalSize += static_cast<u32>(chip.size());
            }
            m_programRom.resize(totalSize);
            
            u32 offset = 0;
            for (const auto& chip : programRomChips) {
                std::memcpy(&m_programRom[offset], chip.data(), chip.size());
                offset += static_cast<u32>(chip.size());
            }
        }
    }
    
    // Check for invalid ROMs (files in ZIP that aren't in database)
    for (const auto& pair : romFiles) {
        bool inDatabase = false;
        for (u32 i = 0; i < m_gameInfo->romCount; i++) {
            std::string lowerFilename = pair.first;
            std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
            std::string lowerEntry = m_gameInfo->roms[i].filename;
            std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), ::tolower);
            
            if (lowerFilename == lowerEntry) {
                inDatabase = true;
                break;
            }
        }
        if (!inDatabase) {
            invalidROMs.push_back(pair.first);
        }
    }
    
    // Report errors
    if (!missingROMs.empty()) {
        std::cerr << "Error: Missing required ROM files:" << std::endl;
        for (const auto& filename : missingROMs) {
            std::cerr << "  - " << filename << std::endl;
        }
        return false;
    }
    
    if (!invalidROMs.empty()) {
        std::cerr << "Warning: Unknown ROM files in ZIP (will be ignored):" << std::endl;
        for (const auto& filename : invalidROMs) {
            std::cerr << "  - " << filename << std::endl;
        }
    }
    
    // Update sizes
    m_programRomSize = static_cast<u32>(m_programRom.size());
    m_graphicsRomSize = static_cast<u32>(m_graphicsRom.size());
    m_soundRomSize = static_cast<u32>(m_soundRom.size());
    
    byteswapProgramROM();
    
    return true;
}

void Cartridge::byteswapProgramROM() {
    // CPS1 program ROMs are stored in an interleaved format
    // In the ROM files, bytes are swapped within each 16-bit word
    // We need to swap them back to get the correct big-endian format for 68000
    for (u32 i = 0; i < m_programRomSize - 1; i += 2) {
        std::swap(m_programRom[i], m_programRom[i + 1]);
    }
}

void Cartridge::reset() {
    // ROM data doesn't change, so no reset needed
}

u8 Cartridge::readROM8(u32 address) {
    if (address < m_programRomSize) {
        return m_programRom[address];
    }
    return 0;
}

u16 Cartridge::readROM16(u32 address) {
    // CPS1 ROMs are big-endian
    u16 high = readROM8(address);
    u16 low = readROM8(address + 1);
    return (high << 8) | low;
}

u32 Cartridge::readROM32(u32 address) {
    u32 high = readROM16(address);
    u32 low = readROM16(address + 2);
    return (high << 16) | low;
}

u8 Cartridge::readGraphicsROM8(u32 address) const {
    if (address < m_graphicsRomSize) {
        return m_graphicsRom[address];
    }
    return 0;
}

u16 Cartridge::readGraphicsROM16(u32 address) const {
    // Graphics ROMs are typically byte-swapped or have specific endianness
    // For now, assume big-endian like program ROMs
    u16 high = readGraphicsROM8(address);
    u16 low = readGraphicsROM8(address + 1);
    return (high << 8) | low;
}

u32 Cartridge::readGraphicsROM32(u32 address) const {
    u32 high = readGraphicsROM16(address);
    u32 low = readGraphicsROM16(address + 2);
    return (high << 16) | low;
}

u8 Cartridge::readSoundROM8(u16 address) const {
    if (address < m_soundRomSize) {
        return m_soundRom[address];
    }
    return 0;
}

u16 Cartridge::readSoundROM16(u16 address) const {
    // Sound ROMs are typically little-endian (Z80)
    u16 low = readSoundROM8(address);
    u16 high = readSoundROM8(address + 1);
    return (high << 8) | low;
}

void Cartridge::saveState(std::ofstream& file) {
    // ROM doesn't change, so we don't need to save it
    // Just save the title, ROM set name, and sizes for verification
    u32 titleLen = static_cast<u32>(m_title.length());
    file.write(reinterpret_cast<const char*>(&titleLen), sizeof(titleLen));
    file.write(m_title.c_str(), titleLen);
    
    u32 romSetNameLen = static_cast<u32>(m_romSetName.length());
    file.write(reinterpret_cast<const char*>(&romSetNameLen), sizeof(romSetNameLen));
    file.write(m_romSetName.c_str(), romSetNameLen);
    
    file.write(reinterpret_cast<const char*>(&m_programRomSize), sizeof(m_programRomSize));
    file.write(reinterpret_cast<const char*>(&m_graphicsRomSize), sizeof(m_graphicsRomSize));
    file.write(reinterpret_cast<const char*>(&m_soundRomSize), sizeof(m_soundRomSize));
    file.write(reinterpret_cast<const char*>(&m_programByteswap), sizeof(m_programByteswap));
}

void Cartridge::loadState(std::ifstream& file) {
    u32 titleLen;
    file.read(reinterpret_cast<char*>(&titleLen), sizeof(titleLen));
    m_title.resize(titleLen);
    file.read(&m_title[0], titleLen);
    
    u32 romSetNameLen;
    file.read(reinterpret_cast<char*>(&romSetNameLen), sizeof(romSetNameLen));
    m_romSetName.resize(romSetNameLen);
    file.read(&m_romSetName[0], romSetNameLen);
    
    file.read(reinterpret_cast<char*>(&m_programRomSize), sizeof(m_programRomSize));
    file.read(reinterpret_cast<char*>(&m_graphicsRomSize), sizeof(m_graphicsRomSize));
    file.read(reinterpret_cast<char*>(&m_soundRomSize), sizeof(m_soundRomSize));
    file.read(reinterpret_cast<char*>(&m_programByteswap), sizeof(m_programByteswap));
}

} // namespace cps1
