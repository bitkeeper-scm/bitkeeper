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
	return (0);
}

int
c_explain(resolve *rs)
{
	system("bk help merge");
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
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->gca, n->local, rs->pager);
	system(cmd);
	return (0);
}

int
c_dgr(resolve *rs)
{
	names	*n = rs->tnames;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->gca, n->remote, rs->pager);
	system(cmd);
	return (0);
}

int
c_dlm(resolve *rs)
{
	names	*n = rs->tnames;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->local, rs->s->gfile, rs->pager);
	system(cmd);
	return (0);
}

int
c_drm(resolve *rs)
{
	names	*n = rs->tnames;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->remote, rs->s->gfile, rs->pager);
	system(cmd);
	return (0);
}

int
c_em(resolve *rs)
{
	char	cmd[MAXPATH*2];

	sprintf(cmd, "%s %s", rs->editor, rs->s->gfile);
	system(cmd);
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
	names	*n = rs->tnames;
	int	ret;
	char	cmd[MAXPATH*4];

	/* IS_EDITED doesn't work - XXX */
	if (exists(sccs_Xfile(rs->s, 'p'))) {
		fprintf(stderr, "Unedit %s\n", rs->s->gfile);
		sccs_clean(rs->s, CLEAN_UNEDIT);
	}
	exit(1);
}

int
c_merge(resolve *rs)
{
	names	*n = rs->tnames;
	int	ret;
	char	cmd[MAXPATH*4];

	sprintf(cmd, "bk %s %s %s %s %s",
	    rs->opts->mergeprog, n->local, n->gca, n->remote, rs->s->gfile);
	unless (IS_EDITED(rs->s)) {
		if (edit(rs)) return (-1);
	}
	ret = system(cmd) & 0xffff;
	if (ret == 0) {
		unless (rs->opts->quiet) {
			fprintf(stderr, "merge of %s OK\n", rs->s->gfile);
		}
		return (rs->opts->advance);
	}
	if (ret == 0xff00) {
	    	fprintf(stderr, "Can not execute '%s'\n", cmd);
		rs->opts->errors = 1;
		return (0);
	}
	if ((ret >> 8) == 1) {
		if (rs->opts->advance && rs->opts->force) return (1);
		fprintf(stderr, "Conflicts during merge of %s\n", rs->s->gfile);
		return (0);
	}
	fprintf(stderr,
	    "Merge of %s failed for unknown reasons\n", rs->s->gfile);
	return (0);
}

int
c_sccstool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "sccstool";
	av[2] = "-l";
	av[3] = rs->revs->local;
	av[4] = "-r";
	av[5] = rs->revs->remote;
	av[6] = "-G";
	av[7] = rs->revs->gca;
	av[8] = rs->s->gfile;
	av[9] = 0;
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
	char	cmd[MAXPATH];

	unless (exists(rs->s->gfile)) {
		fprintf(stderr, "%s hasn't been merged yet.\n", rs->s->gfile);
		return (0);
	}
	more(rs, rs->s->gfile);
	return (0);
}

needs_merge(resolve *rs)
{
	MMAP	*m;
	char	*t;

	unless (exists(rs->s->gfile)) return (0);

	unless (m = mopen(rs->s->gfile, "r")) {
		fprintf(stderr, "%s can not be opened\n", rs->s->gfile);
		return (0);
	}
	while ((t = mnext(m)) && ((m->end - t) > 7)) {
		if (strneq(t, "<<<<<<", 6)) {
			mclose(m);
			return (1);
		}
	}
	mclose(m);
	return (0);
}

int
c_commit(resolve *rs)
{
	unless (exists(rs->s->gfile)) {
		fprintf(stderr, "%s has not been merged\n", rs->s->gfile);
		return (0);
	}

	if (rs->opts->force) goto doit;

	if (needs_merge(rs)) {
		fprintf(stderr, "%s %s\n",
		    "The file has unresolved conflicts. ",
		    "Use 'e' to edit the file and resolve.");
		return (0);
	}
	
	/*
	 * If in text only mode, then check in the file now.
	 * Otherwise, leave it for citool.
	 */
doit:	if (rs->opts->textOnly) do_delta(rs->opts, rs->s);
	rs->opts->resolved++;
	return (1);
}

rfuncs	c_funcs[] = {
    { "?", "help", "print this help", c_help },
    { "a", "abort", "abort the patch", res_abort },
    { "C", "commit", "commit to the merged file", c_commit },
    { "cl", "clear", "clear the screen", res_clear },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
      "run graphical difftool on local and remote", res_difftool },
    { "dl", "diff local", "diff the GCA vs local file", c_dgl },
    { "dr", "diff remote", "diff the GCA vs remote file", c_dgr },
    { "dlm", "diff local merge", "diff the local file vs merge file", c_dlm },
    { "drm", "diff remote merge", "diff the remote file vs merge file", c_drm },
    { "e", "edit merge", "edit the merge file", c_em },
    { "f", "fmtool", "merge with graphical filemerge", c_fmtool },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "H", "helptool", "show merge help in helptool", c_helptool },
    { "m", "merge", "automerge the two files", c_merge },
    { "p", "sccstool", "graphical picture of the file history", c_sccstool },
    { "q", "quit", "immediately exit resolve", c_quit },
    { "sd", "sdiff",
      "side by side diff of the local file vs. the remote file", res_sdiff },
    { "vl", "view local", "view the local file", res_vl },
    { "v", "view merge", "view the merge file", c_vm },
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
	names	*n = calloc(1, sizeof(*n));
	delta	*d;
	char	*nm = basenm(rs->s->gfile);
	int	ret;
	char	buf[MAXPATH];

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
	ret = resolve_loop("resolve_contents", rs, c_funcs);
	unlink(n->local);
	unlink(n->gca);
	unlink(n->remote);
	return (ret);
}
