/*
**
** version.h
** UnrealIRCd
** $Id: version.h,v 1.1.1.1.2.16 2004/07/03 19:04:18 syzop Exp $
*/
#ifndef __versioninclude
#define __versioninclude 1

/* 
 * Mark of settings
 */
#ifdef DEBUGMODE
#define DEBUGMODESET "+(debug)"
#else
#define DEBUGMODESET ""
#endif
 /**/
#ifdef DEBUG
#define DEBUGSET "(Debug)"
#else
#define DEBUGSET ""
#endif
     /**/
#define COMPILEINFO DEBUGMODESET DEBUGSET
/*
 * Version Unreal3.2.1-DalNetRU2.2.9.1
 */
#define UnrealProtocol 		2304
#define PATCH1  		"3"
#define PATCH2  		".2"
#define PATCH3  		".1"
#define PATCH4  		"-DalNetRU2.2.9.1"
#define PATCH5  		"-Patch1"
#define PATCH6  		"-IRCLine.RU"
#define PATCH7  		""
#define PATCH8  		COMPILEINFO
#define PATCH9  		""
/* release header */
#define Rh BASE_VERSION
#define VERSIONONLY		PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7
#endif /* __versioninclude */
