/*
 * Copyright 2001-2003,2006,2009-2010,2014 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bkd.h"

/*
 * Note: this function is also called from bkd_changes.c
 */
int
cmd_synckeys(int ac, char **av)
{
	char	*p;
	int	c, status;
	size_t	n;
	FILE	*fout;
	sccs	*cset;
	u32	flags = SK_SENDREV;

	while ((c = getopt(ac, av, "S", 0)) != -1) {
	    switch (c) {
		case 'S':	/* look through the root log for a match */
		    flags |= SK_SYNCROOT;
		    break;
		default: bk_badArg(c, av);
	    }
	}

	setmode(0, _O_BINARY); /* needed for gzip mode */
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		return (1);
	}

	if (emptyDir(".")) {
		out("@OK@\n");
		out("@EMPTY TREE@\n");
		return (0);
	}

	unless(isdir("BitKeeper")) { /* not a packageg root */
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}
	unless (cset = sccs_csetInit(0)) {
		/* Historical response from bk _listkey is 3 */
		out("@END@\n");
		out("ERROR-listkey failed (status=3)\n");
		return (1);
	}
	Opts.use_stdio = 1;	/* bkd.c: getav() - set up to drain buf */
	fout = fmem();
	status = listkey(cset, flags, stdin, fout);
	sccs_free(cset);
	if (status > 1) {
		out("@END@\n"); /* just in case list key did not send one */
		error("listkey failed (status=%d)\n", status);
		fclose(fout);
		return (1);
	}

	out("@OK@\n");
	p = fmem_peek(fout, &n);
	unless (writen(1, p, n) == n) {
		fclose(fout);
		return (1);
	}
	fclose(fout);
	return (0);
}
