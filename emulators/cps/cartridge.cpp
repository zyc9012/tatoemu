#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
#include "db.h"
#include "decrypt.h"
#include "zip_reader.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <cstring>

namespace cps {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_cpsVer(1)
    , m_title("Unknown CPS Game")
    , m_romSetName("")
    , m_gameInfo(nullptr)
    , m_programRomSize(0)
    , m_graphicsRomSize(0)
    , m_soundProgramRomSize(0)
    , m_soundSampleRomSize(0)
    , m_decryptKey{0, 0, 0, 0}
    , m_decryptStart(0)
    , m_decryptEnd(0)
    , m_watchdogOpcode(0) {
}

void Cartridge::setPPU(PPU* ppu) {
    m_ppu = ppu;
}

BoardConfig Cartridge::getBoardConfig() const {
    return GameDatabase::getBoardConfig(m_gameInfo->board);
}

CPSBoard Cartridge::getBoardType() const {
    return m_gameInfo->board;
}

CPSMapper Cartridge::getMapper() const {
    return m_gameInfo->mapper;
}

bool Cartridge::load(const fs::path& filename) {
    // Check if it's a ZIP file
    fs::path ext = filename.extension();
    if (ext != ".zip") {
        std::cerr << "CPS ROMs must be in ZIP format" << std::endl;
        return false;
    }
    
    // Extract ROM set name from filename (without .zip extension)
    m_romSetName = filename.stem().string();
    
    // Look up game in unified database
    const GameInfo* gameInfo = GameDatabase::findGame(m_romSetName);
    if (!gameInfo) {
        std::cerr << "Unsupported CPS game: " << m_romSetName << std::endl;
        std::cerr << "Only games in the database are supported." << std::endl;
        return false;
    }
    
    m_gameInfo = gameInfo;
    m_title = m_gameInfo->name;
    m_cpsVer = m_gameInfo->cpsVer;  // Set CPS version from database
    
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
    
    std::cout << (m_cpsVer == 2 ? "Loaded CPS2 ROM: " : "Loaded CPS1 ROM: ") << m_title << std::endl;
    std::cout << "  ROM Set: " << m_romSetName << std::endl;
    std::cout << "  Program ROM: " << (m_programRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Graphics ROM: " << (m_graphicsRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Sound Program ROM: " << (m_soundProgramRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Sound Sample ROM: " << (m_soundSampleRomSize / 1024) << " KB" << std::endl;
    if (m_cpsVer == 1) {
        std::cout << "  Board Type: " << static_cast<int>(m_gameInfo->board) << std::endl;
        std::cout << "  Graphics Mapper: " << static_cast<int>(m_gameInfo->mapper) << std::endl;
    }
    
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
    
    // CPS1-specific: Temporary storage for program ROMs (need to interleave them)
    std::vector<std::vector<u8>> programRomChips;
    std::vector<bool> programRomNeedsInterleave;
    
    // Load ROMs in database order
    for (u32 i = 0; i < m_gameInfo->romCount; i++) {
        const ROMEntry& entry = m_gameInfo->roms[i];
        
        // CPS2-specific: Handle encryption keys separately
        if (m_cpsVer == 2 && entry.type == ROMType::ENCRYPTION_KEY) {
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
                    }
                    foundROMs[i] = true;
                    break;
                }
            }
            continue;
        }
        
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
                        std::cout << "Loading program: " << entry.filename << std::endl;
                        if (m_cpsVer == 1) {
                            // CPS1: Store program ROMs separately for interleaving
                            programRomChips.push_back(pair.second);
                            programRomNeedsInterleave.push_back(entry.flags & ROM_FLAG_INTERLEAVE);
                        } else {
                            // CPS2: Store encrypted program ROMs directly
                            m_programRomEncrypted.insert(m_programRomEncrypted.end(), pair.second.begin(), pair.second.end());
                        }
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
    
    // Check for missing required ROMs
    if (!missingROMs.empty()) {
        std::cerr << "Error: Missing required ROM files:" << std::endl;
        for (const auto& filename : missingROMs) {
            std::cerr << "  - " << filename << std::endl;
        }
        return false;
    }
    
    // CPS1-specific: Handle program ROM interleaving
    if (m_cpsVer == 1 && !programRomChips.empty()) {
        // Calculate total size
        u32 totalSize = 0;
        for (const auto& chip : programRomChips) {
            totalSize += static_cast<u32>(chip.size());
        }
        m_programRom.resize(totalSize);
        
        // Process each ROM chip based on its interleave flag
        u32 offset = 0;
        for (size_t i = 0; i < programRomChips.size(); i++) {
            bool needsInterleaving = programRomNeedsInterleave[i];
            
            if (needsInterleaving) {
                // This ROM needs interleaving - it should be part of a pair
                // Check if we have a pair (even/odd)
                if (i + 1 >= programRomChips.size()) {
                    std::cerr << "Error: ROM chip at index " << i << " needs interleaving but has no pair" << std::endl;
                    return false;
                }
                
                // Verify the pair also needs interleaving
                if (!programRomNeedsInterleave[i + 1]) {
                    std::cerr << "Error: ROM chip at index " << i << " needs interleaving but its pair doesn't" << std::endl;
                    return false;
                }
                
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
                
                // Skip the next chip since we've already processed it as part of the pair
                i++;
            } else {
                // Simple concatenation
                const auto& chip = programRomChips[i];
                std::memcpy(&m_programRom[offset], chip.data(), chip.size());
                offset += static_cast<u32>(chip.size());
            }
        }
    }
    
    // Update sizes
    if (m_cpsVer == 1) {
        m_programRomSize = static_cast<u32>(m_programRom.size());
    } else {
        m_programRomSize = static_cast<u32>(m_programRomEncrypted.size());
    }
    m_graphicsRomSize = static_cast<u32>(m_graphicsRom.size());
    m_soundProgramRomSize = static_cast<u32>(m_soundProgramRom.size());
    m_soundSampleRomSize = static_cast<u32>(m_soundSampleRom.size());
    
    // CPS2-specific: Decrypt program ROM (must be done before byteswap)
    if (m_cpsVer == 2) {
        decryptProgramROM();
    }
    
    // Byteswap program ROM (both CPS1 and CPS2 use big-endian for 68000)
    byteswapProgramROM();
    
    return true;
}

void Cartridge::decryptProgramROM() {
    // Only for CPS2
    if (m_cpsVer != 2) {
        return;
    }
    
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
    // Both CPS1 and CPS2 program ROMs need byteswapping
    // Swap bytes for each 16-bit word
    for (size_t i = 0; i < m_programRom.size() - 1; i += 2) {
        std::swap(m_programRom[i], m_programRom[i + 1]);
    }
    if (!m_programRomEncrypted.empty()) {
        for (size_t i = 0; i < m_programRomEncrypted.size() - 1; i += 2) {
            std::swap(m_programRomEncrypted[i], m_programRomEncrypted[i + 1]);
        }
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
    // CPS ROMs are big-endian
    u16 high = readROM8(address);
    u16 low = readROM8(address + 1);
    return (high << 8) | low;
}

u8 Cartridge::readEncryptedROM8(u32 address) {
    // For CPS2, read from encrypted ROM (for data reads like exception vectors)
    // For CPS1, encrypted ROM doesn't exist, so use decrypted ROM
    if (m_cpsVer == 2 && address < m_programRomEncrypted.size()) {
        return m_programRomEncrypted[address];
    }
    return readROM8(address);
}

u16 Cartridge::readEncryptedROM16(u32 address) {
    // CPS ROMs are big-endian
    u16 high = readEncryptedROM8(address);
    u16 low = readEncryptedROM8(address + 1);
    return (high << 8) | low;
}

u8 Cartridge::readGraphicsROM8(u32 address) const {
    if (address < m_graphicsRomSize) {
        return m_graphicsRom[address];
    }
    return 0;
}

u8 Cartridge::readSoundROM8(u16 address) const {
    if (address < m_soundProgramRom.size()) {
        return m_soundProgramRom[address];
    }
    return 0;
}

void Cartridge::saveState(std::ofstream& file) {
    // Save CPS version
    file.write(reinterpret_cast<const char*>(&m_cpsVer), sizeof(m_cpsVer));
    
    // Save title and ROM set name for verification
    u32 titleLen = static_cast<u32>(m_title.length());
    file.write(reinterpret_cast<const char*>(&titleLen), sizeof(titleLen));
    file.write(m_title.c_str(), titleLen);
    
    u32 romSetNameLen = static_cast<u32>(m_romSetName.length());
    file.write(reinterpret_cast<const char*>(&romSetNameLen), sizeof(romSetNameLen));
    file.write(m_romSetName.c_str(), romSetNameLen);
    
    // Save decryption keys (CPS2 only, but save for both for compatibility)
    file.write(reinterpret_cast<const char*>(&m_decryptKey), sizeof(m_decryptKey));
    file.write(reinterpret_cast<const char*>(&m_decryptStart), sizeof(m_decryptStart));
    file.write(reinterpret_cast<const char*>(&m_decryptEnd), sizeof(m_decryptEnd));
}

void Cartridge::loadState(std::ifstream& file) {
    // Load CPS version
    file.read(reinterpret_cast<char*>(&m_cpsVer), sizeof(m_cpsVer));
    
    // Load title and ROM set name for verification
    u32 titleLen;
    file.read(reinterpret_cast<char*>(&titleLen), sizeof(titleLen));
    m_title.resize(titleLen);
    file.read(&m_title[0], titleLen);
    
    u32 romSetNameLen;
    file.read(reinterpret_cast<char*>(&romSetNameLen), sizeof(romSetNameLen));
    m_romSetName.resize(romSetNameLen);
    file.read(&m_romSetName[0], romSetNameLen);
    
    // Load decryption keys
    file.read(reinterpret_cast<char*>(&m_decryptKey), sizeof(m_decryptKey));
    file.read(reinterpret_cast<char*>(&m_decryptStart), sizeof(m_decryptStart));
    file.read(reinterpret_cast<char*>(&m_decryptEnd), sizeof(m_decryptEnd));
}

} // namespace cps
