/*
 * SRec - Screen Recorder
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define CATCOMP_NUMBERS
#include "cli.h"
#include "locale.h"
#include "srec.h"
#include "pointer.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#define TEMPLATE "FILENAME/A,WIDTH/N,HEIGHT/N,FPS/N,AVI/S,NOFILTER/S,NOPOINTER/S,NOALTIVEC/S,CREATEICON/S"

enum {
	ARG_FILENAME,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_FPS,
	ARG_AVI,
	ARG_NOFILTER,
	ARG_NOPOINTER,
	ARG_NOALTIVEC,
	ARG_CREATEICON,
	ARG_MAX
};

int cli_main(struct LocaleInfo *loc) {
	int32 args[ARG_MAX];
	struct RDArgs *rdargs;
	struct SRecArgs *srec_args = NULL;
	struct MsgPort *mp = NULL;
	struct DeathMessage *death_msg = NULL, *dm;
	struct Process *srec_proc;
	uint32 srec_pid;
	uint32 signals;
	int rc = RETURN_ERROR;

	IUtility->ClearMem(args, sizeof(args));

	rdargs = IDOS->ReadArgs(TEMPLATE, args, NULL);
	if (rdargs == NULL) {
		IDOS->PrintFault(IDOS->IoErr(), PROGNAME);
		goto out;
	}

	srec_args = IExec->AllocVecTags(sizeof(*srec_args),
		AVT_Type, MEMF_SHARED,
		TAG_END);
	if (srec_args == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, PROGNAME);
		goto out;
	}

	IUtility->Strlcpy(srec_args->output_file, (CONST_STRPTR)args[ARG_FILENAME], sizeof(srec_args->output_file));

	IUtility->Strlcpy(srec_args->pointer_file, DEFAULT_POINTER_FILE, sizeof(srec_args->pointer_file));
	IUtility->Strlcpy(srec_args->busy_pointer_file, DEFAULT_BUSY_POINTER_FILE, sizeof(srec_args->busy_pointer_file));

	strip_info_extension(srec_args->pointer_file);
	strip_info_extension(srec_args->busy_pointer_file);

	srec_args->container   = DEFAULT_CONTAINER;
	srec_args->video_codec = DEFAULT_VIDEO_CODEC;
	srec_args->width       = DEFAULT_WIDTH;
	srec_args->height      = DEFAULT_HEIGHT;
	srec_args->fps         = DEFAULT_FPS;
	srec_args->audio_codec = DEFAULT_AUDIO_CODEC;
	srec_args->sample_size = DEFAULT_SAMPLE_SIZE;
	srec_args->channels    = DEFAULT_CHANNELS;
	srec_args->sample_rate = DEFAULT_SAMPLE_RATE;

	if ((APTR)args[ARG_WIDTH] != NULL)
		srec_args->width = *(uint32 *)args[ARG_WIDTH];

	if ((APTR)args[ARG_HEIGHT] != NULL)
		srec_args->height = *(uint32 *)args[ARG_HEIGHT];

	if ((APTR)args[ARG_FPS] != NULL)
		srec_args->fps = *(uint32 *)args[ARG_FPS];

	if (args[ARG_AVI])
		srec_args->container = CONTAINER_AVI;

	srec_args->no_filter   = args[ARG_NOFILTER]   ? TRUE : FALSE;
	srec_args->no_pointer  = args[ARG_NOPOINTER]  ? TRUE : FALSE;
	srec_args->no_altivec  = args[ARG_NOALTIVEC]  ? TRUE : FALSE;
	srec_args->create_icon = args[ARG_CREATEICON] ? TRUE : FALSE;

	mp = IExec->AllocSysObject(ASOT_PORT, NULL);
	if (mp == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, PROGNAME);
		goto out;
	}

	death_msg = IExec->AllocSysObjectTags(ASOT_MESSAGE,
		ASOMSG_Name,      PROGNAME " death message",
		ASOMSG_ReplyPort, mp,
		ASOMSG_Size,      sizeof(*death_msg),
		TAG_END);
	if (death_msg == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, PROGNAME);
		goto out;
	}

	srec_proc = IDOS->CreateNewProcTags(
		NP_Name,                 SREC_PROCNAME,
		NP_Priority,             SREC_PRIORITY,
		NP_Child,                TRUE,
		NP_Entry,                srec_entry,
		NP_StackSize,            SREC_STACKSIZE,
		NP_LockStack,            TRUE,
		NP_UserData,             srec_args,
		NP_NotifyOnDeathMessage, death_msg,
		TAG_END);
	if (srec_proc == NULL) {
		IDOS->PrintFault(IDOS->IoErr(), PROGNAME);
		goto out;
	}

	/* On success IoErr() returns the PID */
	srec_pid = IDOS->IoErr();

	IDOS->Printf("%s\n", GetString(loc, MSG_CTRL_C_TO_STOP));
	signals = IExec->Wait(SIGBREAKF_CTRL_C | (1UL << mp->mp_SigBit));

	if (signals & SIGBREAKF_CTRL_C)
		safe_signal_proc(srec_pid, SIGBREAKF_CTRL_C);

	IExec->WaitPort(mp);
	dm = (struct DeathMessage *)IExec->GetMsg(mp);

	srec_pid = 0;

	if (dm->dm_ReturnCode != RETURN_OK) {
		BPTR ErrorOut = IDOS->ErrorOutput();
		if (ErrorOut == ZERO)
			ErrorOut = IDOS->Output();

		IDOS->FPrintf(ErrorOut, "Screen Recorder process failed!\n"
			"Return code: %ld Error: %ld\n",
			dm->dm_ReturnCode, dm->dm_Result2);
		goto out;
	}

	rc = RETURN_OK;

out:
	if (death_msg != NULL)
		IExec->FreeSysObject(ASOT_MESSAGE, death_msg);

	if (srec_args != NULL)
		IExec->FreeVec(srec_args);

	if (mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, mp);

	if (rdargs != NULL)
		IDOS->FreeArgs(rdargs);

	return rc;
}

