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
#include <altivec.h>

typedef __vector uint8 vuint8;

static inline vuint8 zmbv_xor_row_altivec(const struct zmbv_state *state, uint8 *out,
	const uint8 *row1, const uint8 *row2, uint32 row_len)
{
	uint32 vectors = row_len >> 4;
	uint32 remains = row_len & 15;
	vuint8 result = vec_splat_u8(0);
	vuint8 x, y, z;

	while (vectors--) {
		x = vec_ld(0, row1);
		y = vec_ld(0, row2);
		z = vec_xor(x, y);
		vec_st(z, 0, out);
		result = vec_or(z, result);

		row1 += 16;
		row2 += 16;
		out  += 16;
	}

	if (remains) {
		vuint8 mask;
		mask = vec_ld(0, state->unaligned_mask_vector);
		x = vec_ld(0, row1);
		y = vec_ld(0, row2);
		z = vec_and(vec_xor(x, y), mask);
		vec_st(z, 0, out);
		result = vec_or(z, result);
	}

	return result;
}

static inline vuint8 zmbv_xor_row_altivec_unaligned(const struct zmbv_state *state, uint8 *out,
	const uint8 *row1, const uint8 *row2, uint32 row_len)
{
	uint32 vectors = row_len >> 4;
	uint32 remains = row_len & 15;
	vuint8 result = vec_splat_u8(0);
	vuint8 perm_vector;
	vuint8 store_mask;
	vuint8 low;
	vuint8 x, y, z;

	low = vec_ld(0, out);
	perm_vector = vec_lvsr(0, out);
	store_mask = vec_perm(vec_splat_u8(0), vec_splat_u8(-1), perm_vector);

	while (vectors--) {
		x = vec_ld(0, row1);
		y = vec_ld(0, row2);
		z = vec_xor(x, y);
		result = vec_or(z, result);

		z = vec_perm(z, z, perm_vector);
		low = vec_sel(z, low, store_mask);
		vec_st(low, 0, out);
		low = vec_sel(vec_splat_u8(0), z, store_mask);

		row1 += 16;
		row2 += 16;
		out  += 16;
	}

	if (remains) {
		vuint8 mask;
		mask = vec_ld(0, state->unaligned_mask_vector);
		x = vec_ld(0, row1);
		y = vec_ld(0, row2);
		z = vec_and(vec_xor(x, y), mask);
		result = vec_or(z, result);

		z = vec_perm(z, z, perm_vector);
		low = vec_sel(z, low, store_mask);
		vec_st(low, 0, out);
		low = vec_sel(vec_splat_u8(0), z, store_mask);

		out += 16;
	}

	vec_st(low, 0, out);

	return result;
}

#ifdef ENABLE_CLUT
uint8 zmbv_xor_palette_altivec(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst)
{
	vuint8 result;

	result = zmbv_xor_row_altivec(state, dst, src, dst, 3 * 256);

	return vec_all_eq(result, vec_splat_u8(0)) ? 0 : 1;
}
#endif

uint8 zmbv_xor_block_altivec(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp)
{
	vuint8  result = vec_splat_u8(0);
	uint8  *out = *outp;
	uint32  i;
	vuint8  x;

	for (i = 0; i != blk_h; i++) {
		if (((uint32)out & 15) == 0)
			x = zmbv_xor_row_altivec(state, out, ras1, ras2, blk_w);
		else
			x = zmbv_xor_row_altivec_unaligned(state, out, ras1, ras2, blk_w);

		result = vec_or(x, result);

		ras1 += bpr;
		ras2 += bpr;
		out  += blk_w;
	}

	if (vec_all_eq(result, vec_splat_u8(0)))
		return 0;

	*outp = out;
	return 1;
}

static const vuint8 endian_swap_32bit = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12 };
static const vuint8 endian_swap_16bit = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14 };
static const vuint8 rgba32_to_bgra32  = { 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15 };
static const vuint8 abgr32_to_bgra32  = { 1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12 };

void zmbv_format_convert_altivec(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr)
{
	vuint8 perm_vector;
	uint32 vectors;
	uint32 mod;
	uint32 i, j;
	vuint8 x, y;

	switch (state->pixfmt) {
		case PIXF_A8R8G8B8: // ARGB32
			perm_vector = endian_swap_32bit;
			break;
		case PIXF_R8G8B8A8: // RGBA32
			perm_vector = rgba32_to_bgra32;
			break;
		case PIXF_A8B8G8R8: // ABGR32
			perm_vector = abgr32_to_bgra32;
			break;
		case PIXF_R5G6B5:   // RGB16
		case PIXF_R5G5B5:   // RGB15
			perm_vector = endian_swap_16bit;
			break;
		default:
			/* Fall back on non-Altivec code for unsupported formats */
			zmbv_format_convert_ppc(state, src, dst, packed_bpr, height, padded_bpr);
			return;
	}

	vectors = (packed_bpr + 15) >> 4;
	mod = (padded_bpr - packed_bpr) & ~15;

	for (i = 0; i != height; i++) {
		if (vectors != 0) {
			for (j = 0; j != vectors; j++) {
				x = vec_ld(0, src);
				y = vec_perm(x, x, perm_vector);
				vec_st(y, 0, dst);

				src += 16;
				dst += 16;
			}
		}

		src += mod;
		dst += mod;
	}
}

