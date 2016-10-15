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

#ifndef SCALE_H
#define SCALE_H

#ifndef SREC_H
#include "srec.h"
#endif

struct SRecGlobal;

void scale_bitmap(const struct SRecGlobal *gd, struct BitMap *src_bm, struct BitMap *dst_bm,
	uint32 src_x, uint32 src_y, uint32 src_w, uint32 src_h,
	uint32 dst_x, uint32 dst_y, uint32 dst_w, uint32 dst_h,
	float scale_x, float scale_y);

#endif

