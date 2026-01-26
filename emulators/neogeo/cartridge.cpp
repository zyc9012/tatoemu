#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
#include "db.h"
#include "../../utilities/zip_reader.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <cstring>

namespace neogeo {

Cartridge::Cartridge()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_gameInfo(nullptr)
    , m_biosVectorTableActive(true)  // Start with BIOS vector table active
    , m_programRomSize(0)
    , m_spriteRomSize(0)
    , m_textRomSize(0)
    , m_soundRomSize(0) {
}

bool Cartridge::load(const fs::path& filename, u32 bios68kIndex) {
    // Check if it's a ZIP file
    fs::path ext = filename.extension();
    if (ext != ".zip") {
        std::cerr << "NeoGeo ROMs must be in ZIP format" << std::endl;
        return false;
    }
    
    // Extract ROM set name from filename (without .zip extension)
    m_romSetName = filename.stem().string();
    m_romFilename = filename;

    // Look up game in database
    std::string romSetNameLower = m_romSetName;
    std::transform(romSetNameLower.begin(), romSetNameLower.end(), romSetNameLower.begin(), ::tolower);
    
    m_gameInfo = GameDatabase::findGame(romSetNameLower);
    if (!m_gameInfo) {
        std::cerr << "Unsupported NeoGeo game: " << m_romSetName << std::endl;
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
    
    // Load BIOS ROMs from game ZIP first, then neogeo.zip
    if (!loadBIOSROMs(romFiles, filename, bios68kIndex)) {
        #ifdef __EMSCRIPTEN__
        std::cerr << "Failed to load BIOS ROMs. Upload neogeo.zip and try again." << std::endl;
        #else
        std::cerr << "Failed to load BIOS ROMs. Put neogeo.zip in the same directory as the game ZIP and try again." << std::endl;
        #endif
        return false;
    }
    
    // Load ROMs using game database
    if (!loadROMsFromDatabase(romFiles)) {
        return false;
    }
    
    std::cout << "Loaded NeoGeo ROM: " << m_title << std::endl;
    std::cout << "  ROM Set: " << m_romSetName << std::endl;
    std::cout << "  Program ROM: " << (m_programRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Sprite ROM: " << (m_spriteRomSize / 1024) << " KB" << std::endl;
    std::cout << "  Text ROM: " << (m_textRomSize / 1024) << " KB" << std::endl;
    if (m_soundRomSize > 0) {
        std::cout << "  Sound ROM: " << (m_soundRomSize / 1024) << " KB" << std::endl;
    }
    
    return true;
}

bool Cartridge::loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles) {
    if (!m_gameInfo) {
        return false;
    }
    
    // Clear existing ROMs
    m_programRom.clear();
    m_spriteRom.clear();
    m_textRom.clear();
    m_soundRom.clear();
    m_adpcmRom.clear();
    
    // Track which ROMs we've found
    std::vector<bool> foundROMs(m_gameInfo->romCount, false);
    std::vector<std::string> missingROMs;
    
    // Handle SWAPP flag: swap first half/second half of first program ROM
    bool swappFlag = (m_gameInfo->flags & GAME_FLAG_SWAPP) != 0;
    bool swapcFlag = (m_gameInfo->flags & GAME_FLAG_SWAPC) != 0;
    bool swapvFlag = (m_gameInfo->flags & GAME_FLAG_SWAPV) != 0;
    u32 programRomIndex = 0;

    // Load ROMs in database order
    bool interleaveInProgress = false;
    for (u32 i = 0; i < m_gameInfo->romCount; i++) {
        const ROMEntry& entry = m_gameInfo->roms[i];
        
        // Find the ROM file (case-insensitive)
        bool found = false;
        for (const auto& pair : romFiles) {
            if (GameDatabase::validateROM(pair.first, pair.second, entry)) {
                found = true;
                foundROMs[i] = true;
                
                // Add to appropriate ROM bank
                switch (entry.type) {
                    case ROMType::PROGRAM: {
                        std::cout << "Loading program: " << entry.filename << std::endl;
                        
                        // Normal program ROM loading
                        std::vector<u8> romData = pair.second;
                        
                        // Handle SWAPP: swap first half with second half of first ROM
                        if (swappFlag && programRomIndex == 0) {
                            u32 halfSize = static_cast<u32>(romData.size()) / 2;
                            for (u32 j = 0; j < halfSize; j++) {
                                std::swap(romData[j], romData[j + halfSize]);
                            }
                        }
                        
                        m_programRom.insert(m_programRom.end(), romData.begin(), romData.end());
                        programRomIndex++;
                        break;
                    }
                    case ROMType::TEXT:
                        std::cout << "Loading text: " << entry.filename << std::endl;
                        m_textRom.insert(m_textRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::SPRITE:
                        std::cout << "Loading sprite: " << entry.filename << std::endl;
                        if (!interleaveInProgress) {
                            m_spriteRom.resize(m_spriteRom.size() + pair.second.size() * 2);
                            interleavedCopy(m_spriteRom.end().base() - pair.second.size() * 2, pair.second.data(), pair.second.size());
                        } else {
                            interleavedCopy(m_spriteRom.end().base() - pair.second.size() * 2 + 1, pair.second.data(), pair.second.size());
                        }
                        interleaveInProgress = !interleaveInProgress;
                        break;
                    case ROMType::SOUND_PROGRAM:
                        std::cout << "Loading sound program: " << entry.filename << std::endl;
                        m_soundRom.insert(m_soundRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    case ROMType::SOUND_SAMPLE:
                        std::cout << "Loading sound sample: " << entry.filename << std::endl;
                        m_adpcmRom.insert(m_adpcmRom.end(), pair.second.begin(), pair.second.end());
                        break;
                    default:
                        break;
                }
                break;
            }
        }
        
        if (!found) {
            missingROMs.push_back(entry.filename);
        }
    }
    
    // Check for missing ROMs
    if (!missingROMs.empty()) {
        std::cerr << "Missing ROMs:" << std::endl;
        for (const auto& filename : missingROMs) {
            std::cerr << "  - " << filename << std::endl;
        }
        return false;
    }
    
    // Byteswap program ROM (68000 is big-endian, ROMs stored little-endian)
    byteswap(m_programRom);

    // Process sprite ROMs
    std::cout << "Decoding sprite ROMs..." << std::endl;
    // If no text ROM was loaded, extract text data from sprites
    if (m_textRom.empty() && !m_spriteRom.empty() && m_spriteRom.size() > 0x80000) {
        // Extract 512KB of text data from the end of the sprite ROM
        extractTextFromSprites(0x80000);
    }

    decodeSpriteROM();

    // Handle SWAPC: swap sprite ROM regions (0x200000-0x3FFFFF with 0x400000-0x4FFFFF)
    if (swapcFlag) {
        processSWAPCRom();
    }
    
    // Decode text ROMs
    std::cout << "Decoding text ROMs..." << std::endl;
    decodeTextROM();
    
    // Update sizes
    m_programRomSize = static_cast<u32>(m_programRom.size());
    m_spriteRomSize = static_cast<u32>(m_spriteRom.size());
    m_textRomSize = static_cast<u32>(m_textRom.size());
    m_soundRomSize = static_cast<u32>(m_soundRom.size());
    
    // Build vector tables
    buildVectorTables();
    
    return true;
}

void Cartridge::reset() {
    m_biosVectorTableActive = true;  // Reset to BIOS vector table
}

u8 Cartridge::readProgramROM8(u32 offset) const {
    // Direct ROM access by offset - Memory class handles address mapping
    if (offset >= m_programRomSize) {
        return 0;
    }
    return m_programRom[offset];
}

u8 Cartridge::readSpriteROM8(u32 address) const {
    if (address >= m_spriteRomSize) {
        return 0;
    }
    return m_spriteRom[address];
}

u8 Cartridge::readTextROM8(u32 address) const {
    if (address >= m_textRomSize) {
        return 0;
    }
    return m_textRom[address];
}

u8 Cartridge::readSoundROM8(u32 address) const {
    if (address >= m_soundRomSize) {
        return 0;
    }
    return m_soundRom[address];
}

u8 Cartridge::readBIOS68K8(u32 address) const {
    if (address >= m_bios68kRom.size()) {
        return 0;
    }
    return m_bios68kRom[address];
}

u8 Cartridge::readBIOSZ808(u32 address) const {
    if (address >= m_biosZ80Rom.size()) {
        return 0;
    }
    return m_biosZ80Rom[address];
}

u8 Cartridge::readBIOSText8(u32 address) const {
    if (address >= m_biosTextRom.size()) {
        return 0;
    }
    return m_biosTextRom[address];
}

u8 Cartridge::readZoomROM8(u32 address) const {
    if (address >= m_zoomRom.size()) {
        return 0;
    }
    return m_zoomRom[address];
}

void Cartridge::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_biosVectorTableActive), sizeof(m_biosVectorTableActive));
}

void Cartridge::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_biosVectorTableActive), sizeof(m_biosVectorTableActive));
}

// Decode text ROM: reorganize 32-byte tiles for text layer rendering
void Cartridge::decodeTextTile(const u8* src, u8* dst) {
    u8 buffer[32];
    
    // Reorganize tile data: rearrange bytes from [0-7, 8-15, 16-23, 24-31]
    // to [16, 24, 0, 8, 17, 25, 1, 9, ...]
    for (int i = 0; i < 8; i++) {
        buffer[0 + i * 4] = src[16 + i];
        buffer[1 + i * 4] = src[24 + i];
        buffer[2 + i * 4] = src[0 + i];
        buffer[3 + i * 4] = src[8 + i];
    }
    
    // Swap nibbles in each byte
    for (int i = 0; i < 32; i++) {
        dst[i] = (buffer[i] << 4) | (buffer[i] >> 4);
    }
}

void Cartridge::byteswap(std::vector<u8>& rom) {
    if (rom.empty()) {
        return;
    }
    for (size_t i = 0; i < rom.size() - 1; i += 2) {
        std::swap(rom[i], rom[i + 1]);
    }
}

void Cartridge::interleavedCopy(u8* dest, const u8* src, u32 size) {
    for (u32 i = 0; i < size; i++) {
        dest[i * 2] = src[i];
    }
}

void Cartridge::decodeTextROM() {
    if (m_textRom.empty()) {
        return;
    }
    
    // Decode in-place: process each 32-byte tile
    u32 tileCount = m_textRom.size() / 32;
    for (u32 i = 0; i < tileCount; i++) {
        u8* tileSrc = m_textRom.data() + (i * 32);
        u8* tileDst = tileSrc;  // Decode in-place
        
        u8 temp[32];
        decodeTextTile(tileSrc, temp);
        std::memcpy(tileDst, temp, 32);
    }
}

void Cartridge::decodeSpriteROM() {
    if (m_spriteRom.empty()) {
        return;
    }
    
    // Decode sprites: reorganize 128-byte sprite tiles
    // Each sprite tile is 16x16 pixels = 128 bytes
    // The decoding reorganizes the bitplanes for easier rendering
    u32 spriteCount = m_spriteRom.size() / 128;
    
    for (u32 spriteIdx = 0; spriteIdx < spriteCount; spriteIdx++) {
        u8* sprite = m_spriteRom.data() + (spriteIdx * 128);
        u32 data[32];
        
        // Process each row (16 rows per sprite)
        for (int y = 0; y < 16; y++) {
            u32 n = 0;
            // First 8 pixels of row
            for (int x = 0; x < 8; x++) {
                u32 m = ((sprite[67 + (y << 2)] >> x) & 1) << 3;
                m |= ((sprite[65 + (y << 2)] >> x) & 1) << 2;
                m |= ((sprite[66 + (y << 2)] >> x) & 1) << 1;
                m |= ((sprite[64 + (y << 2)] >> x) & 1) << 0;
                n |= m << (x << 2);
            }
            data[(y << 1) + 0] = n;
            
            n = 0;
            // Second 8 pixels of row
            for (int x = 0; x < 8; x++) {
                u32 m = ((sprite[3 + (y << 2)] >> x) & 1) << 3;
                m |= ((sprite[1 + (y << 2)] >> x) & 1) << 2;
                m |= ((sprite[2 + (y << 2)] >> x) & 1) << 1;
                m |= ((sprite[0 + (y << 2)] >> x) & 1) << 0;
                n |= m << (x << 2);
            }
            data[(y << 1) + 1] = n;
        }
        
        // Copy decoded data back to sprite tile
        std::memcpy(sprite, data, 128);
    }
}

bool Cartridge::loadBIOSROMs(const std::map<std::string, std::vector<u8>>& romFiles, const fs::path& gameRomPath, u32 bios68kIndex) {
    // Clear existing BIOS ROMs
    m_bios68kRom.clear();
    m_biosZ80Rom.clear();
    m_biosTextRom.clear();
    m_zoomRom.clear();
    
    // Load BIOS ROMs using specific indices from s_biosROMs array
    // Index 0: 68K BIOS (default: sp-s3.sp1)
    // Index 37: Z80 BIOS (sm1.sm1)
    // Index 38: Text BIOS (sfix.sfix)
    // Index 39: Zoom table (000-lo.lo)
    const BIOSROMEntry* biosROMs = GameDatabase::getBIOSROMs();

    // BIOS files from neogeo.zip - loaded on demand
    std::map<std::string, std::vector<u8>> biosFiles;

    // Lambda function to load a BIOS file with priority: game ZIP -> neogeo.zip
    auto loadBIOSFile = [&](const BIOSROMEntry& entry, std::vector<u8>& targetRom, const std::string& biosTypeName, bool checkEmptyFilename = true) -> bool {
        bool found = false;
        // First check game ZIP for BIOS file
        for (const auto& pair : romFiles) {
            if (GameDatabase::validateBIOSROM(pair.first, pair.second, entry)) {
                std::cout << "Loading " << biosTypeName << ": " << entry.filename << std::endl;
                targetRom.assign(pair.second.begin(), pair.second.end());
                found = true;
                break;
            }
        }
        // If not found in game ZIP, check neogeo.zip (load it on demand if needed)
        if (!found) {
            if (biosFiles.empty()) {
                // Try to load neogeo.zip on demand
                fs::path biosZipPath = gameRomPath.parent_path() / "neogeo.zip";
                if (fs::exists(biosZipPath)) {
                    util::ZipReader biosZip;
                    if (!biosZip.open(biosZipPath) || !biosZip.extractAll(biosFiles)) {
                        std::cerr << "Failed to load BIOS files from neogeo.zip" << std::endl;
                        return false;
                    }
                }
            }
            // Search in loaded BIOS files
            for (const auto& pair : biosFiles) {
                if (GameDatabase::validateBIOSROM(pair.first, pair.second, entry)) {
                    std::cout << "Loading " << biosTypeName << " (neogeo.zip): " << entry.filename << std::endl;
                    targetRom.assign(pair.second.begin(), pair.second.end());
                    found = true;
                    break;
                }
            }
        }
        if (!found && (checkEmptyFilename || entry.filename[0] != '\0')) {
            std::cerr << "Error: " << biosTypeName << " ROM not found: " << entry.filename << std::endl;
        }
        return found;
    };

    // Load 68K BIOS using provided index
    const BIOSROMEntry& bios68kEntry = biosROMs[bios68kIndex];

    if (!loadBIOSFile(bios68kEntry, m_bios68kRom, "68K BIOS", false)) {
        return false;
    }
    
    // Load Z80 BIOS (index 37)
    const BIOSROMEntry& biosZ80Entry = biosROMs[37];
    if (!loadBIOSFile(biosZ80Entry, m_biosZ80Rom, "Z80 BIOS")) {
        return false;
    }
    
    // Load Text BIOS (index 38)
    const BIOSROMEntry& biosTextEntry = biosROMs[38];
    if (!loadBIOSFile(biosTextEntry, m_biosTextRom, "Text BIOS")) {
        return false;
    }
    
    // Load Zoom table (index 39)
    const BIOSROMEntry& zoomEntry = biosROMs[39];
    if (!loadBIOSFile(zoomEntry, m_zoomRom, "Zoom table")) {
        return false;
    }
    
    // For 68K BIOS, we need to handle 0x20000 -> 0x80000 expansion
    // Some BIOSes are 0x20000, some are 0x80000
    if (!m_bios68kRom.empty() && m_bios68kRom.size() == 0x20000) {
        // Expand 0x20000 BIOS to 0x80000 (repeat 4 times)
        std::vector<u8> expanded(0x80000);
        for (int i = 0; i < 4; i++) {
            std::memcpy(expanded.data() + (i * 0x20000), m_bios68kRom.data(), 0x20000);
        }
        m_bios68kRom = expanded;
    }
    
    // Byteswap 68K BIOS
    byteswap(m_bios68kRom);
    
    // Decode BIOS text ROM
    decodeBIOSTextROM();
    
    return true;
}

void Cartridge::decodeBIOSTextROM() {
    if (m_biosTextRom.empty()) {
        return;
    }
    
    // Decode in-place: process each 32-byte tile
    u32 tileCount = static_cast<u32>(m_biosTextRom.size()) / 32;
    for (u32 i = 0; i < tileCount; i++) {
        u8* tileSrc = m_biosTextRom.data() + (i * 32);
        u8 temp[32];
        decodeTextTile(tileSrc, temp);
        std::memcpy(tileSrc, temp, 32);
    }
}

void Cartridge::processSWAPCRom() {
    // SWAPC: Swap sprite ROM regions
    // Swap region 0x200000-0x3FFFFF with 0x400000-0x4FFFFF (as 16-bit words)
    const u32 BUFFER_START = 0x200000;  // Start copying from here
    const u32 BUFFER_END = 0x600000;    // Copy up to (but not including) here
    const u32 SWAP_REGION1_START = 0x200000;  // First swap region start
    const u32 SWAP_REGION2_START = 0x400000;  // Second swap region start
    const u32 WORD_COUNT = 0x100000;          // 0x100000 16-bit words = 0x200000 bytes per region
    
    if (m_spriteRom.size() < BUFFER_END) {
        // Not enough sprite ROM for SWAPC
        return;
    }
    
    // Allocate temporary buffer (0x600000 - 0x200000 = 0x400000 bytes)
    std::vector<u8> tempBuffer(BUFFER_END - BUFFER_START);
    
    // Copy bytes 0x200000-0x5FFFFF to temp buffer
    std::memcpy(tempBuffer.data(), m_spriteRom.data() + BUFFER_START, BUFFER_END - BUFFER_START);
    
    // Swap regions as 16-bit words
    u16* dest1 = reinterpret_cast<u16*>(m_spriteRom.data() + SWAP_REGION1_START);
    u16* dest2 = reinterpret_cast<u16*>(m_spriteRom.data() + SWAP_REGION2_START);
    u16* src1 = reinterpret_cast<u16*>(tempBuffer.data() + (SWAP_REGION2_START - BUFFER_START));
    u16* src2 = reinterpret_cast<u16*>(tempBuffer.data() + (SWAP_REGION1_START - BUFFER_START));
    
    for (u32 i = 0; i < WORD_COUNT; i++) {
        dest1[i] = src1[i];  // Copy region 2 (from temp) to region 1
        dest2[i] = src2[i];  // Copy region 1 (from temp) to region 2
    }
}

void Cartridge::extractTextFromSprites(u32 textRomSize) {
    if (m_spriteRom.empty() || textRomSize == 0) {
        return;
    }

    m_textRom.resize(textRomSize);

    // Extract text data from the end of sprite ROMs
    const u8* spriteRom = m_spriteRom.data();
    u32 spriteRomSize = static_cast<u32>(m_spriteRom.size());
    const u8* src = spriteRom + (spriteRomSize - textRomSize);

    // Rearrange bytes
    for (u32 i = 0; i < textRomSize; i++) {
        u32 srcOffset = (i & ~0x1F) + ((i & 7) << 2) + ((~i & 8) >> 2) + ((i & 0x10) >> 4);
        if (srcOffset < textRomSize) {
            m_textRom[i] = src[srcOffset];
        } else {
            m_textRom[i] = 0;
        }
    }
}

void Cartridge::buildVectorTables() {
    // Vector table is 0x400 bytes (0x000000-0x0003FF)
    const u32 VECTOR_TABLE_SIZE = 0x400;
    
    // Clear and resize
    m_hybridBiosVectors.clear();
    m_hybridCartVectors.clear();
    m_hybridBiosVectors.resize(VECTOR_TABLE_SIZE, 0);
    m_hybridCartVectors.resize(VECTOR_TABLE_SIZE, 0);
    
    // Build m_hybridBiosVectors
    // BIOS[0x00-0x7F] + Cartridge[0x80-0x3FF]
    // Used at 0x000000 when BIOS vector table is active
    if (m_bios68kRom.size() >= 0x80) {
        std::memcpy(m_hybridBiosVectors.data(), m_bios68kRom.data(), 0x80);
    }
    if (m_programRom.size() >= VECTOR_TABLE_SIZE) {
        std::memcpy(m_hybridBiosVectors.data() + 0x80, m_programRom.data() + 0x80, VECTOR_TABLE_SIZE - 0x80);
    }
    
    // Build m_hybridCartVectors
    // Cartridge[0x00-0x7F] + BIOS[0x80-0x3FF]
    // Used at 0xC00000 when BIOS vector table is active
    if (m_bios68kRom.size() >= VECTOR_TABLE_SIZE) {
        std::memcpy(m_hybridCartVectors.data(), m_bios68kRom.data(), VECTOR_TABLE_SIZE);
    }
    if (m_programRom.size() >= 0x80) {
        std::memcpy(m_hybridCartVectors.data(), m_programRom.data(), 0x80);
    }
}

u8 Cartridge::readVectorTable8(u32 address) const {
    // Vector table at 0x000000-0x0003FF
    u32 offset = address & 0x3FF;
    
    if (m_biosVectorTableActive) {
        // BIOS vectors active: 0x000000 maps to hybrid (BIOS[0x00-0x7F] + Cart[0x80-0x3FF])
        if (offset < m_hybridBiosVectors.size()) {
            return m_hybridBiosVectors[offset];
        }
    } else {
        // Cartridge vectors active: 0x000000 maps to straight cartridge ROM
        if (offset < m_programRom.size()) {
            return m_programRom[offset];
        }
    }
    
    return 0;
}

u8 Cartridge::readBiosVectorTable8(u32 address) const {
    // BIOS region at 0xC00000-0xC003FF
    u32 offset = address & 0x3FF;
    
    if (m_biosVectorTableActive) {
        // BIOS vectors active: 0xC00000 maps to hybrid (Cart[0x00-0x7F] + BIOS[0x80-0x3FF])
        if (offset < m_hybridCartVectors.size()) {
            return m_hybridCartVectors[offset];
        }
    } else {
        // Cartridge vectors active: 0xC00000 maps to straight BIOS ROM
        if (offset < m_bios68kRom.size()) {
            return m_bios68kRom[offset];
        }
    }
    
    return 0;
}

void Cartridge::setBiosVectorTableActive(bool active) {
    m_biosVectorTableActive = active;
}

} // namespace neogeo
