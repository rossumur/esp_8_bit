#ifndef Z80_H
#define Z80_H

//#define CLEAR_LINE      0       /* clear (a fired, held or pulsed) line */
//#define ASSERT_LINE     1       /* assert an interrupt immediately */


#include "cpuintrf.h"
#include "osd_cpu.h"

enum {
	Z80_PC=1, Z80_SP, Z80_AF, Z80_BC, Z80_DE, Z80_HL,
	Z80_IX, Z80_IY,	Z80_AF2, Z80_BC2, Z80_DE2, Z80_HL2,
	Z80_R, Z80_I, Z80_IM, Z80_IFF1, Z80_IFF2, Z80_HALT,
	Z80_NMI_STATE, Z80_IRQ_STATE, Z80_DC0, Z80_DC1, Z80_DC2, Z80_DC3,
	Z80_NMI_NESTING
};

extern int z80_ICount;              /* T-state count                        */

#define Z80_IGNORE_INT  -1          /* Ignore interrupt                     */
#define Z80_NMI_INT 	-2			/* Execute NMI							*/
#define Z80_IRQ_INT 	-1000		/* Execute IRQ							*/

/* Port number written to when entering/leaving HALT state */
#define Z80_HALT_PORT   0x10000

extern void z80_reset (void *param);
extern void z80_exit (void);
extern int z80_execute(int cycles);
extern void z80_burn(int cycles);
extern unsigned z80_get_context (void *dst);
extern void z80_set_context (void *src);
extern unsigned z80_get_pc (void);
extern void z80_set_pc (unsigned val);
extern unsigned z80_get_sp (void);
extern void z80_set_sp (unsigned val);
extern unsigned z80_get_reg (int regnum);
extern void z80_set_reg (int regnum, unsigned val);
extern void z80_set_nmi_line(int state);
extern void z80_set_irq_line(int irqline, int state);
extern void z80_set_irq_callback(int (*irq_callback)(int));
extern void z80_state_save(void *file);
extern void z80_state_load(void *file);
extern const char *z80_info(void *context, int regnum);
extern unsigned z80_dasm(char *buffer, unsigned pc);

#ifdef MAME_DEBUG
extern unsigned DasmZ80(char *buffer, unsigned pc);
#endif

/****************************************************************************/
/* The Z80 registers. HALT is set to 1 when the CPU is halted, the refresh  */
/* register is calculated as follows: refresh=(Regs.R&127)|(Regs.R2&128)    */
/****************************************************************************/
typedef struct {
/* 00 */    PAIR    PREPC,PC,SP,AF,BC,DE,HL,IX,IY;
/* 24 */    PAIR    AF2,BC2,DE2,HL2;
/* 34 */    UINT8   R,R2,IFF1,IFF2,HALT,IM,I;
/* 3B */    UINT8   irq_max;            /* number of daisy chain devices        */
/* 3C */	INT8	request_irq;		/* daisy chain next request device		*/
/* 3D */	INT8	service_irq;		/* daisy chain next reti handling device */
/* 3E */	UINT8	nmi_state;			/* nmi line state */
/* 3F */	UINT8	irq_state;			/* irq line state */
/* 40 */    UINT8   int_state[Z80_MAXDAISY];
/* 44 */    Z80_DaisyChain irq[Z80_MAXDAISY];
/* 84 */    int     (*irq_callback)(int irqline);
/* 88 */    int     extra_cycles;       /* extra cycles for interrupts */
}   Z80_Regs;

extern Z80_Regs *Z80_Context;
extern int after_EI;
extern unsigned char *cpu_readmap[8];
extern unsigned char *cpu_writemap[8];

#endif

