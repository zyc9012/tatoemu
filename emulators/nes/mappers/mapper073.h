#pragma once

#include "../cartridge.h"

namespace nes {

// Mapper 73: VRC3 (Konami) - Salamander
class Mapper073 : public Mapper {
public:
    Mapper073(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void scanlineCounter() override;
    void clockAudio() override;  // Used to clock the IRQ timer every CPU cycle
    
    void saveState(Buffer* buf) override;
    void loadState(Buffer* buf) override;
    
private:
    template <typename Visit> void visitState(Visit visit);

    u8 m_prgBank;           // 16KB PRG bank
    
    // IRQ
    u16 m_irqLatch;
    u16 m_irqCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;         // 0 = 16-bit, 1 = 8-bit (low byte only)
};

} // namespace nes

