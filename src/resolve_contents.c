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
New work has been added locally and remotely and must be merged.
File:   %s\n\
GCA:    %s (greatest common ancestor)\n\
Local:  %s\n\
Remote: %s\n\
---------------------------------------------------------------------------\n",
	    rs->d->pathname, 
	    rs->revs->gca, rs->revs->local, rs->revs->remote);
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
	names	*n = (names*)rs->opaque;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->gca, n->local, rs->pager);
	system(cmd);
	return (0);
}

int
c_dgr(resolve *rs)
{
	names	*n = (names*)rs->opaque;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->gca, n->remote, rs->pager);
	system(cmd);
	return (0);
}

int
c_dlm(resolve *rs)
{
	names	*n = (names*)rs->opaque;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->local, rs->s->gfile, rs->pager);
	system(cmd);
	return (0);
}

int
c_drm(resolve *rs)
{
	names	*n = (names*)rs->opaque;
	char	cmd[MAXPATH*2];

	sprintf(cmd, "bk diff %s %s | %s", n->remote, rs->s->gfile, rs->pager);
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

int
c_merge(resolve *rs)
{
	names	*n = (names*)rs->opaque;
	int	ret;
	char	cmd[MAXPATH*4];

	sprintf(cmd, "bk %s %s %s %s %s",
	    rs->opts->mergeprog, n->local, n->gca, n->remote, rs->s->gfile);
	ret = system(cmd) & 0xffff;
	if (ret == 0) {
		unless (rs->opts->quiet) {
			fprintf(rs->opts->log,
			    "merge of %s OK\n", rs->s->gfile);
		}
		if (edit(rs)) return (-1);
	    	rs->opts->resolved++;
		unlink(sccs_Xfile(rs->s, 'r'));
		return (1);
	}
	if (ret == 0xff00) {
	    	fprintf(stderr, "Can not execute '%s'\n", cmd);
		rs->opts->errors = 1;
		return (0);
	}
	if ((ret >> 8) == 1) {
		fprintf(stderr, "Conflicts during merge of %s\n", rs->s->gfile);
		rs->opts->hadconflicts = 1;
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
	av[2] = rs->s->gfile;
	av[3] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

/* Run fmtool in background */
int
c_fmtool(resolve *rs)
{
	char	*av[10];
	names	*n = (names*)rs->opaque;

	av[0] = "bk";
	av[1] = "fmtool";
	av[2] = n->local;
	av[3] = n->gca;
	av[4] = n->remote;
	av[5] = rs->s->gfile;
	av[6] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
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
	sprintf(cmd, "%s %s", rs->pager, rs->s->gfile);
	return (0);
}

rfuncs	c_funcs[] = {
    { "?", "help", "print this help", c_help },
    { "a", "abort", "abort the patch", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
      "run graphical difftool on local and remote", res_difftool },
    { "dl", "diff local", "diff the GCA vs local file", c_dgl },
    { "dr", "diff remote", "diff the GCA vs remote file", c_dgr },
    { "dlm", "diff local merge", "diff the local file vs merge file", c_dlm },
    { "dlm", "diff remote merge", "diff the remote file vs merge file", c_drm },
    { "e", "edit merge", "edit the merge file", res_vl },
    { "el", "edit local", "edit the local file", res_vl },
    { "er", "edit remote", "edit the remote file", res_vr },
    { "f", "fmtool", "merge with graphical filemerge", c_fmtool },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "H", "helptool", "show merge help in helptool", c_helptool },
    { "m", "merge", "automerge the two files", c_merge },
    { "p", "sccstool", "graphical picture of the file history", c_sccstool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "vl", "view local", "view the local file", res_vl },
    { "vm", "view merge", "view the merge file", c_vm },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", c_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the contents.
 * Set up the list of temp files in the opaque pointer,
 * get the various versions into the temp files,
 * do the resolve,
 * and then clean them up.
 */
int
resolve_contents(resolve *rs)
{
	names	names;
	delta	*d;
	char	*nm = basenm(rs->s->gfile);
	int	ret;
	char	buf[MAXPATH];

	d = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	names.local = strdup(buf);
	d = sccs_getrev(rs->s, rs->revs->gca, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	names.gca = strdup(buf);
	d = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
	assert(d);
	sprintf(buf, "BitKeeper/tmp/%s_%s@%s", nm, d->user, d->rev);
	names.remote = strdup(buf);
	rs->opaque = (void*)&names;
	rs->prompt = rs->s->gfile;
	if (get_revs(rs, &names)) {
		rs->opts->errors = 1;
		freenames(&names, 0);
		return (-1);
	}
	ret = resolve_loop("resolve_contents", rs, c_funcs);
	freenames(&names, 0);
	return (ret);
}
