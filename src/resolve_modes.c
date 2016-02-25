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
 * resolve_mode.c - resolver for permission conflicts
 */
#include "resolve.h"

private int
m_help(resolve *rs)
{
	int	i;
	deltas	*d = (deltas *)rs->opaque;
	char	*l, *r;

	if ((MODE(rs->s, d->gca) == MODE(rs->s, d->local)) &&
	    (MODE(rs->s, d->gca) != MODE(rs->s, d->remote))) {
		l = aprintf("unchanged mode  %s", mode2a(MODE(rs->s, d->local)));
		r = aprintf("changed mode to %s", mode2a(MODE(rs->s, d->remote)));
	} else if ((MODE(rs->s, d->gca) != MODE(rs->s, d->local)) &&
	    (MODE(rs->s, d->gca) == MODE(rs->s, d->remote))) {
		l = aprintf("changed mode to %s", mode2a(MODE(rs->s, d->local)));
		r = aprintf("unchanged mode  %s", mode2a(MODE(rs->s, d->remote)));
	} else {
		l = aprintf("changed mode to %s", mode2a(MODE(rs->s, d->local)));
		r = aprintf("changed mode to %s", mode2a(MODE(rs->s, d->remote)));
	}

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
The file has conflicting changes to the modes.\n\
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
private int
m_local(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s); /* for win32 */
	mode_delta(rs, rs->s->sfile, r, MODE(rs->s, l), REMOTE);
	return (1);
}

/* add the remote modes to the local file */
private int
m_remote(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s); /* for win32 */
	mode_delta(rs, rs->s->sfile, l, MODE(rs->s, r), LOCAL);
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
	deltas	*d = (deltas *)rs->opaque;

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_modes: ");
		resolve_dump(rs);
	}
	if (rs->opts->automerge) {
		if ((MODE(rs->s, d->gca) == MODE(rs->s, d->local)) &&
		    (MODE(rs->s, d->gca) != MODE(rs->s, d->remote))) {
		    	/* remote only change, use remote */
			m_remote(rs);
			return (1);
		} else if ((MODE(rs->s, d->gca) != MODE(rs->s, d->local)) &&
		    (MODE(rs->s, d->gca) == MODE(rs->s, d->remote))) {
		    	/* local only change, use local */
			m_local(rs);
			return (1);
		} else {
			rs->opts->hadConflicts++;
			rs->opts->notmerged =
			    addLine(rs->opts->notmerged,
			    aprintf("%s (modes)", rs->s->gfile));
			return (EAGAIN);
		}
	}
	rs->prompt = rs->s->gfile;
	return (resolve_loop("mode conflict", rs, m_funcs));
}
