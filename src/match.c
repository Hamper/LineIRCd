/*
 *   Unreal Internet Relay Chat Daemon, src/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
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


#include "struct.h"
#include "common.h"
#include "sys.h"

ID_Copyright("(C) 1990 Jarkko Oikarinen");

unsigned char my_tolower(unsigned char c) {
	if ((c >= 'A') && (c <= 'Z')) {
		return (c | 32);
	}
	if ((c >= 192) && (c <= 223)) {
		return (c + 32);
	}
	if (c == 168) {
		return (184);
	}
	return (c);
}

unsigned char my_toupper(unsigned char c) {
	if ((c >= 'a') && (c <= 'z')) {
		return (c & 223);
	}
	if ((c >= 224) && (c <= 255)) {
		return (c - 32);
	}
	if (c == 184) {
		return (168);
	}
	return (c);
}

char my_tolower_rus2eng(char c) {
	char rus[]= { 234, 226, 241, 240, 237, 245, 224, 229, 242, 238, 236, 243 };
	char eng[]="kbcphxaetomy";
	int i;
	char a = my_tolower(c);
	for (i=0; i <= 11; i++) {
		if (rus[i] == a) {
			a = eng[i];
			break;
		}
	}
	return a;
}

void my_str_tolower_rus2eng(char *s1) {
	while (*s1)
	{
		*s1=my_tolower_rus2eng(*s1);
		*s1++;
	}
}

/*
 *  Compare if a given string (name) matches the given
 *  mask (which can contain wild cards: '*' - match any
 *  number of chars, '?' - match any single character.
 *
 *	return	0, if match
 *		1, if no match
 */

/*
 * match()
 *  written by binary
 */
#define lc(x) tolower(x)
static inline int match2(char *mask, char *name)
{
	u_char *m;		/* why didn't the old one use registers */
	u_char *n;		/* because registers suck -- codemastr */
	u_char cm;
	u_char *wsn;
	u_char *wsm;

	m = (u_char *)mask;

	cm = *m;

	n = (u_char *)name;
	if (cm == '*')
	{
		if (m[1] == '\0')	/* mask is just "*", so true */
			return 0;
	}
	else if (cm != '?' && lc(cm) != lc(*n))
		return 1;	/* most likely first chars won't match */
	else if ((cm == '_') && (*n != ' ') && (*n != '_'))
		return 1;	/* false: '_' but first character not '_' nor ' ' */
	else if ((*m == '\0') && (*n == '\0'))
		return 0;  /* true: both are empty */
	else if (*n == '\0')
		return 1; /* false: name is empty */
	else
	{
		m++;
		n++;
	}
	cm = lc(*m);
	wsm = (char *)NULL;
	wsn = (char *)NULL;
	while (1)
	{
		if (cm == '*')	/* found the * wildcard */
		{
			m++;	/* go to next char of mask */
			if (!*m)	/* if at end of mask, */
				return 0;	/* function becomes true. */
			while (*m == '*')	/* while the char at m is "*" */
			{
				m++;	/* go to next char of mask */
				if (!*m)	/* if at end of mask, */
					return 0;	/* function becomes true. */
			}
			cm = *m;
			if (cm == '\\')	/* don't do ? checking if a \ */
			{
				
				cm = *(++m);	/* just skip this char, no ? checking */
				/* In case of something like: '*\', return false. */
				if (!*m)
					return 1;
			}
			else if (cm == '?')	/* if it's a ? */
			{
				do
				{
					m++;	/* go to the next char of both */
					if (!*n)
						return 1; /* false: no character left */
					n++;
					if (!*n)	/* if end of test string... */
						return (!*m ? 0 : 1);	/* true if end of mask str, else false */
				}
				while (*m == '?');	/* while we have ?'s */
				cm = *m;
				if (!cm)	/* last char of mask is ?, so it's true */
					return 0;
			}
			cm = lc(cm);
			while (lc(*n) != cm)
			{	/* compare */
				if (!*n)	/* if at end of n string */
					return 1;	/* function becomes false. */
				n++;	/* go to next char of n */
			}
			wsm = m;	/* mark after where wildcard found */
			cm = lc(*(++m));	/* go to next mask char */
			wsn = n;	/* mark spot first char was found */
			n++;	/* go to next char of n */
			continue;
		}
		if (cm == '?')	/* found ? wildcard */
		{
			cm = lc(*(++m));	/* just skip and go to next */
			if (!*n)
				return 1; /* false: no character left */
			n++;
			if (!*n)	/* return true if end of both, */
				return (cm ? 1 : 0);	/* false if end of test str only */
			continue;
		}
		if (cm == '_')	/* found _: check for '_' or ' ' */
		{
			cm = lc(*(++m));	/* just skip and go to next */
			if ((*n != ' ') && (*n != '_'))
				return 1; /* false: didnt match or no character left */
			n++;
			if (!*n)	/* return true if end of both, */
				return (cm ? 1 : 0);	/* false if end of test str only */
			continue;
		}
		if (cm == '\\')	/* next char will not be a wildcard. */
		{		/* skip wild checking, don't continue */
			cm = lc(*(++m));
			n++;
		}
		/* Complicated to read, but to save CPU time.  Every ounce counts. */
		if (lc(*n) != cm)	/* if the current chars don't equal, */
		{
			if (!wsm)	/* if there was no * wildcard, */
				return 1;	/* function becomes false. */
			n = wsn + 1;	/* start on char after the one we found last */
			m = wsm;	/* set m to the spot after the "*" */
			cm = lc(*m);
			while (cm != lc(*n))
			{	/* compare them */
				if (!*n)	/* if we reached end of n string, */
					return 1;	/* function becomes false. */
				n++;	/* go to next char of n */
			}
			wsn = n;	/* mark spot first char was found */
		}
		if (!cm)	/* cm == cn, so if !cm, then we've */
			return 0;	/* reached end of BOTH, so it matches */
		m++;		/* go to next mask char */
		n++;		/* go to next testing char */
		cm = lc(*m);	/* pointers are slower */
	}
}
#undef lc

#define lc(x) my_tolower_rus2eng(x)
static inline int match2_rus(char *mask, char *name)
{
	u_char *m;		/* why didn't the old one use registers */
	u_char *n;		/* because registers suck -- codemastr */
	u_char cm;
	u_char *wsn;
	u_char *wsm;

	m = (u_char *)mask;

	cm = *m;

	n = (u_char *)name;
	if (cm == '*')
	{
		if (m[1] == '\0')	/* mask is just "*", so true */
			return 0;
	}
	else if (cm != '?' && lc(cm) != lc(*n))
		return 1;	/* most likely first chars won't match */
	else if ((cm == '_') && (*n != ' ') && (*n != '_'))
		return 1;	/* false: '_' but first character not '_' nor ' ' */
	else if ((*m == '\0') && (*n == '\0'))
		return 0;  /* true: both are empty */
	else if (*n == '\0')
		return 1; /* false: name is empty */
	else
	{
		m++;
		n++;
	}
	cm = lc(*m);
	wsm = (char *)NULL;
	wsn = (char *)NULL;
	while (1)
	{
		if (cm == '*')	/* found the * wildcard */
		{
			m++;	/* go to next char of mask */
			if (!*m)	/* if at end of mask, */
				return 0;	/* function becomes true. */
			while (*m == '*')	/* while the char at m is "*" */
			{
				m++;	/* go to next char of mask */
				if (!*m)	/* if at end of mask, */
					return 0;	/* function becomes true. */
			}
			cm = *m;
			if (cm == '\\')	/* don't do ? checking if a \ */
			{
				
				cm = *(++m);	/* just skip this char, no ? checking */
				/* In case of something like: '*\', return false. */
				if (!*m)
					return 1;
			}
			else if (cm == '?')	/* if it's a ? */
			{
				do
				{
					m++;	/* go to the next char of both */
					if (!*n)
						return 1; /* false: no character left */
					n++;
					if (!*n)	/* if end of test string... */
						return (!*m ? 0 : 1);	/* true if end of mask str, else false */
				}
				while (*m == '?');	/* while we have ?'s */
				cm = *m;
				if (!cm)	/* last char of mask is ?, so it's true */
					return 0;
			}
			cm = lc(cm);
			while (lc(*n) != cm)
			{	/* compare */
				if (!*n)	/* if at end of n string */
					return 1;	/* function becomes false. */
				n++;	/* go to next char of n */
			}
			wsm = m;	/* mark after where wildcard found */
			cm = lc(*(++m));	/* go to next mask char */
			wsn = n;	/* mark spot first char was found */
			n++;	/* go to next char of n */
			continue;
		}
		if (cm == '?')	/* found ? wildcard */
		{
			cm = lc(*(++m));	/* just skip and go to next */
			if (!*n)
				return 1; /* false: no character left */
			n++;
			if (!*n)	/* return true if end of both, */
				return (cm ? 1 : 0);	/* false if end of test str only */
			continue;
		}
		if (cm == '_')	/* found _: check for '_' or ' ' */
		{
			cm = lc(*(++m));	/* just skip and go to next */
			if ((*n != ' ') && (*n != '_'))
				return 1; /* false: didnt match or no character left */
			n++;
			if (!*n)	/* return true if end of both, */
				return (cm ? 1 : 0);	/* false if end of test str only */
			continue;
		}
		if (cm == '\\')	/* next char will not be a wildcard. */
		{		/* skip wild checking, don't continue */
			cm = lc(*(++m));
			n++;
		}
		/* Complicated to read, but to save CPU time.  Every ounce counts. */
		if (lc(*n) != cm)	/* if the current chars don't equal, */
		{
			if (!wsm)	/* if there was no * wildcard, */
				return 1;	/* function becomes false. */
			n = wsn + 1;	/* start on char after the one we found last */
			m = wsm;	/* set m to the spot after the "*" */
			cm = lc(*m);
			while (cm != lc(*n))
			{	/* compare them */
				if (!*n)	/* if we reached end of n string, */
					return 1;	/* function becomes false. */
				n++;	/* go to next char of n */
			}
			wsn = n;	/* mark spot first char was found */
		}
		if (!cm)	/* cm == cn, so if !cm, then we've */
			return 0;	/* reached end of BOTH, so it matches */
		m++;		/* go to next mask char */
		n++;		/* go to next testing char */
		cm = lc(*m);	/* pointers are slower */
	}
}
#undef lc

/*
 * collapse a pattern string into minimal components.
 * This particular version is "in place", so that it changes the pattern
 * which is to be reduced to a "minimal" size.
 */
char *collapse(char *pattern)
{
	char *s;
	char *s1;
	char *t;

	s = pattern;

	if (BadPtr(pattern))
		return pattern;
	/*
	 * Collapse all \** into \*, \*[?]+\** into \*[?]+
	 */
	for (; *s; s++)
		if (*s == '\\')
		{
			if (!*(s + 1))
				break;
			else
				s++;
		}
		else if (*s == '*')
		{
			if (*(t = s1 = s + 1) == '*')
				while (*t == '*')
					t++;
			else if (*t == '?')
				for (t++, s1++; *t == '*' || *t == '?'; t++)
					if (*t == '?')
						*s1++ = *t;
			while ((*s1++ = *t++))
				;
		}
	return pattern;
}


/*
 *  Case insensitive comparison of two NULL terminated strings.
 *
 *	returns	 0, if s1 equal to s2
 *		<0, if s1 lexicographically less than s2
 *		>0, if s1 lexicographically greater than s2
 */
int  smycmp(char *s1, char *s2)
{
	u_char *str1;
	u_char *str2;
	int  res;

	str1 = (u_char *)s1;
	str2 = (u_char *)s2;

	while ((res = toupper(*str1) - toupper(*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}

static int chrcmpn(char c1, char c2) {
	if (my_tolower_rus2eng(c1) == my_tolower_rus2eng(c2)) {
		return 0;
	} else {
		return 1;
	}
}

int my_smycmp(char *s1, char *s2) {
	u_char *str1;
	u_char *str2;
	int  res;

	str1 = (u_char *)s1;
	str2 = (u_char *)s2;

	while ((res = chrcmpn(*str1,*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}

int  myncmp(char *str1, char *str2, int n)
{
	u_char *s1;
	u_char *s2;
	int  res;

	s1 = (u_char *)str1;
	s2 = (u_char *)str2;

	while ((res = toupper(*s1) - toupper(*s2)) == 0)
	{
		s1++;
		s2++;
		n--;
		if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
			return 0;
	}
	return (res);
}

u_char char_atribs[] = {
/* 0-7 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 8-12 */ CNTRL, CNTRL | SPACE, CNTRL | SPACE, CNTRL | SPACE,
	CNTRL | SPACE,
/* 13-15 */ CNTRL | SPACE, CNTRL, CNTRL,
/* 16-23 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 24-31 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* space */ PRINT | SPACE,
/* !"#$%&'( */ PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
/* )*+,-./ */ PRINT, PRINT, PRINT, PRINT, PRINT | ALLOW, PRINT | ALLOW,
	PRINT,
/* 012 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 345 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 678 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 9:; */ PRINT | DIGIT | ALLOW, PRINT, PRINT,
/* <=>? */ PRINT, PRINT, PRINT, PRINT,
/* @ */ PRINT,
/* ABC */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* DEF */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* GHI */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* JKL */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* MNO */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* PQR */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* STU */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* VWX */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* YZ[ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT | ALPHA,
/* \]^ */ PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
/* _`  */ PRINT | ALLOW, PRINT,
/* abc */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* def */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* ghi */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* jkl */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* mno */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* pqr */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* stu */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* vwx */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* yz{ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT | ALPHA,
/* |}~ */ PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
/* del */ 0,
/* 80-8f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90-9f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0-af */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0-bf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0-cf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0-df */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0-ef */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0-ff */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Old match() */
int _match(char *mask, char *name) {
	return match2(mask,name);
}


/* Old match() plus some optimizations from bahamut */
int match(char *mask, char *name) {
	if (mask[0] == '*' && mask[1] == '!') {
		mask += 2;
		while (*name != '!' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
		
	if (mask[0] == '*' && mask[1] == '@') {
		mask += 2;
		while (*name != '@' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
	return match2(mask,name);
}

int match_rus(char *mask, char *name) {
	if (mask[0] == '*' && mask[1] == '!') {
		mask += 2;
		while (*name != '!' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
		
	if (mask[0] == '*' && mask[1] == '@') {
		mask += 2;
		while (*name != '@' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
	return match2_rus(mask,name);
}

char my_str_buf[4096]; // Size must be not less than cleanstr size (badwords.c)

char *my_str_convert(char *s1) {
	int i;
	strcpy(my_str_buf, s1);
	for (i=0; my_str_buf[i]; i++)
		my_str_buf[i]=my_tolower_rus2eng(my_str_buf[i]);
	return my_str_buf;
}
