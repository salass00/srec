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

#ifndef SREC_INTERNAL_H
#define SREC_INTERNAL_H

#ifndef SREC_H
#include "srec.h"
#endif

#ifndef INTERFACES_INTUITION_H
#include <interfaces/intuition.h>
#endif
#ifndef INTERFACES_ICON_H
#include <interfaces/icon.h>
#endif
#ifndef INTERFACES_TIMER_H
#include <interfaces/timer.h>
#endif

typedef struct {
	float x, y;
	float s, t, w;
} vertex_t;

struct VertexRectangle {
	vertex_t min, max;
};

struct SRecGlobal {
	const struct SRecArgs *args;
	struct IntuitionIFace *iintuition;
	struct GraphicsIFace  *igraphics;
	struct IconIFace      *iicon;
	struct TimerIFace     *itimer;
	struct BitMap         *bitmap;
	uint32                 pixfmt;
	uint32                 disp_width, disp_height;
	float                  scale_x, scale_y;
	struct VertexRectangle scale_rect;
	struct Rect32          dest_rect;
};

void init_vertex_array_from_rect(vertex_t *vertex_array, const struct VertexRectangle *rect);
void get_frame_data(const struct SRecGlobal *gd, struct BitMap *dest_bm, uint32 width, uint32 height, uint32 bpr);

#endif

