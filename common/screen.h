/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: screen.h,v 10.14 1996/04/03 14:32:16 bostic Exp $ (Berkeley) $Date: 1996/04/03 14:32:16 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  The row
 * minimum is fixed at 1 (the svi code can share a line between the text line
 * and the colon command/message line).  Column calculation is a lot trickier.
 * For example, you have to have enough columns to display the line number,
 * not to mention guaranteeing that tabstop and shiftwidth values are smaller
 * than the current column value.  It's simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is almost certainly wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 1
#define	MINIMUM_SCREEN_COLS	20

/*
 * SCR --
 *	The screen structure.  To the extent possible, all screen information
 *	is stored in the various private areas.  The only information here
 *	is used by global routines or is shared by too many screens.
 */
struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	CIRCLEQ_ENTRY(_scr) q;		/* Screens. */

	int	 id;			/* Screen id #. */
	int	 refcnt;		/* Reference count. */

	GS	*gp;			/* Pointer to global area. */
	SCR	*nextdisp;		/* Next display screen. */
	SCR	*ccl_parent;		/* Colon command-line parent screen. */
	EXF	*ep;			/* Screen's current EXF structure. */

	FREF	*frp;			/* FREF being edited. */
	char	**argv;			/* NULL terminated file name array. */
	char	**cargv;		/* Current file name. */

	u_long	 ccnt;			/* Command count. */
	u_long	 q_ccnt;		/* Quit or ZZ command count. */

					/* Screen's: */
	size_t	 rows;			/* 1-N: number of rows. */
	size_t	 cols;			/* 1-N: number of columns. */
	size_t	 t_rows;		/* 1-N: cur number of text rows. */
	size_t	 t_maxrows;		/* 1-N: max number of text rows. */
	size_t	 t_minrows;		/* 1-N: min number of text rows. */
	size_t	 woff;			/* 0-N: screen offset in frame. */

					/* Cursor's: */
	recno_t	 lno;			/* 1-N: file line. */
	size_t	 cno;			/* 0-N: file character in line. */

	size_t	 rcm;			/* Vi: 0-N: Most attractive column. */

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_DELETED	2		/* Deleted lines. */
#define	L_JOINED	3		/* Joined lines. */
#define	L_MOVED		4		/* Moved lines. */
#define	L_SHIFT		5		/* Shift lines. */
#define	L_YANKED	6		/* Yanked lines. */
	recno_t	 rptlchange;		/* Ex/vi: last L_CHANGED lno. */
	recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	TEXTH	 tiq;			/* Ex/vi: text input queue. */

	SCRIPT	*script;		/* Vi: script mode information .*/

	recno_t	 defscroll;		/* Vi: ^D, ^U scroll information. */

					/* Display character. */
	CHAR_T	 cname[MAX_CHARACTER_COLUMNS + 1];
	size_t	 clen;			/* Length of display character. */

	enum {				/* Vi editor mode. */
	    SM_APPEND = 0, SM_CHANGE, SM_COMMAND, SM_INSERT,
	    SM_REPLACE } showmode;

	void	*ex_private;		/* Ex private area. */
	void	*sex_private;		/* Ex screen private area. */
	void	*vi_private;		/* Vi private area. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	char	*alt_name;		/* Ex/vi: alternate file name. */

	CHAR_T	 at_lbuf;		/* Ex/vi: Last executed at buffer. */

					/* Ex/vi: re_compile flags. */
#define	RE_C_SEARCH	0x0001		/* Compile search replacement. */
#define	RE_C_SUBST	0x0002		/* Compile substitute replacement. */
#define	RE_C_TAG	0x0004		/* Compile tag. */

#define	RE_WSTART	"[[:<:]]"	/* Ex/vi: not-in-word search pattern. */
#define	RE_WSTOP	"[[:>:]]"
					/* Ex/vi: flags to search routines. */
#define	SEARCH_EOL	0x0001		/* Offset past EOL is okay. */
#define	SEARCH_FILE	0x0002		/* Search the entire file. */
#define	SEARCH_INCR	0x0004		/* Search incrementally. */
#define	SEARCH_MSG	0x0008		/* Display search warning messages. */
#define	SEARCH_PARSE	0x0010		/* Parse the search pattern. */
#define	SEARCH_SET	0x0020		/* Set search direction. */
#define	SEARCH_TAG	0x0040		/* Search pattern is a tag pattern. */

					/* Ex/vi: RE information. */
	dir_t	 searchdir;		/* Last file search direction. */
	char	*re;			/* Search RE (uncompiled form). */
	regex_t	 re_c;			/* Search RE (compiled form). */
	size_t	 re_len;		/* Search RE length. */
	char	*subre;			/* Substitute RE (uncompiled form.) */
	regex_t	 subre_c;		/* Substitute RE (compiled form.) */
	size_t	 subre_len;		/* Substitute RE length. */
	char	*repl;			/* Substitute replacement. */
	size_t	 repl_len;		/* Substitute replacement length.*/
	size_t	*newl;			/* Newline offset array. */
	size_t	 newl_len;		/* Newline array size. */
	size_t	 newl_cnt;		/* Newlines in replacement. */
	u_int8_t c_suffix;		/* Edcompatible 'c' suffix value. */
	u_int8_t g_suffix;		/* Edcompatible 'g' suffix value. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Ex/vi: Options. */

/*
 * Screen flags.
 *
 * Editor screens.
 */
#define	S_EX		0x00000001	/* Ex editor. */
#define	S_VI		0x00000002	/* Vi editor. */

/*
 * Screen formatting flags, first major, then minor.
 *
 * S_SCR_EX
 *	Ex screen, i.e. cooked mode.
 * S_SCR_VI
 *	Vi screen, i.e. raw mode.
 * S_SCR_EXWROTE
 *	The editor had to write on the screen behind curses' back, and we can't
 *	let curses change anything until the user agrees, e.g. entering the
 *	commands :!utility followed by :set.  We have to switch back into the
 *	vi "editor" to read the user's command input, but we can't touch the
 *	rest of the screen because it's known to be wrong.
 * S_SCR_REFORMAT
 *	The expected presentation of the lines on the screen have changed,
 *	requiring that the intended screen lines be recalculated.  Implies
 *	S_SCR_REDRAW.
 * S_SCR_REDRAW
 *	The screen doesn't correctly represent the file; repaint it.  Note,
 *	setting S_SCR_REDRAW in the current window causes *all* windows to
 *	be repainted.
 * S_SCR_CENTER
 *	If the current line isn't already on the screen, center it.
 * S_SCR_TOP
 *	If the current line isn't already on the screen, put it at the to@.
 */
#define	S_SCR_EX	0x00000004	/* Screen is in ex mode. */
#define	S_SCR_VI	0x00000008	/* Screen is in vi mode. */
#define	S_SCR_EXWROTE	0x00000010	/* Ex overwrite: see comment above. */
#define	S_SCR_REFORMAT	0x00000020	/* Reformat (refresh). */
#define	S_SCR_REDRAW	0x00000040	/* Refresh. */

#define	S_SCR_CENTER	0x00000080	/* Center the line if not visible. */
#define	S_SCR_TOP	0x00000100	/* Top the line if not visible. */

/* Screen/file changes. */
#define	S_EXIT		0x00000200	/* Exiting (not forced). */
#define	S_EXIT_FORCE	0x00000400	/* Exiting (forced). */
#define	S_FSWITCH	0x00000800	/* Switch underlying files. */
#define	S_SSWITCH	0x00001000	/* Switch screens. */

#define	S_ARGNOFREE	0x00002000	/* Argument list wasn't allocated. */
#define	S_ARGRECOVER	0x00004000	/* Argument list is recovery files. */
#define	S_AT_SET	0x00008000	/* Last at buffer set. */
#define	S_COMEDIT	0x00010000	/* Colon command-line edit window. */
#define	S_EX_DONTWAIT	0x00020000	/* Ex: don't wait for the user. */
#define	S_EX_GLOBAL	0x00040000	/* Ex: executing a global command. */
#define	S_EX_SILENT	0x00080000	/* Ex: batch script. */
#define	S_INPUT		0x00100000	/* Doing text input. */
#define	S_INPUT_INFO	0x00200000	/* Doing text input on info line. */
#define	S_RE_SEARCH	0x00800000	/* Search RE has been compiled. */
#define	S_RE_SUBST	0x01000000	/* Substitute RE has been compiled. */
#define	S_SCRIPT	0x02000000	/* Shell script window. */
#define	S_STATUS	0x04000000	/* Schedule welcome message. */
	u_int32_t flags;
};
