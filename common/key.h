/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 8.35 1994/03/16 08:05:58 bostic Exp $ (Berkeley) $Date: 1994/03/16 08:05:58 $
 */

/* Structure to return a character and associated information. */
struct _ch {
	CHAR_T	 ch;		/* Character. */

#define K_NOTUSED	 0
#define	K_CARAT		 1
#define	K_CNTRLD	 2
#define	K_CNTRLR	 3
#define	K_CNTRLT	 4
#define	K_CNTRLZ	 5
#define	K_COLON	 	 6
#define	K_CR		 7
#define	K_ESCAPE	 8
#define	K_FORMFEED	 9
#define	K_NL		10
#define	K_RIGHTBRACE	11
#define	K_RIGHTPAREN	12
#define	K_TAB		13
#define	K_VERASE	14
#define	K_VINTR		15
#define	K_VKILL		16
#define	K_VLNEXT	17
#define	K_VWERASE	18
#define	K_ZERO		19
	u_char	 value;		/* Special character flag values. */

#define	CH_ABBREVIATED	0x01	/* Character from an abbreviation. */
#define	CH_NOMAP	0x02	/* Do not attempt to map the character. */
#define	CH_QUOTED	0x04	/* Character is already quoted. */
	u_char	 flags;
};

/*
 * Structure for the key input buffer.
 *
 * MAX_MAP_COUNT was chosen based on the vi maze script, which remaps
 * characters roughly 250 times.
 */
struct _ibuf {
	CHAR_T	*ch;		/* Array of characters. */
	u_char	*chf;		/* Array of character flags (CH_*). */
#define	MAX_MAP_COUNT	270	/* Maximum times a character can remap. */
	u_short *cmap;		/* Number of times character has been mapped. */

	size_t	 cnt;		/* Count of remaining characters. */
	size_t	 nelem;		/* Numer of array elements. */
	size_t	 next;		/* Offset of next array entry. */
};
				/* Return if more keys in queue. */
#define	KEYS_WAITING(sp)	((sp)->gp->tty->cnt)
#define	MAPPED_KEYS_WAITING(sp)						\
	(KEYS_WAITING(sp) && sp->gp->tty->cmap[sp->gp->tty->next])

/*
 * Structure to name a character.  Used both as an interface to the
 * screen and to name objects named by characters in error messages.
 */
struct _chname {
	char	*name;		/* Character name. */
	u_char	 len;		/* Length of the character name. */
};

/*
 * Routines that return a key as a side-effect return:
 *
 *	INP_OK		Returning a character; must be 0.
 *	INP_EOF		EOF.
 *	INP_ERR		Error.
 *
 * The vi structure depends on the key routines being able to return INP_EOF
 * multiple times without failing -- eventually enough things will end due to
 * INP_EOF that vi will reach the command level for the screen, at which point
 * the exit flags will be set and vi will exit.
 */
enum input	{ INP_OK=0, INP_EOF, INP_ERR };

/*
 * Routines that return a confirmation return:
 *
 *	CONF_NO		User answered no.
 *	CONF_QUIT	User answered quit, eof or an error.
 *	CONF_YES	User answered yes.
 */
enum confirm	{ CONF_NO, CONF_QUIT, CONF_YES };

/*
 * Ex/vi commands are generally separated by whitespace characters.  We
 * can't use the standard isspace(3) macro because it returns true for
 * characters like ^K in the ASCII character set.  The 4.4BSD isblank(3)
 * macro does exactly what we want, but it's not portable yet.
 *
 * XXX
 * Note side effect, ch is evaluated multiple times.
 */
#ifndef isblank
#define	isblank(ch)	((ch) == ' ' || (ch) == '\t')
#endif

/* The "standard" tab width, for displaying things to users. */
#define	STANDARD_TAB	6

/* Various special characters, messages. */
#define	CURSOR_CH	' '			/* Cursor character. */
#define	END_CH		'$'			/* End of a range. */
#define	HEX_CH		'x'			/* Leading hex number. */
#define	LITERAL_CH	'\026'			/* Standard literal ^V. */
#define	NOT_DIGIT_CH	'a'			/* A non-isdigit() character. */
#define	NO_CH		'n'			/* No. */
#define	QUIT_CH		'q'			/* Quit. */
#define	YES_CH		'y'			/* Yes. */

#define	CONFSTRING	"confirm? [ynq]"
#define	CONTMSG		"Enter return to continue: "
#define	CONTMSG_I	"Enter return to continue [q to quit]: "

/* Flags describing how input is handled. */
#define	TXT_AICHARS	0x0000001	/* Leading autoindent chars. */
#define	TXT_ALTWERASE	0x0000002	/* Option: altwerase. */
#define	TXT_APPENDEOL	0x0000004	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x0000008	/* Autoindent set this line. */
#define	TXT_BEAUTIFY	0x0000010	/* Only printable characters. */
#define	TXT_BS		0x0000020	/* Backspace returns the buffer. */
#define	TXT_CNTRLD	0x0000040	/* Control-D is a special command. */
#define	TXT_CNTRLT	0x0000080	/* Control-T is an indent special. */
#define	TXT_CR		0x0000100	/* CR returns the buffer. */
#define	TXT_EMARK	0x0000200	/* End of replacement mark. */
#define	TXT_ESCAPE	0x0000400	/* Escape returns the buffer. */
#define	TXT_INFOLINE	0x0000800	/* Editing the info line. */
#define	TXT_MAPCOMMAND	0x0001000	/* Apply the command map. */
#define	TXT_MAPINPUT	0x0002000	/* Apply the input map. */
#define	TXT_MAPNODIGIT	0x0004000	/* Return to a digit. */
#define	TXT_NLECHO	0x0008000	/* Echo the newline. */
#define	TXT_OVERWRITE	0x0010000	/* Overwrite characters. */
#define	TXT_PROMPT	0x0020000	/* Display a prompt. */
#define	TXT_RECORD	0x0040000	/* Record for replay. */
#define	TXT_REPLACE	0x0080000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x0100000	/* Replay the last input. */
#define	TXT_RESOLVE	0x0200000	/* Resolve the text into the file. */
#define	TXT_SHOWMATCH	0x0400000	/* Option: showmatch. */
#define	TXT_TTYWERASE	0x0800000	/* Option: ttywerase. */
#define	TXT_WRAPMARGIN	0x1000000	/* Option: wrapmargin. */

/* Support keyboard routines. */
int		__term_key_val __P((SCR *, ARG_CHAR_T));
void		term_ab_flush __P((SCR *, char *));
int		term_init __P((SCR *));
enum input	term_key __P((SCR *, CH *, u_int));
int		term_key_queue __P((SCR *));
void		term_map_flush __P((SCR *, char *));
int		term_push __P((SCR *, CHAR_T *, size_t, u_int, u_int));
enum input	term_user_key __P((SCR *, CH *));
int		term_waiting __P((SCR *));
