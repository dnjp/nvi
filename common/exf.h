/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.19 1992/11/11 18:31:03 bostic Exp $ (Berkeley) $Date: 1992/11/11 18:31:03 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

#include <regex.h>
#include <db.h>

#include "mark.h"
#include "cut.h"

typedef struct exf {
	struct exf *next, *prev;	/* Linked list of files. */

					/* Screen state. */
	recno_t top;			/* 1-N:     Physical screen top line. */
	recno_t otop;			/* 1-N: Old physical screen top line. */
	recno_t lno;			/* 1-N:     Physical cursor line. */
	recno_t olno;			/* 0-N: Old physical cursor line. */
	size_t cno;			/* 0-N:     Physical cursor col. */
	size_t ocno;			/* 0-N: Old physical cursor col. */
	size_t scno;			/* 0-N: Logical cursor col. */
	size_t shift;			/* 0-N: Shift count. */
	size_t rcm;			/* 0-N: Column suck. */
	size_t lines;			/* Physical lines. */
	size_t cols;			/* Physical columns. */
	u_char cwidth;			/* Width of current character. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_char rcmflags;
	enum confirmation (*s_confirm) __P((struct exf *, MARK *, MARK *));
	int (*s_end) __P((struct exf *));

					/* Underlying database state. */
	DB *db;				/* File db structure. */
	u_char *c_lp;			/* Cached line. */
	size_t c_len;			/* Cached line length. */
	recno_t c_lno;			/* Cached line number. */
	recno_t c_nlines;		/* Lines in the file. */

	DB *log;			/* Log db structure. */
	u_char *l_lp;			/* Log buffer. */
	size_t l_len;			/* Log buffer length. */

	FILE *stdfp;			/* Ex/vi output function. */

	regex_t sre;			/* Current RE. */

	recno_t	rptlines;		/* Lines modified by command. */
	char *rptlabel;			/* How lines modified. */

	char *name;			/* File name. */
	size_t nlen;			/* File name length. */

#define	F_IGNORE	0x001		/* File inserted later. */
#define	F_IN_GLOBAL	0x002		/* In global command. */
#define	F_MODIFIED	0x004		/* File was modified. */
#define	F_NAMECHANGED	0x008		/* File name changed. */
#define	F_NEWSESSION	0x010		/* File newly edited. */
#define	F_NONAME	0x020		/* File has no name. */
#define	F_RDONLY	0x040		/* File is read-only. */
#define	F_RE_SET	0x080		/* RE has been set. */

#define	FF_SET(ep, f)	(ep)->flags |= (f)
#define	FF_CLR(ep, f)	(ep)->flags &= ~(f)
#define	FF_ISSET(ep, f)	((ep)->flags & (f))
	u_int flags;
} EXF;

extern EXF *curf;			/* Current file. */

typedef struct {
	EXF *next, *prev;
} EXFLIST;

/* Remove from the file chain. */
#define	rmexf(p) {							\
        (p)->prev->next = (p)->next;					\
        (p)->next->prev = (p)->prev;					\
}

/* Insert into the file chain after p. */
#define insexf(p, hp) {							\
        (p)->next = (hp)->next;						\
        (p)->prev = (EXF *)(hp);					\
        (hp)->next->prev = (p);						\
        (hp)->next = (p);						\
}

/* Insert into the file chain before p. */
#define instailexf(p, hp) {						\
	(hp)->prev->next = (p);						\
	(p)->prev = (hp)->prev;						\
	(hp)->prev = (p);						\
	(p)->next = (EXF *)(hp);					\
}

#define	GETLINE_ERR(lno) {						\
	bell();								\
	msg("Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, lno);				\
}

#define	MODIFY_CHECK(ep, force) {					\
	if ((ep)->flags & F_MODIFIED && !force) {			\
		if (ISSET(O_AUTOWRITE)) {				\
			if (file_sync((ep), force))			\
				return (1);				\
		} else {						\
	msg("Modified since last write; write or use ! to override.");	\
			return (1);					\
		}							\
	}								\
}

#define	MODIFY_WARN(ep) {						\
	if ((ep)->flags & F_MODIFIED && ISSET(O_WARN))			\
		msg("Modified since last write.");			\
}

#define	PANIC {								\
	msg("No file state!");						\
	mode = MODE_QUIT;						\
	return (1);							\
}

/* File routines. */
EXF	*file_first __P((int));
void	 file_init __P((void));
int	 file_ins __P((EXF *, char *, int));
EXF	*file_last __P((void));
EXF	*file_locate __P((char *));
EXF	*file_next __P((EXF *, int));
EXF	*file_prev __P((EXF *, int));
int	 file_set __P((int, char *[]));
int	 file_start __P((EXF *));
int	 file_stop __P((EXF *, int));
int	 file_sync __P((EXF *, int));

/* Line routines. */
int	 file_aline __P((EXF *, recno_t, u_char *, size_t));
int	 file_dline __P((EXF *, recno_t));
u_char	*file_gline __P((EXF *, recno_t, size_t *));
int	 file_ibresolv __P((EXF *, IB *));
int	 file_iline __P((EXF *, recno_t, u_char *, size_t));
recno_t	 file_lline __P((EXF *));
int	 file_sline __P((EXF *, recno_t, u_char *, size_t));

#endif /* !_EXF_H_ */
