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

#include "srec.h"
#include "interfaces.h"
#include "pointer.h"
#include "timer.h"
#include "avilib.h"
#include "libmkv.h"
#include "zmbv.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <sys/param.h>
#include <math.h>
#include "SRec_rev.h"

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

typedef struct {
	float x, y;
	float s, t, w;
} vertex_t;

int srec_entry(STRPTR argstring, int32 arglen, struct ExecBase *sysbase) {
	struct ExecIFace *IExec = (struct ExecIFace *)sysbase->MainInterface;
	struct Process *proc;
	const struct SRecArgs *args;
	struct SRecGlobal gd;
	struct zmbv_state *encoder = NULL;
	struct TimeRequest *tr = NULL;
	uint32 signals, timer_sig;
	BOOL timer_in_use = FALSE;
	struct Screen *current_screen = NULL;
	struct BitMap *screen_bitmap = NULL;
	struct BitMap *bitmap = NULL;
	uint32 disp_width, disp_height;
	vertex_t vertex_array[6];
	uint32 frames = 0;
	uint32 duration_us;
	uint64 duration_ns;
	uint64 timestamp = 0;
	struct srec_pointer *pointer = NULL;
	struct srec_pointer *busy_pointer = NULL;
	BOOL use_busy_pointer = FALSE;
	int rc = RETURN_ERROR;

	/* AVI output */
	avi_t *AVI = NULL;

	/* MKV output */
	mk_Writer *writer = NULL;
	BITMAPINFOHEADER bmih;
	mk_TrackConfig video_conf;
	mk_Track *video_track = NULL;

	proc = (struct Process *)IExec->FindTask(NULL);

	IExec->WaitPort(&proc->pr_MsgPort);
	args = (const struct SRecArgs *)IExec->GetMsg(&proc->pr_MsgPort);

	#define IIntuition gd.iintuition
	#define IGraphics  gd.igraphics
	#define IIcon      gd.iicon

	IIntuition = (struct IntuitionIFace *)OpenInterface("intuition.library", 53, "main", 1);
	IGraphics  = (struct GraphicsIFace *)OpenInterface("graphics.library", 54, "main", 1);
	IIcon      = (struct IconIFace *)OpenInterface("icon.library", 53, "main", 1);
	if (IIntuition == NULL || IGraphics == NULL || IIcon == NULL)
		goto out;

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

	tr = OpenTimerDevice(UNIT_MICROHZ);
	if (tr == NULL)
		goto out;

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
			uint32 comp_err;

			if (timer_in_use)
				IExec->WaitIO((struct IORequest *)tr);

			tr->Request.io_Command = TR_ADDREQUEST;
			tr->Time.Seconds       = 0UL;
			tr->Time.Microseconds  = duration_us;
			IExec->SendIO((struct IORequest *)tr);
			timer_in_use = TRUE;

			IntuitionBase = (struct IntuitionBase *)IIntuition->Data.LibBase;

			ilock = IIntuition->LockIBase(0);

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

					if (zmbv_set_source_bm(encoder, bitmap)) {
						uint32 width, height;
						uint32 mode_id, info_len;
						struct DimensionInfo diminfo;
						float scale_x, scale_y;
						float scaled_width, scaled_height;
						float min_x, min_y, max_x, max_y;
						float min_s, min_t, max_s, max_t;

						IGraphics->InitRastPort(&temp_rp);
						temp_rp.BitMap = bitmap;

						width  = IGraphics->GetBitMapAttr(bitmap, BMA_WIDTH);
						height = IGraphics->GetBitMapAttr(bitmap, BMA_HEIGHT);
						IGraphics->RectFillColor(&temp_rp, 0, 0, width - 1, height - 1, 0);

						disp_width  = current_screen->Width;
						disp_height = current_screen->Height;

						mode_id  = IGraphics->GetVPModeID(&current_screen->ViewPort);
						info_len = IGraphics->GetDisplayInfoData(NULL, &diminfo, sizeof(diminfo), DTAG_DIMS, mode_id);
						if (info_len >= offsetof(struct DimensionInfo, MaxOScan)) {
							disp_width  = MIN(disp_width,  (uint16)diminfo.Nominal.MaxX - (uint16)diminfo.Nominal.MinX + 1);
							disp_height = MIN(disp_height, (uint16)diminfo.Nominal.MaxY - (uint16)diminfo.Nominal.MinY + 1);
						}

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
						max_x = min_x + scaled_width - 1.0f;
						max_y = min_y + scaled_height - 1.0f;

						min_s = 0.0f;
						min_t = 0.0f;
						max_s = (float)disp_width - 1.0f;
						max_t = (float)disp_height - 1.0f;

						vertex_array[0].x = min_x;
						vertex_array[0].y = min_y;
						vertex_array[0].s = min_s;
						vertex_array[0].t = min_t;
						vertex_array[0].w = 1.0f;

						vertex_array[1].x = max_x;
						vertex_array[1].y = min_y;
						vertex_array[1].s = max_s;
						vertex_array[1].t = min_t;
						vertex_array[1].w = 1.0f;

						vertex_array[2].x = min_x;
						vertex_array[2].y = max_y;
						vertex_array[2].s = min_s;
						vertex_array[2].t = max_t;
						vertex_array[2].w = 1.0f;

						vertex_array[3].x = min_x;
						vertex_array[3].y = max_y;
						vertex_array[3].s = min_s;
						vertex_array[3].t = max_t;
						vertex_array[3].w = 1.0f;

						vertex_array[4].x = max_x;
						vertex_array[4].y = min_y;
						vertex_array[4].s = max_s;
						vertex_array[4].t = min_t;
						vertex_array[4].w = 1.0f;

						vertex_array[5].x = max_x;
						vertex_array[5].y = max_y;
						vertex_array[5].s = max_s;
						vertex_array[5].t = max_t;
						vertex_array[5].w = 1.0f;

						//FIXME: Scale pointers as well
					} else {
						IGraphics->FreeBitMap(bitmap);
						bitmap = NULL;
					}
				} else {
					screen_bitmap = NULL;
					IExec->DebugPrintF("non-RTG or CLUT formats are not supported!\n");
				}
			}

			active_window = IntuitionBase->ActiveWindow;
			if (active_window != NULL && active_window->ReqCount != 0)
				use_busy_pointer = TRUE;
			else
				use_busy_pointer = FALSE;

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

				if (comp_err != COMPERR_Success) {
					IExec->DebugPrintF("composite call failed, error = %lu!\n", comp_err);
					goto out;
				}

				if (use_busy_pointer) {
					//FIXME: Render busy pointer
				} else {
					//FIXME: Render normal pointer
				}

				if (!zmbv_encode(encoder, &frame, &framesize, &keyframe))
					goto out;

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
			}
		}
	}

	rc = RETURN_OK;

out:
	if (bitmap != NULL)
		IGraphics->FreeBitMap(bitmap);

	if (tr != NULL) {
		if (timer_in_use) {
			if (IExec->CheckIO((struct IORequest *)tr) == NULL)
				IExec->AbortIO((struct IORequest *)tr);
			IExec->WaitIO((struct IORequest *)tr);
		}
		CloseTimerDevice(tr);
	}

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

	if (IIcon != NULL)
		CloseInterface((struct Interface *)IIcon);

	if (IGraphics != NULL)
		CloseInterface((struct Interface *)IGraphics);

	if (IIntuition != NULL)
		CloseInterface((struct Interface *)IIntuition);

	return rc;
}

