#include "cartridge.h"
#include "../cpu.h"
#include "ppu.h"
#include "zip_reader.h"
#include "db.h"
#include "decrypt.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <cstring>

namespace cps2 {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_title("Unknown CPS2 Game")
    , m_romSetName("")
    , m_gameInfo(nullptr)
    , m_programRomSize(0)
    , m_graphicsRomSize(0)
    , m_soundRomSize(0)
    , m_decryptKey{0, 0, 0, 0}
    , m_decryptStart(0)
    , m_decryptEnd(0)
    , m_watchdogOpcode(0) {
}

void Cartridge::setPPU(cps::PPUBase* ppu) {
    m_ppu = static_cast<PPU*>(ppu);
}

bool Cartridge::load(const fs::path& filename) {
    // Check if it's a ZIP file
    fs::path ext = filename.extension();
    if (ext != ".zip") {
        std::cerr << "CPS2 ROMs must be in ZIP format" << std::endl;
        return false;
    }
    
    // Extract ROM set name from filename (without .zip extension)
    m_romSetName = filename.stem().string();
    
    // Look up game in database
    m_gameInfo = GameDatabase::findGame(m_romSetName);
    if (!m_gameInfo) {
        std::cerr << "Unsupported CPS2 game: " << m_romSetName << std::endl;
        std::cerr << "Only games in the database are supported." << std::endl;
        return false;
    }
    
    m_title = m_gameInfo->name;
    
    // Open and extract ZIP file
    util::ZipReader zip;
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
    
    // After loading, ensure ROMs are properly set up
    // The CPU and PPU will access ROMs through their respective interfaces
    
    // Log successful load summary
    std::cout << "Loaded CPS2 ROM: " << m_title << std::endl;
    std::cout << "  ROM Set: " << m_romSetName << std::endl;
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
    m_programRomEncrypted.clear();
    m_graphicsRom.clear();
    m_soundProgramRom.clear();
    m_soundSampleRom.clear();
    
    // Track which ROMs we've found
    std::vector<bool> foundROMs(m_gameInfo->romCount, false);
    std::vector<std::string> missingROMs;
    std::vector<std::string> invalidROMs;
    
    // Load ROMs in database order
    for (u32 i = 0; i < m_gameInfo->romCount; i++) {
        const ROMEntry& entry = m_gameInfo->roms[i];
        
        // Handle encryption keys separately
        if (entry.type == ROMType::ENCRYPTION_KEY) {
            // Find and load encryption key from ROM file
            for (const auto& pair : romFiles) {
                if (GameDatabase::validateROM(pair.first, pair.second, entry)) {
                    std::cout << "Loading encryption key: " << entry.filename << std::endl;
                    // CPS2 encryption keys are 20 bytes (0x14) with bit-scrambling
                    if (pair.second.size() >= 20) {
                        // The key file contains 160 bits (20 bytes) that need to be unscrambled
                        u16 decoded[10];
                        std::memset(decoded, 0, sizeof(decoded));
                        
                        // Scramble bits: for each output bit b (0-159), find source bit
                        for (s32 b = 0; b < 10 * 16; b++) {
                            s32 bit = (317 - b) % 160;  // Source bit position in scrambled data
                            // Read bit from scrambled data
                            u8 byte_idx = static_cast<u8>(bit / 8);
                            u8 bit_pos = static_cast<u8>((bit ^ 7) % 8);
                            if ((pair.second[byte_idx] >> bit_pos) & 1) {
                                decoded[b / 16] |= (0x8000 >> (b % 16));
                            }
                        }
                        
                        // Extract 64-bit key from decoded words
                        m_decryptKey[0] = (static_cast<u32>(decoded[0]) << 16) | decoded[1];
                        m_decryptKey[1] = (static_cast<u32>(decoded[2]) << 16) | decoded[3];
                        
                        // Store second 64-bit key (not used for decryption but stored)
                        m_decryptKey[2] = (static_cast<u32>(decoded[4]) << 16) | decoded[5];
                        m_decryptKey[3] = (static_cast<u32>(decoded[6]) << 16) | decoded[7];
                        
                        // Extract decrypt range from decoded[9]
                        if (decoded[9] == 0xffff) {
                            m_decryptStart = 0xff0000;
                            m_decryptEnd = 0xffffff;
                        } else {
                            m_decryptStart = 0;
                            m_decryptEnd = (((~decoded[9] & 0x3ff) << 14) | 0x3fff) + 1;
                        }
                        
                        // Watchdog opcode would be in decoded[8] if needed
                        // For now, we'll extract it from the database if available
                    }
                    foundROMs[i] = true;
                    break;
                }
            }
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
                        std::cout << "Loading program: " << entry.filename << std::endl;
                        m_programRomEncrypted.insert(m_programRomEncrypted.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::GRAPHICS:
                        std::cout << "Loading graphics: " << entry.filename << std::endl;
                        m_graphicsRom.insert(m_graphicsRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::SOUND_PROGRAM:
                        std::cout << "Loading sound program: " << entry.filename << std::endl;
                        m_soundProgramRom.insert(m_soundProgramRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::SOUND_SAMPLE:
                        std::cout << "Loading sound sample: " << entry.filename << std::endl;
                        m_soundSampleRom.insert(m_soundSampleRom.end(), pair.second.begin(), pair.second.end());
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
    
    // Check for missing required ROMs
    if (!missingROMs.empty()) {
        std::cerr << "Error: Missing required ROMs:" << std::endl;
        for (const auto& rom : missingROMs) {
            std::cerr << "  " << rom << std::endl;
        }
        return false;
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
    
    if (!invalidROMs.empty()) {
        std::cerr << "Warning: Unknown ROM files in ZIP (ignored):" << std::endl;
        for (const auto& rom : invalidROMs) {
            std::cerr << "  " << rom << std::endl;
        }
    }
    
    // Set sizes
    m_programRomSize = static_cast<u32>(m_programRomEncrypted.size());
    m_graphicsRomSize = static_cast<u32>(m_graphicsRom.size());
    m_soundRomSize = static_cast<u32>(m_soundProgramRom.size());
    
    // Load decryption info from game database (as fallback if key file not found)
    // The key file takes precedence if loaded above
    if (m_decryptKey[0] == 0 && m_decryptKey[1] == 0 && m_decryptKey[2] == 0 && m_decryptKey[3] == 0) {
        // Copy keys directly from database (already stored as u32[4])
        m_decryptKey[0] = m_gameInfo->decryptKey[0];
        m_decryptKey[1] = m_gameInfo->decryptKey[1];
        m_decryptKey[2] = m_gameInfo->decryptKey[2];
        m_decryptKey[3] = m_gameInfo->decryptKey[3];
    }
    m_decryptStart = m_gameInfo->decryptStart;
    m_decryptEnd = m_gameInfo->decryptEnd;
    
    // Decrypt program ROM (must be done before byteswap)
    // The decryption algorithm expects ROM in a specific format
    decryptProgramROM();
    
    // Byteswap program ROM (CPS2 uses big-endian for 68000)
    // This converts from little-endian (as stored in ZIP) to big-endian (for CPU)
    byteswapProgramROM();
    
    // Verify ROM sizes are reasonable
    if (m_programRomSize == 0) {
        std::cerr << "Error: No program ROM loaded" << std::endl;
        return false;
    }
    
    if (m_graphicsRomSize == 0) {
        std::cerr << "Warning: No graphics ROM loaded" << std::endl;
    }
    
    // Verify program ROM size is reasonable (CPS2 games typically have 256KB-4MB)
    if (m_programRomSize < 64 * 1024) {
        std::cerr << "Warning: Program ROM size seems too small: " << m_programRomSize << " bytes" << std::endl;
    }
    if (m_programRomSize > 4 * 1024 * 1024) {
        std::cerr << "Warning: Program ROM size exceeds expected maximum: " << m_programRomSize << " bytes" << std::endl;
    }
    
    return true;
}

void Cartridge::decryptProgramROM() {
    // Allocate output buffer for decrypted ROM
    m_programRom.resize(m_programRomEncrypted.size());
    
    // Check if we have valid decryption keys
    if (m_decryptKey[0] == 0 && m_decryptKey[1] == 0 && m_decryptKey[2] == 0 && m_decryptKey[3] == 0) {
        std::cerr << "Warning: No decryption keys found. ROM may not work correctly." << std::endl;
        // Copy encrypted ROM as-is
        m_programRom = m_programRomEncrypted;
        return;
    }
    
    // Use first 64-bit key (m_decryptKey[0-1]) as master key for decryption
    // CPS2 uses a single 64-bit master key (stored as 2x32-bit values)
    // Note: m_decryptKey[2-3] (second 64-bit key) is stored but NOT used for decryption
    u32 master_key[2];
    master_key[0] = m_decryptKey[0];
    master_key[1] = m_decryptKey[1];
    
    // Print decryption key for verification
    std::cout << "Decrypting CPS2 ROM..." << std::endl;
    
    // Convert address limits from bytes to 16-bit words
    u32 lower_limit = m_decryptStart / 2;
    u32 upper_limit = m_decryptEnd / 2;
    
    // If decrypt range not set, use default (entire ROM)
    if (m_decryptStart == 0 && m_decryptEnd == 0) {
        lower_limit = 0;
        upper_limit = static_cast<u32>(m_programRomEncrypted.size() / 2);
    }
    
    // Perform decryption
    decryptCPS2(master_key, lower_limit, upper_limit,
                m_programRomEncrypted.data(), m_programRom.data(),
                static_cast<u32>(m_programRomEncrypted.size()));
}

void Cartridge::byteswapProgramROM() {
    // CPS2 program ROMs are stored in big-endian format
    // Swap bytes for each 16-bit word
    for (size_t i = 0; i < m_programRom.size() - 1; i += 2) {
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
    if (address < m_soundProgramRom.size()) {
        return m_soundProgramRom[address];
    }
    return 0;
}

u16 Cartridge::readSoundROM16(u16 address) const {
    // Sound ROMs are typically little-endian (Z80)
    u16 low = readSoundROM8(address);
    u16 high = readSoundROM8(address + 1);
    return (high << 8) | low;
}

// ============================================================================
// Save/Load State
// ============================================================================

void Cartridge::saveState(std::ofstream& file) {
    // Save title and ROM set name for verification
    u32 titleLen = static_cast<u32>(m_title.length());
    file.write(reinterpret_cast<const char*>(&titleLen), sizeof(titleLen));
    file.write(m_title.c_str(), titleLen);
    
    u32 romSetNameLen = static_cast<u32>(m_romSetName.length());
    file.write(reinterpret_cast<const char*>(&romSetNameLen), sizeof(romSetNameLen));
    file.write(m_romSetName.c_str(), romSetNameLen);
    
    // Save ROM sizes for verification
    file.write(reinterpret_cast<const char*>(&m_programRomSize), sizeof(m_programRomSize));
    file.write(reinterpret_cast<const char*>(&m_graphicsRomSize), sizeof(m_graphicsRomSize));
    file.write(reinterpret_cast<const char*>(&m_soundRomSize), sizeof(m_soundRomSize));
    
    // Note: We don't save ROM contents as they're read-only
    // But we save decryption keys for verification
    file.write(reinterpret_cast<const char*>(&m_decryptKey), sizeof(m_decryptKey));
    file.write(reinterpret_cast<const char*>(&m_decryptStart), sizeof(m_decryptStart));
    file.write(reinterpret_cast<const char*>(&m_decryptEnd), sizeof(m_decryptEnd));
}

void Cartridge::loadState(std::ifstream& file) {
    // Load title and ROM set name for verification
    u32 titleLen;
    file.read(reinterpret_cast<char*>(&titleLen), sizeof(titleLen));
    m_title.resize(titleLen);
    file.read(&m_title[0], titleLen);
    
    u32 romSetNameLen;
    file.read(reinterpret_cast<char*>(&romSetNameLen), sizeof(romSetNameLen));
    m_romSetName.resize(romSetNameLen);
    file.read(&m_romSetName[0], romSetNameLen);
    
    // Load ROM sizes for verification
    file.read(reinterpret_cast<char*>(&m_programRomSize), sizeof(m_programRomSize));
    file.read(reinterpret_cast<char*>(&m_graphicsRomSize), sizeof(m_graphicsRomSize));
    file.read(reinterpret_cast<char*>(&m_soundRomSize), sizeof(m_soundRomSize));
    
    // Load decryption keys
    file.read(reinterpret_cast<char*>(&m_decryptKey), sizeof(m_decryptKey));
    file.read(reinterpret_cast<char*>(&m_decryptStart), sizeof(m_decryptStart));
    file.read(reinterpret_cast<char*>(&m_decryptEnd), sizeof(m_decryptEnd));
    
    // Verify ROM sizes match (sanity check)
    if (m_programRomSize != static_cast<u32>(m_programRom.size()) ||
        m_graphicsRomSize != static_cast<u32>(m_graphicsRom.size()) ||
        m_soundRomSize != static_cast<u32>(m_soundProgramRom.size())) {
        std::cerr << "Warning: ROM sizes in save state don't match current ROM" << std::endl;
    }
}

} // namespace cps2
