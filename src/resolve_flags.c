/*
 * resolve_flags.c - resolver for file flags
 *
 * (c) 2000 Larry McVoy
 */
#include "resolve.h"

/* XXX - this should be shared with the xflags.c & prs dspec code */
private	char	*
flags(int bits)
{
	int	comma = 0;
	static 	char buf[512];

#define	fs(s)	strcat(buf, s)

	buf[0] = 0;
	if (bits & X_RCS) {
		if (comma) fs(","); fs("RCS"); comma = 1;
	}
	if (bits & X_YEAR4) {
		if (comma) fs(","); fs("YEAR4"); comma = 1;
	}
#ifdef X_SHELL
	if (bits & X_SHELL) {
		if (comma) fs(","); fs("SHELL"); comma = 1;
	}
#endif
	if (bits & X_EXPAND1) {
		if (comma) fs(","); fs("EXPAND1"); comma = 1;
	}
	if (bits & X_SCCS) {
		if (comma) fs(","); fs("SCCS"); comma = 1;
	}
	if (bits & X_EOLN_NATIVE) {
		if (comma) fs(","); fs("EOLN_NATIVE"); comma = 1;
	}
	if (bits & X_KV) {
		if (comma) fs(","); fs("KV"); comma = 1;
	}
	if (bits & X_NOMERGE) {
		if (comma) fs(","); fs("NOMERGE"); comma = 1;
	}
	if (bits & X_MONOTONIC) {
		if (comma) fs(","); fs("MONOTONIC"); comma = 1;
	}
	unless (buf[0]) strcat(buf, "<none>");
	assert(strlen(buf) < sizeof buf);
	return (buf);
}

private int
f_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	int	lf, rf, gf;
	char	*g, *l, *r;

	lf = sccs_xflags(d->local);
	rf = sccs_xflags(d->remote);
	gf = sccs_xflags(d->gca);
	g = aprintf("original  flags  %s", gf ? flags(gf) : "<none>");
	if ((gf == lf) && (gf != rf)) {
		l = aprintf("unchanged flags  %s", flags(lf));
		r = aprintf("changed flags to %s", flags(rf));
	} else if ((gf != lf) && (gf == rf)) {
		l = aprintf("changed flags to %s", flags(lf));
		r = aprintf("unchanged flags  %s", flags(rf));
	} else {
		l = aprintf("changed flags to %s", flags(lf));
		r = aprintf("changed flags to %s", flags(rf));
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
	free(g); free(l); free(r);
	return (0);
}

private int
f_explain(resolve *rs)
{
	fprintf(stderr, 
"------------------------------------------------------------------------\n\
This file has a flag conflict where the local and remote files have \n\
file flags that are different.\n\
You need to resolve this conflict by picking one of the files.\n\
Your can choose either the local or remote flags as the ones you want.\n\
However, if it turns out that neither of these are what you want, you can \n\
temporarily pick one file to finish the resolve, and then modify the \n\
file later using the following: \"bk admin -f<FLAG> file\".\n\
------------------------------------------------------------------------\n\n");
	return (0);
}

/* add the local modes to the remote file */
private int
f_local(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	sccs_close(rs->s); /* for win32 */
	flags_delta(rs, rs->s->sfile,
	    r, sccs_xflags(l), sccs_Xfile(rs->s, 'r'), REMOTE);
	return (1);
}

/* add the remote modes to the local file */
private int
f_remote(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	sccs_close(rs->s); /* for win32 */
	flags_delta(rs, rs->s->sfile,
	    l, sccs_xflags(r), sccs_Xfile(rs->s, 'r'), LOCAL);
	return (1);
}

/*
 * auto resolve flags according to the following table.
 * 
 * key: <gca val><local val><remote val> = <new val>
 *
 *  000 -> 0
 *  001 -> 1
 *  010 -> 1
 *  011 -> 1
 *  100 -> 0
 *  101 -> 0
 *  110 -> 0
 *  111 -> 1
 *
 *   out = ((L ^ G) | (R ^ G)) ^ G
 *   out = (L & R) | ~G & (~L & R | L & ~R)
 *   out = (L & R) | ~G & (L ^ R)
 */
private int
f_merge(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
	delta	*g = sccs_getrev(rs->s, rs->revs->gca, 0, 0);
	int	lf, rf, gf;
	int	newflags;

	lf = sccs_xflags(l);
	rf = sccs_xflags(r);
	gf = sccs_xflags(g);

	newflags = (lf & rf) | ~gf & (lf ^ rf);

	if (newflags == lf) {
		f_local(rs);
		unless (rs->opts->quiet) {
			fprintf(stderr,
"automerge OK.  Using flags from local copy.\n");
		}
	} else if (newflags == rf) {
		f_remote(rs);
		unless (rs->opts->quiet) {
			fprintf(stderr,
"automerge OK.  Using flags from remote copy.\n");
		}
	} else {
		/* add the new modes to the local file */
		sccs_close(rs->s); /* for win32 */
		flags_delta(rs, rs->s->sfile, 
			    l, newflags, sccs_Xfile(rs->s, 'r'), LOCAL);
		/* remove delta must be refetched after previous delta */
		r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
		sccs_close(rs->s); /*
				    * for win32, have to close it again
				    * becuase the previous flags_delta()
				    * called sccs_init()
				    */
		flags_delta(rs, rs->s->sfile, 
			    r, newflags, sccs_Xfile(rs->s, 'r'), REMOTE);
		unless (rs->opts->quiet) {
			fprintf(stderr,
"automerge OK. Added merged flags (%s) to both local and remote files.\n",
				flags(newflags));
		}
	}
	return (1);
}

private	rfuncs	f_funcs[] = {
    { "?", "help", "print this help", f_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "l", "local", "use the flags on local file", f_local },
    { "m", "merge", "attempt to automerge the flags conflict", f_merge },
    { "p", "revtool", "graphical picture of the file history", res_revtool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the flags on remote file", f_remote },
    { "x", "explain", "explain the choices", f_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the flags.
 */
int
resolve_flags(resolve *rs)
{
        if (rs->opts->debug) {
		fprintf(stderr, "resolve_flags: ");
		resolve_dump(rs);
	}
	if (rs->opts->automerge && f_merge(rs)) return (1);
	rs->prompt = rs->s->gfile;
	return (resolve_loop("flag conflict", rs, f_funcs));
}
