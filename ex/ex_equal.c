/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static const char sccsid[] = "$Id: ex_equal.c,v 8.7 1994/08/17 09:47:18 bostic Exp $ (Berkeley) $Date: 1994/08/17 09:47:18 $";
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
#include "excmd.h"

/*
 * ex_equal -- :address =
 */
int
ex_equal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	/*
	 * Print out the line number matching the specified address,
	 * or the number of the last line in the file if no address
	 * specified.
	 *
	 * !!!
	 * Historically, ":0=" displayed 0, and ":=" or ":1=" in an
	 * empty file displayed 1.  Until somebody complains loudly,
	 * we're going to do it right.  The tables in excmd.c permit
	 * lno to get away with any address from 0 to the end of the
	 * file, which, in an empty file, is 0.
	 */
	if (F_ISSET(cmdp, E_ADDRDEF)) {
		if (file_lline(sp, ep, &lno))
			return (1);
	} else
		lno = cmdp->addr1.lno;

	(void)ex_printf(EXCOOKIE, "%ld\n", lno);
	return (0);
}
