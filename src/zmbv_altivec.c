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

#include "zmbv_internal.h"
#include <altivec.h>

typedef __vector uint8 vuint8;

#define PREFETCH 1

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

#ifdef PREFETCH
static inline uint32 get_prefetch_constant(uint32 block_size_in_vectors, uint32 block_count, uint32 block_stride) {
	return ((block_size_in_vectors << 24) & 0x1f000000) |
	       ((block_count << 16) & 0x00ff0000) |
	       (block_stride & 0xffff);
}
#endif

uint8 zmbv_xor_block_altivec(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp)
{
	vuint8  result = vec_splat_u8(0);
	#ifdef PREFETCH
	uint32  prefetch;
	#endif
	uint8  *out = *outp;
	uint32  i;
	vuint8  x;

	#ifdef PREFETCH
	prefetch = get_prefetch_constant((blk_w + 15) >> 4, blk_h, bpr);
	vec_dst(ras1, prefetch, 0);
	vec_dst(ras2, prefetch, 1);
	#endif

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

	#ifdef PREFETCH
	vec_dss(0);
	vec_dss(1);
	#endif

	if (!vec_all_eq(result, vec_splat_u8(0))) {
		*outp = out;
		return 1;
	} else {
		return 0;
	}
}

static const vuint8 endian_swap_32bit = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12 };
static const vuint8 endian_swap_16bit = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14 };
static const vuint8 rgba32_to_bgra32  = { 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15 };
static const vuint8 abgr32_to_bgra32  = { 1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12 };

#ifdef PREFETCH
#define MAX_VECTORS (32UL * 256UL)

static inline uint32 get_prefetch_constant_simple(uint32 total_vectors) {
	uint32 block_size_in_vectors = 1;
	uint32 block_count           = total_vectors;
	uint32 block_stride          = 16;

	while (block_count > 256) {
		block_size_in_vectors <<= 1;
		block_stride          <<= 1;
		block_count             = (block_count + 1) >> 1;
	}

	return get_prefetch_constant(block_size_in_vectors, block_count, block_stride);
}
#endif

void zmbv_format_convert_altivec(const struct zmbv_state *state,
	const uint8 *src, uint8 *dst, uint32 packed_bpr, uint32 height,
	uint32 padded_bpr)
{
	vuint8 perm_vector;
	uint32 vectors;
	uint32 mod;
	#ifdef PREFETCH
	uint32 max_prefetch;
	uint32 prefetch;
	uint32 num_max, k;
	#endif
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

	#ifdef PREFETCH
	max_prefetch = get_prefetch_constant(32, 256, 512);
	prefetch     = get_prefetch_constant_simple(vectors);

	num_max = vectors / MAX_VECTORS;
	vectors = vectors % MAX_VECTORS;
	#endif

	for (i = 0; i != height; i++) {
		#ifdef PREFETCH
		for (j = 0; j != num_max; j++) {
			if (src == dst)
				vec_dstst(src, max_prefetch, 0);
			else
				vec_dst(src, max_prefetch, 0);

			for (k = 0; k != MAX_VECTORS; k++) {
				x = vec_ld(0, src);
				y = vec_perm(x, x, perm_vector);
				vec_st(y, 0, dst);

				src += 16;
				dst += 16;
			}
		}
		#endif

		if (vectors != 0) {
			#ifdef PREFETCH
			if (src == dst)
				vec_dstst(src, prefetch, 0);
			else
				vec_dst(src, prefetch, 0);
			#endif

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

	#ifdef PREFETCH
	vec_dss(0);
	#endif
}

