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

#ifndef SREC_H
#define SREC_H

#include <exec/exec.h>
#include <interfaces/intuition.h>
#include <interfaces/graphics.h>
#include <interfaces/icon.h>
#include <interfaces/timer.h>

#define PROGNAME "SRec"

#define SREC_PROCNAME  "SRec: Screen Recorder"
#define SREC_PRIORITY  (0)
#define SREC_STACKSIZE (64UL << 10)

enum {
	CONTAINER_AVI = 1,
	CONTAINER_MKV,
};

enum {
	VIDEO_CODEC_ZMBV = 1
};

enum {
	AUDIO_CODEC_NONE = 0
};

enum {
	CHANNELS_MONO = 1,
	CHANNELS_STEREO = 2
};

#define DEFAULT_OUTPUT_FILE "RAM:output.mkv"

#define DEFAULT_CONTAINER   CONTAINER_MKV

#define DEFAULT_VIDEO_CODEC VIDEO_CODEC_ZMBV
#define DEFAULT_WIDTH       640
#define DEFAULT_HEIGHT      480
#define DEFAULT_FPS         10

#define DEFAULT_AUDIO_CODEC AUDIO_CODEC_NONE
#define DEFAULT_SAMPLE_SIZE 16
#define DEFAULT_CHANNELS    CHANNELS_STEREO
#define DEFAULT_SAMPLE_RATE 48000

#define DEFAULT_POINTER_FILE      "ENV:Sys/def_pointer.info"
#define DEFAULT_BUSY_POINTER_FILE "ENV:Sys/def_busypointer.info"

#define MIN_WIDTH  16
#define MAX_WIDTH  16000
#define MIN_HEIGHT 16
#define MAX_HEIGHT 16000
#define MIN_FPS    1
#define MAX_FPS    100

struct SRecArgs {
	struct Message message;
	uint32         container;
	uint32         video_codec;
	uint32         width, height;
	uint32         fps;
	uint32         audio_codec;
	uint32         sample_size;
	uint32         channels;
	uint32         sample_rate;
	BOOL           no_filter;
	BOOL           no_pointer;
	BOOL           no_altivec;
	TEXT           output_file[1024];
	TEXT           pointer_file[1024];
	TEXT           busy_pointer_file[1024];
};

typedef struct {
	float x, y;
	float s, t, w;
} vertex_t;

struct vertex_rect {
	float min_x, min_y, max_x, max_y;
	float min_s, min_t, max_s, max_t;
};

struct SRecGlobal {
	const struct SRecArgs *args;
	struct IntuitionIFace *iintuition;
	struct GraphicsIFace  *igraphics;
	struct IconIFace      *iicon;
	struct TimerIFace     *itimer;
	struct BitMap         *bitmap;
	uint32                 disp_width, disp_height;
	float                  scale_x, scale_y;
	struct vertex_rect     scale_rect;
};

BOOL safe_signal_proc(uint32 pid, uint32 sigmask);
void get_screen_dimensions(struct GraphicsIFace *IGraphics,
	const struct Screen *screen, uint32 *widthp, uint32 *heightp);
void set_rect_vertex_array(vertex_t *vertex_array, const struct vertex_rect *rect);
void get_frame_data(const struct SRecGlobal *gd, struct BitMap *dest_bm, uint32 width, uint32 height, uint32 bpr);
int srec_entry(STRPTR argstring, int32 arglen, struct ExecBase *sysbase);

#endif

