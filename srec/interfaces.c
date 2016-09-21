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

#include "interfaces.h"
#include <proto/exec.h>

struct Interface *OpenInterface(CONST_STRPTR name, uint32 version) {
	struct Library *library;
	struct Interface *interface;

	library = IExec->OpenLibrary(name, version);
	interface = IExec->GetInterface(library, "main", 1, NULL);
	if (interface == NULL)
		IExec->CloseLibrary(library);

	return interface;
}

void CloseInterface(struct Interface *interface) {
	if (interface != NULL) {
		struct Library *library = interface->Data.LibBase;

		IExec->DropInterface(interface);
		IExec->CloseLibrary(library);
	}
}

