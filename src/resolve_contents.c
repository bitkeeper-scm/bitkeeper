/*
 * resolve_contents.c - resolver for content conflicts
 */
#include "resolve.h"

int
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
	    rs->d->pathname, rs->revs->gca, rs->revs->local, rs->revs->remote);
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

int
c_explain(resolve *rs)
{
	system("bk help merging");
	return (0);
}

int
res_clear(resolve *rs)
{
	system("clear");
	return (0);
}

int
c_dgl(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->gca, n->local, 1);
	return (0);
}

int
c_dgr(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->gca, n->remote, 1);
	return (0);
}

int
c_dlm(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->gca, rs->s->gfile, 1);
	return (0);
}

int
c_drm(resolve *rs)
{
	names	*n = rs->tnames;

	do_diff(rs, n->remote, rs->s->gfile, 1);
	return (0);
}

int
c_em(resolve *rs)
{
	unless (exists(rs->s->gfile)) c_merge(rs);
	sys(rs->editor, rs->s->gfile, SYS);
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
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

/*
 * Unedit the file if we are quiting.
 */
int
c_quit(resolve *rs)
{
	if (IS_LOCKED(rs->s)) {
		fprintf(stderr, "Unedit %s\n", rs->s->gfile);
		rs->s = sccs_restart(rs->s);
		sccs_clean(rs->s, CLEAN_UNEDIT);
	}
	exit(1);
}

int
c_merge(resolve *rs)
{
	names	*n = rs->tnames;
	int	ret;

	ret = sys("bk", rs->opts->mergeprog,
	    n->local, n->gca, n->remote, rs->s->gfile, SYS);
	sccs_restart(rs->s);
	unless (WIFEXITED(ret)) {
	    	fprintf(stderr, "Cannot execute '%s'\n", rs->opts->mergeprog);
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
	return (0);
}

int
c_smerge(resolve *rs)
{
	int	ret;
	char	*branch;
	char	*opt;

	branch = strchr(rs->revs->local, '.');
	assert(branch);
	if (strchr(++branch, '.')) {
		branch = rs->revs->local;
	} else {
		branch = rs->revs->remote;
	}
	/* bk get -pM{branch} {rs->s->gfile} > {rs->s->gfile} */
	opt = aprintf("-pM%s", branch);
	ret = sysio(0, rs->s->gfile, 0, "bk", "get", opt, rs->s->gfile, SYS);
	free(opt);
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
	char    revs[3][MAXREV+2];
	int     i;

	av[i=0] = "bk";
	av[++i] = "revtool";
	sprintf(revs[0], "-l%s", rs->revs->local);
	sprintf(revs[1], "-r%s", rs->revs->remote);
	sprintf(revs[2], "-G%s", rs->revs->gca);
	av[++i] = revs[0];
	av[++i] = revs[1];
	av[++i] = revs[2];
	av[++i] = rs->s->gfile;
	av[++i] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);

	return (0);
}

int
c_fm3tool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "fm3tool";
	av[2] = rs->tnames->local;
	av[3] = rs->tnames->gca;
	av[4] = rs->tnames->remote;
	av[5] = rs->s->gfile;
	av[6] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

/* Run fmtool in background */
int
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
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

int
c_vm(resolve *rs)
{
	unless (exists(rs->s->gfile)) {
		fprintf(stderr, "%s hasn't been merged yet.\n", rs->s->gfile);
		return (0);
	}
	more(rs, rs->s->gfile);
	return (0);
}

int
needs_merge(resolve *rs)
{
	MMAP	*m;
	char	*t;
	int	ok = 1;

	unless (exists(rs->s->gfile)) return (1);
	if (rs->s->encoding & E_BINARY) return (0);

	unless (m = mopen(rs->s->gfile, "r")) {
		fprintf(stderr, "%s cannot be opened\n", rs->s->gfile);
		return (0);
	}
	while ((t = mnext(m)) && ((m->end - t) > 7)) {
		if (strneq(t, "<<<<<<", 6)) {
			ok = 0;
			break;
		}
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

int
c_commit(resolve *rs)
{
	if (rs->opts->debug) fprintf(stderr, "commit(%s)\n", rs->s->gfile);

	unless (exists(rs->s->gfile) && writable(rs->s->gfile)) {
		fprintf(stderr, "%s has not been merged\n", rs->s->gfile);
		return (0);
	}

	if (rs->opts->force) goto doit;

	if (needs_merge(rs)) return (0);
	
	/*
	 * If in text only mode, then check in the file now.
	 * Otherwise, leave it for citool.
	 */
doit:	if (rs->opts->textOnly) {
		unless (sccs_hasDiffs(rs->s, 0, 0)) {
			do_delta(rs->opts, rs->s, SCCS_MERGE);
		} else {
			do_delta(rs->opts, rs->s, 0);
		}
	}
	rs->opts->resolved++;
	return (1);
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
	char	buf[MAXPATH];

	sprintf(buf, "BK_GCA=%s", n->gca); putenv(strdup(buf));
	sprintf(buf, "BK_LOCAL=%s", n->local); putenv(strdup(buf));
	sprintf(buf, "BK_REMOTE=%s", n->remote); putenv(strdup(buf));
	sprintf(buf, "BK_MERGE=%s", rs->s->gfile); putenv(strdup(buf));
	unless (rs->shell && rs->shell[0]) {
		system("sh -i");
		return (0);
	}
	system(rs->shell);
	return (0);
}

/* An alias for !cp $BK_LOCAL $BK_MERGE */
int
c_ul(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->local, rs->s->gfile, SYS)) {
		return (c_commit(rs));
	}
	return (0);
}

/* An alias for !cp $BK_REMOTE $BK_MERGE */
int
c_ur(resolve *rs)
{
	names	*n = rs->tnames;

	unless (sys("cp", "-f", n->remote, rs->s->gfile, SYS)) {
		return (c_commit(rs));
	}
	return (0);
}

rfuncs	c_funcs[] = {
    { "?", "help", "print this help", c_help },
    { "!", "shell", "escape to an interactive shell", c_shell },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "C", "commit", "commit the merged file", c_commit },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
      "run side-by-side graphical difftool on local and remote", res_difftool },
    { "dl", "diff local", "diff the GCA vs. local file", c_dgl },
    { "dr", "diff remote", "diff the GCA vs. remote file", c_dgr },
    { "dlm", "diff local merge", "diff the local file vs. merge file", c_dlm },
    { "drm", "diff remote merge", "diff the remote file vs merge file", c_drm },
    { "e", "edit merge",
      "automerge (if not yet merged) and then edit the merged file", c_em },
    { "f", "fmtool", "merge with graphical filemerge", c_fmtool },
    { "F", "fm3tool",
      "merge with graphical experimental three-way filemerge", c_fm3tool },
    { "h", "history", "revision history of all changes", res_h },
    { "hl", "hist local", "revision history of the local changes", res_hl },
    { "hr", "hist remote", "revision history of the remote changes", res_hr },
    { "H", "helptool", "show merge help in helptool", c_helptool },
    { "m", "merge", "automerge the two files", c_merge },
    { "p", "revtool", "graphical picture of the file history", c_revtool },
    { "q", "quit", "immediately exit resolve", c_quit },
    { "s", "sccsmerge", "merge the two files using SCCS' algorthm", c_smerge },
    { "sd", "sdiff",
      "side-by-side diff of the local file vs. the remote file", res_sdiff },
    { "ul", "use local", "use the local version of the file", c_ul },
    { "ur", "use remote", "use the remote version of the file", c_ur },
    { "v", "view merge", "view the merged file", c_vm },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
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
	delta	*d;
	int	ret;
	char	buf[MAXPATH];

	if (rs->s->encoding & E_BINARY) return (resolve_binary(rs));

	n = calloc(1, sizeof(*n));
	nm = basenm(rs->s->gfile);
	d = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	n->local = strdup(buf);
	d = sccs_getrev(rs->s, rs->revs->gca, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	n->gca = strdup(buf);
	d = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	n->remote = strdup(buf);
	rs->tnames = n;
	rs->prompt = rs->s->gfile;
	rs->res_contents = 1;
	if (get_revs(rs, n)) {
		rs->opts->errors = 1;
		freenames(n, 1);
		return (-1);
	}
	unless (IS_LOCKED(rs->s)) {
		if (edit(rs)) return (-1);
	}
	if (sameFiles(n->local, n->remote)) {
		automerge(rs, n);
		ret = 1;
	} else {
		ret = resolve_loop("resolve_contents", rs, c_funcs);
	}
	unlink(n->local);
	unlink(n->gca);
	unlink(n->remote);
	freenames(n, 1);
	return (ret);
}
