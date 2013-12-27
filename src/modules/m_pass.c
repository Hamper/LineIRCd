/*
 *   IRC - Internet Relay Chat, src/modules/m_pass.c
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

DLLFUNC int m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_PASS 	"PASS"	
#define TOK_PASS 	"<"	

ModuleHeader MOD_HEADER(m_pass)
  = {
	"m_pass",
	"$Id: m_pass.c,v 1.1.6.2 2004/07/03 19:04:37 syzop Exp $",
	"command /pass", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_pass)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_PASS, TOK_PASS, m_pass, 1, M_UNREGISTERED|M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_pass)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_pass)(int module_unload)
{
	if (del_Command(MSG_PASS, TOK_PASS, m_pass) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_pass).name);
	}
	return MOD_SUCCESS;
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
DLLFUNC CMD_FUNC(m_pass)
{
	char *password = parc > 1 ? parv[1] : NULL;
	char *services_nick, *services_pass, *server_pass;

	if (BadPtr(password))
	{
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PASS");
		return 0;
	}

	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	{
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		return 0;
	}

	if (strchr(password, ':')) {
		// Указан пароль для сервисов и может быть для сервера
		// (разделитель: двоеточие)
		services_nick = strtok(password, ":");
		services_pass = strtok(NULL, ":");
		server_pass = strtok(NULL, "");
	} else if (strchr(password, ';')) {
		// Указан пароль для сервисов и может быть для сервера
		// (разделитель: точка с запятой)
		services_nick = strtok(password, ";");
		services_pass = strtok(NULL, ";");
		server_pass = strtok(NULL, "");
	} else {
		// Указан только пароль для сервера
		services_nick = NULL;
		services_pass = NULL;
		server_pass = password;
	}

	// Идентификация на сервисах
	if (services_nick && services_pass) {
		if (find_server_quick(SERVICES_NAME)) {
			// Сервисы доступны
			if (next_requestid <= 0) {
				next_requestid = 1;
			}
			cptr->requestid = next_requestid++;
			SetClientFrozen(cptr);
			sendto_one(sptr, ":%s NOTICE AUTH :*** Checking services password...", me.name);
			{
				// Отправка запроса на сервисы
				char starttime_text[30], newrequestid_text[30];
#define IDREQ_PARC 8
				char *idreq_parv[IDREQ_PARC];
				char uhost[HOSTLEN + USERLEN + 3]; // Для локального постфикса
				snprintf(starttime_text, sizeof(starttime_text), "%lu", (unsigned long)me.since);
				snprintf(newrequestid_text, sizeof(newrequestid_text), "%d", cptr->requestid);
				// Синтаксис команды IDREQ см. в m_idreq.c
				idreq_parv[0] = me.name;
				idreq_parv[1] = SERVICES_NAME;
				idreq_parv[2] = starttime_text;
				idreq_parv[3] = newrequestid_text;
				idreq_parv[4] = services_nick;
				idreq_parv[5] = services_pass;
				if (local_postfix && (!cptr->hostp || local_postfix_for_all) && local_ipv4(Inet_ia2p(&cptr->ip))) {
					// Использовать локальный постфикс
					unsigned int a, b, c, d;
					sscanf(Inet_ia2p(&cptr->ip), "%u.%u.%u.%u", &a, &b, &c, &d);
					ircsprintf(uhost,  "%d-%d-%d-%d.%s", a, b, c, d, local_postfix);
					idreq_parv[6] = uhost;
				} else {
					// Не использовать локальный постфикс
					if (cptr->hostp) {
						idreq_parv[6] = cptr->hostp->h_name;
					} else {
						idreq_parv[6] = Inet_ia2p(&cptr->ip);
					}
				}
				idreq_parv[7] = Inet_ia2p(&cptr->ip);
				hunt_server_token(cptr, sptr, MSG_IDREQ, TOK_IDREQ, "%s %s %s %s %s %s %s", 1, IDREQ_PARC, idreq_parv);
			}
		} else {
			// Сервисы не доступны
			sendto_one(sptr, ":%s NOTICE AUTH :*** Services are currently down. Services password ignored.", me.name);
		}
	}

	// Обработка пароля для сервера
	if (server_pass) {
		int server_pass_len;
		server_pass_len = strlen(server_pass);
		if (cptr->passwd) {
			MyFree(cptr->passwd);
		}
		if (server_pass_len > PASSWDLEN) {
			server_pass_len = PASSWDLEN;
		}
		cptr->passwd = MyMalloc(server_pass_len + 1);
		strncpyzt(cptr->passwd, server_pass, server_pass_len + 1);

		/* note: the original non-truncated password is supplied as 2nd parameter. */
		RunHookReturnInt2(HOOKTYPE_LOCAL_PASS, sptr, server_pass, !=0);
	}

	return 0;
}
