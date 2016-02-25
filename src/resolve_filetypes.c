/*
 * Copyright 2000-2001,2005,2012,2015-2016 BitMover, Inc
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
 * resolve_filetypes.c - resolver for different file types
 */
#include "resolve.h"

private int
ft_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	char	*l, *r;

	if ((fileType(MODE(rs->s, d->gca)) == fileType(MODE(rs->s, d->local))) &&
	    (fileType(MODE(rs->s, d->gca)) != fileType(MODE(rs->s, d->remote)))) {
		l = aprintf(
		    	"unchanged type  %s", mode2FileType(MODE(rs->s, d->local)));
		r = aprintf(
		    	"changed type to %s", mode2FileType(MODE(rs->s, d->remote)));
	} else if ((fileType(MODE(rs->s, d->gca)) != fileType(MODE(rs->s, d->local))) &&
	    (fileType(MODE(rs->s, d->gca)) == fileType(MODE(rs->s, d->remote)))) {
		l = aprintf(
			"changed type to %s", mode2FileType(MODE(rs->s, d->local)));
		r = aprintf(
		    	"unchanged type  %s", mode2FileType(MODE(rs->s, d->remote)));
	} else {
		l = aprintf(
		    	"changed type to %s", mode2FileType(MODE(rs->s, d->local)));
		r = aprintf(
		    	"changed type to %s", mode2FileType(MODE(rs->s, d->remote)));
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has a file type conflict:\n\
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

private int
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
private int
ft_local(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s);	/* for windows */
	type_delta(rs, rs->s->sfile, l, r, LOCAL);
	return (1);
}

/* make the local take the remote file type */
private int
ft_remote(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s);	/* for windows */
	type_delta(rs, rs->s->sfile, l, r, REMOTE);
	return (1);
}

rfuncs	ft_funcs[] = {
    { "?", "help", "print this help", ft_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "l", "local", "use the type of the local file", ft_local },
    { "p", "revtool", "graphical picture of the file history", res_revtool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the type of the remote file", ft_remote },
    { "x", "explain", "explain the choices", ft_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the file types.
 *
 * Note on automerge: this isn't precisely correct, consider this:
 * GCA: typeA
 * Local: change to typeB then back to typeA
 * Remote: change to typeB
 * Result: automerge to typeB but many people would want a manual merge there.
 * My thinking is they can fix up the merge after the fact because the far
 * more common case is that it's a change on one side.
 * The other answer is just refuse to allow switching of types.
 */
int
resolve_filetypes(resolve *rs)
{
	deltas	*d = (deltas *)rs->opaque;

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_filetypes: ");
		resolve_dump(rs);
	}
	if (rs->opts->automerge) {
#define	SAMETYPE(a, b)	(fileType(a) == fileType(b))
		if (SAMETYPE(MODE(rs->s, d->gca), MODE(rs->s, d->local)) &&
		    !SAMETYPE(MODE(rs->s, d->gca), MODE(rs->s, d->remote))) {
		    	/* remote only change, use remote */
			ft_remote(rs);
			return (1);
		} else if (!SAMETYPE(MODE(rs->s, d->gca), MODE(rs->s, d->local)) &&
		    SAMETYPE(MODE(rs->s, d->gca), MODE(rs->s, d->remote))) {
		    	/* local only change, use local */
			ft_local(rs);
			return (1);
		} else {
			/* XXX - this will never happen, right? */
			rs->opts->hadConflicts++;
			rs->opts->notmerged =
			    addLine(rs->opts->notmerged,
			    aprintf("%s (types)", rs->s->gfile));
			return (EAGAIN);
		}
	}
	rs->prompt = rs->s->gfile;
	return (resolve_loop("filetype conflict", rs, ft_funcs));
}
