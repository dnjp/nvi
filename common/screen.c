/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 5.6 1993/04/17 11:51:08 bostic Exp $ (Berkeley) $Date: 1993/04/17 11:51:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"

static int	 cut_copy __P((SCR *, SCR *));
static int	 opt_copy __P((SCR *, SCR *));
static int	 seq_copy __P((SCR *, SCR *));
static int	 tag_copy __P((SCR *, SCR *));

/*
 * scr_init --
 *	Do the default initialization of an SCR structure.
 */
int
scr_init(orig, sp)
	SCR *orig, *sp;
{
	extern CHNAME asciiname[];		/* XXX */
	int nore;

/* INITIALIZED AT SCREEN CREATE. */
	memset(sp, 0, sizeof(SCR));

	if (orig != NULL)
		HDR_APPEND(sp, orig, next, prev, SCR);

	sp->olno = OOBLNO;
	sp->lno = 1;
	sp->cno = sp->ocno = 0;

#ifdef FWOPEN_NOT_AVAILABLE
	sp->trapped_fd = -1;
#endif
	FD_ZERO(&sp->rdfd);

	HDR_INIT(sp->bhdr, next, prev);
	HDR_INIT(sp->txthdr, next, prev);

	sp->lastcmd = &cmds[C_PRINT];

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig != NULL) {
		sp->gp = orig->gp;
		
		/* User can replay the last input, but nothing else. */
		if (orig->rep_len != 0)
			if ((sp->rep = malloc(orig->rep_len)) == NULL)
				goto mem;
			else {
				memmove(sp->rep, orig->rep, orig->rep_len);
				sp->rep_len = orig->rep_len;
			}

		if (orig->VB != NULL && (sp->VB = strdup(orig->VB)) == NULL)
			goto mem;
		
		if (orig->lastbcomm != NULL &&
		    (sp->lastbcomm = strdup(orig->lastbcomm)) == NULL)
			goto mem;

		sp->inc_lastch = orig->inc_lastch;
		sp->inc_lastval = orig->inc_lastval;

		if (cut_copy(orig, sp))
			goto mem;

		if (tag_copy(orig, sp))
			goto mem;
			
		/* Retain all searching/substitution information. */
		if (orig->searchdir == NOTSET)
			sp->searchdir = NOTSET;
		else {
			sp->sre = orig->sre;
			sp->searchdir = FORWARD;
		}
		sp->csearchdir = CNOTSET;
		sp->lastckey = orig->lastckey;

		nore = 0;
		if (orig->matchsize && (sp->match =
		    malloc(orig->matchsize * sizeof(regmatch_t))) == NULL)
			goto mem;
		else {
			if (sp->matchsize = orig->matchsize)
				memmove(sp->match, orig->match,
				    orig->matchsize * sizeof(regmatch_t));
		}
		if (sp->repl_len &&
		    (sp->repl = malloc(orig->repl_len)) == NULL)
			goto mem;
		else {
			if (sp->repl_len = orig->repl_len)
				memmove(sp->repl, orig->repl, orig->repl_len);
		}
		if (sp->newl_len && (sp->newl =
		    malloc(orig->newl_len * sizeof(size_t))) == NULL)
			goto mem;
		else {
			sp->newl_len = orig->newl_len;
			if (sp->newl_cnt = orig->newl_cnt)
				memmove(sp->newl, orig->newl,
				    orig->newl_len * sizeof(size_t));
		}
		if (!nore && F_ISSET(orig, S_RE_SET))
			F_SET(sp, S_RE_SET);

		sp->cname = orig->cname;
		memmove(sp->special, orig->special, sizeof(sp->special));

		if (seq_copy(orig, sp))
			goto mem;

		if (opt_copy(orig, sp)) {
mem:			msgq(orig, M_ERR,
			    "new screen attributes: %s", strerror(errno));
			scr_end(sp);
			return (1);
		}

		sp->flags = orig->flags & S_SCREEN_RETAIN;
	} else {
		if (isatty(STDIN_FILENO))
			F_SET(sp, S_ISFROMTTY);

		sp->inc_lastch = '+';
		sp->inc_lastval = 1;

		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;

		sp->cname = asciiname;			/* XXX */
	}

	return (0);
}

/*
 * scr_end --
 *	Release a screen.
 */
int
scr_end(sp)
	SCR *sp;
{
	/* Free the memory map. */
	if (sp->h_smap != NULL)
		FREE(sp->h_smap, sp->w_rows * sizeof(SMAP));

	/* Free the argument list. */
	{ int cnt;
		for (cnt = 0; cnt < sp->argscnt; ++cnt)
			if (F_ISSET(&sp->args[cnt], A_ALLOCATED))
				FREE(sp->args[cnt].bp, sp->args[cnt].len);
		FREE(sp->args, sp->argscnt * sizeof(ARGS *));
		FREE(sp->argv, sp->argscnt * sizeof(char *));
	}

	/* Free globbing information. */
	if (sp->g.gl_pathc)
		globfree(&sp->g);

	/* Free line input buffer. */
	if (sp->ibp != NULL)
		FREE(sp->ibp, sp->ibp_len);

	/* Free text input, command chains. */
	text_free(&sp->txthdr);
	text_free(&sp->bhdr);

	/* Free vi text input memory. */
	if (sp->rep != NULL)
		FREE(sp->rep, sp->rep_len);

	/* Free visual bell termcap string. */
	if (sp->VB != NULL)
		FREE(sp->VB, strlen(sp->VB) + 1);

	/* Free last bang command. */
	if (sp->lastbcomm != NULL)
		FREE(sp->lastbcomm, strlen(sp->lastbcomm) + 1);

	/* Free cut buffers. */
	{ CB *cb; int cnt;
		for (cb = sp->cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt)
			if (cb->txthdr.next != NULL)
				text_free(&cb->txthdr);
	}

	/* Free up tag information. */
	{ int cnt;
		if (F_ISSET(&sp->opts[O_TAGS], OPT_ALLOCATED) &&
		    sp->tfhead != NULL) {
			for (cnt = 0; sp->tfhead[cnt] != NULL; ++cnt)
				FREE(sp->tfhead[cnt]->fname,
				    strlen(sp->tfhead[cnt]->fname) + 1);
			free(sp->tfhead);
		}
		if (sp->tlast != NULL)
			FREE(sp->tlast, strlen(sp->tlast) + 1);
	}

	/* Free up search information. */
	if (sp->match != NULL)
		FREE(sp->match, sizeof(regmatch_t));
	if (sp->repl != NULL)
		FREE(sp->repl, sp->repl_len);
	if (sp->newl != NULL)
		FREE(sp->newl, sp->newl_len);

	/* Free up linked lists of sequences. */
	{ SEQ *qp, *next;
		for (qp = sp->seqhdr.next;
		    qp != (SEQ *)&sp->seqhdr; qp = next) {
			next = qp->next;
			if (qp->name != NULL)
				FREE(qp->name, strlen(qp->name) + 1);
			FREE(qp->output, strlen(qp->output) + 1);
			FREE(qp->input, strlen(qp->input) + 1);
			FREE(qp, sizeof(SEQ));
		}
	}

	/* Free up executed buffer. */
	if (sp->atkey_buf)
		FREE(sp->atkey_buf, sp->atkey_len);

	/*
	 * Free the message chain last, so previous failures have a place
	 * to put messages.  Copy messages to (in order) a related screen,
	 * any screen, the global area. 
	 */
	{ SCR *c_sp; MSG *c_mp, *mp, *next;
		if (sp->parent != NULL) {
			c_sp = sp->parent;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if (sp->child != NULL) {
			c_sp = sp->child;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if (sp->next != (SCR *)&sp->gp->scrhdr) {
			c_sp = sp->next;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else {
			c_sp = NULL;
			c_mp = sp->gp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(sp->gp, S_BELLSCHED);
		}

		for (mp = sp->msgp;
		    mp != NULL && !F_ISSET(mp, M_EMPTY); mp = mp->next)
			msga(sp->gp, c_sp,
			    mp->flags & M_INV_VIDEO, mp->mbuf, mp->len);

		for (mp = sp->msgp; mp != NULL; mp = next) {
			next = mp->next;
			if (mp->mbuf != NULL)
				FREE(mp->mbuf, mp->blen);
			FREE(mp, sizeof(MSG));
		}
	}

	/* Remove the screen from the global chain of screens. */
	HDR_DELETE(sp, next, prev, SCR);

	/* Remove the screen from the chain of related screens. */
	if (sp->parent != NULL) {
		sp->parent->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = sp->parent;
	} else if (sp->child != NULL)
		sp->child->parent = NULL;

	/* Free the screen itself. */
	FREE(sp, sizeof(SCR));

	return (0);
}

/*
 * cut_copy --
 *	Copy a screen's cut buffers.
 */
static int
cut_copy(a, b)
	SCR *a, *b;
{
	CB *acb, *bcb;
	TEXT *atp, *tp;
	int cnt;

	for (acb = a->cuts, bcb = b->cuts, cnt = 0;
	    cnt < UCHAR_MAX; ++acb, ++bcb, ++cnt) {
		if (acb->txthdr.next == NULL ||
		    acb->txthdr.next == &acb->txthdr)
			continue;
		HDR_INIT(bcb->txthdr, next, prev);
		for (atp = acb->txthdr.next;
		    atp != (TEXT *)&acb->txthdr; atp = atp->next) {
			if ((tp = malloc(sizeof(TEXT))) == NULL)
				return (1);
			if ((tp->lb = malloc(atp->len)) == NULL) {
				FREE(tp, sizeof(TEXT));
				return (1);
			}
			memmove(atp->lb, tp->lb, tp->len = atp->len);
			HDR_INSERT(tp, &bcb->txthdr, next, prev, TEXT);
		}
		bcb->len = acb->len;
		bcb->flags = acb->flags;
	}
	return (0);
}

/*
 * opt_copy --
 *	Copy a screen's OPTION array.
 */
static int
opt_copy(a, b)
	SCR *a, *b;
{
	OPTION *op;
	int cnt;

	/* Copy most everything without change. */
	memmove(b->opts, a->opts, sizeof(a->opts));

	/*
	 * Allocate copies of the strings -- keep trying to reallocate
	 * after ENOMEM failure, otherwise end up with more than one
	 * screen referencing the original memory.
	 */
	for (op = b->opts, cnt = 0; cnt < O_OPTIONCOUNT; ++cnt, ++op)
		if (F_ISSET(&b->opts[cnt], OPT_ALLOCATED) &&
		    (O_STR(b, cnt) = strdup(O_STR(b, cnt))) == NULL) {
			msgq(a, M_ERR,
			    "Error: option copy: %s", strerror(errno));
			return (1);
		}
	return (0);
}

/*
 * seq_copy --
 *	Copy a screen's SEQ structures.
 */
static int
seq_copy(a, b)
	SCR *a, *b;
{
	SEQ *ap;

	/* Initialize linked list. */
	HDR_INIT(b->seqhdr, next, prev);

	for (ap = a->seqhdr.next; ap != (SEQ *)&a->seqhdr; ap = ap->next)
		if (seq_set(b,
		    ap->name, ap->input, ap->output, ap->stype,
		    F_ISSET(ap, S_USERDEF)))
			return (1);
	return (0);
}

/*
 * tag_copy --
 *	Copy a screen's tag structures.
 */
static int
tag_copy(a, b)
	SCR *a, *b;
{
	TAGF **atfp, **btfp;
	TAG *atp, *btp, *tp;
	int cnt;

	/* Copy the tag stack. */
	for (atp = a->thead; atp != NULL; atp = atp->prev) {
		if ((tp = malloc(sizeof(TAG))) == NULL)
			goto nomem;
		if ((tp->tag = strdup(atp->tag)) == NULL) {
			FREE(tp, sizeof(TAG));
			goto nomem;
		}
		if ((tp->fname = strdup(atp->fname)) == NULL) {
			FREE(tp, sizeof(TAG));
			FREE(tp->tag, strlen(tp->tag) + 1);
			goto nomem;
		}
		if ((tp->line = strdup(atp->line)) == NULL) {
			FREE(tp, sizeof(TAG));
			FREE(tp->tag, strlen(tp->tag) + 1);
			FREE(tp->fname, strlen(tp->fname) + 1);
			goto nomem;
		}
		tp->prev = NULL;
		if (b->thead == NULL)
			btp = b->thead = tp;
		else {
			btp->prev = tp;
			btp = tp;
		}
	}

	/* Copy the list of tag files. */
	for (atfp = a->tfhead, cnt = 1; *atfp != NULL; ++atfp, ++cnt);

	if (cnt > 1) {
		if ((b->tfhead = malloc(cnt * sizeof(TAGF **))) == NULL)
			goto nomem;
		for (atfp = a->tfhead,
		    btfp = b->tfhead; *atfp != NULL; ++atfp, ++btfp) {
			if ((*btfp = malloc(sizeof(TAGF))) == NULL)
				goto nomem;
			if (((*btfp)->fname = strdup((*atfp)->fname)) == NULL) {
				FREE(*btfp, sizeof(TAGF));
				*btfp = NULL;
				goto nomem;
			}
			(*btfp)->flags = 0;
		}
		*btfp = NULL;
	}
		
	/* Copy the last tag. */
	if (a->tlast != NULL && (b->tlast = strdup(a->tlast)) == NULL)
		goto nomem;
	return (0);

nomem:	msgq(a, M_ERR, "Error: tag copy: %s", strerror(errno));
	return (1);
}
