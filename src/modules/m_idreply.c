/*
 *   IRC - Internet Relay Chat, src/modules/m_idreply.c
 *   (C) 2008 JSergey
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
#include "proto.h"
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_idreply(aClient *cptr, aClient *sptr, int parc, char *parv[]);

ModuleHeader MOD_HEADER(m_idreply) = {
	"idreply", /* Name of module */
	"Unreal3.2.1-DalNetRU2.3", /* Version */
	"command /idreply", /* Short description of module */
	"3.2-b8-1",
	NULL
};

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_idreply)(ModuleInfo *modinfo) {
	add_Command(MSG_IDREPLY, TOK_IDREPLY, m_idreply, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_idreply)(int module_load) {
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_idreply)(int module_unload) {
	if (del_Command(MSG_IDREPLY, TOK_IDREPLY, m_idreply) < 0) {
		sendto_realops("Failed to delete commands when unloading %s", MOD_HEADER(m_idreply).name);
	}
	return MOD_SUCCESS;
}

/*
 * Команда: IDREPLY
 * Параметры:
 * parv[0] - Отправитель
 * parv[1] - Сервер-получатель
 * parv[2] - Время запуска сервера (для проверки правильности сервера)
 * parv[3] - ID запроса
 * parv[4] - Servicestamp или 0 если ошибка
 * parv[5] - Новый username (* - не изменять username)
 * parv[6] - VHOST (* - не назначать VHOST)
 * parv[7] - Исключение из K:Line (1 - да, 0 - нет)
 * parv[8] - Исключение из G:Line (1 - да, 0 - нет)
 * parv[9] - Сообщение для пользователя
 */
DLLFUNC int m_idreply(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
	aClient *acptr;

	if (!IsULine(sptr)) {
		return -1;
	}

	if (parc != 10) {
		return -1;
	}

	if (hunt_server_token(cptr, sptr, MSG_IDREPLY, TOK_IDREPLY, "%s %s %s %s %s %s %s %s :%s", 1, parc, parv)
		!= HUNTED_ISME) {
		return 0;
	}

	// Проверка правильности сервера по времени запуска сервера
	time_t starttime = strtoul(parv[2], NULL, 10);
	if (starttime != me.since) {
		return 0;
	}

	// Поиск пользователя, к которому относится запрос
	{
		int i, found = 0;
		int requestid = atoi(parv[3]);
		for (i = 0; i <= LastSlot; i++) {
			acptr = local[i];
			if (!acptr) {
				continue;
			}
			if (acptr->requestid == requestid) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}

	acptr->idreq_servicestamp = strtoul(parv[4], NULL, 10);
	if (acptr->idreq_servicestamp) {
		// Смена username
		if (strcmp(parv[5], "*")) {
			SetClientSUser(acptr);
			strncpyzt(acptr->username, parv[5], USERLEN + 1);
		}
		// VHOST
		if (strcmp(parv[6], "*")) {
		}
		// Исключение из K:Line
		if (atoi(parv[7]) == 1) {
			SetClientExceptK(acptr);
		}
		// Исключение из G:Line
		if (atoi(parv[8]) == 1) {
			SetClientExceptG(acptr);
		}
		// Сообщение
		sendto_one(acptr, ":%s NOTICE AUTH :*** Services identification: OK (%s)", me.name, parv[9]);
	} else {
		sendto_one(acptr, ":%s NOTICE AUTH :*** Services identification: ERROR (%s)", me.name, parv[9]);
	}

	acptr->requestid = 0;
	ClearClientFrozen(acptr);

	return 0;
}
