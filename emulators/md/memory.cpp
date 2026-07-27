#include "memory.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "vdp.h"
#include "audio.h"
#include "cartridge.h"
#include "controller.h"
#include "config.h"

namespace md {

Memory::Memory() {
    reset();
}

void Memory::reset() {
    m_workRam.fill(0);
    m_z80Ram.fill(0);
    m_z80BankBase = 0;
    m_z80BankShift = 0;
    m_z80BankLatch = 0;
}

// ---------------------------------------------------------------------------
// I/O area (0xA10000-0xA1001F) and the Z80 control lines
// ---------------------------------------------------------------------------

u8 Memory::readIO(u32 address) {
    const u32 reg = (address >> 1) & 0x0F;

    switch (reg) {
        case 0x00: {
            // Version register: overseas/domestic, PAL/NTSC, no expansion.
            u8 value = 0x20;  // no Sega CD attached
            if (Config::Region != 0) value |= 0x80;  // overseas
            if (Config::Region == 2) value |= 0x40;  // PAL
            return value;
        }
        case 0x01: return m_controller ? m_controller->readData(0) : 0x7F;
        case 0x02: return m_controller ? m_controller->readData(1) : 0x7F;
        case 0x03: return 0x7F;  // expansion port
        case 0x04: return m_controller ? m_controller->readCtrl(0) : 0x00;
        case 0x05: return m_controller ? m_controller->readCtrl(1) : 0x00;
        case 0x06: return 0x00;
        default:   return 0x00;  // serial control registers
    }
}

void Memory::writeIO(u32 address, u8 value) {
    const u32 reg = (address >> 1) & 0x0F;

    switch (reg) {
        case 0x01: if (m_controller) m_controller->writeData(0, value); break;
        case 0x02: if (m_controller) m_controller->writeData(1, value); break;
        case 0x04: if (m_controller) m_controller->writeCtrl(0, value); break;
        case 0x05: if (m_controller) m_controller->writeCtrl(1, value); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// VDP ports (0xC00000-0xC0001F)
// ---------------------------------------------------------------------------

u16 Memory::readVDPPort(u32 address) {
    if (!m_vdp) return 0;

    const u32 offset = address & 0x1F;

    if (offset < 0x04) return m_vdp->readData();
    if (offset < 0x08) return m_vdp->readControl();
    if (offset < 0x10) return m_vdp->readHVCounter();
    return 0;
}

void Memory::writeVDPPort(u32 address, u16 value, bool byteAccess, u8 byteValue) {
    if (!m_vdp) return;

    const u32 offset = address & 0x1F;

    if (offset < 0x04) {
        // Byte writes to the data port duplicate the byte into both halves.
        m_vdp->writeData(byteAccess ? static_cast<u16>((byteValue << 8) | byteValue) : value);
        return;
    }
    if (offset < 0x08) {
        m_vdp->writeControl(byteAccess ? static_cast<u16>((byteValue << 8) | byteValue) : value);
        return;
    }
    if (offset >= 0x10 && offset < 0x18) {
        // PSG, byte port at 0xC00011.
        if (m_audio) m_audio->writePSG(byteAccess ? byteValue : static_cast<u8>(value & 0xFF));
        return;
    }
}

// ---------------------------------------------------------------------------
// 68000 bus
// ---------------------------------------------------------------------------

u8 Memory::read8(u32 address) {
    address &= 0xFFFFFF;

    if (address < 0x400000) {
        return m_cartridge ? m_cartridge->read8(address) : 0xFF;
    }

    if (address >= 0xE00000) {
        return m_workRam[address & (WORK_RAM_SIZE - 1)];
    }

    if (address >= 0xA00000 && address < 0xA10000) {
        // The 68000 may only touch the Z80 area while it owns the bus, but
        // enforcing that breaks more games than it fixes.
        return readZ80(static_cast<u16>(address & 0x7FFF));
    }

    if (address >= 0xA10000 && address < 0xA10020) {
        return readIO(address);
    }

    if (address == 0xA11100 || address == 0xA11101) {
        // Bit 0 (of the high byte) clear means the 68000 has been granted the bus.
        const bool granted = m_soundCpu && m_soundCpu->isBusGranted();
        if (address == 0xA11100) return granted ? 0x00 : 0x01;
        return 0x00;
    }

    if (address >= 0xC00000 && address < 0xC00020) {
        const u16 value = readVDPPort(address);
        return (address & 1) ? static_cast<u8>(value & 0xFF) : static_cast<u8>(value >> 8);
    }

    return 0xFF;
}

u16 Memory::read16(u32 address) {
    address &= 0xFFFFFE;

    if (address < 0x400000) {
        return m_cartridge ? m_cartridge->read16(address) : 0xFFFF;
    }

    if (address >= 0xE00000) {
        const u32 offset = address & (WORK_RAM_SIZE - 1);
        return static_cast<u16>((m_workRam[offset] << 8) | m_workRam[offset + 1]);
    }

    if (address >= 0xC00000 && address < 0xC00020) {
        return readVDPPort(address);
    }

    if (address >= 0xA00000 && address < 0xA10000) {
        // Word access to the Z80 area mirrors the byte into both halves.
        const u8 value = readZ80(static_cast<u16>(address & 0x7FFF));
        return static_cast<u16>((value << 8) | value);
    }

    if (address >= 0xA10000 && address < 0xA10020) {
        const u8 value = readIO(address | 1);
        return static_cast<u16>((value << 8) | value);
    }

    if (address == 0xA11100) {
        return (m_soundCpu && m_soundCpu->isBusGranted()) ? 0x0000 : 0x0100;
    }

    return 0xFFFF;
}

void Memory::write8(u32 address, u8 value) {
    address &= 0xFFFFFF;

    if (address >= 0xE00000) {
        m_workRam[address & (WORK_RAM_SIZE - 1)] = value;
        return;
    }

    if (address < 0x400000) {
        if (m_cartridge) m_cartridge->write8(address, value);
        return;
    }

    if (address >= 0xA00000 && address < 0xA10000) {
        writeZ80(static_cast<u16>(address & 0x7FFF), value);
        return;
    }

    if (address >= 0xA10000 && address < 0xA10020) {
        writeIO(address, value);
        return;
    }

    if (address == 0xA11100 || address == 0xA11101) {
        if (address == 0xA11100 && m_soundCpu) m_soundCpu->setBusRequest((value & 0x01) != 0);
        return;
    }

    if (address == 0xA11200 || address == 0xA11201) {
        if (address == 0xA11200 && m_soundCpu) m_soundCpu->setResetLine((value & 0x01) == 0);
        return;
    }

    if (address >= 0xA130F0 && address < 0xA13100) {
        if (m_cartridge) m_cartridge->writeControl(address, value);
        return;
    }

    if (address >= 0xC00000 && address < 0xC00020) {
        writeVDPPort(address, 0, true, value);
        return;
    }

    // 0xA14000 TMSS register and the Sega CD window are accepted and ignored.
}

void Memory::write16(u32 address, u16 value) {
    address &= 0xFFFFFE;

    if (address >= 0xE00000) {
        const u32 offset = address & (WORK_RAM_SIZE - 1);
        m_workRam[offset]     = static_cast<u8>(value >> 8);
        m_workRam[offset + 1] = static_cast<u8>(value & 0xFF);
        return;
    }

    if (address >= 0xC00000 && address < 0xC00020) {
        writeVDPPort(address, value, false, 0);
        return;
    }

    if (address < 0x400000) {
        if (m_cartridge) m_cartridge->write16(address, value);
        return;
    }

    if (address >= 0xA00000 && address < 0xA10000) {
        writeZ80(static_cast<u16>(address & 0x7FFF), static_cast<u8>(value >> 8));
        return;
    }

    if (address >= 0xA10000 && address < 0xA10020) {
        writeIO(address | 1, static_cast<u8>(value & 0xFF));
        return;
    }

    if (address == 0xA11100) {
        if (m_soundCpu) m_soundCpu->setBusRequest((value & 0x0100) != 0);
        return;
    }

    if (address == 0xA11200) {
        if (m_soundCpu) m_soundCpu->setResetLine((value & 0x0100) == 0);
        return;
    }

    if (address >= 0xA130F0 && address < 0xA13100) {
        if (m_cartridge) m_cartridge->writeControl(address | 1, static_cast<u8>(value & 0xFF));
        return;
    }
}

u16 Memory::readDMA16(u32 address) {
    address &= 0xFFFFFE;

    if (address < 0x400000) {
        return m_cartridge ? m_cartridge->read16(address) : 0xFFFF;
    }
    if (address >= 0xE00000) {
        const u32 offset = address & (WORK_RAM_SIZE - 1);
        return static_cast<u16>((m_workRam[offset] << 8) | m_workRam[offset + 1]);
    }
    return 0xFFFF;
}

// ---------------------------------------------------------------------------
// Z80 bus
// ---------------------------------------------------------------------------

u8 Memory::readZ80(u16 address) {
    if (address < 0x4000) {
        return m_z80Ram[address & (Z80_RAM_SIZE - 1)];
    }

    if (address < 0x6000) {
        return m_audio ? m_audio->readFM(static_cast<u8>(address & 3)) : 0xFF;
    }

    if (address < 0x7F00) {
        return 0xFF;
    }

    if (address < 0x7F20) {
        // The Z80 can reach the VDP ports, though doing so stalls real hardware.
        const u16 value = readVDPPort(address & 0x1F);
        return (address & 1) ? static_cast<u8>(value & 0xFF) : static_cast<u8>(value >> 8);
    }

    if (address >= 0x8000) {
        const u32 target = m_z80BankBase | (address & 0x7FFF);
        return read8(target);
    }

    return 0xFF;
}

void Memory::writeZ80(u16 address, u8 value) {
    if (address < 0x4000) {
        m_z80Ram[address & (Z80_RAM_SIZE - 1)] = value;
        return;
    }

    if (address < 0x6000) {
        if (m_audio) m_audio->writeFM(static_cast<u8>(address & 3), value);
        return;
    }

    if (address < 0x6100) {
        // Bank register: nine writes shift one bit at a time into the base.
        m_z80BankLatch = (m_z80BankLatch >> 1) | ((value & 1) ? 0x100u : 0u);
        if (++m_z80BankShift >= 9) {
            m_z80BankShift = 0;
            m_z80BankBase = m_z80BankLatch << 15;
        }
        return;
    }

    if (address >= 0x7F00 && address < 0x7F20) {
        writeVDPPort(address & 0x1F, 0, true, value);
        return;
    }

    if (address >= 0x8000) {
        const u32 target = m_z80BankBase | (address & 0x7FFF);
        write8(target, value);
        return;
    }
}

// ---------------------------------------------------------------------------
// Save states
// ---------------------------------------------------------------------------

void Memory::saveState(Buffer* buf) {
    buffer_write(buf, m_workRam.data(), m_workRam.size());
    buffer_write(buf, m_z80Ram.data(), m_z80Ram.size());
    buffer_write(buf, &m_z80BankBase, sizeof(m_z80BankBase));
    buffer_write(buf, &m_z80BankShift, sizeof(m_z80BankShift));
    buffer_write(buf, &m_z80BankLatch, sizeof(m_z80BankLatch));
}

void Memory::loadState(Buffer* buf) {
    buffer_read(buf, m_workRam.data(), m_workRam.size());
    buffer_read(buf, m_z80Ram.data(), m_z80Ram.size());
    buffer_read(buf, &m_z80BankBase, sizeof(m_z80BankBase));
    buffer_read(buf, &m_z80BankShift, sizeof(m_z80BankShift));
    buffer_read(buf, &m_z80BankLatch, sizeof(m_z80BankLatch));
}

} // namespace md
