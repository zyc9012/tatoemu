#include "audio.h"
#include "cpu.h"
#include <algorithm>
#include <cstring>

#include "../components/compact.h"

extern "C" {
#include "../components/sound/fm/fm.h"
}

namespace md {

namespace {

// The YM2612 is clocked at the 68000 frequency; the PSG at the Z80 frequency.
constexpr u32 YM2612_CLOCK = M68K_CLOCK;
constexpr u32 PSG_CLOCK    = Z80_CLOCK;

// Relative levels; the PSG is noticeably quieter than the FM chip on hardware.
constexpr float FM_GAIN  = 1.0f;
constexpr float PSG_GAIN = 0.5f;

Audio* s_audio = nullptr;

// The YM2612 interrupt line is not connected on the Mega Drive.
void ym2612IrqHandler(int, int) {}

// Called by fm.c whenever a timer is started or stopped.
void ym2612TimerHandler(int, int channel, int count, double stepTime) {
    if (!s_audio) return;

    if (count == 0) {
        s_audio->setTimer(channel, -1);
        return;
    }

    const double periodSeconds = count * stepTime;
    s_audio->setTimer(channel, static_cast<s32>(periodSeconds * M68K_CLOCK));
}

} // namespace

Audio::Audio() {
    s_audio = this;
    m_fmLeft.fill(0);
    m_fmRight.fill(0);
    m_psgOut.fill(0);
}

Audio::~Audio() {
    YM2612Shutdown();
    if (s_audio == this) s_audio = nullptr;
}

void Audio::init(u32 sampleRate) {
    m_sampleRate = sampleRate ? sampleRate : 44100;

    YM2612Shutdown();
    if (YM2612Init(1, 0, static_cast<int>(YM2612_CLOCK), static_cast<int>(m_sampleRate),
                   ym2612TimerHandler, ym2612IrqHandler) != 0) {
        log_error("Error: Failed to initialize YM2612");
    }
    YM2612ResetChip(0);

    m_psg.init(PSG_CLOCK, m_sampleRate);
}

void Audio::reset() {
    m_timerA = -1;
    m_timerB = -1;
    m_fmPosition = 0;
    m_psgPosition = 0;

    init(m_sampleRate);
}

void Audio::setSampleRate(u32 sampleRate) {
    if (sampleRate == m_sampleRate) return;
    init(sampleRate);
}

void Audio::setTimer(int timer, s32 cycles) {
    if (timer == 0) m_timerA = cycles;
    else            m_timerB = cycles;
}

void Audio::updateTimers(u32 m68kCycles) {
    if (m_timerA >= 0) {
        m_timerA -= static_cast<s32>(m68kCycles);
        if (m_timerA <= 0) YM2612TimerOver(0, 0);
    }
    if (m_timerB >= 0) {
        m_timerB -= static_cast<s32>(m68kCycles);
        if (m_timerB <= 0) YM2612TimerOver(0, 1);
    }
}

void Audio::writeFM(u8 port, u8 value) {
    renderUpTo();
    YM2612Write(0, port & 3, value);
}

u8 Audio::readFM(u8 port) {
    return YM2612Read(0, port & 3);
}

void Audio::writePSG(u8 value) {
    renderUpTo();
    m_psg.write(value);
}

u32 Audio::computeSamplesNeeded() const {
    const u32 cycles = m_cpu ? m_cpu->frameCycles() : 0;
    u32 samples = static_cast<u32>(
        (static_cast<u64>(cycles) * m_sampleRate) / M68K_CLOCK) + 1;
    return std::min(samples, AUDIO_BUFFER_SIZE);
}

void Audio::renderSamples(u32 samplesNeeded) {
    if (samplesNeeded > AUDIO_BUFFER_SIZE) samplesNeeded = AUDIO_BUFFER_SIZE;

    if (samplesNeeded > m_fmPosition) {
        const u32 count = samplesNeeded - m_fmPosition;
        s16* buffers[2] = {
            m_fmLeft.data() + m_fmPosition,
            m_fmRight.data() + m_fmPosition
        };
        YM2612UpdateOne(0, buffers, static_cast<int>(count));
        m_fmPosition = samplesNeeded;
    }

    if (samplesNeeded > m_psgPosition) {
        const u32 count = samplesNeeded - m_psgPosition;
        m_psg.update(m_psgOut.data() + m_psgPosition, count);
        m_psgPosition = samplesNeeded;
    }
}

void Audio::renderUpTo() {
    renderSamples(computeSamplesNeeded());
}

void Audio::endFrame(double gameSpeed) {
    const u32 totalSamples = computeSamplesNeeded();
    renderSamples(totalSamples);

    if (!m_audioDevice || totalSamples == 0) {
        m_fmPosition = 0;
        m_psgPosition = 0;
        return;
    }

    const u32 outputSamples = (gameSpeed > 0.0)
        ? static_cast<u32>(totalSamples / gameSpeed)
        : totalSamples;

    u32 mixPos = 0;
    for (u32 i = 0; i < outputSamples; i++) {
        u32 src = (gameSpeed > 0.0 && gameSpeed != 1.0)
            ? static_cast<u32>(i * gameSpeed)
            : i;
        if (src >= totalSamples) src = totalSamples - 1;

        const float psg = m_psgOut[src] * PSG_GAIN;
        float left  = m_fmLeft[src]  * FM_GAIN + psg;
        float right = m_fmRight[src] * FM_GAIN + psg;

        left  = std::clamp(left,  -32768.0f, 32767.0f);
        right = std::clamp(right, -32768.0f, 32767.0f);

        m_mixBuffer[mixPos++] = left  / 32768.0f * m_volume;
        m_mixBuffer[mixPos++] = right / 32768.0f * m_volume;

        if (mixPos >= m_mixBuffer.size()) {
            m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
            mixPos = 0;
        }
    }

    if (mixPos > 0) {
        m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
    }

    m_fmPosition = 0;
    m_psgPosition = 0;
}

void Audio::saveState(Buffer* buf) {
    buffer_write(buf, &m_timerA, sizeof(m_timerA));
    buffer_write(buf, &m_timerB, sizeof(m_timerB));
    m_psg.saveState(buf);
    YM2612SaveContext(buf);
}

void Audio::loadState(Buffer* buf) {
    buffer_read(buf, &m_timerA, sizeof(m_timerA));
    buffer_read(buf, &m_timerB, sizeof(m_timerB));
    m_psg.loadState(buf);
    YM2612LoadContext(buf);

    m_fmPosition = 0;
    m_psgPosition = 0;
}

} // namespace md
