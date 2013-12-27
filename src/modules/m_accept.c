/*
 *   IRC - Internet Relay Chat, src/modules/m_accept.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

DLLFUNC int m_accept(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_ACCEPT "ACCEPT"
#define TOK_ACCEPT "AT"

ModuleHeader MOD_HEADER(m_accept)
= {
	"m_accept",
	"Unreal3.2.1-DalNetRU2.1",
	"command /accept",
	"Unreal3.2.1-DalNetRU2.1",
	NULL
};

DLLFUNC int MOD_INIT(m_accept)(ModuleInfo *modinfo)
{
	add_Command(MSG_ACCEPT, TOK_ACCEPT, m_accept, MAXPARA);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_accept)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_accept)(int module_unload)
{
	if (del_Command(MSG_ACCEPT, TOK_ACCEPT, m_accept) < 0) {
		sendto_realops("Failed to delete commands when unloading %s", MOD_HEADER(m_accept).name);
	}
	return MOD_SUCCESS;
}

DLLFUNC CMD_FUNC(m_accept)
{
	if (!sptr->user) {
		return 0;
	}
	if (!IsCallerID(sptr)) {
		sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND), me.name, sptr->name, MSG_ACCEPT, "You are not +l");
		return 0;
	}
	if (parc >= 2) {
		char *cp;
		cp = parv[1];
		if (*parv[1] == '-') {
			/* DEL: -Nick (ACCEPT -Nick) or -Nick!User@Host (ACCEPT -Nick!User@Host) */
			cp++;
			if (!index(cp, '!') || !index(cp, '@')) {
				/* Nick */
				aClient *user;
				user = find_person(cp, NULL);
				if (!user) {
					sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, cp);
					return 0;
				}
				if (!del_accept(sptr, user, NULL, 0)) {
					del_accept(user, sptr, NULL, 1);
					sendto_one(sptr, ":%s ACCEPT -%s", sptr->name, cp);
				}
			} else {
				/* Nick!User@Host */
				if (!del_accept(sptr, NULL, cp, 0)) {
					sendto_one(sptr, ":%s ACCEPT -%s", sptr->name, cp);
				}
			}
		} else {
			/* ADD: +Nick (ACCEPT +Nick), Nick (ACCEPT Nick) or +Nick!User@Host (ACCEPT +Nick!User@Host) */
			Link *lp;
			int count = 0;
			if (*parv[1] == '+') {
				cp++;
			}
			/* Count */
			for (lp = sptr->user->accept; lp; lp = lp->next) {
				if (!AcceptIsRef(lp)) {
					count++;
				}
			}
			if (count + 1 > ACCEPT_LIMIT) {
				sendto_one(sptr, err_str(ERR_ACCEPTLISTFULL), me.name, sptr->name);
				return -1;
			}
			if (!index(cp, '!') || !index(cp, '@')) {
				/* Nick */
				aClient *user;
				user = find_person(cp, NULL);
				if (!user) {
					sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, cp);
					return 0;
				}
				if (user == sptr) {
					return 0;
				}
				if (!add_accept(sptr, user, NULL, 0)) {
					add_accept(user, sptr, NULL, 1);
					sptr->user->last_req = 0;
					sendto_one(sptr, ":%s ACCEPT +%s", sptr->name, cp);
				}
			} else {
				/* Nick!User@Host */
				if (!add_accept(sptr, NULL, cp, 0)) {
					sptr->user->last_req = 0;
					sendto_one(sptr, ":%s ACCEPT +%s", sptr->name, cp);
				}
			}
		}
	} else {
		/* LIST */
		Link *lp;
		for (lp = sptr->user->accept; lp; lp = lp->next) {
			if (AcceptIsUser(lp)) {
				sendto_one(sptr, rpl_str(RPL_ACCEPTLIST), me.name, sptr->name, lp->value.cptr->name);
			} else if (AcceptIsMask(lp)) {
				sendto_one(sptr, rpl_str(RPL_ACCEPTLIST), me.name, sptr->name, lp->value.cp);
			}
		}
		sendto_one(sptr, rpl_str(RPL_ENDOFACCEPTLIST), me.name, sptr->name);
	}
	return 0;
}
