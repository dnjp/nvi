/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 9.4 1994/12/16 12:43:04 bostic Exp $ (Berkeley) $Date: 1994/12/16 12:43:04 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 */
int
v_right(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;

	if (file_gline(sp, vp->m_start.lno, &len) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, NULL);
		else
			FILE_LERR(sp, vp->m_start.lno);
		return (1);
	}

	/* It's always illegal to move right on empty lines. */
	if (len == 0) {
		v_eol(sp, NULL);
		return (1);
	}

	/*
	 * Non-motion commands move to the end of the range.  Delete and
	 * yank stay at the start.  Ignore others.  Adjust the end of the
	 * range for motion commands.
	 *
	 * !!!
	 * Historically, "[cdsy]l" worked at the end of a line.  Also,
	 * EOL is a count sink.
	 */
	vp->m_stop.cno = vp->m_start.cno +
	    (F_ISSET(vp, VC_C1SET) ? vp->count : 1);
	if (vp->m_start.cno == len - 1 && !ISMOTION(vp)) {
		v_eol(sp, NULL);
		return (1);
	}
	if (vp->m_stop.cno >= len) {
		vp->m_stop.cno = len - 1;
		vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	} else if (ISMOTION(vp)) {
		--vp->m_stop.cno;
		vp->m_final = vp->m_start;
	} else
		vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_dollar -- [count]$
 *	Move to the last column.
 */
int
v_dollar(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;

	/*
	 * !!!
	 * A count moves down count - 1 rows, so, "3$" is the same as "2j$".
	 */
	if ((F_ISSET(vp, VC_C1SET) ? vp->count : 1) != 1) {
		/*
		 * !!!
		 * Historically, if the $ is a motion, and deleting from
		 * at or before the first non-blank of the line, it's a
		 * line motion, and the line motion flag is set.
		 */
		vp->m_stop.cno = 0;
		if (nonblank(sp, vp->m_start.lno, &vp->m_stop.cno))
			return (1);
		if (ISMOTION(vp) && vp->m_start.cno <= vp->m_stop.cno)
			F_SET(vp, VM_LMODE);

		--vp->count;
		if (v_down(sp, vp))
			return (1);
	}

	/*
	 * !!!
	 * Historically, it was illegal to use $ as a motion command on
	 * an empty line.  Unfortunately, even though C was historically
	 * aliased to c$, it was special cased to work on empty lines.
	 * Since we alias C to c$ too, we have a problem.  To fix it, we
	 * let c$ go through, on the assumption that it's not a problem
	 * to let it work.
	 */
	if (file_gline(sp, vp->m_stop.lno, &len) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno == 0) {
			if (!ISCMD(vp->rkp, 'c')) {
				v_eol(sp, NULL);
				return (1);
			}
			return (0);
		}
		FILE_LERR(sp, vp->m_start.lno);
		return (1);
	}

	if (len == 0 && !ISCMD(vp->rkp, 'c')) {
		v_eol(sp, NULL);
		return (1);
	}

	/*
	 * Non-motion commands move to the end of the range.  Delete
	 * and yank stay at the start.  Ignore others.
	 */
	vp->m_stop.cno = len ? len - 1 : 0;
	vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	return (0);
}
