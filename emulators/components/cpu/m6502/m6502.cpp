/*****************************************************************************
 *
 *   m6502.c
 *   Portable 6502/65c02/65sc02/6510/n2a03 emulator V1.2
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
/* 10.March   2000 PeT added 6502 set overflow input line */
/* 13.September 2000 PeT N2A03 jmp indirect */

#include "m6502.h"

#define M6502_INLINE

#define change_pc(newpc)	m6502.pc.d = (newpc)

void	(*const *insnActive)(void);

extern INT32 M6502GetActive();

static m6502_Regs m6502;
#define m6502_ICount m6502.ICount

#include "ops02.h"
#include "opsn2a03.h"
#include "ill02.h"


#define M6502_NMI_VEC	0xfffa
#define M6502_RST_VEC	0xfffc
#define M6502_IRQ_VEC	0xfffe

#define DECO16_RST_VEC	0xfff0
#define DECO16_IRQ_VEC	0xfff2
#define DECO16_NMI_VEC	0xfff4

#define VERBOSE 0

#define LOG(x)

/***************************************************************
 * include the opcode macros, functions and tables
 ***************************************************************/
#include "t6502.c"

#include "tn2a03.c"

/*****************************************************************************
 *
 *      6502 CPU interface functions
 *
 *****************************************************************************/

static void m6502_common_init(UINT8 subtype, void (*const *insn)(void)/*, const char *type*/)
{
	memset(&m6502, 0, sizeof(m6502));
	m6502.subtype = subtype;
	insnActive = insn;
}

void m6502_init()
{
	m6502_common_init(SUBTYPE_6502, insn6502/*, "m6502"*/);
}

void m6502_reset(void)
{
	/* wipe out the rest of the m6502 structure */
	/* read the reset vector into PC */
	PCL = RDMEM(M6502_RST_VEC);
	PCH = RDMEM(M6502_RST_VEC+1);

	m6502.sp.d = 0x01ff;	/* stack pointer starts at page 1 offset FF */
	m6502.p = F_T|F_I|F_Z|F_B|(P&F_D);	/* set T, I and Z flags */
	m6502.a = 0;
	m6502.x = 0;
	m6502.y = 0;
	m6502.pending_irq = 0;	/* nonzero if an IRQ is pending */
	m6502.after_cli = 0;	/* pending IRQ and last insn cleared I */
	m6502.irq_state = 0;
	m6502.nmi_state = 0;
	m6502.nmi_req = 0;

	change_pc(PCD);
}

void m6502_exit(void)
{
	/* nothing to do yet */
}

void m6502_get_context (void *dst)
{
	if( dst ) 
		*(m6502_Regs*)dst = m6502;
}

void m6502_set_context (void *src)
{
	if( src )
	{
		m6502 = *(m6502_Regs*)src;
		if (m6502.subtype == SUBTYPE_6502)  insnActive = insn6502;
		if (m6502.subtype == SUBTYPE_2A03)  insnActive = insn2a03;
		change_pc(PCD);
	}
}

int m6502_get_fetch_status()
{
	return m6502.fetching_opcode;
}

void m6502_set_irq_hold()
{
	m6502.hold_irq = 1;
}

void m6502_set_nmi_hold()
{
	m6502.hold_nmi = 1;
}

// for nes - ppuctrl transition to 0x80 during vblank takes nmi after
// next opcode has executed
void m6502_set_nmi_hold2()
{
	m6502.hold_nmi = 1;
	m6502.delay_nmi = 2;
}

M6502_INLINE void m6502_take_irq(void)
{
	if (m6502.nmi_req)
	{
		if (m6502.hold_nmi) {
			m6502.hold_nmi = 0;
			m6502.nmi_state = M6502_CLEAR_LINE;
		}
		m6502.nmi_req = 0;
		EAD = M6502_NMI_VEC;
		m6502.ICount -= 2;
		PUSH(PCH);
		PUSH(PCL);
		PUSH(P & ~F_B);
		P |= F_I;		/* set I flag */
		PCL = RDMEM(EAD);
		PCH = RDMEM(EAD+1);
		change_pc(PCD);
	} else {
		if( !(P & F_I) )
		{
			EAD = M6502_IRQ_VEC;
			m6502.ICount -= 2;
			PUSH(PCH);
			PUSH(PCL);
			PUSH(P & ~F_B);
			P |= F_I;		/* set I flag */
			PCL = RDMEM(EAD);
			PCH = RDMEM(EAD+1);
			/* call back the cpuintrf to let it clear the line */

			if (m6502.hold_irq) {
				m6502.hold_irq = 0;
				m6502.irq_state = M6502_CLEAR_LINE;
			}

			if (m6502.irq_callback) (*m6502.irq_callback)(0);
			change_pc(PCD);
		}
		m6502.pending_irq = 0;
	}
}

int m6502_releaseslice()
{
	m6502.ICount = 0;

	return 0;
}

int m6502_dec_icount(int todec)
{
	m6502.ICount -= todec;

	return 0;
}

int m6502_get_segmentcycles()
{
	return m6502.segmentcycles - m6502.ICount;
}

int m6502_execute(int cycles)
{
	m6502.segmentcycles = cycles;
	m6502.ICount = cycles;

	m6502.end_run = 0;

	change_pc(PCD);

	do
	{
		UINT8 op;
		PPC = PCD;

		/* if an irq is pending, take it now */

		if (m6502.delay_nmi > 0) {
			m6502.delay_nmi--;
			if (m6502.delay_nmi == 0) {
				m6502.nmi_req = 1;
				m6502_take_irq();
			}
		}

		if( m6502.pending_irq == 1)
			m6502_take_irq();

		op = RDOP();

		(*insnActive[op])();

		if( m6502.nmi_req )
			m6502_take_irq();

		/* check if the I flag was just reset (interrupts enabled) */
		if( m6502.after_cli )
		{
			m6502.after_cli = 0;
			if (m6502.irq_state != M6502_CLEAR_LINE)
			{
				m6502.pending_irq = 1;
			}
		}
		else {
			if ( m6502.pending_irq == 2 ) {
				if ( m6502.IntOccured - m6502.ICount > 1 ) {
					m6502.pending_irq = 1;
				}
			}
			if( m6502.pending_irq == 1 )
				m6502_take_irq();
			if ( m6502.pending_irq == 2 ) {
				m6502.pending_irq = 1;
			}
		}
	} while (m6502.ICount > 0 && !m6502.end_run);

	cycles = cycles - m6502.ICount;

	m6502.segmentcycles = m6502.ICount = 0;

	return cycles;
}

void m6502_set_irq_line(int irqline, int state)
{
	if (irqline == M6502_INPUT_LINE_NMI)
	{
		if (m6502.nmi_state == state) return;
		m6502.nmi_state = state;
		m6502.nmi_req = (state != M6502_CLEAR_LINE);
	}
	else
	{
		if( irqline == M6502_SET_OVERFLOW )
		{
			if( m6502.so_state && !state )
			{
				P|=F_V;
			}
			m6502.so_state=state;
			return;
		}
		m6502.irq_state = state;
		if( state != M6502_CLEAR_LINE )
		{
			m6502.pending_irq = 1;
//          m6502.pending_irq = 2;
			m6502.IntOccured = m6502.ICount;
		}
	}
}

void m6502_set_pc(unsigned int pc_)
{
	m6502.pc.d = pc_;
}

UINT32 m6502_get_pc()
{
	return m6502.pc.d;
}

UINT32 m6502_get_prev_pc()
{
	return m6502.ppc.d;
}

/****************************************************************************
 * 2A03 section
 ****************************************************************************/
void n2a03_init()
{
	m6502_common_init(SUBTYPE_2A03, insn2a03);
}

void M6502RunEnd()
{
	m6502.end_run = 1;
}
