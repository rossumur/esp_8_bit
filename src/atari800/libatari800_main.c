/*
 * libatari800/main.c - Atari800 as a library - main interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2019 Rob McMullen
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
#include <stdio.h>
#include <string.h>

/* Atari800 includes */
#include "atari.h"
#include "akey.h"
#include "afile.h"
#include "input.h"
#include "log.h"
#include "antic.h"
#include "cpu.h"
#include "platform.h"
#include "memory.h"
#include "screen.h"
#ifdef SOUND
#include "sound.h"
#endif
#include "util.h"
//#include "videomode.h"
#include "sio.h"
#include "cartridge.h"
//#include "ui.h"
#include "libatari800_main.h"
#include "libatari800_init.h"
#include "libatari800_input.h"
#include "libatari800_video.h"
#include "libatari800_statesav.h"

/* mainloop includes */
#include "antic.h"
#include "devices.h"
#include "gtia.h"
#include "pokey.h"
#ifdef PBI_BB
#include "pbi_bb.h"
#endif
#if defined(PBI_XLD) || defined (VOICEBOX)
#include "votraxsnd.h"
#endif

extern int debug_sound;

int PLATFORM_Configure(char *option, char *parameters)
{
	return TRUE;
}

void PLATFORM_ConfigSave(FILE *fp)
{
	;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	debug_sound = FALSE;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-help") == 0) {
			help_only = TRUE;
		}
		if (strcmp(argv[i], "-libatari800-debug-sound") == 0) {
			debug_sound = TRUE;
		}
		argv[j++] = argv[i];
	}
	*argc = j;

	if (!help_only) {
		if (!LIBATARI800_Initialise()) {
			return FALSE;
		}
	}

	if (!LIBATARI800_Video_Initialise(argc, argv)
#ifdef SOUND
		|| !Sound_Initialise(argc, argv)
#endif
		|| !LIBATARI800_Input_Initialise(argc, argv))
		return FALSE;

	/* turn off frame sync, return frames as fast as possible and let whatever
	 calls process_frame to manage syncing to NTSC or PAL */
	Atari800_turbo = TRUE;

	return TRUE;
}


void LIBATARI800_Frame(void)
{
	switch (INPUT_key_code) {
	case AKEY_COLDSTART:
		Atari800_Coldstart();
		break;
	case AKEY_WARMSTART:
		Atari800_Warmstart();
		break;
	case AKEY_UI:
		PLATFORM_Exit(TRUE);  /* run monitor */
		break;
	default:
		break;
	}

#ifdef PBI_BB
	PBI_BB_Frame(); /* just to make the menu key go up automatically */
#endif
#if defined(PBI_XLD) || defined (VOICEBOX)
	VOTRAXSND_Frame(); /* for the Votrax */
#endif
	Devices_Frame();
	INPUT_Frame();
	GTIA_Frame();
	ANTIC_Frame(TRUE);
	INPUT_DrawMousePointer();
	Screen_DrawAtariSpeed(Util_time());
	Screen_DrawDiskLED();
	Screen_Draw1200LED();
	POKEY_Frame();
#ifdef SOUND
	Sound_Update();
#endif
	Atari800_nframes++;
}


/* Stub routines to replace text-based UI */

int libatari800_error_code;

int UI_SelectCartType(int k) {
	libatari800_error_code = LIBATARI800_UNIDENTIFIED_CART_TYPE;
	return CARTRIDGE_NONE;
}

void UI_Run(void) {
	;
}
#define UI_MAX_DIRECTORIES 8

int UI_is_active;
int UI_alt_function;
int UI_current_function;
//char UI_atari_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
//char UI_saved_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
char UI_atari_files_dir[0][0];
char UI_saved_files_dir[0][0];

int UI_n_atari_files_dir;
int UI_n_saved_files_dir;
int UI_show_hidden_files = FALSE;

/* User visible routines */

int libatari800_init(int argc, char **argv) {
	CPU_cim_encountered = 0;
	libatari800_error_code = 0;
	Atari800_nframes = 0;
	MEMORY_selftest_enabled = 0;
	return Atari800_Initialise(&argc, argv);
}

char *error_messages[] = {
	"no error",
	"unidentified cartridge",
	"CPU crash",
	"BRK instruction",
	"invalid display list",
	"self test",
	"memo pad",
	"invalid escape opcode"
};
char *unknown_error = "unknown error";

char *libatari800_error_message() {
	if ((libatari800_error_code < 0) || (libatari800_error_code > (sizeof(error_messages)))) {
		return unknown_error;
	}
	return error_messages[libatari800_error_code];
}

void libatari800_clear_input_array(input_template_t *input)
{
	/* Initialize input and output arrays to zero */
	memset(input, 0, sizeof(input_template_t));
	INPUT_key_code = AKEY_NONE;
}

#ifdef HAVE_SETJMP
jmp_buf libatari800_cpu_crash;
#endif

int libatari800_next_frame(input_template_t *input)
{
	//LIBATARI800_Input_array = input; // not used
	INPUT_key_code = PLATFORM_Keyboard();
	LIBATARI800_Mouse();
#ifdef HAVE_SETJMP
	if ((libatari800_error_code = setjmp(libatari800_cpu_crash))) {
		/* called from within CPU_GO to indicate crash */
		Log_print("libatari800_next_frame: notified of CPU crash: %d\n", CPU_cim_encountered);
	}
	else
#endif /* HAVE_SETJMP */
	{
		/* normal operation */
		LIBATARI800_Frame();
		if (CPU_cim_encountered) {
			libatari800_error_code = LIBATARI800_CPU_CRASH;
		}
		else if (ANTIC_dlist == 0) {
			libatari800_error_code = LIBATARI800_DLIST_ERROR;
		}
	}
	PLATFORM_DisplayScreen();
	return !libatari800_error_code;
}

int libatari800_mount_disk_image(int diskno, const char *filename, int readonly)
{
	return SIO_Mount(diskno, filename, readonly);
}

int libatari800_reboot_with_file(const char *filename)
{
	int file_type;

	file_type = AFILE_OpenFile(filename, FALSE, 1, FALSE);
	if (file_type != AFILE_ERROR) {
		Atari800_Coldstart();
	}
	return file_type;
}

UBYTE *libatari800_get_main_memory_ptr()
{
	return MEMORY_mem;
}

UBYTE *libatari800_get_screen_ptr()
{
	return (UBYTE *)Screen_atari;
}

void libatari800_get_current_state(emulator_state_t *state)
{
	LIBATARI800_StateSave(state->state, &state->tags);
	state->flags.selftest_enabled = MEMORY_selftest_enabled;
}

void libatari800_restore_state(emulator_state_t *state)
{
	LIBATARI800_StateLoad(state->state);
}

/*
vim:ts=4:sw=4:
*/
