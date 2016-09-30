#include "zmbv.h"
#include <altivec.h>

typedef __vector uint8 vuint8;

#define PREFETCH 1

static inline vuint8 zmbv_xor_row_altivec(const struct zmbv_state *state, uint8 *out,
	const uint8 *row1, const uint8 *row2, uint32 row_len)
{
	uint32 vectors = row_len >> 4;
	uint32 remains = row_len & 15;
	vuint8 result = vec_splat_u8(0);

	while (vectors--) {
		vuint8 r1, r2, x;
		r1 = vec_ld(0, row1);
		r2 = vec_ld(0, row2);
		x = vec_xor(r1, r2);
		vec_st(x, 0, out);
		result = vec_or(x, result);

		row1 += 16;
		row2 += 16;
		out  += 16;
	}

	if (remains) {
		vuint8 mask, r1, r2, x;
		mask = vec_ld(0, state->unaligned_mask_vector);
		r1 = vec_ld(0, row1);
		r2 = vec_ld(0, row2);
		x = vec_and(vec_xor(r1, r2), mask);
		vec_st(x, 0, out);
		result = vec_or(x, result);
	}

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
	vuint8 result = vec_splat_u8(0);
	#ifdef PREFETCH
	uint32 prefetch;
	#endif
	uint8 *out = *outp;
	uint32 i;
	vuint8 x;

	#ifdef PREFETCH
	prefetch = get_prefetch_constant((blk_w + 15) >> 4, blk_h, bpr);
	vec_dst(ras1, prefetch, 0);
	vec_dst(ras2, prefetch, 1);
	#endif

	for (i = 0; i != blk_h; i++) {
		x = zmbv_xor_row_altivec(state, out, ras1, ras2, blk_w);
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

void zmbv_endian_convert_altivec(const struct zmbv_state *state,
	uint8 *ras, uint32 packed_bpr, uint32 height, uint32 padded_bpr)
{
	uint32 vectors = (packed_bpr + 15) >> 4;
	vuint8 perm_vector;
	#ifdef PREFETCH
	uint32 max_prefetch;
	uint32 prefetch;
	uint32 num_max, k;
	#endif
	uint8 *row;
	uint32 i, j;
	vuint8 x, y;

	switch (state->pixfmt) {
		case PIXF_A8R8G8B8:
			perm_vector = endian_swap_32bit;
			break;
		case PIXF_R5G6B5:
		case PIXF_R5G5B5:
			perm_vector = endian_swap_16bit;
			break;
		default:
			/* Do nothing */
			return;
	}

	#ifdef PREFETCH
	max_prefetch = get_prefetch_constant(32, 256, 512);
	prefetch     = get_prefetch_constant_simple(vectors);

	num_max = vectors / MAX_VECTORS;
	vectors = vectors % MAX_VECTORS;
	#endif

	for (i = 0; i != height; i++) {
		row = ras;

		#ifdef PREFETCH
		for (j = 0; j != num_max; j++) {
			vec_dstst(row, max_prefetch, 0);

			for (k = 0; k != MAX_VECTORS; k++) {
				x = vec_ld(0, row);
				y = vec_perm(x, x, perm_vector);
				vec_st(y, 0, row);

				row += 16;
			}
		}
		#endif

		if (vectors != 0) {
			#ifdef PREFETCH
			vec_dstst(row, prefetch, 0);
			#endif

			for (j = 0; j != vectors; j++) {
				x = vec_ld(0, row);
				y = vec_perm(x, x, perm_vector);
				vec_st(y, 0, row);

				row += 16;
			}
		}

		ras += padded_bpr;
	}

	#ifdef PREFETCH
	vec_dss(0);
	#endif
}

