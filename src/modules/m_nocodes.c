/*
 * "No Codes", a very simple (but often requested) module written by Syzop.
 * This module will strip messages with bold/underline/reverse if chanmode
 * +S is set, and block them if +c is set.
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

#define NOCODES_VERSION "v0.1"

ModuleHeader MOD_HEADER(m_nocodes)
  = {
	"m_nocodes",	/* Name of module */
	NOCODES_VERSION, /* Version */
	"Strip/block bold/underline/reverse", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

static ModuleInfo NoCodesModInfo;

static Hook *CheckMsg;

DLLFUNC char *nocodes_checkmsg(aClient *, aClient *, aChannel *, char *, int);

DLLFUNC int MOD_INIT(m_nocodes)(ModuleInfo *modinfo)
{
	bcopy(modinfo,&NoCodesModInfo,modinfo->size);
	CheckMsg = HookAddPCharEx(NoCodesModInfo.handle, HOOKTYPE_CHANMSG, nocodes_checkmsg);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_dummy)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_dummy)(int module_unload)
{
	HookDel(CheckMsg);
	return MOD_SUCCESS;
}

DLLFUNC char *nocodes_checkmsg(aClient *cptr, aClient *sptr, aChannel *chptr, char *text, int notice)
{
static char retbuf[4096];

	if (chptr->mode.mode & MODE_STRIP)
	{
		strncpyzt(retbuf, StripControlCodes(text), sizeof(retbuf));
		return retbuf;
	} else
	if (chptr->mode.mode & MODE_NOCOLOR)
	{
		if (strchr(text, '\002') || strchr(text, '\037') || strchr(text, '\026'))
		{
			sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
				me.name, sptr->name, sptr->name,
				"Control codes (bold/underline/reverse) are not permitted in this channel",
				chptr->chname);
			return NULL;
		} else
			return text;
	} else
		return text;
}
