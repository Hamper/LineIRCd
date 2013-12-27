/*
 *   IRC - Internet Relay Chat, src/modules/m_idreq.c
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

DLLFUNC int m_idreq(aClient *cptr, aClient *sptr, int parc, char *parv[]);

ModuleHeader MOD_HEADER(m_idreq) = {
	"idreq", /* Name of module */
	"Unreal3.2.1-DalNetRU2.3", /* Version */
	"command /idreq", /* Short description of module */
	"3.2-b8-1",
	NULL
};

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_idreq)(ModuleInfo *modinfo) {
	add_Command(MSG_IDREQ, TOK_IDREQ, m_idreq, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_idreq)(int module_load) {
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_idreq)(int module_unload) {
	if (del_Command(MSG_IDREQ, TOK_IDREQ, m_idreq) < 0) {
		sendto_realops("Failed to delete commands when unloading %s", MOD_HEADER(m_idreq).name);
	}
	return MOD_SUCCESS;
}

/*
 * Команда: IDREQ
 * Параметры:
 * parv[0] - Отправитель
 * parv[1] - Сервер-получатель
 * parv[2] - Время запуска сервера-получателя (для проверки правильности сервера)
 * parv[3] - ID запроса
 * parv[4] - Логин
 * parv[5] - Пароль
 * parv[6] - Хост отправителя (без username)
 * parv[7] - IP адрес отправителя
 */
DLLFUNC int m_idreq(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
	aClient *acptr;

	if (!IsServer(sptr)) {
		return -1;
	}

	if (parc != 8) {
		return -1;
	}

	if (hunt_server_token(cptr, sptr, MSG_IDREQ, TOK_IDREQ, "%s %s %s %s %s %s %s", 1, parc, parv)
		!= HUNTED_ISME) {
		return 0;
	}

	// Запрос адресован данному серверу
	// Так не должно быть. Ничего не делать

	return 0;
}
