/*
 * Copyright 2000-2005,2009,2011-2012,2015-2016 BitMover, Inc
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
 * resolve_generic.c - support code for the resolver
 */
#include "resolve.h"

/*
 * Set up for a resolver.
 */
resolve	*
resolve_init(opts *opts, sccs *s)
{
	resolve	*rs = new(resolve);
	char	buf[MAXKEY];

	rs->opts = opts;
	rs->s = s;
	sccs_sdelta(rs->s, sccs_ino(rs->s), buf);
	rs->key = strdup(buf);
	if (rs->snames = res_getnames(rs->s, 'm')) {
		rs->gnames	   = new(names);
		rs->gnames->local  = sccs2name(rs->snames->local);
		rs->gnames->gca    = sccs2name(rs->snames->gca);
		rs->gnames->remote = sccs2name(rs->snames->remote);
	}
	rs->revs = res_getnames(rs->s, 'r');
	rs->pager = pager();
	unless (rs->editor = getenv("EDITOR")) rs->editor = EDITOR;
	if (rs->snames && !streq(rs->snames->local, rs->snames->remote)) {
		if (streq(rs->snames->local, rs->snames->gca)) {
			if (rs->revs) {
				rs->d = sccs_findrev(rs->s, rs->revs->remote);
				assert(rs->d);
			} else {
				rs->d = sccs_top(rs->s);
				assert(rs->d);
			}
		} else if (streq(rs->snames->remote, rs->snames->gca)) {
			assert(rs->revs);
			rs->d = sccs_findrev(rs->s, rs->revs->local);
			assert(rs->d);
		}
	} else {
		rs->d = sccs_top(rs->s);
		assert(rs->d);
	}
	if (rs->d) {
		rs->dname = name2sccs(PATHNAME(rs->s, rs->d));
	}
	if (opts->debug) {
		fprintf(stderr, "rs_init(%s - %s)",
			rs->s->gfile, rs->d ? PATHNAME(rs->s, rs->d) : "<conf>");
		if (rs->snames) fprintf(stderr, " snames");
		if (rs->revs) fprintf(stderr, " revs");
		fprintf(stderr, "\n");
	}
	return (rs);
}

private void
pnames(char *prefix, names *n)
{
	if (n->local) fprintf(stderr, "%s local: %s\n", prefix, n->local);
	if (n->gca) fprintf(stderr, "%s gca: %s\n", prefix, n->gca);
	if (n->remote) fprintf(stderr, "%s remote: %s\n", prefix, n->remote);
}

/*
 * Debugging, dump everything that we can.
 */
void
resolve_dump(resolve *rs)
{
	fprintf(stderr, "=== resolve dump of %s ===\n", p2str(rs));
	if (rs->prompt) fprintf(stderr, "prompt: %s\n", rs->prompt);
	if (rs->s) fprintf(stderr, "sfile: %s\n", rs->s->sfile);
	if (rs->key) fprintf(stderr, "key: %s\n", rs->key);
	if (rs->d) fprintf(stderr, "delta: %s\n", REV(rs->s, rs->d));
	if (rs->dname) fprintf(stderr, "dname: %s\n", rs->dname);
	if (rs->revs) pnames("revs", rs->revs);
	if (rs->rnames) pnames("rnames", rs->rnames);
	if (rs->gnames) pnames("gnames", rs->gnames);
	if (rs->snames) pnames("snames", rs->snames);
	if (rs->tnames) pnames("tnames", rs->tnames);
	fprintf(stderr, "n: %d\n", rs->n);
	if (rs->opaque) fprintf(stderr, "opaque: %s\n", p2str(rs->opaque));
	if (rs->res_gcreate) fprintf(stderr, "gcreate conflict\n");
	if (rs->res_screate) fprintf(stderr, "screate conflict\n");
	if (rs->res_dirfile) fprintf(stderr, "dirfile conflict\n");
	if (rs->res_resync) fprintf(stderr, "resync conflict\n");
	if (rs->res_contents) fprintf(stderr, "contents conflict\n");
}

void
resolve_free(resolve *rs)
{
	if (rs->gnames) freenames(rs->gnames, 1);
	if (rs->snames) freenames(rs->snames, 1);
	if (rs->revs) freenames(rs->revs, 1);
	if (rs->dname) free(rs->dname);
	sccs_free(rs->s);
	free(rs->key);
	bzero(rs, sizeof(*rs));
	free(rs);
}

int
resolve_loop(char *name, resolve *rs, rfuncs *rf)
{
	int	i, ret, bang = -1;
	char	buf[100];

	rs->funcs = rf;
	if (rs->opts->debug) {
		fprintf(stderr, "%s\n", name);
		fprintf(stderr, "\tsccs: %s\n", rs->s->gfile);
		if (rs->d) {
			fprintf(stderr, "\ttot: %s@%s\n",
			    PATHNAME(rs->s, rs->d), REV(rs->s, rs->d));
		}
	}

	if (rs->opts->noconflicts) {
		fprintf(stderr,
	    	"Cannot resolve %s for %s in a push\n",
		name, rs->s->gfile);
		return (1);
	}

	rs->n = 0;
	for (i = 0; rf[i].spec && !streq(rf[i].spec, "!"); i++);
	if (rf[i].spec) bang = i;
	while (1) {
		fprintf(stderr, "(%s) %s>> ", name, rs->prompt);
		if (getline(0, buf, sizeof(buf)) < 0) strcpy(buf, "q");
		unless (buf[0]) strcpy(buf, "?");
		if (streq(buf, "dump")) {
			resolve_dump(rs);
			continue;
		}
		if (streq(buf, "debug")) {
			rs->opts->debug = !rs->opts->debug;
			continue;
		}

again:		/* 100 tries for the same file means we're hosed.  */
		if (++rs->n == 100) return (ELOOP);
		for (i = 0; rf[i].spec && !streq(rf[i].spec, buf); i++);
		if (!rf[i].spec && (buf[0] == '!') && (bang >= 0)) {
			i = bang;
			rs->shell = &buf[1];
		} else {
			rs->shell = 0;
		}
		unless (rf[i].spec) {
			strcpy(buf, "?");
			goto again;
		}
		if (rs->opts->debug) {
			fprintf(stderr,
			    "[%s] Calling %s on %s\n", buf, rf[i].name, name);
		}
		if (ret = rf[i].func(rs)) {
			if (rs->opts->debug) {
				fprintf(stderr,
				    "%s returns %d\n", rf[i].name, ret);
			}
			return (ret);
		}
		if (rs->opts->debug) {
			fprintf(stderr, "%s returns 0\n", rf[i].name);
		}
		/*
		 * reap any background processes
		 * so we don't overflow our proc table.
		 */
		while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	}
}
