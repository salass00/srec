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

#ifndef EXEC_PORTS_H
#include <exec/ports.h>
#endif
#ifndef INTUITION_SCREENS_H
#include <intuition/screens.h>
#endif
#ifndef INTERFACES_GRAPHICS_H
#include <interfaces/graphics.h>
#endif

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
	AUDIO_CODEC_NONE = 0,
	AUDIO_CODEC_PCM
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
	BOOL           create_icon;
	TEXT           output_file[1024];
	TEXT           pointer_file[1024];
	TEXT           busy_pointer_file[1024];
};

struct SRecGlobal;

BOOL safe_signal_proc(uint32 pid, uint32 sigmask);
void get_screen_dimensions(struct GraphicsIFace *IGraphics,
	const struct Screen *screen, uint32 *widthp, uint32 *heightp);
int srec_entry(STRPTR argstring, int32 arglen, struct ExecBase *sysbase);

#endif

