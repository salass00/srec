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
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <interfaces/icon.h>
#include <string.h>

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
	uint32 processed;
	APTR lock;
	uint8 *buffer;
	uint32 image_bpr, buffer_bpr;
	uint32 i;

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

	if (format != IDFMT_DIRECTMAPPED)
		goto out;

	sp = IExec->AllocVecTags(sizeof(*sp),
		AVT_Type,           MEMF_PRIVATE,
		AVT_Lock,           TRUE,
		AVT_ClearWithValue, 0,
		TAG_END);
	if (sp == NULL)
		goto out;

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

	buffer     = sp->buffer;
	image_bpr  = width << 2;
	buffer_bpr = sp->bpr;

	for (i = 0; i != height; i++) {
		IExec->CopyMem(image, buffer, image_bpr);

		image  += image_bpr;
		buffer += buffer_bpr;
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
		free_pointer(gd, sp);

	if (icon != NULL)
		IIcon->FreeDiskObject(icon);

	return NULL;
}

void free_pointer(const struct SRecGlobal *gd, struct srec_pointer *sp) {
	struct GraphicsIFace *IGraphics = gd->igraphics;

	if (sp != NULL) {
		if (sp->vram_bm != NULL)
			IGraphics->FreeBitMap(sp->vram_bm);

		if (sp->user_bm != NULL)
			IGraphics->FreeBitMap(sp->user_bm);

		IExec->FreeVec(sp);
	}
}


