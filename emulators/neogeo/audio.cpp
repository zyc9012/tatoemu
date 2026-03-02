#include "audio.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "consts.h"
#include <cstring>

extern "C" {

#include "../components/sound/fm/fm.h"
#include "../components/sound/ay8910/ay8910.h"

}  // extern "C"

// Forward declare the Audio pointer used by the update-request callback
namespace neogeo { class Audio; }
static neogeo::Audio* s_ym2610Audio = nullptr;

// YM2610UpdateReq callback - called from within YM2610Write (in fm.c)
extern "C" void YM2610UpdateReqCallback(void) {
    if (s_ym2610Audio) {
        s_ym2610Audio->renderUpTo();
    }
}

namespace neogeo {

// YM2610 clock frequency
static constexpr u32 YM2610_CLOCK = 8000000;

// Static pointer to SoundCPU for interrupt handler callback
static SoundCPU* s_ym2610SoundCpu = nullptr;

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
    , m_cpu(nullptr)
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
    , m_ym2610Position(0)
    , m_ay8910Position(0) {
    m_ym2610Left.fill(0);
    m_ym2610Right.fill(0);
    m_ay8910A.fill(0);
    m_ay8910B.fill(0);
    m_ay8910C.fill(0);
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
    
    // Reset timers
    m_timerA = -1;
    m_timerB = -1;

    // Reset buffer positions
    m_ym2610Position = 0;
    m_ay8910Position = 0;
    
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

u32 Audio::cyclesToNextTimer() const {
    u32 next = UINT32_MAX;
    if (m_timerA >= 0 && static_cast<u32>(m_timerA) < next) {
        next = static_cast<u32>(m_timerA);
    }
    if (m_timerB >= 0 && static_cast<u32>(m_timerB) < next) {
        next = static_cast<u32>(m_timerB);
    }
    return next;
}

u32 Audio::computeSamplesNeeded() const {
    // Convert current Z80 cycle position to number of samples that should
    // have been generated by now
    u32 z80Cycles = m_soundCpu->frameCycles();
    return static_cast<u32>(static_cast<u64>(z80Cycles) * m_sampleRate / SOUND_CPU_FREQUENCY) + 1;
}

void Audio::renderSamples(u32 samplesNeeded) {
    if (samplesNeeded > AUDIO_BUFFER_SIZE) {
        samplesNeeded = AUDIO_BUFFER_SIZE;
    }

    // Render YM2610 (FM + ADPCM)
    if (samplesNeeded > m_ym2610Position) {
        u32 count = samplesNeeded - m_ym2610Position;
        s16* bufs[2] = {
            m_ym2610Left.data() + m_ym2610Position,
            m_ym2610Right.data() + m_ym2610Position
        };
        YM2610UpdateOne(0, bufs, static_cast<int>(count));
        m_ym2610Position = samplesNeeded;
    }

    // Render AY8910 (SSG)
    if (samplesNeeded > m_ay8910Position) {
        u32 count = samplesNeeded - m_ay8910Position;
        s16* bufs[3] = {
            m_ay8910A.data() + m_ay8910Position,
            m_ay8910B.data() + m_ay8910Position,
            m_ay8910C.data() + m_ay8910Position
        };
        AY8910Update(0, bufs, static_cast<int>(count));
        m_ay8910Position = samplesNeeded;
    }
}

void Audio::renderUpTo() {
    // Called by YM2610UpdateReq before register writes so the chip's
    // internal state is captured into buffers before it changes.
    renderSamples(computeSamplesNeeded());
}

void Audio::endFrame(double gameSpeed) {
    if (!m_audioDevice) return;

    // Catch up Z80 to end of frame
    runSoundCPUTo(static_cast<s32>(SOUND_CPU_CYCLES_PER_FRAME));

    // Render any remaining samples that haven't been rendered yet
    u32 totalSamples = computeSamplesNeeded();
    renderSamples(totalSamples);

    // Compute output sample count.
    u32 outputSamples = (gameSpeed > 0.0)
        ? static_cast<u32>(totalSamples / gameSpeed)
        : totalSamples;

    constexpr float ay8910Volume = 0.20f;
    u32 mixPos = 0;

    for (u32 i = 0; i < outputSamples; i++) {
        // Map output sample index back to source buffer index for speed adjustment
        u32 src = (gameSpeed > 0.0 && gameSpeed != 1.0)
            ? static_cast<u32>(i * gameSpeed)
            : i;
        if (src >= totalSamples) src = totalSamples - 1;

        s32 ayMixed = m_ay8910A[src] + m_ay8910B[src] + m_ay8910C[src];
        ayMixed = std::clamp(ayMixed, -32768, 32767);

        s32 left = m_ym2610Left[src] + static_cast<s16>(ayMixed * ay8910Volume);
        s32 right = m_ym2610Right[src] + static_cast<s16>(ayMixed * ay8910Volume);

        m_mixBuffer[mixPos++] = std::clamp(left, -32768, 32767) / 32768.0f * m_volume;
        m_mixBuffer[mixPos++] = std::clamp(right, -32768, 32767) / 32768.0f * m_volume;

        // Flush when mix buffer is full
        if (mixPos >= m_mixBuffer.size()) {
            m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
            mixPos = 0;
        }
    }

    // Flush remaining samples
    if (mixPos > 0) {
        m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
    }

    // Reset buffer positions for next frame
    m_ym2610Position = 0;
    m_ay8910Position = 0;
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

void Audio::runSoundCPUTo(s32 targetZ80Cycle) {
    s32 remaining = targetZ80Cycle - static_cast<s32>(m_soundCpu->frameCycles());
    if (remaining > 0) {
        m_soundCpu->step(static_cast<u32>(remaining));
    }
}

void Audio::setSoundCommand(u8 command) {
    // Catch up Z80 to current 68K position before delivering the command
    runSoundCPUTo(static_cast<s32>(m_cpu->frameCycles() * SOUND_CYCLES_RATIO));

    m_soundCommand = command;
    m_soundStatus = false;
    
    // Trigger NMI to Z80 if enabled
    if (m_nmiEnabled) {
        m_soundCpu->nmi();
    }
}

u8 Audio::getSoundReply() {
    // Catch up Z80 so it has had time to process and write its reply
    runSoundCPUTo(static_cast<s32>(m_cpu->frameCycles() * SOUND_CYCLES_RATIO));
    return m_soundReply;
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
