#include "memory.h"
#include "upd4990a.h"
#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"
#include "controller.h"
#include "apu.h"
#include "core.h"
#include "protections.h"
#include <cstring>
#include <filesystem>

namespace neogeo {

Memory::Memory()
    : m_cpu(nullptr)
    , m_soundCpu(nullptr)
    , m_ppu(nullptr)
    , m_cartridge(nullptr)
    , m_controller(nullptr)
    , m_apu(nullptr)
    , m_core(nullptr)
    , m_nvramLoaded(false)
    , m_inputSelect(0)
    , m_nvramWritable(false)
    , m_paletteBank(0)
    , m_darkenPalette(false)
    , m_biosTextRomEnabled(false)
    , m_irqControl(0)
    , m_irqOffset(0)
    , m_programRomBank(0x100000)
    , m_z80Bank0(0x02)
    , m_z80Bank1(0x06)
    , m_z80Bank2(0x0E)
    , m_z80Bank3(0x1E)
    , m_z80BiosRomMapped(false)
    , m_memoryHijacker(nullptr) {
    m_nvram.fill(0);
}

Memory::~Memory() {
    saveNVRAM();
}

void Memory::reset() {
    // Clear RAM
    m_workRam.fill(0);
    m_paletteRam.fill(0);
    m_z80Ram.fill(0);
    
    // Reset I/O registers
    m_inputSelect = 0;
    m_nvramWritable = false;
    m_paletteBank = 0;
    m_darkenPalette = false;
    m_biosTextRomEnabled = false;
    
    // Reset video controller registers
    m_irqControl = 0;
    m_irqOffset = 0;
    
    // Reset Z80 banking
    m_z80Bank0 = 0x02;  // Bank 0: starts at 0x8000 in Z80 ROM
    m_z80Bank1 = 0x06;  // Bank 1: starts at 0xC000 in Z80 ROM
    m_z80Bank2 = 0x0E;  // Bank 2: starts at 0x38000 in Z80 ROM
    m_z80Bank3 = 0x1E;  // Bank 3: starts at 0x3C000 in Z80 ROM
    m_z80BiosRomMapped = false;
    
    // Reset 68K ROM banking (initial bank is 0x100000 for games > 1MB)
    m_programRomBank = 0x100000;

    // Save NVRAM before rom filename is updated
    if (m_nvramLoaded) {
        saveNVRAM();
    }

    m_romFilename = m_cartridge->getRomFilename();

    // Load after rom filename is updated
    loadNVRAM();
    m_nvramLoaded = true;

    // Install memory hijacker for specific games
    m_memoryHijacker = initProtections(this, m_cartridge);
}

u8 Memory::read8(u32 address) {
    // Vector table (0x000000-0x0003FF) - can be BIOS or cartridge
    if (address < 0x400) {
        if (m_cartridge) {
            return m_cartridge->readVectorTable8(address);
        }
        return 0xFF;
    }
    
    // Program ROM (0x000400-0x0FFFFF)
    if (address >= 0x400 && address < 0x100000) {
        if (m_cartridge) {
            return m_cartridge->readProgramROM8(address);
        }
        return 0xFF;
    }
    
    // Work RAM mirror (0x100000-0x1FFFFF) - 64KB mirrored
    if (address >= 0x100000 && address < 0x200000) {
        return m_workRam[address & 0xFFFF];
    }
    
    // Banked ROM area (0x200000-0x2FFFFF) - for games > 1MB
    if (address >= 0x200000 && address < 0x300000) {
        if (m_cartridge) {
            u32 romOffset = (address - 0x200000) + m_programRomBank;
            return m_cartridge->readProgramROM8(romOffset);
        }
        return 0xFF;
    }
    
    // Input ports
    switch (address & 0xFE0000) {
        case 0x300000:
            return ~m_controller->readInput1(address & 0xFF);
            
        case 0x320000:
            if ((address & 1) == 0) {
                // Even: Sound reply
                if (m_apu) {
                    return m_apu->getSoundReply();
                }
                return 0x00;
            } else {
                // Odd: System status byte
                u8 inputBank3 = m_controller->getInputBank(3);
                if (Config::System == SystemType::MVS) {
                    return (~inputBank3 & 0x3F) | (m_upd4990a->read() << 6);
                } else {
                    return (~inputBank3 & 0x7F) & 0xE7;
                }
            }
            
        case 0x340000:
            return ~m_controller->readInput2(address & 0xFF);
            
        case 0x380000:
            return ~m_controller->readInput3(address & 0xFF);
    }
    
    // Video controller (0x3C0000-0x3C000F)
    if (address >= 0x3C0000 && address <= 0x3C000F) {
        if (address & 1) {
            return 0xFF;
        }
        return readVideoController(address) >> 8;
    }
    
    // Palette RAM (0x400000-0x7FFFFF) - readable, mirrored every 0x2000 bytes
    // The palette RAM is 8KB (0x2000) and is mirrored throughout this range
    if (address >= 0x400000 && address < 0x800000) {
        u32 paletteOffset = address & 0x1FFF;  // Palette is 0x2000 bytes, mirrored
        return readPalette8(paletteOffset);
    }
    
    // Note: Sprite ROM (0x400000-0x4FFFFF) and Text ROM (0x500000-0x5FFFFF) are NOT
    // directly readable through the CPU address space on the NeoGeo.
    // The sprite and text data are accessed by the PPU through dedicated buses.
    
    // BIOS ROM (0xC00000-0xCFFFFF) - first 0x400 bytes are vector table
    if (address >= 0xC00000 && address <= 0xCFFFFF) {
        if (address <= 0xC003FF) {
            // Vector table area
            if (m_cartridge) {
                return m_cartridge->readBiosVectorTable8(address);
            }
        } else {
            // BIOS ROM (mirrors within 0xC00000-0xCFFFFF)
            if (m_cartridge) {
                u32 biosOffset = address & 0x7FFFF;  // BIOS is usually 0x80000 bytes
                return m_cartridge->readBIOS68K8(biosOffset);
            }
        }
        return 0xFF;
    }
    
    // NVRAM (0xD00000-0xDFFFFF) - MVS only, 64KB mirrored
    if (address >= 0xD00000 && address < 0xE00000) {
        if (Config::System == SystemType::MVS) {
            return m_nvram[address & 0xFFFF];
        }
        return 0xFF;  // Open bus on AES
    }
    
    // 0xE00000-0xFFFFFF: Open bus for cartridge systems (CD transfer area for NeoCD)
    
    return 0xFF;
}

u16 Memory::read16(u32 address) {
    if (m_memoryHijacker) {
        u16 ret = 0;
        if (m_memoryHijacker->read16(address, ret)) {
            return ret;
        }
    }

    // Video controller
    if (address >= 0x3C0000 && address <= 0x3C000F) {
        return readVideoController(address);
    }
    
    // For other addresses, combine two byte reads
    u8 high = read8(address);
    u8 low = read8(address + 1);
    return (static_cast<u16>(high) << 8) | low;
}

u32 Memory::read32(u32 address) {
    u16 high = read16(address);
    u16 low = read16(address + 2);
    return (static_cast<u32>(high) << 16) | low;
}

void Memory::write8(u32 address, u8 value) {
    // ROM is read-only (0x000000-0x0FFFFF)
    if (address < 0x100000) {
        return;
    }
    
    // Work RAM mirror (0x100000-0x1FFFFF) - 64KB mirrored
    if (address >= 0x100000 && address < 0x200000) {
        m_workRam[address & 0xFFFF] = value;
        return;
    }
    
    // Banked ROM area (0x200000-0x2FFFFF) - bankswitch at 0x2FFFF0-0x2FFFFF
    if (address >= 0x2FFFF0 && address <= 0x2FFFFF) {
        // Bank selection: bank_offset = 0x100000 + ((value & 7) << 20)
        u32 newBank = 0x100000 + ((value & 7) << 20);
        // Clamp to ROM size if needed
        if (m_cartridge && newBank >= m_cartridge->getProgramROMSize()) {
            newBank = 0x100000;
        }
        m_programRomBank = newBank;
        return;
    }
    
    // I/O ports
    switch (address & 0xFF0000) {
        case 0x300000:
            // Watchdog timer reset (odd addresses)
            if ((address & 1) == 1) {
                if (m_core) {
                    m_core->resetWatchdog();
                }
            }
            return;
            
        case 0x320000:
            // Sound command (even addresses)
            if ((address & 1) == 0) {
                // Send sound command (triggers NMI to Z80 if enabled)
                if (m_apu) {
                    m_apu->setSoundCommand(value);
                }
            }
            return;
            
        case 0x380000:
            // I/O port 1
            writeIO1(address & 0xFF, value);
            return;
            
        case 0x3A0000:
            // I/O port 2 - uses lower 5 bits for offset
            writeIO2(address & 0x1F, value);
            return;
    }
    
    // Video controller
    if (address >= 0x3C0000 && address <= 0x3C000F) {
        u16 wordValue = (static_cast<u16>(value) << 8) | value;  // Byte smearing
        writeVideoController(address, wordValue);
        return;
    }
    
    // Palette RAM (0x400000-0x7FFFFF) - writable, mirrored every 0x2000 bytes
    if (address >= 0x400000 && address < 0x800000) {
        u32 paletteOffset = address & 0x1FFF;  // Palette is 0x2000 bytes, mirrored
        writePalette8(paletteOffset, value);
        return;
    }
    
    // NVRAM (0xD00000-0xDFFFFF) - MVS only, 64KB mirrored
    if (address >= 0xD00000 && address < 0xE00000) {
        if (Config::System == SystemType::MVS && m_nvramWritable) {
            m_nvram[address & 0xFFFF] = value;
        }
        return;
    }
}

void Memory::write16(u32 address, u16 value) {
    if (m_memoryHijacker) {
        if (m_memoryHijacker->write16(address, value)) {
            return;
        }
    }

    // Video controller
    if (address >= 0x3C0000 && address <= 0x3C000F) {
        writeVideoController(address, value);
        return;
    }
    
    // For other addresses, write as two bytes
    write8(address, static_cast<u8>(value >> 8));
    write8(address + 1, static_cast<u8>(value & 0xFF));
}

void Memory::write32(u32 address, u32 value) {
    write16(address, static_cast<u16>(value >> 16));
    write16(address + 2, static_cast<u16>(value & 0xFFFF));
}

u16 Memory::readVideoController(u32 address) {
    switch (address & 0x0E) {
        case 0x00:
        case 0x02:
            // Graphics RAM read
            if (m_ppu) {
                return m_ppu->readVRAM();
            }
            return 0;
            
        case 0x04:
            // Graphics RAM modulo
            if (m_ppu) {
                return m_ppu->getVRAMModulo();
            }
            return 0;
            
        case 0x06:
            // Display status (scanline + sprite frame)
            if (m_ppu) {
                constexpr u32 scanlineOffset = 0xF8;
                u32 currentScanline = (m_ppu->getScanline() + 248) % 264;
                u32 spriteFrame = m_ppu->getSpriteFrame();
                return static_cast<u16>(((currentScanline + scanlineOffset) << 7) | (spriteFrame & 7));
            }
            return (0xF8 << 7);  // Fake VBlank
            
        default:
            return 0;
    }
}

void Memory::writeVideoController(u32 address, u16 value) {
    switch (address & 0x0E) {
        case 0x00:
            // Graphics RAM pointer
            if (m_ppu) {
                m_ppu->setVRAMPointer(value);
            }
            break;
            
        case 0x02:
            // Graphics RAM write
            if (m_ppu) {
                m_ppu->writeVRAM(value);
            }
            break;
            
        case 0x04:
            // Graphics RAM modulo
            if (m_ppu) {
                m_ppu->setVRAMModulo(static_cast<s16>(value));
            }
            break;
            
        case 0x06:
            // IRQ control + sprite frame speed
            m_irqControl = value & 0xFF;
            if (m_ppu) {
                m_ppu->setSpriteFrameSpeed((value >> 8) & 0xFF);
            }
            break;
            
        case 0x08:
            // IRQ offset (high word)
            m_irqOffset = (m_irqOffset & 0x0000FFFF) | ((value & 0x7FFF) << 16);
            break;
            
        case 0x0A:
            // IRQ offset (low word)
            m_irqOffset = (m_irqOffset & 0xFFFF0000) | value;
            // IRQ scheduling would go here
            break;
            
        case 0x0C:
            // IRQ acknowledge
            if (m_cpu) {
                m_cpu->irq(0);
            }
            break;
    }
}

u8 Memory::readPalette8(u32 address) {
    if (address >= PALETTE_RAM_SIZE) {
        return 0;
    }
    
    // Swap byte order (68000 is big-endian, we store little-endian)
    address ^= 1;
    
    // Use current palette bank
    u32 bankOffset = m_paletteBank * (PALETTE_RAM_SIZE / 2);
    u16 paletteEntry = m_paletteRam[bankOffset + (address / 2)];
    if (address & 1) {
        return static_cast<u8>(paletteEntry >> 8);
    } else {
        return static_cast<u8>(paletteEntry & 0xFF);
    }
}

u16 Memory::readPalette16(u32 address) const {
    if (address >= PALETTE_RAM_SIZE) {
        return 0;
    }
    // Use current palette bank
    u32 bankOffset = m_paletteBank * (PALETTE_RAM_SIZE / 2);
    return m_paletteRam[bankOffset + (address / 2)];
}

u16 Memory::readPalette16Private(u32 address) {
    return readPalette16(address);
}

void Memory::writePalette8(u32 address, u8 value) {
    if (address >= PALETTE_RAM_SIZE) {
        return;
    }
    
    // Swap byte order (68000 is big-endian, we store little-endian)
    address ^= 1;
    
    // Use current palette bank
    u32 bankOffset = m_paletteBank * (PALETTE_RAM_SIZE / 2);
    u16& paletteEntry = m_paletteRam[bankOffset + (address / 2)];
    if (address & 1) {
        paletteEntry = (paletteEntry & 0x00FF) | (static_cast<u16>(value) << 8);
    } else {
        paletteEntry = (paletteEntry & 0xFF00) | value;
    }
}

void Memory::writePalette16(u32 address, u16 value) {
    if (address >= PALETTE_RAM_SIZE) {
        return;
    }
    // Use current palette bank
    u32 bankOffset = m_paletteBank * (PALETTE_RAM_SIZE / 2);
    m_paletteRam[bankOffset + (address / 2)] = value;
}

void Memory::writeIO1(u8 offset, u8 value) {
    // I/O port 1 (0x380000 area)
    switch (offset) {
        case 0x01:
            // Select the input returned at 0x300000
            m_inputSelect = value;
            break;
            
        case 0x21:
            // Select the active cartridge slot (not used for single slot)
            break;
            
        case 0x31:
            // Send latched output to LEDs
            break;
            
        case 0x41:
            // Latch LED output
            break;
            
        case 0x51:
            // Send command to RTC (MVS only)
            if (m_upd4990a) {
                m_upd4990a->write(value & 2, value & 4, value & 1);
            }
            break;
            
        case 0x61:
            // Coin lockout chute 1 & input bank select
            break;
            
        case 0x63:
            // Coin lockout chute 2
            break;
            
        case 0x65:
            // Coin counter chute 1 -> High
            break;
            
        case 0x67:
            // Coin counter chute 2 -> High
            break;
            
        case 0xD1:
            // Send command to RTC (MVS only)
            if (m_upd4990a) {
                m_upd4990a->write(value & 2, value & 4, value & 1);
            }
            break;
            
        case 0xE1:
            // Chute 2 coin lockout -> Low / Input bank 1 selected
            break;
            
        case 0xE3:
            // Chute 2 coin lockout -> Low
            break;
            
        case 0xE5:
            // Coin counter chute 1 -> Low
            break;
            
        case 0xE7:
            // Coin counter chute 2 -> Low
            break;
    }
}

void Memory::writeIO2(u8 offset, u8 /* value */) {
    // I/O port 2 (0x3A0000 area) - uses only lower 5 bits
    switch (offset) {
        case 0x01:
        case 0x11:
            // Shadow latch - causes the palette to darken
            m_darkenPalette = (offset == 0x11);
            break;
            
        case 0x03:
            // Select BIOS vector table
            if (m_cartridge) {
                m_cartridge->setBiosVectorTableActive(true);
            }
            break;
            
        case 0x0B:
            // Select BIOS text/Z80 ROM
            // For AES systems, this doesn't enable BIOS text ROM (games use their own text ROM)
            // For MVS systems, this enables BIOS text ROM
            if (Config::System == SystemType::MVS) {
                m_biosTextRomEnabled = true;
            }
            if (m_z80BiosRomMapped == false) {
                m_z80BiosRomMapped = true;
            }
            break;
            
        case 0x0D:
            // Write-protect NVRAM
            m_nvramWritable = false;
            break;
            
        case 0x0F:
            // Select palette bank 1
            m_paletteBank = 1;
            break;
            
        case 0x13:
            // Select game/cartridge vector table
            if (m_cartridge) {
                m_cartridge->setBiosVectorTableActive(false);
            }
            break;
            
        case 0x1B:
            // Select game text/Z80 ROM
            m_biosTextRomEnabled = false;
            if (m_z80BiosRomMapped == true) {
                m_z80BiosRomMapped = false;
            }
            break;
            
        case 0x1D:
            // Write-enable NVRAM
            m_nvramWritable = true;
            break;
            
        case 0x1F:
            // Select palette bank 0
            m_paletteBank = 0;
            break;
    }
}

// Z80 Memory Map:
// 0x0000-0x7FFF: Z80 BIOS ROM or cartridge ROM (32KB, switchable)
// 0x8000-0xBFFF: Bank 0 (16KB, bank << 14)
// 0xC000-0xDFFF: Bank 1 (8KB, bank << 13)
// 0xE000-0xEFFF: Bank 2 (4KB, bank << 12)
// 0xF000-0xF7FF: Bank 3 (2KB, bank << 11)
// 0xF800-0xFFFF: Z80 RAM (2KB)

u8 Memory::readZ80(u32 address) {
    address &= 0xFFFF;  // 16-bit address space
    
    // 0x0000-0x7FFF: Z80 BIOS ROM or cartridge ROM
    if (address < 0x8000) {
        if (m_cartridge) {
            if (m_z80BiosRomMapped) {
                return m_cartridge->readBIOSZ808(address);
            } else {
                return m_cartridge->readSoundROM8(address);
            }
        }
        return 0xFF;
    }
    
    // 0x8000-0xBFFF: Bank 0 (16KB)
    if (address < 0xC000) {
        if (m_cartridge) {
            u32 bankOffset = (m_z80Bank0 & 0x0F) << 14;  // bank << 14
            u32 romAddress = bankOffset + (address - 0x8000);
            return m_cartridge->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    // 0xC000-0xDFFF: Bank 1 (8KB)
    if (address < 0xE000) {
        if (m_cartridge) {
            u32 bankOffset = (m_z80Bank1 & 0x1F) << 13;  // bank << 13
            u32 romAddress = bankOffset + (address - 0xC000);
            return m_cartridge->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    // 0xE000-0xEFFF: Bank 2 (4KB)
    if (address < 0xF000) {
        if (m_cartridge) {
            u32 bankOffset = (m_z80Bank2 & 0x3F) << 12;  // bank << 12
            u32 romAddress = bankOffset + (address - 0xE000);
            return m_cartridge->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    // 0xF000-0xF7FF: Bank 3 (2KB)
    if (address < 0xF800) {
        if (m_cartridge) {
            u32 bankOffset = (m_z80Bank3 & 0x7F) << 11;  // bank << 11
            u32 romAddress = bankOffset + (address - 0xF000);
            return m_cartridge->readSoundROM8(romAddress);
        }
        return 0xFF;
    }
    
    // 0xF800-0xFFFF: Z80 RAM (2KB)
    return m_z80Ram[address - 0xF800];
}

void Memory::writeZ80(u32 address, u8 value) {
    address &= 0xFFFF;  // 16-bit address space
    
    // ROM areas are read-only (0x0000-0xF7FF)
    if (address < 0xF800) {
        return;
    }
    
    // 0xF800-0xFFFF: Z80 RAM (2KB)
    m_z80Ram[address - 0xF800] = value;
}

u8 Memory::readZ80IO(u16 port) {
    u8 portLow = port & 0xFF;
    u8 portHigh = (port >> 8) & 0xFF;
    
    switch (portLow) {
        case 0x00:
            // Read sound command from 68000
            if (m_apu) {
                return m_apu->readPort(port);
            }
            return 0x00;
            
        case 0x04:
        case 0x05:
        case 0x06:
            // YM2610 read
            if (m_apu) {
                return m_apu->readPort(port);
            }
            return 0x00;
            
        case 0x08:
            // Bank 3 switch (uses high byte as bank number)
            m_z80Bank3 = portHigh & 0x7F;
            return 0x00;
            
        case 0x09:
            // Bank 2 switch
            m_z80Bank2 = portHigh & 0x3F;
            return 0x00;
            
        case 0x0A:
            // Bank 1 switch
            m_z80Bank1 = portHigh & 0x1F;
            return 0x00;
            
        case 0x0B:
            // Bank 0 switch
            m_z80Bank0 = portHigh & 0x0F;
            return 0x00;
            
        default:
            return 0x00;
    }
}

void Memory::writeZ80IO(u16 port, u8 value) {
    if (m_apu) {
        m_apu->writePort(port, value);
    }
}

void Memory::saveState(Buffer* buf) {
    buffer_write(buf, m_workRam.data(), m_workRam.size());
    buffer_write(buf, m_nvram.data(), m_nvram.size());
    buffer_write(buf, m_paletteRam.data(), m_paletteRam.size() * sizeof(u16));
    buffer_write(buf, m_z80Ram.data(), m_z80Ram.size());
    buffer_write(buf, &m_inputSelect, sizeof(m_inputSelect));
    buffer_write(buf, &m_nvramWritable, sizeof(m_nvramWritable));
    buffer_write(buf, &m_paletteBank, sizeof(m_paletteBank));
    buffer_write(buf, &m_darkenPalette, sizeof(m_darkenPalette));
    buffer_write(buf, &m_biosTextRomEnabled, sizeof(m_biosTextRomEnabled));
    buffer_write(buf, &m_irqControl, sizeof(m_irqControl));
    buffer_write(buf, &m_irqOffset, sizeof(m_irqOffset));
    buffer_write(buf, &m_z80Bank0, sizeof(m_z80Bank0));
    buffer_write(buf, &m_z80Bank1, sizeof(m_z80Bank1));
    buffer_write(buf, &m_z80Bank2, sizeof(m_z80Bank2));
    buffer_write(buf, &m_z80Bank3, sizeof(m_z80Bank3));
    buffer_write(buf, &m_z80BiosRomMapped, sizeof(m_z80BiosRomMapped));
    buffer_write(buf, &m_programRomBank, sizeof(m_programRomBank));
}

void Memory::loadState(Buffer* buf) {
    buffer_read(buf, m_workRam.data(), m_workRam.size());
    buffer_read(buf, m_nvram.data(), m_nvram.size());
    buffer_read(buf, m_paletteRam.data(), m_paletteRam.size() * sizeof(u16));
    buffer_read(buf, m_z80Ram.data(), m_z80Ram.size());
    buffer_read(buf, &m_inputSelect, sizeof(m_inputSelect));
    buffer_read(buf, &m_nvramWritable, sizeof(m_nvramWritable));
    buffer_read(buf, &m_paletteBank, sizeof(m_paletteBank));
    buffer_read(buf, &m_darkenPalette, sizeof(m_darkenPalette));
    buffer_read(buf, &m_biosTextRomEnabled, sizeof(m_biosTextRomEnabled));
    buffer_read(buf, &m_irqControl, sizeof(m_irqControl));
    buffer_read(buf, &m_irqOffset, sizeof(m_irqOffset));
    buffer_read(buf, &m_z80Bank0, sizeof(m_z80Bank0));
    buffer_read(buf, &m_z80Bank1, sizeof(m_z80Bank1));
    buffer_read(buf, &m_z80Bank2, sizeof(m_z80Bank2));
    buffer_read(buf, &m_z80Bank3, sizeof(m_z80Bank3));
    buffer_read(buf, &m_z80BiosRomMapped, sizeof(m_z80BiosRomMapped));
    buffer_read(buf, &m_programRomBank, sizeof(m_programRomBank));
}

void Memory::saveNVRAM() {
    if (m_romFilename.empty()) {
        return;
    }

    fs::path nvramPath = m_romFilename;
    nvramPath.replace_extension(".sav");

    FILE* file = fopen(nvramPath.string().c_str(), "wb");
    if (!file) {
        log_error("Failed to create NVRAM file: %s", nvramPath.string().c_str());
        return;
    }

    // Write NVRAM data
    fwrite(m_nvram.data(), 1, m_nvram.size(), file);

    fclose(file);
    log_info("NVRAM saved to: %s", nvramPath.string().c_str());
}

void Memory::loadNVRAM() {
    fs::path nvramPath = m_romFilename;
    nvramPath.replace_extension(".sav");

    FILE* file = fopen(nvramPath.string().c_str(), "rb");
    if (!file) {
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read NVRAM data
    size_t nvramSize = m_nvram.size();
    if (fileSize >= static_cast<long>(nvramSize)) {
        fread(m_nvram.data(), 1, nvramSize, file);
        log_info("NVRAM loaded from: %s", nvramPath.string().c_str());
    } else {
        log_error("NVRAM file size mismatch, expected %d bytes, got %d bytes", static_cast<int>(nvramSize), static_cast<int>(fileSize));
    }

    fclose(file);
}

} // namespace neogeo
