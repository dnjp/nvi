/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 8.8 1993/09/08 15:16:50 bostic Exp $ (Berkeley) $Date: 1993/09/08 15:16:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

enum which {WQ, WRITE, XIT};

static int exwr __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_wq --	:wq[!] [>>] [file]
 *	Write to a file.
 */
int
ex_wq(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	if (exwr(sp, ep, cmdp, WQ))
		return (1);

	force = F_ISSET(cmdp, E_FORCE);
	if (!force && ep->refcnt <= 1 && file_next(sp, 0) != NULL) {
		msgq(sp, M_ERR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * ex_write --	:write[!] [>>] [file]
 *		:write [!] [cmd]
 *	Write to a file.
 */
int
ex_write(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (exwr(sp, ep, cmdp, WRITE));
}


/*
 * ex_xit -- :x[it]! [file]
 *
 *	Write out any modifications and quit.
 */
int
ex_xit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	if (F_ISSET((ep), F_MODIFIED) && exwr(sp, ep, cmdp, XIT))
		return (1);

	force = F_ISSET(cmdp, E_FORCE);
	if (!force && ep->refcnt <= 1 && file_next(sp, 0) != NULL) {
		msgq(sp, M_ERR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * exwr --
 *	The guts of the ex write commands.
 */
static int
exwr(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	register char *p;
	MARK rm;
	int flags, force;
	char *fname;

	/* All write commands can have an associated '!'. */
	LF_INIT(FS_POSSIBLE);
	if (F_ISSET(cmdp, E_FORCE))
		LF_SET(FS_FORCE);

	/* If no more arguments, just write the file back. */
	for (p = cmdp->argv[0]; *p && isblank(*p); ++p);
	if (*p == '\0') {
		if (F_ISSET(cmdp, E_ADDR2_ALL))
			LF_SET(FS_ALL);
		return (file_write(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, NULL, flags));
	}

	/* If "write !" it's a pipe to a utility. */
	if (cmd == WRITE && *p == '!') {
		for (; *p && isblank(*p); ++p);
		if (*p == '\0') {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, ++p, FILTER_WRITE))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* If "write >>" it's an append to a file. */
	if (cmd != XIT && p[0] == '>' && p[1] == '>') {
		LF_SET(FS_APPEND);

		/* Skip ">>" and whitespace. */
		for (p += 2; *p && isblank(*p); ++p);
	}

	/* Build an argv (so we get file expansion). */
	if (file_argv(sp, ep, p, &cmdp->argc, &cmdp->argv))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = sp->frp->fname;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		set_alt_fname(sp, fname);
		break;
	default:
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	if (F_ISSET(cmdp, E_ADDR2_ALL))
		LF_SET(FS_ALL);
	return (file_write(sp, ep, &cmdp->addr1, &cmdp->addr2, fname, flags));
}

/*
 * ex_writefp --
 *	Write a range of lines to a FILE *.
 */
int
ex_writefp(sp, ep, fname, fp, fm, tm, success_msg)
	SCR *sp;
	EXF *ep;
	char *fname;
	FILE *fp;
	MARK *fm, *tm;
	int success_msg;
{
	register u_long ccnt, fline, tline;
	recno_t nlines;
	size_t len;
	char *p;

	fline = fm->lno;
	tline = tm->lno;

	ccnt = 0;
	/*
	 * The vi filter code has multiple processes running simultaneously,
	 * and one of them calls ex_writefp().  The "unsafe" function calls
	 * in this code are to file_gline() and msgq().  File_gline() is safe,
	 * see the comment in filter.c:filtercmd() for details.  We don't call
	 * msgq if the multiple process bit in the EXF is set.
	 *
	 * !!!
	 * Historic vi permitted files of 0 length to be written.  However,
	 * since the way vi got around dealing with "empty" files was to
	 * always have a line in the file no matter what, it wrote them as
	 * files of a single, empty line.  `Alex, I'll take vi trivia for
	 * $1000.'
	 */
	if (tline != 0)
		for (; fline <= tline; ++fline) {
			if ((p = file_gline(sp, ep, fline, &len)) == NULL)
				break;
			if (fwrite(p, 1, len, fp) != len) {
				msgq(sp, M_ERR,
				    "%s: %s", fname, strerror(errno));
				(void)fclose(fp);
				return (1);
			}
			ccnt += len;
			if (putc('\n', fp) != '\n')
				break;
			++ccnt;
		}
	if (fclose(fp)) {
		if (!F_ISSET(ep, F_MULTILOCK))
			msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		return (1);
	}
	if (success_msg) {
		nlines = tm->lno == 0 ? 0 : tm->lno - fm->lno + 1;
		if (!F_ISSET(ep, F_MULTILOCK))
			msgq(sp, M_INFO, "%s: %lu line%s, %lu characters.",
			    fname, nlines, nlines == 1 ? "" : "s", ccnt);
	}
	return (0);
}
