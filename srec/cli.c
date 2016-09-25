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
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#define TEMPLATE "FILENAME/A,WIDTH/N,HEIGHT/N,FPS/N,NOFILTER/S,NOALTIVEC/S"

enum {
	ARG_FILENAME,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_FPS,
	ARG_NOFILTER,
	ARG_NOALTIVEC,
	ARG_MAX
};

int cli_main(struct LocaleInfo *loc) {
	int32 args[ARG_MAX];
	struct RDArgs *rdargs;
	struct SRecArgs *srec_args = NULL;
	struct MsgPort *mp = NULL;
	struct DeathMessage *deathm = NULL;
	struct Process *srec_proc;
	uint32 signals;
	int rc = RETURN_ERROR;

	IUtility->ClearMem(args, sizeof(args));

	rdargs = IDOS->ReadArgs(TEMPLATE, args, NULL);
	if (rdargs == NULL) {
		IDOS->PrintFault(IDOS->IoErr(), "SRec");
		goto out;
	}

	srec_args = IExec->AllocVecTags(sizeof(*srec_args),
		AVT_Type,           MEMF_SHARED,
		AVT_Lock,           FALSE,
		TAG_END);
	if (srec_args == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, "SRec");
		goto out;
	}

	srec_args->width  = DEFAULT_WIDTH;
	srec_args->height = DEFAULT_HEIGHT;
	srec_args->fps    = DEFAULT_FPS;

	IUtility->Strlcpy(srec_args->filename, (CONST_STRPTR)args[ARG_FILENAME], sizeof(srec_args->filename));

	if ((APTR)args[ARG_WIDTH] != NULL)
		srec_args->width = *(uint32 *)args[ARG_WIDTH];

	if ((APTR)args[ARG_HEIGHT] != NULL)
		srec_args->height = *(uint32 *)args[ARG_HEIGHT];

	if ((APTR)args[ARG_FPS] != NULL)
		srec_args->fps = *(uint32 *)args[ARG_FPS];

	srec_args->no_filter = args[ARG_NOFILTER] ? TRUE : FALSE;
	srec_args->no_altivec = args[ARG_NOALTIVEC] ? TRUE : FALSE;

	mp = IExec->AllocSysObject(ASOT_PORT, NULL);
	if (mp == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, "SRec");
		goto out;
	}

	deathm = IExec->AllocSysObjectTags(ASOT_MESSAGE,
		ASOMSG_Name,      "SRec death message",
		ASOMSG_ReplyPort, mp,
		ASOMSG_Length,    sizeof(struct DeathMessage),
		TAG_END);
	if (deathm == NULL) {
		IDOS->PrintFault(ERROR_NO_FREE_STORE, "SRec");
		goto out;
	}

	srec_proc = IDOS->CreateNewProcTags(
		NP_Name,                 SREC_PROCNAME,
		NP_Priority,             SREC_PRIORITY,
		NP_Child,                TRUE,
		NP_Entry,                srec_entry,
		NP_UserData,             srec_args,
		#ifdef SREC_STACKSIZE
		NP_StackSize,            SREC_STACKSIZE,
		#endif
		NP_LockStack,            TRUE,
		NP_NotifyOnDeathMessage, deathm,
		TAG_END);
	if (srec_proc == NULL) {
		IDOS->PrintFault(IDOS->IoErr(), "SRec");
		goto out;
	}

	IDOS->Printf("%s\n", GetString(loc, MSG_CTRL_C_TO_STOP));

	signals = IExec->Wait(SIGBREAKF_CTRL_C | (1UL << mp->mp_SigBit));

	if (signals & SIGBREAKF_CTRL_C)
		IExec->Signal((struct Task *)srec_proc, SIGBREAKF_CTRL_C);

	IExec->WaitPort(mp);
	deathm = (struct DeathMessage *)IExec->GetMsg(mp);

	if (deathm->dm_ReturnCode != RETURN_OK) {
		BPTR ErrorOut = IDOS->ErrorOutput();
		if (ErrorOut == ZERO)
			ErrorOut = IDOS->Output();

		IDOS->FPrintf(ErrorOut, "Screen Recorder process failed! (rc: %ld IoErr(): %ld)\n",
			deathm->dm_ReturnCode, deathm->dm_Result2);
		goto out;
	}

	rc = RETURN_OK;

out:
	if (deathm != NULL)
		IExec->FreeSysObject(ASOT_MESSAGE, deathm);

	if (mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, mp);

	if (srec_args != NULL)
		IExec->FreeVec(srec_args);

	if (rdargs != NULL)
		IDOS->FreeArgs(rdargs);

	return rc;
}

