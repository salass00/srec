#ifndef LOCALE_STRINGS_H
#define LOCALE_STRINGS_H

/* This file was created automatically by catcomp v1.4.
 * Do NOT edit by hand!
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifdef CATCOMP_ARRAY
#ifndef CATCOMP_NUMBERS
#define CATCOMP_NUMBERS
#endif
#ifndef CATCOMP_STRINGS
#define CATCOMP_STRINGS
#endif
#endif

#ifdef CATCOMP_BLOCK
#ifndef CATCOMP_STRINGS
#define CATCOMP_STRINGS
#endif
#endif

#ifdef CATCOMP_NUMBERS

#define MSG_TOO_OLD_OS_VERSION 100
#define MSG_CTRL_C_TO_STOP 101

#endif

#ifdef CATCOMP_STRINGS

#define MSG_TOO_OLD_OS_VERSION_STR "SRec %ld.%ld requires %s or newer!"
#define MSG_CTRL_C_TO_STOP_STR "CTRL-C to stop recording."

#endif

#ifdef CATCOMP_ARRAY

struct CatCompArrayType
{
	LONG         cca_ID;
	CONST_STRPTR cca_Str;
};

STATIC CONST struct CatCompArrayType CatCompArray[] =
{
	{MSG_TOO_OLD_OS_VERSION,(CONST_STRPTR)MSG_TOO_OLD_OS_VERSION_STR},
	{MSG_CTRL_C_TO_STOP,(CONST_STRPTR)MSG_CTRL_C_TO_STOP_STR},
};

#endif

#ifdef CATCOMP_BLOCK

STATIC CONST UBYTE CatCompBlock[] =
{
	"\x00\x00\x00\x64\x00\x24"
	MSG_TOO_OLD_OS_VERSION_STR "\x00\x00"
	"\x00\x00\x00\x65\x00\x1A"
	MSG_CTRL_C_TO_STOP_STR "\x00"
};

#endif

#endif
