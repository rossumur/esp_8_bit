/*
 * gtia.c - GTIA chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2015 Atari800 development team (see DOC/CREDITS)
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
#include <string.h>

#include "antic.h"
#include "binload.h"
#include "cassette.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#ifndef BASIC
#include "statesav.h"
#endif
#include "pokeysnd.h"
#include "screen.h"

/* GTIA Registers ---------------------------------------------------------- */

UBYTE GTIA_M0PL;
UBYTE GTIA_M1PL;
UBYTE GTIA_M2PL;
UBYTE GTIA_M3PL;
UBYTE GTIA_P0PL;
UBYTE GTIA_P1PL;
UBYTE GTIA_P2PL;
UBYTE GTIA_P3PL;
UBYTE GTIA_HPOSP0;
UBYTE GTIA_HPOSP1;
UBYTE GTIA_HPOSP2;
UBYTE GTIA_HPOSP3;
UBYTE GTIA_HPOSM0;
UBYTE GTIA_HPOSM1;
UBYTE GTIA_HPOSM2;
UBYTE GTIA_HPOSM3;
UBYTE GTIA_SIZEP0;
UBYTE GTIA_SIZEP1;
UBYTE GTIA_SIZEP2;
UBYTE GTIA_SIZEP3;
UBYTE GTIA_SIZEM;
UBYTE GTIA_GRAFP0;
UBYTE GTIA_GRAFP1;
UBYTE GTIA_GRAFP2;
UBYTE GTIA_GRAFP3;
UBYTE GTIA_GRAFM;
UBYTE GTIA_COLPM0;
UBYTE GTIA_COLPM1;
UBYTE GTIA_COLPM2;
UBYTE GTIA_COLPM3;
UBYTE GTIA_COLPF0;
UBYTE GTIA_COLPF1;
UBYTE GTIA_COLPF2;
UBYTE GTIA_COLPF3;
UBYTE GTIA_COLBK;
UBYTE GTIA_PRIOR;
UBYTE GTIA_VDELAY;
UBYTE GTIA_GRACTL;

/* Internal GTIA state ----------------------------------------------------- */

int GTIA_speaker;
int GTIA_consol_override = 0;
static UBYTE consol;
UBYTE consol_mask;
UBYTE GTIA_TRIG[4];
UBYTE GTIA_TRIG_latch[4];

#if defined(BASIC) || defined(CURSES_BASIC)

static UBYTE PF0PM = 0;
static UBYTE PF1PM = 0;
static UBYTE PF2PM = 0;
static UBYTE PF3PM = 0;
#define GTIA_collisions_mask_missile_playfield 0
#define GTIA_collisions_mask_player_playfield 0
#define GTIA_collisions_mask_missile_player 0
#define GTIA_collisions_mask_player_player 0

#else /* defined(BASIC) || defined(CURSES_BASIC) */

void set_prior(UBYTE byte);			/* in antic.c */

/* Player/Missile stuff ---------------------------------------------------- */

/* change to 0x00 to disable collisions */
UBYTE GTIA_collisions_mask_missile_playfield = 0x0f;
UBYTE GTIA_collisions_mask_player_playfield = 0x0f;
UBYTE GTIA_collisions_mask_missile_player = 0x0f;
UBYTE GTIA_collisions_mask_player_player = 0x0f;

#ifdef NEW_CYCLE_EXACT
/* temporary collision registers for the current scanline only */
UBYTE P1PL_T;
UBYTE P2PL_T;
UBYTE P3PL_T;
UBYTE M0PL_T;
UBYTE M1PL_T;
UBYTE M2PL_T;
UBYTE M3PL_T;
/* If partial collisions have been generated during a scanline, this
 * is the position of the up-to-date collision point , otherwise it is 0
 */
int collision_curpos;
/* if hitclr has been written to during a scanline, this is the position
 * within pm_scaline at which it was written to, and collisions should
 * only be generated from this point on, otherwise it is 0
 */
int hitclr_pos;
#else
#define P1PL_T GTIA_P1PL
#define P2PL_T GTIA_P2PL
#define P3PL_T GTIA_P3PL
#define M0PL_T GTIA_M0PL
#define M1PL_T GTIA_M1PL
#define M2PL_T GTIA_M2PL
#define M3PL_T GTIA_M3PL
#endif /* NEW_CYCLE_EXACT */

static UBYTE *hposp_ptr[4];
static UBYTE *hposm_ptr[4];
static ULONG hposp_mask[4];

//static ULONG grafp_lookup[4][256];
const ULONG grafp_lookup[4][256] = {
{
0x00000000,0x00000080,0x00000040,0x000000C0,0x00000020,0x000000A0,0x00000060,0x000000E0,
0x00000010,0x00000090,0x00000050,0x000000D0,0x00000030,0x000000B0,0x00000070,0x000000F0,
0x00000008,0x00000088,0x00000048,0x000000C8,0x00000028,0x000000A8,0x00000068,0x000000E8,
0x00000018,0x00000098,0x00000058,0x000000D8,0x00000038,0x000000B8,0x00000078,0x000000F8,
0x00000004,0x00000084,0x00000044,0x000000C4,0x00000024,0x000000A4,0x00000064,0x000000E4,
0x00000014,0x00000094,0x00000054,0x000000D4,0x00000034,0x000000B4,0x00000074,0x000000F4,
0x0000000C,0x0000008C,0x0000004C,0x000000CC,0x0000002C,0x000000AC,0x0000006C,0x000000EC,
0x0000001C,0x0000009C,0x0000005C,0x000000DC,0x0000003C,0x000000BC,0x0000007C,0x000000FC,
0x00000002,0x00000082,0x00000042,0x000000C2,0x00000022,0x000000A2,0x00000062,0x000000E2,
0x00000012,0x00000092,0x00000052,0x000000D2,0x00000032,0x000000B2,0x00000072,0x000000F2,
0x0000000A,0x0000008A,0x0000004A,0x000000CA,0x0000002A,0x000000AA,0x0000006A,0x000000EA,
0x0000001A,0x0000009A,0x0000005A,0x000000DA,0x0000003A,0x000000BA,0x0000007A,0x000000FA,
0x00000006,0x00000086,0x00000046,0x000000C6,0x00000026,0x000000A6,0x00000066,0x000000E6,
0x00000016,0x00000096,0x00000056,0x000000D6,0x00000036,0x000000B6,0x00000076,0x000000F6,
0x0000000E,0x0000008E,0x0000004E,0x000000CE,0x0000002E,0x000000AE,0x0000006E,0x000000EE,
0x0000001E,0x0000009E,0x0000005E,0x000000DE,0x0000003E,0x000000BE,0x0000007E,0x000000FE,
0x00000001,0x00000081,0x00000041,0x000000C1,0x00000021,0x000000A1,0x00000061,0x000000E1,
0x00000011,0x00000091,0x00000051,0x000000D1,0x00000031,0x000000B1,0x00000071,0x000000F1,
0x00000009,0x00000089,0x00000049,0x000000C9,0x00000029,0x000000A9,0x00000069,0x000000E9,
0x00000019,0x00000099,0x00000059,0x000000D9,0x00000039,0x000000B9,0x00000079,0x000000F9,
0x00000005,0x00000085,0x00000045,0x000000C5,0x00000025,0x000000A5,0x00000065,0x000000E5,
0x00000015,0x00000095,0x00000055,0x000000D5,0x00000035,0x000000B5,0x00000075,0x000000F5,
0x0000000D,0x0000008D,0x0000004D,0x000000CD,0x0000002D,0x000000AD,0x0000006D,0x000000ED,
0x0000001D,0x0000009D,0x0000005D,0x000000DD,0x0000003D,0x000000BD,0x0000007D,0x000000FD,
0x00000003,0x00000083,0x00000043,0x000000C3,0x00000023,0x000000A3,0x00000063,0x000000E3,
0x00000013,0x00000093,0x00000053,0x000000D3,0x00000033,0x000000B3,0x00000073,0x000000F3,
0x0000000B,0x0000008B,0x0000004B,0x000000CB,0x0000002B,0x000000AB,0x0000006B,0x000000EB,
0x0000001B,0x0000009B,0x0000005B,0x000000DB,0x0000003B,0x000000BB,0x0000007B,0x000000FB,
0x00000007,0x00000087,0x00000047,0x000000C7,0x00000027,0x000000A7,0x00000067,0x000000E7,
0x00000017,0x00000097,0x00000057,0x000000D7,0x00000037,0x000000B7,0x00000077,0x000000F7,
0x0000000F,0x0000008F,0x0000004F,0x000000CF,0x0000002F,0x000000AF,0x0000006F,0x000000EF,
0x0000001F,0x0000009F,0x0000005F,0x000000DF,0x0000003F,0x000000BF,0x0000007F,0x000000FF,
},{
0x00000000,0x0000C000,0x00003000,0x0000F000,0x00000C00,0x0000CC00,0x00003C00,0x0000FC00,
0x00000300,0x0000C300,0x00003300,0x0000F300,0x00000F00,0x0000CF00,0x00003F00,0x0000FF00,
0x000000C0,0x0000C0C0,0x000030C0,0x0000F0C0,0x00000CC0,0x0000CCC0,0x00003CC0,0x0000FCC0,
0x000003C0,0x0000C3C0,0x000033C0,0x0000F3C0,0x00000FC0,0x0000CFC0,0x00003FC0,0x0000FFC0,
0x00000030,0x0000C030,0x00003030,0x0000F030,0x00000C30,0x0000CC30,0x00003C30,0x0000FC30,
0x00000330,0x0000C330,0x00003330,0x0000F330,0x00000F30,0x0000CF30,0x00003F30,0x0000FF30,
0x000000F0,0x0000C0F0,0x000030F0,0x0000F0F0,0x00000CF0,0x0000CCF0,0x00003CF0,0x0000FCF0,
0x000003F0,0x0000C3F0,0x000033F0,0x0000F3F0,0x00000FF0,0x0000CFF0,0x00003FF0,0x0000FFF0,
0x0000000C,0x0000C00C,0x0000300C,0x0000F00C,0x00000C0C,0x0000CC0C,0x00003C0C,0x0000FC0C,
0x0000030C,0x0000C30C,0x0000330C,0x0000F30C,0x00000F0C,0x0000CF0C,0x00003F0C,0x0000FF0C,
0x000000CC,0x0000C0CC,0x000030CC,0x0000F0CC,0x00000CCC,0x0000CCCC,0x00003CCC,0x0000FCCC,
0x000003CC,0x0000C3CC,0x000033CC,0x0000F3CC,0x00000FCC,0x0000CFCC,0x00003FCC,0x0000FFCC,
0x0000003C,0x0000C03C,0x0000303C,0x0000F03C,0x00000C3C,0x0000CC3C,0x00003C3C,0x0000FC3C,
0x0000033C,0x0000C33C,0x0000333C,0x0000F33C,0x00000F3C,0x0000CF3C,0x00003F3C,0x0000FF3C,
0x000000FC,0x0000C0FC,0x000030FC,0x0000F0FC,0x00000CFC,0x0000CCFC,0x00003CFC,0x0000FCFC,
0x000003FC,0x0000C3FC,0x000033FC,0x0000F3FC,0x00000FFC,0x0000CFFC,0x00003FFC,0x0000FFFC,
0x00000003,0x0000C003,0x00003003,0x0000F003,0x00000C03,0x0000CC03,0x00003C03,0x0000FC03,
0x00000303,0x0000C303,0x00003303,0x0000F303,0x00000F03,0x0000CF03,0x00003F03,0x0000FF03,
0x000000C3,0x0000C0C3,0x000030C3,0x0000F0C3,0x00000CC3,0x0000CCC3,0x00003CC3,0x0000FCC3,
0x000003C3,0x0000C3C3,0x000033C3,0x0000F3C3,0x00000FC3,0x0000CFC3,0x00003FC3,0x0000FFC3,
0x00000033,0x0000C033,0x00003033,0x0000F033,0x00000C33,0x0000CC33,0x00003C33,0x0000FC33,
0x00000333,0x0000C333,0x00003333,0x0000F333,0x00000F33,0x0000CF33,0x00003F33,0x0000FF33,
0x000000F3,0x0000C0F3,0x000030F3,0x0000F0F3,0x00000CF3,0x0000CCF3,0x00003CF3,0x0000FCF3,
0x000003F3,0x0000C3F3,0x000033F3,0x0000F3F3,0x00000FF3,0x0000CFF3,0x00003FF3,0x0000FFF3,
0x0000000F,0x0000C00F,0x0000300F,0x0000F00F,0x00000C0F,0x0000CC0F,0x00003C0F,0x0000FC0F,
0x0000030F,0x0000C30F,0x0000330F,0x0000F30F,0x00000F0F,0x0000CF0F,0x00003F0F,0x0000FF0F,
0x000000CF,0x0000C0CF,0x000030CF,0x0000F0CF,0x00000CCF,0x0000CCCF,0x00003CCF,0x0000FCCF,
0x000003CF,0x0000C3CF,0x000033CF,0x0000F3CF,0x00000FCF,0x0000CFCF,0x00003FCF,0x0000FFCF,
0x0000003F,0x0000C03F,0x0000303F,0x0000F03F,0x00000C3F,0x0000CC3F,0x00003C3F,0x0000FC3F,
0x0000033F,0x0000C33F,0x0000333F,0x0000F33F,0x00000F3F,0x0000CF3F,0x00003F3F,0x0000FF3F,
0x000000FF,0x0000C0FF,0x000030FF,0x0000F0FF,0x00000CFF,0x0000CCFF,0x00003CFF,0x0000FCFF,
0x000003FF,0x0000C3FF,0x000033FF,0x0000F3FF,0x00000FFF,0x0000CFFF,0x00003FFF,0x0000FFFF,
},{
0x00000000,0x00000080,0x00000040,0x000000C0,0x00000020,0x000000A0,0x00000060,0x000000E0,
0x00000010,0x00000090,0x00000050,0x000000D0,0x00000030,0x000000B0,0x00000070,0x000000F0,
0x00000008,0x00000088,0x00000048,0x000000C8,0x00000028,0x000000A8,0x00000068,0x000000E8,
0x00000018,0x00000098,0x00000058,0x000000D8,0x00000038,0x000000B8,0x00000078,0x000000F8,
0x00000004,0x00000084,0x00000044,0x000000C4,0x00000024,0x000000A4,0x00000064,0x000000E4,
0x00000014,0x00000094,0x00000054,0x000000D4,0x00000034,0x000000B4,0x00000074,0x000000F4,
0x0000000C,0x0000008C,0x0000004C,0x000000CC,0x0000002C,0x000000AC,0x0000006C,0x000000EC,
0x0000001C,0x0000009C,0x0000005C,0x000000DC,0x0000003C,0x000000BC,0x0000007C,0x000000FC,
0x00000002,0x00000082,0x00000042,0x000000C2,0x00000022,0x000000A2,0x00000062,0x000000E2,
0x00000012,0x00000092,0x00000052,0x000000D2,0x00000032,0x000000B2,0x00000072,0x000000F2,
0x0000000A,0x0000008A,0x0000004A,0x000000CA,0x0000002A,0x000000AA,0x0000006A,0x000000EA,
0x0000001A,0x0000009A,0x0000005A,0x000000DA,0x0000003A,0x000000BA,0x0000007A,0x000000FA,
0x00000006,0x00000086,0x00000046,0x000000C6,0x00000026,0x000000A6,0x00000066,0x000000E6,
0x00000016,0x00000096,0x00000056,0x000000D6,0x00000036,0x000000B6,0x00000076,0x000000F6,
0x0000000E,0x0000008E,0x0000004E,0x000000CE,0x0000002E,0x000000AE,0x0000006E,0x000000EE,
0x0000001E,0x0000009E,0x0000005E,0x000000DE,0x0000003E,0x000000BE,0x0000007E,0x000000FE,
0x00000001,0x00000081,0x00000041,0x000000C1,0x00000021,0x000000A1,0x00000061,0x000000E1,
0x00000011,0x00000091,0x00000051,0x000000D1,0x00000031,0x000000B1,0x00000071,0x000000F1,
0x00000009,0x00000089,0x00000049,0x000000C9,0x00000029,0x000000A9,0x00000069,0x000000E9,
0x00000019,0x00000099,0x00000059,0x000000D9,0x00000039,0x000000B9,0x00000079,0x000000F9,
0x00000005,0x00000085,0x00000045,0x000000C5,0x00000025,0x000000A5,0x00000065,0x000000E5,
0x00000015,0x00000095,0x00000055,0x000000D5,0x00000035,0x000000B5,0x00000075,0x000000F5,
0x0000000D,0x0000008D,0x0000004D,0x000000CD,0x0000002D,0x000000AD,0x0000006D,0x000000ED,
0x0000001D,0x0000009D,0x0000005D,0x000000DD,0x0000003D,0x000000BD,0x0000007D,0x000000FD,
0x00000003,0x00000083,0x00000043,0x000000C3,0x00000023,0x000000A3,0x00000063,0x000000E3,
0x00000013,0x00000093,0x00000053,0x000000D3,0x00000033,0x000000B3,0x00000073,0x000000F3,
0x0000000B,0x0000008B,0x0000004B,0x000000CB,0x0000002B,0x000000AB,0x0000006B,0x000000EB,
0x0000001B,0x0000009B,0x0000005B,0x000000DB,0x0000003B,0x000000BB,0x0000007B,0x000000FB,
0x00000007,0x00000087,0x00000047,0x000000C7,0x00000027,0x000000A7,0x00000067,0x000000E7,
0x00000017,0x00000097,0x00000057,0x000000D7,0x00000037,0x000000B7,0x00000077,0x000000F7,
0x0000000F,0x0000008F,0x0000004F,0x000000CF,0x0000002F,0x000000AF,0x0000006F,0x000000EF,
0x0000001F,0x0000009F,0x0000005F,0x000000DF,0x0000003F,0x000000BF,0x0000007F,0x000000FF,
},{
0x00000000,0xF0000000,0x0F000000,0xFF000000,0x00F00000,0xF0F00000,0x0FF00000,0xFFF00000,
0x000F0000,0xF00F0000,0x0F0F0000,0xFF0F0000,0x00FF0000,0xF0FF0000,0x0FFF0000,0xFFFF0000,
0x0000F000,0xF000F000,0x0F00F000,0xFF00F000,0x00F0F000,0xF0F0F000,0x0FF0F000,0xFFF0F000,
0x000FF000,0xF00FF000,0x0F0FF000,0xFF0FF000,0x00FFF000,0xF0FFF000,0x0FFFF000,0xFFFFF000,
0x00000F00,0xF0000F00,0x0F000F00,0xFF000F00,0x00F00F00,0xF0F00F00,0x0FF00F00,0xFFF00F00,
0x000F0F00,0xF00F0F00,0x0F0F0F00,0xFF0F0F00,0x00FF0F00,0xF0FF0F00,0x0FFF0F00,0xFFFF0F00,
0x0000FF00,0xF000FF00,0x0F00FF00,0xFF00FF00,0x00F0FF00,0xF0F0FF00,0x0FF0FF00,0xFFF0FF00,
0x000FFF00,0xF00FFF00,0x0F0FFF00,0xFF0FFF00,0x00FFFF00,0xF0FFFF00,0x0FFFFF00,0xFFFFFF00,
0x000000F0,0xF00000F0,0x0F0000F0,0xFF0000F0,0x00F000F0,0xF0F000F0,0x0FF000F0,0xFFF000F0,
0x000F00F0,0xF00F00F0,0x0F0F00F0,0xFF0F00F0,0x00FF00F0,0xF0FF00F0,0x0FFF00F0,0xFFFF00F0,
0x0000F0F0,0xF000F0F0,0x0F00F0F0,0xFF00F0F0,0x00F0F0F0,0xF0F0F0F0,0x0FF0F0F0,0xFFF0F0F0,
0x000FF0F0,0xF00FF0F0,0x0F0FF0F0,0xFF0FF0F0,0x00FFF0F0,0xF0FFF0F0,0x0FFFF0F0,0xFFFFF0F0,
0x00000FF0,0xF0000FF0,0x0F000FF0,0xFF000FF0,0x00F00FF0,0xF0F00FF0,0x0FF00FF0,0xFFF00FF0,
0x000F0FF0,0xF00F0FF0,0x0F0F0FF0,0xFF0F0FF0,0x00FF0FF0,0xF0FF0FF0,0x0FFF0FF0,0xFFFF0FF0,
0x0000FFF0,0xF000FFF0,0x0F00FFF0,0xFF00FFF0,0x00F0FFF0,0xF0F0FFF0,0x0FF0FFF0,0xFFF0FFF0,
0x000FFFF0,0xF00FFFF0,0x0F0FFFF0,0xFF0FFFF0,0x00FFFFF0,0xF0FFFFF0,0x0FFFFFF0,0xFFFFFFF0,
0x0000000F,0xF000000F,0x0F00000F,0xFF00000F,0x00F0000F,0xF0F0000F,0x0FF0000F,0xFFF0000F,
0x000F000F,0xF00F000F,0x0F0F000F,0xFF0F000F,0x00FF000F,0xF0FF000F,0x0FFF000F,0xFFFF000F,
0x0000F00F,0xF000F00F,0x0F00F00F,0xFF00F00F,0x00F0F00F,0xF0F0F00F,0x0FF0F00F,0xFFF0F00F,
0x000FF00F,0xF00FF00F,0x0F0FF00F,0xFF0FF00F,0x00FFF00F,0xF0FFF00F,0x0FFFF00F,0xFFFFF00F,
0x00000F0F,0xF0000F0F,0x0F000F0F,0xFF000F0F,0x00F00F0F,0xF0F00F0F,0x0FF00F0F,0xFFF00F0F,
0x000F0F0F,0xF00F0F0F,0x0F0F0F0F,0xFF0F0F0F,0x00FF0F0F,0xF0FF0F0F,0x0FFF0F0F,0xFFFF0F0F,
0x0000FF0F,0xF000FF0F,0x0F00FF0F,0xFF00FF0F,0x00F0FF0F,0xF0F0FF0F,0x0FF0FF0F,0xFFF0FF0F,
0x000FFF0F,0xF00FFF0F,0x0F0FFF0F,0xFF0FFF0F,0x00FFFF0F,0xF0FFFF0F,0x0FFFFF0F,0xFFFFFF0F,
0x000000FF,0xF00000FF,0x0F0000FF,0xFF0000FF,0x00F000FF,0xF0F000FF,0x0FF000FF,0xFFF000FF,
0x000F00FF,0xF00F00FF,0x0F0F00FF,0xFF0F00FF,0x00FF00FF,0xF0FF00FF,0x0FFF00FF,0xFFFF00FF,
0x0000F0FF,0xF000F0FF,0x0F00F0FF,0xFF00F0FF,0x00F0F0FF,0xF0F0F0FF,0x0FF0F0FF,0xFFF0F0FF,
0x000FF0FF,0xF00FF0FF,0x0F0FF0FF,0xFF0FF0FF,0x00FFF0FF,0xF0FFF0FF,0x0FFFF0FF,0xFFFFF0FF,
0x00000FFF,0xF0000FFF,0x0F000FFF,0xFF000FFF,0x00F00FFF,0xF0F00FFF,0x0FF00FFF,0xFFF00FFF,
0x000F0FFF,0xF00F0FFF,0x0F0F0FFF,0xFF0F0FFF,0x00FF0FFF,0xF0FF0FFF,0x0FFF0FFF,0xFFFF0FFF,
0x0000FFFF,0xF000FFFF,0x0F00FFFF,0xFF00FFFF,0x00F0FFFF,0xF0F0FFFF,0x0FF0FFFF,0xFFF0FFFF,
0x000FFFFF,0xF00FFFFF,0x0F0FFFFF,0xFF0FFFFF,0x00FFFFFF,0xF0FFFFFF,0x0FFFFFFF,0xFFFFFFFF,
},};

static ULONG *grafp_ptr[4];
static int global_sizem[4];

static const int PM_Width[4] = {1, 2, 1, 4};

/* Meaning of bits in GTIA_pm_scanline:
bit 0 - Player 0
bit 1 - Player 1
bit 2 - Player 2
bit 3 - Player 3
bit 4 - Missile 0
bit 5 - Missile 1
bit 6 - Missile 2
bit 7 - Missile 3
*/

UBYTE GTIA_pm_scanline[Screen_WIDTH / 2 + 8];	/* there's a byte for every *pair* of pixels */
int GTIA_pm_dirty = TRUE;

#define C_PM0	0x01
#define C_PM1	0x02
#define C_PM01	0x03
#define C_PM2	0x04
#define C_PM3	0x05
#define C_PM23	0x06
#define C_PM023	0x07
#define C_PM123	0x08
#define C_PM0123 0x09
#define C_PM25	0x0a
#define C_PM35	0x0b
#define C_PM235	0x0c
#define C_COLLS	0x0d
#define C_BAK	0x00
#define C_HI2	0x20
#define C_HI3	0x30
#define C_PF0	0x40
#define C_PF1	0x50
#define C_PF2	0x60
#define C_PF3	0x70

#define PF0PM (*(UBYTE *) &ANTIC_cl[C_PF0 | C_COLLS])
#define PF1PM (*(UBYTE *) &ANTIC_cl[C_PF1 | C_COLLS])
#define PF2PM (*(UBYTE *) &ANTIC_cl[C_PF2 | C_COLLS])
#define PF3PM (*(UBYTE *) &ANTIC_cl[C_PF3 | C_COLLS])

/* Colours ----------------------------------------------------------------- */

#ifdef USE_COLOUR_TRANSLATION_TABLE
UWORD colour_translation_table[256];
#endif /* USE_COLOUR_TRANSLATION_TABLE */

static void setup_gtia9_11(void) {
	int i;
#ifdef USE_COLOUR_TRANSLATION_TABLE
	UWORD temp;
	temp = colour_translation_table[GTIA_COLBK & 0xf0];
	ANTIC_lookup_gtia11[0] = ((ULONG) temp << 16) + temp;
	for (i = 1; i < 16; i++) {
		temp = colour_translation_table[GTIA_COLBK | i];
		ANTIC_lookup_gtia9[i] = ((ULONG) temp << 16) + temp;
		temp = colour_translation_table[GTIA_COLBK | (i << 4)];
		ANTIC_lookup_gtia11[i] = ((ULONG) temp << 16) + temp;
	}
#else
	ULONG count9 = 0;
	ULONG count11 = 0;
	ANTIC_lookup_gtia11[0] = ANTIC_lookup_gtia9[0] & 0xf0f0f0f0;
	for (i = 1; i < 16; i++) {
		ANTIC_lookup_gtia9[i] = ANTIC_lookup_gtia9[0] | (count9 += 0x01010101);
		ANTIC_lookup_gtia11[i] = ANTIC_lookup_gtia9[0] | (count11 += 0x10101010);
	}
#endif
}

#endif /* defined(BASIC) || defined(CURSES_BASIC) */

/* Initialization ---------------------------------------------------------- */

int GTIA_Initialise(int *argc, char *argv[])
{
#if !defined(BASIC) && !defined(CURSES_BASIC)
	int i;
#if 0
	for (i = 0; i < 256; i++) {
		int tmp = i + 0x100;
		ULONG grafp1 = 0;
		ULONG grafp2 = 0;
		ULONG grafp4 = 0;
		do {
			grafp1 <<= 1;
			grafp2 <<= 2;
			grafp4 <<= 4;
			if (tmp & 1) {
				grafp1++;
				grafp2 += 3;
				grafp4 += 15;
			}
			tmp >>= 1;
		} while (tmp != 1);
		grafp_lookup[2][i] = grafp_lookup[0][i] = grafp1;
		grafp_lookup[1][i] = grafp2;
		grafp_lookup[3][i] = grafp4;
	}
#endif
	memset(ANTIC_cl, GTIA_COLOUR_BLACK, sizeof(ANTIC_cl));
	for (i = 0; i < 32; i++)
		GTIA_PutByte((UWORD) i, 0);
#endif /* !defined(BASIC) && !defined(CURSES_BASIC) */

	(void)argc; (void)argv;  /* prevent "unused parameter" warnings */
	return TRUE;
}

#ifdef NEW_CYCLE_EXACT

/* generate updated PxPL and MxPL for part of a scanline */
/* slow, but should be called rarely */
static void generate_partial_pmpl_colls(int l, int r)
{
	int i;
	if (r < 0 || l >= (int) sizeof(GTIA_pm_scanline) / (int) sizeof(GTIA_pm_scanline[0]))
		return;
	if (r >= (int) sizeof(GTIA_pm_scanline) / (int) sizeof(GTIA_pm_scanline[0])) {
		r = (int) sizeof(GTIA_pm_scanline) / (int) sizeof(GTIA_pm_scanline[0]);
	}
	if (l < 0)
		l = 0;

	for (i = l; i <= r; i++) {
		UBYTE p = GTIA_pm_scanline[i];
/* It is possible that some bits are set in PxPL/MxPL here, which would
 * not otherwise be set ever in GTIA_NewPmScanline.  This is because the
 * player collisions are always generated in order in GTIA_NewPmScanline.
 * However this does not cause any problem because we never use those bits
 * of PxPL/MxPL in the collision reading code.
 */
		GTIA_P1PL |= (p & (1 << 1)) ?  p : 0;
		GTIA_P2PL |= (p & (1 << 2)) ?  p : 0;
		GTIA_P3PL |= (p & (1 << 3)) ?  p : 0;
		GTIA_M0PL |= (p & (0x10 << 0)) ?  p : 0;
		GTIA_M1PL |= (p & (0x10 << 1)) ?  p : 0;
		GTIA_M2PL |= (p & (0x10 << 2)) ?  p : 0;
		GTIA_M3PL |= (p & (0x10 << 3)) ?  p : 0;
	}

}

/* update pm->pl collisions for a partial scanline */
static void update_partial_pmpl_colls(void)
{
	int l = collision_curpos;
	int r = ANTIC_XPOS * 2 - 37;
	generate_partial_pmpl_colls(l, r);
	collision_curpos = r;
}

/* update pm-> pl collisions at the end of a scanline */
void GTIA_UpdatePmplColls(void)
{
	if (hitclr_pos != 0){
		generate_partial_pmpl_colls(hitclr_pos,
				sizeof(GTIA_pm_scanline) / sizeof(GTIA_pm_scanline[0]) - 1);
/* If hitclr was written to, then only part of GTIA_pm_scanline should be used
 * for collisions */

	}
	else {
/* otherwise the whole of pm_scaline can be used for collisions.  This will
 * update the collision registers based on the generated collisions for the
 * current line */
		GTIA_P1PL |= P1PL_T;
		GTIA_P2PL |= P2PL_T;
		GTIA_P3PL |= P3PL_T;
		GTIA_M0PL |= M0PL_T;
		GTIA_M1PL |= M1PL_T;
		GTIA_M2PL |= M2PL_T;
		GTIA_M3PL |= M3PL_T;
	}
	collision_curpos = 0;
	hitclr_pos = 0;
}

#else
#define update_partial_pmpl_colls()
#endif /* NEW_CYCLE_EXACT */

/* Prepare PMG scanline ---------------------------------------------------- */

#if !defined(BASIC) && !defined(CURSES_BASIC)

void GTIA_NewPmScanline(void)
{
#ifdef NEW_CYCLE_EXACT
/* reset temporary pm->pl collisions */
	P1PL_T = P2PL_T = P3PL_T = 0;
	M0PL_T = M1PL_T = M2PL_T = M3PL_T = 0;
#endif /* NEW_CYCLE_EXACT */
/* Clear if necessary */
	if (GTIA_pm_dirty) {
		memset(GTIA_pm_scanline, 0, Screen_WIDTH / 2);
		GTIA_pm_dirty = FALSE;
	}

/* Draw Players */

#define DO_PLAYER(n)	if (GTIA_GRAFP##n) {						\
	ULONG grafp = grafp_ptr[n][GTIA_GRAFP##n] & hposp_mask[n];	\
	if (grafp) {											\
		UBYTE *ptr = hposp_ptr[n];							\
		GTIA_pm_dirty = TRUE;									\
		do {												\
			if (grafp & 1)									\
				P##n##PL_T |= *ptr |= 1 << n;					\
			ptr++;											\
			grafp >>= 1;									\
		} while (grafp);									\
	}														\
}

	/* optimized DO_PLAYER(0): GTIA_pm_scanline is clear and P0PL is unused */
	if (GTIA_GRAFP0) {
		ULONG grafp = grafp_ptr[0][GTIA_GRAFP0] & hposp_mask[0];
		if (grafp) {
			UBYTE *ptr = hposp_ptr[0];
			GTIA_pm_dirty = TRUE;
			do {
				if (grafp & 1)
					*ptr = 1;
				ptr++;
				grafp >>= 1;
			} while (grafp);
		}
	}

	DO_PLAYER(1)
	DO_PLAYER(2)
	DO_PLAYER(3)

/* Draw Missiles */

#define DO_MISSILE(n,p,m,r,l)	if (GTIA_GRAFM & m) {	\
	int j = global_sizem[n];						\
	UBYTE *ptr = hposm_ptr[n];						\
	if (GTIA_GRAFM & r) {								\
		if (GTIA_GRAFM & l)								\
			j <<= 1;								\
	}												\
	else											\
		ptr += j;									\
	if (ptr < GTIA_pm_scanline + 2) {					\
		j += ptr - GTIA_pm_scanline - 2;					\
		ptr = GTIA_pm_scanline + 2;						\
	}												\
	else if (ptr + j > GTIA_pm_scanline + Screen_WIDTH / 2 - 2)	\
		j = GTIA_pm_scanline + Screen_WIDTH / 2 - 2 - ptr;		\
	if (j > 0)										\
		do											\
			M##n##PL_T |= *ptr++ |= p;				\
		while (--j);								\
}

	if (GTIA_GRAFM) {
		GTIA_pm_dirty = TRUE;
		DO_MISSILE(3, 0x80, 0xc0, 0x80, 0x40)
		DO_MISSILE(2, 0x40, 0x30, 0x20, 0x10)
		DO_MISSILE(1, 0x20, 0x0c, 0x08, 0x04)
		DO_MISSILE(0, 0x10, 0x03, 0x02, 0x01)
	}
}

#endif /* !defined(BASIC) && !defined(CURSES_BASIC) */

/* GTIA registers ---------------------------------------------------------- */

void GTIA_Frame(void)
{
#ifdef BASIC
	consol = 0xf;
#else
	consol = INPUT_key_consol | 0x08;
#endif

	if (GTIA_GRACTL & 4) {
		GTIA_TRIG_latch[0] &= GTIA_TRIG[0];
		GTIA_TRIG_latch[1] &= GTIA_TRIG[1];
		GTIA_TRIG_latch[2] &= GTIA_TRIG[2];
		GTIA_TRIG_latch[3] &= GTIA_TRIG[3];
	}
}

UBYTE GTIA_GetByte(UWORD addr, int no_side_effects)
{
	switch (addr & 0x1f) {
	case GTIA_OFFSET_M0PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x10) >> 4)
		      + ((PF1PM & 0x10) >> 3)
		      + ((PF2PM & 0x10) >> 2)
		      + ((PF3PM & 0x10) >> 1)) & GTIA_collisions_mask_missile_playfield;
	case GTIA_OFFSET_M1PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x20) >> 5)
		      + ((PF1PM & 0x20) >> 4)
		      + ((PF2PM & 0x20) >> 3)
		      + ((PF3PM & 0x20) >> 2)) & GTIA_collisions_mask_missile_playfield;
	case GTIA_OFFSET_M2PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x40) >> 6)
		      + ((PF1PM & 0x40) >> 5)
		      + ((PF2PM & 0x40) >> 4)
		      + ((PF3PM & 0x40) >> 3)) & GTIA_collisions_mask_missile_playfield;
	case GTIA_OFFSET_M3PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x80) >> 7)
		      + ((PF1PM & 0x80) >> 6)
		      + ((PF2PM & 0x80) >> 5)
		      + ((PF3PM & 0x80) >> 4)) & GTIA_collisions_mask_missile_playfield;
	case GTIA_OFFSET_P0PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return ((PF0PM & 0x01)
		      + ((PF1PM & 0x01) << 1)
		      + ((PF2PM & 0x01) << 2)
		      + ((PF3PM & 0x01) << 3)) & GTIA_collisions_mask_player_playfield;
	case GTIA_OFFSET_P1PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x02) >> 1)
		      + (PF1PM & 0x02)
		      + ((PF2PM & 0x02) << 1)
		      + ((PF3PM & 0x02) << 2)) & GTIA_collisions_mask_player_playfield;
	case GTIA_OFFSET_P2PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x04) >> 2)
		      + ((PF1PM & 0x04) >> 1)
		      + (PF2PM & 0x04)
		      + ((PF3PM & 0x04) << 1)) & GTIA_collisions_mask_player_playfield;
	case GTIA_OFFSET_P3PF:
#ifdef NEW_CYCLE_EXACT
	if (ANTIC_DRAWING_SCREEN) {
			ANTIC_UpdateScanline();
	}
#endif
		return (((PF0PM & 0x08) >> 3)
		      + ((PF1PM & 0x08) >> 2)
		      + ((PF2PM & 0x08) >> 1)
		      + (PF3PM & 0x08)) & GTIA_collisions_mask_player_playfield;
	case GTIA_OFFSET_M0PL:
		update_partial_pmpl_colls();
		return GTIA_M0PL & GTIA_collisions_mask_missile_player;
	case GTIA_OFFSET_M1PL:
		update_partial_pmpl_colls();
		return GTIA_M1PL & GTIA_collisions_mask_missile_player;
	case GTIA_OFFSET_M2PL:
		update_partial_pmpl_colls();
		return GTIA_M2PL & GTIA_collisions_mask_missile_player;
	case GTIA_OFFSET_M3PL:
		update_partial_pmpl_colls();
		return GTIA_M3PL & GTIA_collisions_mask_missile_player;
	case GTIA_OFFSET_P0PL:
		update_partial_pmpl_colls();
		return (((GTIA_P1PL & 0x01) << 1)  /* mask in player 1 */
		      + ((GTIA_P2PL & 0x01) << 2)  /* mask in player 2 */
		      + ((GTIA_P3PL & 0x01) << 3)) /* mask in player 3 */
		     & GTIA_collisions_mask_player_player;
	case GTIA_OFFSET_P1PL:
		update_partial_pmpl_colls();
		return ((GTIA_P1PL & 0x01)         /* mask in player 0 */
		      + ((GTIA_P2PL & 0x02) << 1)  /* mask in player 2 */
		      + ((GTIA_P3PL & 0x02) << 2)) /* mask in player 3 */
		     & GTIA_collisions_mask_player_player;
	case GTIA_OFFSET_P2PL:
		update_partial_pmpl_colls();
		return ((GTIA_P2PL & 0x03)         /* mask in player 0 and 1 */
		      + ((GTIA_P3PL & 0x04) << 1)) /* mask in player 3 */
		     & GTIA_collisions_mask_player_player;
	case GTIA_OFFSET_P3PL:
		update_partial_pmpl_colls();
		return (GTIA_P3PL & 0x07)          /* mask in player 0,1, and 2 */
		     & GTIA_collisions_mask_player_player;
	case GTIA_OFFSET_TRIG0:
		return GTIA_TRIG[0] & GTIA_TRIG_latch[0];
	case GTIA_OFFSET_TRIG1:
		return GTIA_TRIG[1] & GTIA_TRIG_latch[1];
	case GTIA_OFFSET_TRIG2:
		return GTIA_TRIG[2] & GTIA_TRIG_latch[2];
	case GTIA_OFFSET_TRIG3:
		return GTIA_TRIG[3] & GTIA_TRIG_latch[3];
	case GTIA_OFFSET_PAL:
		return (Atari800_tv_mode == Atari800_TV_PAL) ? 0x01 : 0x0f;
	case GTIA_OFFSET_CONSOL:
		{
			UBYTE byte = consol & consol_mask;
			if (!no_side_effects && GTIA_consol_override > 0) {
				/* Check if we're called from outside OS. This avoids sending
				   console keystrokes to diagnostic cartridges. */
				if (CPU_regPC < 0xc000)
					/* Not from OS. Disable console override. */
					GTIA_consol_override = 0;
				else {
				--GTIA_consol_override;
					if (Atari800_builtin_basic && Atari800_disable_basic && !BINLOAD_loading_basic)
						/* Only for XL/XE - hold Option during reboot. */
						byte &= ~INPUT_CONSOL_OPTION;
					if (CASSETTE_hold_start && Atari800_machine_type != Atari800_MACHINE_5200) {
						/* Only for the computers - hold Start during reboot. */
						byte &= ~INPUT_CONSOL_START;
						if (GTIA_consol_override == 0) {
							/* press Space after Start to start cassette boot. */
							CASSETTE_press_space = 1;
							CASSETTE_hold_start = CASSETTE_hold_start_on_reboot;
						}
					}
				}
			}
			return byte;
		}
	default:
		break;
	}

	return 0xf;
}

void GTIA_PutByte(UWORD addr, UBYTE byte)
{
#if !defined(BASIC) && !defined(CURSES_BASIC)
	UWORD cword;
	UWORD cword2;

#ifdef NEW_CYCLE_EXACT
	int x; /* the cycle-exact update position in GTIA_pm_scanline */
	if (ANTIC_DRAWING_SCREEN) {
		if ((addr & 0x1f) != GTIA_PRIOR) {
			ANTIC_UpdateScanline();
		} else {
			ANTIC_UpdateScanlinePrior(byte);
		}
	}
#define UPDATE_PM_CYCLE_EXACT if(ANTIC_DRAWING_SCREEN) GTIA_NewPmScanline();
#else
#define UPDATE_PM_CYCLE_EXACT
#endif

#endif /* !defined(BASIC) && !defined(CURSES_BASIC) */

	switch (addr & 0x1f) {
	case GTIA_OFFSET_CONSOL:
		GTIA_speaker = !(byte & 0x08);
#ifdef CONSOLE_SOUND
		POKEYSND_UpdateConsol(1);
#endif
		consol_mask = (~byte) & 0x0f;
		break;

#if defined(BASIC) || defined(CURSES_BASIC)

	/* We use these for Antic modes 6, 7 on Curses */
	case GTIA_OFFSET_COLPF0:
		GTIA_COLPF0 = byte;
		break;
	case GTIA_OFFSET_COLPF1:
		GTIA_COLPF1 = byte;
		break;
	case GTIA_OFFSET_COLPF2:
		GTIA_COLPF2 = byte;
		break;
	case GTIA_OFFSET_COLPF3:
		GTIA_COLPF3 = byte;
		break;

#else

#ifdef USE_COLOUR_TRANSLATION_TABLE
	case GTIA_OFFSET_COLBK:
		GTIA_COLBK = byte &= 0xfe;
		ANTIC_cl[C_BAK] = cword = colour_translation_table[byte];
		if (cword != (UWORD) (ANTIC_lookup_gtia9[0]) ) {
			ANTIC_lookup_gtia9[0] = cword + (cword << 16);
			if (GTIA_PRIOR & 0x40)
				setup_gtia9_11();
		}
		break;
	case GTIA_OFFSET_COLPF0:
		GTIA_COLPF0 = byte &= 0xfe;
		ANTIC_cl[C_PF0] = cword = GTIA_colour_translation_table[byte];
		if ((GTIA_PRIOR & 1) == 0) {
			ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF0 | C_PM3] = ANTIC_cl[C_PF0 | C_PM2] = cword;
			if ((GTIA_PRIOR & 3) == 0) {
				if (GTIA_PRIOR & 0xf) {
					ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF0 | C_PM1] = ANTIC_cl[C_PF0 | C_PM0] = cword;
					if ((GTIA_PRIOR & 0xf) == 0xc)
						ANTIC_cl[C_PF0 | C_PM0123] = ANTIC_cl[C_PF0 | C_PM123] = ANTIC_cl[C_PF0 | C_PM023] = cword;
				}
				else {
					ANTIC_cl[C_PF0 | C_PM0] = colour_translation_table[byte | GTIA_COLPM0];
					ANTIC_cl[C_PF0 | C_PM1] = colour_translation_table[byte | GTIA_COLPM1];
					ANTIC_cl[C_PF0 | C_PM01] = colour_translation_table[byte | GTIA_COLPM0 | GTIA_COLPM1];
				}
			}
			if ((GTIA_PRIOR & 0xf) >= 0xa)
				ANTIC_cl[C_PF0 | C_PM25] = cword;
		}
		break;
	case GTIA_OFFSET_COLPF1:
		GTIA_COLPF1 = byte &= 0xfe;
		ANTIC_cl[C_PF1] = cword = GTIA_colour_translation_table[byte];
		if ((GTIA_PRIOR & 1) == 0) {
			ANTIC_cl[C_PF1 | C_PM23] = ANTIC_cl[C_PF1 | C_PM3] = ANTIC_cl[C_PF1 | C_PM2] = cword;
			if ((GTIA_PRIOR & 3) == 0) {
				if (GTIA_PRIOR & 0xf) {
					ANTIC_cl[C_PF1 | C_PM01] = ANTIC_cl[C_PF1 | C_PM1] = ANTIC_cl[C_PF1 | C_PM0] = cword;
					if ((GTIA_PRIOR & 0xf) == 0xc)
						ANTIC_cl[C_PF1 | C_PM0123] = ANTIC_cl[C_PF1 | C_PM123] = ANTIC_cl[C_PF1 | C_PM023] = cword;
				}
				else {
					ANTIC_cl[C_PF1 | C_PM0] = colour_translation_table[byte | GTIA_COLPM0];
					ANTIC_cl[C_PF1 | C_PM1] = colour_translation_table[byte | GTIA_COLPM1];
					ANTIC_cl[C_PF1 | C_PM01] = colour_translation_table[byte | GTIA_COLPM0 | GTIA_COLPM1];
				}
			}
		}
		{
			UBYTE byte2 = (GTIA_COLPF2 & 0xf0) + (byte & 0xf);
			ANTIC_cl[C_HI2] = cword = colour_translation_table[byte2];
			ANTIC_cl[C_HI3] = colour_translation_table[(GTIA_COLPF3 & 0xf0) | (byte & 0xf)];
			if (GTIA_PRIOR & 4)
				ANTIC_cl[C_HI2 | C_PM01] = ANTIC_cl[C_HI2 | C_PM1] = ANTIC_cl[C_HI2 | C_PM0] = cword;
			if ((GTIA_PRIOR & 9) == 0) {
				if (GTIA_PRIOR & 0xf)
					ANTIC_cl[C_HI2 | C_PM23] = ANTIC_cl[C_HI2 | C_PM3] = ANTIC_cl[C_HI2 | C_PM2] = cword;
				else {
					ANTIC_cl[C_HI2 | C_PM2] = colour_translation_table[byte2 | (GTIA_COLPM2 & 0xf0)];
					ANTIC_cl[C_HI2 | C_PM3] = colour_translation_table[byte2 | (GTIA_COLPM3 & 0xf0)];
					ANTIC_cl[C_HI2 | C_PM23] = colour_translation_table[byte2 | ((GTIA_COLPM2 | GTIA_COLPM3) & 0xf0)];
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPF2:
		GTIA_COLPF2 = byte &= 0xfe;
		ANTIC_cl[C_PF2] = cword = GTIA_colour_translation_table[byte];
		{
			UBYTE byte2 = (byte & 0xf0) + (GTIA_COLPF1 & 0xf);
			ANTIC_cl[C_HI2] = cword2 = colour_translation_table[byte2];
			if (GTIA_PRIOR & 4) {
				ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF2 | C_PM1] = ANTIC_cl[C_PF2 | C_PM0] = cword;
				ANTIC_cl[C_HI2 | C_PM01] = ANTIC_cl[C_HI2 | C_PM1] = ANTIC_cl[C_HI2 | C_PM0] = cword2;
			}
			if ((GTIA_PRIOR & 9) == 0) {
				if (GTIA_PRIOR & 0xf) {
					ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF2 | C_PM3] = ANTIC_cl[C_PF2 | C_PM2] = cword;
					ANTIC_cl[C_HI2 | C_PM23] = ANTIC_cl[C_HI2 | C_PM3] = ANTIC_cl[C_HI2 | C_PM2] = cword2;
				}
				else {
					ANTIC_cl[C_PF2 | C_PM2] = colour_translation_table[byte | GTIA_COLPM2];
					ANTIC_cl[C_PF2 | C_PM3] = colour_translation_table[byte | GTIA_COLPM3];
					ANTIC_cl[C_PF2 | C_PM23] = colour_translation_table[byte | GTIA_COLPM2 | GTIA_COLPM3];
					ANTIC_cl[C_HI2 | C_PM2] = colour_translation_table[byte2 | (GTIA_COLPM2 & 0xf0)];
					ANTIC_cl[C_HI2 | C_PM3] = colour_translation_table[byte2 | (GTIA_COLPM3 & 0xf0)];
					ANTIC_cl[C_HI2 | C_PM23] = colour_translation_table[byte2 | ((GTIA_COLPM2 | GTIA_COLPM3) & 0xf0)];
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPF3:
		GTIA_COLPF3 = byte &= 0xfe;
		ANTIC_cl[C_PF3] = cword = colour_translation_table[byte];
		ANTIC_cl[C_HI3] = cword2 = colour_translation_table[(byte & 0xf0) | (GTIA_COLPF1 & 0xf)];
		if (GTIA_PRIOR & 4)
			ANTIC_cl[C_PF3 | C_PM01] = ANTIC_cl[C_PF3 | C_PM1] = ANTIC_cl[C_PF3 | C_PM0] = cword;
		if ((GTIA_PRIOR & 9) == 0) {
			if (GTIA_PRIOR & 0xf)
				ANTIC_cl[C_PF3 | C_PM23] = ANTIC_cl[C_PF3 | C_PM3] = ANTIC_cl[C_PF3 | C_PM2] = cword;
			else {
				ANTIC_cl[C_PF3 | C_PM25] = ANTIC_cl[C_PF2 | C_PM25] = ANTIC_cl[C_PM25] = ANTIC_cl[C_PF3 | C_PM2] = colour_translation_table[byte | GTIA_COLPM2];
				ANTIC_cl[C_PF3 | C_PM35] = ANTIC_cl[C_PF2 | C_PM35] = ANTIC_cl[C_PM35] = ANTIC_cl[C_PF3 | C_PM3] = colour_translation_table[byte | GTIA_COLPM3];
				ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = colour_translation_table[byte | GTIA_COLPM2 | GTIA_COLPM3];
				ANTIC_cl[C_PF0 | C_PM235] = ANTIC_cl[C_PF0 | C_PM35] = ANTIC_cl[C_PF0 | C_PM25] =
				ANTIC_cl[C_PF1 | C_PM235] = ANTIC_cl[C_PF1 | C_PM35] = ANTIC_cl[C_PF1 | C_PM25] = cword;
			}
		}
		break;
	case GTIA_OFFSET_COLPM0:
		GTIA_COLPM0 = byte &= 0xfe;
		ANTIC_cl[C_PM023] = ANTIC_cl[C_PM0] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | GTIA_COLPM1;
			ANTIC_cl[C_PM0123] = ANTIC_cl[C_PM01] = cword2 = colour_translation_table[byte2];
			if ((GTIA_PRIOR & 4) == 0) {
				ANTIC_cl[C_PF2 | C_PM0] = ANTIC_cl[C_PF3 | C_PM0] = cword;
				ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF3 | C_PM01] = cword2;
				ANTIC_cl[C_HI2 | C_PM0] = colour_translation_table[(byte & 0xf0) | (GTIA_COLPF1 & 0xf)];
				ANTIC_cl[C_HI2 | C_PM01] = colour_translation_table[(byte2 & 0xf0) | (GTIA_COLPF1 & 0xf)];
				if ((GTIA_PRIOR & 0xc) == 0) {
					if (GTIA_PRIOR & 3) {
						ANTIC_cl[C_PF0 | C_PM0] = ANTIC_cl[C_PF1 | C_PM0] = cword;
						ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF1 | C_PM01] = cword2;
					}
					else {
						ANTIC_cl[C_PF0 | C_PM0] = colour_translation_table[byte | GTIA_COLPF0];
						ANTIC_cl[C_PF1 | C_PM0] = colour_translation_table[byte | GTIA_COLPF1];
						ANTIC_cl[C_PF0 | C_PM01] = colour_translation_table[byte2 | GTIA_COLPF0];
						ANTIC_cl[C_PF1 | C_PM01] = colour_translation_table[byte2 | GTIA_COLPF1];
					}
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPM1:
		GTIA_COLPM1 = byte &= 0xfe;
		ANTIC_cl[C_PM123] = ANTIC_cl[C_PM1] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | GTIA_COLPM0;
			ANTIC_cl[C_PM0123] = ANTIC_cl[C_PM01] = cword2 = colour_translation_table[byte2];
			if ((GTIA_PRIOR & 4) == 0) {
				ANTIC_cl[C_PF2 | C_PM1] = ANTIC_cl[C_PF3 | C_PM1] = cword;
				ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF3 | C_PM01] = cword2;
				ANTIC_cl[C_HI2 | C_PM1] = colour_translation_table[(byte & 0xf0) | (GTIA_COLPF1 & 0xf)];
				ANTIC_cl[C_HI2 | C_PM01] = colour_translation_table[(byte2 & 0xf0) | (GTIA_COLPF1 & 0xf)];
				if ((GTIA_PRIOR & 0xc) == 0) {
					if (GTIA_PRIOR & 3) {
						ANTIC_cl[C_PF0 | C_PM1] = ANTIC_cl[C_PF1 | C_PM1] = cword;
						ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF1 | C_PM01] = cword2;
					}
					else {
						ANTIC_cl[C_PF0 | C_PM1] = colour_translation_table[byte | GTIA_COLPF0];
						ANTIC_cl[C_PF1 | C_PM1] = colour_translation_table[byte | GTIA_COLPF1];
						ANTIC_cl[C_PF0 | C_PM01] = colour_translation_table[byte2 | GTIA_COLPF0];
						ANTIC_cl[C_PF1 | C_PM01] = colour_translation_table[byte2 | GTIA_COLPF1];
					}
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPM2:
		GTIA_COLPM2 = byte &= 0xfe;
		ANTIC_cl[C_PM2] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | GTIA_COLPM3;
			ANTIC_cl[C_PM23] = cword2 = colour_translation_table[byte2];
			if (GTIA_PRIOR & 1) {
				ANTIC_cl[C_PF0 | C_PM2] = ANTIC_cl[C_PF1 | C_PM2] = cword;
				ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF1 | C_PM23] = cword2;
			}
			if ((GTIA_PRIOR & 6) == 0) {
				if (GTIA_PRIOR & 9) {
					ANTIC_cl[C_PF2 | C_PM2] = ANTIC_cl[C_PF3 | C_PM2] = cword;
					ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF3 | C_PM23] = cword2;
					ANTIC_cl[C_HI2 | C_PM2] = colour_translation_table[(byte & 0xf0) | (GTIA_COLPF1 & 0xf)];
					ANTIC_cl[C_HI2 | C_PM23] = colour_translation_table[(byte2 & 0xf0) | (GTIA_COLPF1 & 0xf)];
				}
				else {
					ANTIC_cl[C_PF2 | C_PM2] = colour_translation_table[byte | GTIA_COLPF2];
					ANTIC_cl[C_PF3 | C_PM25] = ANTIC_cl[C_PF2 | C_PM25] = ANTIC_cl[C_PM25] = ANTIC_cl[C_PF3 | C_PM2] = colour_translation_table[byte | GTIA_COLPF3];
					ANTIC_cl[C_PF2 | C_PM23] = colour_translation_table[byte2 | GTIA_COLPF2];
					ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = colour_translation_table[byte2 | GTIA_COLPF3];
					ANTIC_cl[C_HI2 | C_PM2] = colour_translation_table[((byte | GTIA_COLPF2) & 0xf0) | (GTIA_COLPF1 & 0xf)];
					ANTIC_cl[C_HI2 | C_PM25] = colour_translation_table[((byte | GTIA_COLPF3) & 0xf0) | (GTIA_COLPF1 & 0xf)];
					ANTIC_cl[C_HI2 | C_PM23] = colour_translation_table[((byte2 | GTIA_COLPF2) & 0xf0) | (GTIA_COLPF1 & 0xf)];
					ANTIC_cl[C_HI2 | C_PM235] = colour_translation_table[((byte2 | GTIA_COLPF3) & 0xf0) | (GTIA_COLPF1 & 0xf)];
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPM3:
		GTIA_COLPM3 = byte &= 0xfe;
		ANTIC_cl[C_PM3] = cword = colour_translation_table[byte];
		{
			UBYTE byte2 = byte | GTIA_COLPM2;
			ANTIC_cl[C_PM23] = cword2 = colour_translation_table[byte2];
			if (GTIA_PRIOR & 1) {
				ANTIC_cl[C_PF0 | C_PM3] = ANTIC_cl[C_PF1 | C_PM3] = cword;
				ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF1 | C_PM23] = cword2;
			}
			if ((GTIA_PRIOR & 6) == 0) {
				if (GTIA_PRIOR & 9) {
					ANTIC_cl[C_PF2 | C_PM3] = ANTIC_cl[C_PF3 | C_PM3] = cword;
					ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF3 | C_PM23] = cword2;
				}
				else {
					ANTIC_cl[C_PF2 | C_PM3] = colour_translation_table[byte | GTIA_COLPF2];
					ANTIC_cl[C_PF3 | C_PM35] = ANTIC_cl[C_PF2 | C_PM35] = ANTIC_cl[C_PM35] = ANTIC_cl[C_PF3 | C_PM3] = colour_translation_table[byte | GTIA_COLPF3];
					ANTIC_cl[C_PF2 | C_PM23] = colour_translation_table[byte2 | GTIA_COLPF2];
					ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = colour_translation_table[byte2 | GTIA_COLPF3];
					ANTIC_cl[C_HI2 | C_PM3] = colour_translation_table[((byte | GTIA_COLPF2) & 0xf0) | (GTIA_COLPF1 & 0xf)];
					ANTIC_cl[C_HI2 | C_PM23] = colour_translation_table[((byte2 | GTIA_COLPF2) & 0xf0) | (GTIA_COLPF1 & 0xf)];
				}
			}
		}
		break;
#else /* USE_COLOUR_TRANSLATION_TABLE */
	case GTIA_OFFSET_COLBK:
		GTIA_COLBK = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_BAK] = cword;
		if (cword != (UWORD) (ANTIC_lookup_gtia9[0]) ) {
			ANTIC_lookup_gtia9[0] = cword + (cword << 16);
			if (GTIA_PRIOR & 0x40)
				setup_gtia9_11();
		}
		break;
	case GTIA_OFFSET_COLPF0:
		GTIA_COLPF0 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PF0] = cword;
		if ((GTIA_PRIOR & 1) == 0) {
			ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF0 | C_PM3] = ANTIC_cl[C_PF0 | C_PM2] = cword;
			if ((GTIA_PRIOR & 3) == 0) {
				if (GTIA_PRIOR & 0xf) {
					ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF0 | C_PM1] = ANTIC_cl[C_PF0 | C_PM0] = cword;
					if ((GTIA_PRIOR & 0xf) == 0xc)
						ANTIC_cl[C_PF0 | C_PM0123] = ANTIC_cl[C_PF0 | C_PM123] = ANTIC_cl[C_PF0 | C_PM023] = cword;
				}
				else
					ANTIC_cl[C_PF0 | C_PM01] = (ANTIC_cl[C_PF0 | C_PM0] = cword | ANTIC_cl[C_PM0]) | (ANTIC_cl[C_PF0 | C_PM1] = cword | ANTIC_cl[C_PM1]);
			}
			if ((GTIA_PRIOR & 0xf) >= 0xa)
				ANTIC_cl[C_PF0 | C_PM25] = cword;
		}
		break;
	case GTIA_OFFSET_COLPF1:
		GTIA_COLPF1 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PF1] = cword;
		if ((GTIA_PRIOR & 1) == 0) {
			ANTIC_cl[C_PF1 | C_PM23] = ANTIC_cl[C_PF1 | C_PM3] = ANTIC_cl[C_PF1 | C_PM2] = cword;
			if ((GTIA_PRIOR & 3) == 0) {
				if (GTIA_PRIOR & 0xf) {
					ANTIC_cl[C_PF1 | C_PM01] = ANTIC_cl[C_PF1 | C_PM1] = ANTIC_cl[C_PF1 | C_PM0] = cword;
					if ((GTIA_PRIOR & 0xf) == 0xc)
						ANTIC_cl[C_PF1 | C_PM0123] = ANTIC_cl[C_PF1 | C_PM123] = ANTIC_cl[C_PF1 | C_PM023] = cword;
				}
				else
					ANTIC_cl[C_PF1 | C_PM01] = (ANTIC_cl[C_PF1 | C_PM0] = cword | ANTIC_cl[C_PM0]) | (ANTIC_cl[C_PF1 | C_PM1] = cword | ANTIC_cl[C_PM1]);
			}
		}
		((UBYTE *)ANTIC_hires_lookup_l)[0x80] = ((UBYTE *)ANTIC_hires_lookup_l)[0x41] = (UBYTE)
			(ANTIC_hires_lookup_l[0x60] = cword & 0xf0f);
		break;
	case GTIA_OFFSET_COLPF2:
		GTIA_COLPF2 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PF2] = cword;
		if (GTIA_PRIOR & 4)
			ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF2 | C_PM1] = ANTIC_cl[C_PF2 | C_PM0] = cword;
		if ((GTIA_PRIOR & 9) == 0) {
			if (GTIA_PRIOR & 0xf)
				ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF2 | C_PM3] = ANTIC_cl[C_PF2 | C_PM2] = cword;
			else
				ANTIC_cl[C_PF2 | C_PM23] = (ANTIC_cl[C_PF2 | C_PM2] = cword | ANTIC_cl[C_PM2]) | (ANTIC_cl[C_PF2 | C_PM3] = cword | ANTIC_cl[C_PM3]);
		}
		break;
	case GTIA_OFFSET_COLPF3:
		GTIA_COLPF3 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PF3] = cword;
		if (GTIA_PRIOR & 4)
			ANTIC_cl[C_PF3 | C_PM01] = ANTIC_cl[C_PF3 | C_PM1] = ANTIC_cl[C_PF3 | C_PM0] = cword;
		if ((GTIA_PRIOR & 9) == 0) {
			if (GTIA_PRIOR & 0xf)
				ANTIC_cl[C_PF3 | C_PM23] = ANTIC_cl[C_PF3 | C_PM3] = ANTIC_cl[C_PF3 | C_PM2] = cword;
			else {
				ANTIC_cl[C_PF3 | C_PM25] = ANTIC_cl[C_PF2 | C_PM25] = ANTIC_cl[C_PM25] = ANTIC_cl[C_PF3 | C_PM2] = cword | ANTIC_cl[C_PM2];
				ANTIC_cl[C_PF3 | C_PM35] = ANTIC_cl[C_PF2 | C_PM35] = ANTIC_cl[C_PM35] = ANTIC_cl[C_PF3 | C_PM3] = cword | ANTIC_cl[C_PM3];
				ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = ANTIC_cl[C_PF3 | C_PM2] | ANTIC_cl[C_PF3 | C_PM3];
				ANTIC_cl[C_PF0 | C_PM235] = ANTIC_cl[C_PF0 | C_PM35] = ANTIC_cl[C_PF0 | C_PM25] =
				ANTIC_cl[C_PF1 | C_PM235] = ANTIC_cl[C_PF1 | C_PM35] = ANTIC_cl[C_PF1 | C_PM25] = cword;
			}
		}
		break;
	case GTIA_OFFSET_COLPM0:
		GTIA_COLPM0 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PM023] = ANTIC_cl[C_PM0] = cword;
		ANTIC_cl[C_PM0123] = ANTIC_cl[C_PM01] = cword2 = cword | ANTIC_cl[C_PM1];
		if ((GTIA_PRIOR & 4) == 0) {
			ANTIC_cl[C_PF2 | C_PM0] = ANTIC_cl[C_PF3 | C_PM0] = cword;
			ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF3 | C_PM01] = cword2;
			if ((GTIA_PRIOR & 0xc) == 0) {
				if (GTIA_PRIOR & 3) {
					ANTIC_cl[C_PF0 | C_PM0] = ANTIC_cl[C_PF1 | C_PM0] = cword;
					ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF1 | C_PM01] = cword2;
				}
				else {
					ANTIC_cl[C_PF0 | C_PM0] = cword | ANTIC_cl[C_PF0];
					ANTIC_cl[C_PF1 | C_PM0] = cword | ANTIC_cl[C_PF1];
					ANTIC_cl[C_PF0 | C_PM01] = cword2 | ANTIC_cl[C_PF0];
					ANTIC_cl[C_PF1 | C_PM01] = cword2 | ANTIC_cl[C_PF1];
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPM1:
		GTIA_COLPM1 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PM123] = ANTIC_cl[C_PM1] = cword;
		ANTIC_cl[C_PM0123] = ANTIC_cl[C_PM01] = cword2 = cword | ANTIC_cl[C_PM0];
		if ((GTIA_PRIOR & 4) == 0) {
			ANTIC_cl[C_PF2 | C_PM1] = ANTIC_cl[C_PF3 | C_PM1] = cword;
			ANTIC_cl[C_PF2 | C_PM01] = ANTIC_cl[C_PF3 | C_PM01] = cword2;
			if ((GTIA_PRIOR & 0xc) == 0) {
				if (GTIA_PRIOR & 3) {
					ANTIC_cl[C_PF0 | C_PM1] = ANTIC_cl[C_PF1 | C_PM1] = cword;
					ANTIC_cl[C_PF0 | C_PM01] = ANTIC_cl[C_PF1 | C_PM01] = cword2;
				}
				else {
					ANTIC_cl[C_PF0 | C_PM1] = cword | ANTIC_cl[C_PF0];
					ANTIC_cl[C_PF1 | C_PM1] = cword | ANTIC_cl[C_PF1];
					ANTIC_cl[C_PF0 | C_PM01] = cword2 | ANTIC_cl[C_PF0];
					ANTIC_cl[C_PF1 | C_PM01] = cword2 | ANTIC_cl[C_PF1];
				}
			}
		}
		break;
	case GTIA_OFFSET_COLPM2:
		GTIA_COLPM2 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PM2] = cword;
		ANTIC_cl[C_PM23] = cword2 = cword | ANTIC_cl[C_PM3];
		if (GTIA_PRIOR & 1) {
			ANTIC_cl[C_PF0 | C_PM2] = ANTIC_cl[C_PF1 | C_PM2] = cword;
			ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF1 | C_PM23] = cword2;
		}
		if ((GTIA_PRIOR & 6) == 0) {
			if (GTIA_PRIOR & 9) {
				ANTIC_cl[C_PF2 | C_PM2] = ANTIC_cl[C_PF3 | C_PM2] = cword;
				ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF3 | C_PM23] = cword2;
			}
			else {
				ANTIC_cl[C_PF2 | C_PM2] = cword | ANTIC_cl[C_PF2];
				ANTIC_cl[C_PF3 | C_PM25] = ANTIC_cl[C_PF2 | C_PM25] = ANTIC_cl[C_PM25] = ANTIC_cl[C_PF3 | C_PM2] = cword | ANTIC_cl[C_PF3];
				ANTIC_cl[C_PF2 | C_PM23] = cword2 | ANTIC_cl[C_PF2];
				ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = cword2 | ANTIC_cl[C_PF3];
			}
		}
		break;
	case GTIA_OFFSET_COLPM3:
		GTIA_COLPM3 = byte &= 0xfe;
		GTIA_COLOUR_TO_WORD(cword,byte);
		ANTIC_cl[C_PM3] = cword;
		ANTIC_cl[C_PM23] = cword2 = cword | ANTIC_cl[C_PM2];
		if (GTIA_PRIOR & 1) {
			ANTIC_cl[C_PF0 | C_PM3] = ANTIC_cl[C_PF1 | C_PM3] = cword;
			ANTIC_cl[C_PF0 | C_PM23] = ANTIC_cl[C_PF1 | C_PM23] = cword2;
		}
		if ((GTIA_PRIOR & 6) == 0) {
			if (GTIA_PRIOR & 9) {
				ANTIC_cl[C_PF2 | C_PM3] = ANTIC_cl[C_PF3 | C_PM3] = cword;
				ANTIC_cl[C_PF2 | C_PM23] = ANTIC_cl[C_PF3 | C_PM23] = cword2;
			}
			else {
				ANTIC_cl[C_PF2 | C_PM3] = cword | ANTIC_cl[C_PF2];
				ANTIC_cl[C_PF3 | C_PM35] = ANTIC_cl[C_PF2 | C_PM35] = ANTIC_cl[C_PM35] = ANTIC_cl[C_PF3 | C_PM3] = cword | ANTIC_cl[C_PF3];
				ANTIC_cl[C_PF2 | C_PM23] = cword2 | ANTIC_cl[C_PF2];
				ANTIC_cl[C_PF3 | C_PM235] = ANTIC_cl[C_PF2 | C_PM235] = ANTIC_cl[C_PM235] = ANTIC_cl[C_PF3 | C_PM23] = cword2 | ANTIC_cl[C_PF3];
			}
		}
		break;
#endif /* USE_COLOUR_TRANSLATION_TABLE */
	case GTIA_OFFSET_GRAFM:
		GTIA_GRAFM = byte;
		UPDATE_PM_CYCLE_EXACT
		break;

#ifdef NEW_CYCLE_EXACT
#define CYCLE_EXACT_GRAFP(n) x = ANTIC_XPOS * 2 - 3;\
	if (GTIA_HPOSP##n >= x) {\
	/* hpos right of x */\
		/* redraw */  \
		UPDATE_PM_CYCLE_EXACT\
	}
#else
#define CYCLE_EXACT_GRAFP(n)
#endif /* NEW_CYCLE_EXACT */

#define DO_GRAFP(n) case GTIA_OFFSET_GRAFP##n:\
	GTIA_GRAFP##n = byte;\
	CYCLE_EXACT_GRAFP(n);\
	break;

	DO_GRAFP(0)
	DO_GRAFP(1)
	DO_GRAFP(2)
	DO_GRAFP(3)

	case GTIA_OFFSET_HITCLR:
		GTIA_M0PL = GTIA_M1PL = GTIA_M2PL = GTIA_M3PL = 0;
		GTIA_P0PL = GTIA_P1PL = GTIA_P2PL = GTIA_P3PL = 0;
		PF0PM = PF1PM = PF2PM = PF3PM = 0;
#ifdef NEW_CYCLE_EXACT
		hitclr_pos = ANTIC_XPOS * 2 - 37;
		collision_curpos = hitclr_pos;
#endif
		break;
/* TODO: cycle-exact missile HPOS, GRAF, SIZE */
/* this is only an approximation */
	case GTIA_OFFSET_HPOSM0:
		GTIA_HPOSM0 = byte;
		hposm_ptr[0] = GTIA_pm_scanline + byte - 0x20;
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_HPOSM1:
		GTIA_HPOSM1 = byte;
		hposm_ptr[1] = GTIA_pm_scanline + byte - 0x20;
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_HPOSM2:
		GTIA_HPOSM2 = byte;
		hposm_ptr[2] = GTIA_pm_scanline + byte - 0x20;
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_HPOSM3:
		GTIA_HPOSM3 = byte;
		hposm_ptr[3] = GTIA_pm_scanline + byte - 0x20;
		UPDATE_PM_CYCLE_EXACT
		break;

#ifdef NEW_CYCLE_EXACT
#define CYCLE_EXACT_HPOSP(n) x = ANTIC_XPOS * 2 - 1;\
	if (GTIA_HPOSP##n < x && byte < x) {\
	/* case 1: both left of x */\
		/* do nothing */\
	}\
	else if (GTIA_HPOSP##n >= x && byte >= x ) {\
	/* case 2: both right of x */\
		/* redraw, clearing first */\
		UPDATE_PM_CYCLE_EXACT\
	}\
	else if (GTIA_HPOSP##n <x && byte >= x) {\
	/* case 3: new value is right, old value is left */\
		/* redraw without clearing first */\
		/* note: a hack, we can get away with it unless another change occurs */\
		/* before the original copy that wasn't erased due to changing */\
		/* GTIA_pm_dirty is drawn */\
		GTIA_pm_dirty = FALSE;\
		UPDATE_PM_CYCLE_EXACT\
		GTIA_pm_dirty = TRUE; /* can't trust that it was reset correctly */\
	}\
	else {\
	/* case 4: new value is left, old value is right */\
		/* remove old player and don't draw the new one */\
		UBYTE save_graf = GTIA_GRAFP##n;\
		GTIA_GRAFP##n = 0;\
		UPDATE_PM_CYCLE_EXACT\
		GTIA_GRAFP##n = save_graf;\
	}
#else
#define CYCLE_EXACT_HPOSP(n)
#endif /* NEW_CYCLE_EXACT */
#define DO_HPOSP(n)	case GTIA_OFFSET_HPOSP##n:								\
	hposp_ptr[n] = GTIA_pm_scanline + byte - 0x20;					\
	if (byte >= 0x22) {											\
		if (byte > 0xbe) {										\
			if (byte >= 0xde)									\
				hposp_mask[n] = 0;								\
			else												\
				hposp_mask[n] = 0xffffffff >> (byte - 0xbe);	\
		}														\
		else													\
			hposp_mask[n] = 0xffffffff;							\
	}															\
	else if (byte > 2)											\
		hposp_mask[n] = 0xffffffff << (0x22 - byte);			\
	else														\
		hposp_mask[n] = 0;										\
	CYCLE_EXACT_HPOSP(n)\
	GTIA_HPOSP##n = byte;											\
	break;

	DO_HPOSP(0)
	DO_HPOSP(1)
	DO_HPOSP(2)
	DO_HPOSP(3)

/* TODO: cycle-exact size changes */
/* this is only an approximation */
	case GTIA_OFFSET_SIZEM:
		GTIA_SIZEM = byte;
		global_sizem[0] = PM_Width[byte & 0x03];
		global_sizem[1] = PM_Width[(byte & 0x0c) >> 2];
		global_sizem[2] = PM_Width[(byte & 0x30) >> 4];
		global_sizem[3] = PM_Width[(byte & 0xc0) >> 6];
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_SIZEP0:
		GTIA_SIZEP0 = byte;
		grafp_ptr[0] = grafp_lookup[byte & 3];
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_SIZEP1:
		GTIA_SIZEP1 = byte;
		grafp_ptr[1] = grafp_lookup[byte & 3];
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_SIZEP2:
		GTIA_SIZEP2 = byte;
		grafp_ptr[2] = grafp_lookup[byte & 3];
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_SIZEP3:
		GTIA_SIZEP3 = byte;
		grafp_ptr[3] = grafp_lookup[byte & 3];
		UPDATE_PM_CYCLE_EXACT
		break;
	case GTIA_OFFSET_PRIOR:
		ANTIC_SetPrior(byte);
		GTIA_PRIOR = byte;
		if (byte & 0x40)
			setup_gtia9_11();
		break;
	case GTIA_OFFSET_VDELAY:
		GTIA_VDELAY = byte;
		break;
	case GTIA_OFFSET_GRACTL:
		GTIA_GRACTL = byte;
		ANTIC_missile_gra_enabled = (byte & 0x01);
		ANTIC_player_gra_enabled = (byte & 0x02);
		ANTIC_player_flickering = ((ANTIC_player_dma_enabled | ANTIC_player_gra_enabled) == 0x02);
		ANTIC_missile_flickering = ((ANTIC_missile_dma_enabled | ANTIC_missile_gra_enabled) == 0x01);
		if ((byte & 4) == 0)
			GTIA_TRIG_latch[0] = GTIA_TRIG_latch[1] = GTIA_TRIG_latch[2] = GTIA_TRIG_latch[3] = 1;
		break;

#endif /* defined(BASIC) || defined(CURSES_BASIC) */
	}
}

/* State ------------------------------------------------------------------- */

#ifndef BASIC

void GTIA_StateSave(void)
{
	int next_console_value = 7;

	STATESAV_TAG(gtia);
	StateSav_SaveUBYTE(&GTIA_HPOSP0, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSP1, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSP2, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSP3, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSM0, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSM1, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSM2, 1);
	StateSav_SaveUBYTE(&GTIA_HPOSM3, 1);
	StateSav_SaveUBYTE(&PF0PM, 1);
	StateSav_SaveUBYTE(&PF1PM, 1);
	StateSav_SaveUBYTE(&PF2PM, 1);
	StateSav_SaveUBYTE(&PF3PM, 1);
	StateSav_SaveUBYTE(&GTIA_M0PL, 1);
	StateSav_SaveUBYTE(&GTIA_M1PL, 1);
	StateSav_SaveUBYTE(&GTIA_M2PL, 1);
	StateSav_SaveUBYTE(&GTIA_M3PL, 1);
	StateSav_SaveUBYTE(&GTIA_P0PL, 1);
	StateSav_SaveUBYTE(&GTIA_P1PL, 1);
	StateSav_SaveUBYTE(&GTIA_P2PL, 1);
	StateSav_SaveUBYTE(&GTIA_P3PL, 1);
	StateSav_SaveUBYTE(&GTIA_SIZEP0, 1);
	StateSav_SaveUBYTE(&GTIA_SIZEP1, 1);
	StateSav_SaveUBYTE(&GTIA_SIZEP2, 1);
	StateSav_SaveUBYTE(&GTIA_SIZEP3, 1);
	StateSav_SaveUBYTE(&GTIA_SIZEM, 1);
	StateSav_SaveUBYTE(&GTIA_GRAFP0, 1);
	StateSav_SaveUBYTE(&GTIA_GRAFP1, 1);
	StateSav_SaveUBYTE(&GTIA_GRAFP2, 1);
	StateSav_SaveUBYTE(&GTIA_GRAFP3, 1);
	StateSav_SaveUBYTE(&GTIA_GRAFM, 1);
	StateSav_SaveUBYTE(&GTIA_COLPM0, 1);
	StateSav_SaveUBYTE(&GTIA_COLPM1, 1);
	StateSav_SaveUBYTE(&GTIA_COLPM2, 1);
	StateSav_SaveUBYTE(&GTIA_COLPM3, 1);
	StateSav_SaveUBYTE(&GTIA_COLPF0, 1);
	StateSav_SaveUBYTE(&GTIA_COLPF1, 1);
	StateSav_SaveUBYTE(&GTIA_COLPF2, 1);
	StateSav_SaveUBYTE(&GTIA_COLPF3, 1);
	StateSav_SaveUBYTE(&GTIA_COLBK, 1);
	StateSav_SaveUBYTE(&GTIA_PRIOR, 1);
	StateSav_SaveUBYTE(&GTIA_VDELAY, 1);
	StateSav_SaveUBYTE(&GTIA_GRACTL, 1);

	StateSav_SaveUBYTE(&consol_mask, 1);
	StateSav_SaveINT(&GTIA_speaker, 1);
	StateSav_SaveINT(&next_console_value, 1);
	StateSav_SaveUBYTE(GTIA_TRIG_latch, 4);
}

void GTIA_StateRead(UBYTE version)
{
	int next_console_value;	/* ignored */

	StateSav_ReadUBYTE(&GTIA_HPOSP0, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSP1, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSP2, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSP3, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSM0, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSM1, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSM2, 1);
	StateSav_ReadUBYTE(&GTIA_HPOSM3, 1);
	StateSav_ReadUBYTE(&PF0PM, 1);
	StateSav_ReadUBYTE(&PF1PM, 1);
	StateSav_ReadUBYTE(&PF2PM, 1);
	StateSav_ReadUBYTE(&PF3PM, 1);
	StateSav_ReadUBYTE(&GTIA_M0PL, 1);
	StateSav_ReadUBYTE(&GTIA_M1PL, 1);
	StateSav_ReadUBYTE(&GTIA_M2PL, 1);
	StateSav_ReadUBYTE(&GTIA_M3PL, 1);
	StateSav_ReadUBYTE(&GTIA_P0PL, 1);
	StateSav_ReadUBYTE(&GTIA_P1PL, 1);
	StateSav_ReadUBYTE(&GTIA_P2PL, 1);
	StateSav_ReadUBYTE(&GTIA_P3PL, 1);
	StateSav_ReadUBYTE(&GTIA_SIZEP0, 1);
	StateSav_ReadUBYTE(&GTIA_SIZEP1, 1);
	StateSav_ReadUBYTE(&GTIA_SIZEP2, 1);
	StateSav_ReadUBYTE(&GTIA_SIZEP3, 1);
	StateSav_ReadUBYTE(&GTIA_SIZEM, 1);
	StateSav_ReadUBYTE(&GTIA_GRAFP0, 1);
	StateSav_ReadUBYTE(&GTIA_GRAFP1, 1);
	StateSav_ReadUBYTE(&GTIA_GRAFP2, 1);
	StateSav_ReadUBYTE(&GTIA_GRAFP3, 1);
	StateSav_ReadUBYTE(&GTIA_GRAFM, 1);
	StateSav_ReadUBYTE(&GTIA_COLPM0, 1);
	StateSav_ReadUBYTE(&GTIA_COLPM1, 1);
	StateSav_ReadUBYTE(&GTIA_COLPM2, 1);
	StateSav_ReadUBYTE(&GTIA_COLPM3, 1);
	StateSav_ReadUBYTE(&GTIA_COLPF0, 1);
	StateSav_ReadUBYTE(&GTIA_COLPF1, 1);
	StateSav_ReadUBYTE(&GTIA_COLPF2, 1);
	StateSav_ReadUBYTE(&GTIA_COLPF3, 1);
	StateSav_ReadUBYTE(&GTIA_COLBK, 1);
	StateSav_ReadUBYTE(&GTIA_PRIOR, 1);
	StateSav_ReadUBYTE(&GTIA_VDELAY, 1);
	StateSav_ReadUBYTE(&GTIA_GRACTL, 1);

	StateSav_ReadUBYTE(&consol_mask, 1);
	StateSav_ReadINT(&GTIA_speaker, 1);
	StateSav_ReadINT(&next_console_value, 1);
	if (version >= 7)
		StateSav_ReadUBYTE(GTIA_TRIG_latch, 4);

	GTIA_PutByte(GTIA_OFFSET_HPOSP0, GTIA_HPOSP0);
	GTIA_PutByte(GTIA_OFFSET_HPOSP1, GTIA_HPOSP1);
	GTIA_PutByte(GTIA_OFFSET_HPOSP2, GTIA_HPOSP2);
	GTIA_PutByte(GTIA_OFFSET_HPOSP3, GTIA_HPOSP3);
	GTIA_PutByte(GTIA_OFFSET_HPOSM0, GTIA_HPOSM0);
	GTIA_PutByte(GTIA_OFFSET_HPOSM1, GTIA_HPOSM1);
	GTIA_PutByte(GTIA_OFFSET_HPOSM2, GTIA_HPOSM2);
	GTIA_PutByte(GTIA_OFFSET_HPOSM3, GTIA_HPOSM3);
	GTIA_PutByte(GTIA_OFFSET_SIZEP0, GTIA_SIZEP0);
	GTIA_PutByte(GTIA_OFFSET_SIZEP1, GTIA_SIZEP1);
	GTIA_PutByte(GTIA_OFFSET_SIZEP2, GTIA_SIZEP2);
	GTIA_PutByte(GTIA_OFFSET_SIZEP3, GTIA_SIZEP3);
	GTIA_PutByte(GTIA_OFFSET_SIZEM, GTIA_SIZEM);
	GTIA_PutByte(GTIA_OFFSET_GRAFP0, GTIA_GRAFP0);
	GTIA_PutByte(GTIA_OFFSET_GRAFP1, GTIA_GRAFP1);
	GTIA_PutByte(GTIA_OFFSET_GRAFP2, GTIA_GRAFP2);
	GTIA_PutByte(GTIA_OFFSET_GRAFP3, GTIA_GRAFP3);
	GTIA_PutByte(GTIA_OFFSET_GRAFM, GTIA_GRAFM);
	GTIA_PutByte(GTIA_OFFSET_COLPM0, GTIA_COLPM0);
	GTIA_PutByte(GTIA_OFFSET_COLPM1, GTIA_COLPM1);
	GTIA_PutByte(GTIA_OFFSET_COLPM2, GTIA_COLPM2);
	GTIA_PutByte(GTIA_OFFSET_COLPM3, GTIA_COLPM3);
	GTIA_PutByte(GTIA_OFFSET_COLPF0, GTIA_COLPF0);
	GTIA_PutByte(GTIA_OFFSET_COLPF1, GTIA_COLPF1);
	GTIA_PutByte(GTIA_OFFSET_COLPF2, GTIA_COLPF2);
	GTIA_PutByte(GTIA_OFFSET_COLPF3, GTIA_COLPF3);
	GTIA_PutByte(GTIA_OFFSET_COLBK, GTIA_COLBK);
	GTIA_PutByte(GTIA_OFFSET_PRIOR, GTIA_PRIOR);
	GTIA_PutByte(GTIA_OFFSET_GRACTL, GTIA_GRACTL);
}

#endif /* BASIC */
