/*
 * Copyright (C) 1984-2004  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Functions to define the character set
 * and do things specific to the character set.
 */

#include "less.h"
#if HAVE_LOCALE
#include <locale.h>
#include <ctype.h>
#endif

#define UTF_IS_CONT(c)  (((c) & 0xC0) == 0x80)

public int utf_mode = 0;

/*
 * Predefined character sets,
 * selected by the LESSCHARSET environment variable.
 */
struct charset {
	char *name;
	int *p_flag;
	char *desc;
} charsets[] = {
	{ "ascii",	NULL,       "8bcccbcc18b95.b" },
	{ "dos",	NULL,       "8bcccbcc12bc5b223.b" },
	{ "ebcdic",	NULL,       "5bc6bcc7bcc41b.9b7.9b5.b..8b6.10b6.b9.7b9.8b8.17b3.3b9.7b9.8b8.6b10.b.b.b." },
	{ "IBM-1047",	NULL,       "4cbcbc3b9cbccbccbb4c6bcc5b3cbbc4bc4bccbc191.b" },
	{ "iso8859",	NULL,       "8bcccbcc18b95.33b." },
	{ "koi8-r",	NULL,       "8bcccbcc18b95.b128." },
	{ "next",	NULL,       "8bcccbcc18b95.bb125.bb" },
	{ "utf-8",	&utf_mode,  "8bcccbcc18b." },
	{ "windows",	NULL,       "8bcccbcc12bc5b95.b." },
	{ NULL, NULL, NULL }
};

struct cs_alias {
	char *name;
	char *oname;
} cs_aliases[] = {
	{ "latin1",	"iso8859" },
	{ "latin9",	"iso8859" },
	{ NULL, NULL }
};

#define	IS_BINARY_CHAR	01
#define	IS_CONTROL_CHAR	02

static char chardef[256];
static char *binfmt = NULL;
public int binattr = AT_STANDOUT;


/*
 * Define a charset, given a description string.
 * The string consists of 256 letters,
 * one for each character in the charset.
 * If the string is shorter than 256 letters, missing letters
 * are taken to be identical to the last one.
 * A decimal number followed by a letter is taken to be a 
 * repetition of the letter.
 *
 * Each letter is one of:
 *	. normal character
 *	b binary character
 *	c control character
 */
	static void
ichardef(s)
	char *s;
{
	register char *cp;
	register int n;
	register char v;

	n = 0;
	v = 0;
	cp = chardef;
	while (*s != '\0')
	{
		switch (*s++)
		{
		case '.':
			v = 0;
			break;
		case 'c':
			v = IS_CONTROL_CHAR;
			break;
		case 'b':
			v = IS_BINARY_CHAR|IS_CONTROL_CHAR;
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (10 * n) + (s[-1] - '0');
			continue;

		default:
			error("invalid chardef", NULL_PARG);
			quit(QUIT_ERROR);
			/*NOTREACHED*/
		}

		do
		{
			if (cp >= chardef + sizeof(chardef))
			{
				error("chardef longer than 256", NULL_PARG);
				quit(QUIT_ERROR);
				/*NOTREACHED*/
			}
			*cp++ = v;
		} while (--n > 0);
		n = 0;
	}

	while (cp < chardef + sizeof(chardef))
		*cp++ = v;
}

/*
 * Define a charset, given a charset name.
 * The valid charset names are listed in the "charsets" array.
 */
	static int
icharset(name)
	register char *name;
{
	register struct charset *p;
	register struct cs_alias *a;

	if (name == NULL || *name == '\0')
		return (0);

	/* First see if the name is an alias. */
	for (a = cs_aliases;  a->name != NULL;  a++)
	{
		if (strcmp(name, a->name) == 0)
		{
			name = a->oname;
			break;
		}
	}

	for (p = charsets;  p->name != NULL;  p++)
	{
		if (strcmp(name, p->name) == 0)
		{
			ichardef(p->desc);
			if (p->p_flag != NULL)
				*(p->p_flag) = 1;
			return (1);
		}
	}

	error("invalid charset name", NULL_PARG);
	quit(QUIT_ERROR);
	/*NOTREACHED*/
	return (0);
}

#if HAVE_LOCALE
/*
 * Define a charset, given a locale name.
 */
	static void
ilocale()
{
	register int c;

	setlocale(LC_ALL, "");
	for (c = 0;  c < (int) sizeof(chardef);  c++)
	{
		if (isprint(c))
			chardef[c] = 0;
		else if (iscntrl(c))
			chardef[c] = IS_CONTROL_CHAR;
		else
			chardef[c] = IS_BINARY_CHAR|IS_CONTROL_CHAR;
	}
}
#endif

/*
 * Define the printing format for control chars.
 */
   	public void
setbinfmt(s)
	char *s;
{
	if (s == NULL || *s == '\0')
		s = "*s<%X>";
	/*
	 * Select the attributes if it starts with "*".
	 */
	if (*s == '*')
	{
		switch (s[1])
		{
		case 'd':  binattr = AT_BOLD;      break;
		case 'k':  binattr = AT_BLINK;     break;
		case 's':  binattr = AT_STANDOUT;  break;
		case 'u':  binattr = AT_UNDERLINE; break;
		default:   binattr = AT_NORMAL;    break;
		}
		s += 2;
	}
	binfmt = s;
}

/*
 * Initialize charset data structures.
 */
	public void
init_charset()
{
	register char *s;

	s = lgetenv("LESSBINFMT");
	setbinfmt(s);
	
	/*
	 * See if environment variable LESSCHARSET is defined.
	 */
	s = lgetenv("LESSCHARSET");
	if (icharset(s))
		return;
	/*
	 * LESSCHARSET is not defined: try LESSCHARDEF.
	 */
	s = lgetenv("LESSCHARDEF");
	if (s != NULL && *s != '\0')
	{
		ichardef(s);
		return;
	}

#if HAVE_STRSTR
	/*
	 * Check whether LC_ALL, LC_CTYPE or LANG look like UTF-8 is used.
	 */
	if ((s = lgetenv("LC_ALL")) != NULL ||
	    (s = lgetenv("LC_CTYPE")) != NULL ||
	    (s = lgetenv("LANG")) != NULL)
	{
		if (strstr(s, "UTF-8") != NULL || strstr(s, "utf-8") != NULL)
			if (icharset("utf-8"))
				return;
	}
#endif

#if HAVE_LOCALE
	/*
	 * Use setlocale.
	 */
	ilocale();
#else
#if MSDOS_COMPILER
	/*
	 * Default to "dos".
	 */
	(void) icharset("dos");
#else
	/*
	 * Default to "latin1".
	 */
	(void) icharset("latin1");
#endif
#endif
}

/*
 * Is a given character a "binary" character?
 */
	public int
binary_char(c)
	unsigned char c;
{
	c &= 0377;
	return (chardef[c] & IS_BINARY_CHAR);
}

/*
 * Is a given character a "control" character?
 */
	public int
control_char(c)
	int c;
{
	c &= 0377;
	return (chardef[c] & IS_CONTROL_CHAR);
}

/*
 * Return the printable form of a character.
 * For example, in the "ascii" charset '\3' is printed as "^C".
 */
	public char *
prchar(c)
	int c;
{
	static char buf[8];

	c &= 0377;
	if (!control_char(c))
		sprintf(buf, "%c", c);
	else if (c == ESC)
		sprintf(buf, "ESC");
#if IS_EBCDIC_HOST
	else if (!binary_char(c) && c < 64)
		sprintf(buf, "^%c",
		/*
		 * This array roughly inverts CONTROL() #defined in less.h,
	 	 * and should be kept in sync with CONTROL() and IBM-1047.
 	 	 */
		"@ABC.I.?...KLMNO"
		"PQRS.JH.XY.."
		"\\]^_"
		"......W[.....EFG"
		"..V....D....TU.Z"[c]);
#else
  	else if (c < 128 && !control_char(c ^ 0100))
  		sprintf(buf, "^%c", c ^ 0100);
#endif
	else
		sprintf(buf, binfmt, c);
	return (buf);
}

/*
 * Get the length of a UTF-8 character in bytes.
 */
	public int
utf_len(ch)
	char ch;
{
	if ((ch & 0x80) == 0)
		return 1;
	if ((ch & 0xE0) == 0xC0)
		return 2;
	if ((ch & 0xF0) == 0xE0)
		return 3;
	if ((ch & 0xF8) == 0xF0)
		return 4;
	if ((ch & 0xFC) == 0xF8)
		return 5;
	if ((ch & 0xFE) == 0xFC)
		return 6;
	/* Invalid UTF-8 encoding. */
	return 1;
}

/*
 * Get the value of a UTF-8 character.
 */
	public WCHAR
get_wchar(p)
	char *p;
{
	switch (utf_len(p[0]))
	{
	case 1:
	default:
		return (WCHAR)
			(p[0] & 0xFF);
	case 2:
		return (WCHAR) (
			((p[0] & 0x1F) << 6) |
			(p[1] & 0x3F));
	case 3:
		return (WCHAR) (
			((p[0] & 0x0F) << 12) |
			((p[1] & 0x3F) << 6) |
			(p[2] & 0x3F));
	case 4:
		return (WCHAR) (
			((p[0] & 0x07) << 18) |
			((p[1] & 0x3F) << 12) | 
			((p[2] & 0x3F) << 6) | 
			(p[3] & 0x3F));
	case 5:
		return (WCHAR) (
			((p[0] & 0x03) << 24) |
			((p[1] & 0x3F) << 18) | 
			((p[2] & 0x3F) << 12) | 
			((p[3] & 0x3F) << 6) | 
			(p[4] & 0x3F));
	case 6:
		return (WCHAR) (
			((p[0] & 0x01) << 30) |
			((p[1] & 0x3F) << 24) | 
			((p[2] & 0x3F) << 18) | 
			((p[3] & 0x3F) << 12) | 
			((p[4] & 0x3F) << 6) | 
			(p[5] & 0x3F));
	}
}

/*
 * Step forward or backward one character in a string.
 */
	public WCHAR
step_char(pp, dir, limit)
	char **pp;
	signed int dir;
	char *limit;
{
	WCHAR ch;
	char *p = *pp;

	if (!utf_mode)
	{
		/* It's easy if chars are one byte. */
		if (dir > 0)
			ch = (WCHAR) ((p < limit) ? *p++ : 0);
		else
			ch = (WCHAR) ((p > limit) ? *--p : 0);
	} else if (dir > 0)
	{
		if (p + utf_len(*p) > limit)
			ch = 0;
		else
		{
			ch = get_wchar(p);
			p++;
			while (UTF_IS_CONT(*p))
				p++;
		}
	} else
	{
		while (p > limit && UTF_IS_CONT(p[-1]))
			p--;
		if (p > limit)
			ch = get_wchar(--p);
		else
			ch = 0;
	}
	*pp = p;
	return ch;
}

/*
 * Unicode characters data
 */
struct wchar_range { WCHAR first, last; };

static struct wchar_range comp_table[] = {
	{0x300,0x357}, {0x35d,0x36f}, {0x483,0x486}, {0x488,0x489}, 
	{0x591,0x5a1}, {0x5a3,0x5b9}, {0x5bb,0x5bd}, {0x5bf,0x5bf}, 
	{0x5c1,0x5c2}, {0x5c4,0x5c4}, {0x610,0x615}, {0x64b,0x658}, 
	{0x670,0x670}, {0x6d6,0x6dc}, {0x6de,0x6e4}, {0x6e7,0x6e8}, 
	{0x6ea,0x6ed}, {0x711,0x711}, {0x730,0x74a}, {0x7a6,0x7b0}, 
	{0x901,0x902}, {0x93c,0x93c}, {0x941,0x948}, {0x94d,0x94d}, 
	{0x951,0x954}, {0x962,0x963}, {0x981,0x981}, {0x9bc,0x9bc}, 
	{0x9c1,0x9c4}, {0x9cd,0x9cd}, {0x9e2,0x9e3}, {0xa01,0xa02}, 
	{0xa3c,0xa3c}, {0xa41,0xa42}, {0xa47,0xa48}, {0xa4b,0xa4d}, 
	{0xa70,0xa71}, {0xa81,0xa82}, {0xabc,0xabc}, {0xac1,0xac5}, 
	{0xac7,0xac8}, {0xacd,0xacd}, {0xae2,0xae3}, {0xb01,0xb01}, 
	{0xb3c,0xb3c}, {0xb3f,0xb3f}, {0xb41,0xb43}, {0xb4d,0xb4d}, 
	{0xb56,0xb56}, {0xb82,0xb82}, {0xbc0,0xbc0}, {0xbcd,0xbcd}, 
	{0xc3e,0xc40}, {0xc46,0xc48}, {0xc4a,0xc4d}, {0xc55,0xc56}, 
	{0xcbc,0xcbc}, {0xcbf,0xcbf}, {0xcc6,0xcc6}, {0xccc,0xccd}, 
	{0xd41,0xd43}, {0xd4d,0xd4d}, {0xdca,0xdca}, {0xdd2,0xdd4}, 
	{0xdd6,0xdd6}, {0xe31,0xe31}, {0xe34,0xe3a}, {0xe47,0xe4e}, 
	{0xeb1,0xeb1}, {0xeb4,0xeb9}, {0xebb,0xebc}, {0xec8,0xecd}, 
	{0xf18,0xf19}, {0xf35,0xf35}, {0xf37,0xf37}, {0xf39,0xf39}, 
	{0xf71,0xf7e}, {0xf80,0xf84}, {0xf86,0xf87}, {0xf90,0xf97}, 
	{0xf99,0xfbc}, {0xfc6,0xfc6}, {0x102d,0x1030}, {0x1032,0x1032}, 
	{0x1036,0x1037}, {0x1039,0x1039}, {0x1058,0x1059}, 
	{0x1712,0x1714}, {0x1732,0x1734}, {0x1752,0x1753}, 
	{0x1772,0x1773}, {0x17b7,0x17bd}, {0x17c6,0x17c6}, 
	{0x17c9,0x17d3}, {0x17dd,0x17dd}, {0x180b,0x180d}, 
	{0x18a9,0x18a9}, {0x1920,0x1922}, {0x1927,0x1928}, 
	{0x1932,0x1932}, {0x1939,0x193b}, {0x20d0,0x20ea}, 
	{0x302a,0x302f}, {0x3099,0x309a}, {0xfb1e,0xfb1e}, 
	{0xfe00,0xfe0f}, {0xfe20,0xfe23}, {0x1d167,0x1d169}, 
	{0x1d17b,0x1d182}, {0x1d185,0x1d18b}, {0x1d1aa,0x1d1ad}, 
	{0xe0100,0xe01ef}, 
};

static struct wchar_range comb_table[] = {
	{0x0644,0x0622}, {0x0644,0x0623}, {0x0644,0x0625}, {0x0644,0x0627}, 
};

/*
 * Is a character a UTF-8 composing character?
 * If a composing character follows any char, the two combine into one glyph.
 */
	public int
is_composing_char(ch)
	WCHAR ch;
{
	int hi;
	int lo;

	/* Binary search in the table. */
	if (ch < comp_table[0].first)
		return 0;
	lo = 0;
	hi = (sizeof(comp_table) / sizeof(*comp_table)) - 1;
	while (lo <= hi)
	{
		int mid = (lo + hi) / 2;
		if (ch > comp_table[mid].last)
			lo = mid + 1;
		else if (ch < comp_table[mid].first)
			hi = mid - 1;
		else
			return 1;
	}
	return 0;
}

/*
 * Is a character a UTF-8 combining character?
 * A combining char acts like an ordinary char, but if it follows
 * a specific char (not any char), the two combine into one glyph.
 */
	public int
is_combining_char(ch1, ch2)
	WCHAR ch1;
	WCHAR ch2;
{
	/* The table is small; use linear search. */
	int i;
	for (i = 0;  i < sizeof(comb_table)/sizeof(*comb_table);  i++)
	{
		if (ch1 == comb_table[i].first &&
		    ch2 == comb_table[i].last)
			return 1;
	}
	return 0;
}
