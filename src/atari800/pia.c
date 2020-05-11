/*
 * pia.c - PIA chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"
#include "pokey.h"
#ifdef XEP80_EMULATION
#include "xep80.h"
#endif
#ifndef BASIC
#include "input.h"
#include "statesav.h"
#endif

UBYTE PIA_PACTL;
UBYTE PIA_PBCTL;
UBYTE PIA_PORTA;
UBYTE PIA_PORTB;
UBYTE PIA_PORT_input[2];

UBYTE PIA_PORTA_mask;
UBYTE PIA_PORTB_mask;
int PIA_CA2 = 1;
int PIA_CA2_negpending = 0;
int PIA_CA2_pospending = 0;
int PIA_CB2 = 1;
int PIA_CB2_negpending = 0;
int PIA_CB2_pospending = 0;
int PIA_IRQ = 0;

int PIA_Initialise(int *argc, char *argv[])
{
	PIA_PACTL = 0x3f;
	PIA_PBCTL = 0x3f;
	PIA_PORTA = 0xff;
	PIA_PORTB = 0xff;
	PIA_PORTA_mask = 0xff;
	PIA_PORTB_mask = 0xff;
	PIA_PORT_input[0] = 0xff;
	PIA_PORT_input[1] = 0xff;
	PIA_CA2 = 1; /* cassette motor line */
	PIA_CB2 = 1; /* sio not command line */

	return TRUE;
}

void PIA_Reset(void)
{
	PIA_PORTA = 0xff;
	if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
		MEMORY_HandlePORTB(0xff, (UBYTE) (PIA_PORTB | PIA_PORTB_mask));
	}
	PIA_PORTB = 0xff;
}

static void set_CA2(int value)
{
	/* This code is part of the cassette emulation */
	if (PIA_CA2 != value) {
		/* The motor status has changed */
		CASSETTE_TapeMotor(!value);
	}
	PIA_CA2 = value;
}

static void set_CB2(int value)
{
	/* This code is part of the serial I/O emulation */
	if (PIA_CB2 != value) {
		/* The command line status has changed */
		SIO_SwitchCommandFrame(!value);
	}
	PIA_CB2 = value;
}

static void update_PIA_IRQ(void)
{
	PIA_IRQ = 0;
	if (((PIA_PACTL & 0x40) && (PIA_PACTL & 0x28) == 0x08) || 
		 ((PIA_PBCTL & 0x40) && (PIA_PBCTL & 0x28) == 0x08) ||
		 ((PIA_PACTL & 0x80) && (PIA_PACTL & 0x01)) ||
		 ((PIA_PBCTL & 0x80) && (PIA_PBCTL & 0x01))) {
		PIA_IRQ = 1;
	}
	/* update pokey IRQ status */
	POKEY_PutByte(POKEY_OFFSET_IRQEN, POKEY_IRQEN);
}

UBYTE PIA_GetByte(UWORD addr, int no_side_effects)
{
	switch (addr & 0x03) {
	case PIA_OFFSET_PACTL: 
		/* read CRA (control register A) */
		return PIA_PACTL;
	case PIA_OFFSET_PBCTL: 
		/* read CRB (control register B) */
		return PIA_PBCTL;
	case PIA_OFFSET_PORTA:
		if ((PIA_PACTL & 0x04) == 0) {
			/* read DDRA (data direction register A) */
			return ~PIA_PORTA_mask;
		}
		else {
			/* read PIBA (peripheral interface buffer A) */
			/* also called ORA (output register A) even for reading in data sheet */
			if (!no_side_effects) {
				if (((PIA_PACTL & 0x38)>>3) == 0x04) { /* handshake */
					if (PIA_CA2 == 1) {
						PIA_CA2_negpending = 1;
					}
					set_CA2(0);
				}
				else if (((PIA_PACTL & 0x38)>>3) == 0x05) { /* pulse */
					set_CA2(0);
					set_CA2(1); /* FIXME one cycle later ... */
				}
				PIA_PACTL &= 0x3f; /* clear bit 6 & 7 */
				update_PIA_IRQ();
			}
#ifdef XEP80_EMULATION
			if (XEP80_enabled) {
				return(XEP80_GetBit() & PIA_PORT_input[0] & (PIA_PORTA | PIA_PORTA_mask));
			}
#endif /* XEP80_EMULATION */
			return PIA_PORT_input[0] & (PIA_PORTA | PIA_PORTA_mask);
		}
	case PIA_OFFSET_PORTB:
		if ((PIA_PBCTL & 0x04) == 0) {
			/* read DDRA (data direction register B) */
			return ~PIA_PORTB_mask;
		}
		else {
			/* read PIBB (peripheral interface buffer B) */
			/* also called ORB (output register B) even for reading in data sheet */
			if (!no_side_effects) {
				PIA_PBCTL &= 0x3f; /* clear bit 6 & 7 */
				update_PIA_IRQ();
			}
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				return PIA_PORTB | PIA_PORTB_mask;
			}
			else {
				return PIA_PORT_input[1] & (PIA_PORTB | PIA_PORTB_mask);
			}
		}
	}
	/* for stupid compilers */
	return 0xff;
}

void PIA_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0x03) {
	case PIA_OFFSET_PACTL: 
		/* write CRA (control register A) */
		byte &= 0x3f; /* bits 6 & 7 cannot be written */
	
		PIA_PACTL = ((PIA_PACTL & 0xc0) | byte);
		/* CA2 control */
		switch((byte & 0x38)>>3) {
			case 0: /* 000 input, negative transition, no irq*/
			case 1: /* 001 input, negative transition, irq */
				if (PIA_CA2_negpending) {
					PIA_PACTL |= 0x40;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 2: /* 010 input, positive transition, no irq */
			case 3: /* 011 input, positive transition, irq */
				if (!PIA_CA2 || PIA_CA2_pospending) {
					PIA_PACTL |= 0x40;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 4: /* 100 output, handshake mode */
				PIA_CA2_pospending = 0;
				PIA_PACTL &= 0x3f;
			break;
			case 5: /* 101 output, pulse mode */
				set_CA2(1); /* FIXME after one cycle, if 0 */
				PIA_PACTL &= 0x3f;
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 6: /* 110 output low */
				set_CA2(0);
				PIA_PACTL &= 0x3f;
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 7: /* 111 output high */
				PIA_PACTL &= 0x3f;
				if (!PIA_CA2_negpending && PIA_CA2 == 0) {
					PIA_CA2_pospending = 1;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
			break;
		}
		update_PIA_IRQ();
		break;
	case PIA_OFFSET_PBCTL: 
		/* write CRB (control register B) */
		byte &= 0x3f; /* bits 6 & 7 cannot be written */
		PIA_PBCTL = ((PIA_PBCTL & 0xc0) | byte);
		/* CB2 control */
		switch((byte & 0x38)>>3) {
			case 0: /* 000 input, negative transition, no irq*/
			case 1: /* 001 input, negative transition, irq */
				/* PACTL has: if (PIA_CB2_negpending) */
				if (PIA_CB2_negpending || PIA_CB2_pospending) {
					PIA_PBCTL |= 0x40;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 2: /* 010 input, positive transition, no irq */
			case 3: /* 011 input, positive transition, irq */
				if (!PIA_CB2 || PIA_CB2_pospending) {
					PIA_PBCTL |= 0x40;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 4: /* 100 output, handshake mode */
				PIA_CB2_pospending = 0;
				PIA_PBCTL &= 0x3f;
			break;
			case 5: /* 101 output, pulse mode */
				set_CB2(1); /* FIXME after one cycle, if 0 */
				PIA_PBCTL &= 0x3f;
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 6: /* 110 output low */
				set_CB2(0);
				PIA_PBCTL &= 0x3f;
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 7: /* 111 output high */
				PIA_PBCTL &= 0x3f;
				/* PACTL has: if (!PIA_CB2_negpending && PIA_CB2 == 0) */
				if (PIA_CB2 == 0) {
					PIA_CB2_pospending = 1;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
			break;
		}
		update_PIA_IRQ();
		break;
	case PIA_OFFSET_PORTA:
		if ((PIA_PACTL & 0x04) == 0) {
			/* write DDRA (data direction register A) */
 			PIA_PORTA_mask = ~byte;
		}
		else {
			/* write ORA (output register A) */
#ifdef XEP80_EMULATION
			if (XEP80_enabled && (~PIA_PORTA_mask & 0x11)) {
				XEP80_PutBit(byte);
			}
#endif /* XEP80_EMULATION */
			PIA_PORTA = byte;		/* change from thor */
		}
#ifndef BASIC
		INPUT_SelectMultiJoy((PIA_PORTA | PIA_PORTA_mask) >> 4);
#endif
		break;
	case PIA_OFFSET_PORTB:
		if ((PIA_PBCTL & 0x04) == 0) {
			/* write DDRB (data direction register B) */
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				MEMORY_HandlePORTB((UBYTE) (PIA_PORTB | ~byte), (UBYTE) (PIA_PORTB | PIA_PORTB_mask));
			}
			PIA_PORTB_mask = ~byte;
		}
		else {
			/* write ORB (output register B) */
			if (((PIA_PBCTL & 0x38)>>3) == 0x04) { /* handshake */
				if (PIA_CB2 == 1) {
					PIA_CB2_negpending = 1;
				}
				set_CB2(0);
			}
			else if (((PIA_PBCTL & 0x38)>>3) == 0x05) { /* pulse */
				set_CB2(0);
				set_CB2(1); /* FIXME one cycle later ... */
			}
			if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
				MEMORY_HandlePORTB((UBYTE) (byte | PIA_PORTB_mask), (UBYTE) (PIA_PORTB | PIA_PORTB_mask));
			}
			PIA_PORTB = byte;
		}
		break;
	}
}

#ifndef BASIC

void PIA_StateSave(void)
{
	STATESAV_TAG(pia);
	StateSav_SaveUBYTE( &PIA_PACTL, 1 );
	StateSav_SaveUBYTE( &PIA_PBCTL, 1 );
	StateSav_SaveUBYTE( &PIA_PORTA, 1 );
	StateSav_SaveUBYTE( &PIA_PORTB, 1 );

	StateSav_SaveUBYTE( &PIA_PORTA_mask, 1 );
	StateSav_SaveUBYTE( &PIA_PORTB_mask, 1 );
	StateSav_SaveINT( &PIA_CA2, 1 );
	StateSav_SaveINT( &PIA_CA2_negpending, 1 );
	StateSav_SaveINT( &PIA_CA2_pospending, 1 );
	StateSav_SaveINT( &PIA_CB2, 1 );
	StateSav_SaveINT( &PIA_CB2_negpending, 1 );
	StateSav_SaveINT( &PIA_CB2_pospending, 1 );
}

void PIA_StateRead(UBYTE version)
{
	UBYTE byte;
	int temp;

	StateSav_ReadUBYTE( &byte, 1 );
	if (version <= 7 ) {
		PIA_PutByte(PIA_OFFSET_PACTL, byte); /* set PIA_CA2 and tape motor */
	}
	PIA_PACTL = byte;
	StateSav_ReadUBYTE( &byte, 1 );
	if (version <= 7 ) {
		PIA_PutByte(PIA_OFFSET_PBCTL, byte); /* set PIA_CB2 and !command line */
	}
	PIA_PBCTL = byte;
	StateSav_ReadUBYTE( &PIA_PORTA, 1 );
	StateSav_ReadUBYTE( &PIA_PORTB, 1 );

	/* In version 7 and later these variables are read in memory.c. */
	if (version <= 6) {
		int Ram256;
		StateSav_ReadINT( &MEMORY_xe_bank, 1 );
		StateSav_ReadINT( &MEMORY_selftest_enabled, 1 );
		StateSav_ReadINT( &Ram256, 1 );

		if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
			if (Ram256 == 1 && MEMORY_ram_size == MEMORY_RAM_320_COMPY_SHOP)
				MEMORY_ram_size = MEMORY_RAM_320_RAMBO;
		}
		StateSav_ReadINT( &MEMORY_cartA0BF_enabled, 1 );
	}

	StateSav_ReadUBYTE( &PIA_PORTA_mask, 1 );
	StateSav_ReadUBYTE( &PIA_PORTB_mask, 1 );

	if (version >= 8) {
		StateSav_ReadINT( &temp, 1 );
		set_CA2(temp);
		StateSav_ReadINT( &PIA_CA2_negpending, 1 );
		StateSav_ReadINT( &PIA_CA2_pospending, 1 );
		StateSav_ReadINT( &temp, 1 );
		set_CB2(temp);
		StateSav_ReadINT( &PIA_CB2_negpending, 1 );
		StateSav_ReadINT( &PIA_CB2_pospending, 1 );
		update_PIA_IRQ();
	}
}

#endif /* BASIC */
