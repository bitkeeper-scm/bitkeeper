/*
 * resolve_tags.c - resolver for file tags
 *
 * (c) 2000-2001 Larry McVoy
 */
#include "resolve.h"

typedef struct	tags {
	char	*name;
	delta	*local;		/* delta associated with the tag */
	delta	*mlocal;	/* metadata delta for the tag */
	delta	*remote;	/* delta associated with the tag */
	delta	*mremote;	/* metadata delta for the tag */
} tags;
private	int	n;		/* number of tags still to resolve */

int
t_help(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;
	int	i;

	fprintf(stderr, "Tag ``%s'' was added to two changesets.\n", t->name);
	fprintf(stderr, "\nLocal:  ChangeSet %s\n", t->local->rev);
	EACH(t->local->comments) {
		fprintf(stderr, "\t%s\n", t->local->comments[i]);
	}
	fprintf(stderr, "Remote: ChangeSet %s\n", t->remote->rev);
	EACH(t->remote->comments) {
		fprintf(stderr, "\t%s\n", t->remote->comments[i]);
	}
	fprintf(stderr, "\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

int
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

int
t_local(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	if (n == 1) {
		sccs_tagMerge(rs->s, t->local, t->name);
	} else {
		/*
		 * If the "right" one is later, that's what will be hit in
		 * the symbol table anyway, don't add another node.
		 */
		if (t->mlocal->serial < t->mremote->serial) {
			sccs_tagLeaf(rs->s, t->local, t->mlocal, t->name);
		}
	}
	return (1);
}

int
t_remote(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	if (n == 1) {
		sccs_tagMerge(rs->s, t->remote, t->name);
	} else {
		/* see t_local */
		if (t->mlocal->serial > t->mremote->serial) {
			sccs_tagLeaf(rs->s, t->remote, t->mremote, t->name);
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
	sprintf(revs[0], "-l%s", t->local->rev);
	sprintf(revs[1], "-r%s", t->remote->rev);
	av[++i] = revs[0];
	av[++i] = revs[1];
	av[++i] = rs->s->gfile;
	av[++i] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

int
t_tags(resolve *rs)
{
	sys("bk", "tags", SYS);
	return (0);
}

int
t_merge(resolve *rs)
{
	FILE	*f;
	tags	*t = (tags *)rs->opaque;

	unless (exists("SCCS/r.ChangeSet")) {
		fprintf(stderr,
"This is a tag conflict only, there is no merge changeset.\n"
"You must pick the local or remote tag location.\n");
		return (0);
	}
	/* close the graph, doesn't matter which side, we're adding a new tag */
	sccs_tagMerge(rs->s, t->local, 0);
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
int
resolve_tags(opts *opts)
{
	char 	s_cset[] = CHANGESET;
	sccs	*s = sccs_init(s_cset, 0, 0);
	MDBM	*m = sccs_tagConflicts(s);
	sccs	*local;
	delta	*d;
	resolve	*rs;
	kvpair	kv;
	int	i = 0;
	int	a, b;
	tags	t;
	char	key[MAXKEY];

	unless (m) {
		sccs_free(s);
		return (1);
	}
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		i++;
	}
	unless (i) {
		sccs_tagMerge(s, 0, 0);
		sccs_free(s);
		mdbm_close(m);
		return (1);
	}
	chdir(RESYNC2ROOT);
	local = sccs_init(s_cset, 0, 0);
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
		delta	*l, *r;

		sscanf(kv.val.dptr, "%d %d", &a, &b);
		d = sfind(s, a);
		sccs_sdelta(s, d, key);
		if (sccs_findKey(local, key)) {
			l = d;
			r = sfind(s, b);
		} else {
			r = d;
			l = sfind(s, b);
		}
		fprintf(stderr,
		    "%-10.10s %-12s %-7.7s %-11.11s  %-12s %-7.7s %11.11s\n",
		    kv.key.dptr, l->rev, l->user, l->sdate + 3,
		    r->rev, r->user, r->sdate + 3);
		n++;
	}
	fprintf(stderr, "\n");
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		bzero(&t, sizeof(t));
		t.name = kv.key.dptr;
		rs->prompt = aprintf("\"%s\"", t.name);
		sscanf(kv.val.dptr, "%d %d", &a, &b);
		d = sfind(s, a);
		assert(d);
		sccs_sdelta(s, d, key);
		if (sccs_findKey(local, key)) {
			t.mlocal = d;
			t.mremote = sfind(s, b);
		} else {
			t.mremote = d;
			t.mlocal = sfind(s, b);
		}
		assert(t.mlocal && t.mremote);
		for (d = t.mlocal; d->type == 'R'; d = d->parent);
		t.local = d;
		for (d = t.mremote; d->type == 'R'; d = d->parent);
		t.remote = d;

		if (rs->opts->debug) {
			fprintf(stderr, "resolve_tags: ");
			resolve_dump(rs);
		}
		while (!resolve_loop("tag conflict", rs, t_funcs));
		sccs_free(s);
		s = sccs_init(s_cset, 0, 0);
		rs->s = s;
		free(rs->prompt);
		n--;
	}
	sccs_free(local);
	mdbm_close(m);
	resolve_free(rs);
	return (1);
}
