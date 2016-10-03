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

#ifndef POINTER_H
#define POINTER_H

#include "srec.h"

struct srec_pointer {
	uint32         width, height;
	struct BitMap *user_bm;
	uint8         *buffer;
	uint32         bpr;
	struct BitMap *vram_bm;
};

void strip_info_extension(STRPTR name);
struct srec_pointer *load_pointer(const struct SRecGlobal *gd, CONST_STRPTR name);
void free_pointer(const struct SRecGlobal *gd, struct srec_pointer *sp);

#endif

