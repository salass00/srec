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

#include "srec_internal.h"
#include "interfaces.h"
#include "pointer.h"
#include "timer.h"
#include "avilib.h"
#include "libmkv.h"
#include "zmbv.h"
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <sys/param.h>
#include <math.h>
#include "SRec_rev.h"

#define MAX_VRAM_TO_RAM_TRANSFER_SIZE (256UL << 10)

#define VLC_COMPAT 1

static int32 sighookfunc(struct Hook *hook, uint32 pid, struct Process *proc) {
	if (IDOS->GetPID(proc, GPID_PROCESS) == pid) {
		IExec->Signal(&proc->pr_Task, (uint32)hook->h_Data);
		return TRUE;
	}
	return FALSE;
}

BOOL safe_signal_proc(uint32 pid, uint32 sigmask) {
	struct Hook hook;

	if (pid == 0)
		return FALSE;

	IUtility->ClearMem(&hook, sizeof(hook));

	hook.h_Entry = (HOOKFUNC)sighookfunc;
	hook.h_Data  = (void *)sigmask;

	return IDOS->ProcessScan(&hook, (void *)pid, 0) ? TRUE : FALSE;
}

typedef struct {
	uint32_t biSize;
	int32_t  biWidth;
	int32_t  biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t  biXPelsPerMeter;
	int32_t  biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
} BITMAPINFOHEADER;

static inline uint16_t h2le16(uint16_t x) {
	return (x << 8) | (x >> 8);
}

static inline uint32_t h2le32(uint32_t x) {
	return (x << 24) | ((x & 0xff00UL) << 8) | ((x & 0xff0000UL) >> 8) | (x >> 24);
}

static void create_icon(const struct SRecGlobal *gd, CONST_STRPTR path) {
	struct IconIFace *IIcon = gd->iicon;
	const struct SRecArgs *args = gd->args;
	BOOL create_icon = FALSE;

	if (args->create_icon) {
		TEXT buffer[1024];

		BPTR lock;

		IUtility->SNPrintf(buffer, sizeof(buffer), "%s.info", path);

		lock = IDOS->LockTags(
			LK_Name,      buffer,
			LK_ResolveSL, FALSE,
			TAG_END);
		if (lock != ZERO)
			IDOS->UnLock(lock);

		/* Only create a new icon if one doesn't already exist */
		if (lock == ZERO && IDOS->IoErr() == ERROR_OBJECT_NOT_FOUND)
			create_icon = TRUE;
	}

	if (create_icon) {
		CONST_STRPTR container;
		struct DiskObject *icon;

		if (args->container == CONTAINER_AVI)
			container = "avi";
		else
			container = "mkv";

		icon = IIcon->GetIconTags(NULL,
			ICONGETA_GetDefaultName, container,
			ICONGETA_RemapIcon,      FALSE,
			TAG_END);

		if (icon == NULL) {
			icon = IIcon->GetIconTags(NULL,
				ICONGETA_GetDefaultName, "video",
				ICONGETA_GetDefaultType, WBPROJECT,
				ICONGETA_RemapIcon,      FALSE,
				TAG_END);
		}

		if (icon != NULL) {
			IIcon->PutIconTags(path, icon,
				ICONPUTA_NotifyWorkbench, TRUE,
				TAG_END);

			IIcon->FreeDiskObject(icon);
		}
	}
}

static int64 get_uptime_micros(const struct SRecGlobal *gd) {
	struct TimerIFace *ITimer = gd->itimer;
	struct TimeVal tv;

	ITimer->GetUpTime(&tv);

	return ((uint64)tv.Seconds * 1000000ULL) + (uint64)tv.Microseconds;
}

void get_screen_dimensions(struct GraphicsIFace *IGraphics,
	const struct Screen *screen, uint32 *widthp, uint32 *heightp)
{
	struct DimensionInfo diminfo;
	uint32 mode_id, info_len;
	uint32 width  = screen->Width;
	uint32 height = screen->Height;

	mode_id  = IGraphics->GetVPModeID(&screen->ViewPort);
	info_len = IGraphics->GetDisplayInfoData(NULL, &diminfo, sizeof(diminfo), DTAG_DIMS, mode_id);
	if (info_len >= offsetof(struct DimensionInfo, MaxOScan)) {
		width  = MIN(width,  (uint16)diminfo.Nominal.MaxX - (uint16)diminfo.Nominal.MinX + 1);
		height = MIN(height, (uint16)diminfo.Nominal.MaxY - (uint16)diminfo.Nominal.MinY + 1);
	}

	*widthp  = width;
	*heightp = height;
}

void init_vertex_array_from_rect(vertex_t *vertex_array, const struct VertexRectangle *rect) {
	vertex_array[0].x = rect->min.x;
	vertex_array[0].y = rect->min.y;
	vertex_array[0].s = rect->min.s;
	vertex_array[0].t = rect->min.t;
	vertex_array[0].w = 1.0f;

	vertex_array[1].x = rect->max.x;
	vertex_array[1].y = rect->min.y;
	vertex_array[1].s = rect->max.s;
	vertex_array[1].t = rect->min.t;
	vertex_array[1].w = 1.0f;

	vertex_array[2].x = rect->min.x;
	vertex_array[2].y = rect->max.y;
	vertex_array[2].s = rect->min.s;
	vertex_array[2].t = rect->max.t;
	vertex_array[2].w = 1.0f;

	vertex_array[3].x = rect->min.x;
	vertex_array[3].y = rect->max.y;
	vertex_array[3].s = rect->min.s;
	vertex_array[3].t = rect->max.t;
	vertex_array[3].w = 1.0f;

	vertex_array[4].x = rect->max.x;
	vertex_array[4].y = rect->min.y;
	vertex_array[4].s = rect->max.s;
	vertex_array[4].t = rect->min.t;
	vertex_array[4].w = 1.0f;

	vertex_array[5].x = rect->max.x;
	vertex_array[5].y = rect->max.y;
	vertex_array[5].s = rect->max.s;
	vertex_array[5].t = rect->max.t;
	vertex_array[5].w = 1.0f;
}

void get_frame_data(const struct SRecGlobal *gd, struct BitMap *dest_bm, uint32 width, uint32 height, uint32 bpr) {
	struct GraphicsIFace *IGraphics = gd->igraphics;
	struct BitMap *src_bm = gd->bitmap;

	if (MAX_VRAM_TO_RAM_TRANSFER_SIZE == 0 ||
	    MAX_VRAM_TO_RAM_TRANSFER_SIZE >= (bpr * height))
	{
		IGraphics->BltBitMapTags(
			BLITA_Source, src_bm,
			BLITA_Dest,   dest_bm,
			BLITA_Width,  width,
			BLITA_Height, height,
			TAG_END);
	} else {
		uint32 max_rows  = MAX_VRAM_TO_RAM_TRANSFER_SIZE / bpr;
		uint32 rows_left = height;
		uint32 y         = 0;

		if (max_rows == 0)
			max_rows = 1;

		while (rows_left > max_rows) {
			IGraphics->BltBitMapTags(
				BLITA_Source, src_bm,
				BLITA_Dest,   dest_bm,
				BLITA_SrcY,   y,
				BLITA_DestY,  y,
				BLITA_Width,  width,
				BLITA_Height, max_rows,
				TAG_END);

			y += max_rows;
			rows_left -= max_rows;
		}

		IGraphics->BltBitMapTags(
			BLITA_Source, src_bm,
			BLITA_Dest,   dest_bm,
			BLITA_SrcY,   y,
			BLITA_DestY,  y,
			BLITA_Width,  width,
			BLITA_Height, rows_left,
			TAG_END);
	}
}

int srec_entry(STRPTR argstring, int32 arglen, struct ExecBase *sysbase) {
	struct ExecIFace *IExec = (struct ExecIFace *)sysbase->MainInterface;
	struct Process *proc;
	struct SRecGlobal gd;
	struct zmbv_state *encoder = NULL;
	struct TimeRequest *tr = NULL;
	uint32 signals, timer_sig;
	BOOL timer_in_use = FALSE;
	struct Screen *current_screen = NULL;
	struct BitMap *screen_bitmap = NULL;
	vertex_t vertex_array[6];
	uint32 frames = 0, skipped = 0;
	uint32 duration_us;
	uint64 duration_ns;
	uint64 timestamp = 0;
	struct srec_pointer *pointer = NULL;
	struct srec_pointer *busy_pointer = NULL;
	int32 mouse_x = 0, mouse_y = 0;
	BOOL use_busy_pointer = FALSE;
	uint64 current_time, prev_time;
	int64 current_diff, total_diff;
	uint64 encode_time = 0;
	int rc = RETURN_ERROR;

	/* AVI output */
	avi_t *AVI = NULL;

	/* MKV output */
	mk_Writer *writer = NULL;
	BITMAPINFOHEADER bmih;
	mk_TrackConfig video_conf;
	mk_Track *video_track = NULL;

	IUtility->ClearMem(&gd, sizeof(gd));

	#define args        gd.args

	#define IIntuition  gd.iintuition
	#define IGraphics   gd.igraphics
	#define IIcon       gd.iicon
	#define ITimer      gd.itimer

	#define bitmap      gd.bitmap
	#define disp_width  gd.disp_width
	#define disp_height gd.disp_height
	#define scale_x     gd.scale_x
	#define scale_y     gd.scale_y
	#define min_x       gd.scale_rect.min.x
	#define min_y       gd.scale_rect.min.y
	#define max_x       gd.scale_rect.max.x
	#define max_y       gd.scale_rect.max.y
	#define min_s       gd.scale_rect.min.s
	#define min_t       gd.scale_rect.min.t
	#define max_s       gd.scale_rect.max.s
	#define max_t       gd.scale_rect.max.t

	proc = (struct Process *)IExec->FindTask(NULL);

	IExec->WaitPort(&proc->pr_MsgPort);
	args = (const struct SRecArgs *)IExec->GetMsg(&proc->pr_MsgPort);

	if (args->width < MIN_WIDTH || args->width > MAX_WIDTH)
		goto out;

	if (args->height < MIN_HEIGHT || args->height > MAX_HEIGHT)
		goto out;

	if (args->fps < MIN_FPS || args->fps > MAX_FPS)
		goto out;

	if (args->container != CONTAINER_AVI && args->container != CONTAINER_MKV)
		goto out;

	if (args->video_codec != VIDEO_CODEC_ZMBV || args->audio_codec != AUDIO_CODEC_NONE)
		goto out;

	IIntuition = (struct IntuitionIFace *)OpenInterface("intuition.library", 53, "main", 1);
	IGraphics  = (struct GraphicsIFace *)OpenInterface("graphics.library", 54, "main", 1);
	IIcon      = (struct IconIFace *)OpenInterface("icon.library", 53, "main", 1);
	if (IIntuition == NULL || IGraphics == NULL || IIcon == NULL)
		goto out;

	tr = OpenTimerDevice(UNIT_MICROHZ);
	if (tr == NULL)
		goto out;

	ITimer = (struct TimerIFace *)IExec->GetInterface((struct Library *)tr->Request.io_Device, "main", 1, NULL);
	if (ITimer == NULL)
		goto out;

	if (!args->no_pointer) {
		pointer = load_pointer(&gd, args->pointer_file);
		busy_pointer = load_pointer(&gd, args->busy_pointer_file);
		if (pointer == NULL || busy_pointer == NULL)
			goto out;
	}

	duration_us = (uint32)lroundf(1000.0f / (float)args->fps) * 1000UL;
	duration_ns = duration_us * 1000UL;

	encoder = zmbv_init(&gd, args);
	if (encoder == NULL)
		goto out;

	if (args->container == CONTAINER_AVI) {
		AVI = AVI_open_output_file(args->output_file);
		if (AVI == NULL)
			goto out;

		AVI_set_video(AVI, args->width, args->height, 24, args->fps, "ZMBV");
	} else {
		writer = mk_createWriter(args->output_file, 1000000LL, VLC_COMPAT);
		if (writer == NULL)
			goto out;

		IUtility->ClearMem(&bmih, sizeof(bmih));

		bmih.biSize      = h2le32(sizeof(bmih));
		bmih.biWidth     = h2le32(args->width);
		bmih.biHeight    = h2le32(args->height);
		bmih.biPlanes    = h2le16(1);
		bmih.biBitCount  = h2le16(24);
		bmih.biSizeImage = h2le32(args->width * args->height * 3);

		IExec->CopyMem("ZMBV", &bmih.biCompression, 4);

		IUtility->ClearMem(&video_conf, sizeof(video_conf));

		video_conf.trackType        = MK_TRACK_VIDEO;
		video_conf.flagEnabled      = 1;
		video_conf.flagDefault      = 1;
		video_conf.flagForced       = 1;
		video_conf.flagLacing       = 0; // Is this correct?
		video_conf.defaultDuration  = duration_ns;
		video_conf.codecID          = MK_VCODEC_MSVCM; // "V_MS/VFW/FOURCC"
		video_conf.codecPrivate     = &bmih;
		video_conf.codecPrivateSize = sizeof(bmih);

		video_conf.extra.video.flagInterlaced  = 0;
		video_conf.extra.video.pixelWidth      = args->width;
		video_conf.extra.video.pixelHeight     = args->height;
		video_conf.extra.video.displayWidth    = args->width;
		video_conf.extra.video.displayHeight   = args->height;
		video_conf.extra.video.displayUnit     = 0; // 0 = pixels
		video_conf.extra.video.aspectRatioType = MK_ASPECTRATIO_KEEP;

		video_track = mk_createTrack(writer, &video_conf);
		if (video_track == NULL)
			goto out;

		if (mk_writeHeader(writer, VERS) != 0)
			goto out;
	}

	create_icon(&gd, args->output_file);

	prev_time = get_uptime_micros(&gd);
	total_diff = 0;

	timer_sig = 1UL << tr->Request.io_Message.mn_ReplyPort->mp_SigBit;
	IExec->Signal(IExec->FindTask(NULL), timer_sig);

	while (TRUE) {
		signals = IExec->Wait(SIGBREAKF_CTRL_C | timer_sig);

		if (signals & SIGBREAKF_CTRL_C)
			break;

		if (signals & timer_sig) {
			struct IntuitionBase *IntuitionBase;
			struct Screen *first_screen;
			struct Window *active_window;
			uint32 ilock;
			uint32 comp_err = COMPERR_Success;

			if (timer_in_use)
				IExec->WaitIO((struct IORequest *)tr);

			/* Calculate time difference for frame skipping */
			current_time = get_uptime_micros(&gd);
			current_diff = current_time - prev_time - duration_us;
			if (current_diff > 0)
				total_diff += current_diff;
			prev_time = current_time;

			tr->Request.io_Command = TR_ADDREQUEST;
			tr->Time.Seconds       = 0UL;
			tr->Time.Microseconds  = duration_us;
			IExec->SendIO((struct IORequest *)tr);
			timer_in_use = TRUE;

			IntuitionBase = (struct IntuitionBase *)IIntuition->Data.LibBase;

			ilock = IIntuition->LockIBase(0);

			/* Check if the frontmost screen has changed and handle that */
			first_screen = IntuitionBase->FirstScreen;
			if (first_screen != current_screen) {
				if (bitmap != NULL) {
					IGraphics->FreeBitMap(bitmap);
					bitmap = NULL;
				}

				current_screen = first_screen;
				screen_bitmap  = current_screen->RastPort.BitMap;
				if (IGraphics->GetBitMapAttr(screen_bitmap, BMA_ISRTG) &&
				    IGraphics->GetBitMapAttr(screen_bitmap, BMA_BYTESPERPIXEL) > 1)
				{
					struct RastPort temp_rp;
					uint32 depth;

					depth  = IGraphics->GetBitMapAttr(screen_bitmap, BMA_DEPTH);
					bitmap = IGraphics->AllocBitMapTags(args->width, args->height, depth,
						BMATags_Friend, screen_bitmap,
						TAG_END);

					if (bitmap != NULL && zmbv_set_source_bm(encoder, bitmap)) {
						uint32 width, height;
						float scaled_width, scaled_height;

						IGraphics->InitRastPort(&temp_rp);
						temp_rp.BitMap = bitmap;

						width  = IGraphics->GetBitMapAttr(bitmap, BMA_WIDTH);
						height = IGraphics->GetBitMapAttr(bitmap, BMA_HEIGHT);
						IGraphics->RectFillColor(&temp_rp, 0, 0, width - 1, height - 1, 0);

						get_screen_dimensions(IGraphics, current_screen, &disp_width, &disp_height);

						scale_x = (float)args->width  / (float)disp_width;
						scale_y = (float)args->height / (float)disp_height;

						if (scale_x > scale_y)
							scale_x = scale_y;
						else
							scale_y = scale_x;

						scaled_width  = roundf((float)disp_width  * scale_x);
						scaled_height = roundf((float)disp_height * scale_y);

						min_x = floorf(((float)args->width  - scaled_width ) / 2.0f);
						min_y = floorf(((float)args->height - scaled_height) / 2.0f);
						max_x = min_x + scaled_width;
						max_y = min_y + scaled_height;

						min_s = 0.0f;
						min_t = 0.0f;
						max_s = (float)disp_width;
						max_t = (float)disp_height;

						init_vertex_array_from_rect(vertex_array, &gd.scale_rect);

						if (!args->no_pointer) {
							scale_pointer(&gd, pointer);
							scale_pointer(&gd, busy_pointer);
						}
					} else {
						IGraphics->FreeBitMap(bitmap);
						bitmap = NULL;
					}
				} else {
					screen_bitmap = NULL;
					IExec->DebugPrintF("non-RTG or CLUT formats are not supported!\n");
				}
			}

			if (!args->no_pointer) {
				mouse_x = current_screen->MouseX;
				mouse_y = current_screen->MouseY;

				active_window = IntuitionBase->ActiveWindow;
				if (active_window != NULL && active_window->ReqCount != 0)
					use_busy_pointer = TRUE;
				else
					use_busy_pointer = FALSE;
			}

			if (screen_bitmap != NULL && bitmap != NULL) {
				uint32 comp_flags = COMPFLAG_SrcAlphaOverride | COMPFLAG_HardwareOnly | COMPFLAG_IgnoreDestAlpha;

				if (!args->no_filter)
					comp_flags |= COMPFLAG_SrcFilter;

				comp_err = IGraphics->CompositeTags(COMPOSITE_Src, screen_bitmap, bitmap,
					COMPTAG_Flags,        comp_flags,
					COMPTAG_VertexArray,  vertex_array,
					COMPTAG_VertexFormat, COMPVF_STW0_Present,
					COMPTAG_NumTriangles, 2,
					TAG_END);
			}

			IIntuition->UnlockIBase(ilock);

			if (bitmap != NULL) {
				void *frame;
				uint32 framesize;
				BOOL keyframe;
				uint64 before, after;

				if (comp_err != COMPERR_Success) {
					IExec->DebugPrintF("composite call failed, error = %lu!\n", comp_err);
					goto out;
				}

				if (!args->no_pointer) {
					struct srec_pointer *sp = use_busy_pointer ? busy_pointer : pointer;
					render_pointer(&gd, sp, mouse_x, mouse_y);
				}

				before = get_uptime_micros(&gd);
				if (!zmbv_encode(encoder, &frame, &framesize, &keyframe))
					goto out;
				after = get_uptime_micros(&gd);
				encode_time += after - before;

				if (args->container == CONTAINER_AVI) {
					if (AVI_write_frame(AVI, frame, framesize, keyframe) != 0) {
						IExec->DebugPrintF("error outputting frame #%lu\n", frames);
						goto out;
					}
				} else {
					if (mk_startFrame(writer, video_track) != 0 ||
						mk_addFrameData(writer, video_track, frame, framesize) != 0 ||
						mk_setFrameFlags(writer, video_track, timestamp, keyframe, duration_ns) != 0)
					{
						IExec->DebugPrintF("error outputting frame #%lu\n", frames);
						goto out;
					}
				}

				timestamp += duration_ns;
				frames++;

				/* Skip frames if time difference is too large */
				while (total_diff >= (int64)duration_us) {
					total_diff -= (int64)duration_us;

					if (!zmbv_encode_dup(encoder, &frame, &framesize))
						goto out;

					if (args->container == CONTAINER_AVI) {
						if (AVI_write_frame(AVI, frame, framesize, FALSE) != 0) {
							IExec->DebugPrintF("error outputting frame #%lu\n", frames);
							goto out;
						}
					} else {
						if (mk_startFrame(writer, video_track) != 0 ||
							mk_addFrameData(writer, video_track, frame, framesize) != 0 ||
							mk_setFrameFlags(writer, video_track, timestamp, FALSE, duration_ns) != 0)
						{
							IExec->DebugPrintF("error outputting frame #%lu\n", frames);
							goto out;
						}
					}

					timestamp += duration_ns;
					frames++;
					skipped++;
				}
			} else {
				/* If there's nothing to output don't worry about skipping frames */
				total_diff = 0;
			}
		}
	}

	rc = RETURN_OK;

	IExec->DebugPrintF("Frames recorded: %lu\n"
		"Frames skipped: %lu\n"
		"Total: %lu\n",
		frames - skipped, skipped, frames);
	IExec->DebugPrintF("Avg time spent in encoder per frame: %lu microseconds\n",
		(uint32)(encode_time / (frames - skipped)));

out:
	if (bitmap != NULL)
		IGraphics->FreeBitMap(bitmap);

	if (args->container == CONTAINER_AVI) {
		if (AVI != NULL) {
			AVI_close(AVI);
		}
	} else {
		if (writer != NULL) {
			mk_close(writer);
		}
	}

	if (encoder != NULL)
		zmbv_end(encoder);

	if (!args->no_pointer) {
		if (pointer != NULL)
			free_pointer(&gd, pointer);

		if (busy_pointer != NULL)
			free_pointer(&gd, busy_pointer);
	}

	if (ITimer != NULL)
		IExec->DropInterface((struct Interface *)ITimer);

	if (tr != NULL) {
		if (timer_in_use) {
			if (IExec->CheckIO((struct IORequest *)tr) == NULL)
				IExec->AbortIO((struct IORequest *)tr);
			IExec->WaitIO((struct IORequest *)tr);
		}
		CloseTimerDevice(tr);
	}

	if (IIcon != NULL)
		CloseInterface((struct Interface *)IIcon);

	if (IGraphics != NULL)
		CloseInterface((struct Interface *)IGraphics);

	if (IIntuition != NULL)
		CloseInterface((struct Interface *)IIntuition);

	return rc;
}

