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
#include "gui.h"
#include "locale.h"
#include "interfaces.h"
#include "srec.h"
#include "pointer.h"
#include <intuition/menuclass.h>
#include <classes/requester.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/getfile.h>
#include <gadgets/clicktab.h>
#include <gadgets/chooser.h>
#include <gadgets/string.h>
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
#include <interfaces/layout.h>
#include <interfaces/chooser.h>
#include <sys/param.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "SRec_rev.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define CLAMP(val,min,max) MAX(MIN(val, max), min)

#define ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

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
"Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA";

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
	OID_HOT_KEYS_PAGE,
	OID_POPUP_KEY,
	OID_RECORD_KEY,
	OID_STOP_KEY,
	OID_MISC_PAGE,
	OID_BILINEAR_FILTER,
	OID_DISABLE_ALTIVEC,
	OID_CREATE_ICON,
	OID_RECORD_STOP_LAYOUT,
	OID_RECORD,
	OID_STOP,
	OID_MAX
};

/* Global data structure for GUI */
struct srec_gui {
	struct LocaleInfo        *locale_info;
	uint32                    vector_unit;

	struct IntuitionIFace    *iintuition;
	struct GraphicsIFace     *igraphics;
	struct IconIFace         *iicon;
	struct CommoditiesIFace  *icommodities;
	struct ApplicationIFace  *iapplication;
	struct PrefsObjectsIFace *iprefsobjects;
	struct LayoutIFace       *ilayout;
	struct ChooserIFace      *ichooser;
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

	uint32                    srec_pid;
	struct MsgPort           *srec_mp;
	struct SRecArgs          *srec_args;
	struct DeathMessage      *death_msg;

	CONST_STRPTR              output_file;
	CONST_STRPTR              pointer_file;
	CONST_STRPTR              busy_pointer_file;
	uint32                    container;
	uint32                    video_codec;
	uint32                    aspect_ratio;
	uint32                    width;
	uint32                    height;
	uint32                    fps;
	uint32                    audio_codec;
	uint32                    sample_size;
	uint32                    channels;
	uint32                    sample_rate;
	BOOL                      enable_pointer;
	BOOL                      enable_filter;
	BOOL                      enable_altivec;
	BOOL                      create_icon;

	struct MsgPort           *wb_mp;
	Object                   *obj[OID_MAX];
	struct List              *container_list;
	struct List              *video_codec_list;
	struct List              *aspect_ratio_list;
	struct List              *audio_codec_list;
	struct List              *sample_size_list;
	struct List              *channels_list;
	struct List              *sample_rate_list;
	TEXT                      window_title[64];
};

static BOOL gui_open_libs(struct srec_gui *gd) {
	BOOL error = 0;

	error |= !(gd->iintuition = (struct IntuitionIFace *)OpenInterface("intuition.library", 53, "main", 1));
	error |= !(gd->igraphics = (struct GraphicsIFace *)OpenInterface("graphics.library", 54, "main", 1));
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

	if (gd->igraphics != NULL)
		CloseInterface((struct Interface *)gd->igraphics);

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

	if (gd->layoutbase != NULL)
		error |= !(gd->ilayout = (struct LayoutIFace *)IExec->GetInterface(&gd->layoutbase->cl_Lib, "main", 1, NULL));

	if (gd->chooserbase != NULL)
		error |= !(gd->ichooser = (struct ChooserIFace *)IExec->GetInterface(&gd->chooserbase->cl_Lib, "main", 1, NULL));

	return (error == 0) ? TRUE : FALSE;
}

static void gui_close_classes(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->ichooser != NULL)
		IExec->DropInterface((struct Interface *)gd->ichooser);

	if (gd->ilayout != NULL)
		IExec->DropInterface((struct Interface *)gd->ilayout);

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
	nb.nb_Name    = PROGNAME;
	nb.nb_Title   = VERS;
	nb.nb_Descr   = GetString(gd->locale_info, MSG_DESCRIPTION);
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

	/* Set to NULL for empty strings */
	if (gd->popkey != NULL && gd->popkey[0] == '\0')
		gd->popkey = NULL;
	if (gd->recordkey != NULL && gd->recordkey[0] == '\0')
		gd->recordkey = NULL;
	if (gd->stopkey != NULL && gd->stopkey[0] == '\0')
		gd->stopkey = NULL;

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

	gd->app_id = IApplication->RegisterApplication(PROGNAME,
		REGAPP_URLIdentifier,     "a500.org",
		REGAPP_Description,       GetString(gd->locale_info, MSG_DESCRIPTION),
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

static inline BOOL gui_is_recording(const struct srec_gui *gd) {
	return (gd->srec_pid != 0) ? TRUE : FALSE;
}

struct chooser_map {
	uint32 cfg_val;
	CONST_STRPTR cfg_str;
	int32 msg_id;
};

static void gui_free_chooser_list(const struct srec_gui *gd, struct List *list) {
	struct ChooserIFace *IChooser = gd->ichooser;
	struct Node *node;

	if (list != NULL) {
		while ((node = IExec->RemHead(list)) != NULL)
			IChooser->FreeChooserNode(node);

		IExec->FreeSysObject(ASOT_LIST, list);
	}
}

static struct List *gui_create_chooser_list(const struct srec_gui *gd,
	const struct chooser_map *map, uint32 len)
{
	struct ChooserIFace *IChooser = gd->ichooser;
	struct LocaleInfo *loc = gd->locale_info;
	struct List *list;
	struct Node *node;
	uint32 i;

	list = IExec->AllocSysObject(ASOT_LIST, NULL);
	if (list != NULL) {
		for (i = 0; i != len; i++) {
			node = IChooser->AllocChooserNode(
				CNA_Text,     GetString(loc, map[i].msg_id),
				CNA_UserData, &map[i],
				TAG_END);
			if (node == NULL) {
				gui_free_chooser_list(gd, list);
				return NULL;
			}

			IExec->AddTail(list, node);
		}
	}

	return list;
}

static uint32 gui_map_cfg_str_to_cfg_val(const struct srec_gui *gd, CONST_STRPTR str,
	const struct chooser_map *map, uint32 len, uint32 def)
{
	uint32 i;

	if (str != NULL) {
		for (i = 0; i != len; i++) {
			if (strcmp(map[i].cfg_str, str) == 0)
				return map[i].cfg_val;
		}
	}

	return def;
}

static uint32 gui_map_find_closest_cfg_val(const struct srec_gui *gd, uint32 val,
	const struct chooser_map *map, uint32 len)
{
	uint32 closest_diff = UINT32_MAX;
	uint32 closest_val  = map[0].cfg_val;
	uint32 diff;
	uint32 i;

	for (i = 0; i != len; i++) {
		diff = ABS((int32)map[i].cfg_val - (int32)val);

		if (diff == 0)
			return val;

		if (diff < closest_diff) {
			closest_diff = diff;
			closest_val  = map[i].cfg_val;
		}
	}

	return closest_val;
}

static uint32 gui_map_cfg_val_to_chooser_index(const struct srec_gui *gd, uint32 val,
	const struct chooser_map *map, uint32 len)
{
	uint32 i;

	for (i = 0; i != len; i++) {
		if (map[i].cfg_val == val)
			return i;
	}

	/* Should never happen */
	return 0;
}

const struct chooser_map container_map[] = {
	{ CONTAINER_AVI, "AVI", MSG_CONTAINER_FORMAT_AVI },
	{ CONTAINER_MKV, "MKV", MSG_CONTAINER_FORMAT_MKV }
};

const struct chooser_map video_codec_map[] = {
	{ VIDEO_CODEC_ZMBV, "ZMBV", MSG_VIDEO_CODEC_ZMBV }
};

enum {
	ASPECT_RATIO_LIKE_WB = 0,
	ASPECT_RATIO_CUSTOM,
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_5_4,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_16_10
};

#define DEFAULT_ASPECT_RATIO ASPECT_RATIO_LIKE_WB

static const struct chooser_map aspect_ratio_map[] = {
	{ ASPECT_RATIO_LIKE_WB, "LIKEWB", MSG_ASPECT_RATIO_LIKE_WB },
	{ ASPECT_RATIO_CUSTOM,  "CUSTOM", MSG_ASPECT_RATIO_CUSTOM  },
	{ ASPECT_RATIO_4_3,       "4:3",  MSG_ASPECT_RATIO_4_3     },
	{ ASPECT_RATIO_5_4,       "5:4",  MSG_ASPECT_RATIO_5_4     },
	{ ASPECT_RATIO_16_9,     "16:9",  MSG_ASPECT_RATIO_16_9    },
	{ ASPECT_RATIO_16_10,   "16:10",  MSG_ASPECT_RATIO_16_10   }
};

static const struct chooser_map audio_codec_map[] = {
	{ AUDIO_CODEC_NONE, "", MSG_AUDIO_CODEC_NONE }
};

static const struct chooser_map sample_size_map[] = {
	{  8, NULL, MSG_AUDIO_SAMPLE_SIZE_8BIT  },
	{ 16, NULL, MSG_AUDIO_SAMPLE_SIZE_16BIT },
	{ 32, NULL, MSG_AUDIO_SAMPLE_SIZE_32BIT }
};

static const struct chooser_map channels_map[] = {
	{ CHANNELS_MONO,   "MONO",   MSG_AUDIO_CHANNELS_MONO   },
	{ CHANNELS_STEREO, "STEREO", MSG_AUDIO_CHANNELS_STEREO }
};

static const struct chooser_map sample_rate_map[] = {
	{ 11025, NULL, MSG_AUDIO_SAMPLE_RATE_11025HZ },
	{ 22050, NULL, MSG_AUDIO_SAMPLE_RATE_22050HZ },
	{ 44100, NULL, MSG_AUDIO_SAMPLE_RATE_44100HZ },
	{ 48000, NULL, MSG_AUDIO_SAMPLE_RATE_48000HZ }
};

static void gui_read_settings(struct srec_gui *gd) {
	struct PrefsObjectsIFace *IPrefsObjects = gd->iprefsobjects;
	CONST_STRPTR cfg_str;
	uint32 cfg_val;

	gd->output_file = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "OutputFile", DEFAULT_OUTPUT_FILE);
	if (gd->output_file[0] == '\0')
		gd->output_file = DEFAULT_OUTPUT_FILE;

	cfg_str = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "ContainerFormat", NULL);
	gd->container = gui_map_cfg_str_to_cfg_val(gd, cfg_str, container_map, ARRAY_LEN(container_map), DEFAULT_CONTAINER);

	cfg_str = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "VideoCodec", NULL);
	gd->video_codec = gui_map_cfg_str_to_cfg_val(gd, cfg_str, video_codec_map, ARRAY_LEN(video_codec_map), DEFAULT_VIDEO_CODEC);

	cfg_str = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "AspectRatio", NULL);
	gd->aspect_ratio = gui_map_cfg_str_to_cfg_val(gd, cfg_str, aspect_ratio_map, ARRAY_LEN(aspect_ratio_map), DEFAULT_ASPECT_RATIO);

	gd->width  = CLAMP(IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoWidth", DEFAULT_WIDTH), MIN_WIDTH, MAX_WIDTH);
	gd->height = CLAMP(IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoHeight", DEFAULT_HEIGHT), MIN_HEIGHT, MAX_HEIGHT);
	gd->fps    = CLAMP(IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "VideoFPS", DEFAULT_FPS), MIN_FPS, MAX_FPS);

	cfg_str = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "AudioCodec", NULL);
	gd->audio_codec = gui_map_cfg_str_to_cfg_val(gd, cfg_str, audio_codec_map, ARRAY_LEN(audio_codec_map), DEFAULT_AUDIO_CODEC);

	cfg_val = IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AudioSampleSize", DEFAULT_SAMPLE_SIZE);
	gd->sample_size = gui_map_find_closest_cfg_val(gd, cfg_val, sample_size_map, ARRAY_LEN(sample_size_map));

	cfg_str = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "AudioChannels", NULL);
	gd->channels = gui_map_cfg_str_to_cfg_val(gd, cfg_str, channels_map, ARRAY_LEN(channels_map), DEFAULT_CHANNELS);

	cfg_val = IPrefsObjects->DictGetIntegerForKey(gd->app_prefs, "AudioSampleRate", DEFAULT_SAMPLE_RATE);
	gd->sample_rate = gui_map_find_closest_cfg_val(gd, cfg_val, sample_rate_map, ARRAY_LEN(sample_rate_map));

	gd->enable_pointer = IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "EnablePointer", TRUE);

	gd->pointer_file      = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "PointerFile", DEFAULT_POINTER_FILE);
	gd->busy_pointer_file = IPrefsObjects->DictGetStringForKey(gd->app_prefs, "BusyPointerFile", DEFAULT_BUSY_POINTER_FILE);

	gd->enable_filter  = IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "BilinearFilter", TRUE);
	gd->enable_altivec = IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "EnableAltivec", FALSE);
	gd->create_icon    = IPrefsObjects->DictGetBoolForKey(gd->app_prefs, "CreateIcon", TRUE);
}

static VARARGS68K void gui_set_gadget_attrs(const struct srec_gui *gd, uint32 id, ...) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct Gadget *gadget = (struct Gadget *)gd->obj[id];
	struct Window *window;
	struct TagItem *tags;
	uint32 temp;
	va_list ap;

	IIntuition->GetAttr(WINDOW_Window, gd->obj[OID_WINDOW], &temp);
	window = (struct Window *)temp;

	va_startlinear(ap, id);
	tags = va_getlinearva(ap, struct TagItem *);

	if (id > OID_TAB_PAGES && id < OID_RECORD_STOP_LAYOUT) {
		struct LayoutIFace *ILayout = gd->ilayout;
		ILayout->SetPageGadgetAttrsA(gadget, gd->obj[OID_TAB_PAGES], window, NULL, tags);
	} else {
		IIntuition->SetGadgetAttrsA(gadget, window, NULL, tags);
	}

	va_end(ap);
}

static BOOL gui_get_wb_dimensions(const struct srec_gui *gd, uint32 *widthp, uint32 *heightp) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct Screen *screen;

	screen = IIntuition->LockPubScreen(NULL);
	if (screen == NULL)
		return FALSE;

	get_screen_dimensions(gd->igraphics, screen, widthp, heightp);

	IIntuition->UnlockPubScreen(NULL, screen);

	return TRUE;
}

static void gui_enforce_aspect_ratio(struct srec_gui *gd, BOOL change_height) {
	uint32 width  = gd->width;
	uint32 height = gd->height;
	uint32 wb_w, wb_h;

	if (gd->aspect_ratio == ASPECT_RATIO_CUSTOM) {
		/* Do nothing */
		return;
	}

	if (change_height) {
		switch (gd->aspect_ratio) {
			case ASPECT_RATIO_LIKE_WB:
				if (gui_get_wb_dimensions(gd, &wb_w, &wb_h))
					height = width * wb_h / wb_w;
				break;
			case ASPECT_RATIO_4_3:
				height = width * 3 / 4;
				break;
			case ASPECT_RATIO_5_4:
				height = width * 4 / 5;
				break;
			case ASPECT_RATIO_16_9:
				height = width * 9 / 16;
				break;
			case ASPECT_RATIO_16_10:
				height = width * 10 / 16;
				break;
		}
		gui_set_gadget_attrs(gd, OID_VIDEO_HEIGHT,
			INTEGER_Number, height,
			TAG_END);
		gd->height = CLAMP(height, MIN_HEIGHT, MAX_HEIGHT);
	} else {
		switch (gd->aspect_ratio) {
			case ASPECT_RATIO_LIKE_WB:
				if (gui_get_wb_dimensions(gd, &wb_w, &wb_h))
					width = height * wb_w / wb_h;
				break;
			case ASPECT_RATIO_4_3:
				width = height * 4 / 3;
				break;
			case ASPECT_RATIO_5_4:
				width = height * 5 / 4;
				break;
			case ASPECT_RATIO_16_9:
				width = height * 16 / 9;
				break;
			case ASPECT_RATIO_16_10:
				width = height * 16 / 10;
				break;
		}
		gui_set_gadget_attrs(gd, OID_VIDEO_WIDTH,
			INTEGER_Number, width,
			TAG_END);
		gd->width = CLAMP(width, MIN_WIDTH, MAX_WIDTH);
	}
}

static void gui_update_audio_gadgets(struct srec_gui *gd) {
	BOOL codec_is_none;

	codec_is_none = (gd->audio_codec == AUDIO_CODEC_NONE) ? TRUE : FALSE;

	gui_set_gadget_attrs(gd, OID_AUDIO_SAMPLE_SIZE,
		GA_Disabled, codec_is_none,
		TAG_END);

	gui_set_gadget_attrs(gd, OID_AUDIO_CHANNELS,
		GA_Disabled, codec_is_none,
		TAG_END);

	gui_set_gadget_attrs(gd, OID_AUDIO_SAMPLE_RATE,
		GA_Disabled, codec_is_none,
		TAG_END);
}

static void gui_update_pointer_gadgets(struct srec_gui *gd) {
	BOOL pointer_disabled = !gd->enable_pointer;

	gui_set_gadget_attrs(gd, OID_POINTER_FILE,
		GA_Disabled, pointer_disabled,
		TAG_END);

	gui_set_gadget_attrs(gd, OID_BUSY_POINTER_FILE,
		GA_Disabled, pointer_disabled,
		TAG_END);
}

static void gui_update_record_stop_buttons(struct srec_gui *gd) {
	BOOL is_recording;

	is_recording = gui_is_recording(gd);

	gui_set_gadget_attrs(gd, OID_RECORD,
		GA_Disabled, is_recording,
		TAG_END);

	gui_set_gadget_attrs(gd, OID_STOP,
		GA_Disabled, !is_recording,
		TAG_END);
}

static BOOL gui_create_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct LocaleInfo *loc = gd->locale_info;
	CONST_STRPTR tab_titles[5];
	BOOL result;
	BOOL error;

	IUtility->SNPrintf(gd->window_title, sizeof(gd->window_title), GetString(loc, MSG_WINDOW_TITLE),
		PROGNAME, gd->popkey ? gd->popkey : GetString(loc, MSG_KEY_NONE));

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

	error = 0;
	error |= !(gd->container_list = gui_create_chooser_list(gd, container_map, ARRAY_LEN(container_map)));
	error |= !(gd->video_codec_list = gui_create_chooser_list(gd, video_codec_map, ARRAY_LEN(video_codec_map)));
	error |= !(gd->aspect_ratio_list = gui_create_chooser_list(gd, aspect_ratio_map, ARRAY_LEN(aspect_ratio_map)));
	error |= !(gd->audio_codec_list = gui_create_chooser_list(gd, audio_codec_map, ARRAY_LEN(audio_codec_map)));
	error |= !(gd->sample_size_list = gui_create_chooser_list(gd, sample_size_map, ARRAY_LEN(sample_size_map)));
	error |= !(gd->channels_list = gui_create_chooser_list(gd, channels_map, ARRAY_LEN(channels_map)));
	error |= !(gd->sample_rate_list = gui_create_chooser_list(gd, sample_rate_map, ARRAY_LEN(sample_rate_map)));
	if (error != 0)
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
		GETFILE_FullFile,   gd->output_file,
		TAG_END);

	gd->obj[OID_CONTAINER_FORMAT] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_CONTAINER_FORMAT,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->container_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->container, container_map, ARRAY_LEN(container_map)),
		TAG_END);

	gd->obj[OID_VIDEO_CODEC] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_VIDEO_CODEC,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->video_codec_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->video_codec, video_codec_map, ARRAY_LEN(video_codec_map)),
		TAG_END);

	gd->obj[OID_ASPECT_RATIO] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_ASPECT_RATIO,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->aspect_ratio_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->aspect_ratio, aspect_ratio_map, ARRAY_LEN(aspect_ratio_map)),
		TAG_END);

	gd->obj[OID_VIDEO_WIDTH] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,                    OID_VIDEO_WIDTH,
		GA_TabCycle,              TRUE,
		GA_RelVerify,             TRUE,
		STRINGA_RelVerifySpecial, TRUE,
		INTEGER_Minimum,          MIN_WIDTH,
		INTEGER_Maximum,          MAX_WIDTH,
		INTEGER_Number,           gd->width,
		TAG_END);

	gd->obj[OID_VIDEO_HEIGHT] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,                    OID_VIDEO_HEIGHT,
		GA_TabCycle,              TRUE,
		GA_RelVerify,             TRUE,
		STRINGA_RelVerifySpecial, TRUE,
		INTEGER_Minimum,          MIN_HEIGHT,
		INTEGER_Maximum,          MAX_HEIGHT,
		INTEGER_Number,           gd->height,
		TAG_END);

	gd->obj[OID_VIDEO_WIDTH_HEIGHT_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,           OID_VIDEO_WIDTH_HEIGHT_LAYOUT,
		LAYOUT_AddChild, gd->obj[OID_VIDEO_WIDTH],
		CHILD_Label,     LABEL(MSG_VIDEO_WIDTH_GAD),
		LAYOUT_AddChild, gd->obj[OID_VIDEO_HEIGHT],
		CHILD_Label,     LABEL(MSG_VIDEO_HEIGHT_GAD),
		TAG_END);

	gd->obj[OID_VIDEO_FPS] = IIntuition->NewObject(gd->integerclass, NULL,
		GA_ID,                    OID_VIDEO_FPS,
		GA_TabCycle,              TRUE,
		GA_RelVerify,             TRUE,
		STRINGA_RelVerifySpecial, TRUE,
		INTEGER_Minimum,          MIN_FPS,
		INTEGER_Maximum,          MAX_FPS,
		INTEGER_Number,           gd->fps,
		TAG_END);

	gd->obj[OID_VIDEO_FPS_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,           OID_VIDEO_FPS_LAYOUT,
		LAYOUT_AddChild, SPACE,
		LAYOUT_AddChild, gd->obj[OID_VIDEO_FPS],
		CHILD_Label,     LABEL(MSG_VIDEO_FPS_GAD),
		TAG_END);

	gd->obj[OID_AUDIO_CODEC] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_AUDIO_CODEC,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->audio_codec_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->audio_codec, audio_codec_map, ARRAY_LEN(audio_codec_map)),
		TAG_END);

	gd->obj[OID_AUDIO_SAMPLE_SIZE] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_AUDIO_SAMPLE_SIZE,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->sample_size_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->sample_size, sample_size_map, ARRAY_LEN(sample_size_map)),
		TAG_END);

	gd->obj[OID_AUDIO_CHANNELS] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_AUDIO_CHANNELS,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->channels_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->channels, channels_map, ARRAY_LEN(channels_map)),
		TAG_END);

	gd->obj[OID_AUDIO_SAMPLE_RATE] = IIntuition->NewObject(gd->chooserclass, NULL,
		GA_ID,            OID_AUDIO_SAMPLE_RATE,
		GA_RelVerify,     TRUE,
		CHOOSER_Labels,   gd->sample_rate_list,
		CHOOSER_Selected, gui_map_cfg_val_to_chooser_index(gd, gd->sample_rate, sample_rate_map, ARRAY_LEN(sample_rate_map)),
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
		CHECKBOX_Checked, gd->enable_pointer,
		TAG_END);

	gd->obj[OID_POINTER_FILE] = IIntuition->NewObject(gd->getfileclass, NULL,
		GA_ID,              OID_POINTER_FILE,
		GA_TabCycle,        TRUE,
		GA_RelVerify,       TRUE,
		GETFILE_Pattern,    "#?.info",
		GETFILE_DoSaveMode, FALSE,
		GETFILE_FullFile,   gd->pointer_file,
		TAG_END);

	gd->obj[OID_POINTER_FILE_LAYOUT] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,             OID_POINTER_FILE_LAYOUT,
		LAYOUT_Label,      GetString(loc, MSG_POINTER_FILE_GAD),
		LAYOUT_BevelStyle, BVS_GROUP,
		LAYOUT_AddChild,   gd->obj[OID_POINTER_FILE],
		TAG_END);

	gd->obj[OID_BUSY_POINTER_FILE] = IIntuition->NewObject(gd->getfileclass, NULL,
		GA_ID,              OID_BUSY_POINTER_FILE,
		GA_TabCycle,        TRUE,
		GA_RelVerify,       TRUE,
		GETFILE_Pattern,    "#?.info",
		GETFILE_DoSaveMode, FALSE,
		GETFILE_FullFile,   gd->busy_pointer_file,
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

	gd->obj[OID_POPUP_KEY] = IIntuition->NewObject(gd->buttonclass, NULL,
		GA_ID,         OID_POPUP_KEY,
		GA_ReadOnly,   TRUE,
		GA_Underscore, 0,
		GA_Text,       gd->popkey ? gd->popkey : GetString(loc, MSG_KEY_NONE),
		TAG_END);

	gd->obj[OID_RECORD_KEY] = IIntuition->NewObject(gd->buttonclass, NULL,
		GA_ID,         OID_RECORD_KEY,
		GA_ReadOnly,   TRUE,
		GA_Underscore, 0,
		GA_Text,       gd->recordkey ? gd->recordkey : GetString(loc, MSG_KEY_NONE),
		TAG_END);

	gd->obj[OID_STOP_KEY] = IIntuition->NewObject(gd->buttonclass, NULL,
		GA_ID,         OID_STOP_KEY,
		GA_ReadOnly,   TRUE,
		GA_Underscore, 0,
		GA_Text,       gd->stopkey ? gd->stopkey : GetString(loc, MSG_KEY_NONE),
		TAG_END);

	gd->obj[OID_HOT_KEYS_PAGE] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,                OID_HOT_KEYS_PAGE,
		LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,      gd->obj[OID_POPUP_KEY],
		CHILD_Label,          LABEL(MSG_POPUP_KEY_GAD),
		CHILD_WeightedHeight, 0,
		LAYOUT_AddChild,      gd->obj[OID_RECORD_KEY],
		CHILD_Label,          LABEL(MSG_RECORD_KEY_GAD),
		CHILD_WeightedHeight, 0,
		LAYOUT_AddChild,      gd->obj[OID_STOP_KEY],
		CHILD_Label,          LABEL(MSG_STOP_KEY_GAD),
		CHILD_WeightedHeight, 0,
		LAYOUT_AddChild,      SPACE,
		TAG_END);

	gd->obj[OID_BILINEAR_FILTER] = IIntuition->NewObject(gd->checkboxclass, NULL,
		GA_ID,            OID_BILINEAR_FILTER,
		GA_RelVerify,     TRUE,
		CHECKBOX_Checked, gd->enable_filter,
		TAG_END);

	gd->obj[OID_DISABLE_ALTIVEC] = IIntuition->NewObject(gd->checkboxclass, NULL,
		GA_ID,            OID_DISABLE_ALTIVEC,
		GA_RelVerify,     TRUE,
		CHECKBOX_Checked, !gd->enable_altivec,
		TAG_END);

	gd->obj[OID_CREATE_ICON] = IIntuition->NewObject(gd->checkboxclass, NULL,
		GA_ID,            OID_CREATE_ICON,
		GA_RelVerify,     TRUE,
		CHECKBOX_Checked, gd->create_icon,
		TAG_END);

	gd->obj[OID_MISC_PAGE] = IIntuition->NewObject(gd->layoutclass, NULL,
		GA_ID,              OID_MISC_PAGE,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_AddChild,    gd->obj[OID_BILINEAR_FILTER],
		CHILD_Label,        LABEL(MSG_BILINEAR_FILTER_GAD),
		LAYOUT_AddChild,    gd->obj[OID_DISABLE_ALTIVEC],
		CHILD_Label,        LABEL(MSG_DISABLE_ALTIVEC_GAD),
		LAYOUT_AddChild,    gd->obj[OID_CREATE_ICON],
		CHILD_Label,        LABEL(MSG_CREATE_ICON_GAD),
		LAYOUT_AddChild,    SPACE,
		TAG_END);

	gd->obj[OID_TAB_PAGES] = IIntuition->NewObject(NULL, "page.gadget",
		GA_ID,        OID_TAB_PAGES,
		PAGE_Add,     gd->obj[OID_FORMAT_PAGE],
		PAGE_Add,     gd->obj[OID_POINTER_PAGE],
		PAGE_Add,     gd->obj[OID_HOT_KEYS_PAGE],
		PAGE_Add,     gd->obj[OID_MISC_PAGE],
		PAGE_Current, 0,
		TAG_END);

	tab_titles[0] = GetString(loc, MSG_FORMAT_TAB);
	tab_titles[1] = GetString(loc, MSG_POINTER_TAB);
	tab_titles[2] = GetString(loc, MSG_HOT_KEYS_TAB);
	tab_titles[3] = GetString(loc, MSG_MISC_TAB);
	tab_titles[4] = NULL;

	gd->obj[OID_TABS] = IIntuition->NewObject(gd->clicktabclass, NULL,
		GA_ID,              OID_TABS,
		GA_RelVerify,       TRUE,
		GA_Text,            tab_titles,
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
		WA_Title,               gd->window_title,
		WA_ScreenTitle,         VERS " (" DATE ")",
		WA_CloseGadget,         TRUE,
		WA_DragBar,             TRUE,
		WA_DepthGadget,         TRUE,
		WA_Activate,            TRUE,
		WA_NoCareRefresh,       TRUE,
		WA_IDCMP,               IDCMP_GADGETUP | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW,
		WINDOW_UniqueID,        "main",
		WINDOW_CharSet,         loc->li_CodeSet,
		WINDOW_Position,        WPOS_TOPLEFT,
		WINDOW_MenuStrip,       gd->obj[OID_MENUSTRIP],
		WINDOW_AppPort,         gd->wb_mp,
		WINDOW_IconifyGadget,   TRUE,
		WINDOW_IconTitle,       PROGNAME,
		WINDOW_Icon,            gd->icon,
		WINDOW_IconNoDispose,   TRUE,
		WINDOW_PopupGadget,     TRUE,
		WINDOW_JumpScreensMenu, TRUE,
		WINDOW_Layout,          gd->obj[OID_ROOT_LAYOUT],
		TAG_END);

	if (gd->obj[OID_TAB_PAGES] == NULL || gd->obj[OID_WINDOW] == NULL)
		return FALSE;

	if (gd->vector_unit != VECTORTYPE_ALTIVEC) {
		gui_set_gadget_attrs(gd, OID_DISABLE_ALTIVEC,
			GA_Disabled, TRUE,
			TAG_END);
	}

	gui_enforce_aspect_ratio(gd, TRUE);
	gui_update_audio_gadgets(gd);
	gui_update_pointer_gadgets(gd);
	gui_update_record_stop_buttons(gd);

	return TRUE;
}

static void gui_free_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;

	if (gd->obj[OID_WINDOW] != NULL)
		IIntuition->DisposeObject(gd->obj[OID_WINDOW]);

	gui_free_chooser_list(gd, gd->container_list);
	gui_free_chooser_list(gd, gd->video_codec_list);
	gui_free_chooser_list(gd, gd->aspect_ratio_list);
	gui_free_chooser_list(gd, gd->audio_codec_list);
	gui_free_chooser_list(gd, gd->sample_size_list);
	gui_free_chooser_list(gd, gd->channels_list);
	gui_free_chooser_list(gd, gd->sample_rate_list);

	if (gd->wb_mp != NULL)
		IExec->FreeSysObject(ASOT_PORT, gd->wb_mp);

	gui_free_menu(gd);
}

static BOOL gui_show_window(struct srec_gui *gd, BOOL screen_to_front) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct ApplicationIFace *IApplication = gd->iapplication;
	struct Window *window;

	window = (struct Window *)IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_OPEN, NULL);
	if (window == NULL)
		return FALSE;

	IApplication->SetApplicationAttrs(gd->app_id,
		APPATTR_Hidden, FALSE,
		TAG_END);

	if (screen_to_front)
		IIntuition->ScreenToFront(window->WScreen);

	return TRUE;
}

static BOOL gui_hide_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct ApplicationIFace *IApplication = gd->iapplication;
	uint32 success;

	success = IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_CLOSE, NULL);
	if (!success)
		return FALSE;

	IApplication->SetApplicationAttrs(gd->app_id,
		APPATTR_Hidden, TRUE,
		TAG_END);

	return TRUE;
}

static BOOL gui_iconify_window(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct ApplicationIFace *IApplication = gd->iapplication;
	uint32 success;

	success = IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_ICONIFY, NULL);
	if (!success)
		return FALSE;

	IApplication->SetApplicationAttrs(gd->app_id,
		APPATTR_Hidden, TRUE,
		TAG_END);

	return TRUE;
}

static BOOL gui_jump_screen(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct Screen *new_screen;
	uint32 temp;

	IIntuition->GetAttr(WA_PubScreen, gd->obj[OID_WINDOW], &temp);
	new_screen = (struct Screen *)temp;

	if (new_screen == NULL)
		return FALSE;

	if (!gui_hide_window(gd))
		return FALSE;

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_PubScreen, new_screen,
		TAG_END);

	if (!gui_show_window(gd, TRUE))
		return FALSE;

	return TRUE;
}

static void gui_about_requester(const struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct LocaleInfo *loc = gd->locale_info;
	struct Window *window;
	Object *requester;
	TEXT requester_title[64];
	TEXT body_text[1024];
	uint32 temp;

	IIntuition->GetAttr(WINDOW_Window, gd->obj[OID_WINDOW], &temp);
	window = (struct Window *)temp;

	IUtility->SNPrintf(requester_title, sizeof(requester_title),
		GetString(loc, MSG_ABOUT_WINDOW_TITLE), PROGNAME);

	IUtility->SNPrintf(body_text, sizeof(body_text),
		"%s %s %lu.%lu (%s)\n\n"
		"%s (C) 2016 Fredrik Wikstrom <fredrik@a500.org>\n\n"
		"%s",
		PROGNAME, GetString(loc, MSG_VERSION), VERSION, REVISION, DATE,
		GetString(loc, MSG_COPYRIGHT), gpl_license);

	requester = IIntuition->NewObject(gd->requesterclass, NULL,
		REQ_Image,      REQIMAGE_INFO,
		REQ_CharSet,    loc->li_CodeSet,
		REQ_TitleText,  requester_title,
		REQ_BodyText,   body_text,
		REQ_GadgetText, GetString(loc, MSG_OK_GAD),
		TAG_END);

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, TRUE,
		TAG_END);

	if (requester != NULL) {
		IIntuition->IDoMethod(requester, RM_OPENREQ, NULL, window, NULL);

		IIntuition->DisposeObject(requester);
	}

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, FALSE,
		TAG_END);
}

static void gui_getfile_requester(const struct srec_gui *gd, uint32 id) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct Window *window;
	uint32 temp;

	IIntuition->GetAttr(WINDOW_Window, gd->obj[OID_WINDOW], &temp);
	window = (struct Window *)temp;
	if (window == NULL) {
		/* Do nothing */
		return;
	}

	IIntuition->IDoMethod(gd->obj[id], GFILE_REQUEST, window);
}

static void gui_notify(const struct srec_gui *gd, int32 msg_id) {
	struct ApplicationIFace *IApplication = gd->iapplication;
	struct LocaleInfo *loc = gd->locale_info;

	IApplication->Notify(gd->app_id,
		APPNOTIFY_Title,         PROGNAME,
		APPNOTIFY_Update,        TRUE,
		APPNOTIFY_PubScreenName, "FRONT",
		APPNOTIFY_Text,          GetString(loc, msg_id),
		TAG_END);
}

static void gui_start_recording(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct SRecArgs *srec_args = gd->srec_args;
	CONST_STRPTR output_file;
	CONST_STRPTR pointer_file;
	CONST_STRPTR busy_pointer_file;
	struct Process *proc;
	uint32 temp;

	if (gui_is_recording(gd))
		return;

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, TRUE,
		TAG_END);

	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_OUTPUT_FILE], &temp);
	output_file = (CONST_STRPTR)temp;
	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_POINTER_FILE], &temp);
	pointer_file = (CONST_STRPTR)temp;
	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_BUSY_POINTER_FILE], &temp);
	busy_pointer_file = (CONST_STRPTR)temp;

	IUtility->Strlcpy(srec_args->output_file, output_file, sizeof(srec_args->output_file));

	IUtility->Strlcpy(srec_args->pointer_file, pointer_file, sizeof(srec_args->pointer_file));
	IUtility->Strlcpy(srec_args->busy_pointer_file, busy_pointer_file, sizeof(srec_args->busy_pointer_file));

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, FALSE,
		TAG_END);

	strip_info_extension(srec_args->pointer_file);
	strip_info_extension(srec_args->busy_pointer_file);

	srec_args->container   = gd->container;
	srec_args->video_codec = gd->video_codec;
	srec_args->width       = gd->width;
	srec_args->height      = gd->height;
	srec_args->fps         = gd->fps;
	srec_args->audio_codec = gd->audio_codec;
	srec_args->sample_size = gd->sample_size;
	srec_args->channels    = gd->channels;
	srec_args->sample_rate = gd->sample_rate;
	srec_args->no_filter   = !gd->enable_filter;
	srec_args->no_pointer  = !gd->enable_pointer;
	srec_args->no_altivec  = !gd->enable_altivec;
	srec_args->create_icon = gd->create_icon;

	proc = IDOS->CreateNewProcTags(
		NP_Name,                 SREC_PROCNAME,
		NP_Priority,             SREC_PRIORITY,
		NP_Child,                TRUE,
		NP_Entry,                srec_entry,
		NP_StackSize,            SREC_STACKSIZE,
		NP_LockStack,            TRUE,
		NP_UserData,             gd->srec_args,
		NP_NotifyOnDeathMessage, gd->death_msg,
		TAG_END);
	if (proc == NULL) {
		//FIXME: Add error requester
		return;
	}

	/* On success IoErr() returns the PID */
	gd->srec_pid = IDOS->IoErr();

	gui_notify(gd, MSG_RECORDING_STARTED);

	gui_update_record_stop_buttons(gd);
}

static void gui_stop_recording(struct srec_gui *gd) {
	if (gui_is_recording(gd)) {
		safe_signal_proc(gd->srec_pid, SIGBREAKF_CTRL_C);
	}
}

static void gui_srec_error_requester(const struct srec_gui *gd, const struct DeathMessage *dm) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct LocaleInfo *loc = gd->locale_info;
	struct Window *window;
	Object *requester;
	TEXT requester_title[64];
	TEXT body_text[128];
	uint32 temp;

	IIntuition->GetAttr(WINDOW_Window, gd->obj[OID_WINDOW], &temp);
	window = (struct Window *)temp;

	IUtility->SNPrintf(requester_title, sizeof(requester_title),
		GetString(loc, MSG_ERROR_WINDOW_TITLE), PROGNAME);

	IUtility->SNPrintf(body_text, sizeof(body_text),
		"Screen Recorder process failed!\n\n"
		"Return code: %ld Error: %ld",
		dm->dm_ReturnCode, dm->dm_Result2);

	requester = IIntuition->NewObject(gd->requesterclass, NULL,
		REQ_Image,      REQIMAGE_ERROR,
		REQ_CharSet,    loc->li_CodeSet,
		REQ_TitleText,  requester_title,
		REQ_BodyText,   body_text,
		REQ_GadgetText, GetString(loc, MSG_OK_GAD),
		TAG_END);

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, TRUE,
		TAG_END);

	if (requester != NULL) {
		IIntuition->IDoMethod(requester, RM_OPENREQ, NULL, window, NULL);

		IIntuition->DisposeObject(requester);
	}

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, FALSE,
		TAG_END);
}

static void gui_save_settings(struct srec_gui *gd) {
	struct IntuitionIFace *IIntuition = gd->iintuition;
	struct PrefsObjectsIFace *IPrefsObjects = gd->iprefsobjects;
	CONST_STRPTR output_file;
	CONST_STRPTR pointer_file;
	CONST_STRPTR busy_pointer_file;
	uint32 temp, index;
	PrefsObject *obj;

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, TRUE,
		TAG_END);

	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_OUTPUT_FILE], &temp);
	output_file = (CONST_STRPTR)temp;
	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_POINTER_FILE], &temp);
	pointer_file = (CONST_STRPTR)temp;
	IIntuition->GetAttr(GETFILE_FullFile, gd->obj[OID_BUSY_POINTER_FILE], &temp);
	busy_pointer_file = (CONST_STRPTR)temp;

	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,             NULL,
		ALPOSTR_AllocSetString, output_file,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "OutputFile");

	IIntuition->GetAttr(CHOOSER_Selected, gd->obj[OID_CONTAINER_FORMAT], &index);
	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,        NULL,
		ALPOSTR_SetString, container_map[index].cfg_str,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "ContainerFormat");

	IIntuition->GetAttr(CHOOSER_Selected, gd->obj[OID_VIDEO_CODEC], &index);
	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,        NULL,
		ALPOSTR_SetString, video_codec_map[index].cfg_str,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "VideoCodec");

	IIntuition->GetAttr(CHOOSER_Selected, gd->obj[OID_ASPECT_RATIO], &index);
	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,        NULL,
		ALPOSTR_SetString, aspect_ratio_map[index].cfg_str,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "AspectRatio");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetLong, gd->width,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "VideoWidth");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetLong, gd->height,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "VideoHeight");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetLong, gd->fps,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "VideoFPS");

	IIntuition->GetAttr(CHOOSER_Selected, gd->obj[OID_AUDIO_CODEC], &index);
	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,        NULL,
		ALPOSTR_SetString, audio_codec_map[index].cfg_str,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "AudioCodec");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetLong, gd->sample_size,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "AudioSampleSize");

	IIntuition->GetAttr(CHOOSER_Selected, gd->obj[OID_AUDIO_CHANNELS], &index);
	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,        NULL,
		ALPOSTR_SetString, channels_map[index].cfg_str,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "AudioChannels");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetLong, gd->sample_rate,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "AudioSampleRate");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetBool, gd->enable_pointer,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "EnablePointer");

	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,             NULL,
		ALPOSTR_AllocSetString, pointer_file,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "PointerFile");

	obj = IPrefsObjects->PrefsString(NULL, NULL,
		ALPO_Alloc,             NULL,
		ALPOSTR_AllocSetString, busy_pointer_file,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "BusyPointerFile");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetBool, gd->enable_filter,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "BilinearFilter");

	obj = IPrefsObjects->PrefsNumber(NULL, NULL,
		ALPO_Alloc,           NULL,
		ALPONUM_AllocSetBool, gd->enable_altivec,
		TAG_END);
	IPrefsObjects->DictSetObjectForKey(gd->app_prefs, obj, "EnableAltivec");

	IPrefsObjects->WritePrefs(gd->app_prefs,
		WRITEPREFS_AppID,       gd->app_id,
		WRITEPREFS_WriteENV,    TRUE,
		WRITEPREFS_WriteENVARC, TRUE,
		TAG_END);

	IIntuition->SetAttrs(gd->obj[OID_WINDOW],
		WA_BusyPointer, FALSE,
		TAG_END);
}

int gui_main(struct LocaleInfo *loc, struct WBStartup *wbs) {
	struct srec_gui *gd;
	struct IntuitionIFace *IIntuition;
	struct CommoditiesIFace *ICommodities;
	uint32 signals, cx_sig, app_sig, srec_sig, window_sigs;
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
	gd->vector_unit = VECTORTYPE_NONE;

	IExec->GetCPUInfoTags(
		GCIT_VectorUnit, &gd->vector_unit,
		TAG_END);

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

	gd->srec_mp = IExec->AllocSysObject(ASOT_PORT, NULL);
	if (gd->srec_mp == NULL)
		goto out;

	gd->srec_args = IExec->AllocVecTags(sizeof(*gd->srec_args),
		AVT_Type, MEMF_SHARED,
		TAG_END);
	if (gd->srec_args == NULL)
		goto out;

	gd->death_msg = IExec->AllocSysObjectTags(ASOT_MESSAGE,
		ASOMSG_Name,      PROGNAME " death message",
		ASOMSG_ReplyPort, gd->srec_mp,
		ASOMSG_Size,      sizeof(*gd->death_msg),
		TAG_END);
	if (gd->death_msg == NULL)
		goto out;

	gui_read_settings(gd);

	if (!gui_create_window(gd))
		goto out;

	if (!gd->hidden && !gui_show_window(gd, TRUE))
		goto out;

	IIntuition   = gd->iintuition;
	ICommodities = gd->icommodities;

	cx_sig   = 1UL << gd->broker_mp->mp_SigBit;
	app_sig  = 1UL << gd->app_mp->mp_SigBit;
	srec_sig = 1UL << gd->srec_mp->mp_SigBit;

	ICommodities->ActivateCxObj(gd->broker, TRUE);

	while (!done) {
		IIntuition->GetAttr(WINDOW_SigMask, gd->obj[OID_WINDOW], &window_sigs);
		signals = IExec->Wait(SIGBREAKF_CTRL_C | cx_sig | app_sig | srec_sig | window_sigs);

		if (signals & SIGBREAKF_CTRL_C)
			done = TRUE;

		if (signals & cx_sig) {
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
								gui_show_window(gd, TRUE);
								break;
							case EVT_RECORDKEY:
								if (gui_is_recording(gd) && gd->stopkey == NULL) {
									gui_stop_recording(gd);
									break;
								}
								gui_start_recording(gd);
								break;
							case EVT_STOPKEY:
								gui_stop_recording(gd);
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
								gui_show_window(gd, TRUE);
								break;
							case CXCMD_ENABLE:
								ICommodities->ActivateCxObj(gd->broker, TRUE);
								break;
							case CXCMD_DISABLE:
								ICommodities->ActivateCxObj(gd->broker, FALSE);
								break;
						}
						break;
				}
			}
		}

		if (signals & app_sig) {
			struct ApplicationMsg *msg;
			uint32 type;

			while ((msg = (struct ApplicationMsg *)IExec->GetMsg(gd->app_mp)) != NULL) {
				type = msg->type;

				IExec->ReplyMsg((struct Message *)msg);

				switch (type) {
					case APPLIBMT_Quit:
						done = TRUE;
						break;
					case APPLIBMT_Hide:
						gui_hide_window(gd);
						break;
					case APPLIBMT_Unhide:
						gui_show_window(gd, FALSE);
						break;
					case APPLIBMT_ToFront:
						gui_show_window(gd, TRUE);
						break;
				}
			}
		}

		if (signals & srec_sig) {
			struct DeathMessage *dm;

			dm = (struct DeathMessage *)IExec->GetMsg(gd->srec_mp);
			if (dm != NULL) {
				gd->srec_pid = 0;

				gui_notify(gd, MSG_RECORDING_STOPPED);

				gui_update_record_stop_buttons(gd);

				if (dm->dm_ReturnCode != RETURN_OK) {
					gui_srec_error_requester(gd, dm);
				}
			}
		}

		if (signals & window_sigs) {
			uint32 result;
			uint16 code;
			uint32 id;

			while ((result = IIntuition->IDoMethod(gd->obj[OID_WINDOW], WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
				switch (result & WMHI_CLASSMASK) {
					case WMHI_GADGETUP:
						id = result & WMHI_GADGETMASK;
						switch (id) {
							case OID_OUTPUT_FILE:
								gui_getfile_requester(gd, OID_OUTPUT_FILE);
								break;
							case OID_CONTAINER_FORMAT:
								gd->container = container_map[code].cfg_val;
								break;
							case OID_VIDEO_CODEC:
								gd->video_codec = video_codec_map[code].cfg_val;
								break;
							case OID_ASPECT_RATIO:
								gd->aspect_ratio = aspect_ratio_map[code].cfg_val;
								gui_enforce_aspect_ratio(gd, TRUE);
								break;
							case OID_VIDEO_WIDTH:
								gd->width = code;
								gui_enforce_aspect_ratio(gd, TRUE);
								break;
							case OID_VIDEO_HEIGHT:
								gd->height = code;
								gui_enforce_aspect_ratio(gd, FALSE);
								break;
							case OID_VIDEO_FPS:
								gd->fps = code;
								break;
							case OID_AUDIO_CODEC:
								gd->audio_codec = audio_codec_map[code].cfg_val;
								gui_update_audio_gadgets(gd);
								break;
							case OID_AUDIO_SAMPLE_SIZE:
								gd->sample_size = sample_size_map[code].cfg_val;
								break;
							case OID_AUDIO_CHANNELS:
								gd->channels = channels_map[code].cfg_val;
								break;
							case OID_AUDIO_SAMPLE_RATE:
								gd->sample_rate = sample_rate_map[code].cfg_val;
								break;
							case OID_ENABLE_POINTER:
								gd->enable_pointer = code ? TRUE : FALSE;
								gui_update_pointer_gadgets(gd);
								break;
							case OID_POINTER_FILE:
								gui_getfile_requester(gd, OID_POINTER_FILE);
								break;
							case OID_BUSY_POINTER_FILE:
								gui_getfile_requester(gd, OID_BUSY_POINTER_FILE);
								break;
							case OID_BILINEAR_FILTER:
								gd->enable_filter = code ? TRUE : FALSE;
								break;
							case OID_DISABLE_ALTIVEC:
								gd->enable_altivec = code ? FALSE : TRUE;
								break;
							case OID_CREATE_ICON:
								gd->create_icon = code ? TRUE : FALSE;
								break;
							case OID_RECORD:
								gui_start_recording(gd);
								break;
							case OID_STOP:
								gui_stop_recording(gd);
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
									gui_hide_window(gd);
									break;
								case MID_PROJECT_ICONIFY:
									gui_iconify_window(gd);
									break;
								case MID_PROJECT_QUIT:
									done = TRUE;
									break;
								case MID_SETTINGS_SAVE_AS_DEFAULTS:
									gui_save_settings(gd);
									break;
							}
						}
						break;

					case WMHI_JUMPSCREEN:
						gui_jump_screen(gd);
						break;

					case WMHI_ICONIFY:
						gui_iconify_window(gd);
						break;

					case WMHI_UNICONIFY:
						gui_show_window(gd, TRUE);
						break;

					case WMHI_CLOSEWINDOW:
						gui_hide_window(gd);
						break;
				}
			}
		}
	}

	rc = RETURN_OK;

out:

	if (gd != NULL) {
		if (gui_is_recording(gd)) {
			struct DeathMessage *dm;

			gui_stop_recording(gd);

			IExec->WaitPort(gd->srec_mp);
			dm = (struct DeathMessage *)IExec->GetMsg(gd->srec_mp);

			gd->srec_pid = 0;

			gui_notify(gd, MSG_RECORDING_STOPPED);

			gui_update_record_stop_buttons(gd);

			if (dm->dm_ReturnCode != RETURN_OK) {
				gui_srec_error_requester(gd, dm);
			}
		}

		gui_free_window(gd);

		if (gd->death_msg != NULL)
			IExec->FreeSysObject(ASOT_PORT, gd->death_msg);

		if (gd->srec_args != NULL)
			IExec->FreeVec(gd->srec_args);

		if (gd->srec_mp != NULL)
			IExec->FreeSysObject(ASOT_PORT, gd->srec_mp);

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

