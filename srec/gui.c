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
#include "gui.h"
#include "locale.h"
#include "interfaces.h"
#include "srec.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <proto/commodities.h>
#include <proto/application.h>
#include "SRec_rev.h"

enum {
	EVT_POPKEY = 1,
	EVT_RECORDKEY,
	EVT_STOPKEY
};

enum {
	OID_WINDOW,
	OID_OUTPUT_FILE,
	OID_TABS,
	OID_TAB_PAGES,
	OID_FORMAT_PAGE,
	OID_CONTAINER_FORMAT,
	OID_VIDEO_CODEC,
	OID_VIDEO_PARAMS,
	OID_ASPECT_RATIO,
	OID_VIDEO_WIDTH,
	OID_VIDEO_HEIGHT,
	OID_VIDEO_FPS,
	OID_AUDIO_CODEC,
	OID_AUDIO_PARAMS,
	OID_SAMPLE_SIZE,
	OID_CHANNELS,
	OID_SAMPLE_RATE,
	OID_POINTER_PAGE,
	OID_ENABLE_POINTER,
	OID_POINTER_PARAMS,
	OID_POINTER_FILE,
	OID_BUSY_POINTER_PARAMS,
	OID_BUSY_POINTER_FILE,
	OID_MISC_PAGE,
	OID_BILINEAR_FILTER,
	OID_RECORD,
	OID_STOP,
	OID_MAX
};

/* Global data structure for GUI */
struct srec_gui {
	struct LocaleInfo       *locale_info;
	struct IntuitionIFace   *iintuition;
	struct IconIFace        *iicon;
	struct CommoditiesIFace *icommodities;
	struct ApplicationIFace *iapplication;
	struct ClassLibrary     *requesterbase;
	Class                   *requesterclass;
	struct ClassLibrary     *windowbase;
	Class                   *windowclass;
	struct ClassLibrary     *layoutbase;
	Class                   *layoutclass;
	struct ClassLibrary     *getfilebase;
	Class                   *getfileclass;
	struct ClassLibrary     *clicktabbase;
	Class                   *clicktabclass;
	struct ClassLibrary     *chooserbase;
	Class                   *chooserclass;
	struct ClassLibrary     *checkboxbase;
	Class                   *checkboxclass;
	struct ClassLibrary     *buttonbase;
	Class                   *buttonclass;
	struct DiskObject       *icon;
	struct MsgPort          *broker_mp;
	CxObj                   *broker;
	CONST_STRPTR             popkey;
	CONST_STRPTR             recordkey;
	CONST_STRPTR             stopkey;
	BOOL                     hidden;
	uint32                   app_id;
	struct MsgPort          *app_mp;
	Object                  *obj[OID_MAX];
};

static BOOL gui_open_libs(struct srec_gui *gd) {
	BOOL error = 0;

	error |= !(gd->iintuition = (struct IntuitionIFace   *)OpenInterface("intuition.library", 53, "main", 1));
	error |= !(gd->iicon = (struct IconIFace *)OpenInterface("icon.library", 53, "main", 1));
	error |= !(gd->icommodities = (struct CommoditiesIFace *)OpenInterface("commodities.library", 53, "main", 1));
	error |= !(gd->iapplication = (struct ApplicationIFace *)OpenInterface("application.library", 53, "application", 2));

	return (error == 0) ? TRUE : FALSE;
}

static void gui_close_libs(struct srec_gui *gd) {
	if (gd->iapplication != NULL)
		CloseInterface((struct Interface *)gd->iapplication);

	if (gd->icommodities != NULL)
		CloseInterface((struct Interface *)gd->icommodities);

	if (gd->iicon != NULL)
		CloseInterface((struct Interface *)gd->iicon);

	if (gd->iintuition != NULL)
		CloseInterface((struct Interface *)gd->iintuition);
}

static BOOL gui_open_classes(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	BOOL error = 0;

	error |= !(gd->requesterbase = IIntuition->OpenClass("requester.class", 53, &gd->requesterclass));
	error |= !(gd->windowbase = IIntuition->OpenClass("window.class", 53, &gd->windowclass));
	error |= !(gd->layoutbase = IIntuition->OpenClass("gadgets/layout.gadget", 53, &gd->layoutclass));
	error |= !(gd->getfilebase = IIntuition->OpenClass("gadgets/getfile.gadget", 53, &gd->getfileclass));
	error |= !(gd->clicktabbase = IIntuition->OpenClass("gadgets/clicktab.gadget", 53, &gd->clicktabclass));
	error |= !(gd->chooserbase = IIntuition->OpenClass("gadgets/chooser.gadget", 53, &gd->chooserclass));
	error |= !(gd->checkboxbase = IIntuition->OpenClass("gadgets/checkbox.gadget", 53, &gd->checkboxclass));
	error |= !(gd->buttonbase = IIntuition->OpenClass("gadgets/button.gadget", 53, &gd->buttonclass));

	return (error == 0) ? TRUE : FALSE;
}

static void gui_close_classes(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->buttonbase != NULL)
		IIntuition->CloseClass(gd->buttonbase);

	if (gd->checkboxbase != NULL)
		IIntuition->CloseClass(gd->checkboxbase);

	if (gd->chooserbase != NULL)
		IIntuition->CloseClass(gd->chooserbase);

	if (gd->clicktabbase != NULL)
		IIntuition->CloseClass(gd->clicktabbase);

	if (gd->getfilebase != NULL)
		IIntuition->CloseClass(gd->getfilebase);

	if (gd->layoutbase != NULL)
		IIntuition->CloseClass(gd->layoutbase);

	if (gd->windowbase != NULL)
		IIntuition->CloseClass(gd->windowbase);

	if (gd->requesterbase != NULL)
		IIntuition->CloseClass(gd->requesterbase);
}

static BOOL gui_get_icon(struct srec_gui *gd, struct WBStartup *wbs) {
	struct IconIFace *IIcon = gd->iicon;
	struct WBArg *wbargs = wbs->sm_ArgList;
	BPTR oldlock;

	oldlock = IDOS->SetCurrentDir(wbargs[0].wa_Lock);
	gd->icon = IIcon->GetIconTagList(wbargs[0].wa_Name, NULL);
	IDOS->SetCurrentDir(oldlock);

	if (gd->icon == NULL)
		gd->icon = IIcon->GetDefDiskObject(WBTOOL);

	if (gd->icon != NULL)
		return TRUE;

	return FALSE;
}

static BOOL gui_add_cx_event(struct srec_gui *gd, CONST_STRPTR descr, uint32 id) {
	struct CommoditiesIFace *ICommodities = gd->icommodities;
	CxObj *filter, *sender, *translate;

	if (descr == NULL)
		return TRUE;

	filter = ICommodities->CreateCxObj(CX_FILTER, (uint32)descr, 0);
	if (filter != NULL) {
		ICommodities->AttachCxObj(gd->broker, filter);

		sender = ICommodities->CreateCxObj(CX_SEND, (uint32)gd->broker_mp, id);
		translate = ICommodities->CreateCxObj(CX_TRANSLATE, 0, 0);

		ICommodities->AttachCxObj(filter, sender);
		ICommodities->AttachCxObj(filter, translate);

		if (ICommodities->CxObjError(filter) == 0)
			return TRUE;
	}
	return FALSE;
}

static BOOL gui_register_broker(struct srec_gui *gd) {
	struct IconIFace *IIcon = gd->iicon;
	struct CommoditiesIFace *ICommodities = gd->icommodities;
	STRPTR *tta = gd->icon->do_ToolTypes;
	CONST_STRPTR tt;
	int32 priority;
	struct NewBroker nb;
	BOOL error = 0;

	gd->broker_mp = IExec->AllocSysObject(ASOT_PORT, NULL);
	if (gd->broker_mp == NULL)
		return FALSE;

	IUtility->ClearMem(&nb, sizeof(nb));

	priority = 0;
	tt = IIcon->FindToolType(tta, "CX_PRIORITY");
	if (tt != NULL)
		IDOS->StrToLong(tt, &priority);

	nb.nb_Version = NB_VERSION;
	nb.nb_Name    = "SRec";
	nb.nb_Title   = VERS;
	nb.nb_Descr   = GetString(gd->locale_info, MSG_CX_DESCR);
	nb.nb_Unique  = NBU_UNIQUE | NBU_NOTIFY;
	nb.nb_Flags   = COF_SHOW_HIDE;
	nb.nb_Pri     = priority;
	nb.nb_Port    = gd->broker_mp;

	gd->broker = ICommodities->CxBroker(&nb, NULL);
	if (gd->broker == NULL)
		return FALSE;

	gd->popkey    = IIcon->FindToolType(tta, "CX_POPKEY");
	gd->recordkey = IIcon->FindToolType(tta, "RECORDKEY");
	gd->stopkey   = IIcon->FindToolType(tta, "STOPKEY");

	error |= !gui_add_cx_event(gd, gd->popkey, EVT_POPKEY);
	error |= !gui_add_cx_event(gd, gd->recordkey, EVT_RECORDKEY);
	error |= !gui_add_cx_event(gd, gd->stopkey, EVT_STOPKEY);
	if (error != 0)
		return FALSE;

	tt = IIcon->FindToolType(tta, "CX_POPUP");
	if (tt == NULL ||
		IUtility->Stricmp(tt, "no") == 0 ||
		IUtility->Stricmp(tt, "off") == 0 ||
		IUtility->Stricmp(tt, "false") == 0)
	{
		gd->hidden = TRUE;
	}

	return TRUE;
}

static void gui_unregister_broker(struct srec_gui *gd) {
	struct CommoditiesIFace *ICommodities = gd->icommodities;

	if (gd->broker != NULL)
		ICommodities->DeleteCxObjAll(gd->broker);

	if (gd->broker_mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, gd->broker_mp);
}

static BOOL gui_register_application(struct srec_gui *gd) {
	struct ApplicationIFace *IApplication = gd->iapplication;

	gd->app_id = IApplication->RegisterApplication("SRec",
		REGAPP_URLIdentifier,     "a500.org",
		REGAPP_UniqueApplication, TRUE,
		REGAPP_Hidden,            gd->hidden,
		TAG_END);
	if (gd->app_id == 0)
		return FALSE;

	IApplication->GetApplicationAttrs(gd->app_id,
		APPATTR_Port, &gd->app_mp,
		TAG_END);

	return TRUE;
}

static void gui_unregister_application(struct srec_gui *gd) {
	struct ApplicationIFace *IApplication = gd->iapplication;

	if (gd->app_id != 0)
		IApplication->UnregisterApplicationA(gd->app_id, NULL);
}

int gui_main(struct LocaleInfo *loc, struct WBStartup *wbs) {
	struct srec_gui *gd;
	int rc = RETURN_ERROR;

	gd = IExec->AllocVecTags(sizeof(*gd),
		AVT_Type,           MEMF_PRIVATE,
		AVT_Lock,           FALSE,
		AVT_ClearWithValue, 0,
		TAG_END);
	if (gd == NULL)
		goto out;

	gd->locale_info = loc;

	if (!gui_open_libs(gd))
		goto out;

	if (!gui_open_classes(gd))
		goto out;

	if (!gui_get_icon(gd, wbs))
		goto out;

	if (!gui_register_broker(gd))
		goto out;

	if (!gui_register_application(gd))
		goto out;

	rc = RETURN_OK;

out:

	if (gd != NULL) {
		gui_unregister_application(gd);

		gui_unregister_broker(gd);

		if (gd->icon != NULL)
			gd->iicon->FreeDiskObject(gd->icon);

		gui_close_classes(gd);

		gui_close_libs(gd);

		IExec->FreeVec(gd);
	}

	return rc;
}

