/*
 * resolve_filetypes.c - resolver for different file types
 *
 * (c) 2000 Larry McVoy
 */
#include "resolve.h"

int
ft_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	char	l[100], r[100];

	if ((fileType(d->gca->mode) == fileType(d->local->mode)) &&
	    (fileType(d->gca->mode) != fileType(d->remote->mode))) {
		sprintf(l,
		    "unchanged type  %s", mode2FileType(d->local->mode));
		sprintf(r,
		    "changed type to %s", mode2FileType(d->remote->mode));
	} else if ((fileType(d->gca->mode) != fileType(d->local->mode)) &&
	    (fileType(d->gca->mode) == fileType(d->remote->mode))) {
		sprintf(l, "changed type to %s", mode2FileType(d->local->mode));
		sprintf(r,
		    "unchanged type  %s", mode2FileType(d->remote->mode));
	} else {
		sprintf(l,
		    "changed type to %s", mode2FileType(d->local->mode));
		sprintf(r,
		    "changed type to %s", mode2FileType(d->remote->mode));
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a file type conflict:
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
	return (0);
}

int
ft_explain(resolve *rs)
{
	fprintf(stderr, 
"\n----------------------------------------------------------------------\n\
The file has a type conflict.  What this means is that both the local\n\
and the remove have attached file types which have changed.\n\
You need to resolve this by picking one of them.\n\
Your choices are to either choose the local or remote type.\n\
----------------------------------------------------------------------\n\n");
	return (0);
}

/* make the remote take the local file type */
int
ft_local(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	type_delta(rs, rs->s->sfile, l, r, sccs_Xfile(rs->s, 'r'), LOCAL);
	return (1);
}

/* make the local take the remote file type */
int
ft_remote(resolve *rs)
{
	delta	*l = sccs_getrev(rs->s, rs->revs->local, 0, 0);
	delta	*r = sccs_getrev(rs->s, rs->revs->remote, 0, 0);

	type_delta(rs, rs->s->sfile, l, r, sccs_Xfile(rs->s, 'r'), REMOTE);
	return (1);
}

rfuncs	ft_funcs[] = {
    { "?", "help", "print this help", ft_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "l", "local", "use the type of the local file", ft_local },
    { "p", "sccstool", "graphical picture of the file history", res_sccstool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the type of the remote file", ft_remote },
    { "x", "explain", "explain the choices", ft_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the file types.
 */
int
resolve_filetypes(resolve *rs)
{
	rs->prompt = rs->s->gfile;
	return (resolve_loop("resolve_filetypes", rs, ft_funcs));
}
