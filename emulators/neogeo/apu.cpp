#include "apu.h"
#include "sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "consts.h"
#include <iostream>

namespace neogeo {

APU::APU()
    : m_soundCpu(nullptr)
    , m_memory(nullptr)
    , m_cartridge(nullptr)
    , m_audioDevice(nullptr)
    , m_sampleRate(44100)
    , m_volume(1.0f)
    , m_soundCommand(0)
    , m_soundReply(0)
    , m_soundStatus(false)
    , m_nmiEnabled(false)
    , m_cycleAccumulator(0)
    , m_cyclesPerSample(0) {
    m_ym2610RegSelect[0] = 0;
    m_ym2610RegSelect[1] = 0;
}

void APU::reset() {
    m_soundCommand = 0;
    m_soundReply = 0;
    m_soundStatus = false;
    m_nmiEnabled = false;
    m_ym2610RegSelect[0] = 0;
    m_ym2610RegSelect[1] = 0;
    m_cycleAccumulator = 0;
    
    // Calculate cycles per sample based on Z80 frequency
    if (m_sampleRate > 0) {
        m_cyclesPerSample = SOUND_CPU_FREQUENCY / m_sampleRate;
    }
}

void APU::step(u32 cycles, double gameSpeed) {
    // Stub: In a full implementation, this would:
    // 1. Run the YM2610 emulation
    // 2. Generate audio samples
    // 3. Mix FM and ADPCM channels
    // 4. Output to audio device
    (void)cycles;
    (void)gameSpeed;
}

u8 APU::readPort(u16 port) {
    switch (port & 0xFF) {
        case 0x00:
            // Read sound command from 68000
            m_soundStatus = true;
            return m_soundCommand;
            
        case 0x04:
            // YM2610 status register (address A)
            // Stub: Return ready status
            return 0x00;
            
        case 0x05:
            // YM2610 data read (address A)
            return 0x00;
            
        case 0x06:
            // YM2610 status register (address B)
            return 0x00;
            
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // Bank switch commands - handled by Memory class via I/O read
            // The high byte of the port address contains the bank number
            // This is handled in Memory::readZ80IO
            return 0x00;
            
        default:
            return 0x00;
    }
}

void APU::writePort(u16 port, u8 value) {
    switch (port & 0xFF) {
        case 0x00:
            // Clear sound command
            m_soundCommand = 0;
            break;
            
        case 0x04:
            // YM2610 address write (address A)
            m_ym2610RegSelect[0] = value;
            break;
            
        case 0x05:
            // YM2610 data write (address A)
            // Stub: Would write to YM2610 register
            break;
            
        case 0x06:
            // YM2610 address write (address B)
            m_ym2610RegSelect[1] = value;
            break;
            
        case 0x07:
            // YM2610 data write (address B)
            // Stub: Would write to YM2610 register
            break;
            
        case 0x08:
            // Enable NMI
            m_nmiEnabled = true;
            break;
            
        case 0x18:
            // Disable NMI
            m_nmiEnabled = false;
            break;
            
        case 0x0C:
            // Write reply to 68000
            m_soundReply = value;
            // In FBNeo this calls ZetRunEnd() to return control
            break;
            
        case 0x80:
        case 0xC0:
        case 0xC1:
        case 0xC2:
            // NOP
            break;
            
        default:
            break;
    }
}

void APU::setSoundCommand(u8 command) {
    m_soundCommand = command;
    m_soundStatus = false;
    
    // Trigger NMI to Z80 if enabled
    if (m_nmiEnabled && m_soundCpu) {
        m_soundCpu->nmi();
    }
}

void APU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.write(reinterpret_cast<const char*>(&m_soundReply), sizeof(m_soundReply));
    file.write(reinterpret_cast<const char*>(&m_soundStatus), sizeof(m_soundStatus));
    file.write(reinterpret_cast<const char*>(&m_nmiEnabled), sizeof(m_nmiEnabled));
    file.write(reinterpret_cast<const char*>(m_ym2610RegSelect), sizeof(m_ym2610RegSelect));
}

void APU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_soundCommand), sizeof(m_soundCommand));
    file.read(reinterpret_cast<char*>(&m_soundReply), sizeof(m_soundReply));
    file.read(reinterpret_cast<char*>(&m_soundStatus), sizeof(m_soundStatus));
    file.read(reinterpret_cast<char*>(&m_nmiEnabled), sizeof(m_nmiEnabled));
    file.read(reinterpret_cast<char*>(m_ym2610RegSelect), sizeof(m_ym2610RegSelect));
}

} // namespace neogeo
