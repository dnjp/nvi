/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 8.4 1993/09/16 09:38:36 bostic Exp $ (Berkeley) $Date: 1993/09/16 09:38:36 $
 */

typedef struct _tagf {				/* Tag file. */
	char	*fname;				/* Tag file name. */
#define	TAGF_DNE	0x01			/* Didn't exist. */
#define	TAGF_DNE_WARN	0x02			/* DNE error reported. */
	u_char	 flags;
} TAGF;

typedef struct _tag {				/* Tag stack. */
	struct _tag *next, *prev;		/* Linked list of tags. */
	FREF	*frp;				/* Saved file name. */
	recno_t	 lno;				/* Saved line number. */
	size_t	 cno;				/* Saved column number. */
} TAG;

int	ex_tagfirst __P((SCR *, char *));
