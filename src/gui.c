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
#include <intuition/menuclass.h>
#include <classes/requester.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/getfile.h>
#include <gadgets/clicktab.h>
#include <gadgets/chooser.h>
#include <gadgets/integer.h>
#include <gadgets/checkbox.h>
#include <images/label.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <interfaces/intuition.h>
#include <interfaces/icon.h>
#include <interfaces/commodities.h>
#include <interfaces/application.h>
#include <stdarg.h>
#include "SRec_rev.h"

const TEXT gpl_license[] =
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software Foundation,\n"
"Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA";

enum {
	EVT_POPKEY = 1,
	EVT_RECORDKEY,
	EVT_STOPKEY
};

enum {
	MID_DUMMY,
	MID_PROJECT_MENU,
	MID_PROJECT_ABOUT,
	MID_PROJECT_HIDE,
	MID_PROJECT_ICONIFY,
	MID_PROJECT_QUIT,
	MID_SETTINGS_MENU,
	MID_SETTINGS_SAVE_AS_DEFAULTS
};

enum {
	OID_MENUSTRIP,
	OID_WINDOW,
	OID_ROOT_LAYOUT,
	OID_OUTPUT_FILE,
	OID_TABS,
	OID_TAB_PAGES,
	OID_FORMAT_PAGE,
	OID_CONTAINER_FORMAT,
	OID_VIDEO_CODEC,
	OID_ASPECT_RATIO,
	OID_VIDEO_WIDTH_HEIGHT_LAYOUT,
	OID_VIDEO_WIDTH,
	OID_VIDEO_HEIGHT,
	OID_VIDEO_FPS_LAYOUT,
	OID_VIDEO_FPS,
	OID_AUDIO_CODEC,
	OID_AUDIO_SAMPLE_SIZE,
	OID_AUDIO_CHANNELS,
	OID_AUDIO_SAMPLE_RATE,
	OID_POINTER_PAGE,
	OID_ENABLE_POINTER,
	OID_POINTER_FILE_LAYOUT,
	OID_POINTER_FILE,
	OID_BUSY_POINTER_FILE_LAYOUT,
	OID_BUSY_POINTER_FILE,
	OID_MISC_PAGE,
	OID_BILINEAR_FILTER,
	OID_RECORD_STOP_LAYOUT,
	OID_RECORD,
	OID_STOP,
	OID_MAX
};

/* Global data structure for GUI */
struct srec_gui {
	struct LocaleInfo        *locale_info;
	struct IntuitionIFace    *iintuition;
	struct IconIFace         *iicon;
	struct CommoditiesIFace  *icommodities;
	struct ApplicationIFace  *iapplication;
	struct PrefsObjectsIFace *iprefsobjects;
	struct ClassLibrary      *requesterbase;
	Class                    *requesterclass;
	struct ClassLibrary      *windowbase;
	Class                    *windowclass;
	struct ClassLibrary      *layoutbase;
	Class                    *layoutclass;
	struct ClassLibrary      *getfilebase;
	Class                    *getfileclass;
	struct ClassLibrary      *clicktabbase;
	Class                    *clicktabclass;
	struct ClassLibrary      *spacebase;
	Class                    *spaceclass;
	struct ClassLibrary      *chooserbase;
	Class                    *chooserclass;
	struct ClassLibrary      *integerbase;
	Class                    *integerclass;
	struct ClassLibrary      *checkboxbase;
	Class                    *checkboxclass;
	struct ClassLibrary      *buttonbase;
	Class                    *buttonclass;
	struct ClassLibrary      *labelbase;
	Class                    *labelclass;
	struct DiskObject        *icon;
	struct MsgPort           *broker_mp;
	CxObj                    *broker;
	CONST_STRPTR              popkey;
	CONST_STRPTR              recordkey;
	CONST_STRPTR              stopkey;
	BOOL                      hidden;
	uint32                    app_id;
	struct MsgPort           *app_mp;
	PrefsObject              *app_prefs;
	struct MsgPort           *wb_mp;
	Object                   *obj[OID_MAX];
	TEXT                      window_title[64];
};

static BOOL gui_open_libs(struct srec_gui *gd) {
	BOOL error = 0;

	error |= !(gd->iintuition = (struct IntuitionIFace   *)OpenInterface("intuition.library", 53, "main", 1));
	error |= !(gd->iicon = (struct IconIFace *)OpenInterface("icon.library", 53, "main", 1));
	error |= !(gd->icommodities = (struct CommoditiesIFace *)OpenInterface("commodities.library", 53, "main", 1));
	error |= !(gd->iapplication = (struct ApplicationIFace *)OpenInterface("application.library", 53, "application", 2));
	error |= !(gd->iprefsobjects = (struct PrefsObjectsIFace *)OpenInterface("application.library", 53, "prefsobjects", 2));

	return (error == 0) ? TRUE : FALSE;
}

static void gui_close_libs(struct srec_gui *gd) {
	if (gd->iprefsobjects != NULL)
		CloseInterface((struct Interface *)gd->iprefsobjects);

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
	error |= !(gd->spacebase = IIntuition->OpenClass("gadgets/space.gadget", 53, &gd->spaceclass));
	error |= !(gd->chooserbase = IIntuition->OpenClass("gadgets/chooser.gadget", 53, &gd->chooserclass));
	error |= !(gd->integerbase = IIntuition->OpenClass("gadgets/integer.gadget", 53, &gd->integerclass));
	error |= !(gd->checkboxbase = IIntuition->OpenClass("gadgets/checkbox.gadget", 53, &gd->checkboxclass));
	error |= !(gd->buttonbase = IIntuition->OpenClass("gadgets/button.gadget", 53, &gd->buttonclass));
	error |= !(gd->labelbase = IIntuition->OpenClass("images/label.image", 53, &gd->labelclass));

	return (error == 0) ? TRUE : FALSE;
}

static void gui_close_classes(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->labelbase != NULL)
		IIntuition->CloseClass(gd->labelbase);

	if (gd->buttonbase != NULL)
		IIntuition->CloseClass(gd->buttonbase);

	if (gd->checkboxbase != NULL)
		IIntuition->CloseClass(gd->checkboxbase);

	if (gd->integerbase != NULL)
		IIntuition->CloseClass(gd->integerbase);

	if (gd->chooserbase != NULL)
		IIntuition->CloseClass(gd->chooserbase);

	if (gd->spacebase != NULL)
		IIntuition->CloseClass(gd->spacebase);

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

	if (gd->icon == NULL)
		return FALSE;

	gd->icon->do_CurrentX = NO_ICON_POSITION;
	gd->icon->do_CurrentY = NO_ICON_POSITION;
	return TRUE;
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
		REGAPP_LoadPrefs,         TRUE,
		TAG_END);
	if (gd->app_id == 0)
		return FALSE;

	IApplication->GetApplicationAttrs(gd->app_id,
		APPATTR_Port,          &gd->app_mp,
		APPATTR_MainPrefsDict, &gd->app_prefs,
		TAG_END);

	return TRUE;
}

static void gui_unregister_application(struct srec_gui *gd) {
	struct ApplicationIFace *IApplication = gd->iapplication;

	if (gd->app_id != 0)
		IApplication->UnregisterApplicationA(gd->app_id, NULL);
}

static BOOL VARARGS68K gui_create_menu(struct srec_gui *gd, ...) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	const struct TagItem *tags;
	BOOL result;
	va_list ap;

	gd->obj[OID_MENUSTRIP] = IIntuition->NewObject(NULL, "menuclass", MA_Type, T_ROOT, TAG_END);
	if (gd->obj[OID_MENUSTRIP] == NULL)
		return FALSE;

	va_startlinear(ap, gd);
	tags = va_getlinearva(ap, const struct TagItem *);
	result = IIntuition->IDoMethod(gd->obj[OID_MENUSTRIP], MM_NEWMENU, 0, tags);
	va_end(ap);

	return result;
}

static void gui_free_menu(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->obj[OID_MENUSTRIP] != NULL)
		IIntuition->DisposeObject(gd->obj[OID_MENUSTRIP]);
}

static BOOL gui_create_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct PrefsObjectsIFace *IPrefsObjects = gd->iprefsobjects;
	struct LocaleInfo *loc = gd->locale_info;
	CONST_STRPTR strings[7];
	BOOL result;

	IUtility->SNPrintf(gd->window_title, sizeof(gd->window_title), GetString(loc, MSG_WINDOW_TITLE),
		"SRec", gd->popkey ? gd->popkey : GetString(loc, MSG_KEY_NONE));

	result = gui_create_menu(gd,
		NM_Menu, GetString(loc, MSG_PROJECT_MENU), MA_ID, MID_PROJECT_MENU,
			NM_Item, GetString(loc, MSG_PROJECT_ABOUT), MA_Key, "?", MA_ID, MID_PROJECT_ABOUT,
			NM_Item, ML_SEPARATOR,
			NM_Item, GetString(loc, MSG_PROJECT_HIDE), MA_Key, "H", MA_ID, MID_PROJECT_HIDE,
			NM_Item, GetString(loc, MSG_PROJECT_ICONIFY), MA_Key, "I", MA_ID, MID_PROJECT_ICONIFY,
			NM_Item, ML_SEPARATOR,
			NM_Item, GetString(loc, MSG_PROJECT_QUIT), MA_Key, "Q", MA_ID, MID_PROJECT_QUIT,
		NM_Menu, GetString(loc, MSG_SETTINGS_MENU), MA_ID, MID_SETTINGS_MENU,
			NM_Item, GetString(loc, MSG_SETTINGS_SAVE_AS_DEFAULTS), MA_ID, MID_SETTINGS_SAVE_AS_DEFAULTS,
		TAG_END);
	if (!result)
		return FALSE;

	gd->wb_mp = IExec->AllocSysObject(ASOT_PORT, NULL);
	if (gd->wb_mp == NULL)
		return FALSE;

	#define SPACE \
		IIntuition->NewObjectA(gd->spaceclass, NULL, NULL)
	#define LABEL(id) \
		IIntuition->NewObject(gd->labelclass, NULL, \
			LABEL_Text, GetString(loc, id), \
			TAG_END)

	gd->obj[OID_OUTPUT_FILE] = IIntuition->NewObject(gd->getfileclass, NULL,
		GA_ID,              OID_OUTPUT_FILE,
		GA_RelVerify,       TRUE,
		GETFILE_Pattern,    "#?.mkv",
		GETFILE_DoSaveMode, TRUE,
		GETFILE_FullFile,   IPrefsObjects->DictGetStringForKey(gd->app_prefs, "OutputFile", ""),
		TAG_END);

	strings[0] = "MKV";
	strings[1] = NULL;

	gd->obj[OID_CONTAINER_FORMAT] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_CONTAINER_FORMAT,
		GA_RelVerify,       TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "ContainerFormat", 0),
		TAG_END);

	strings[0] = "ZMBV";
	strings[1] = NULL;

	gd->obj[OID_VIDEO_CODEC] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_VIDEO_CODEC,
		GA_RelVerify,       TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoCodec", 0),
		TAG_END);

	strings[0] = GetString(loc, MSG_ASPECT_RATIO_LIKE_WB);
	strings[1] = GetString(loc, MSG_ASPECT_RATIO_CUSTOM);
	strings[2] = GetString(loc, MSG_ASPECT_RATIO_4_3);
	strings[3] = GetString(loc, MSG_ASPECT_RATIO_5_4);
	strings[4] = GetString(loc, MSG_ASPECT_RATIO_16_9);
	strings[5] = GetString(loc, MSG_ASPECT_RATIO_16_10);
	strings[6] = NULL;

	gd->obj[OID_ASPECT_RATIO] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_ASPECT_RATIO,
		GA_RelVerify,       TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AspectRatio", 0),
		TAG_END);

	gd->obj[OID_VIDEO_WIDTH] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,           OID_VIDEO_WIDTH,
		GA_TabCycle,     TRUE,
		GA_RelVerify,    TRUE,
		INTEGER_Minimum, 16,
		INTEGER_Maximum, 16000,
		INTEGER_Number,  IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoWidth", DEFAULT_WIDTH),
		TAG_END);

	gd->obj[OID_VIDEO_HEIGHT] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,           OID_VIDEO_HEIGHT,
		GA_TabCycle,     TRUE,
		GA_RelVerify,    TRUE,
		INTEGER_Minimum, 16,
		INTEGER_Maximum, 16000,
		INTEGER_Number,  IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoHeight", DEFAULT_HEIGHT),
		TAG_END);

	gd->obj[OID_VIDEO_WIDTH_HEIGHT_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,           OID_VIDEO_WIDTH_HEIGHT_LAYOUT,
		LAYOUT_AddChild, gd->obj[OID_VIDEO_WIDTH],
		CHILD_Label,     LABEL(MSG_VIDEO_WIDTH_GAD),
		LAYOUT_AddChild, gd->obj[OID_VIDEO_HEIGHT],
		CHILD_Label,     LABEL(MSG_VIDEO_HEIGHT_GAD),
		TAG_END);

	gd->obj[OID_VIDEO_FPS] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,           OID_VIDEO_FPS,
		GA_TabCycle,     TRUE,
		GA_RelVerify,    TRUE,
		INTEGER_Minimum, 1,
		INTEGER_Maximum, 100,
		INTEGER_Number,  IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoFPS", DEFAULT_FPS),
		TAG_END);

	gd->obj[OID_VIDEO_FPS_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,           OID_VIDEO_FPS_LAYOUT,
		LAYOUT_AddChild, SPACE,
		LAYOUT_AddChild, gd->obj[OID_VIDEO_FPS],
		CHILD_Label,     LABEL(MSG_VIDEO_FPS_GAD),
		TAG_END);

	strings[0] = GetString(loc, MSG_AUDIO_CODEC_NONE);
	strings[1] = NULL;

	gd->obj[OID_AUDIO_CODEC] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_AUDIO_CODEC,
		GA_RelVerify,       TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   0,
		TAG_END);

	strings[0] = GetString(loc, MSG_AUDIO_SAMPLE_SIZE_8BIT);
	strings[1] = GetString(loc, MSG_AUDIO_SAMPLE_SIZE_16BIT);
	strings[2] = GetString(loc, MSG_AUDIO_SAMPLE_SIZE_32BIT);
	strings[3] = NULL;

	gd->obj[OID_AUDIO_SAMPLE_SIZE] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_AUDIO_SAMPLE_SIZE,
		GA_RelVerify,       TRUE,
		GA_Disabled,        TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AudioSampleSize", 1),
		TAG_END);

	strings[0] = GetString(loc, MSG_AUDIO_CHANNELS_MONO);
	strings[1] = GetString(loc, MSG_AUDIO_CHANNELS_STEREO);
	strings[2] = NULL;

	gd->obj[OID_AUDIO_CHANNELS] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_AUDIO_CHANNELS,
		GA_RelVerify,       TRUE,
		GA_Disabled,        TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AudioChannels", 1),
		TAG_END);

	strings[0] = GetString(loc, MSG_AUDIO_SAMPLE_RATE_11025HZ);
	strings[1] = GetString(loc, MSG_AUDIO_SAMPLE_RATE_22050HZ);
	strings[2] = GetString(loc, MSG_AUDIO_SAMPLE_RATE_44100HZ);
	strings[3] = GetString(loc, MSG_AUDIO_SAMPLE_RATE_48000HZ);
	strings[4] = NULL;

	gd->obj[OID_AUDIO_SAMPLE_RATE] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,              OID_AUDIO_SAMPLE_RATE,
		GA_RelVerify,       TRUE,
		GA_Disabled,        TRUE,
		CHOOSER_LabelArray, strings,
		CHOOSER_Selected,   IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AudioSampleRate", 3),
		TAG_END);

	gd->obj[OID_FORMAT_PAGE] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,              OID_FORMAT_PAGE,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,    gd->obj[OID_CONTAINER_FORMAT],
		CHILD_Label,        LABEL(MSG_CONTAINER_FORMAT_GAD),
		LAYOUT_AddChild,    gd->obj[OID_VIDEO_CODEC],
		CHILD_Label,        LABEL(MSG_VIDEO_CODEC_GAD),
		LAYOUT_AddChild,    gd->obj[OID_ASPECT_RATIO],
		CHILD_Label,        LABEL(MSG_ASPECT_RATIO_GAD),
		LAYOUT_AddChild,    gd->obj[OID_VIDEO_WIDTH_HEIGHT_LAYOUT],
		LAYOUT_AddChild,    gd->obj[OID_VIDEO_FPS_LAYOUT],
		LAYOUT_AddChild,    gd->obj[OID_AUDIO_CODEC],
		CHILD_Label,        LABEL(MSG_AUDIO_CODEC_GAD),
		LAYOUT_AddChild,    gd->obj[OID_AUDIO_SAMPLE_SIZE],
		CHILD_Label,        LABEL(MSG_AUDIO_SAMPLE_SIZE_GAD),
		LAYOUT_AddChild,    gd->obj[OID_AUDIO_CHANNELS],
		CHILD_Label,        LABEL(MSG_AUDIO_CHANNELS_GAD),
		LAYOUT_AddChild,    gd->obj[OID_AUDIO_SAMPLE_RATE],
		CHILD_Label,        LABEL(MSG_AUDIO_SAMPLE_RATE_GAD),
		TAG_END);

	gd->obj[OID_ENABLE_POINTER] = IIntuition->NewObject(gd->checkboxclass, NULL,
		GA_ID,            OID_ENABLE_POINTER,
		GA_RelVerify,     TRUE,
		CHECKBOX_Checked, IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "EnablePointer", TRUE),
		TAG_END);

	gd->obj[OID_POINTER_FILE] = IIntuition->NewObject(gd->getfileclass, NULL,
		GA_ID,            OID_POINTER_FILE,
		GA_TabCycle,      TRUE,
		GA_RelVerify,     TRUE,
		GETFILE_Pattern,  "#?.info",
		GETFILE_FullFile, IPrefsObjects->DictGetStringForKey(gd->app_prefs, "PointerFile", "ENV:Sys/def_pointer.info"),
		TAG_END);

	gd->obj[OID_POINTER_FILE_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,             OID_POINTER_FILE_LAYOUT,
		LAYOUT_Label,      GetString(loc, MSG_POINTER_FILE_GAD),
		LAYOUT_BevelStyle, BVS_GROUP,
		LAYOUT_AddChild,   gd->obj[OID_POINTER_FILE],
		TAG_END);

	gd->obj[OID_BUSY_POINTER_FILE] = IIntuition->NewObject(gd->getfileclass, NULL,
		GA_ID,            OID_BUSY_POINTER_FILE,
		GA_TabCycle,      TRUE,
		GA_RelVerify,     TRUE,
		GETFILE_Pattern,  "#?.info",
		GETFILE_FullFile, IPrefsObjects->DictGetStringForKey(gd->app_prefs, "BusyPointerFile", "ENV:Sys/def_busypointer.info"),
		TAG_END);

	gd->obj[OID_BUSY_POINTER_FILE_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,             OID_BUSY_POINTER_FILE_LAYOUT,
		LAYOUT_Label,      GetString(loc, MSG_BUSY_POINTER_FILE_GAD),
		LAYOUT_BevelStyle, BVS_GROUP,
		LAYOUT_AddChild,   gd->obj[OID_BUSY_POINTER_FILE],
		TAG_END);

	gd->obj[OID_POINTER_PAGE] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,                OID_POINTER_PAGE,
		LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,      gd->obj[OID_ENABLE_POINTER],
		CHILD_Label,          LABEL(MSG_ENABLE_POINTER_GAD),
		LAYOUT_AddChild,      gd->obj[OID_POINTER_FILE_LAYOUT],
		CHILD_WeightedHeight, 0,
		LAYOUT_AddChild,      gd->obj[OID_BUSY_POINTER_FILE_LAYOUT],
		CHILD_WeightedHeight, 0,
		LAYOUT_AddChild,      SPACE,
		TAG_END);

	gd->obj[OID_BILINEAR_FILTER] = IIntuition->NewObject(gd->checkboxclass, NULL,
		GA_ID,            OID_BILINEAR_FILTER,
		GA_RelVerify,     TRUE,
		CHECKBOX_Checked, IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "BilinearFilter", TRUE),
		TAG_END);

	gd->obj[OID_MISC_PAGE] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,              OID_MISC_PAGE,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,    gd->obj[OID_BILINEAR_FILTER],
		CHILD_Label,        LABEL(MSG_BILINEAR_FILTER_GAD),
		LAYOUT_AddChild,    SPACE,
		TAG_END);

	gd->obj[OID_TAB_PAGES] = IIntuition->NewObject(NULL, "page.gadget",
		GA_ID,        OID_TAB_PAGES,
		PAGE_Add,     gd->obj[OID_FORMAT_PAGE],
		PAGE_Add,     gd->obj[OID_POINTER_PAGE],
		PAGE_Add,     gd->obj[OID_MISC_PAGE],
		PAGE_Current, 0,
		TAG_END);

	strings[0] = GetString(loc, MSG_FORMAT_TAB);
	strings[1] = GetString(loc, MSG_POINTER_TAB);
	strings[2] = GetString(loc, MSG_MISC_TAB);
	strings[3] = NULL;

	gd->obj[OID_TABS] = IIntuition->NewObject(gd->clicktabclass, NULL,
		GA_ID,              OID_TABS,
		GA_RelVerify,       TRUE,
		GA_Text,            strings,
		CLICKTAB_PageGroup, gd->obj[OID_TAB_PAGES],
		TAG_END);

	gd->obj[OID_RECORD] = IIntuition->NewObject(gd->buttonclass, NULL,
		GA_ID,        OID_RECORD,
		GA_RelVerify, TRUE,
		GA_Text,      GetString(loc, MSG_RECORD_GAD),
		TAG_END);

	gd->obj[OID_STOP] = IIntuition->NewObject(gd->buttonclass, NULL,
		GA_ID,        OID_STOP,
		GA_RelVerify, TRUE,
		GA_Text,      GetString(loc, MSG_STOP_GAD),
		TAG_END);

	gd->obj[OID_RECORD_STOP_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,             OID_RECORD_STOP_LAYOUT,
		LAYOUT_BevelStyle, BVS_SBAR_VERT,
		LAYOUT_AddChild,   gd->obj[OID_RECORD],
		LAYOUT_AddChild,   gd->obj[OID_STOP],
		TAG_END);

	gd->obj[OID_ROOT_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,              OID_ROOT_LAYOUT,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,    gd->obj[OID_OUTPUT_FILE],
		CHILD_Label,        LABEL(MSG_OUTPUT_FILE_GAD),
		LAYOUT_AddChild,    gd->obj[OID_TABS],
		LAYOUT_AddChild,    gd->obj[OID_RECORD_STOP_LAYOUT],
		TAG_END);

	gd->obj[OID_WINDOW] = IIntuition->NewObject(gd->windowclass, NULL,
		WA_Title,             gd->window_title,
		WA_CloseGadget,       TRUE,
		WA_DragBar,           TRUE,
		WA_DepthGadget,       TRUE,
		WA_Activate,          TRUE,
		WA_NoCareRefresh,     TRUE,
		WA_IDCMP,             IDCMP_GADGETUP | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW,
		WINDOW_CharSet,       loc->li_CodeSet,
		WINDOW_Position,      WPOS_TOPLEFT,
		WINDOW_MenuStrip,     gd->obj[OID_MENUSTRIP],
		WINDOW_AppPort,       gd->wb_mp,
		WINDOW_IconifyGadget, TRUE,
		WINDOW_IconTitle,     "SRec",
		WINDOW_Icon,          gd->icon,
		WINDOW_IconNoDispose, TRUE,
		WINDOW_Layout,        gd->obj[OID_ROOT_LAYOUT],
		TAG_END);

	if (gd->obj[OID_TAB_PAGES] == NULL || gd->obj[OID_WINDOW] == NULL)
		return FALSE;

	return TRUE;
}

static void gui_free_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->obj[OID_WINDOW] != NULL)
		IIntuition->DisposeObject(gd->obj[OID_WINDOW]);

	if (gd->wb_mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, gd->wb_mp);

	gui_free_menu(gd);
}

static BOOL gui_show_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct Window *window;

	window = (struct Window *)IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_OPEN, NULL);
	if (window == NULL)
		return FALSE;

	return TRUE;
}

static void gui_hide_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_CLOSE, NULL);
}

static void gui_iconify_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_ICONIFY, NULL);
}

static void gui_about_requester(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct LocaleInfo *loc = gd->locale_info;
	struct Window *window;
	Object *requester;
	TEXT requester_title[64];
	TEXT body_text[1024];

	IIntuition->GetAttr(WINDOW_Window, gd->obj[OID_WINDOW], (uint32 *)&window);

	IUtility->SNPrintf(requester_title, sizeof(requester_title),
		GetString(loc, MSG_ABOUT_WINDOW_TITLE), "SRec");

	IUtility->SNPrintf(body_text, sizeof(body_text),
		"SRec version %lu.%lu (%s)\n\n"
		"Copyright (C) 2016 Fredrik Wikstrom <fredrik@a500.org>\n\n"
		"%s",
		VERSION, REVISION, DATE, gpl_license);

	requester = IIntuition->NewObject(gd->requesterclass, NULL,
		REQ_Image,      REQIMAGE_INFO,
		REQ_CharSet,    loc->li_CodeSet,
		REQ_TitleText,  requester_title,
		REQ_BodyText,   body_text,
		REQ_GadgetText, GetString(loc, MSG_OK_GAD),
		TAG_END);

	if (requester != NULL) {
		IIntuition->IDoMethod(requester, RM_OPENREQ, NULL, window, NULL);

		IIntuition->DisposeObject(requester);
	}
}

int gui_main(struct LocaleInfo *loc, struct WBStartup *wbs) {
	struct srec_gui *gd;
	struct IntuitionIFace *IIntuition;
	uint32 signals, cx_sig, window_sigs;
	BOOL done = FALSE;
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

	if (!gui_create_window(gd))
		goto out;

	if (!gd->hidden && !gui_show_window(gd))
		goto out;

	IIntuition = gd->iintuition;

	cx_sig = 1UL << gd->broker_mp->mp_SigBit;

	while (!done) {
		IIntuition->GetAttr(WINDOW_SigMask, gd->obj[OID_WINDOW], &window_sigs);
		signals = IExec->Wait(SIGBREAKF_CTRL_C | cx_sig | window_sigs);

		if (signals & SIGBREAKF_CTRL_C)
			done = TRUE;

		if (signals & cx_sig) {
			struct CommoditiesIFace *ICommodities = gd->icommodities;
			CxMsg *msg;
			uint32 type, id;

			while ((msg = (CxMsg *)IExec->GetMsg(gd->broker_mp)) != NULL) {
				type = ICommodities->CxMsgType(msg);
				id   = ICommodities->CxMsgID(msg);

				IExec->ReplyMsg((struct Message *)msg);

				switch (type) {
					case CXM_IEVENT:
						switch (id) {
							case EVT_POPKEY:
								gui_show_window(gd);
								break;
							case EVT_RECORDKEY:
								//FIXME: Start recording
								break;
							case EVT_STOPKEY:
								//FIXME: Stop recording
								break;
						}
						break;

					case CXM_COMMAND:
						switch (id) {
							case CXCMD_KILL:
								done = TRUE;
								break;
							case CXCMD_DISAPPEAR:
								gui_hide_window(gd);
								break;
							case CXCMD_UNIQUE:
							case CXCMD_APPEAR:
								gui_show_window(gd);
								break;
							case CXCMD_DISABLE:
							case CXCMD_ENABLE:
								ICommodities->ActivateCxObj(gd->broker, (id == CXCMD_ENABLE) ? TRUE : FALSE);
								break;
						}
						break;
				}
			}
		}

		if (signals & window_sigs) {
			BOOL hide = FALSE;
			BOOL iconify = FALSE;
			uint32 result;
			uint16 code;
			uint32 id;

			while ((result = IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
				switch (result & WMHI_CLASSMASK) {
					case WMHI_GADGETUP:
						id = result & WMHI_GADGETMASK;
						switch (id) {
							case OID_RECORD:
								//FIXME: Start recording
								break;
							case OID_STOP:
								//FIXME: Stop recording
								break;
						}
						break;

					case WMHI_MENUPICK:
						id = NO_MENU_ID;
						while ((id = IIntuition->IDoMethod(gd->obj[OID_MENUSTRIP], MM_NEXTSELECT, 0, id)) != NO_MENU_ID) {
							switch (id) {
								case MID_PROJECT_ABOUT:
									gui_about_requester(gd);
									break;
								case MID_PROJECT_HIDE:
									hide = TRUE;
									break;
								case MID_PROJECT_ICONIFY:
									iconify = TRUE;
									break;
								case MID_PROJECT_QUIT:
									done = TRUE;
									break;
							}
						}
						break;

					case WMHI_CLOSEWINDOW:
						hide = TRUE;
						break;
				}
			}

			if (hide) {
				gui_hide_window(gd);
			} else if (iconify) {
				gui_iconify_window(gd);
			}
		}
	}

	rc = RETURN_OK;

out:

	if (gd != NULL) {
		gui_free_window(gd);

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

