/*
 * Copyright (C) 2016 Fredrik Wikstrom <fredrik@a500.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SREC_H
#define SREC_H

#include <exec/exec.h>

#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 480
#define DEFAULT_FPS    10

#define SREC_PROCNAME  "SRec: Screen Recorder"
#define SREC_PRIORITY  (0)
#define SREC_STACKSIZE (64 << 10)

struct SRecArgs {
	TEXT   filename[1024];
	uint32 width, height;
	uint32 fps;
	BOOL   filter;
};

int srec_entry(STRPTR argstring, int32 arglen, struct ExecBase *sysbase);

#endif

