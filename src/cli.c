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
	struct SRecArgs *startup_msg = NULL;
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

	startup_msg = IExec->AllocSysObjectTags(ASOT_MESSAGE,
		ASOMSG_Name, PROGNAME " startup message",
		ASOMSG_Size, sizeof(*startup_msg),
		TAG_END);
	if (startup_msg == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, PROGNAME);
		goto out;
	}

	IUtility->Strlcpy(startup_msg->output_file, (CONST_STRPTR)args[ARG_FILENAME], sizeof(startup_msg->output_file));

	IUtility->Strlcpy(startup_msg->pointer_file, DEFAULT_POINTER_FILE, sizeof(startup_msg->pointer_file));
	IUtility->Strlcpy(startup_msg->busy_pointer_file, DEFAULT_BUSY_POINTER_FILE, sizeof(startup_msg->busy_pointer_file));

	strip_info_extension(startup_msg->pointer_file);
	strip_info_extension(startup_msg->busy_pointer_file);

	startup_msg->container   = DEFAULT_CONTAINER;
	startup_msg->video_codec = DEFAULT_VIDEO_CODEC;
	startup_msg->width       = DEFAULT_WIDTH;
	startup_msg->height      = DEFAULT_HEIGHT;
	startup_msg->fps         = DEFAULT_FPS;
	startup_msg->audio_codec = DEFAULT_AUDIO_CODEC;
	startup_msg->sample_size = DEFAULT_SAMPLE_SIZE;
	startup_msg->channels    = DEFAULT_CHANNELS;
	startup_msg->sample_rate = DEFAULT_SAMPLE_RATE;

	if ((APTR)args[ARG_WIDTH] != NULL)
		startup_msg->width = *(uint32 *)args[ARG_WIDTH];

	if ((APTR)args[ARG_HEIGHT] != NULL)
		startup_msg->height = *(uint32 *)args[ARG_HEIGHT];

	if ((APTR)args[ARG_FPS] != NULL)
		startup_msg->fps = *(uint32 *)args[ARG_FPS];

	if (args[ARG_AVI])
		startup_msg->container = CONTAINER_AVI;

	startup_msg->no_filter   = args[ARG_NOFILTER]   ? TRUE : FALSE;
	startup_msg->no_pointer  = args[ARG_NOPOINTER]  ? TRUE : FALSE;
	startup_msg->no_altivec  = args[ARG_NOALTIVEC]  ? TRUE : FALSE;
	startup_msg->create_icon = args[ARG_CREATEICON] ? TRUE : FALSE;

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
		NP_NotifyOnDeathMessage, death_msg,
		TAG_END);
	if (srec_proc == NULL) {
		IDOS->PrintFault(IDOS->IoErr(), PROGNAME);
		goto out;
	}

	srec_pid = IDOS->GetPID(srec_proc, GPID_PROCESS);
	IExec->PutMsg(&srec_proc->pr_MsgPort, &startup_msg->message);

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

	if (startup_msg != NULL)
		IExec->FreeSysObject(ASOT_MESSAGE, startup_msg);

	if (mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, mp);

	if (rdargs != NULL)
		IDOS->FreeArgs(rdargs);

	return rc;
}

