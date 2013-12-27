/*
 * =================================================================
 * Filename:          noctcp.c
 * Description:       Channel mode +D and user modes +m & +M
 * Author:            AngryWolf <angrywolf@flashmail.com>
 * Documentation:     noctcp.txt (comes with the package)
 * =================================================================
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
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

#ifndef EXTCMODE
 #error "This module requires extended channel modes to be enabled."
 #error "See the EXTCMODE macro in include/config.h for more information."
 #error "Compilation aborted."
#endif

extern void			sendto_one(aClient *to, char *pattern, ...);

#define FLAG_NOACTION		'D'
#define UFLAG_NOACTION		'm'
#define UFLAG_NODCC		'M'
#define CANNOT_SEND_NOCTCP	"CTCPs are not permitted in this channel"
#define NoChannelAction(x)	((x)->mode.extmode & MODE_NOACTION)
#define NoUserAction(x)		((x)->umodes & UMODE_NOACTION)
#define NoUserDCC(x)		((x)->umodes & UMODE_NODCC)
#define DelCmode(x)		if (x) CmodeDel(x); x = NULL
#define DelUmode(x)		if (x) UmodeDel(x); x = NULL
#define DelHook(x)		if (x) HookDel(x); x = NULL

DLLFUNC int			MOD_UNLOAD(noctcp)(int module_unload);
static Cmode			*AddCmode(Module *module, CmodeInfo *req, Cmode_t *mode);
static Umode			*AddUmode(Module *module, char ch, long *mode);
static int			mode_noaction_is_ok(aClient *, aChannel *, char *, int, int);
static char			*cb_privmsg(aClient *, aClient *, aClient *, char *, int);
static char			*cb_chanmsg(aClient *, aClient *, aChannel *, char *, int);

Cmode_t				MODE_NOACTION = 0L;
long				UMODE_NOACTION = 0;
long				UMODE_NODCC = 0;
Cmode				*ModeNoAction = NULL;
Umode				*UmodeNoAction = NULL, *UmodeNoDCC = NULL;
Hook				*HookPrivMsg = NULL, *HookChanMsg = NULL;

ModuleHeader MOD_HEADER(noctcp)
  = {
	"noctcp",
	"$Id: noctcp.c,v 1.3 2004/04/09 11:00:00 angrywolf Exp $",
	"channel mode +D and user modes +m & +M",
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_TEST(noctcp)(ModuleInfo *modinfo)
{
	CmodeInfo req;

	memset(&req, 0, sizeof req);

	req.paracount	= 0;
	req.is_ok	= mode_noaction_is_ok;
	req.flag	= FLAG_NOACTION;
	ModeNoAction	= AddCmode(modinfo->handle, &req, &MODE_NOACTION);
	UmodeNoAction	= AddUmode(modinfo->handle, UFLAG_NOACTION, &UMODE_NOACTION);
	UmodeNoDCC	= AddUmode(modinfo->handle, UFLAG_NODCC, &UMODE_NODCC);

	if (!ModeNoAction || !UmodeNoAction || !UmodeNoDCC)
	{
		MOD_UNLOAD(noctcp)(0);
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(noctcp)(ModuleInfo *modinfo)
{
#ifndef STATIC_LINKING
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
#endif
	HookPrivMsg = HookAddPCharEx(modinfo->handle, HOOKTYPE_USERMSG, cb_privmsg);
	HookChanMsg = HookAddPCharEx(modinfo->handle, HOOKTYPE_CHANMSG, cb_chanmsg);

	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(noctcp)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(noctcp)(int module_unload)
{
	DelCmode(ModeNoAction);
	DelUmode(UmodeNoDCC);
	DelUmode(UmodeNoAction);
	DelHook(HookChanMsg);
	DelHook(HookPrivMsg);

	return MOD_SUCCESS;
}

static Cmode *AddCmode(Module *module, CmodeInfo *req, Cmode_t *mode)
{
	Cmode *cmode;

	*mode = 0;
	cmode = CmodeAdd(module, *req, mode);

#ifndef STATIC_LINKING
        if (ModuleGetError(module) != MODERR_NOERROR || !cmode)
#else
        if (!cmode)
#endif
	{
#ifndef STATIC_LINKING
		config_error("Error adding channel mode +%c when loading module %s: %s",
			req->flag, MOD_HEADER(noctcp).name, ModuleGetErrorStr(module));
#else
		config_error("Error adding channel mode +%c when loading module %s",
			req->flag, MOD_HEADER(noctcp).name);
#endif
		return NULL;
	}

	return cmode;
}

static Umode *AddUmode(Module *module, char ch, long *mode)
{
	Umode *umode;

	*mode = 0;
	umode = UmodeAdd(module, ch, UMODE_GLOBAL, NULL, mode);

#ifndef STATIC_LINKING
        if (ModuleGetError(module) != MODERR_NOERROR || !umode)
#else
        if (!umode)
#endif
	{
#ifndef STATIC_LINKING
		config_error("Error adding user mode +%c when loading module %s: %s",
			ch, MOD_HEADER(noctcp).name, ModuleGetErrorStr(module));
#else
		config_error("Error adding user mode +%c when loading module %s",
			ch, MOD_HEADER(noctcp).name);
#endif
		return NULL;
	}

	return umode;
}

static int mode_noaction_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr))
		{
			if (type == EXCHK_ACCESS_ERR)
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
					me.name, sptr->name, chptr->chname);
			return EX_DENY;
		}

		return EX_ALLOW;
	}

	return 0;
}

static char *cb_privmsg(aClient *cptr, aClient *from, aClient *to, char *str, int notice)
{
	if (!IsOper(from) && *str == 1 && from != to &&
	    ((NoUserAction(to) && !myncmp(str+1, "ACTION ", 7)) ||
	    (NoUserDCC(to) && !myncmp(str+1, "DCC ", 4))))
	{
		sendto_one(from, err_str(ERR_NOCTCP),
			me.name, from->name, to->name);
		return NULL;
	}

	return str;
}

static char *cb_chanmsg(aClient *cptr, aClient *from, aChannel *to, char *str, int notice)
{
	if (NoChannelAction(to) && !is_chan_op(from, to) &&
	    *str == 1 && !myncmp(str+1, "ACTION ", 7))
	{
		sendto_one(from, err_str(ERR_CANNOTSENDTOCHAN),
			me.name, from->name, to->chname,
			CANNOT_SEND_NOCTCP, to->chname);
		return NULL;
	}

	return str;
}
