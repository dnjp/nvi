/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.19 1992/11/04 10:42:38 bostic Exp $ (Berkeley) $Date: 1992/11/04 10:42:38 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"
#include "extern.h"

static size_t exlinecount, extotalcount;

/*
 * v_ex --
 *	Execute strings of ex commands.
 */
int
v_ex(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags, key;
	u_char *p;

	v_startex();
	for (flags = GB_BS;; flags |= GB_NLECHO) {
		/*
		 * Get an ex command; echo the newline on any prompts after
		 * the first.  We may have to overwrite the command later;
		 * get the length for later.
		 */
		if (v_gb(ISSET(O_PROMPT) ? ':' : 0, &p, &len, flags) ||
		    p == NULL)
			break;

		(void)ex_cstring(p, len, 0);
		(void)fflush(curf->stdfp);
		if (extotalcount <= 1) {
			needexerase = 1;
			break;
		}
			
		(void)msg_vflush(curf);

		standout();
		(void)v_exwrite(curf, CONTMSG, sizeof(CONTMSG) - 1);
		standend();
		refresh();

		/* The user may continue in ex mode by entering a ':'. */
                if ((key = getkey(0)) != ':')
                        break;

		/* Reset current count. */
		exlinecount = 0;
	}

	/*
	 * The file may have changed, if so, the main vi loop will take
	 * care of it.  Otherwise, the only cursor modifications will be
	 * real, however, the underlying line may have changed; don't
	 * trust anything.  This section of code has been a remarkably
	 * fertile place for bugs.  Don't trust ANYTHING.
	 */
	if (!(curf->flags & F_NEWSESSION)) {
		v_leaveex();
		curf->olno = OOBLNO;
		rp->lno = curf->lno;
		if (file_gline(curf, curf->lno, &len) == NULL) {
			rp->cno = 0;
			if (file_lline(curf) != 0) {
				GETLINE_ERR(curf->lno);
				return (1);
			}
		} else
			rp->cno = MIN(curf->cno, len ? len - 1 : 0);
	}
	return (0);
}
		
/*
 * v_startex --
 *	Vi calls ex.
 */
void
v_startex()
{
	exlinecount = extotalcount = 0;
}

/*
 * v_leaveex --
 *	Ex returns to vi.
 */
void
v_leaveex()
{
	if (extotalcount == 0)
		return;
	if (extotalcount >= SCREENSIZE(curf)) {
		scr_ref(curf);
		return;
	}
	while (--extotalcount)
		(void)scr_update(curf,
		    curf->lines - extotalcount, NULL, 0, LINE_RESET);
}

/*
 * v_exwrite --
 *	Write out the ex messages.
 */
int
v_exwrite(cookie, line, llen)
	void *cookie;
	const char *line;
	int llen;
{
	EXF *ep;
	int ch, len, rlen;
	char *p;

	for (ep = cookie, rlen = llen; llen;) {
		/* Newline delimits. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/* Fold if past end-of-screen. */
		if (len > ep->cols)
			len = ep->cols;
		llen -= len + (p == NULL ? 0 : 1);
		
		/* First line is a special case. */
		if (extotalcount != 0) {
			/*
			 * Scroll the screen.  Instead of scrolling the entire
			 * screen, delete the line above the first line output
			 * so that we preserve the maximum amount of the screen.
			 */
			if (extotalcount >= SCREENSIZE(ep)) {
				MOVE(0, 0);
			} else
				MOVE(SCREENSIZE(ep) - extotalcount, 0);
			deleteln();

			/* If just displayed a full screen, wait. */
			if (exlinecount == SCREENSIZE(ep)) {
				MOVE(SCREENSIZE(ep), 0);
				addbytes(CONTMSG, sizeof(CONTMSG) - 1);
				clrtoeol();
				refresh();
				while (special[ch = getkey(0)] != K_CR &&
				    !isspace(ch))
					bell();
				exlinecount = 0;
			}
		}
		++extotalcount;
		++exlinecount;

		MOVE(SCREENSIZE(ep), 0);
		addbytes(line, len);

		if (len < ep->cols)
			clrtoeol();

		line += len + (p == NULL ? 0 : 1);
	}
	return (rlen);
}
