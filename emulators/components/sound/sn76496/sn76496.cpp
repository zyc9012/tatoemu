#include "sn76496.h"
#include <cmath>
#include <cstring>

namespace {

// Each attenuation step is 2 dB; step 15 mutes the channel.  The peak is scaled
// so that all four channels together stay inside 16-bit range.
constexpr double PSG_MAX_OUTPUT = 7000.0;

// The PSG divides its input clock by 16 before driving the tone counters.
constexpr u32 INTERNAL_DIVIDER = 16;

} // namespace

SN76496::SN76496() {
    for (int i = 0; i < 15; i++) {
        m_volumeTable[i] = static_cast<s16>(PSG_MAX_OUTPUT * std::pow(10.0, -0.1 * 2.0 * i));
    }
    m_volumeTable[15] = 0;
    reset();
}

void SN76496::init(u32 clock, u32 sampleRate) {
    m_clock = clock;
    m_sampleRate = sampleRate ? sampleRate : 44100;

    const double internalRate = static_cast<double>(m_clock) / INTERNAL_DIVIDER;
    m_step = static_cast<u32>((internalRate / m_sampleRate) * 65536.0);

    reset();
}

void SN76496::reset() {
    for (int i = 0; i < 4; i++) {
        m_period[i] = 1;
        m_counter[i] = 1;
        m_volume[i] = 0x0F;  // silent
        m_output[i] = 0;
    }
    m_noiseShift = 0x8000;
    m_noiseMode = 0;
    m_latchedChannel = 0;
    m_latchedVolume = false;
    m_accumulator = 0;
}

void SN76496::write(u8 value) {
    if (value & 0x80) {
        // Latch/data byte: selects the channel and register.
        m_latchedChannel = (value >> 5) & 0x03;
        m_latchedVolume = (value & 0x10) != 0;

        if (m_latchedVolume) {
            m_volume[m_latchedChannel] = value & 0x0F;
            return;
        }

        if (m_latchedChannel == 3) {
            m_noiseMode = value & 0x07;
            m_noiseShift = 0x8000;
            return;
        }

        m_period[m_latchedChannel] = (m_period[m_latchedChannel] & 0x03F0) | (value & 0x0F);
        if (m_period[m_latchedChannel] == 0) m_period[m_latchedChannel] = 1;
        return;
    }

    // Data byte: completes the previous latch.
    if (m_latchedVolume) {
        m_volume[m_latchedChannel] = value & 0x0F;
        return;
    }

    if (m_latchedChannel == 3) {
        m_noiseMode = value & 0x07;
        m_noiseShift = 0x8000;
        return;
    }

    m_period[m_latchedChannel] =
        (m_period[m_latchedChannel] & 0x000F) | ((value & 0x3F) << 4);
    if (m_period[m_latchedChannel] == 0) m_period[m_latchedChannel] = 1;
}

void SN76496::tick() {
    for (int i = 0; i < 3; i++) {
        if (--m_counter[i] <= 0) {
            m_counter[i] = m_period[i] > 0 ? m_period[i] : 1;
            // Periods below 2 produce a DC level rather than an audible tone.
            m_output[i] = (m_period[i] > 1) ? (m_output[i] ^ 1) : 1;
        }
    }

    if (--m_counter[3] <= 0) {
        const u32 rate = m_noiseMode & 0x03;
        m_counter[3] = (rate == 3) ? (m_period[2] > 0 ? m_period[2] : 1)
                                   : static_cast<s32>(0x10u << rate);

        // Bit 2 selects white noise (taps 0 and 3) over periodic noise.
        const u16 feedback = (m_noiseMode & 0x04)
            ? static_cast<u16>((m_noiseShift ^ (m_noiseShift >> 3)) & 1)
            : static_cast<u16>(m_noiseShift & 1);

        m_noiseShift = static_cast<u16>((m_noiseShift >> 1) | (feedback << 15));
        m_output[3] = static_cast<u8>(m_noiseShift & 1);
    }
}

s32 SN76496::mix() const {
    s32 out = 0;
    for (int i = 0; i < 4; i++) {
        const s16 level = m_volumeTable[m_volume[i]];
        out += m_output[i] ? level : -level;
    }
    return out;
}

void SN76496::update(s16* buffer, u32 samples) {
    if (!buffer) return;

    for (u32 i = 0; i < samples; i++) {
        m_accumulator += m_step;
        u32 clocks = m_accumulator >> 16;
        m_accumulator &= 0xFFFF;

        while (clocks--) tick();

        s32 sample = mix();
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buffer[i] = static_cast<s16>(sample);
    }
}

void SN76496::saveState(Buffer* buf) {
    buffer_write(buf, m_period, sizeof(m_period));
    buffer_write(buf, m_counter, sizeof(m_counter));
    buffer_write(buf, m_volume, sizeof(m_volume));
    buffer_write(buf, m_output, sizeof(m_output));
    buffer_write(buf, &m_noiseShift, sizeof(m_noiseShift));
    buffer_write(buf, &m_noiseMode, sizeof(m_noiseMode));
    buffer_write(buf, &m_latchedChannel, sizeof(m_latchedChannel));
    buffer_write(buf, &m_latchedVolume, sizeof(m_latchedVolume));
    buffer_write(buf, &m_accumulator, sizeof(m_accumulator));
}

void SN76496::loadState(Buffer* buf) {
    buffer_read(buf, m_period, sizeof(m_period));
    buffer_read(buf, m_counter, sizeof(m_counter));
    buffer_read(buf, m_volume, sizeof(m_volume));
    buffer_read(buf, m_output, sizeof(m_output));
    buffer_read(buf, &m_noiseShift, sizeof(m_noiseShift));
    buffer_read(buf, &m_noiseMode, sizeof(m_noiseMode));
    buffer_read(buf, &m_latchedChannel, sizeof(m_latchedChannel));
    buffer_read(buf, &m_latchedVolume, sizeof(m_latchedVolume));
    buffer_read(buf, &m_accumulator, sizeof(m_accumulator));
}
