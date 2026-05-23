/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by Colin Phipps, Florian Schulze
 *
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  Misc system stuff needed by Doom, implemented for POSIX systems.
 *  Timers and signals.
 *
 *-----------------------------------------------------------------------------
 */

#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

#include "doomtype.h"
#include "m_fixed.h"
#include "i_system.h"
#include "doomdef.h"
#include "lprintf.h"
#include "d_main.h"
#include "d_event.h"
#include "global_data.h"
#include "tables.h"
#include "input.h"

#include "i_system.h"

#include "global_data.h"
#include "hid_ev.h"

#include "sys_config.h"
#include "typesdef.h"
#include "dev.h"
#include "hal/gpio.h"
#include "stdio.h"
#include "stdint.h"
#include "../buttons.h"

#define MODIFIER_PIN  PA_14   /* held = turn mode, released = walk mode */

/* cphipps - I_GetVersionString
 * Returns a version string in the given buffer
 */
const char *I_GetVersionString(char *buf, size_t sz)
{
	sprintf(buf, "PocketDoom v%s", VERSION);
	return buf;
}

#define MAX_MESSAGE_SIZE 1024

void I_Error(const char *error, ...)
{
	char msg[MAX_MESSAGE_SIZE];

	va_list v;
	va_start(v, error);

	vsprintf(msg, error, v);

	va_end(v);

	printf("%s", msg);

	fflush(stderr);
	fflush(stdout);

	// fgets(msg, sizeof(msg), stdin);

	I_Quit();
}


volatile int rotary_counter = 0;

void initTheCustomIO()
{
	gpio_set_mode(MODIFIER_PIN, GPIO_PULL_UP, GPIO_PULL_LEVEL_4_7K);
	gpio_set_dir(MODIFIER_PIN, GPIO_DIR_INPUT);
}
void I_ProcessKeyEvents()
{
	static btn_id_t prev_btn = BTN_NONE;
	static int prev_doom_key = 0;

	int modifier = (gpio_get_val(MODIFIER_PIN) == 0) ? 1 : 0;  /* PA_4 active low */
	btn_id_t btn = buttons_read();

	int doom_key = 0;
	if (btn == BTN_UP)    doom_key = modifier ? KEYD_RIGHT : KEYD_UP;
	if (btn == BTN_DOWN)  doom_key = modifier ? KEYD_LEFT  : KEYD_DOWN;
	if (btn == BTN_PRESS) doom_key = KEYD_A;

	event_t ev;
	if (prev_doom_key && prev_doom_key != doom_key) {
		ev.type = ev_keyup;
		ev.data1 = prev_doom_key;
		D_PostEvent(&ev);
	}
	if (doom_key && doom_key != prev_doom_key) {
		ev.type = ev_keydown;
		ev.data1 = doom_key;
		D_PostEvent(&ev);
	}

	prev_doom_key = doom_key;
	prev_btn = btn;
}
void I_Quit()
{
	while (1)
		;
}