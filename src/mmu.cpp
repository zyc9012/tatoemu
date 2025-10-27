#include "mmu.h"
#include "cartridge.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "apu.h"
#include <algorithm>

MMU::MMU()
    : m_cartridge(nullptr)
    , m_ppu(nullptr)
    , m_joypad(nullptr)
    , m_timer(nullptr)
    , m_apu(nullptr)
    , m_ie(0) {
    std::fill(m_wram.begin(), m_wram.end(), 0);
    std::fill(m_hram.begin(), m_hram.end(), 0);
}

MMU::~MMU() {
}

void MMU::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_wram.data()), m_wram.size());
    file.write(reinterpret_cast<const char*>(m_hram.data()), m_hram.size());
    file.write(reinterpret_cast<const char*>(&m_ie), sizeof(m_ie));
}

void MMU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_wram.data()), m_wram.size());
    file.read(reinterpret_cast<char*>(m_hram.data()), m_hram.size());
    file.read(reinterpret_cast<char*>(&m_ie), sizeof(m_ie));
}

void MMU::setCartridge(Cartridge* cartridge) {
    m_cartridge = cartridge;
}

void MMU::setPPU(PPU* ppu) {
    m_ppu = ppu;
}

void MMU::setJoypad(Joypad* joypad) {
    m_joypad = joypad;
}

void MMU::setTimer(Timer* timer) {
    m_timer = timer;
}

void MMU::setAPU(APU* apu) {
    m_apu = apu;
}

u8 MMU::read(u16 address) const {
    // ROM (0x0000 - 0x7FFF) and Cartridge RAM (0xA000 - 0xBFFF)
    if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
        if (m_cartridge) {
            return m_cartridge->read(address);
        }
        return 0xFF;
    }
    // VRAM (0x8000 - 0x9FFF)
    else if (address < 0xA000) {
        if (m_ppu) {
            return m_ppu->readVRAM(address);
        }
        return 0xFF;
    }
    // Work RAM (0xC000 - 0xDFFF)
    else if (address < 0xE000) {
        return m_wram[address - 0xC000];
    }
    // Echo RAM (0xE000 - 0xFDFF) - mirror of WRAM
    else if (address < 0xFE00) {
        return m_wram[address - 0xE000];
    }
    // OAM (0xFE00 - 0xFE9F)
    else if (address < 0xFEA0) {
        if (m_ppu) {
            return m_ppu->readOAM(address);
        }
        return 0xFF;
    }
    // Unusable (0xFEA0 - 0xFEFF)
    else if (address < 0xFF00) {
        return 0xFF;
    }
    // I/O Registers (0xFF00 - 0xFF7F)
    else if (address < 0xFF80) {
        return readIO(address);
    }
    // High RAM (0xFF80 - 0xFFFE)
    else if (address < 0xFFFF) {
        return m_hram[address - 0xFF80];
    }
    // Interrupt Enable Register (0xFFFF)
    else {
        return m_ie;
    }
}

void MMU::write(u16 address, u8 value) {
    // ROM (0x0000 - 0x7FFF) and Cartridge RAM (0xA000 - 0xBFFF)
    if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
        if (m_cartridge) {
            m_cartridge->write(address, value);
        }
    }
    // VRAM (0x8000 - 0x9FFF)
    else if (address < 0xA000) {
        if (m_ppu) {
            m_ppu->writeVRAM(address, value);
        }
    }
    // Work RAM (0xC000 - 0xDFFF)
    else if (address < 0xE000) {
        m_wram[address - 0xC000] = value;
    }
    // Echo RAM (0xE000 - 0xFDFF)
    else if (address < 0xFE00) {
        m_wram[address - 0xE000] = value;
    }
    // OAM (0xFE00 - 0xFE9F)
    else if (address < 0xFEA0) {
        if (m_ppu) {
            m_ppu->writeOAM(address, value);
        }
    }
    // Unusable (0xFEA0 - 0xFEFF)
    else if (address < 0xFF00) {
        // Do nothing
    }
    // I/O Registers (0xFF00 - 0xFF7F)
    else if (address < 0xFF80) {
        writeIO(address, value);
    }
    // High RAM (0xFF80 - 0xFFFE)
    else if (address < 0xFFFF) {
        m_hram[address - 0xFF80] = value;
    }
    // Interrupt Enable Register (0xFFFF)
    else {
        m_ie = value;
    }
}

u8 MMU::readIO(u16 address) const {
    switch (address) {
        case 0xFF00: // Joypad
            return m_joypad ? m_joypad->read() : 0xFF;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TMA
        case 0xFF07: // TAC
            return m_timer ? m_timer->read(address) : 0xFF;
        case 0xFF0F: // IF (Interrupt Flag)
            // This is handled by CPU
            return 0xFF;
        // APU registers (0xFF10-0xFF26, 0xFF30-0xFF3F)
        case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13: case 0xFF14:
        case 0xFF16: case 0xFF17: case 0xFF18: case 0xFF19:
        case 0xFF1A: case 0xFF1B: case 0xFF1C: case 0xFF1D: case 0xFF1E:
        case 0xFF20: case 0xFF21: case 0xFF22: case 0xFF23:
        case 0xFF24: case 0xFF25: case 0xFF26:
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return m_apu ? m_apu->readRegister(address) : 0xFF;
        case 0xFF40: // LCDC
        case 0xFF41: // STAT
        case 0xFF42: // SCY
        case 0xFF43: // SCX
        case 0xFF44: // LY
        case 0xFF45: // LYC
        case 0xFF46: // DMA
        case 0xFF47: // BGP
        case 0xFF48: // OBP0
        case 0xFF49: // OBP1
        case 0xFF4A: // WY
        case 0xFF4B: // WX
            return m_ppu ? m_ppu->readRegister(address) : 0xFF;
        default:
            return 0xFF;
    }
}

void MMU::writeIO(u16 address, u8 value) {
    switch (address) {
        case 0xFF00: // Joypad
            if (m_joypad) {
                m_joypad->write(value);
            }
            break;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TMA
        case 0xFF07: // TAC
            if (m_timer) {
                m_timer->write(address, value);
            }
            break;
        case 0xFF0F: // IF (Interrupt Flag)
            // This is handled by CPU
            break;
        // APU registers (0xFF10-0xFF26, 0xFF30-0xFF3F)
        case 0xFF10: case 0xFF11: case 0xFF12: case 0xFF13: case 0xFF14:
        case 0xFF16: case 0xFF17: case 0xFF18: case 0xFF19:
        case 0xFF1A: case 0xFF1B: case 0xFF1C: case 0xFF1D: case 0xFF1E:
        case 0xFF20: case 0xFF21: case 0xFF22: case 0xFF23:
        case 0xFF24: case 0xFF25: case 0xFF26:
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            if (m_apu) {
                m_apu->writeRegister(address, value);
            }
            break;
        case 0xFF40: // LCDC
        case 0xFF41: // STAT
        case 0xFF42: // SCY
        case 0xFF43: // SCX
        case 0xFF44: // LY
        case 0xFF45: // LYC
        case 0xFF46: // DMA
        case 0xFF47: // BGP
        case 0xFF48: // OBP0
        case 0xFF49: // OBP1
        case 0xFF4A: // WY
        case 0xFF4B: // WX
            if (m_ppu) {
                m_ppu->writeRegister(address, value);
            }
            break;
        default:
            break;
    }
}

