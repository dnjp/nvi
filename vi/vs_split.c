/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 8.19 1993/11/28 18:32:06 bostic Exp $ (Berkeley) $Date: 1993/11/28 18:32:06 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

/*
 * svi_split --
 *	Split the screen.
 */
int
svi_split(sp, argv)
	SCR *sp;
	char *argv[];
{
	SCR *tsp;
	SMAP *smp;
	size_t cnt, half;
	int issmallscreen, nochange, splitup;

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "Screen must be larger than %d to split",
		     MINIMUM_SCREEN_ROWS);
		return (1);
	}

	/* Get a new screen. */
	if (screen_init(sp, &tsp, 0))
		goto mem1;
	if ((_HMAP(tsp) = malloc(SIZE_HMAP(sp) * sizeof(SMAP))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		goto mem2;
	}

/* INITIALIZED AT SCREEN CREATE. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 3b.
	 * Set a flag so we know to fix the screen up later.
	 */
	issmallscreen = ISSMALLSCREEN(sp);

	/* Flag if we're changing screens. */
	nochange = argv == NULL;

	/*
	 * Split the screen, and link the screens together.  If the cursor
	 * is in the top half of the current screen, the new screen goes
	 * under the current screen.  Else, it goes above the current screen.
	 *
	 * The columns in the screen don't change.
	 */
	tsp->cols = sp->cols;
	
	cnt = svi_sm_cursor(sp, sp->ep, &smp) ? 0 : smp - HMAP;
	if (cnt <= half) {			/* Parent is top half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->woff = sp->woff + half;
		tsp->t_maxrows = tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->t_maxrows = sp->rows - 1;

		/* Link in after the parent. */
		CIRCLEQ_INSERT_AFTER(&sp->gp->dq, sp, tsp, q);
		splitup = 0;
	} else {				/* Parent is bottom half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->woff = sp->woff;
		tsp->t_maxrows = tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->woff += tsp->rows;
		sp->t_maxrows = sp->rows - 1;

		/* Shift the parent's map down. */
		memmove(_HMAP(sp),
		    _HMAP(sp) + tsp->rows, sp->t_maxrows * sizeof(SMAP));

		/* Link in before the parent. */
		CIRCLEQ_INSERT_BEFORE(&sp->gp->dq, sp, tsp, q);
		splitup = 1;
	}

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 3b.
	 *
	 * The child may have different screen options sizes than the
	 * parent, so use them.  Make sure that the text counts aren't
	 * larger than the new screen sizes.
	 */
	if (issmallscreen) {
		/* Fix the text line count for the parent. */
		if (splitup)
			sp->t_rows -= tsp->rows;

		/* Fix the parent screen. */
		if (sp->t_rows > sp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > sp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;

		/* Fix the child screen. */
		tsp->t_minrows = tsp->t_rows = O_VAL(sp, O_WINDOW);
		if (tsp->t_rows > tsp->t_maxrows)
			tsp->t_rows = tsp->t_maxrows;
		if (tsp->t_minrows > tsp->t_maxrows)
			tsp->t_minrows = tsp->t_maxrows;

		/*
		 * If we split up, i.e. the child is on top, lines that
		 * were painted in the parent may not be painted in the
		 * child.  Clear any lines not being used in the child
		 * screen.
		 *
		 */
		if (splitup)
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				MOVE(tsp, cnt, 0)
				clrtoeol();
			}
	} else {
		sp->t_minrows = sp->t_rows = sp->rows - 1;

		/*
		 * The new screen may be a small screen, even though the
		 * parent was not.  Don't complain if O_WINDOW is too large,
		 * we're splitting the screen so the screen is much smaller
		 * than normal.  Clear any lines not being used in the child
		 * screen.
		 */
		tsp->t_minrows = tsp->t_rows = O_VAL(sp, O_WINDOW);
		if (tsp->t_rows > tsp->rows - 1)
			tsp->t_minrows = tsp->t_rows = tsp->rows - 1;
		else
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				MOVE(tsp, cnt, 0)
				clrtoeol();
			}
	}

	/* Adjust the ends of both maps. */
	_TMAP(sp) = _HMAP(sp) + (sp->t_rows - 1);
	_TMAP(tsp) = _HMAP(tsp) + (tsp->t_rows - 1);

	/*
	 * In any case, if the size of the scrolling region hasn't been
	 * modified by the user, reset it so it's reasonable for the split
	 * screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET)) {
		O_VAL(sp, O_SCROLL) = sp->t_maxrows / 2;
		O_VAL(tsp, O_SCROLL) = sp->t_maxrows / 2;
	}

	/*
	 * If files specified, build the file list, else, link to the
	 * current file.
	 */
	if (argv != NULL && *argv != NULL) {
		for (; *argv != NULL; ++argv)
			if (file_add(tsp, NULL, *argv, 0) == NULL)
				goto mem3;
	} else {
		if (file_add(tsp, NULL, FILENAME(sp->frp), 0) == NULL)
			goto mem3;
	}

	if ((tsp->frp = file_first(tsp, 0)) == NULL) {
		msgq(sp, M_ERR, "No files in the file list.");
		goto mem3;
	}

	/*
	 * Copy the file state flags, start the file.  Fill the child's
	 * screen map.  If the file is unchanged, keep the screen and
	 * cursor the same.
	 */
	if (nochange) {
		tsp->ep = sp->ep;
		++sp->ep->refcnt;

		tsp->frp->flags = sp->frp->flags;
		tsp->frp->lno = sp->lno;
		tsp->frp->cno = sp->cno;
		F_SET(tsp->frp, FR_CURSORSET);

		/* Copy the parent's map into the child's map. */
		memmove(_HMAP(tsp), _HMAP(sp), tsp->t_rows * sizeof(SMAP));
	} else {
		if (file_init(tsp, tsp->frp, NULL, 0))
			goto mem3;
		(void)svi_sm_fill(tsp, tsp->ep, 1, P_TOP);
	}

	/* Clear the current information lines in both screens. */
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	MOVE(tsp, INFOLINE(tsp), 0);
	clrtoeol();

	/* Redraw the status line for the parent screen. */
	(void)status(sp, sp->ep, sp->lno, 0);

	/* Save the parent screen's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Completely redraw the child screen. */
	F_SET(tsp, S_REDRAW);

	/* Switch screens. */
	sp->nextdisp = tsp;
	F_SET(sp, S_SSWITCH);
	return (0);

mem3:	FREE(_HMAP(tsp), SIZE_HMAP(sp) * sizeof(SMAP));
	FREE(SVP(sp), sizeof(SVI_PRIVATE));
mem2:	(void)screen_end(tsp);
mem1:	FREE(tsp, sizeof(SCR));
	return (1);
}

/*
 * svi_bg --
 *	Background the screen, and switch to the next one.
 */
int
svi_bg(csp)
	SCR *csp;
{
	SCR *sp;

	/* Try and join with another screen. */
	if ((svi_join(csp, &sp)))
		return (1);
	if (sp == NULL) {
		msgq(csp, M_ERR,
		    "You may not background your only displayed screen.");
		return (1);
	}

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, S_SSWITCH);

	return (0);
}

/*
 * svi_join --
 *	Join the screen into a related screen, if one exists,
 *	and return that screen.
 */
int
svi_join(csp, nsp)
	SCR *csp, **nsp;
{
	SCR *sp;
	size_t cnt;

	/*
	 * If a split screen, add space to parent/child.  Make no effort
	 * to clean up the screen's values.  If it's not exiting, we'll
	 * get it when the user asks to show it again.
	 */
	if ((sp = csp->q.cqe_prev) == (void *)&csp->gp->dq) {
		if ((sp = csp->q.cqe_next) == (void *)&csp->gp->dq) {
			*nsp = NULL;
			return (0);
		}
		sp->woff = csp->woff;
	}
	sp->rows += csp->rows;
	if (ISSMALLSCREEN(sp)) {
		sp->t_maxrows += csp->rows;
		for (cnt = sp->t_rows; ++cnt <= sp->t_maxrows;) {
			MOVE(sp, cnt, 0);
			clrtoeol();
		}
		TMAP = HMAP + (sp->t_rows - 1);
	} else {
		sp->t_maxrows += csp->rows;
		sp->t_rows = sp->t_minrows = sp->t_maxrows;
		TMAP = HMAP + (sp->t_rows - 1);
		if (svi_sm_fill(sp, sp->ep, sp->lno, P_FILL))
			return (1);
		F_SET(sp, S_REDRAW);
	}

	/*
	 * If the size of the scrolling region hasn't been modified by
	 * the user, reset it so it's reasonable for the new screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET))
		O_VAL(sp, O_SCROLL) = sp->t_maxrows / 2;

	/* Screen is no longer displayed. */
	CIRCLEQ_REMOVE(&csp->gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&csp->gp->hq, csp, q);

	*nsp = sp;
	return (0);
}

/*
 * svi_fg --
 *	Background the current screen, and foreground a new one.
 */
int
svi_fg(csp, name)
	SCR *csp;
	char *name;
{
	SCR *sp;

	if (svi_swap(csp, &sp, name))
		return (1);
	if (sp == NULL) {
		if (name == NULL)
			msgq(csp, M_ERR, "There are no background screens.");
		else
			msgq(csp, M_ERR,
		    "There's no background screen editing a file named %s.",
			    name);
		return (1);
	}
	return (0);
}

/*
 * svi_swap --
 *	Swap the current screen with a hidden one.
 */
int
svi_swap(csp, nsp, name)
	SCR *csp, **nsp;
	char *name;
{
	SCR *sp;
	int issmallscreen;

	/* Find the screen, or, if name is NULL, the first screen. */
	for (sp = csp->gp->hq.cqh_first;
	    sp != (void *)&csp->gp->hq; sp = sp->q.cqe_next)
		if (name == NULL || !strcmp(FILENAME(sp->frp), name))
			break;
	if (sp == (void *)&csp->gp->hq) {
		*nsp = NULL;
		return (0);
	}
	*nsp = sp;
		
	/* Save the old screen's cursor information. */
	csp->frp->lno = csp->lno;
	csp->frp->cno = csp->cno;
	F_SET(csp->frp, FR_CURSORSET);

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, S_SSWITCH);

	/* Initialize terminal information. */
	SVP(sp)->srows = SVP(csp)->srows;

	issmallscreen = ISSMALLSCREEN(sp);

	/* Initialize screen information. */
	sp->rows = csp->rows;
	sp->cols = csp->cols;
	sp->woff = csp->woff;

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 3b.
	 *
	 * The new screens may have different screen options sizes than the
	 * old one, so use them.  Make sure that text counts aren't larger
	 * than the new screen sizes.
	 */
	if (issmallscreen) {
		sp->t_minrows = sp->t_rows = O_VAL(sp, O_WINDOW);
		if (sp->t_rows > csp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > csp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;
	} else
		sp->t_rows = sp->t_maxrows = sp->rows - 1;

	/*
	 * If the size of the scrolling region hasn't been modified by
	 * the user, reset it so it's reasonable for the new screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET))
		O_VAL(sp, O_SCROLL) = sp->t_maxrows / 2;

	/*
	 * Don't change the screen's cursor information other than to
	 * note that the cursor is wrong.
	 */
	F_SET(SVP(sp), SVI_CUR_INVALID);

	/*
	 * The HMAP may be NULL, if the screen got resized and
	 * a bunch of screens had to be hidden.
	 */
	if (HMAP == NULL &&
	    (HMAP = malloc(SIZE_HMAP(sp) * sizeof(SMAP))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	TMAP = HMAP + (sp->t_rows - 1);

	/* Fill the map. */
	if (svi_sm_fill(sp, sp->ep, sp->lno, P_FILL))
		return (1);

	/* The new screen replaces the old screen in the parent/child list. */
	CIRCLEQ_REMOVE(&sp->gp->hq, sp, q);
	CIRCLEQ_INSERT_AFTER(&csp->gp->dq, csp, sp, q);
	CIRCLEQ_REMOVE(&csp->gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&csp->gp->hq, csp, q);

	F_SET(sp, S_REDRAW);
	return (0);
}

/*
 * svi_resize --
 *	Change the size of the current screen.
 */
int
svi_resize(sp, count)
	SCR *sp;
	long count;
{
	SCR *g, *s;

	/*
	 * Figure out which screens will grow, which will shrink, and
	 * make sure it's possible.
	 */
	if (count == 0)
		return (0);
	if (count < 0) {
		count = -count;
		s = sp;
		if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
			goto toosmall;
		if ((g = sp->q.cqe_prev) == (void *)&sp->gp->dq) {
			if ((g = sp->q.cqe_next) == (void *)&sp->gp->dq)
				goto toobig;
			g->woff -= count;
		} else
			s->woff += count;
	} else {
		g = sp;
		if ((s = sp->q.cqe_next) != (void *)&sp->gp->dq)
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
				s = NULL;
			else
				s->woff += count;
		if (s == NULL) {
			if ((s = sp->q.cqe_prev) == (void *)&sp->gp->dq) {
toobig:				msgq(sp, M_BERR, "The screen cannot %s.",
				    count < 0 ? "shrink" : "grow");
				return (1);
			}
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count) {
toosmall:			msgq(sp, M_BERR,
				    "The screen can only shrink to %d rows.",
				    MINIMUM_SCREEN_ROWS);
				return (1);
			}
			g->woff -= count;
		}
	}

	/* Update the screens. */
	g->rows += count;
	g->t_rows += count;
	if (g->t_minrows == g->t_maxrows)
		g->t_minrows += count;
	g->t_maxrows += count;
	_TMAP(g) = _HMAP(g) + (g->t_rows - 1);
	(void)status(g, g->ep, g->lno, 0);
	F_SET(g, S_REDRAW);

	s->rows -= count;
	s->t_rows -= count;
	s->t_maxrows -= count;
	if (s->t_minrows > s->t_maxrows)
		s->t_minrows = s->t_maxrows;
	_TMAP(s) = _HMAP(s) + (s->t_rows - 1);
	(void)status(s, s->ep, s->lno, 0);
	F_SET(s, S_REDRAW);

	return (0);
}
