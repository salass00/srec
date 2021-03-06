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

#include "zmbv_internal.h"

static inline uint32 zmbv_xor_row_ppc(const struct zmbv_state *state, uint8 *out,
	const uint8 *row1, const uint8 *row2, uint32 row_len)
{
	uint32 longs = row_len >> 2;
	uint32 remains = row_len & 3;
	uint32 result = 0;
	uint32 x, y, z;

	while (longs--) {
		x = *(const uint32 *)row1;
		y = *(const uint32 *)row2;
		z = x ^ y;
		*(uint32 *)out = z;
		result |= z;

		row1 += 4;
		row2 += 4;
		out  += 4;
	}

	if (remains) {
		uint32 mask;
		mask = -1UL << ((4 - remains) << 3);
		x = *(const uint32 *)row1;
		y = *(const uint32 *)row2;
		z = (x ^ y) & mask;
		*(uint32 *)out = z;
		result |= z;
	}

	return result;
}

#ifdef ENABLE_CLUT
uint8 zmbv_xor_palette_ppc(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst)
{
	return zmbv_xor_row_ppc(state, dst, src, dst, 3 * 256);
}
#endif

uint8 zmbv_xor_block_ppc(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp)
{
	uint8  *out = *outp;
	uint32  result = 0;
	uint32  i;

	for (i = 0; i != blk_h; i++) {
		result |= zmbv_xor_row_ppc(state, out, ras1, ras2, blk_w);
		ras1 += bpr;
		ras2 += bpr;
		out  += blk_w;
	}

	if (result) {
		*outp = out;
		return 1;
	} else {
		return 0;
	}
}

void zmbv_format_convert_ppc(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr)
{
	uint32 width = (packed_bpr + 3) >> 2;
	uint32 mod = (padded_bpr - packed_bpr) & ~3;
	uint32 i, j;

	switch (state->pixfmt) {
		case PIXF_A8R8G8B8: // ARGB32
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("rlwinm %0,%1,24,0,31\n\t"
					        "rlwimi %0,%1,8,8,15\n\t"
					        "rlwimi %0,%1,8,24,31"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		case PIXF_R8G8B8A8: // RGBA32
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("rlwinm %0,%1,16,0,31\n\t"
					        "rlwimi %0,%1,0,8,15\n\t"
					        "rlwimi %0,%1,0,24,31"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		case PIXF_A8B8G8R8: // ABGR32
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("rlwinm %0,%1,8,0,31"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		case PIXF_R5G6B5:   // RGB16
		case PIXF_R5G5B5:   // RGB15
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("rlwinm %0,%1,24,0,31\n\t"
					        "rlwimi %0,%1,8,0,7\n\t"
					        "rlwimi %0,%1,8,16,23"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		case PIXF_B5G6R5PC: // BGR16PC
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("mr     %0,%1\n\t"
					        "rlwimi %0,%1,5,3,7\n\t"
					        "rlwimi %0,%1,27,8,12\n\t"
					        "rlwimi %0,%1,5,19,23\n\t"
					        "rlwimi %0,%1,27,24,28"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		case PIXF_B5G5R5PC: // BGR15PC
			for (i = 0; i != height; i++) {
				for (j = 0; j != width; j++) {
					uint32 x = *(const uint32 *)src;
					__asm__("mr     %0,%1\n\t"
					        "rlwimi %0,%1,6,3,7\n\t"
					        "rlwimi %0,%1,26,9,13\n\t"
					        "rlwimi %0,%1,6,19,23\n\t"
					        "rlwimi %0,%1,26,25,29"
					        : "=&r" (x)
					        : "r" (x));
					*(uint32 *)dst = x;
					src += 4;
					dst += 4;
				}
				src += mod;
				dst += mod;
			}
			break;
		default:
			/* Do nothing */
			break;
	}
}

