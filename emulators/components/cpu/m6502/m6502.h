/*****************************************************************************
 *
 *   m6502.h
 *   Portable 6502/65c02/65sc02/6510/n2a03 emulator interface
 *
 *   Copyright Juergen Buchmueller, all rights reserved.
 *   65sc02 core Copyright Peter Trauner.
 *   Deco16 portions Copyright Bryan McPhail.
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     pullmoll@t-online.de
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *****************************************************************************/
/* 2.February 2000 PeT added 65sc02 subtype */

#pragma once

#ifndef __M6502_H__
#define __M6502_H__

#include "../../compact.h"

typedef struct
{
	UINT8	subtype;		/* currently selected cpu sub type */
	PAIR	ppc;			/* previous program counter */
	PAIR	pc; 			/* program counter */
	PAIR	sp; 			/* stack pointer (always 100 - 1FF) */
	PAIR	zp; 			/* zero page address */
	PAIR	ea; 			/* effective address */
	UINT8	a;				/* Accumulator */
	UINT8	x;				/* X index register */
	UINT8	y;				/* Y index register */
	UINT8	p;				/* Processor status */
	UINT8	pending_irq;	/* nonzero if an IRQ is pending */
	UINT8	after_cli;		/* pending IRQ and last insn cleared I */
	UINT8	nmi_state;
	UINT8   nmi_req;
	UINT8	irq_state;
	UINT8   so_state;
	UINT8   hold_irq;
	UINT8   hold_nmi;
	UINT8   delay_nmi;

	INT32   IntOccured;
	INT32   ICount;
	INT32   segmentcycles;
	INT32   end_run;

	int 	(*irq_callback)(int irqline);	/* IRQ callback */

	int     fetching_opcode; // true when fetching opcode (no stating necessary)

}	m6502_Regs;

#define M6502_CLEAR_LINE		0
#define M6502_ASSERT_LINE		1
#define M6502_INPUT_LINE_NMI	32

/* set to 1 to test cur_mrhard/cur_wmhard to avoid calls */
#define FAST_MEMORY 0

#define SUBTYPE_6502	0
#define SUBTYPE_2A03	3

enum
{
	M6502_PC=1, M6502_S, M6502_P, M6502_A, M6502_X, M6502_Y,
	M6502_EA, M6502_ZP,
	M6502_SUBTYPE
};

#define M6502_IRQ_LINE		0
/* use cpunum_set_input_line(machine, cpu, M6502_SET_OVERFLOW, level)
   to change level of the so input line
   positiv edge sets overflow flag */
#define M6502_SET_OVERFLOW	1

unsigned char M6502ReadPort(unsigned short Address);
void M6502WritePort(unsigned short Address, unsigned char Data);
unsigned char M6502ReadByte(unsigned short Address);
void M6502WriteByte(unsigned short Address, unsigned char Data);
unsigned char M6502ReadOp(unsigned short Address);
unsigned char M6502ReadOpArg(unsigned short Address);

void m6502_init();
void m6502_reset(void);
void m6502_exit(void);
void m6502_get_context (void *dst);
void m6502_set_context (void *src);
int m6502_execute(int cycles);
void m6502_set_irq_line(int irqline, int state);
void m6502_set_irq_hold();
void m6502_set_nmi_hold();
void m6502_set_nmi_hold2();
void m6502_set_pc(unsigned int pc_);
UINT32 m6502_get_pc();
UINT32 m6502_get_prev_pc();
int m6502_releaseslice();
int m6502_dec_icount(int todec);
int m6502_get_segmentcycles();
int m6502_get_fetch_status();

void n2a03_init();

/****************************************************************************
 * The 2A03 (NES 6502 without decimal mode ADC/SBC)
 ****************************************************************************/
#define N2A03_A 						M6502_A
#define N2A03_X 						M6502_X
#define N2A03_Y 						M6502_Y
#define N2A03_S 						M6502_S
#define N2A03_PC						M6502_PC
#define N2A03_P 						M6502_P
#define N2A03_EA						M6502_EA
#define N2A03_ZP						M6502_ZP
#define N2A03_NMI_STATE 				M6502_NMI_STATE
#define N2A03_IRQ_STATE 				M6502_IRQ_STATE

#define N2A03_IRQ_LINE					M6502_IRQ_LINE

#define N2A03_DEFAULTCLOCK (21477272.724 / 12)

#endif /* __M6502_H__ */
