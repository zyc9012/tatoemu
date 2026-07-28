// OKI MSM6295 module
// Emulation by Jan Klaassen

// Aug 24, 2020: Cubic interpolation disabled.
// reasoning:
// 1: clicks in a lot of games, such as:
//   sf2ce (titlescreen percussion)
//   boogwing (start game, when your plane takes off and bombs are exploding)
//   .. others I forget ..
// 2: too much top-end (treble) loss
// conclusion: if at least #1 can be fixed, it will be re-instated.

#include <array>
#include <cmath>
#include <cstring>

#include "msm6295.h"

// Position within a sample is tracked in 20.12 fixed point.
#define POSITION_ONE (0x1000)

static constexpr s32 stepShift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

// ADPCM delta for each (step, nibble) pair.
static std::array<s32, 49 * 16> makeDeltaTable()
{
    std::array<s32, 49 * 16> tab{};

    for (s32 i = 0; i < 49; i++) {
        s32 step = (s32)(pow(1.1, (double)i) * 16.0);
        for (s32 n = 0; n < 16; n++) {
            s32 delta = step >> 3;
            if (n & 1) {
                delta += step >> 2;
            }
            if (n & 2) {
                delta += step >> 1;
            }
            if (n & 4) {
                delta += step;
            }
            if (n & 8) {
                delta = -delta;
            }
            tab[(i << 4) + n] = delta;
        }
    }

    return tab;
}

// Attenuation for each of the 16 volume levels, roughly 3 dB per step.
static std::array<s32, 16> makeVolumeTable()
{
    std::array<s32, 16> tab{};

    for (s32 i = 0; i < 16; i++) {
        double volume = 256.0;
        for (s32 n = i; n > 0; n--) {
            volume /= 1.412537545;
        }
        tab[i] = (s32)(volume + 0.5);
    }

    return tab;
}

static const std::array<s32, 49 * 16> deltaTable = makeDeltaTable();
static const std::array<s32, 16> volumeTable = makeVolumeTable();


void Msm6295::init(u32 chipRate, u32 hostRate)
{
    for (Channel& channel : m_channel) {
        channel = Channel{};
    }
    std::memset(m_page, 0, sizeof(m_page));

    m_volume = 256;
    setSampleRate(chipRate, hostRate);
    reset();
}

void Msm6295::reset()
{
    m_status = 0;
    m_isCommand = false;
    m_fractionalPosition = 0;
    m_currentSample = 0;

    for (Channel& channel : m_channel) {
        channel.playing = 0;
    }
}

void Msm6295::setSampleRate(u32 chipRate, u32 hostRate)
{
    if (hostRate == 0) {
        hostRate = 11025;   // avoid division by 0
    }

    m_sampleSize = (s32)((chipRate << 12) / hostRate);
}

void Msm6295::setVolume(double volume)
{
    m_volume = (s32)(volume * 256.0 + 0.5);
}

void Msm6295::setBank(const u8* romData, u32 start, u32 end)
{
    if (romData == nullptr) return;

    for (u32 i = 0; i < ((end - start) >> PAGE_SHIFT) + 1; i++) {
        m_page[(start >> PAGE_SHIFT) + i] = romData + (i << PAGE_SHIFT);
    }
}

u8 Msm6295::readData(u32 addr) const
{
    return m_page[(addr >> PAGE_SHIFT) & (PAGE_COUNT - 1)][addr & 0xff];
}

// Decode one ADPCM nibble on every active channel and sum them.
void Msm6295::stepChannels()
{
    m_currentSample = 0;

    for (s32 nChannel = 0; nChannel < 4; nChannel++) {
        if (!(m_status & (1 << nChannel))) {
            continue;
        }

        Channel* channel = &m_channel[nChannel];

        // Check for end of sample
        if (channel->sampleCount-- <= 0) {
            m_status &= ~(1u << nChannel);
            channel->playing = 0;
            continue;
        }

        // Get new delta from ROM
        s32 delta;
        if (channel->position & 1) {
            delta = channel->delta & 0x0F;
        } else {
            channel->delta = readData((channel->position >> 1) & 0x3ffff);
            delta = channel->delta >> 4;
        }

        // Compute new sample
        s32 sample = channel->sample + deltaTable[(channel->step << 4) + delta];
        if (sample > 2047) {
            sample = 2047;
        } else {
            if (sample < -2048) {
                sample = -2048;
            }
        }
        channel->sample = sample;
        channel->output = sample * channel->volume;

        // Update step value
        channel->step = channel->step + stepShift[delta & 7];
        if (channel->step > 48) {
            channel->step = 48;
        } else {
            if (channel->step < 0) {
                channel->step = 0;
            }
        }

        m_currentSample += channel->output / 16;

        // Advance sample position
        channel->position++;
    }
}

void Msm6295::render(s16* out, u32 samples)
{
    while (samples--) {
        while (m_fractionalPosition >= POSITION_ONE) {
            stepChannels();
            m_fractionalPosition -= POSITION_ONE;
        }

        // Scale all 4 channels
        s32 sample = (m_currentSample * m_volume) >> 8;
        if (sample > 32767) {
            sample = 32767;
        } else if (sample < -32768) {
            sample = -32768;
        }
        *out++ = (s16)sample;

        m_fractionalPosition += m_sampleSize;
    }
}

void Msm6295::write(u8 command)
{
    if (m_isCommand) {
        // Process second half of command
        s32 volume = command & 0x0F;
        command >>= 4;

        m_isCommand = false;

        for (s32 nChannel = 0; nChannel < 4; nChannel++) {
            if (!(command & (0x01 << nChannel))) {
                continue;
            }
            if (m_channel[nChannel].playing != 0) {
                continue;
            }

            const u32 header = (u32)(m_sampleInfo & 0x03ff);

            s32 sampleStart  = readData(header + 0) << 17;
            sampleStart     |= readData(header + 1) <<  9;
            sampleStart     |= readData(header + 2) <<  1;

            s32 sampleCount  = readData(header + 3) << 17;
            sampleCount     |= readData(header + 4) <<  9;
            sampleCount     |= readData(header + 5) <<  1;

            m_sampleInfo &= 0xFF;

            sampleCount -= sampleStart;

            if (sampleCount < 0x80000) {
                // Start playing channel
                Channel& channel = m_channel[nChannel];
                channel.volume = volumeTable[volume];
                channel.position = sampleStart;
                channel.sampleCount = sampleCount;
                channel.step = 0;
                channel.sample = -1;
                channel.playing = 1;
                channel.output = 0;

                m_status |= command;
            }
        }
    } else {
        // Process command
        if (command & 0x80) {
            m_sampleInfo = (command & 0x7F) << 3;
            m_isCommand = true;
        } else {
            // Stop playing samples
            command >>= 3;
            m_status &= ~(u32)command;

            for (s32 nChannel = 0; nChannel < 4; nChannel++, command >>= 1) {
                if (command & 1) {
                    m_channel[nChannel].playing = 0;
                }
            }
        }
    }
}

// The bank pointers are rebuilt when the ROM is loaded, and m_volume and
// m_sampleSize come from the current configuration, so neither belongs in a
// save state.
template <typename Visit>
void Msm6295::visitState(Visit visit)
{
    for (Channel& channel : m_channel) {
        visit(channel.output);
        visit(channel.volume);
        visit(channel.position);
        visit(channel.sampleCount);
        visit(channel.sample);
        visit(channel.step);
        visit(channel.delta);
        visit(channel.playing);
    }

    visit(m_isCommand);
    visit(m_sampleInfo);
    visit(m_status);
    visit(m_fractionalPosition);
    visit(m_currentSample);
}

void Msm6295::saveState(Buffer* buf)
{
    visitState(StateWriter{buf});
}

void Msm6295::loadState(Buffer* buf)
{
    visitState(StateReader{buf});
}
