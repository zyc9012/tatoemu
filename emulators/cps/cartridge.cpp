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
    initSepTable();
}

void Cartridge::initSepTable() {
    // Lambda to separate bits: ABCDEFGH -> A00B00C00D00E00F00G00H00
    auto separate = [](u32 b) -> u32 {
        u32 a = b;                                       // 00000000 00000000 00000000 ABCDEFGH
        a = ((a & 0x000000F0) << 12) | (a & 0x0000000F); // 00000000 0000ABCD 00000000 0000EFGH
        a = ((a & 0x000C000C) <<  6) | (a & 0x00030003); // 00000AB0 00000CD0 00000EF0 00000GH0
        a = ((a & 0x02020202) <<  3) | (a & 0x01010101); // 000A000B 000C000D 000E000F 000G000H
        return a;
    };
    
    for (int i = 0; i < 256; i++) {
        m_sepTable[i] = separate(255 - i);  // Inverted for CPS palette indexing
    }
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
    m_decodedGraphicsRom.clear();
    m_soundProgramRom.clear();
    m_soundProgramRomEncrypted.clear();
    m_soundSampleRom.clear();
    
    // Track which ROMs we've found
    std::vector<bool> foundROMs(m_gameInfo->romCount, false);
    std::vector<std::string> missingROMs;
    std::vector<std::string> invalidROMs;
    
    // CPS1-specific: Temporary storage for program ROMs (need to interleave them)
    std::vector<std::vector<u8>> programRomChips;
    std::vector<bool> programRomNeedsInterleave;
    
    // Track graphics ROM sizes for CPS1 decoding (need to check if ROMs < 0x80000)
    std::vector<u32> graphicsRomSizes;
    
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
                        graphicsRomSizes.push_back(entry.size);
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
                for (size_t j = 0; j < evenChip.size(); j++) {
                    m_programRom[offset++] = oddChip[j];
                    m_programRom[offset++] = evenChip[j];
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
    
    if (m_cpsVer == 1) {
        if (isCPS1QSound()) {
            // Save the original encrypted sound program ROM
            m_soundProgramRomEncrypted.resize(m_soundProgramRom.size());
            std::memcpy(m_soundProgramRomEncrypted.data(), m_soundProgramRom.data(), m_soundProgramRom.size());
            // Double the size of the sound program ROM for decryption
            m_soundProgramRom.resize(m_soundProgramRom.size() * 2);
            m_soundProgramRomSize = static_cast<u32>(m_soundProgramRom.size());
            // Decrypt CPS1 sound program ROM
            std::cout << "Decrypting sound program ROM..." << std::endl;
            decryptCPS1SoundProgramROM();
        }
    } else {
        // Decrypt CPS2 program ROM (must be done before byteswap)
        decryptCPS2ProgramROM();
    }
    
    // Byteswap program ROM (both CPS1 and CPS2 use big-endian for 68000)
    byteswapProgramROM();
    
    // Byteswap sound sample ROM for CPS2
    if (m_cpsVer == 2) {
        byteswapSoundSampleROM();
    }
    
    // Decode graphics ROM (CPS1 and CPS2 use different decoding methods)
    decodeGraphicsROM(graphicsRomSizes);
    if (m_ppu) {
        m_ppu->setDecodedGraphics(m_decodedGraphicsRom);
    }
    
    return true;
}

void Cartridge::decryptCPS1SoundProgramROM() {
    if (m_cpsVer != 1 || !m_gameInfo) {
        return;
    }

    const CPS1DecryptKeys* k = m_gameInfo->cps1DecKeys;
    if (!k) {
        return;
    }

    decryptCPS1(k->swapKey1, k->swapKey2, k->addrKey, k->xorKey,
                    m_soundProgramRom.data(), m_soundProgramRom.size());
}

void Cartridge::decryptCPS2ProgramROM() {
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

void Cartridge::byteswapSoundSampleROM() {
    // CPS2 QSound sample ROMs need byteswapping
    // Swap bytes for each 16-bit word
    for (size_t i = 0; i < m_soundSampleRom.size() - 1; i += 2) {
        std::swap(m_soundSampleRom[i], m_soundSampleRom[i + 1]);
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

u8 Cartridge::readSoundROM8(u32 address) const {
    if (address < m_soundProgramRom.size()) {
        return m_soundProgramRom[address];
    }
    return 0;
}

u8 Cartridge::readEncryptedSoundROM8(u32 address) const {
    if (address < m_soundProgramRomEncrypted.size()) {
        return m_soundProgramRomEncrypted[address];
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

void Cartridge::decodeGraphicsROM(const std::vector<u32>& graphicsRomSizes) {
    if (m_graphicsRom.empty()) {
        std::cerr << "Cartridge: No graphics ROM data to decode" << std::endl;
        return;
    }

    std::cout << "Decoding graphics ROM..." << std::endl;
    
    if (m_cpsVer == 1) {
        decodeGraphicsROMCPS1(graphicsRomSizes);
    } else {
        decodeGraphicsROMCPS2(graphicsRomSizes);
    }
}

void Cartridge::decodeGraphicsROMCPS1(const std::vector<u32>& graphicsRomSizes) {
    u32 srcSize = static_cast<u32>(m_graphicsRom.size());
    
    // Output size: same as input (1:1 ratio)
    u32 dstSize = srcSize;
    m_decodedGraphicsRom.resize(dstSize, 0);
    
    // Helper lambda: Process a single ROM in byte mode (for small ROMs < 512KB)
    auto processByteModeRom = [this, srcSize](u32 romOffset, u32 romSize, u32 outputBase, u32 bitShift, u32 halfOffset) {
        for (u32 i = 0; i < romSize && (romOffset + i) < srcSize; i++) {
            u32 outputOffset = outputBase + (i * 8) + halfOffset;
            u8 byteValue = m_graphicsRom[romOffset + i];
            u32 pixelData = m_sepTable[byteValue];
            pixelData <<= bitShift;
            if (outputOffset < m_decodedGraphicsRom.size()) {
                *reinterpret_cast<u32*>(m_decodedGraphicsRom.data() + outputOffset) |= pixelData;
            }
        }
    };
    
    // Helper lambda: Process a byte pair from a ROM in word mode (for standard ROMs >= 512KB)
    auto processWordModeBytePair = [this, srcSize](u32 romOffset, u32 byteIndex, u32 outputOffset, u32 bitShift) {
        if (romOffset + byteIndex + 1 < srcSize) {
            u8 byte0 = m_graphicsRom[romOffset + byteIndex];
            u8 byte1 = m_graphicsRom[romOffset + byteIndex + 1];
            u32 pixelData = m_sepTable[byte0] | (m_sepTable[byte1] << 1);
            pixelData <<= bitShift;
            if (outputOffset < m_decodedGraphicsRom.size()) {
                *reinterpret_cast<u32*>(m_decodedGraphicsRom.data() + outputOffset) |= pixelData;
            }
        }
    };
    
    // Process graphics ROMs in groups, checking size of first ROM in each group
    u32 offset = 0;
    u32 romIndex = 0;
    
    while (offset < srcSize && romIndex < graphicsRomSizes.size()) {
        u32 outputBase = offset;
        u32 firstRomSize = graphicsRomSizes[romIndex];
        
        // Check size of first ROM in this group to determine decoding method
        if (firstRomSize < 0x80000) {
            // Small ROMs (< 512KB): Each ROM provides 1 bit per pixel, requiring 8 ROMs instead of 4
            if (romIndex + 7 < graphicsRomSizes.size()) {
                // Left half: ROM 0-3 (shifts 0, 1, 2, 3)
                for (u32 shift = 0; shift < 4; shift++) {
                    u32 romSize = graphicsRomSizes[romIndex];
                    processByteModeRom(offset, romSize, outputBase, shift, 0);
                    offset += romSize;
                    romIndex++;
                }
                
                // Right half: ROM 4-7 (shifts 0, 1, 2, 3)
                for (u32 shift = 0; shift < 4; shift++) {
                    u32 romSize = graphicsRomSizes[romIndex];
                    processByteModeRom(offset, romSize, outputBase, shift, 4);
                    offset += romSize;
                    romIndex++;
                }
            } else {
                // Not enough ROMs for byte mode, break
                break;
            }
        } else {
            // Standard ROMs (>= 512KB): Process 4 ROMs for word mode
            if (romIndex + 3 < graphicsRomSizes.size()) {
                u32 rom0Size = graphicsRomSizes[romIndex];
                u32 rom1Size = graphicsRomSizes[romIndex + 1];
                u32 rom2Size = graphicsRomSizes[romIndex + 2];
                u32 rom3Size = graphicsRomSizes[romIndex + 3];
                
                // Use the minimum size to ensure we don't overrun
                u32 romChipSize = std::min({rom0Size, rom1Size, rom2Size, rom3Size});
                
                u32 rom0Offset = offset;
                u32 rom1Offset = offset + rom0Size;
                u32 rom2Offset = offset + rom0Size + rom1Size;
                u32 rom3Offset = offset + rom0Size + rom1Size + rom2Size;
                
                // Process each byte pair
                for (u32 i = 0; i < romChipSize && (rom0Offset + i + 1) < srcSize; i += 2) {
                    u32 outputOffset = outputBase + (i / 2) * 8;  // 8-byte stride
                    
                    // Left half (offset 0): Process ROM 0 then OR ROM 1
                    processWordModeBytePair(rom0Offset, i, outputOffset, 0);
                    processWordModeBytePair(rom1Offset, i, outputOffset, 2);
                    
                    // Right half (offset 4): Process ROM 2 then OR ROM 3
                    processWordModeBytePair(rom2Offset, i, outputOffset + 4, 0);
                    processWordModeBytePair(rom3Offset, i, outputOffset + 4, 2);
                }
                
                // Advance offset and romIndex for the 4 ROMs processed
                offset += rom0Size + rom1Size + rom2Size + rom3Size;
                romIndex += 4;
            } else {
                // Not enough ROMs for word mode, break
                break;
            }
        }
    }
}

void Cartridge::decodeGraphicsROMCPS2(const std::vector<u32>& graphicsRomSizes) {
    // Output size calculation:
    // For standard 0x80000 ROMs: 1 section → 0x200000 bytes = 4x input
    u32 dstSize = 0;
    for (u32 i = 0; i < graphicsRomSizes.size(); i += 4) {
        u32 maxRomSize = 0;
        for (u32 j = 0; j < 4 && (i + j) < graphicsRomSizes.size(); j++) {
            if (graphicsRomSizes[i + j] > maxRomSize) {
                maxRomSize = graphicsRomSizes[i + j];
            }
        }
        // Each section (0x80000) produces 0x200000 bytes
        u32 numSections = maxRomSize >> 19;
        if (numSections == 0 && maxRomSize > 0) numSections = 1;
        dstSize += numSections * 0x200000;
    }
    
    m_decodedGraphicsRom.resize(dstSize, 0);
    
    // Process graphics in groups of 4 ROMs
    if (graphicsRomSizes.size() < 4) {
        std::cerr << "Error: CPS2 requires at least 4 graphics ROMs" << std::endl;
        return;
    }
    
    // Process a single CPS2 ROM section
    auto processCps2Section100000 = [this](u8* tileOutput, const u8* romSource, u32 bitShift) {
        u8* tileEnd = tileOutput + 0x100000;
        
        do {
            u32 pixelData = m_sepTable[romSource[0]] | (m_sepTable[romSource[1]] << 1);
            pixelData <<= bitShift;
            *reinterpret_cast<u32*>(tileOutput) |= pixelData;
            
            tileOutput += 8;
            romSource += 4;
        } while (tileOutput < tileEnd);
    };
    
    // Process a single CPS2 ROM
    auto processCps2Rom = [processCps2Section100000](u8* tileOutput, const u8* romData, u32 romLength, u32 bitShift) {
        u8* currentTileOutput = tileOutput;
        const u8* currentRomData = romData;
        
        // Process in sections: for each 0x80000 section of ROM, create two 0x100000 sections of output
        u32 numSections = romLength >> 19;
        for (u32 sectionIndex = 0; sectionIndex < numSections; sectionIndex++) {
            // First 0x100000 section: read from current ROM position
            processCps2Section100000(currentTileOutput, currentRomData, bitShift);
            currentTileOutput += 0x100000;
            
            // Second 0x100000 section: read from current ROM position + 2 bytes
            processCps2Section100000(currentTileOutput, currentRomData + 2, bitShift);
            currentTileOutput += 0x100000;
            
            // Advance to next 512KB section of ROM
            currentRomData += 0x80000;
        }
    };
    
    // Process each group of 4 ROMs
    u32 romIndex = 0;
    u32 outOffset = 0;
    
    while (romIndex + 3 < graphicsRomSizes.size()) {
        u32 rom0Size = graphicsRomSizes[romIndex];
        u32 rom1Size = graphicsRomSizes[romIndex + 1];
        u32 rom2Size = graphicsRomSizes[romIndex + 2];
        u32 rom3Size = graphicsRomSizes[romIndex + 3];
        
        // Calculate ROM offsets in m_graphicsRom
        u32 rom0Offset = 0;
        for (u32 i = 0; i < romIndex; i++) {
            rom0Offset += graphicsRomSizes[i];
        }
        u32 rom1Offset = rom0Offset + rom0Size;
        u32 rom2Offset = rom1Offset + rom1Size;
        u32 rom3Offset = rom2Offset + rom2Size;
        
        // Process ROM 0: left half, shift 0
        processCps2Rom(m_decodedGraphicsRom.data() + outOffset, 
                      m_graphicsRom.data() + rom0Offset, rom0Size, 0);
        
        // Process ROM 1: left half, shift 2
        processCps2Rom(m_decodedGraphicsRom.data() + outOffset, 
                      m_graphicsRom.data() + rom1Offset, rom1Size, 2);
        
        // Process ROM 2: right half, shift 0
        processCps2Rom(m_decodedGraphicsRom.data() + outOffset + 4, 
                      m_graphicsRom.data() + rom2Offset, rom2Size, 0);
        
        // Process ROM 3: right half, shift 2
        processCps2Rom(m_decodedGraphicsRom.data() + outOffset + 4, 
                      m_graphicsRom.data() + rom3Offset, rom3Size, 2);
        
        // Advance output offset: each 0x80000 ROM produces 0x200000 bytes of output
        u32 maxRomSize = std::max({rom0Size, rom1Size, rom2Size, rom3Size});
        u32 numSections = maxRomSize >> 19;
        if (numSections == 0 && maxRomSize > 0) numSections = 1;
        outOffset += numSections * 0x200000;
        romIndex += 4;
    }
}

} // namespace cps
