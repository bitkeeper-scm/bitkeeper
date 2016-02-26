/*
 * Copyright 2000-2004,2009,2011-2012 BitMover, Inc
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

/*
 * resolve_renames.c - resolver for rename conflicts
 */
#include "resolve.h"

private int
r_help(resolve *rs)
{
	int	i;
	char	*l, *r;

	if (streq(rs->gnames->gca, rs->gnames->local) &&
	    !streq(rs->gnames->gca, rs->gnames->remote)) {
		l = "did not move.";
		r = "moved.";
	} else if (!streq(rs->gnames->gca, rs->gnames->local) &&
	    streq(rs->gnames->gca, rs->gnames->remote)) {
		l = "moved.";
		r = "did not move.";
	} else {
		l = r = 0;
		if (i = slotTaken(rs, rs->snames->remote, 0)) {
			switch (i) {
			    case SFILE_CONFLICT:
				r = "wants to move, has sfile conflict.";
				break;
			    case GONE_SFILE_CONFLICT:
				r = "wants to move, has conflict "
				    "with sfile that is marked gone.";
				break;
			    case GFILE_CONFLICT:
				r = "wants to move, has gfile conflict.";
				break;
			    case RESYNC_CONFLICT:
				r = "wants to move, has RESYNC conflict.";
				break;
			}
		}
		if (i = slotTaken(rs, rs->snames->local, 0)) {
			switch (i) {
			    /*
			     * The two SFILE below can't happen since
			     * the local name slot is the same sfile as is
			     * conflicting, therefore local _should_ always
			     * work (ie, bet you can't find a way .. :)
			     */
			    case SFILE_CONFLICT:
				l = "wants to move, has sfile conflict.";
				break;
			    case GONE_SFILE_CONFLICT:
				l = "wants to move, has conflict "
				    "with sfile that is marked gone.";
				break;
			    case GFILE_CONFLICT:
				l = "wants to move, has gfile conflict.";
				break;
			    case RESYNC_CONFLICT:
				l = "wants to move, has RESYNC conflict.";
				break;
			}
		}
		if (!r) r = "pathname is available locally";
		if (!l) l = "pathname is available locally";
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a name conflict of some type.\n\
GCA:    %s (greatest common ancestor)\n\
Local:  %s %s\n\
Remote: %s %s\n\
---------------------------------------------------------------------------\n",
	    rs->gnames->gca,
	    rs->gnames->local, l,
	    rs->gnames->remote, r);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

private int
r_explain(resolve *rs)
{
	fprintf(stderr,
"\nThe file has been moved in both the local and remote repository.\n\
You have to pick one of those two names, or pick a new name.\n\
The original name was             \"%s\"\n\
The local repository moved it to  \"%s\"\n\
the remote repository moved it to \"%s\"\n\n",
    rs->gnames->gca, rs->gnames->local, rs->gnames->remote);
	return (0);
}

/* Run helptool in background */
private int
r_helptool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "helptool";
	av[2] = 0;
	spawnvp(_P_DETACH, "bk", av);
	return (0);
}

private int
r_l(resolve *rs)
{
	int	i;
	char	*l;

	if (i = slotTaken(rs, rs->snames->local, 0)) {
		fprintf(stderr, "Local name is in use by ");
		switch (i) {
		    case SFILE_CONFLICT:
			l = "a local sfile.";
			break;
		    case GONE_SFILE_CONFLICT:
			l = "a local sfile that is marked gone.";
			break;
		    case GFILE_CONFLICT:
			l = "a local gfile.";
			break;
		    case RESYNC_CONFLICT:
			l = "a local sfile in the RESYNC directory.";
			break;
		    default:
		    	l = "huh?";
			break;
		}
		fprintf(stderr, "%s\n", l);
		return (0);
	}
	if (move_remote(rs, rs->snames->local)) {
		perror("move_remote");
		exit(1);
	}
	return (1);
}

private int
r_r(resolve *rs)
{
	int	i;
	char	*l;

	if (i = slotTaken(rs, rs->snames->remote, 0)) {
		fprintf(stderr, "Remote name is in use by ");
		switch (i) {
		    case SFILE_CONFLICT:
			l = "a local sfile.";
			break;
		    case GONE_SFILE_CONFLICT:
			l = "a local sfile that is marked gone.";
			break;
		    case GFILE_CONFLICT:
			l = "a local gfile.";
			break;
		    case RESYNC_CONFLICT:
			l = "a local sfile in the RESYNC directory.";
			break;
		    default:
		    	l = "huh?";
			break;
		}
		fprintf(stderr, "%s\n", l);
		return (0);
	}
	if (move_remote(rs, rs->snames->remote)) {
		perror("move_remote");
		exit(1);
	}
	return (1);
}

int
res_revtool(resolve *rs)
{
	char	*av[10];

	if (rs->revs) return (c_revtool(rs));
	av[0] = "bk";
	av[1] = "revtool";
	av[2] = rs->s->gfile;
	av[3] = 0;
	spawnvp(_P_DETACH, "bk", av);
	return (0);
}

rfuncs	r_funcs[] = {
    { "?", "help", "print this help", r_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
	"run graphical difftool on local and remote", res_difftool },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "H", "helptool", "run helptool", r_helptool },
    { "l", "use local", "use the local file name", r_l },
    { "m", "move", "move the file to someplace else", res_mr },
    { "p", "revtool", "graphical picture of the file history", res_revtool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "use remote", "use the remote file name", r_r },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", r_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the renames.
 * get the various versions into the temp files,
 * do the resolve,
 * and then clean them up.
 */
int
resolve_renames(resolve *rs)
{
	names	*n = 0;
	ser_t	d;
	char	*nm = basenm(rs->s->gfile);
	int	ret;
	char	buf[MAXPATH];

	if (rs->opts->debug) {
		fprintf(stderr, "resolve_renames: ");
		resolve_dump(rs);
	}
	if (rs->revs) {
		n = new(names);
		d = sccs_findrev(rs->s, rs->revs->local);
		assert(d);
		sprintf(buf, "BitKeeper/tmp/%s_%s@%s",
		    nm, USER(rs->s, d), REV(rs->s, d));
		n->local = strdup(buf);
		d = sccs_findrev(rs->s, rs->revs->gca);
		assert(d);
		sprintf(buf, "BitKeeper/tmp/%s_%s@%s",
		    nm, USER(rs->s, d), REV(rs->s, d));
		n->gca = strdup(buf);
		d = sccs_findrev(rs->s, rs->revs->remote);
		assert(d);
		sprintf(buf, "BitKeeper/tmp/%s_%s@%s",
		    nm, USER(rs->s, d), REV(rs->s, d));
		n->remote = strdup(buf);
		rs->tnames = n;
		if (get_revs(rs, n)) {
			rs->opts->errors = 1;
			freenames(n, 1);
			return (-1);
		}
	}
	rs->prompt = rs->gnames->local;
	ret = resolve_loop("rename conflict", rs, r_funcs);
	if (rs->revs) freenames(n, 1);
	return (ret);
}
