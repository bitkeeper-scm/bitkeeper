/*
 * resolve_tags.c - resolver for file tags
 *
 * (c) 2000 Larry McVoy
 */
#include "resolve.h"

typedef struct	tags {
	char	*name;
	delta	*local;		/* delta associated with the tag */
	delta	*mlocal;	/* metadata delta for the tag */
	delta	*remote;	/* delta associated with the tag */
	delta	*mremote;	/* metadata delta for the tag */
} tags;

int
t_help(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;
	int	i;

	fprintf(stderr, "Tag ``%s'' was added to two changesets.\n", t->name);
	fprintf(stderr,
	    "\tLocal:  %s\n\tRemote: %s\n", t->local->rev, t->remote->rev);
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
----------------------------------------------------------------------\n\n");
	return (0);
}

int
t_local(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	sccs_tagMerge(rs->s, t->local, t->name);
	return (1);
}

int
t_remote(resolve *rs)
{
	tags	*t = (tags *)rs->opaque;

	sccs_tagMerge(rs->s, t->remote, t->name);
	return (1);
}

rfuncs	t_funcs[] = {
    { "?", "help", "print this help", t_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "cl", "clear", "clear the screen", res_clear },
    { "l", "local", "use the local changeset", t_local },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remote", "use the remote changeset", t_remote },
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
		return (1);
	}
	chdir(RESYNC2ROOT);
	local = sccs_init(CHANGESET, 0, 0);
	chdir(ROOT2RESYNC);
	rs = resolve_init(opts, s);
	rs->prompt = "Tag conflict";
	rs->opaque = (void*)&t;
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		bzero(&t, sizeof(t));
		t.name = kv.key.dptr;
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
		while (!resolve_loop("resolve_tags", rs, t_funcs));
	}
	sccs_free(local);
	resolve_free(rs);
	return (1);
}
