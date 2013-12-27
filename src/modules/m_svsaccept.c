/*
 *   IRC - Internet Relay Chat, src/modules/m_svsaccept.c
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

DLLFUNC int m_svsaccept(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSACCEPT "SVSACCEPT"
#define TOK_SVSACCEPT "SA"

ModuleHeader MOD_HEADER(m_svsaccept)
= {
	"m_svsaccept",
	"Unreal3.2.1-DalNetRU2.1",
	"command /svsaccept",
	"Unreal3.2.1-DalNetRU2.1",
	NULL
};

DLLFUNC int MOD_INIT(m_svsaccept)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSACCEPT, TOK_SVSACCEPT, m_svsaccept, MAXPARA);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsaccept)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsaccept)(int module_unload)
{
	if (del_Command(MSG_SVSACCEPT, TOK_SVSACCEPT, m_svsaccept) < 0) {
		sendto_realops("Failed to delete commands when unloading %s", MOD_HEADER(m_svsaccept).name);
	}
	return MOD_SUCCESS;
}

DLLFUNC CMD_FUNC(m_svsaccept)
{
	aClient *acptr;
	if (!IsULine(sptr)) {
		return 0;
	}
	if (parc < 3) {
		return 0;
	}
	acptr = find_person(parv[1], NULL);
	if (!acptr) {
		return 0;
	}
	if (MyClient(acptr)) {
		char *cp;
		cp = parv[2];
		if (*parv[2] == '-') {
			/* DEL: -Nick (SVSACCEPT -Nick) or -Nick!User@Host (SVSACCEPT -Nick!User@Host) */
			cp++;
			if (!index(cp, '!') || !index(cp, '@')) {
				/* Nick */
				aClient *user;
				user = find_person(cp, NULL);
				if (!user) {
					return 0;
				}
				if (!del_accept(acptr, user, NULL, 0)) {
					del_accept(user, acptr, NULL, 1);
				}
			} else {
				/* Nick!User@Host */
				del_accept(acptr, NULL, cp, 0);
			}
		} else if (*parv[2] == '+') {
			/* ADD: +Nick (SVSACCEPT +Nick) or +Nick!User@Host (SVSACCEPT +Nick!User@Host) */
			cp++;
			if (!index(cp, '!') || !index(cp, '@')) {
				/* Nick */
				aClient *user;
				user = find_person(cp, NULL);
				if (!user) {
					return 0;
				}
				if (user == acptr) {
					return 0;
				}
				if (!add_accept(acptr, user, NULL, 0)) {
					add_accept(user, acptr, NULL, 1);
					acptr->user->last_req = 0;
				}
			} else {
				/* Nick!User@Host */
				if (!add_accept(acptr, NULL, cp, 0)) {
					acptr->user->last_req = 0;
				}
			}
		} else if (*parv[2] == '*') {
			Link *lp, *next;
			for (lp = acptr->user->accept; lp; lp = next) {
				next = lp->next;
				if (AcceptIsUser(lp)) {
					aClient *tmp;
					tmp = lp->value.cptr;
					if (!del_accept(acptr, tmp, NULL, 0)) {
						del_accept(tmp, acptr, NULL, 1);
					}
				} else if (AcceptIsMask(lp)) {
					del_accept(acptr, NULL, lp->value.cp, 0);
				}
			}
		}
	} else {
		sendto_one(acptr, ":%s %s %s :%s", parv[0], MSG_SVSACCEPT, parv[1], parv[2]);
	}
	return 0;
}
