// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
 * robust glob pattern matcher
 * ozan s. yigit/dec 1994
 * public domain
 *
 * glob patterns:
 *	*	matches zero or more characters
 *	?	matches any single character
 *	[set]	matches any character in the set
 *	[^set]	matches any character NOT in the set
 *		where a set is a group of characters or ranges. a range
 *		is written as two characters separated with a hyphen: a-z denotes
 *		all characters between a to z inclusive.
 *	[-set]	set matches a literal hypen and any character in the set
 *	[]set]	matches a literal close bracket and any character in the set
 *
 *	char	matches itself except where char is '*' or '?' or '['
 *	\char	matches char, including any pattern character
 *
 * examples:
 *	a*c		ac abc abbc ...
 *	a?c		acc abc aXc ...
 *	a[a-z]c		aac abc acc ...
 *	a[-a-z]c	a-c aac abc ...
 *
 * $Log: glob.c,v $
 * Revision 1.3  1995/09/14  23:24:23  oz
 * removed boring test/main code.
 *
 * Revision 1.2  94/12/11  10:38:15  oz
 * cset code fixed. it is now robust and interprets all
 * variations of cset [i think] correctly, including [z-a] etc.
 *
 * Revision 1.1  94/12/08  12:45:23  oz
 * Initial revision
 *
 * 10/05/04 ian
 * Copied and slightly modified from http://www.cse.yorku.ca/~oz/glob.bun
 */

#ifndef _PATTERNMATCHER_H_
#define _PATTERNMATCHER_H_

#define PM_NEGATE '^'     /* std cset negation char */

#define PM_TRUE   1
#define PM_FALSE  0

static int PatternMatch(const char* str, const char* p)
{
	int negate;
	int match;
	int c;

	while (*p)
	{
		if (!*str && *p != '*')
			return PM_FALSE;

		switch (c = *p++)
		{

		case '*':
			while (*p == '*')
				p++;

			if (!*p)
				return PM_TRUE;

			if (*p != '?' && *p != '[' && *p != '\\')
				while (*str && *p != *str)
					str++;

			while (*str)
			{
				if (PatternMatch(str, p))
					return PM_TRUE;
				str++;
			}
			return PM_FALSE;

		case '?':
			if (*str)
				break;
			return PM_FALSE;

		// set specification is inclusive, that is [a-z] is a, z and
		// everything in between. this means [z-a] may be interpreted
		// as a set that contains z, a and nothing in between.

		case '[':
			if (*p != PM_NEGATE)
				negate = PM_FALSE;
			else
			{
				negate = PM_TRUE;
				p++;
			}

			match = PM_FALSE;

			while (!match && (c = *p++))
			{
				if (!*p)
					return PM_FALSE;
				if (*p == '-')    /* c-c */
				{
					if (!*++p)
						return PM_FALSE;
					if (*p != ']')
					{
						if (*str == c || *str == *p ||
						    (*str > c && *str < *p))
							match = PM_TRUE;
					}
					else      /* c-] */
					{
						if (*str >= c)
							match = PM_TRUE;
						break;
					}
				}
				else        /* cc or c] */
				{
					if (c == *str)
						match = PM_TRUE;
					if (*p != ']')
					{
						if (*p == *str)
							match = PM_TRUE;
					}
					else
						break;
				}
			}

			if (negate == match)
				return PM_FALSE;

			// if there is a match, skip past the cset and continue on
			while (*p && *p != ']')
				p++;
			if (!*p++)  /* oops! */
				return PM_FALSE;
			break;

		case '\\':
			if (*p)
				c = *p++;
		default:
			if (c != *str)
				return PM_FALSE;
			break;

		}
		str++;
	}

	return !*str;
}

#undef PM_NEGATE
#undef PM_TRUE
#undef PM_FALSE

#endif //_PATTERNMATCHER_H_
