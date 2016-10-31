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

#include "ahi.h"
#include <proto/exec.h>

struct AHIRequest *OpenAHIDevice(uint32 unit) {
	struct MsgPort *mp;
	struct AHIRequest *ahir;

	mp = IExec->AllocSysObjectTags(ASOT_PORT, NULL);

	ahir = IExec->AllocSysObjectTags(ASOT_IOREQUEST,
		ASOIOR_ReplyPort, mp,
		ASOIOR_Size,      sizeof(*ahir),
		TAG_END);
	if (ahir != NULL) {
		if (IExec->OpenDevice("ahi.device", unit, (struct IORequest *)ahir, 0) == IOERR_SUCCESS)
			return ahir;

		IExec->FreeSysObject(ASOT_IOREQUEST, ahir);
	}

	IExec->FreeSysObject(ASOT_PORT, mp);

	return NULL;
}

void CloseAHIDevice(struct AHIRequest *ahir) {
	if (ahir != NULL) {
		struct MsgPort *mp = ahir->ahir_Std.io_Message.mn_ReplyPort;

		IExec->CloseDevice((struct IORequest *)ahir);

		IExec->FreeSysObject(ASOT_IOREQUEST, ahir);

		IExec->FreeSysObject(ASOT_PORT, mp);
	}
}

