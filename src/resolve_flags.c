/*
 * resolve_flags.c - resolver for file flags
 *
 * (c) 2000 Larry McVoy
 */
#include "resolve.h"

char	*
flags(int bits)
{
	int	comma = 0;
	static 	char buf[512];

#define	fs(s)	strcat(buf, s)

	buf[0] = 0;
	if (bits & X_BITKEEPER) {
		fs("BITKEEPER");
		comma = 1;
	}
	if (bits & X_YEAR4) {
		if (comma) fs(","); fs("YEAR4"); comma = 1;
	}
	if (bits & X_RCS) {
		if (comma) fs(","); fs("RCS"); comma = 1;
	}
	if (bits & X_SCCS) {
		if (comma) fs(","); fs("SCCS"); comma = 1;
	}
	if (bits & X_EXPAND1) {
		if (comma) fs(","); fs("EXPAND1"); comma = 1;
	}
	if (bits & X_CSETMARKED) {
		if (comma) fs(","); fs("CSETMARKED"); comma = 1;
	}
	if (bits & X_HASH) {
		if (comma) fs(","); fs("HASH"); comma = 1;
	}
#ifdef S_ISSHELL
	if (bits & X_ISSHELL) {
		if (comma) fs(","); fs("ISSHELL"); comma = 1;
	}
#endif
	return (buf);
}

int
f_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	int	lf, rf, gf;
	char	g[512], l[512], r[512];

	lf = sccs_getxflags(d->local);
	rf = sccs_getxflags(d->remote);
	gf = sccs_getxflags(d->gca);
	sprintf(g, "original  flags  %s", gf ? flags(gf) : "<none>");
	if ((gf == lf) && (gf != rf)) {
		sprintf(l, "unchanged flags  %s", flags(lf));
		sprintf(r, "changed flags to %s", flags(rf));
	} else if ((gf != lf) && (gf == rf)) {
		sprintf(l, "changed flags to %s", flags(lf));
		sprintf(r, "unchanged flags  %s", flags(rf));
	} else {
		sprintf(l, "changed flags to %s", flags(lf));
		sprintf(r, "changed flags to %s", flags(rf));
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a flags conflict.\n\n\
GCA:	%s@%s\n\t%s\n\
Local:  %s@%s\n\t%s\n\
Remote: %s@%s\n\t%s\n\
---------------------------------------------------------------------------\n",
	    rs->s->gfile, rs->revs->gca, g,
	    rs->s->gfile, rs->revs->local, l,
	    rs->s->gfile, rs->revs->remote, r);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

int
f_explain(resolve *rs)
{
	fprintf(stderr, 
"----------------------------------------------------------------------\n\
The file has a flags conflict.  This means that both the local\n\
and remote have attached file flags which have are different.\n\
You need to resolve this by picking one of them.\n\
Your choices are to either choose the local or remote flags.  If it\n\
turns out that neither of these are what you want, you can pick one,\n\
finish the resolve, and then do a \"bk admin -f<FLAG> file\".\n\
----------------------------------------------------------------------\n\n");
	return (0);
}

/* add the local modes to the remote file */
f_local(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	flags_delta(rs, rs->s->sfile,
	    r, sccs_getxflags(l), sccs_Xfile(rs->s, 'r'), REMOTE);
	return (1);
}

/* add the remote modes to the local file */
f_remote(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	flags_delta(rs, rs->s->sfile,
	    l, sccs_getxflags(r), sccs_Xfile(rs->s, 'r'), LOCAL);
	return (1);
}

rfuncs	f_funcs[] = {
    { "?", "help", "print this help", f_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "e", "explain", "explain the choices", f_explain },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "l", "local", "use the flags on local file", f_local },
    { "p", "sccstool", "graphical picture of the file history", res_sccstool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the flags on remote file", f_remote },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the flags.
 */
int
resolve_flags(resolve *rs)
{
	rs->prompt = rs->s->gfile;
	return (resolve_loop("resolve_flags", rs, f_funcs));
}
