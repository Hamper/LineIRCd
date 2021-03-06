/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dynconf.h
 *   Copyright (C) 1999 Carsten Munk
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
 *
 *   $Id: dynconf.h,v 1.1.1.1.2.16 2004/07/03 19:04:15 syzop Exp $
 */


#define DYNCONF_H
/* config level */
#define DYNCONF_CONF_VERSION "1.5"
#define DYNCONF_NETWORK_VERSION "2.2"

typedef struct zNetwork aNetwork;
struct zNetwork {
	unsigned x_inah:1;
	char *x_ircnetwork;
	char *x_ircnet005;
	char *x_defserv;
	char *x_services_name;
	char *x_oper_host;
	char *x_admin_host;
	char *x_locop_host;
	char *x_sadmin_host;
	char *x_netadmin_host;
	char *x_hidden_host;
	char *x_local_postfix;
	unsigned x_local_postfix_for_all:1;
	unsigned x_no_hide_local:1;
	char *x_prefix_quit;
	char *x_helpchan;
	char *x_stats_server;
};

enum UHAllowed { UHALLOW_ALWAYS, UHALLOW_NOCHANS, UHALLOW_REJOIN, UHALLOW_NEVER };

struct ChMode {
        long mode;
#ifdef EXTCMODE
	long extmodes;
	char *extparams[EXTCMODETABLESZ];
#endif
#ifdef NEWCHFLOODPROT
	ChanFloodProt	floodprot;
#else
        unsigned short  msgs;
        unsigned short  per; 
        unsigned char   kmode;
#endif
};

typedef struct _OperStat {
	struct _OperStat *prev, *next;
	char *flag;
} OperStat;

typedef struct zConfiguration aConfiguration;
struct zConfiguration {
	unsigned som:1;
	unsigned hide_ulines:1;
	unsigned flat_map:1;
	unsigned allow_chatops:1;
	unsigned webtv_support:1;
	unsigned no_oper_hiding:1;
	unsigned ident_check:1;
	unsigned fail_oper_warn:1;
	unsigned show_connect_info:1;
	unsigned dont_resolve:1;
	unsigned use_ban_version:1;
	unsigned mkpasswd_for_everyone:1;
	unsigned allow_part_if_shunned:1;
	unsigned use_egd;
	long host_timeout;
	int  host_retries;
	char *name_server;
#ifdef THROTTLING
	long throttle_period;
	char throttle_count;
#endif
	char *kline_address;
	char *gline_address;
	long conn_modes;
	long oper_modes;
	char *oper_snomask;
	char *user_snomask;
	char *auto_join_chans;
	char *oper_auto_join_chans;
	char *oper_only_stats;
	OperStat *oper_only_stats_ext;
	int  maxchannelsperuser;
	int  maxdccallow;
	int  anti_spam_quit_message_time;
	char *egd_path;
	char *static_quit;
	char *static_part;
#ifdef USE_SSL
	char *x_server_cert_pem;
	char *x_server_key_pem;
	char *trusted_ca_file;
	long ssl_options;
#elif defined(_WIN32)
	void *bogus1, *bogus2, *bogus3;
	long bogus4;
#endif
	enum UHAllowed userhost_allowed;
	char *restrict_usermodes;
	char *restrict_channelmodes;
	char *restrict_extendedbans;
	char *channel_command_prefix;
	long unknown_flood_bantime;
	long unknown_flood_amount;
	struct ChMode modes_on_join;
#ifdef NO_FLOOD_AWAY
	unsigned char away_count;
	long away_period;
#endif
	unsigned char nick_count;
	long nick_period;
	int ident_connect_timeout;
	int ident_read_timeout;
	long default_bantime;
	int who_limit;
	int silence_limit;
	int accept_limit;
	int callerid_time;
#ifdef NEWCHFLOODPROT
	unsigned char modef_default_unsettime;
	unsigned char modef_max_unsettime;
#endif
	long ban_version_tkl_time;
	long spamfilter_ban_time;
	char *spamfilter_ban_reason;
	char *spamfilter_virus_help_channel;
	char spamfilter_vchan_deny;
	SpamExcept *spamexcept;
	char *spamexcept_line;
	aNetwork network;
};

#ifndef DYNCONF_C
extern MODVAR aConfiguration iConf;
#endif

#define KLINE_ADDRESS		iConf.kline_address
#define GLINE_ADDRESS		iConf.gline_address
#define CONN_MODES			iConf.conn_modes
#define OPER_MODES			iConf.oper_modes
#define OPER_SNOMASK			iConf.oper_snomask
#define CONNECT_SNOMASK			iConf.user_snomask
#define SHOWOPERMOTD			iConf.som
#define HIDE_ULINES			iConf.hide_ulines
#define FLAT_MAP			iConf.flat_map
#define ALLOW_CHATOPS			iConf.allow_chatops
#define MAXCHANNELSPERUSER		iConf.maxchannelsperuser
#define MAXDCCALLOW			iConf.maxdccallow
#define WEBTV_SUPPORT			iConf.webtv_support
#define NO_OPER_HIDING			iConf.no_oper_hiding
#define DONT_RESOLVE			iConf.dont_resolve
#define AUTO_JOIN_CHANS			iConf.auto_join_chans
#define OPER_AUTO_JOIN_CHANS		iConf.oper_auto_join_chans
#define HOST_TIMEOUT			iConf.host_timeout
#define HOST_RETRIES			iConf.host_retries
#define NAME_SERVER			iConf.name_server
#define IDENT_CHECK			iConf.ident_check
#define FAILOPER_WARN			iConf.fail_oper_warn
#define SHOWCONNECTINFO			iConf.show_connect_info
#define OPER_ONLY_STATS			iConf.oper_only_stats
#define ANTI_SPAM_QUIT_MSG_TIME		iConf.anti_spam_quit_message_time
#define USE_EGD				iConf.use_egd
#define EGD_PATH			iConf.egd_path

#define ircnetwork			iConf.network.x_ircnetwork
#define ircnet005			iConf.network.x_ircnet005
#define defserv				iConf.network.x_defserv
#define SERVICES_NAME		iConf.network.x_services_name
#define oper_host			iConf.network.x_oper_host
#define admin_host			iConf.network.x_admin_host
#define locop_host			iConf.network.x_locop_host
#define sadmin_host			iConf.network.x_sadmin_host
#define netadmin_host		iConf.network.x_netadmin_host
#define techadmin_host		iConf.network.x_techadmin_host
#define hidden_host			iConf.network.x_hidden_host
#define local_postfix		iConf.network.x_local_postfix
#define local_postfix_for_all	iConf.network.x_local_postfix_for_all
#define no_hide_local		iConf.network.x_no_hide_local
#define helpchan			iConf.network.x_helpchan
#define STATS_SERVER			iConf.network.x_stats_server
#define iNAH				iConf.network.x_inah
#define prefix_quit			iConf.network.x_prefix_quit
#define SSL_SERVER_CERT_PEM		(iConf.x_server_cert_pem ? iConf.x_server_cert_pem : "server.cert.pem")
#define SSL_SERVER_KEY_PEM		(iConf.x_server_key_pem ? iConf.x_server_key_pem : "server.key.pem")

#define STATIC_QUIT			iConf.static_quit
#define STATIC_PART			iConf.static_part
#define UHOST_ALLOWED			iConf.userhost_allowed
#define RESTRICT_USERMODES		iConf.restrict_usermodes
#define RESTRICT_CHANNELMODES		iConf.restrict_channelmodes
#define RESTRICT_EXTENDEDBANS		iConf.restrict_extendedbans
#ifdef THROTTLING
#define THROTTLING_PERIOD		iConf.throttle_period
#define THROTTLING_COUNT		iConf.throttle_count
#endif
#define USE_BAN_VERSION			iConf.use_ban_version
#define UNKNOWN_FLOOD_BANTIME		iConf.unknown_flood_bantime
#define UNKNOWN_FLOOD_AMOUNT		iConf.unknown_flood_amount
#define MODES_ON_JOIN			iConf.modes_on_join.mode

#ifdef NO_FLOOD_AWAY
#define AWAY_PERIOD			iConf.away_period
#define AWAY_COUNT			iConf.away_count
#endif
#define NICK_PERIOD			iConf.nick_period
#define NICK_COUNT			iConf.nick_count

#define IDENT_CONNECT_TIMEOUT	iConf.ident_connect_timeout
#define IDENT_READ_TIMEOUT		iConf.ident_read_timeout

#define MKPASSWD_FOR_EVERYONE	iConf.mkpasswd_for_everyone
#define CHANCMDPFX iConf.channel_command_prefix

#define DEFAULT_BANTIME			iConf.default_bantime
#define WHOLIMIT			iConf.who_limit

#ifdef NEWCHFLOODPROT
#define MODEF_DEFAULT_UNSETTIME	iConf.modef_default_unsettime
#define MODEF_MAX_UNSETTIME		iConf.modef_max_unsettime
#endif

#define ALLOW_PART_IF_SHUNNED	iConf.allow_part_if_shunned

#define BAN_VERSION_TKL_TIME	iConf.ban_version_tkl_time
#define SILENCE_LIMIT (iConf.silence_limit ? iConf.silence_limit : 15)
#define ACCEPT_LIMIT (iConf.accept_limit ? iConf.accept_limit : 30)
#define CALLERID_TIME (iConf.callerid_time ? iConf.callerid_time : 60)

#define SPAMFILTER_BAN_TIME		iConf.spamfilter_ban_time
#define SPAMFILTER_BAN_REASON	iConf.spamfilter_ban_reason
#define SPAMFILTER_VIRUSCHAN	iConf.spamfilter_virus_help_channel
#define SPAMFILTER_VIRUSCHANDENY	iConf.spamfilter_vchan_deny
#define SPAMFILTER_EXCEPT		iConf.spamexcept_line
