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

#include "pointer.h"
#include "srec_internal.h"
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <interfaces/icon.h>
#include <string.h>
#include <math.h>

struct srec_pointer {
	const struct SRecGlobal *global_data;
	int32                    xoffs, yoffs;
	uint32                   width, height;
	struct BitMap           *user_bm;
	uint8                   *buffer;
	uint32                   bpr;
	struct BitMap           *vram_bm;
	float                    scaled_width, scaled_height;
};

void strip_info_extension(STRPTR name) {
	uint32 len = strlen(name);

	if (len >= 5 && IUtility->Stricmp(&name[len - 5], ".info") == 0)
		name[len - 5] = '\0';
}

struct srec_pointer *load_pointer(const struct SRecGlobal *gd, CONST_STRPTR name) {
	struct GraphicsIFace *IGraphics = gd->igraphics;
	struct IconIFace *IIcon = gd->iicon;
	struct DiskObject *icon;
	struct srec_pointer *sp = NULL;
	uint32 format;
	const uint8 *image;
	uint32 width, height;
	struct ColorRegister *palette;
	uint32 processed;
	CONST_STRPTR tt;
	APTR lock;

	icon = IIcon->GetIconTags(name,
		ICONGETA_FailIfUnavailable,  TRUE,
		ICONGETA_RemapIcon,          FALSE,
		ICONGETA_GenerateImageMasks, FALSE,
		TAG_END);
	if (icon == NULL)
		goto out;

	processed = IIcon->IconControl(icon,
		ICONCTRLA_GetImageDataFormat, &format,
		ICONCTRLA_GetImageData1,      &image,
		ICONCTRLA_GetWidth,           &width,
		ICONCTRLA_GetHeight,          &height,
		TAG_END);
	if (processed != 4)
		goto out;

	if (format == IDFMT_DIRECTMAPPED) {
		/* Do nothing */
	} else if (format == IDFMT_PALETTEMAPPED) {
		processed = IIcon->IconControl(icon,
			ICONCTRLA_GetPalette1, &palette,
			TAG_END);
		if (processed != 1)
			goto out;
	} else
		goto out;

	sp = IExec->AllocVecTags(sizeof(*sp),
		AVT_Type,           MEMF_PRIVATE,
		AVT_Lock,           TRUE,
		AVT_ClearWithValue, 0,
		TAG_END);
	if (sp == NULL)
		goto out;

	sp->global_data = gd;

	tt = IIcon->FindToolType(icon->do_ToolTypes, "XOFFSET");
	if (tt != NULL)
		IDOS->StrToLong(tt, &sp->xoffs);
	tt = IIcon->FindToolType(icon->do_ToolTypes, "YOFFSET");
	if (tt != NULL)
		IDOS->StrToLong(tt, &sp->yoffs);

	sp->width  = width;
	sp->height = height;

	sp->vram_bm = IGraphics->AllocBitMapTags(width, height, 32,
		BMATags_PixelFormat, PIXF_A8R8G8B8,
		TAG_END);
	if (sp->vram_bm == NULL)
		goto out;

	sp->user_bm = IGraphics->AllocBitMapTags(width, height, 32,
		BMATags_Friend,      sp->vram_bm,
		BMATags_UserPrivate, TRUE,
		TAG_END);
	if (sp->user_bm == NULL)
		goto out;

	lock = IGraphics->LockBitMapTags(sp->user_bm,
		LBM_BaseAddress, &sp->buffer,
		LBM_BytesPerRow, &sp->bpr,
		TAG_END);
	if (lock != NULL)
		IGraphics->UnlockBitMap(lock);

	if (format == IDFMT_DIRECTMAPPED) {
		uint8 *buffer;
		uint32 i, image_bpr, buffer_bpr;

		buffer     = sp->buffer;
		image_bpr  = width << 2;
		buffer_bpr = sp->bpr;

		for (i = 0; i != height; i++) {
			IExec->CopyMem(image, buffer, image_bpr);

			image  += image_bpr;
			buffer += buffer_bpr;
		}
	} else {
		uint8 *buffer;
		uint32 i, j, p, buffer_mod;

		buffer     = sp->buffer;
		buffer_mod = sp->bpr - (width << 2);

		for (i = 0; i != height; i++) {
			for (j = 0; j != width; j++) {
				p = *image++;
				*buffer++ = (p != 0) ? 0xff : 0x00;
				*buffer++ = palette[p].red;
				*buffer++ = palette[p].green;
				*buffer++ = palette[p].blue;
			}

			buffer += buffer_mod;
		}
	}

	IIcon->FreeDiskObject(icon);
	icon = NULL;

	IGraphics->BltBitMapTags(
		BLITA_Source, sp->user_bm,
		BLITA_Dest,   sp->vram_bm,
		BLITA_Width,  width,
		BLITA_Height, height,
		TAG_END);

	return sp;

out:

	if (sp != NULL)
		free_pointer(sp);

	if (icon != NULL)
		IIcon->FreeDiskObject(icon);

	return NULL;
}

void scale_pointer(struct srec_pointer *sp) {
	const struct SRecGlobal *gd = sp->global_data;

	sp->scaled_width  = roundf((float)sp->width  * gd->scale_x);
	sp->scaled_height = roundf((float)sp->height * gd->scale_y);
	if (sp->scaled_width < 1.0f)
		sp->scaled_width = 1.0f;
	if (sp->scaled_height < 1.0f)
		sp->scaled_height = 1.0f;
}

void render_pointer(struct srec_pointer *sp, int32 mouse_x, int32 mouse_y) {
	const struct SRecGlobal *gd = sp->global_data;
	struct GraphicsIFace *IGraphics = gd->igraphics;
	const struct SRecArgs *args = gd->args;
	int32 pointer_x, pointer_y;
	vertex_t vertex_array[6];
	struct VertexRectangle rect;
	uint32 comp_flags;

	pointer_x = mouse_x + sp->xoffs;
	pointer_y = mouse_y + sp->yoffs;

	rect.min.x = gd->scale_rect.min.x + roundf((float)pointer_x * gd->scale_x);
	rect.min.y = gd->scale_rect.min.y + roundf((float)pointer_y * gd->scale_y);
	rect.max.x = rect.min.x + sp->scaled_width;
	rect.max.y = rect.min.y + sp->scaled_height;

	rect.min.s = 0.0f;
	rect.min.t = 0.0f;
	rect.max.s = (float)sp->width;
	rect.max.t = (float)sp->height;

	if (rect.min.x < gd->scale_rect.min.x)
		rect.min.x = gd->scale_rect.min.x;
	if (rect.max.x > gd->scale_rect.max.x)
		rect.max.x = gd->scale_rect.max.x;
	if (rect.min.y < gd->scale_rect.min.y)
		rect.min.y = gd->scale_rect.min.y;
	if (rect.max.y > gd->scale_rect.max.y)
		rect.max.y = gd->scale_rect.max.y;

	if (pointer_x < 0)
		rect.min.s -= (float)pointer_x;
	if (pointer_x + sp->width > gd->disp_width)
		rect.max.s -= (float)(pointer_x + sp->width - gd->disp_width);
	if (pointer_y < 0)
		rect.min.t -= (float)pointer_y;
	if (pointer_y + sp->height > gd->disp_height)
		rect.max.t -= (float)(pointer_y + sp->height - gd->disp_height);

	init_vertex_array_from_rect(vertex_array, &rect);

	comp_flags = COMPFLAG_HardwareOnly | COMPFLAG_IgnoreDestAlpha;

	if (!args->no_filter)
		comp_flags |= COMPFLAG_SrcFilter;

	IGraphics->CompositeTags(COMPOSITE_Src_Over_Dest, sp->vram_bm, gd->bitmap,
		COMPTAG_Flags,        comp_flags,
		COMPTAG_VertexArray,  vertex_array,
		COMPTAG_VertexFormat, COMPVF_STW0_Present,
		COMPTAG_NumTriangles, 2,
		TAG_END);
}

void free_pointer(struct srec_pointer *sp) {
	if (sp != NULL) {
		const struct SRecGlobal *gd = sp->global_data;
		struct GraphicsIFace *IGraphics = gd->igraphics;

		if (sp->vram_bm != NULL)
			IGraphics->FreeBitMap(sp->vram_bm);

		if (sp->user_bm != NULL)
			IGraphics->FreeBitMap(sp->user_bm);

		IExec->FreeVec(sp);
	}
}


