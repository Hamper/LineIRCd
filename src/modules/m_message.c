/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_message.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

static int is_silenced(aClient *, aClient *);
static int is_accepted(aClient *, aClient *);

DLLFUNC int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice);
DLLFUNC int  m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int  m_private(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_PRIVATE     "PRIVMSG"       /* PRIV */
#define TOK_PRIVATE     "!"     /* 33 */
#define MSG_NOTICE      "NOTICE"        /* NOTI */
#define TOK_NOTICE      "B"     /* 66 */

ModuleHeader MOD_HEADER(m_message)
  = {
	"message",	/* Name of module */
	"$Id: m_message.c,v 1.1.6.4 2004/07/03 19:04:36 syzop Exp $", /* Version */
	"private message and notice", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_message)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_CommandX(MSG_PRIVATE, TOK_PRIVATE, m_private, 2, M_USER|M_SERVER|M_RESETIDLE|M_VIRUS);
	add_Command(MSG_NOTICE, TOK_NOTICE, m_notice, 2);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_message)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_message)(int module_unload)
{
	if (del_Command(MSG_PRIVATE, TOK_PRIVATE, m_private) < 0)
	{
		sendto_realops("Failed to delete command privmsg when unloading %s",
				MOD_HEADER(m_message).name);
	}
	if (del_Command(MSG_NOTICE, TOK_NOTICE, m_notice) < 0)
	{
		sendto_realops("Failed to delete command notice when unloading %s",
				MOD_HEADER(m_message).name);
	}
	return MOD_SUCCESS;
}

static int check_dcc(aClient *sptr, char *target, aClient *targetcli, char *text);
static int check_dcc_soft(aClient *from, aClient *to, char *text);

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/
static int recursive_webtv = 0;
DLLFUNC int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice)
{
	aClient *acptr;
	char *s;
	aChannel *chptr;
	char *nick, *server, *p, *cmd, *ctcp, *p2, *pc, *text;
	int  cansend = 0;
	int  prefix = 0;
	char pfixchan[CHANNELLEN + 4];
	int ret;

	/*
	 * Reasons why someone can't send to a channel
	 */
	static char *err_cantsend[] = {
		"You need voice (+v)",
		"No external channel messages",
		"Color is not permitted in this channel",
		"You are banned",
		"CTCPs are not permitted in this channel",
		"You must have a registered nick (+r) to talk on this channel",
		"Swearing is not permitted in this channel",
		"NOTICEs are not permitted in this channel",
		NULL
	};

	if (IsHandshake(sptr))
		return 0;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;
	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
		    me.name, parv[0], cmd);
		return -1;
	}

	if (parc < 3 || *parv[2] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if (MyConnect(sptr))
		parv[1] = (char *)canonize(parv[1]);
		
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	    nick = strtoken(&p, NULL, ","))
	{
		if (IsVirus(sptr) && (!strcasecmp(nick, "ircd") || !strcasecmp(nick, "irc")))
		{
			sendnotice(sptr, "IRC command(s) unavailable because you are suspected to have a virus");
			continue;
		}
		/*
		   ** nickname addressed?
		 */
		if (!strcasecmp(nick, "ircd") && MyClient(sptr))
		{
			ret = 0;
			if (!recursive_webtv)
			{
				recursive_webtv = 1;
				ret = parse(sptr, parv[2], (parv[2] + strlen(parv[2])));
				recursive_webtv = 0;
			}
			return ret;
		}
		if (!strcasecmp(nick, "irc") && MyClient(sptr))
		{
			if (!recursive_webtv)
			{
				recursive_webtv = 1;
				ret = webtv_parse(sptr, parv[2]);
				if (ret == -99)
				{
					ret = parse(sptr, parv[2], (parv[2] + strlen(parv[2])));
				}
				recursive_webtv = 0;
				return ret;
			}
		}
		if (*nick != '#' && (acptr = find_person(nick, NULL)))
		{
			/* Message to USER */
			if (IsVirus(sptr))
			{
				sendnotice(sptr, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
				continue;
			}
			/* Umode +R (idea from Bahamut) */
			if (IsRegNickMsg(acptr) && !IsRegNick(sptr) && !IsULine(sptr) && !IsOper(sptr)) {
				sendto_one(sptr, err_str(ERR_NONONREG), me.name, parv[0],
					acptr->name);
				return 0;
			}
			if (IsNoCTCP(acptr) && !IsOper(sptr) && *parv[2] == 1 && acptr != sptr)
			{
				ctcp = &parv[2][1];
				if (myncmp(ctcp, "ACTION ", 7) && myncmp(ctcp, "DCC ", 4))
				{
					sendto_one(sptr, err_str(ERR_NOCTCP), me.name, parv[0],
						acptr->name);
					return 0;
				}
			}

			if (MyClient(sptr) && !strncasecmp(parv[2], "\001DCC", 4))
			{
				ret = check_dcc(sptr, acptr->name, acptr, parv[2]);
				if (ret < 0)
					return ret;
				if (ret == 0)
					continue;
			}
			if (MyClient(acptr) && !strncasecmp(parv[2], "\001DCC", 4) &&
			    !check_dcc_soft(sptr, acptr, parv[2]))
				continue;

			if (MyClient(sptr) && check_for_target_limit(sptr, acptr, acptr->name))
				continue;

			if (!is_silenced(sptr, acptr))
			{
#ifdef STRIPBADWORDS
				int blocked = 0;
#endif
				char *newcmd = cmd, *text;
				Hook *tmphook;
				
				if (MyClient(acptr) && IsPerson(sptr) && IsCallerID(acptr) && !IsAnOper(sptr) && !IsULine(sptr) && (sptr != acptr)) {
					if (!is_accepted(sptr, acptr)) {
						int t;
						if (notice) {
							t = 1;
						} else if (*parv[2] == 1) {
							if (!myncmp(parv[2]+1, "ACTION ", 7)) {
								t = 0;
							} else if (!myncmp(parv[2]+1, "DCC ", 4)) {
								t = 3;
							} else {
								t = 2;
							}
						} else {
							t = 0;
						}
						switch (t) {
							case 0:
								if (acptr->user->last_req + CALLERID_TIME < TStime()) {
									acptr->user->last_req = TStime();
									sendto_one(sptr, ":%s NOTICE %s :%s is in CallerID mode. User has been informed that you messaged him. Please wait for reply.", me.name, sptr->name, acptr->name);
									sendto_one(acptr, ":%s NOTICE %s :%s (%s@%s) is messaging you, and you are in CallerID mode.", me.name, acptr->name, sptr->name, sptr->user->username, GetHost(sptr));
								} else {
									sendto_one(sptr, ":%s NOTICE %s :%s is in CallerID mode. Your message was ignored. Please wait %d seconds and try again.", me.name, sptr->name, acptr->name, acptr->user->last_req + CALLERID_TIME - TStime());
								}
								break;
							case 1:
								sendto_one(sptr, ":%s NOTICE %s :%s is in CallerID mode. Your NOTICE was ignored. Use PRIVMSG instead.", me.name, sptr->name, acptr->name);
								break;
							case 2:
								sendto_one(sptr, ":%s NOTICE %s :%s is in CallerID mode. Your CTCP was ignored.", me.name, sptr->name, acptr->name);
								break;
							case 3:
								sendto_one(sptr, ":%s NOTICE %s :%s is in CallerID mode. Your DCC was ignored.", me.name, sptr->name, acptr->name);
								break;
						}
						continue;
					}
				}
				if (MyClient(sptr) && IsPerson(acptr) && IsCallerID(sptr) && !IsULine(acptr) && (sptr != acptr)) {
					if (!is_accepted(acptr, sptr)) {
						Link *lp;
						int count = 0;
						for (lp = sptr->user->accept; lp; lp = lp->next) {
							if (!AcceptIsRef(lp)) {
								count++;
							}
						}
						if (count + 1 <= ACCEPT_LIMIT) {
							if (!add_accept(sptr, acptr, NULL, 0)) {
								add_accept(acptr, sptr, NULL, 1);
							}
						}
					}
				}

				if (notice && IsWebTV(acptr) && *parv[2] != '\1')
					newcmd = MSG_PRIVATE;
				if (!notice && MyConnect(sptr) &&
				    acptr->user && acptr->user->away)
					sendto_one(sptr, rpl_str(RPL_AWAY),
					    me.name, parv[0], acptr->name,
					    acptr->user->away);

#ifdef STRIPBADWORDS
				if (MyClient(sptr) && !IsULine(acptr) && IsFilteringWords(acptr))
				{
					text = stripbadwords_message(parv[2], &blocked);
					if (blocked)
					{
						if (!notice && MyClient(sptr))
							sendto_one(sptr, rpl_str(ERR_NOSWEAR),
								me.name, parv[0], acptr->name);
						continue;
					}
				}
				else
#endif
					text = parv[2];

				if (MyClient(sptr))
				{
					ret = dospamfilter(sptr, text, newcmd == MSG_NOTICE ? SPAMF_USERNOTICE : SPAMF_USERMSG, acptr->name);
					if (ret < 0)
						return FLUSH_BUFFER;
				}

				for (tmphook = Hooks[HOOKTYPE_USERMSG]; tmphook; tmphook = tmphook->next) {
					text = (*(tmphook->func.pcharfunc))(cptr, sptr, acptr, text, (int)(newcmd == MSG_NOTICE ? 1 : 0) );
					if (!text)
						break;
				}
				if (!text)
					continue;
				
				// Send to user
				sendto_message_one(acptr,
				    sptr, parv[0], newcmd, acptr->name, text);
			}
			continue;
		}

		p2 = (char *)strchr(nick, '#');
		prefix = 0;
		if (p2 && (chptr = find_channel(p2, NullChn)))
		{
			/* Message to CHANNEL */
			if (p2 != nick)
			{
				for (pc = nick; pc != p2; pc++)
				{
#ifdef PREFIX_AQ
 #define PREFIX_REST (PREFIX_ADMIN|PREFIX_OWNER)
#else
 #define PREFIX_REST (0)
#endif
					switch (*pc)
					{
					  case '+':
						  prefix |= PREFIX_VOICE | PREFIX_HALFOP | PREFIX_OP | PREFIX_REST;
						  break;
					  case '%':
						  prefix |= PREFIX_HALFOP | PREFIX_OP | PREFIX_REST;
						  break;
					  case '@':
						  prefix |= PREFIX_OP | PREFIX_REST;
						  break;
#ifdef PREFIX_AQ
					  case '&':
						  prefix |= PREFIX_ADMIN | PREFIX_OWNER;
					  	  break;
					  case '~':
						  prefix |= PREFIX_OWNER;
						  break;
#else
					  case '&':
						  prefix |= PREFIX_OP | PREFIX_REST;
					  	  break;
					  case '~':
						  prefix |= PREFIX_OP | PREFIX_REST;
						  break;
#endif
					  default:
						  break;	/* ignore it :P */
					}
				}
				
				if (prefix)
				{
					if (MyClient(sptr) && !op_can_override(sptr))
					{
						Membership *lp = find_membership_link(sptr->user->channel, chptr);
						/* Check if user is allowed to send. RULES:
						 * Need at least voice (+) in order to send to +,% or @
						 * Need at least ops (@) in order to send to & or ~
						 */
						if (!lp || !(lp->flags & (CHFL_VOICE|CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
						{
							sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
								me.name, sptr->name, chptr->chname);
							return 0;
						}
						if (!(prefix & PREFIX_OP) && ((prefix & PREFIX_OWNER) || (prefix & PREFIX_ADMIN)) &&
						    !(lp->flags & (CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
						{
							sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
								me.name, sptr->name, chptr->chname);
							return 0;
						}
					}
					/* Now find out the lowest prefix and use that.. (so @&~#chan becomes @#chan) */
					if (prefix & PREFIX_VOICE)
						pfixchan[0] = '+';
					else if (prefix & PREFIX_HALFOP)
						pfixchan[0] = '%';
					else if (prefix & PREFIX_OP)
						pfixchan[0] = '@';
#ifdef PREFIX_AQ
					else if (prefix & PREFIX_ADMIN)
						pfixchan[0] = '&';
					else if (prefix & PREFIX_OWNER)
						pfixchan[0] = '~';
#endif
					else
						abort();
					strlcpy(pfixchan+1, chptr->chname, sizeof(pfixchan)-1);
					nick = pfixchan;
				} else {
					nick = chptr->chname;
				}
			} else {
				nick = chptr->chname;
			}
			
			if (MyClient(sptr) && (*parv[2] == 1))
			{
				ret = check_dcc(sptr, chptr->chname, NULL, parv[2]);
				if (ret < 0)
					return ret;
				if (ret == 0)
					continue;
			}
			if (IsVirus(sptr) && strcasecmp(chptr->chname, SPAMFILTER_VIRUSCHAN))
			{
				sendnotice(sptr, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
				continue;
			}
			
			cansend =
			    !IsULine(sptr) ? can_send(sptr, chptr, parv[2], notice) : 0;
			if (!cansend)
			{
#ifdef STRIPBADWORDS
				int blocked = 0;
#endif
				Hook *tmphook;
#ifdef NEWCHFLOODPROT
				if (chptr->mode.floodprot && chptr->mode.floodprot->l[FLD_TEXT])
#else
				if (chptr->mode.per)
#endif
					if (check_for_chan_flood(cptr, sptr, chptr) == 1)
						continue;

				if (!CHANCMDPFX)
					sendanyways = (parv[2][0] == '`' ? 1 : 0);
				else
					sendanyways = (strchr(CHANCMDPFX,parv[2][0]) ? 1 : 0);
				text = parv[2];
				if (MyClient(sptr) && (chptr->mode.mode & MODE_STRIP))
					text = StripColors(parv[2]);
#ifdef STRIPBADWORDS
				if (MyClient(sptr))
				{
 #ifdef STRIPBADWORDS_CHAN_ALWAYS
					text = stripbadwords_channel(text,& blocked);
					if (blocked)
					{
						if (!notice)
							sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
							    me.name, parv[0], chptr->chname,
							    err_cantsend[6], p2);
						continue;
					}
 #else
					if (chptr->mode.mode & MODE_STRIPBADWORDS)
					{
						text = stripbadwords_channel(text, &blocked);
						if (blocked)
						{
							if (!notice)
								sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
								    me.name, parv[0], chptr->chname,
								    err_cantsend[6], p2);
							continue;
						}
					}
 #endif
				}
#endif

				if (MyClient(sptr))
				{
					ret = dospamfilter(sptr, text, notice ? SPAMF_CHANNOTICE : SPAMF_CHANMSG, chptr->chname);
					if (ret < 0)
						return ret;
				}

				for (tmphook = Hooks[HOOKTYPE_CHANMSG]; tmphook; tmphook = tmphook->next) {
					text = (*(tmphook->func.pcharfunc))(cptr, sptr, chptr, text, notice);
					if (!text)
						break;
				}
				
				if (!text)
					continue;

				// Send to channel
				sendto_channelprefix_butone_tok(cptr,
				    sptr, chptr,
				    prefix,
				    notice ? MSG_NOTICE : MSG_PRIVATE,
				    notice ? TOK_NOTICE : TOK_PRIVATE,
				    nick, text, 1);

#ifdef NEWCHFLOODPROT
				if (chptr->mode.floodprot && !is_skochanop(sptr, chptr) &&
				    !IsULine(sptr) && do_chanflood(chptr->mode.floodprot, FLD_MSG) &&
				    MyClient(sptr))
				{
					do_chanflood_action(chptr, FLD_MSG, "msg/notice");
				}
				
				if (chptr->mode.floodprot && !is_skochanop(sptr, chptr) &&
				    (text[0] == '\001') && strncmp(text+1, "ACTION ", 7) &&
				    do_chanflood(chptr->mode.floodprot, FLD_CTCP) && MyClient(sptr))
				{
					do_chanflood_action(chptr, FLD_CTCP, "CTCP");
				}
#endif
				sendanyways = 0;
				continue;
			}
			else
			if (MyClient(sptr))
			{
				if (!notice || (cansend == 8)) /* privmsg or 'cannot send notice'... */
					sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					    me.name, parv[0], chptr->chname,
					    err_cantsend[cansend - 1], p2);
			}
			continue;
		}
		else if (p2)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
			    parv[0], p2);
			continue;
		}


		/*
		   ** the following two cases allow masks in NOTICEs
		   ** (for OPERs only)
		   **
		   ** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		 */
		if ((*nick == '$' || *nick == '#') && (IsAnOper(sptr)
		    || IsULine(sptr)))
		{
			if (IsULine(sptr))
				goto itsokay;
			if (!(s = (char *)rindex(nick, '.')))
			{
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL),
				    me.name, parv[0], nick);
				continue;
			}
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			{
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL),
				    me.name, parv[0], nick);
				continue;
			}
		      itsokay:
			sendto_match_butone(IsServer(cptr) ? cptr : NULL,
			    sptr, nick + 1,
			    (*nick == '#') ? MATCH_HOST :
			    MATCH_SERVER,
			    ":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
			continue;
		}

		/*
		   ** user[%host]@server addressed?
		 */
		server = index(nick, '@');
		if (server)	/* meaning there is a @ */
		{
			/* There is always a \0 if its a string */
			if (*(server + 1) != '\0')
			{
				acptr = find_server_quick(server + 1);
				if (acptr)
				{
					/*
					   ** Not destined for a user on me :-(
					 */
					if (!IsMe(acptr))
					{
						if (IsToken(acptr))
							sendto_one(acptr,
							    ":%s %s %s :%s", parv[0],
							    notice ? TOK_NOTICE : TOK_PRIVATE,
							    nick, parv[2]);
						else
							sendto_one(acptr,
							    ":%s %s %s :%s", parv[0],
							    cmd, nick, parv[2]);
						continue;
					}

					/* Find the nick@server using hash. */
					acptr =
					    find_nickserv(nick,
					    (aClient *)NULL);
					if (acptr)
					{
						sendto_prefix_one(acptr, sptr, 1,
						    ":%s %s %s :%s",
						    parv[0], cmd,
						    acptr->name, parv[2]);
						continue;
					}
				}
				if (server
				    && strncasecmp(server + 1, SERVICES_NAME,
				    strlen(SERVICES_NAME)) == 0)
					sendto_one(sptr,
					    err_str(ERR_SERVICESDOWN), me.name,
					    parv[0], nick);
				else
					sendto_one(sptr,
					    err_str(ERR_NOSUCHNICK), me.name,
					    parv[0], nick);

				continue;
			}

		}
		/* nothing, nada, not anything found */
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0],
		    nick);
		continue;
	}
	return 0;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

DLLFUNC int  m_private(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

DLLFUNC int  m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 1);
}
/***********************************************************************
 * m_silence() - Added 19 May 1994 by Run.
 *
 ***********************************************************************/

/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
static int is_silenced(aClient *sptr, aClient *acptr)
{
	Link *lp;
	anUser *user;
	static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
	static char senderx[HOSTLEN + NICKLEN + USERLEN + 5];
	char checkv = 0;
	
	if (!(acptr->user) || !(lp = acptr->user->silence) ||
	    !(user = sptr->user)) return 0;

	ircsprintf(sender, "%s!%s@%s", sptr->name, user->username,
	    user->realhost);
	/* We also check for matches against sptr->user->virthost if present,
	 * this is checked regardless of mode +x so you can't do tricks like:
	 * evil has +x and msgs, victim places silence on +x host, evil does -x
	 * and can msg again. -- Syzop
	 */
	if (sptr->user->virthost)
	{
		ircsprintf(senderx, "%s!%s@%s", sptr->name, user->username,
		    sptr->user->virthost);
		checkv = 1;
	}

	for (; lp; lp = lp->next)
	{
		if (!match(lp->value.cp, sender) || (checkv && !match(lp->value.cp, senderx)))
		{
			if (!MyConnect(sptr))
			{
				sendto_one(sptr->from, ":%s SILENCE %s :%s",
				    acptr->name, sptr->name, lp->value.cp);
				lp->flags = 1;
			}
			return 1;
		}
	}
	return 0;
}

/** Make a viewable dcc filename.
 * This is to protect a bit against tricks like 'flood-it-off-the-buffer'
 * and color 1,1 etc...
 */
char *dcc_displayfile(char *f)
{
static char buf[512];
char *i, *o = buf;
size_t n = strlen(f);

	if (n < 300)
	{
		for (i = f; *i; i++)
			if (*i < 32)
				*o++ = '?';
			else
				*o++ = *i;
		*o = '\0';
		return buf;
	}

	/* Else, we show it as: [first 256 chars]+"[..TRUNCATED..]"+[last 20 chars] */
	for (i = f; i < f+256; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	strcpy(o, "[..TRUNCATED..]");
	o += sizeof("[..TRUNCATED..]");
	for (i = f+n-20; *i; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	*o = '\0';
	return buf;
	
}

/** Checks if a DCC is allowed.
 * PARAMETERS:
 * sptr:		the client to check for
 * target:		the target (eg a user or a channel)
 * targetcli:	the target client, NULL in case of a channel
 * text:		the whole msg
 * RETURNS:
 * 1:			allowed (no dcc, etc)
 * 0:			block
 * <0:			immediately return with this value (could be FLUSH_BUFFER)
 * HISTORY:
 * F:Line stuff by _Jozeph_ added by Stskeeps with comments.
 * moved and various improvements by Syzop.
 */
static int check_dcc(aClient *sptr, char *target, aClient *targetcli, char *text)
{
char *ctcp;
ConfigItem_deny_dcc *fl;
char *end, realfile[BUFSIZE];
int size_string, ret;

	if ((*text != 1) || !MyClient(sptr) || IsOper(sptr) || (targetcli && IsAnOper(targetcli)))
		return 1;

	ctcp = &text[1];
	/* Most likely a DCC send .. */
	if (!myncmp(ctcp, "DCC SEND ", 9))
		ctcp = text + 10;
	else
		return 1; /* something else, allow */

	if (sptr->flags & FLAGS_DCCBLOCK)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** You are blocked from sending files as you have tried to "
		                 "send a forbidden file - reconnect to regain ability to send",
			me.name, sptr->name);
		return 0;
	}
	if (*ctcp == '"' && *(ctcp+1))
		end = index(ctcp+1, '"');
	else
		end = index(ctcp, ' ');

	/* check if it was fake.. just pass it along then .. */
	if (!end || (end < ctcp))
		return 1; /* allow */

	size_string = (int)(end - ctcp);

	if (!size_string || (size_string > (BUFSIZE - 1)))
		return 1; /* allow */

	strlcpy(realfile, ctcp, size_string+1);

	if ((ret = dospamfilter(sptr, realfile, SPAMF_DCC, target)) < 0)
		return ret;

	if ((fl = dcc_isforbidden(sptr, realfile)))
	{
		char *displayfile = dcc_displayfile(realfile);
		sendto_one(sptr,
		    ":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
		    me.name, RPL_TEXT,
		    sptr->name, displayfile, target, fl->reason);
		sendto_one(sptr, ":%s NOTICE %s :*** You have been blocked from sending files, reconnect to regain permission to send files",
			me.name, sptr->name);

		sendto_snomask(SNO_VICTIM,
		    "%s (%s@%s) tried to send forbidden file %s (%s) to %s (is blocked now)",
		    sptr->name, sptr->user->username, sptr->user->realhost, displayfile, fl->reason, target);
		sendto_serv_butone_token(NULL, me.name, MSG_SENDSNO, TOK_SENDSNO, "V :%s (%s@%s) tried to send forbidden file %s (%s) to %s (is blocked now)", sptr->name, sptr->user->username, sptr->user->realhost, displayfile, fl->reason, target);
		sptr->flags |= FLAGS_DCCBLOCK;
		return 0; /* block */
	}

	/* Channel dcc (???) and discouraged? just block */
	if (!targetcli && ((fl = dcc_isdiscouraged(sptr, realfile))))
	{
		char *displayfile = dcc_displayfile(realfile);
		sendto_one(sptr,
		    ":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
		    me.name, RPL_TEXT,
		    sptr->name, displayfile, target, fl->reason);
		return 0; /* block */
	}
	return 1; /* allowed */
}

/** Checks if a DCC is allowed by DCCALLOW rules (only SOFT bans are checked).
 * PARAMETERS:
 * from:		the sender client (possibly remote)
 * to:			the target client (always local)
 * text:		the whole msg
 * RETURNS:
 * 1:			allowed
 * 0:			block
 */
static int check_dcc_soft(aClient *from, aClient *to, char *text)
{
char *ctcp;
ConfigItem_deny_dcc *fl;
char *end, realfile[BUFSIZE];
int size_string, ret;

	if ((*text != 1) || IsOper(from) || IsOper(to))
		return 1;

	ctcp = &text[1];
	/* Most likely a DCC send .. */
	if (!myncmp(ctcp, "DCC SEND ", 9))
		ctcp = text + 10;
	else
		return 1; /* something else, allow */

	if (*ctcp == '"' && *(ctcp+1))
		end = index(ctcp+1, '"');
	else
		end = index(ctcp, ' ');

	/* check if it was fake.. just pass it along then .. */
	if (!end || (end < ctcp))
		return 1; /* allow */

	size_string = (int)(end - ctcp);

	if (!size_string || (size_string > (BUFSIZE - 1)))
		return 1; /* allow */

	strlcpy(realfile, ctcp, size_string+1);

	if ((fl = dcc_isdiscouraged(from, realfile)))
	{
		if (!on_dccallow_list(to, from))
		{
			char *displayfile = dcc_displayfile(realfile);
			sendto_one(from,
				":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
				me.name, RPL_TEXT, from->name, displayfile, to->name, fl->reason);
			sendnotice(from, "User %s is currently not accepting DCC SENDs with such a filename/filetype from you. "
				"Your file %s was not sent.", to->name, displayfile);
			sendnotice(to, "%s (%s@%s) tried to DCC SEND you a file named '%s', the request has been blocked.",
				from->name, from->user->username, GetHost(from), displayfile);
			if (!IsDCCNotice(to))
			{
				SetDCCNotice(to);
				sendnotice(to, "Files like these might contain malicious content (viruses, trojans). "
					"Therefore, you must explicitly allow anyone that tries to send you such files.");
				sendnotice(to, "If you trust %s, and want him/her to send you this file, you may obtain "
					"more information on using the dccallow system by typing '/DCCALLOW HELP'", from->name);
			}
	
			/* if (SHOW_ALLDENIEDDCCS) */
			if (0)
			{
				sendto_snomask(SNO_VICTIM,
					"[DCCALLOW] %s tried to send forbidden file %s (%s) to %s",
					from->name, displayfile, fl->reason, to->name);
			}
			return 0;
		}
	}

	return 1; /* allowed */
}

static int is_accepted(aClient *user, aClient *cptr) {
	Link *lp;
	char sender_host[NICKLEN + USERLEN + HOSTLEN + 5];
	char sender_vhost[NICKLEN + USERLEN + HOSTLEN + 5];

	ircsprintf(sender_host, "%s!%s@%s", user->name, user->user->username, user->user->realhost);
	if (user->user->virthost) {
		ircsprintf(sender_vhost, "%s!%s@%s", user->name, user->user->username, user->user->virthost);
	}

	for (lp = cptr->user->accept; lp; lp = lp->next) {
		if (
			(AcceptIsUser(lp) && (lp->value.cptr == user)) ||
			(AcceptIsMask(lp) && (!match(lp->value.cp, sender_host) || (cptr->user->virthost && !match(lp->value.cp, sender_vhost))))
		) {
			return 1;
		}
	}
	return 0;
}
