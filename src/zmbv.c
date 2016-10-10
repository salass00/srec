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

#include "zmbv.h"
#include "interfaces.h"
#include <proto/exec.h>
#include <proto/utility.h>
#include <malloc.h>

/*
 * For an explanation of the Zip Movable Blocks Video encoding used here you
 * can go to the following link:
 * https://wiki.multimedia.cx/?title=DosBox_Capture_Codec
 */

#define BLKW 16
#define BLKH 16

static void zmbv_close_libs(struct zmbv_state *state) {
	if (state->iz != NULL)
		CloseInterface((struct Interface *)state->iz);
}

static inline uint32 zmbv_xor_row_generic(const struct zmbv_state *state, uint8 *out,
	const uint8 *row1, const uint8 *row2, uint32 row_len)
{
	uint32 longs = row_len >> 2;
	uint32 bytes = row_len & 3;
	uint32 result = 0;

	while (longs--) {
		result |= (*(uint32 *)out = *(uint32 *)row1 ^ *(uint32 *)row2);
		row1 += 4;
		row2 += 4;
		out  += 4;
	}

	while (bytes--) {
		result |= (*out = *row1 ^ *row2);
		out  += 1;
		row1 += 1;
		row2 += 1;
	}

	return result;
}

uint8 zmbv_xor_block_generic(const struct zmbv_state *state,
	const uint8 *ras1, const uint8 *ras2, uint32 blk_w, uint32 blk_h,
	uint32 bpr, uint8 **outp)
{
	uint8  *out = *outp;
	uint32  result = 0;
	uint32  i;

	for (i = 0; i != blk_h; i++) {
		result |= zmbv_xor_row_generic(state, out, ras1, ras2, blk_w);
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

void zmbv_format_convert_generic(const struct zmbv_state *state,
	uint8 *ras, uint32 packed_bpr, uint32 height, uint32 padded_bpr)
{
	uint32  width = (packed_bpr + 3) >> 2;
	uint32 *row;
	uint32  i, j;

	switch (state->pixfmt) {
		case PIXF_A8R8G8B8:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("rlwinm %0,%1,24,0,31\n\t"
					        "rlwimi %0,%1,8,8,15\n\t"
					        "rlwimi %0,%1,8,24,31"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		case PIXF_R8G8B8A8:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("rlwinm %0,%1,16,0,31\n\t"
					        "rlwimi %0,%1,0,8,15\n\t"
					        "rlwimi %0,%1,0,24,31"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		case PIXF_A8B8G8R8:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("rlwinm %0,%1,8,0,31"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		case PIXF_R5G6B5:
		case PIXF_R5G5B5:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("rlwinm %0,%1,8,0,31\n\t"
					        "rlwimi %0,%1,24,8,15\n\t"
					        "rlwimi %0,%1,24,24,31"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		case PIXF_B5G6R5PC:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("mr     %0,%1\n\t"
					        "rlwimi %0,%1,5,3,7\n\t"
					        "rlwimi %0,%1,27,8,12\n\t"
					        "rlwimi %0,%1,5,19,23\n\t"
					        "rlwimi %0,%1,27,24,28"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		case PIXF_B5G5R5PC:
			for (i = 0; i != height; i++) {
				row = (uint32 *)ras;
				for (j = 0; j != width; j++) {
					uint32 x = *row;
					__asm__("mr     %0,%1\n\t"
					        "rlwimi %0,%1,6,3,7\n\t"
					        "rlwimi %0,%1,26,9,13\n\t"
					        "rlwimi %0,%1,6,19,23\n\t"
					        "rlwimi %0,%1,26,25,29"
					        : "=&r" (x)
					        : "r" (x));
					*row++ = x;
				}
				ras += padded_bpr;
			}
			break;
		default:
			/* Do nothing */
			break;
	}
}

static void *zalloc(void *opaque, uint32 items, uint32 size) {
	return _malloc_r(opaque, items * size);
}

static void zfree(void *opaque, void *address) {
	_free_r(opaque, address);
}

struct zmbv_state *zmbv_init(const struct SRecGlobal *gd, const struct SRecArgs *args) {
	struct zmbv_state *state;

	state = IExec->AllocVecTags(sizeof(*state),
		AVT_Type,           MEMF_PRIVATE,
		AVT_Lock,           TRUE,
		AVT_ClearWithValue, 0,
		AVT_Alignment,      16,
		TAG_END);
	if (state == NULL)
		goto out;

	state->igraphics = gd->igraphics;

	state->width  = args->width;
	state->height = args->height;
	state->fps    = args->fps;

	state->vector_unit = VECTORTYPE_NONE;

	state->xor_block_func      = zmbv_xor_block_generic;
	state->format_convert_func = zmbv_format_convert_generic;

	if (!args->no_altivec) {
		IExec->GetCPUInfoTags(
			GCIT_VectorUnit, &state->vector_unit,
			TAG_END);

		if (state->vector_unit == VECTORTYPE_ALTIVEC) {
			IExec->DebugPrintF("altivec unit detected\n");
			state->xor_block_func = zmbv_xor_block_altivec;
		}
	}

	state->iz = (struct ZIFace *)OpenInterface("z.library", 53, "main", 1);
	if (state->iz == NULL)
		goto out;

	state->zstream.opaque = _REENT;
	state->zstream.zalloc = zalloc;
	state->zstream.zfree  = zfree;

	if (state->iz->DeflateInit(&state->zstream, Z_BEST_SPEED) != Z_OK)
		goto out;

	return state;

out:

	if (state != NULL) {
		zmbv_close_libs(state);

		IExec->FreeVec(state);
	}

	IExec->DebugPrintF("failed to setup codec state!\n");

	return NULL;
}

static void zmbv_free_frame_data(struct zmbv_state *state) {
	struct GraphicsIFace *IGraphics = state->igraphics;

	if (state->block_info_buffer != NULL) {
		IExec->FreeVec(state->block_info_buffer);
		state->block_info_buffer = NULL;
		state->block_data_buffer = NULL;
		state->frame_buffer      = NULL;
	}

	if (state->prev_frame_bm != NULL) {
		IGraphics->FreeBitMap(state->prev_frame_bm);
		state->prev_frame_bm = NULL;
		state->prev_frame    = NULL;
	}

	if (state->current_frame_bm != NULL) {
		IGraphics->FreeBitMap(state->current_frame_bm);
		state->current_frame_bm = NULL;
		state->current_frame    = NULL;
	}

	state->srec_bm = NULL;
}

BOOL zmbv_set_source_bm(struct zmbv_state *state, struct BitMap *bm) {
	struct GraphicsIFace *IGraphics = state->igraphics;
	uint32 pixfmt;
	uint32 bpp, padded_width, depth;
	uint32 bm_size;
	uint32 num_blk_w, num_blk_h, num_blk;
	uint32 compare_bpr;
	uint32 block_data_offset, frame_buffer_offset;
	APTR   lock;

	zmbv_free_frame_data(state);

	state->keyframe_cnt = 0;
	state->convert = FALSE;

	pixfmt = IGraphics->GetBitMapAttr(bm, BMA_PIXELFORMAT);
	switch (pixfmt) {
		case PIXF_A8R8G8B8:
		case PIXF_R8G8B8A8:
		case PIXF_A8B8G8R8:
			state->convert = TRUE;
		case PIXF_B8G8R8A8:
			state->zmbv_fmt = 8;
			break;
		case PIXF_R5G6B5:
		case PIXF_B5G6R5PC:
			state->convert = TRUE;
		case PIXF_R5G6B5PC:
			state->zmbv_fmt = 6;
			break;
		case PIXF_R5G5B5:
		case PIXF_B5G5R5PC:
			state->convert = TRUE;
		case PIXF_R5G5B5PC:
			state->zmbv_fmt = 5;
			break;
		default:
			IExec->DebugPrintF("unsupported pixel format: %lu!\n", pixfmt);
			state->zmbv_fmt = (uint8)-1;
			return FALSE;
	}

	bpp          = IGraphics->GetBitMapAttr(bm, BMA_BYTESPERPIXEL);
	padded_width = IGraphics->GetBitMapAttr(bm, BMA_WIDTH);
	depth        = IGraphics->GetBitMapAttr(bm, BMA_DEPTH);

	if (bpp != 2 && bpp != 4) {
		IExec->DebugPrintF("unsupported bytes per pixel: %lu!\n", bpp);
		return FALSE;
	}

	state->pixfmt    = pixfmt;
	state->frame_bpp = bpp;

	padded_width = ((padded_width * bpp + 15) & ~15) / bpp;

	state->current_frame_bm = IGraphics->AllocBitMapTags(padded_width, state->height, depth,
		BMATags_Friend,      bm,
		BMATags_UserPrivate, TRUE,
		BMATags_Alignment,   16,
		TAG_END);
	state->prev_frame_bm = IGraphics->AllocBitMapTags(padded_width, state->height, depth,
		BMATags_Friend,      bm,
		BMATags_UserPrivate, TRUE,
		BMATags_Alignment,   16,
		TAG_END);
	if (state->current_frame_bm == NULL || state->prev_frame_bm == NULL)
		goto out;

	lock = IGraphics->LockBitMapTags(state->current_frame_bm,
		LBM_BaseAddress, &state->current_frame,
		LBM_BytesPerRow, &state->frame_bpr,
		TAG_END);
	if (lock != NULL)
		IGraphics->UnlockBitMap(lock);

	lock = IGraphics->LockBitMapTags(state->prev_frame_bm,
		LBM_BaseAddress, &state->prev_frame,
		LBM_BytesPerRow, &compare_bpr,
		TAG_END);
	if (lock != NULL)
		IGraphics->UnlockBitMap(lock);

	if (state->frame_bpr != compare_bpr) {
		IExec->DebugPrintF("current and previous frame bitmaps have different bytes per row values!\n");
		goto out;
	}

	bm_size = (state->width * bpp) * state->height;

	num_blk_w = (state->width  + BLKW - 1) / BLKW;
	num_blk_h = (state->height + BLKH - 1) / BLKH;
	num_blk = num_blk_w * num_blk_h;

	state->block_info_size = (num_blk * 2 + 3) & ~3;
	state->max_block_data_size = bm_size;
	state->max_frame_size = 1 + state->iz->DeflateBound(&state->zstream, state->block_info_size + state->max_block_data_size);

	if (state->vector_unit == VECTORTYPE_ALTIVEC) {
		uint32 last_bytes = (state->width * bpp) & 15;
		uint32 i;

		for (i = 0; i != last_bytes; i++)
			state->unaligned_mask_vector[i] = 0xff;

		for (; i != 16; i++)
			state->unaligned_mask_vector[i] = 0x00;
	}

	block_data_offset = (state->block_info_size + 15) & ~15;
	frame_buffer_offset = block_data_offset + ((state->max_block_data_size + 15) & ~15);
	state->block_info_buffer = IExec->AllocVecTags(frame_buffer_offset + state->max_frame_size,
		AVT_Type,      MEMF_SHARED,
		AVT_Lock,      TRUE,
		AVT_Alignment, 16,
		TAG_END);
	if (state->block_info_buffer == NULL)
		goto out;

	state->block_data_buffer = state->block_info_buffer + block_data_offset;
	state->frame_buffer = state->block_data_buffer + frame_buffer_offset;

	state->srec_bm = bm;

	return TRUE;

out:

	zmbv_free_frame_data(state);

	return FALSE;
}

BOOL zmbv_encode(struct zmbv_state *state, void **framep, uint32 *framesizep,
	BOOL *keyframep)
{
	struct GraphicsIFace *IGraphics = state->igraphics;
	struct ZIFace *IZ = state->iz;
	uint8  *out       = state->frame_buffer;
	uint32  out_space = state->max_frame_size;

	if (state->srec_bm == NULL)
		return FALSE;

	if (MAX_VRAM_TO_RAM_TRANSFER_SIZE == 0 ||
	    MAX_VRAM_TO_RAM_TRANSFER_SIZE >= (state->frame_bpr * state->height))
	{
		IGraphics->BltBitMapTags(
			BLITA_Source, state->srec_bm,
			BLITA_Dest,   state->current_frame_bm,
			BLITA_Width,  state->width,
			BLITA_Height, state->height,
			TAG_END);
	} else {
		uint32 max_rows  = MAX_VRAM_TO_RAM_TRANSFER_SIZE / state->frame_bpr;
		uint32 width     = state->width;
		uint32 rows_left = state->height;
		uint32 y         = 0;

		if (max_rows == 0)
			max_rows = 1;

		while (rows_left > max_rows) {
			IGraphics->BltBitMapTags(
				BLITA_Source, state->srec_bm,
				BLITA_Dest,   state->current_frame_bm,
				BLITA_SrcY,   y,
				BLITA_DestY,  y,
				BLITA_Width,  width,
				BLITA_Height, max_rows,
				TAG_END);

			y += max_rows;
			rows_left -= max_rows;
		}

		IGraphics->BltBitMapTags(
			BLITA_Source, state->srec_bm,
			BLITA_Dest,   state->current_frame_bm,
			BLITA_SrcY,   y,
			BLITA_DestY,  y,
			BLITA_Width,  width,
			BLITA_Height, rows_left,
			TAG_END);
	}

	if (state->keyframe_cnt == 0) {
		uint8  *ras        = state->current_frame;
		uint32  packed_bpr = state->width * state->frame_bpp;
		uint32  padded_bpr = state->frame_bpr;

		state->keyframe_cnt = state->fps * 5; // keyframe every 5 seconds

		out[0] = 1;               // intraframe
		out[1] = 0;               // major version
		out[2] = 1;               // minor version
		out[3] = 1;               // zlib compressed
		out[4] = state->zmbv_fmt; // video format
		out[5] = BLKW;
		out[6] = BLKH;

		IZ->DeflateReset(&state->zstream);

		state->zstream.next_out = out + 7;
		state->zstream.avail_out = out_space - 7;
		state->zstream.total_out = 0;

		if (state->convert)
			state->format_convert_func(state, state->current_frame, packed_bpr, state->height, padded_bpr);

		if (packed_bpr == padded_bpr) {
			state->zstream.next_in  = ras;
			state->zstream.avail_in = packed_bpr * state->height;

			if (IZ->Deflate(&state->zstream, Z_SYNC_FLUSH) != Z_OK) {
				IExec->DebugPrintF("deflate failed!\n");
				return FALSE;
			}
		} else {
			uint32 last_row = state->height - 1;
			uint32 i;

			for (i = 0; i != last_row; i++) {
				state->zstream.next_in  = ras;
				state->zstream.avail_in = packed_bpr;

				if (IZ->Deflate(&state->zstream, Z_NO_FLUSH) != Z_OK) {
					IExec->DebugPrintF("deflate failed!\n");
					return FALSE;
				}

				ras += padded_bpr;
			}

			state->zstream.next_in  = ras;
			state->zstream.avail_in = packed_bpr;

			if (IZ->Deflate(&state->zstream, Z_SYNC_FLUSH) != Z_OK) {
				IExec->DebugPrintF("deflate failed!\n");
				return FALSE;
			}
		}

		if (state->convert)
			state->format_convert_func(state, state->current_frame, packed_bpr, state->height, padded_bpr);

		*framep = out;
		*framesizep = 7 + state->zstream.total_out;
		*keyframep = TRUE;
	} else {
		zmbv_xor_block_func_t xor_block_func = state->xor_block_func;
		uint8  *current_ras = state->current_frame;
		uint8  *prev_ras    = state->prev_frame;
		uint8  *info        = state->block_info_buffer;
		uint8  *data        = state->block_data_buffer;
		uint32  num_blk_w   = state->width / BLKW;
		uint32  last_blk_w  = (state->width % BLKW) * state->frame_bpp;
		uint32  num_blk_h   = state->height / BLKH;
		uint32  last_blk_h  = state->height % BLKH;
		uint32  blk_w       = BLKW * state->frame_bpp;
		uint32  blk_h       = BLKH;
		uint32  bpr         = state->frame_bpr;
		uint32  blk_mod     = (bpr * BLKH) - (num_blk_w * blk_w);
		uint32  block_data_len;
		uint32  i, j;

		state->keyframe_cnt--;

		IUtility->ClearMem(info, state->block_info_size);

		for (i = 0; i != num_blk_h; i++) {
			for (j = 0; j != num_blk_w; j++) {
				*info = xor_block_func(state, current_ras, prev_ras, blk_w, blk_h, bpr, &data);
				info += 2;

				current_ras += blk_w;
				prev_ras    += blk_w;
			}
			if (last_blk_w) {
				*info = xor_block_func(state, current_ras, prev_ras, last_blk_w, blk_h, bpr, &data);
				info += 2;
			}

			current_ras += blk_mod;
			prev_ras    += blk_mod;
		}
		if (last_blk_h) {
			for (j = 0; j != num_blk_w; j++) {
				*info = xor_block_func(state, current_ras, prev_ras, blk_w, last_blk_h, bpr, &data);
				info += 2;

				current_ras += blk_w;
				prev_ras    += blk_w;
			}
			if (last_blk_w) {
				*info = xor_block_func(state, current_ras, prev_ras, last_blk_w, last_blk_h, bpr, &data);
				info += 2;
			}
		}

		out[0] = 0; // interframe

		state->zstream.next_out  = out + 1;
		state->zstream.avail_out = out_space - 1;
		state->zstream.total_out = 0;

		state->zstream.next_in  = state->block_info_buffer;
		state->zstream.avail_in = state->block_info_size;
		if (IZ->Deflate(&state->zstream, Z_NO_FLUSH) != Z_OK) {
			IExec->DebugPrintF("deflate failed!\n");
			return FALSE;
		}

		block_data_len = data - (uint8 *)state->block_data_buffer;
		if (state->convert)
			state->format_convert_func(state, state->block_data_buffer, block_data_len, 1, 0);

		state->zstream.next_in  = state->block_data_buffer;
		state->zstream.avail_in = block_data_len;
		if (IZ->Deflate(&state->zstream, Z_SYNC_FLUSH) != Z_OK) {
			IExec->DebugPrintF("deflate failed!\n");
			return FALSE;
		}

		*framep = out;
		*framesizep = 1 + state->zstream.total_out;
		*keyframep = FALSE;
	}

	{
		/* Swap current and previous frames */
		struct BitMap *temp_bm;
		void *temp_frame;

		temp_bm = state->current_frame_bm;
		state->current_frame_bm = state->prev_frame_bm;
		state->prev_frame_bm = temp_bm;

		temp_frame = state->current_frame;
		state->current_frame = state->prev_frame;
		state->prev_frame = temp_frame;
	}

	return TRUE;
}

BOOL zmbv_encode_dup(struct zmbv_state *state, void **framep, uint32 *framesizep) {
	struct ZIFace *IZ = state->iz;
	uint8  *out       = state->frame_buffer;
	uint32  out_space = state->max_frame_size;
	uint8  *info      = state->block_info_buffer;
	uint32  info_size = state->block_info_size;

	IUtility->ClearMem(info, info_size);

	out[0] = 0; // interframe

	state->zstream.next_out  = out + 1;
	state->zstream.avail_out = out_space - 1;
	state->zstream.total_out = 0;

	state->zstream.next_in  = info;
	state->zstream.avail_in = info_size;
	if (IZ->Deflate(&state->zstream, Z_SYNC_FLUSH) != Z_OK) {
		IExec->DebugPrintF("deflate failed!\n");
		return FALSE;
	}

	*framep = out;
	*framesizep = 1 + state->zstream.total_out;

	return TRUE;
}

void zmbv_end(struct zmbv_state *state) {
	if (state != NULL) {
		state->iz->DeflateEnd(&state->zstream);

		zmbv_free_frame_data(state);

		zmbv_close_libs(state);

		IExec->FreeVec(state);
	}
}

