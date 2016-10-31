/*
 * SRec - Screen Recorder
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "scale.h"
#include "srec_internal.h"
#include <sys/param.h>
#include <string.h>
#include <math.h>

#define CLAMP(val,min,max) MAX(MIN(val, max), min)

#ifdef ENABLE_CLUT
void scale_bitmap(const struct SRecGlobal *gd, struct BitMap *src_bm, struct BitMap *dst_bm,
	uint32 src_x, uint32 src_y, uint32 src_w, uint32 src_h,
	uint32 dst_x, uint32 dst_y, uint32 dst_w, uint32 dst_h)
{
	struct GraphicsIFace *IGraphics = gd->igraphics;
	float scale_x, scale_y;
	APTR src_lock, dst_lock;
	uint32 src_bpr, dst_bpr;
	uint32 bpp;
	const uint8 *src, *src_row;
	uint8 *dst;
	uint32 dst_mod;

	scale_x = (float)src_w / (float)dst_w;
	scale_y = (float)src_h / (float)dst_h;

	bpp = IGraphics->GetBitMapAttr(src_bm, BMA_BYTESPERPIXEL);

	src_lock = IGraphics->LockBitMapTags(src_bm,
		LBM_BaseAddress, &src,
		LBM_BytesPerRow, &src_bpr,
		TAG_END);
	dst_lock = IGraphics->LockBitMapTags(dst_bm,
		LBM_BaseAddress, &dst,
		LBM_BytesPerRow, &dst_bpr,
		TAG_END);

	src += (src_y * src_bpr) + (src_x * bpp);
	dst += (dst_y * dst_bpr) + (dst_x * bpp);

	dst_mod = dst_bpr - (dst_w * bpp);

	switch (bpp) {
		case 1:
			for (dst_y = 0; dst_y != dst_h; dst_y++) {
				src_y = CLAMP(lroundf((float)dst_y * scale_y), 0, src_h - 1);
				src_row = src + (src_y * src_bpr);
				for (dst_x = 0; dst_x != dst_w; dst_x++) {
					src_x = CLAMP(lroundf((float)dst_x * scale_x), 0, src_w - 1);
					*dst++ = *(src_row + src_x);
				}
				dst += dst_mod;
			}
			break;

		default:
			for (dst_y = 0; dst_y != dst_h; dst_y++) {
				src_y = CLAMP(lroundf((float)dst_y * scale_y), 0, src_h - 1);
				src_row = src + (src_y * src_bpr);
				for (dst_x = 0; dst_x != dst_w; dst_x++) {
					src_x = CLAMP(lroundf((float)dst_x * scale_x), 0, src_w - 1);
					memcpy(dst, src_row + (src_x * bpp), bpp);
					dst += bpp;
				}
				dst += dst_mod;
			}
			break;
	}

	if (src_lock != NULL)
		IGraphics->UnlockBitMap(src_lock);
	if (dst_lock != NULL)
		IGraphics->UnlockBitMap(dst_lock);
}
#endif

