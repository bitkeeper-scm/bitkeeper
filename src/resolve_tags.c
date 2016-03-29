/*
 * Copyright 2000-2007,2011-2012,2015-2016 BitMover, Inc
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
 * resolve_tags.c - resolver for file tags
 */
#include "resolve.h"

typedef struct	tags {
	char	*name;
	ser_t	local;		/* delta associated with the tag */
	ser_t	mlocal;	/* metadata delta for the tag */
	ser_t	remote;	/* delta associated with the tag */
	ser_t	mremote;	/* metadata delta for the tag */
} tags;
private	int	n;		/* number of tags still to resolve */

private int
t_help(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;
	int	i;
	char	*p, *r;
	int	len;

	fprintf(stderr, "Tag ``%s'' was added to two changesets.\n", t->name);
	fprintf(stderr, "\nLocal:  ChangeSet %s\n", REV(rs->s, t->local));
	r = COMMENTS(rs->s, t->local);
	while (p = eachline(&r, &len)) fprintf(stderr, "\t%.*s\n", len, p);
	fprintf(stderr, "Remote: ChangeSet %s\n", REV(rs->s, t->remote));
	r = COMMENTS(rs->s, t->remote);
	while (p = eachline(&r, &len)) fprintf(stderr, "\t%.*s\n", len, p);
	fprintf(stderr, "\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

private int
t_explain(resolve *rs)
{
	fprintf(stderr, 
"----------------------------------------------------------------------\n\
The file has a tag conflict.  This means that both the local\n\
and remote have attached the same tag to different changesets.\n\
You need to resolve this by picking a changeset for this tag.\n\
You can move the tag to either of the existing locations or\n\
to the merge changeset.\n\
----------------------------------------------------------------------\n\n");
	return (0);
}

private int
t_local(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	/*
	 * If the "right" one is later, that's what will be hit in
	 * the symbol table anyway, don't add another node.
	 */
	if (t->mlocal < t->mremote) {
		if (sccs_tagMerge(rs->s, t->local, t->name)) {
			rs->opts->errors = 1;
		}
	} else if (n == 1) {
		if (sccs_tagMerge(rs->s, 0, 0)) {
			rs->opts->errors = 1;
		}
	}
	return (1);
}

private int
t_remote(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	/*
	 * If the "right" one is later, that's what will be hit in
	 * the symbol table anyway, don't add another node.
	 */
	if (t->mlocal > t->mremote) {
		if (sccs_tagMerge(rs->s, t->remote, t->name)) {
			rs->opts->errors = 1;
		}
	} else if (n == 1) {
		if (sccs_tagMerge(rs->s, 0, 0)) {
			rs->opts->errors = 1;
		}
	}
	return (1);
}

private int
t_revtool(resolve *rs)
{
	char    *av[7];
	char    revs[2][MAXREV+2];
	int     i;
	tags	*t = (tags *)rs->opaque;

	av[i=0] = "bk";
	av[++i] = "revtool";
	sprintf(revs[0], "-l%s", REV(rs->s, t->local));
	sprintf(revs[1], "-r%s", REV(rs->s, t->remote));
	av[++i] = revs[0];
	av[++i] = revs[1];
	av[++i] = rs->s->gfile;
	av[++i] = 0;
	spawnvp(_P_DETACH, "bk", av);
	return (0);
}

private int
t_tags(resolve *rs)
{
	sys("bk", "tags", SYS);
	return (0);
}

private int
t_merge(resolve *rs)
{
	FILE	*f;
	tags	*t = (tags *)rs->opaque;

	unless (xfile_exists(CHANGESET, 'r')) {
		fprintf(stderr,
"This is a tag conflict only, there is no merge changeset.\n"
"You must pick the local or remote tag location.\n");
		return (0);
	}
	/* close the graph, doesn't matter which side, we're adding a new tag */
	if (sccs_tagMerge(rs->s, 0, 0)) {
		rs->opts->errors = 1;
		return (1);
	}
	f = fopen("SCCS/t.ChangeSet", "a");
	fprintf(f, "%s\n", t->name);
	fclose(f);
	return (1);
}

rfuncs	t_funcs[] = {
    { "?", "help", "print this help", t_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "l", "local", "use the local changeset", t_local },
    { "m", "merge", "move the tag to the merge changeset", t_merge },
    { "p", "revtool", "graphical picture of the file history", t_revtool },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the remote changeset", t_remote },
    { "t", "tags", "print the tag history", t_tags },
    { "x", "explain", "explain the choices", t_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the flags.
 *
 * We walk the two branches and see if any tag exists on both
 */
void
resolve_tags(opts *opts)
{
	char 	s_cset[] = CHANGESET;
	sccs	*s = sccs_init(s_cset, 0);
	MDBM	*m = sccs_tagConflicts(s);
	sccs	*local;
	ser_t	d;
	resolve	*rs;
	kvpair	kv;
	int	i = 0;
	int	a, b;
	tags	t;
	char	key[MAXKEY];

	unless (m) {
		sccs_free(s);
		return;
	}
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		i++;
	}
	unless (i) {
		if (sccs_tagMerge(s, 0, 0)) opts->errors = 1;
out:		sccs_free(s);
		mdbm_close(m);
		return;
	}
	if (opts->automerge) {
		opts->hadConflicts++;
		opts->notmerged =
		    addLine(opts->notmerged, strdup("| tag conflict |"));
		goto out;
	}
	chdir(RESYNC2ROOT);
	local = sccs_init(s_cset, 0);
	chdir(ROOT2RESYNC);
	rs = resolve_init(opts, s);
	rs->opaque = (void*)&t;
	fprintf(stderr, "\nTag conflicts from this pull:\n");
	fprintf(stderr, "                        Local"
	    "                             Remote\n");
	fprintf(stderr, "           --------------------------------  "
	    "--------------------------------\n");
	fprintf(stderr, "%-10s %-12s %-7s %-11s  %-12s %-7s %s\n",
	    "Tag", "Rev", "User", "Date", "Rev", "User", "Date");
	for (n = 0, kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		ser_t	l, r;
		char	*sdate;

		sscanf(kv.val.dptr, "%d %d", &a, &b);
		sccs_sdelta(s, a, key);
		if (sccs_findKey(local, key)) {
			l = a;
			r = b;
		} else {
			r = a;
			l = b;
		}
		sdate = delta_sdate(s, l);
		fprintf(stderr,
		    "%-10.10s %-12s %-7.7s %-11.11s  ",
		    kv.key.dptr,
		    REV(s, l), USER(s, l), sdate + 3);
		sdate = delta_sdate(s, r);
		fprintf(stderr,
		    "%-12s %-7.7s %11.11s\n",
		    REV(s, r), USER(s, r), sdate + 3);
		n++;
	}
	fprintf(stderr, "\n");
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		bzero(&t, sizeof(t));
		t.name = kv.key.dptr;
		rs->prompt = aprintf("\"%s\"", t.name);
		sscanf(kv.val.dptr, "%d %d", &a, &b);
		sccs_sdelta(s, a, key);
		if (sccs_findKey(local, key)) {
			t.mlocal = a;
			t.mremote = b;
		} else {
			t.mremote = a;
			t.mlocal = b;
		}
		assert(t.mlocal && t.mremote);
		for (d = t.mlocal; TAG(s, d); d = PARENT(s, d));
		t.local = d;
		for (d = t.mremote; TAG(s, d); d = PARENT(s, d));
		t.remote = d;

		if (rs->opts->debug) {
			fprintf(stderr, "resolve_tags: ");
			resolve_dump(rs);
		}
		while (!resolve_loop("tag conflict", rs, t_funcs));
		sccs_free(s);
		s = sccs_init(s_cset, 0);
		rs->s = s;
		free(rs->prompt);
		n--;
		if (opts->errors) break;
	}
	sccs_free(local);
	mdbm_close(m);
	resolve_free(rs);
	return;
}
