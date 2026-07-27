#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"
#include <array>

namespace md {

class CPU;
class SoundCPU;
class VDP;
class Audio;
class Cartridge;
class Controller;

// ---------------------------------------------------------------------------
// System bus.
//
// Routes 68000 and Z80 accesses to the cartridge, work RAM, sound RAM, VDP,
// FM/PSG chips and the controller I/O area, and implements the Z80 bus request
// and reset lines plus the Z80's 68000 bank window.
// ---------------------------------------------------------------------------
class Memory {
public:
    Memory();

    void reset();

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setSoundCPU(SoundCPU* soundCpu) { m_soundCpu = soundCpu; }
    void setVDP(VDP* vdp) { m_vdp = vdp; }
    void setAudio(Audio* audio) { m_audio = audio; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setController(Controller* controller) { m_controller = controller; }

    // --- 68000 bus ---
    u8   read8(u32 address);
    u16  read16(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);

    // Source reads for VDP DMA; these bypass the VDP itself.
    u16  readDMA16(u32 address);

    // --- Z80 bus ---
    u8   readZ80(u16 address);
    void writeZ80(u16 address, u8 value);

    u8* getWorkRAM() { return m_workRam.data(); }

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    u8   readIO(u32 address);
    void writeIO(u32 address, u8 value);
    u16  readVDPPort(u32 address);
    void writeVDPPort(u32 address, u16 value, bool byteAccess, u8 byteValue);

    CPU* m_cpu = nullptr;
    SoundCPU* m_soundCpu = nullptr;
    VDP* m_vdp = nullptr;
    Audio* m_audio = nullptr;
    Cartridge* m_cartridge = nullptr;
    Controller* m_controller = nullptr;

    std::array<u8, WORK_RAM_SIZE> m_workRam{};
    std::array<u8, Z80_RAM_SIZE> m_z80Ram{};

    // Base of the Z80's 32 KB window into the 68000 address space.
    u32 m_z80BankBase = 0;
    u32 m_z80BankShift = 0;
    u32 m_z80BankLatch = 0;
};

} // namespace md
