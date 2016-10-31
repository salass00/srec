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

#include "timer.h"
#include <proto/exec.h>

struct TimeRequest *OpenTimerDevice(uint32 unit) {
	struct MsgPort *mp;
	struct TimeRequest *tr;

	mp = IExec->AllocSysObject(ASOT_PORT, NULL);

	tr = IExec->AllocSysObjectTags(ASOT_IOREQUEST,
		ASOIOR_ReplyPort, mp,
		ASOIOR_Size,      sizeof(*tr),
		TAG_END);
	if (tr != NULL) {
		if (IExec->OpenDevice("timer.device", unit, (struct IORequest *)tr, 0) == IOERR_SUCCESS)
			return tr;

		IExec->FreeSysObject(ASOT_IOREQUEST, tr);
	}

	IExec->FreeSysObject(ASOT_PORT, mp);

	return NULL;
}

void CloseTimerDevice(struct TimeRequest *tr) {
	if (tr != NULL) {
		struct MsgPort *mp = tr->Request.io_Message.mn_ReplyPort;

		IExec->CloseDevice((struct IORequest *)tr);

		IExec->FreeSysObject(ASOT_IOREQUEST, tr);

		IExec->FreeSysObject(ASOT_PORT, mp);
	}
}

