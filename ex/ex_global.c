/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 9.9 1995/02/02 15:09:23 bostic Exp $ (Berkeley) $Date: 1995/02/02 15:09:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

enum which {GLOBAL, V};

static int	global __P((SCR *, EXCMDARG *, enum which));

/*
 * ex_global -- [line [,line]] g[lobal][!] /pattern/ [commands]
 *	Exec on lines matching a pattern.
 */
int
ex_global(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (global(sp, cmdp, F_ISSET(cmdp, E_FORCE) ? V : GLOBAL));
}

/*
 * ex_v -- [line [,line]] v /pattern/ [commands]
 *	Exec on lines not matching a pattern.
 */
int
ex_v(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (global(sp, cmdp, V));
}

static int
global(sp, cmdp, cmd)
	SCR *sp;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK abs;
	RANGE *rp;
	EX_PRIVATE *exp;
	recno_t elno, lno;
	regmatch_t match[1];
	regex_t *re, lre;
	size_t clen, len;
	int delim, eval, reflags, replaced, rval;
	char *cb, *ptrn, *p, *t;

	NEEDFILE(sp, cmdp->cmd);

	/*
	 * Skip leading white space.  Historic vi allowed any non-
	 * alphanumeric to serve as the global command delimiter.
	 */
	if (cmdp->argc == 0)
		goto usage;
	for (p = cmdp->argv[0]->bp; isblank(*p); ++p);
	if (*p == '\0' || isalnum(*p)) {
usage:		ex_message(sp, cmdp->cmd, EXM_USAGE);
		return (1);
	}
	delim = *p++;

	/*
	 * Get the pattern string, toss escaped characters.
	 *
	 * QUOTING NOTE:
	 * Only toss an escaped character if it escapes a delimiter.
	 */
	for (ptrn = t = p;;) {
		if (p[0] == '\0' || p[0] == delim) {
			if (p[0] == delim)
				++p;
			/*
			 * !!!
			 * Nul terminate the pattern string -- it's passed
			 * to regcomp which doesn't understand anything else.
			 */
			*t = '\0';
			break;
		}
		if (p[0] == '\\')
			if (p[1] == delim)
				++p;
			else if (p[1] == '\\')
				*t++ = *p++;
		*t++ = *p++;
	}

	/* If the pattern string is empty, use the last one. */
	if (*ptrn == '\0') {
		if (!F_ISSET(sp, S_RE_SEARCH)) {
			ex_message(sp, NULL, EXM_NOPREVRE);
			return (1);
		}
		re = &sp->sre;
	} else {
		/* Set RE flags. */
		reflags = 0;
		if (O_ISSET(sp, O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (O_ISSET(sp, O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Convert vi-style RE's to POSIX 1003.2 RE's. */
		if (re_conv(sp, &ptrn, &replaced))
			return (1);

		/* Compile the RE. */
		re = &lre;
		eval = regcomp(re, ptrn, reflags);

		/* Free up any allocated memory. */
		if (replaced)
			FREE_SPACE(sp, ptrn, 0);

		if (eval) {
			re_error(sp, eval, re);
			return (1);
		}

		/*
		 * Set saved RE.  Historic practice is that
		 * globals set direction as well as the RE.
		 */
		sp->sre = lre;
		sp->searchdir = FORWARD;
		F_SET(sp, S_RE_SEARCH);
	}

	/*
	 * Get a copy of the command string; the default command is print.
	 * Don't worry about a set of <blank>s with no command, that will
	 * default to print in the ex parser.  We need to have two copies
	 * because the ex parser may step on the command string when it's
	 * parsing it.
	 */
	if ((clen = strlen(p)) == 0) {
		p = "pp";
		clen = 1;
	}
	MALLOC_RET(sp, cb, char *, clen * 2);
	memmove(cb, p, clen);

	/*
	 * The global commands sets the substitute RE as well as
	 * the everything-else RE.
	 */
	sp->subre = sp->sre;
	F_SET(sp, S_RE_SUBST);

	/* Set the global flag. */
	F_SET(sp, S_GLOBAL);

	/* The global commands always set the previous context mark. */
	abs.lno = sp->lno;
	abs.cno = sp->cno;
	if (mark_set(sp, ABSMARK1, &abs, 1))
		goto err;

	/*
	 * For each line...  The semantics of global matching are that we first
	 * have to decide which lines are going to get passed to the command,
	 * and then pass them to the command, ignoring other changes.  There's
	 * really no way to do this in a single pass, since arbitrary line
	 * creation, deletion and movement can be done in the ex command.  For
	 * example, a good vi clone test is ":g/X/mo.-3", or "g/X/.,.+1d".
	 * What we do is create linked list of lines that are tracked through
	 * each ex command.  There's a callback routine which the DB interface
	 * routines call when a line is created or deleted.  This doesn't help
	 * the layering much.
	 */
	exp = EXP(sp);
	for (rval = 0, lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {
		/* Someone's unhappy, time to stop. */
		if (INTERRUPTED(sp))
			break;

		/* Get the line and search for a match. */
		if ((t = file_gline(sp, lno, &len)) == NULL) {
			FILE_LERR(sp, lno);
			goto err;
		}
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch(eval = regexec(re, t, 0, match, REG_STARTEND)) {
		case 0:
			if (cmd == V)
				continue;
			break;
		case REG_NOMATCH:
			if (cmd == GLOBAL)
				continue;
			break;
		default:
			re_error(sp, eval, re);
			goto err;
		}

		/* If follows the last entry, extend the last entry's range. */
		if ((rp = exp->rangeq.cqh_last) != (void *)&exp->rangeq &&
		    rp->stop == lno - 1) {
			++rp->stop;
			continue;
		}

		/* Allocate a new range, and append it to the list. */
		CALLOC(sp, rp, RANGE *, 1, sizeof(RANGE));
		if (rp == NULL)
			goto err;
		rp->start = rp->stop = lno;
		CIRCLEQ_INSERT_TAIL(&exp->rangeq, rp, q);
	}

	exp = EXP(sp);
	exp->range_lno = OOBLNO;
	for (;;) {
		/*
		 * Start at the beginning of the range each time, it may have
		 * been changed (or exhausted) if lines were inserted/deleted.
		 */
		if ((rp = exp->rangeq.cqh_first) == (void *)&exp->rangeq)
			break;
		if (rp->start > rp->stop) {
			CIRCLEQ_REMOVE(&exp->rangeq, exp->rangeq.cqh_first, q);
			free(rp);
			continue;
		}

		/*
		 * Make a new copy and execute the command, setting the cursor
		 * to the line so that relative addressing works.  This means
		 * that the cursor moves to the last line sent to the command,
		 * by default, even if the command fails.
		 */
		exp->range_lno = sp->lno = rp->start++;
		memmove(cb + clen, cb, clen);
		if (ex_cmd(sp, cb + clen, clen, 0))
			goto err;

		/* Someone's unhappy, time to stop. */
		if (INTERRUPTED(sp))
			break;
		/*
		 * If we've exited or switched to a new file/screen, the rest of
		 * the global command is discarded.  This is historic practice,
		 * and changing it would be difficult.
		 */
		if (F_ISSET(sp,
		    S_EXIT | S_EXIT_FORCE | S_GLOBAL_ABORT | S_SSWITCH))
			break;
	}

	/* Set the cursor to the new value, making sure it exists. */
	if (exp->range_lno != OOBLNO) {
		if (file_lline(sp, &lno))
			return (1);
		sp->lno =
		    lno < exp->range_lno ? (lno ? lno : 1) : exp->range_lno;
	}
	if (0) {
err:		rval = 1;
	}

	/* Command we ran may have set the autoprint flag, clear it. */
	F_CLR(exp, EX_AUTOPRINT);

	/* Clear the global flag. */
	F_CLR(sp, S_GLOBAL);

	/* Free any remaining ranges and the command buffer. */
	while ((rp = exp->rangeq.cqh_first) != (void *)&exp->rangeq) {
		CIRCLEQ_REMOVE(&exp->rangeq, exp->rangeq.cqh_first, q);
		free(rp);
	}
	free(cb);
	return (rval);
}

/*
 * global_insdel --
 *	Update the ranges based on an insertion or deletion.
 */
void
global_insdel(sp, op, lno)
	SCR *sp;
	lnop_t op;
	recno_t lno;
{
	EX_PRIVATE *exp;
	RANGE *nrp, *rp;

	exp = EXP(sp);

	switch (op) {
	case LINE_APPEND:
		return;
	case LINE_DELETE:
		for (rp = exp->rangeq.cqh_first;
		    rp != (void *)&exp->rangeq; rp = nrp) {
			nrp = rp->q.cqe_next;
			/* If range less than the line, ignore it. */
			if (rp->stop < lno)
				continue;
			/* If range greater than the line, decrement range. */
			if (rp->start > lno) {
				--rp->start;
				--rp->stop;
				continue;
			}
			/* Lno is inside the range, decrement the end point. */
			if (rp->start > --rp->stop) {
				CIRCLEQ_REMOVE(&exp->rangeq, rp, q);
				free(rp);
			}
		}
		break;
	case LINE_INSERT:
		for (rp = exp->rangeq.cqh_first;
		    rp != (void *)&exp->rangeq; rp = rp->q.cqe_next) {
			/* If range less than the line, ignore it. */
			if (rp->stop < lno)
				continue;
			/* If range greater than the line, increment range. */
			if (rp->start >= lno) {
				++rp->start;
				++rp->stop;
				continue;
			}
			/*
			 * Lno is inside the range, so the range must be split.
			 * Since we're inserting a new element, neither range
			 * can be exhausted.
			 */
			CALLOC(sp, nrp, RANGE *, 1, sizeof(RANGE));
			if (nrp == NULL) {
				F_SET(sp, S_INTERRUPTED);
				return;
			}
			nrp->start = lno + 1;
			nrp->stop = rp->stop + 1;
			rp->stop = lno - 1;
			CIRCLEQ_INSERT_AFTER(&exp->rangeq, rp, nrp, q);
			rp = nrp;
		}
		break;
	case LINE_RESET:
		return;
	}
	/*
	 * If the command deleted/inserted lines, the cursor moves to
	 * the line after the deleted/inserted line.
	 */
	exp->range_lno = lno;
}
