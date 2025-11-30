#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "cartridge.h"
#include "controller.h"

namespace nes {

Memory::Memory()
    : m_cpu(nullptr)
    , m_ppu(nullptr)
    , m_apu(nullptr)
    , m_cartridge(nullptr)
    , m_controller1(nullptr)
    , m_controller2(nullptr)
    , m_controllerStrobe(false) {
    reset();
}

void Memory::reset() {
    m_ram.fill(0);
    m_controllerStrobe = false;
}

u8 Memory::cpuRead(u16 address) {
    if (address < 0x2000) {
        // Internal RAM (2KB mirrored)
        return m_ram[address & 0x07FF];
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        if (m_ppu) {
            return m_ppu->readRegister(address);
        }
        return 0;
    }
    else if (address < 0x4018) {
        // APU and I/O registers
        switch (address) {
            case 0x4015:
                // APU status
                if (m_apu) {
                    return m_apu->readStatus();
                }
                return 0;
                
            case 0x4016:
                // Controller 1
                if (m_controller1) {
                    return m_controller1->read();
                }
                return 0;
                
            case 0x4017:
                // Controller 2
                if (m_controller2) {
                    return m_controller2->read();
                }
                return 0;
                
            default:
                // Other APU registers are write-only
                return 0;
        }
    }
    else if (address < 0x4020) {
        // APU and I/O functionality that is normally disabled
        return 0;
    }
    else {
        // Cartridge space ($4020-$FFFF)
        if (m_cartridge) {
            return m_cartridge->cpuRead(address);
        }
        return 0;
    }
}

void Memory::cpuWrite(u16 address, u8 value) {
    if (address < 0x2000) {
        // Internal RAM (2KB mirrored)
        m_ram[address & 0x07FF] = value;
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        if (m_ppu) {
            m_ppu->writeRegister(address, value);
        }
    }
    else if (address < 0x4018) {
        // APU and I/O registers
        switch (address) {
            case 0x4014:
                // OAM DMA
                if (m_cpu && m_ppu) {
                    // Trigger DMA - copy 256 bytes from $XX00-$XXFF to OAM
                    m_cpu->triggerOAMDMA(value);
                    
                    // Perform the DMA transfer
                    u16 baseAddr = static_cast<u16>(value) << 8;
                    for (u16 i = 0; i < 256; i++) {
                        u8 data = cpuRead(baseAddr + i);
                        m_ppu->writeRegister(0x2004, data);  // Write to OAMDATA
                    }
                }
                break;
                
            case 0x4016:
                // Controller strobe
                if (m_controller1) {
                    m_controller1->write(value);
                }
                if (m_controller2) {
                    m_controller2->write(value);
                }
                break;
                
            case 0x4000: case 0x4001: case 0x4002: case 0x4003:  // Pulse 1
            case 0x4004: case 0x4005: case 0x4006: case 0x4007:  // Pulse 2
            case 0x4008: case 0x4009: case 0x400A: case 0x400B:  // Triangle
            case 0x400C: case 0x400D: case 0x400E: case 0x400F:  // Noise
            case 0x4010: case 0x4011: case 0x4012: case 0x4013:  // DMC
            case 0x4015:  // APU status
            case 0x4017:  // APU frame counter
                if (m_apu) {
                    m_apu->writeRegister(address, value);
                }
                break;
                
            default:
                break;
        }
    }
    else if (address < 0x4020) {
        // APU and I/O functionality that is normally disabled
    }
    else {
        // Cartridge space ($4020-$FFFF)
        if (m_cartridge) {
            m_cartridge->cpuWrite(address, value);
        }
    }
}

void Memory::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(m_ram.data()), m_ram.size());
    file.write(reinterpret_cast<const char*>(&m_controllerStrobe), sizeof(m_controllerStrobe));
}

void Memory::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_ram.data()), m_ram.size());
    file.read(reinterpret_cast<char*>(&m_controllerStrobe), sizeof(m_controllerStrobe));
}

} // namespace nes
