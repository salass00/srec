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
#include "locale.h"
#include "gui.h"
#include "cli.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <signal.h>
#include "SRec_rev.h"

const char USED verstag[] = VERSTAG " alpha version";

extern struct Interface *INewlib;

int main(int argc, char **argv) {
	struct LocaleInfo loc;
	int rc;

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	InitLocaleInfo(&loc, "SRec.catalog", 4);

	if (!LIB_IS_AT_LEAST(IExec->Data.LibBase, 53, 70) || !LIB_IS_AT_LEAST(INewlib->Data.LibBase, 53, 30)) {
		TEXT buffer[128];

		BPTR ErrorOut = IDOS->ErrorOutput();
		if (ErrorOut == ZERO)
			ErrorOut = IDOS->Output();

		IUtility->SNPrintf(buffer, sizeof(buffer), GetString(&loc, MSG_TOO_OLD_OS_VERSION),
			VERSION, REVISION, "AmigaOS 4.1 Final Edition");
		IDOS->FPrintf(ErrorOut, "%s\n", buffer);

		rc = RETURN_FAIL;
		goto out;
	}

	if (argc == 0) {
		rc = gui_main(&loc, (struct WBStartup *)argv);
	} else {
		rc = cli_main(&loc);
	}

out:

	FreeLocaleInfo(&loc);

	return rc;
}

