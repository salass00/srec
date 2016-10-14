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

#ifndef ZMBV_INTERNAL_H
#define ZMBV_INTERNAL_H

#ifndef ZMBV_H
#include "zmbv.h"
#endif

#ifndef INTERFACES_GRAPHICS_H
#include <interfaces/graphics.h>
#endif
#ifndef INTERFACES_Z_H
#include <interfaces/z.h>
#endif

typedef uint8 (*zmbv_xor_block_func_t)(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp);
typedef void (*zmbv_endian_convert_t)(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr);

struct zmbv_state {
	uint8                    unaligned_mask_vector[16];
	zmbv_xor_block_func_t    xor_block_func;
	zmbv_endian_convert_t    format_convert_func;
	uint32                   width, height, fps;
	struct GraphicsIFace    *igraphics;
	struct ZIFace           *iz;
	z_stream                 zstream;
	uint32                   pixfmt;
	BOOL                     convert;
	uint8                    zmbv_fmt;
	struct BitMap           *srec_bm;
	struct BitMap           *prev_frame_bm;
	struct BitMap           *current_frame_bm;
	void                    *prev_frame;
	void                    *current_frame;
	uint32                   frame_bpr;
	uint32                   frame_bpp;
	uint32                   block_info_size;
	uint32                   max_block_data_size;
	uint32                   max_frame_size;
	void                    *block_info_buffer;
	void                    *block_data_buffer;
	void                    *frame_buffer;
	uint32                   keyframe_cnt;
	uint32                   vector_unit;
	const struct SRecGlobal *global_data;
};

uint8 zmbv_xor_block_ppc(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp);
void zmbv_format_convert_ppc(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr);

uint8 zmbv_xor_block_altivec(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp);
void zmbv_format_convert_altivec(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr);

#endif

