/*
 * resolve_renames.c - resolver for rename conflicts
 */
#include "resolve.h"

int
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
		if (i = slotTaken(rs->opts, rs->snames->remote)) {
			l = "";
			switch (i) {
			    case SFILE_CONFLICT:
				r = "wants to move, has sfile conflict.";
				break;
			    case GFILE_CONFLICT:
				r = "wants to move, has gfile conflict.";
				break;
			    case RESYNC_CONFLICT:
				r = "wants to move, has RESYNC conflict.";
				break;
			}
		} else if (i = slotTaken(rs->opts, rs->snames->local)) {
			r = "";
			switch (i) {
			    case SFILE_CONFLICT:
				l = "wants to move, has sfile conflict.";
				break;
			    case GFILE_CONFLICT:
				l = "wants to move, has gfile conflict.";
				break;
			    case RESYNC_CONFLICT:
				l = "wants to move, has RESYNC conflict.";
				break;
			}
		} else {
			l = "unknown conflict";
			r = "unknown conflict";
		}
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a name conflict of some type.
GCA:    %s@%s (greatest common ancestor)\n\
Local:  %s@%s %s\n\
Remote: %s@%s %s\n\
---------------------------------------------------------------------------\n",
	    rs->gnames->gca, rs->revs->gca,
	    rs->gnames->local, rs->revs->local, l,
	    rs->gnames->remote, rs->revs->remote, r);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

int
r_explain(resolve *rs)
{
	fprintf(stderr, "Don't you wish.\n");
	return (0);
}

/* Run helptool in background */
int
r_helptool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "helptool";
	av[2] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

int
r_merge(resolve *rs)
{
	fprintf(stderr,
	    "Merge of %s failed for unknown reasons\n", rs->s->gfile);
	return (0);
}

int
r_sccstool(resolve *rs)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "sccstool";
	av[2] = rs->s->gfile;
	av[3] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

rfuncs	r_funcs[] = {
    { "?", "help", "print this help", r_help },
    { "a", "abort", "abort the patch", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
	"run graphical difftool on local and remote", res_difftool },
    { "e", "explain", "explain the choices", r_explain },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "H", "helptool", "run helptool", r_helptool },
    { "p", "sccstool", "graphical picture of the file history", r_sccstool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the renames.
 * Set up the list of temp files in the opaque pointer,
 * get the various versions into the temp files,
 * do the resolve,
 * and then clean them up.
 */
int
resolve_renames(resolve *rs)
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
	rs->prompt = rs->gnames->local;
	if (get_revs(rs, &names)) {
		rs->opts->errors = 1;
		freenames(&names, 0);
		return (-1);
	}
	ret = resolve_loop("resolve_renames", rs, r_funcs);
	freenames(&names, 0);
	return (ret);
}
