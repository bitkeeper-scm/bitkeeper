/*
 * Copyright 2000-2014 BitMover, Inc
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
 * resolve_contents.c - resolver for content conflicts
 */
#include "resolve.h"

private int
c_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
File:   %s\n\n\
New work has been added locally and remotely and must be merged.\n\n\
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
c_explain(resolve *rs)
{
	system("bk help merging");
	return (0);
}

int
res_clear(resolve *rs)
{
	tty_clear();
	return (0);
}

private int
c_dgl(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->gca, n->local, 1);
	return (0);
}

private int
c_dgr(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->gca, n->remote, 1);
	return (0);
}

private int
c_dlm(resolve *rs)
{
	names	*n = rs->tnames;

	unless(exists(rs->s->gfile)) c_merge(rs);

	do_diff(rs, n->local, rs->s->gfile, 1);
	return (0);
}

private int
c_drm(resolve *rs)
{
	names	*n = rs->tnames;

	unless(exists(rs->s->gfile)) c_merge(rs);

	do_diff(rs, n->remote, rs->s->gfile, 1);
	return (0);
}

private int
c_em(resolve *rs)
{
	char	*cmd;

	unless (exists(rs->s->gfile)) c_merge(rs);
	cmd = aprintf("%s '%s'", rs->editor, rs->s->gfile);
	system(cmd);
	free(cmd);
	return (0);
}

/* Run helptool in background */
int
c_helptool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "helptool";
	av[2] = "merge";
	av[3] = 0;
	spawnvp(_P_DETACH, "bk", av);
	return (0);
}

/*
 * Unedit the file if we are quiting.
 */
int
c_quit(resolve *rs)
{
	if (LOCKED(rs->s)) {
		/*
		 * Mark to be able to find it next time, since
		 * conflict() will skip it because of a pfile.
		 */
		unless (exists(rs->s->gfile)) {
			rs->s = sccs_restart(rs->s);
			sccs_unedit(rs->s, SILENT);
		} else {
			xfile_store(rs->s->gfile, 'a', "");
		}
	}
	assert(exists(RESYNC2ROOT "/" ROOT2RESYNC));
	chdir(RESYNC2ROOT);
	proj_restoreAllCO(0, 0, 0, 0);
	sccs_unlockfile(RESOLVE_LOCK);
	exit(1);
}

int
c_merge(resolve *rs)
{
	names	*n = rs->tnames;
	int	ret;

	if (rs->opts->mergeprog) {
		ret = sys("bk", rs->opts->mergeprog,
		    n->local, n->gca, n->remote, rs->s->gfile, SYS);
	} else {
		char	*l = aprintf("-l%s", rs->revs->local);
		char	*r = aprintf("-r%s", rs->revs->remote);

		ret = sysio(0,
		    rs->s->gfile, 0, "bk", "smerge", l, r, rs->s->gfile, SYS);
		free(l);
		free(r);
	}
	sccs_restart(rs->s);
	unless (WIFEXITED(ret)) {
	    	fprintf(stderr, "Cannot execute '%s'\n",
		    rs->opts->mergeprog ? rs->opts->mergeprog : "bk smerge");
		rs->opts->errors = 1;
		return (0);
	}
	if (WEXITSTATUS(ret) == 0) {
		unless (rs->opts->quiet) {
			fprintf(stderr, "merge of %s OK\n", rs->s->gfile);
		}
		return (rs->opts->advance);
	}
	if (WEXITSTATUS(ret) == 1) {
		fprintf(stderr, "Conflicts during merge of %s\n", rs->s->gfile);
		return (rs->opts->force);
	}
	fprintf(stderr,
	    "Merge of %s failed for unknown reasons\n", rs->s->gfile);
	unlink(rs->s->gfile);
	return (0);
}

private int
c_smerge(resolve *rs)
{
	int	ret;

	/* bk get -pM {rs->s->gfile} > {rs->s->gfile} */
	ret = sysio(0, rs->s->gfile, 0,
	    "bk", "get", "-pkM", rs->s->gfile, SYS);
	ret &= 0xffff;
	/*
	 * We need to restart even if there are errors, otherwise we think
	 * the file is not writable.
	 */
	sccs_restart(rs->s);
	if (ret) {
		fprintf(stderr,
		    "Merge of %s failed for unknown reasons\n", rs->s->gfile);
		rs->opts->errors = 1;
		return (0);
	}
	return (0);
}

int
c_revtool(resolve *rs)
{
	char    *av[7];
	char    revs[2][MAXREV+2];
	int     i;

	av[i=0] = "bk";
	av[++i] = "revtool";
	sprintf(revs[0], "-l%s", rs->revs->local);
	sprintf(revs[1], "-r%s", rs->revs->remote);
	av[++i] = revs[0];
	av[++i] = revs[1];
	av[++i] = rs->s->gfile;
	av[++i] = 0;
	spawnvp(_P_DETACH, "bk", av);

	return (0);
}

private int
c_fm3tool(resolve *rs)
{
	char	*av[10];
	int	i;
	char	*revs[2];

	av[i=0] = "bk";
	av[++i] = "fm3tool";
	av[++i] = "-f";
	revs[0] = aprintf("-l%s", rs->revs->local);
	revs[1] = aprintf("-r%s", rs->revs->remote);
	av[++i] = revs[0];
	av[++i] = revs[1];
	av[++i] = rs->s->gfile;
	av[++i] = 0;
	spawnvp(_P_DETACH, "bk", av);
	free(revs[0]);
	free(revs[1]);
	return (0);
}

/* Run fmtool in background */
private int
c_fmtool(resolve *rs)
{
	char	*av[10];
	names	*n = rs->tnames;

	assert(exists(n->local));
	assert(exists(n->remote));
	av[0] = "bk";
	av[1] = "fmtool";
	av[2] = n->local;
	av[3] = n->remote;
	av[4] = rs->s->gfile;
	av[5] = 0;
	spawnvp(_P_DETACH, "bk", av);
	return (0);
}

private int
c_vm(resolve *rs)
{
	unless (exists(rs->s->gfile)) {
		fprintf(stderr, "%s hasn't been merged yet.\n", rs->s->gfile);
		return (0);
	}
	more(rs, rs->s->gfile);
	return (0);
}

private int
needs_merge(resolve *rs)
{
	MMAP	*m;
	char	*t;
	int	ok = 1;

	unless (exists(rs->s->gfile)) return (1);
	if (BINARY(rs->s)) return (0);

	unless (m = mopen(rs->s->gfile, "r")) {
		fprintf(stderr, "%s cannot be opened\n", rs->s->gfile);
		return (0);
	}
	t = m->where;
	while (t < m->end) {
		if (strneq(t, "<<<<<<", 6)) {
			ok = 0;
			break;
		}
		while (t < m->end && *t++ != '\n');
	}
	mclose(m);
	if (ok) return (0);

	fprintf(stderr, 
"\nThe file has unresolved conflicts.  These conflicts are marked in the\n\
file like so\n\
\n\
	<<<<<<< BitKeeper/tmp/bk.sh_lm@1.191\n\
	changes made by user lm in revision 1.191 of bk.sh\n\
	some more changes by lm\n\
	=======\n\
	changes made by user awc in revision 1.189.1.5 of bk.sh\n\
	more changes by awc\n\
	>>>>>>> BitKeeper/tmp/bk.sh_awc@1.189.1.5\n\
\n\
Use 'e' to edit the file and resolve these conflicts.\n\
Alternatively, you use 'f' to try the graphical filemerge.\n\n");
	return (1);
}

private int
commit(resolve *rs, int delta_now, int show_diffs)
{
	df_opt	dop = {.out_header = 1, .out_unified = 1};

	if (rs->opts->debug) fprintf(stderr, "commit(%s)\n", rs->s->gfile);

	unless (exists(rs->s->gfile) && writable(rs->s->gfile)) {
		fprintf(stderr, "%s has not been merged\n", rs->s->gfile);
		return (0);
	}

	/* If we came from fm3tool or similar, the sccs file flags could
	 * be out of date.  sccs_hasDiffs() needs them to be correct */
	rs->s = sccs_restart(rs->s);

	if (rs->opts->force) goto doit;

	if (needs_merge(rs)) return (0);
	
	/*
	 * If in text only mode, then check in the file now.
	 * Otherwise, leave it for citool.
	 */
doit:	if (delta_now) {
		unless (sccs_hasDiffs(rs->s, 0, 0)) {
			do_delta(rs->opts, rs->s, SCCS_MERGE);
		} else {
			if (show_diffs) {
				sccs_diffs(rs->s, 0, 0, &dop, stdout);
			}
			do_delta(rs->opts, rs->s, 0);
		}
	}
	rs->opts->resolved++;
	return (1);
}

private int
c_commit(resolve *rs)
{
	return (commit(rs, rs->opts->textOnly, 0));
}

private int
c_ccommit(resolve *rs)
{
	return (commit(rs, 1, 1));
}

/*
 * Run a shell with the following in the envronment
 * BK_GCA=gca-filename
 * BK_LOCAL=local-filename
 * BK_REMOTE=remote-filename
 * BK_MERGE=filename
 * XXX - need to add the revs
 */
int
c_shell(resolve *rs)
{
	names	*n = rs->tnames;
	int	i;
	char	*path = strdup(getenv("PATH"));
	char	*av[10];

	safe_putenv("BK_GCA=%s", n->gca);
	safe_putenv("BK_LOCAL=%s", n->local);
	safe_putenv("BK_REMOTE=%s", n->remote);
	safe_putenv("BK_MERGE=%s", rs->s->gfile);
	safe_putenv("PATH=%s", getenv("BK_OLDPATH"));
	unless (av[i=0] = getenv("SHELL")) av[i=0] = "sh";
	unless (rs->shell && rs->shell[0]) {
		av[++i] = "-i";
	} else {
		av[++i] = "-c";
		av[++i] = rs->shell;
	}
	av[++i] = 0;
	spawnvp(_P_WAIT, av[0], av);
	safe_putenv("PATH=%s", path);
	free(path);
	return (0);
}

/* An alias for !cp $BK_LOCAL $BK_MERGE */
private int
c_ul(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->local, rs->s->gfile, SYS)) {
		return (c_commit(rs));
	}
	return (0);
}

/* An alias for !cp $BK_REMOTE $BK_MERGE */
private int
c_ur(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->remote, rs->s->gfile, SYS)) {
		return (c_commit(rs));
	}
	return (0);
}

private int
c_skip(resolve *rs)
{
	if (LOCKED(rs->s) && !sccs_hasDiffs(rs->s, 0, 1)) {
		rs->s = sccs_restart(rs->s);
		sccs_unedit(rs->s, SILENT);
	} else {
		/*
		 * Mark to be able to find it next time, since
		 * conflict() will skip it because of a pfile.
		 */
		xfile_store(rs->s->gfile, 'a', "");
	}
	++rs->opts->hadConflicts;
	rs->opts->notmerged =
	    addLine(rs->opts->notmerged,strdup(rs->s->gfile));
	return (1);
}

rfuncs	c_funcs[] = {
    { "?", "help", "print this help", c_help },
    { "!", "shell", "escape to an interactive shell", c_shell },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "C", "commit", "commit the merged file", c_commit },
    { "CC", "commit w/comments",
	    "commit the merged file with comments", c_ccommit },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
      "run side-by-side graphical difftool on local and remote", res_difftool },
    { "dl", "diff local", "diff the GCA vs. local file", c_dgl },
    { "dr", "diff remote", "diff the GCA vs. remote file", c_dgr },
    { "dlm", "diff local merge",
      "automerge (if not yet merged) and diff the local file vs. merge file",
      c_dlm },
    { "drm", "diff remote merge",
      "automerge (if not yet merged) and diff the remote file vs merge file",
      c_drm },
    { "e", "edit merge",
      "automerge (if not yet merged) and then edit the merged file", c_em },
    { "f", "fm3tool", "merge with graphical three-way filemerge", c_fm3tool },
    { "f2", "fmtool", "merge with graphical two-way filemerge", c_fmtool },
    { "h", "history", "revision history of all changes", res_h },
    { "hl", "hist local", "revision history of the local changes", res_hl },
    { "hr", "hist remote", "revision history of the remote changes", res_hr },
    { "H", "helptool", "show merge help in helptool", c_helptool },
    { "m", "merge", "automerge the two files", c_merge },
    { "p", "revtool", "graphical picture of the file history", c_revtool },
    { "q", "quit", "immediately exit resolve", c_quit },
    { "s", "sccsmerge", "merge the two files using SCCS' algorithm", c_smerge },
    { "sd", "sdiff",
      "side-by-side diff of the local file vs. the remote file", res_sdiff },
    { "ul", "use local", "use the local version of the file", c_ul },
    { "ur", "use remote", "use the remote version of the file", c_ur },
    { "v", "view merge", "view the merged file", c_vm },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "S", "skip file", "skip this file and resolve it later", c_skip },
    { "x", "explain", "explain the choices", c_explain },
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
resolve_contents(resolve *rs)
{
	names	*n;
	char	*nm;
	ser_t	d;
	int	ret;
	char	buf[MAXPATH];

	if (BINARY(rs->s) || NOMERGE(rs->s)) {
		return (resolve_binary(rs));
	}

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_contents: ");
		resolve_dump(rs);
	}
	n = new(names);
	nm = basenm(rs->s->gfile);
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
		ret = resolve_loop("content conflict", rs, c_funcs);
	}
	unlink(n->local);
	unlink(n->gca);
	unlink(n->remote);
	freenames(n, 1);
	return (ret);
}
