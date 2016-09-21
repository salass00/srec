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
#include <proto/graphics.h>
#include <proto/z.h>

#define BLKW 16
#define BLKH 16

struct zmbv_state {
	uint32                width, height, fps;
	struct GraphicsIFace *IGraphics;
	struct ZIFace        *IZ;
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
	uint32                inter_buffer_size;
	uint32                frame_buffer_size;
	void                 *inter_buffer;
	void                 *frame_buffer;
	uint32                keyframe_cnt;
};

static void zmbv_close_libs(struct zmbv_state *state) {
	if (state->IZ != NULL)
		CloseInterface((struct Interface *)state->IZ);

	if (state->IGraphics != NULL)
		CloseInterface((struct Interface *)state->IGraphics);
}

struct zmbv_state *zmbv_init(uint32 width, uint32 height, uint32 fps) {
	struct zmbv_state *state;

	state = IExec->AllocVecTags(sizeof(*state),
		AVT_Type,           MEMF_PRIVATE,
		AVT_Lock,           TRUE,
		AVT_ClearWithValue, 0,
		TAG_END);
	if (state == NULL)
		goto out;

	state->width  = width;
	state->height = height;
	state->fps    = fps;

	state->IGraphics = (struct GraphicsIFace *)OpenInterface("graphics.library", 54);
	state->IZ = (struct ZIFace *)OpenInterface("z.library", 53);
	if (state->IGraphics == NULL || state->IZ == NULL)
		goto out;

	if (state->IZ->DeflateInit(&state->zstream, Z_BEST_SPEED) != Z_OK)
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
	struct GraphicsIFace *IGraphics = state->IGraphics;

	if (state->inter_buffer != NULL) {
		IExec->FreeVec(state->inter_buffer);
		state->inter_buffer = NULL;
	}

	if (state->frame_buffer != NULL) {
		IExec->FreeVec(state->frame_buffer);
		state->frame_buffer = NULL;
	}

	if (state->prev_frame_bm != NULL) {
		IGraphics->FreeBitMap(state->prev_frame_bm);
		state->prev_frame_bm = NULL;
		state->prev_frame = NULL;
	}

	if (state->current_frame_bm != NULL) {
		IGraphics->FreeBitMap(state->current_frame_bm);
		state->current_frame_bm = NULL;
		state->current_frame = NULL;
	}

	state->srec_bm = NULL;
}

BOOL zmbv_set_source_bm(struct zmbv_state *state, struct BitMap *bm) {
	struct GraphicsIFace *IGraphics = state->IGraphics;
	uint32 pixfmt;
	uint32 depth;
	uint32 bm_size;
	uint32 num_blk_w, num_blk_h, num_blk, blk_siz;
	uint32 compare_bpr;
	APTR lock;

	zmbv_free_frame_data(state);

	state->keyframe_cnt = 0;

	pixfmt = IGraphics->GetBitMapAttr(bm, BMA_PIXELFORMAT);
	depth  = IGraphics->GetBitMapAttr(bm, BMA_DEPTH);
	switch (pixfmt) {
		case PIXF_A8R8G8B8:
		case PIXF_B8G8R8A8:
			state->zmbv_fmt = 8;
			break;
		case PIXF_R5G6B5:
		case PIXF_R5G6B5PC:
			state->zmbv_fmt = 6;
			break;
		case PIXF_R5G5B5:
		case PIXF_R5G5B5PC:
			state->zmbv_fmt = 5;
			break;
		default:
			IExec->DebugPrintF("unsupported pixel format: %lu!\n", pixfmt);
			state->zmbv_fmt = (uint8)-1;
			return FALSE;
	}

	state->pixfmt = pixfmt;

	state->current_frame_bm = IGraphics->AllocBitMapTags(state->width, state->height, depth,
		BMATags_Friend,      bm,
		BMATags_UserPrivate, TRUE,
		TAG_END);
	state->prev_frame_bm = IGraphics->AllocBitMapTags(state->width, state->height, depth,
		BMATags_Friend,      bm,
		BMATags_UserPrivate, TRUE,
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

	state->frame_bpp = IGraphics->GetBitMapAttr(bm, BMA_BYTESPERPIXEL);
	bm_size = state->width * state->height * state->frame_bpp;

	num_blk_w = (state->width  + BLKW - 1) / BLKW;
	num_blk_h = (state->height + BLKH - 1) / BLKH;
	num_blk = num_blk_w * num_blk_h;
	blk_siz = BLKH * BLKW * state->frame_bpp;

	state->block_info_size   = (num_blk * 2 + 3) & ~3;
	state->inter_buffer_size = state->block_info_size + (num_blk * blk_siz);
	state->frame_buffer_size = bm_size * 2;

	state->inter_buffer = IExec->AllocVecTags(state->inter_buffer_size,
		AVT_Type, MEMF_PRIVATE,
		AVT_Lock, TRUE,
		TAG_END);
	state->frame_buffer = IExec->AllocVecTags(state->frame_buffer_size,
		AVT_Type, MEMF_SHARED,
		AVT_Lock, TRUE,
		TAG_END);
	if (state->inter_buffer == NULL || state->frame_buffer == NULL)
		goto out;

	state->srec_bm = bm;

	return TRUE;

out:

	zmbv_free_frame_data(state);

	return FALSE;
}

static uint8 zmbv_xor_row(uint8 *out, uint8 *row1, uint8 *row2, uint32 row_len) {
	uint32 longs = row_len >> 2;
	uint32 bytes = row_len & 3;
	uint8 flag = 0;

	while (longs--) {
		if ((*(uint32 *)out = *(uint32 *)row1 ^ *(uint32 *)row2) != 0)
			flag = 1;
		row1 += 4;
		row2 += 4;
		out  += 4;
	}

	while (bytes--) {
		if ((*out++ = *row1++ ^ *row2++) != 0)
			flag = 1;
	}

	return flag;
}

static uint8 zmbv_xor_block(uint8 *ras1, uint8 *ras2, uint32 blk_w, uint32 blk_h, uint32 bpr, uint8 **outp) {
	uint8 *out = *outp;
	uint8 flag = 0;
	uint32 i;

	for (i = 0; i != blk_h; i++) {
		flag |= zmbv_xor_row(out, ras1, ras2, blk_w);

		ras1 += bpr;
		ras2 += bpr;
		out  += blk_w;
	}

	if (flag) *outp = out;

	return flag;
}

static void zmbv_endian_convert(uint32 pixfmt, void *data, uint32 byte_width, uint32 height, uint32 mod) {
	uint32 *ras   = data;
	uint32  width = (byte_width + 3) >> 2;
	uint32  i, j;

	switch (pixfmt) {
		case PIXF_A8R8G8B8:
			{
				for (i = 0; i != height; i++) {
					for (j = 0; j != width; j++) {
						uint32 x = *ras;
						__asm__("rlwinm %0,%1,24,0,31\n\t"
						        "rlwimi %0,%1,8,8,15\n\t"
						        "rlwimi %0,%1,8,24,31"
						        : "=&r" (x)
						        : "r" (x));
						*ras++ = x;
					}
					ras = (uint32 *)((uint8 *)ras + mod);
				}
			}
			break;
		case PIXF_R5G6B5:
		case PIXF_R5G5B5:
			{
				for (i = 0; i != height; i++) {
					for (j = 0; j != width; j++) {
						uint32 x = *ras;
						__asm__("rlwinm %0,%1,8,0,31\n\t"
						        "rlwimi %0,%1,16,8,15\n\t"
						        "rlwimi %0,%1,16,24,31"
						        : "=&r" (x)
						        : "r" (x));
						*ras++ = x;
					}
					ras = (uint32 *)((uint8 *)ras + mod);
				}
			}
			break;
	}
}

BOOL zmbv_encode(struct zmbv_state *state, void **framep, uint32 *framesizep, BOOL *keyframep) {
	struct GraphicsIFace *IGraphics = state->IGraphics;
	struct ZIFace *IZ = state->IZ;
	uint8 *out = state->frame_buffer;
	uint32 out_space = state->frame_buffer_size;

	if (state->srec_bm == NULL)
		return FALSE;

	IGraphics->BltBitMapTags(
		BLITA_Source, state->srec_bm,
		BLITA_Dest,   state->current_frame_bm,
		BLITA_Width,  state->width,
		BLITA_Height, state->height,
		TAG_END);

	if (state->keyframe_cnt == 0) {
		uint8 *ras        = state->current_frame;
		uint32 packed_bpr = state->width * state->frame_bpp;
		uint32 padded_bpr = state->frame_bpr;
		uint32 mod        = padded_bpr - packed_bpr;

		state->keyframe_cnt = state->fps * 5; // keyframe every 5 seconds

		out[0] = 1;               // intraframe
		out[1] = 0;               // major version
		out[2] = 1;               // minor version
		out[3] = 1;               // zlib compressed
		out[4] = state->zmbv_fmt; // video format
		out[5] = BLKW;
		out[6] = BLKH;
		out += 7;
		out_space -= 7;

		IZ->DeflateReset(&state->zstream);

		state->zstream.next_out = out;
		state->zstream.avail_out = out_space;
		state->zstream.total_out = 0;

		zmbv_endian_convert(state->pixfmt, state->current_frame, packed_bpr, state->height, mod);

		if (mod == 0) {
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

		zmbv_endian_convert(state->pixfmt, state->current_frame, packed_bpr, state->height, mod);

		*framep = state->frame_buffer;
		*framesizep = 7 + state->zstream.total_out;
		*keyframep = TRUE;
	} else {
		uint8 *current_ras = state->current_frame;
		uint8 *prev_ras    = state->prev_frame;
		uint8 *info        = state->inter_buffer;
		uint8 *dst_start   = info + state->block_info_size;
		uint8 *dst         = dst_start;
		uint32 num_blk_w   = state->width / BLKW;
		uint32 last_blk_w  = (state->width % BLKW) * state->frame_bpp;
		uint32 num_blk_h   = state->height / BLKH;
		uint32 last_blk_h  = state->height % BLKH;
		uint32 blk_w       = BLKW * state->frame_bpp;
		uint32 blk_h       = BLKH;
		uint32 bpr         = state->frame_bpr;
		uint32 blk_mod     = (bpr * BLKH) - (num_blk_w * blk_w);
		uint32 i, j;

		state->keyframe_cnt--;

		IUtility->ClearMem(info, state->block_info_size);

		for (i = 0; i != num_blk_h; i++) {
			for (j = 0; j != num_blk_w; j++) {
				*info = zmbv_xor_block(current_ras, prev_ras, blk_w, blk_h, bpr, &dst);
				info += 2;

				current_ras += blk_w;
				prev_ras    += blk_w;
			}
			if (last_blk_w) {
				*info = zmbv_xor_block(current_ras, prev_ras, last_blk_w, blk_h, bpr, &dst);
				info += 2;
			}

			current_ras += blk_mod;
			prev_ras    += blk_mod;
		}
		if (last_blk_h) {
			for (j = 0; j != num_blk_w; j++) {
				*info = zmbv_xor_block(current_ras, prev_ras, blk_w, last_blk_h, bpr, &dst);
				info += 2;

				current_ras += blk_w;
				prev_ras    += blk_w;
			}
			if (last_blk_w) {
				*info = zmbv_xor_block(current_ras, prev_ras, last_blk_w, last_blk_h, bpr, &dst);
				info += 2;
			}
		}

		out[0] = 0; // interframe
		out++;
		out_space--;

		state->zstream.next_out  = out;
		state->zstream.avail_out = out_space;
		state->zstream.total_out = 0;

		zmbv_endian_convert(state->pixfmt, dst_start, dst - dst_start, 1, 0);

		state->zstream.next_in  = state->inter_buffer;
		state->zstream.avail_in = dst - (uint8 *)state->inter_buffer;
		if (IZ->Deflate(&state->zstream, Z_SYNC_FLUSH) != Z_OK) {
			IExec->DebugPrintF("deflate failed!\n");
			return FALSE;
		}

		*framep = state->frame_buffer;
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

void zmbv_end(struct zmbv_state *state) {
	if (state != NULL) {
		state->IZ->DeflateEnd(&state->zstream);

		zmbv_free_frame_data(state);

		zmbv_close_libs(state);

		IExec->FreeVec(state);
	}
}

