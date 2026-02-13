/*
 * ARM7TDMI CPU Interface
 * Declares the external functions needed by the ARM7 core
 */
#ifndef ARM7_INTF_H
#define ARM7_INTF_H

#include "../../compact.h"

#ifdef __cplusplus
extern "C" {
#endif

// Memory access callbacks - must be implemented by the system using the ARM7 core
// These are called by the internal arm7_cpu_read/write functions in arm7core.c
UINT8  Arm7ReadByte(UINT32 addr);
UINT16 Arm7ReadWord(UINT32 addr);
UINT32 Arm7ReadLong(UINT32 addr);
void   Arm7WriteByte(UINT32 addr, UINT8 data);
void   Arm7WriteWord(UINT32 addr, UINT16 data);
void   Arm7WriteLong(UINT32 addr, UINT32 data);

// Instruction fetch callbacks (separate from data reads for potential HLE interception)
UINT16 Arm7FetchWord(UINT32 addr);
UINT32 Arm7FetchLong(UINT32 addr);

#ifdef __cplusplus
}
#endif

// Public ARM7 CPU API
void   Arm7Open(int);
void   Arm7Close();
int    Arm7TotalCycles();
void   Arm7RunEndEatCycles();
void   Arm7RunEnd();
void   Arm7BurnCycles(int cycles);
INT32  Arm7Idle(int cycles);
void   Arm7NewFrame();
void   Arm7Reset();
int    Arm7Run(int cycles);
void   arm7_set_irq_line(int irqline, int state);
int    Arm7Scan(int nAction);

// Register access (for HLE BIOS and save states)
UINT32 Arm7GetRegister(int reg);
void   Arm7SetRegister(int reg, UINT32 value);
UINT32 Arm7GetCPSR();
void   Arm7SetCPSR(UINT32 value);
UINT32 Arm7GetSPSR();
void*  Arm7GetState();
int    Arm7GetStateSize();

#endif // ARM7_INTF_H
