/*
 * Copyright 2000-2003,2009,2011,2015-2016 BitMover, Inc
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
 * resolve_flags.c - auto resolver for file flags
 */
#include "resolve.h"

/* add the local modes to the remote file */
private int
f_local(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s); /* for win32 */
	flags_delta(rs, rs->s->sfile, r, XFLAGS(rs->s, l), REMOTE);
	return (1);
}

/* add the remote modes to the local file */
private int
f_remote(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);

	sccs_close(rs->s); /* for win32 */
	flags_delta(rs, rs->s->sfile, l, XFLAGS(rs->s, r), LOCAL);
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
int
resolve_flags(resolve *rs)
{
	ser_t	l = sccs_findrev(rs->s, rs->revs->local);
	ser_t	r = sccs_findrev(rs->s, rs->revs->remote);
	ser_t	g = sccs_findrev(rs->s, rs->revs->gca);
	int	lf, rf, gf, newflags;

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_flags: ");
		resolve_dump(rs);
	}
	lf = XFLAGS(rs->s, l);
	rf = XFLAGS(rs->s, r);
	gf = XFLAGS(rs->s, g);

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
		flags_delta(rs, rs->s->sfile, l, newflags, LOCAL);
		/* remove delta must be refetched after previous delta */
		r = sccs_findrev(rs->s, rs->revs->remote);
		sccs_close(rs->s); /*
				    * for win32, have to close it again
				    * becuase the previous flags_delta()
				    * called sccs_init()
				    */
		flags_delta(rs, rs->s->sfile, r, newflags, REMOTE);
		unless (rs->opts->quiet) {
			fprintf(stderr,
			    "automerge OK. Added merged flags "
			    "to both local and remote versions.\n");
		}
	}
	return (1);
}
