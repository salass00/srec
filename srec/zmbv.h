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

#ifndef ZMBV_H
#define ZMBV_H

#include "srec.h"
#include <exec/types.h>
#include <interfaces/graphics.h>
#include <interfaces/z.h>

struct zmbv_state;

typedef uint8 (*xor_block_func_t)(struct zmbv_state *state, uint8 *ras1, uint8 *ras2, uint32 blk_w, uint32 blk_h, uint32 bpr, uint8 **outp);

struct zmbv_state {
	uint8                 unaligned_mask_vector[16];
	uint32                width, height, fps;
	struct GraphicsIFace *igraphics;
	struct ZIFace        *iz;
	z_stream              zstream;
	uint32                pixfmt;
	uint8                 zmbv_fmt;
	struct BitMap        *srec_bm;
	struct BitMap        *prev_frame_bm;
	struct BitMap        *current_frame_bm;
	void                 *prev_frame;
	void                 *current_frame;
	uint32                frame_bpr;
	uint32                frame_bpp;
	uint32                block_info_size;
	uint32                max_block_data_size;
	uint32                max_frame_size;
	void                 *block_info_buffer;
	void                 *block_data_buffer;
	void                 *frame_buffer;
	uint32                keyframe_cnt;
	uint32                vector_unit;
	xor_block_func_t      xor_block_func;
};

struct zmbv_state *zmbv_init(const struct SRecArgs *args);
BOOL zmbv_set_source_bm(struct zmbv_state *state, struct BitMap *bm);
BOOL zmbv_encode(struct zmbv_state *state, void **framep, uint32 *framesizep, BOOL *keyframep);
void zmbv_end(struct zmbv_state *state);

uint8 zmbv_xor_block_altivec(struct zmbv_state *state, uint8 *ras1, uint8 *ras2, uint32 blk_w, uint32 blk_h, uint32 bpr, uint8 **outp);

#endif

