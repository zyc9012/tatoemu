#include "core.h"
#include "../components/buffer.h"

namespace md {

Core::Core() = default;

bool Core::initialize() {
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_soundCpu = std::make_unique<SoundCPU>();
    m_memory = std::make_unique<Memory>();
    m_vdp = std::make_unique<VDP>();
    m_audio = std::make_unique<Audio>();
    m_controller = std::make_unique<Controller>();

    m_cheatMem.mem = m_memory.get();

    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());

    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setVDP(m_vdp.get());
    m_memory->setAudio(m_audio.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());

    m_vdp->setCPU(m_cpu.get());
    m_vdp->setMemory(m_memory.get());

    m_audio->setCPU(m_cpu.get());

    return true;
}

void Core::setVideoDevice(::VideoDevice* videoDevice) {
    m_vdp->setVideoDevice(videoDevice);
}

void Core::setAudioDevice(::AudioDevice* audioDevice) {
    m_audio->setAudioDevice(audioDevice);
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename)) {
        return false;
    }

    // The cartridge header decides whether we run as a PAL or NTSC machine.
    m_vdp->setPAL(m_cartridge->isPAL());
    Config::Region = m_cartridge->isPAL() ? 2u : 1u;

    reset();
    return true;
}

void Core::reset() {
    m_cartridge->reset();
    m_memory->reset();
    m_vdp->setPAL(m_cartridge->isPAL());
    m_vdp->reset();
    m_vdp->setPAL(m_cartridge->isPAL());
    m_audio->reset();
    m_controller->reset();
    m_soundCpu->reset();
    m_cpu->reset();

    m_z80IrqAsserted = false;
}

void Core::runTo(u32 target68kCycles) {
    while (m_cpu->frameCycles() < target68kCycles) {
        const u32 remaining = target68kCycles - m_cpu->frameCycles();
        const u32 used = m_cpu->step(remaining);
        if (used == 0) break;
        m_audio->updateTimers(used);
    }

    // Keep the Z80 at the same point on the master clock as the 68000.
    const u32 z80Target = static_cast<u32>(
        (static_cast<u64>(m_cpu->frameCycles()) * M68K_CLOCK_DIVIDER) / Z80_CLOCK_DIVIDER);
    if (z80Target > m_soundCpu->frameCycles()) {
        m_soundCpu->step(z80Target - m_soundCpu->frameCycles());
    }
}

void Core::update() {
    const u32 totalLines = m_vdp->totalScanlines();

    m_vdp->beginFrame();

    for (u32 line = 0; line < totalLines; line++) {
        // Exact fractional division of the line's master cycles avoids drift
        // over the frame.
        const u32 lineStart = (line * MASTER_CYCLES_PER_LINE) / M68K_CLOCK_DIVIDER;
        const u32 lineEnd = ((line + 1) * MASTER_CYCLES_PER_LINE) / M68K_CLOCK_DIVIDER;
        const u32 lineCycles = lineEnd - lineStart;

        // Active display covers roughly the first three quarters of the line;
        // the H interrupt fires at the end of it.
        const u32 activeEnd = lineStart + (lineCycles * 3) / 4;

        m_vdp->beginLine(line);
        m_vdp->setLineProgress(0, lineCycles);

        // Clear the previous frame's vertical interrupt pulse to the Z80.
        if (m_z80IrqAsserted) {
            m_soundCpu->irq(false);
            m_z80IrqAsserted = false;
        }

        runTo(activeEnd);

        m_vdp->setLineProgress((lineCycles * 3) / 4, lineCycles);
        m_vdp->endActiveDisplay(line);

        if (m_vdp->consumeVIntEvent()) {
            m_soundCpu->irq(true);
            m_z80IrqAsserted = true;
        }

        runTo(lineEnd);
        m_vdp->endLine();
        m_controller->endLine();
    }

    const u32 frameCycles68k = (totalLines * MASTER_CYCLES_PER_LINE) / M68K_CLOCK_DIVIDER;
    const u32 frameCyclesZ80 = (totalLines * MASTER_CYCLES_PER_LINE) / Z80_CLOCK_DIVIDER;

    m_audio->endFrame(m_gameSpeed);
    m_vdp->endFrame();

    m_cpu->endFrame(frameCycles68k);
    m_soundCpu->endFrame(frameCyclesZ80);
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};

    m_cpu->saveState(&buf);
    m_soundCpu->saveState(&buf);
    m_memory->saveState(&buf);
    m_vdp->saveState(&buf);
    m_audio->saveState(&buf);
    m_cartridge->saveState(&buf);
    m_controller->saveState(&buf);

    return buffer_save_to_file(&buf, filename);
}

bool Core::loadState(const fs::path& filename) {
    Buffer buf = {};
    if (!buffer_load_from_file(&buf, filename)) {
        return false;
    }

    m_cpu->loadState(&buf);
    m_soundCpu->loadState(&buf);
    m_memory->loadState(&buf);
    m_vdp->loadState(&buf);
    m_audio->loadState(&buf);
    m_cartridge->loadState(&buf);
    m_controller->loadState(&buf);

    return true;
}

} // namespace md
