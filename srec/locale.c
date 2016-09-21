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

#define CATCOMP_ARRAY
#include "locale.h"
#include <proto/exec.h>
#include <proto/locale.h>

void InitLocaleInfo(struct LocaleInfo *loc, CONST_STRPTR name, uint32 dcs) {
	loc->li_Catalog = NULL;
	loc->li_CodeSet = dcs;

	loc->li_LocaleBase = IExec->OpenLibrary("locale.library", 53);

	loc->li_ILocale = (struct LocaleIFace *)IExec->GetInterface(loc->li_LocaleBase, "main", 1, NULL);
	if (loc->li_ILocale != NULL) {
		loc->li_Catalog = loc->li_ILocale->OpenCatalog(NULL, name,
			OC_BuiltInCodeSet, dcs,
			TAG_END);
		if (loc->li_Catalog != NULL)
			loc->li_CodeSet = loc->li_Catalog->cat_CodeSet;
	} else {
		IExec->CloseLibrary(loc->li_LocaleBase);
		loc->li_LocaleBase = NULL;
	}
}

void FreeLocaleInfo(struct LocaleInfo *loc) {
	if (loc->li_Catalog != NULL)
		loc->li_ILocale->CloseCatalog(loc->li_Catalog);

	IExec->DropInterface((struct Interface *)loc->li_ILocale);
	IExec->CloseLibrary(loc->li_LocaleBase);
}

CONST_STRPTR GetString(struct LocaleInfo *loc, int32 id) {
	CONST_STRPTR string = "(null)";
	int32 i;

	for (i = 0; i < (sizeof(CatCompArray)/sizeof(CatCompArray[0])); i++) {
		if (CatCompArray[i].cca_ID == id) {
			string = CatCompArray[i].cca_Str;
			break;
		}
	}

	if (loc->li_Catalog != NULL)
		string = loc->li_ILocale->GetCatalogStr(loc->li_Catalog, id, string);

	return string;
}

