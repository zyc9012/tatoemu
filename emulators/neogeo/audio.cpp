#include "audio.h"
#include "sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "consts.h"
#include <cstring>

extern "C" {

#include "../components/sound/fm/fm.h"
#include "../components/sound/ay8910/ay8910.h"

}  // extern "C"

namespace neogeo {

// YM2610 clock frequency
static constexpr u32 YM2610_CLOCK = 8000000;

// Static pointer to SoundCPU for interrupt handler callback
static SoundCPU* s_ym2610SoundCpu = nullptr;

// Static pointer to Audio for timer handler callback
static Audio* s_ym2610Audio = nullptr;

// YM2610 IRQ handler - sets/clears Z80 IRQ line based on YM2610 timer interrupt status
static void ym2610IrqHandler(int chip, int irq) {
    (void)chip;  // Unused, we only have one chip
    if (s_ym2610SoundCpu) {
        s_ym2610SoundCpu->irq(irq != 0);
    }
}

// YM2610 Timer handler - called when YM2610 starts/stops a timer
// n: chip number, c: timer (0=A, 1=B), cnt: counter value (0=stop), stepTime: time per tick in seconds
static void ym2610TimerHandler(int n, int c, int cnt, double stepTime) {
    (void)n;  // Unused, we only have one chip
    if (!s_ym2610Audio) return;
    
    if (cnt == 0) {
        // Timer stopped
        s_ym2610Audio->setTimer(c, -1);
    } else {
        // Timer started
        // Calculate Z80 cycles until timer fires: cnt * stepTime * SOUND_CPU_FREQUENCY
        // stepTime is the time per timer tick in seconds (TimerBase from YM2610)
        double periodSeconds = cnt * stepTime;
        s32 cycles = static_cast<s32>(periodSeconds * SOUND_CPU_FREQUENCY);
        s_ym2610Audio->setTimer(c, cycles);
    }
}

Audio::Audio()
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
    , m_timerA(-1)
    , m_timerB(-1)
    , m_cycleAccumulator(0)
    , m_cyclesPerSample(0) {
}

Audio::~Audio() {
    YM2610Shutdown();
    AY8910Exit(0);
    
    s_ym2610SoundCpu = nullptr;
    s_ym2610Audio = nullptr;
}

void Audio::setSoundCPU(SoundCPU* soundCpu) {
    m_soundCpu = soundCpu;
    // Update the static pointers so the IRQ/timer handlers can work
    s_ym2610SoundCpu = soundCpu;
    s_ym2610Audio = this;
}

void Audio::init(u32 sampleRate) {
    m_sampleRate = sampleRate;
    // Calculate cycles per sample based on Z80 frequency
    if (m_sampleRate > 0) {
        m_cyclesPerSample = SOUND_CPU_FREQUENCY / m_sampleRate;
    }

    // Get ADPCM ROM data from cartridge
    u8* adpcmRomA = const_cast<u8*>(m_cartridge->getADPCMROM());
    int adpcmRomASize = static_cast<int>(m_cartridge->getADPCMROMSize());
    // For Neo Geo, ADPCM-A and ADPCM-B use the same ROM
    u8* adpcmRomB = adpcmRomA;
    int adpcmRomBSize = adpcmRomASize;
    
    // Initialize AY8910 (must be done before YM2610)
    AY8910Exit(0);
    AY8910InitYM(0, YM2610_CLOCK, static_cast<int>(sampleRate), nullptr, nullptr, nullptr, nullptr, []{});

    // Initialize YM2610 with timer and IRQ handlers
    YM2610Shutdown();
    int result = YM2610Init(1, 0, YM2610_CLOCK, static_cast<int>(sampleRate),
                           (void**)&adpcmRomA, &adpcmRomASize,
                           (void**)&adpcmRomB, &adpcmRomBSize,
                           ym2610TimerHandler, ym2610IrqHandler);
    
    if (result != 0) {
        log_error("Error: Failed to initialize YM2610");
    }

    // Reset YM2610
    YM2610ResetChip(0);
}

void Audio::reset() {
    m_soundCommand = 0;
    m_soundReply = 0;
    m_soundStatus = false;
    m_nmiEnabled = false;
    m_cycleAccumulator = 0;
    
    // Reset timers
    m_timerA = -1;
    m_timerB = -1;
    
    init(m_sampleRate);
}

void Audio::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;

    init(sampleRate);
}

void Audio::setTimer(int timer, s32 cycles) {
    if (timer == 0) {
        m_timerA = cycles;
    } else {
        m_timerB = cycles;
    }
}

void Audio::updateTimers(u32 cycles) {
    // Update Timer A
    if (m_timerA >= 0) {
        m_timerA -= static_cast<s32>(cycles);
        if (m_timerA <= 0) {
            YM2610TimerOver(0, 0);
        }
    }
    
    // Update Timer B
    if (m_timerB >= 0) {
        m_timerB -= static_cast<s32>(cycles);
        if (m_timerB <= 0) {
            YM2610TimerOver(0, 1);
        }
    }
}

void Audio::step(u32 cycles, double gameSpeed) {
    // Accumulate cycles and generate samples when needed
    m_cycleAccumulator += cycles;
    
    // Generate audio samples based on accumulated cycles
    while (m_cycleAccumulator >= m_cyclesPerSample * gameSpeed) {
        m_cycleAccumulator -= m_cyclesPerSample * gameSpeed;
    
        // Generate YM2610 samples
        s16 ym2610Left = 0;
        s16 ym2610Right = 0;
        s16* ym2610Buffers[2] = { &ym2610Left, &ym2610Right };
        YM2610UpdateOne(0, ym2610Buffers, 1);

        // Generate AY8910 samples
        s16 ay8910ChanA = 0;
        s16 ay8910ChanB = 0;
        s16 ay8910ChanC = 0;
        s16* ay8910Buffers[3] = { &ay8910ChanA, &ay8910ChanB, &ay8910ChanC };
        AY8910Update(0, ay8910Buffers, 1);

        // Mix AY8910 channels (A + B + C) with 0.20 volume to both channels (like FBNeo)
        s32 ay8910Mixed = ay8910ChanA + ay8910ChanB + ay8910ChanC;
        ay8910Mixed = std::clamp(ay8910Mixed, -32768, 32767);
        float ay8910Volume = 0.20f;

        // Combine YM2610 and AY8910 samples
        s16 leftBuf = ym2610Left + static_cast<s16>(ay8910Mixed * ay8910Volume);
        s16 rightBuf = ym2610Right + static_cast<s16>(ay8910Mixed * ay8910Volume);

        // Apply master volume and convert to float
        float left = leftBuf / 32768.0f * m_volume;
        float right = rightBuf / 32768.0f * m_volume;
        
        float samples[2] = {left, right};
        m_audioDevice->writeSamples(samples, 2 * sizeof(float));
    }
}

u8 Audio::readPort(u16 port) {
    switch (port & 0xFF) {
        case 0x00:
            // Read sound command from 68000
            m_soundStatus = true;
            return m_soundCommand;
            
        case 0x04:
        case 0x05:
        case 0x06:
            return YM2610Read(0, port & 3);
            
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // Bank switch commands - handled by Memory class via I/O read
            return 0x00;
            
        default:
            return 0x00;
    }
}

void Audio::writePort(u16 port, u8 value) {
    switch (port & 0xFF) {
        case 0x00:
            // Clear sound command (acknowledge)
            m_soundCommand = 0;
            break;
            
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // YM2610 data write
            YM2610Write(0, port & 3, value);
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
            break;
            
        case 0x80:
        case 0xC0:
        case 0xC1:
        case 0xC2:
            // NOP - these are used for timing/sync but don't need handling
            break;
            
        default:
            break;
    }
}

void Audio::setSoundCommand(u8 command) {
    m_soundCommand = command;
    m_soundStatus = false;
    
    // Trigger NMI to Z80 if enabled
    if (m_nmiEnabled) {
        m_soundCpu->nmi();
    }
}

void Audio::saveState(Buffer* buf) {
    buffer_write(buf, &m_soundCommand, sizeof(m_soundCommand));
    buffer_write(buf, &m_soundReply, sizeof(m_soundReply));
    buffer_write(buf, &m_soundStatus, sizeof(m_soundStatus));
    buffer_write(buf, &m_nmiEnabled, sizeof(m_nmiEnabled));
    buffer_write(buf, &m_timerA, sizeof(m_timerA));
    buffer_write(buf, &m_timerB, sizeof(m_timerB));

    AY8910SaveContext(buf);
    YM2610SaveContext(buf);
}

void Audio::loadState(Buffer* buf) {
    buffer_read(buf, &m_soundCommand, sizeof(m_soundCommand));
    buffer_read(buf, &m_soundReply, sizeof(m_soundReply));
    buffer_read(buf, &m_soundStatus, sizeof(m_soundStatus));
    buffer_read(buf, &m_nmiEnabled, sizeof(m_nmiEnabled));
    buffer_read(buf, &m_timerA, sizeof(m_timerA));
    buffer_read(buf, &m_timerB, sizeof(m_timerB));

    AY8910LoadContext(buf);
    YM2610LoadContext(buf);
}

} // namespace neogeo
