/*
 * resolve_mode.c - resolver for permission conflicts
 *
 * (c) 2000 Larry McVoy
 */
#include "resolve.h"

int
m_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	char	*l, *r;

	if ((d->gca->mode == d->local->mode) &&
	    (d->gca->mode != d->remote->mode)) {
		l = aprintf("unchanged mode  %s", mode2a(d->local->mode));
		r = aprintf("changed mode to %s", mode2a(d->remote->mode));
	} else if ((d->gca->mode != d->local->mode) &&
	    (d->gca->mode == d->remote->mode)) {
		l = aprintf("changed mode to %s", mode2a(d->local->mode));
		r = aprintf("unchanged mode  %s", mode2a(d->remote->mode));
	} else {
		l = aprintf("changed mode to %s", mode2a(d->local->mode));
		r = aprintf("changed mode to %s", mode2a(d->remote->mode));
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a mode conflict of some type.\n\
Local:  %s@%s\n\t%s\n\
Remote: %s@%s\n\t%s\n\
---------------------------------------------------------------------------\n",
	    rs->s->gfile, rs->revs->local, l,
	    rs->s->gfile, rs->revs->remote, r);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	free(l);
	free(r);
	return (0);
}

int
m_explain(resolve *rs)
{
	fprintf(stderr, 
"----------------------------------------------------------------------\n\
The file has a mode conflict of some type.  What this means is that\n\
both the local and the remove have attached file permissions which\n\
have both changed.  You need to resolve this by picking one of them.\n\
Your choices are to either choose the local or remote modes.  If it\n\
turns out that neither of these are what you want, you can pick one,\n\
finish the resolve, and then do a \"bk chmod <mode> file\".\n\
----------------------------------------------------------------------\n\n");
	return (0);
}

/* add the local modes to the remote file */
int
m_local(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	sccs_close(rs->s); /* for win32 */
	mode_delta(rs,
	    rs->s->sfile, r, l->mode, sccs_Xfile(rs->s, 'r'), REMOTE);
	return (1);
}

/* add the remote modes to the local file */
int
m_remote(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	sccs_close(rs->s); /* for win32 */
	mode_delta(rs, rs->s->sfile, l, r->mode, sccs_Xfile(rs->s, 'r'), LOCAL);
	return (1);
}

rfuncs	m_funcs[] = {
    { "?", "help", "print this help", m_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "l", "local", "use the mode on local file", m_local },
    { "p", "revtool", "graphical picture of the file history", res_revtool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the mode on remote file", m_remote },
    { "x", "explain", "explain the choices", m_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the modes.
 */
int
resolve_modes(resolve *rs)
{
        if (rs->opts->debug) {
		fprintf(stderr, "resolve_modes: ");
		resolve_dump(rs);
	}
	rs->prompt = rs->s->gfile;
	return (resolve_loop("mode conflict", rs, m_funcs));
}
