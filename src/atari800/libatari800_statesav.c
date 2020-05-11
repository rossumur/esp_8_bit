/*
 * libatari800/video.c - Atari800 as a library - saving the emulator's state to a file
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2010 Atari800 development team (see DOC/CREDITS)
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

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "libatari800_statesav.h"
#include "libatari800_init.h"

UBYTE *LIBATARI800_StateSav_buffer = NULL;
statesav_tags_t *LIBATARI800_StateSav_tags = NULL;


void LIBATARI800_StateSave(UBYTE *buffer, statesav_tags_t *tags) {
    LIBATARI800_StateSav_buffer = buffer;
    LIBATARI800_StateSav_tags = tags;
	StateSav_SaveAtariState(NULL, NULL, 0);
}

void LIBATARI800_StateLoad(UBYTE *buffer) {
    LIBATARI800_StateSav_buffer = buffer;
	StateSav_ReadAtariState(NULL, NULL);
}
