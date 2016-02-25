/*
 * Copyright 2001-2004,2008-2009,2011-2012,2016 BitMover, Inc
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
 * resolve_binary.c - resolver for content conflicts for binaries
 */
#include "resolve.h"

private int
b_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Binary: %s\n\n\
New work has been modified locally and remotely and must be merged.\n\
Because this is a binary or a file marked non-mergable,\n\
you have to choose either the local or remote.\n\n\
GCA:    %s\n\
Local:  %s\n\
Remote: %s\n\
---------------------------------------------------------------------------\n",
	    PATHNAME(rs->s, rs->d),
	    rs->revs->gca, rs->revs->local, rs->revs->remote);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Typical command sequence: 'e' 'C';\n");
	fprintf(stderr, "Difficult merges may be helped by 'p'.\n");
	fprintf(stderr, "\n");
	return (0);
}

private int
b_explain(resolve *rs)
{
	system("bk help merge-binaries");
	return (0);
}

private int
b_commit(resolve *rs)
{
	if (rs->opts->debug) fprintf(stderr, "commit(%s)\n", rs->s->gfile);

	unless (exists(rs->s->gfile) && writable(rs->s->gfile)) {
		fprintf(stderr, "%s has not been merged\n", rs->s->gfile);
		return (0);
	}

	rs->s = sccs_restart(rs->s);
	/*
	 * If in text only mode, then check in the file now.
	 * Otherwise, leave it for citool.
	 */
	if (rs->opts->textOnly) do_delta(rs->opts, rs->s, 0);
	rs->opts->resolved++;
	return (1);
}

private int
b_ascii(resolve *rs)
{
	extern	rfuncs	c_funcs[];

	return (resolve_loop("content conflict", rs, c_funcs));
}

/* An alias for !cp $BK_LOCAL $BK_MERGE */
private int
b_ul(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->local, rs->s->gfile, SYS)) {
		return (b_commit(rs));
	}
	return (0);
}

/* An alias for !cp $BK_REMOTE $BK_MERGE */
private int
b_ur(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->remote, rs->s->gfile, SYS)) {
		return (b_commit(rs));
	}
	return (0);
}

rfuncs	b_funcs[] = {
    { "?", "help", "print this help", b_help },
    { "!", "shell", "escape to an interactive shell", c_shell },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "C", "commit", "commit the merged file", b_commit },
    { "d", "diff", "diff the file, even if it is binary", res_diff },
    { "D", "difftool",
      "run side-by-side graphical difftool on local and remote", res_difftool },
    { "h", "history", "revision history of all changes", res_h },
    { "hl", "hist local", "revision history of the local changes", res_hl },
    { "hr", "hist remote", "revision history of the remote changes", res_hr },
    { "H", "helptool", "show merge help in helptool", c_helptool },
    { "p", "revtool", "graphical picture of the file history", c_revtool },
    { "q", "quit", "immediately exit resolve", c_quit },
    { "t", "text", "go to the text file resolver", b_ascii },
    { "ul", "use local", "use the local version of the file", b_ul },
    { "ur", "use remote", "use the remote version of the file", b_ur },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", b_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the contents.
 * Set up the list of temp files,
 * get the various versions into the temp files,
 * do the resolve,
 * and then clean them up.
 */
int
resolve_binary(resolve *rs)
{
	names	*n = new(names);
	ser_t	d;
	char	*nm = basenm(rs->s->gfile);
	int	ret;
	char	buf[MAXPATH];

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_binary: ");
		resolve_dump(rs);
	}
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
	rs->prompt = rs->s->gfile;
	rs->res_contents = 1;
	if (get_revs(rs, n)) {
		rs->opts->errors = 1;
		freenames(n, 1);
		return (-1);
	}
	unless (LOCKED(rs->s)) {
		if (edit(rs)) return (-1);
	}
	if (sameFiles(n->local, n->remote)) {
		automerge(rs, n, 1);
		ret = 1;
	} else {
		ret = resolve_loop("binary conflict", rs, b_funcs);
	}
	unlink(n->local);
	unlink(n->gca);
	unlink(n->remote);
	freenames(n, 1);
	return (ret);
}
