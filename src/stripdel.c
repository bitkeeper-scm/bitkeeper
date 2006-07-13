/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"

typedef struct {
	u32	respectCset:1;
	u32	stripBranches:1;
	u32	checkOnly:1;
	u32	quiet:1;
	u32	forward:1;
	u32	iflags;
} s_opts;

private	delta	*checkCset(sccs *s);
private int doit(sccs *, s_opts);
private	int do_check(sccs *s, int flags);
private int strip_list(s_opts);

private	int	getFlags = 0;

int
stripdel_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	c, rc;
	MDBM	*config;
	char	*co;
	s_opts	opts = {1, 0, 0, 0, 0};
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "bcCdqr;")) != -1) {
		switch (c) {
		    case 'b': opts.stripBranches = 1; break;	/* doc 2.0 */
		    case 'c': opts.checkOnly = 1; break;	/* doc 2.0 */
		    case 'C': opts.respectCset = 0; break;	/* doc 2.0 */
		    case 'd':
			opts.forward = 1;
			opts.iflags = INIT_WACKGRAPH;
			break;
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) goto usage;
			break;
		    default:
usage:			system("bk help -s stripdel");
			return (1);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		rc = strip_list(opts);
		goto done;
	}
	unless (opts.stripBranches || rargs.rstart) {
		fprintf(stderr, "stripdel: must specify revisions.\n");
		return (1);
	}

	if ((config = proj_config(0)) &&
	    (co = mdbm_fetch_str(config, "checkout"))) {
		if (strieq(co, "get")) getFlags = GET_EXPAND;
		if (strieq(co, "edit")) getFlags = GET_EDIT;
	}

	/*
	 * Too dangerous to do autoexpand.
	 * XXX - might want to insist that there is only one file.
	 */
	unless (av[optind]) {
		fprintf(stderr, "stripdel: must specify one file name\n");
		return (1);
	}
	name = sfileFirst("stripdel", &av[optind], SF_NODIREXPAND);
	if (sfileNext()) {
		fprintf(stderr, "stripdel: only one file at a time\n");
		return (1);
	}

	unless ((s = sccs_init(name, opts.iflags)) && HASGRAPH(s)) {
		fprintf(stderr, "stripdel: can't init %s\n", name);
		return (1);
	}

	if (!opts.stripBranches &&
	    range_process("stripdel", s, RANGE_SET, &rargs)) {
		return (1);
	}
	rc = doit(s, opts);
	sccs_free(s);
	sfileDone();
done:
	return (rc);
}

private int
doit(sccs *s, s_opts opts)
{
	delta	*e;
	int	left, n;
	int	flags = 0;

	if (opts.quiet) flags |= SILENT;
	if (BITKEEPER(s) && opts.stripBranches) {
		fprintf(stderr,
		    "stripdel: can't strip branches from a BitKeeper file.\n");
		return (1);
	}

	if (HAS_PFILE(s)) {
		if (!HAS_GFILE(s) || sccs_clean(s, SILENT)) {
			fprintf(stderr, 
			    "stripdel: can't strip an edited file.\n");
			return (1);
		}
		s = sccs_restart(s);
	}

	if (MONOTONIC(s) && !opts.forward) {
		verbose((stderr, 
		    "Not stripping deltas from MONOTONIC file %s\n", s->gfile));
		for (e = s->table; e; e = e->next) {
			if ((e->flags & D_SET) && (e->type == 'D')) {
				e->dangling = 1;
			}
		}
		sccs_newchksum(s);
		return (0);
	}

	if (opts.respectCset && (e = checkCset(s))) {
		fprintf(stderr,
    			"stripdel: can't remove committed delta %s@%s\n",
		    s->gfile, e->rev);
		return (1);
	}

	left = stripdel_setMeta(s, opts.stripBranches, &n); 
	if (opts.checkOnly) return (do_check(s, flags));
	unless (left) {
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "stripdel: can't remove edited %s\n", s->gfile);
			return (1);
		}
		/* see ya! */
		verbose((stderr, "stripdel: remove file %s\n", s->sfile));
		sccs_close(s); /* for win32 */
		unlink(s->sfile);
		sccs_rmEmptyDirs(s->sfile);
		return (0);
	}

	if (sccs_stripdel(s, "stripdel")) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr,
		    "stripdel of %s failed.\n", s->sfile);
		}
		return (1); /* failed */
	}
	verbose((stderr, "stripdel: removed %d deltas from %s\n", n, s->gfile));
	/*
	 * Handle checkout modes
	 */
	if (getFlags && !CSET(s)) {
		sccs	*s2 = sccs_init(s->sfile, 0);
		sccs_get(s2, 0, 0, 0, 0, SILENT|getFlags, "-");
		sccs_free(s2);
	}
	return (0);
}

int
do_check(sccs *s, int flags)
{
	int	f = ADMIN_BK|ADMIN_FORMAT|ADMIN_GONE|flags;
	int	error;

	error = sccs_admin(s, 0, f, 0, 0, 0, 0, 0, 0, 0, 0);
	return(error ? 1 : 0);
}

private int
noparent(delta *e)
{
	delta	*f;

	for (f = e->parent; f->type != 'D'; f = f->parent);
	return (f->flags & D_SET);
}

/*
 * Start at the bottom, recurs to the top, and if the top tag is marked,
 * return that info so the one below it can be marked.
 */
private int
marktags(sccs *s, delta *d)
{
	assert(d);

	/* note that the tagwalk path will use D_BLUE */
	if (d->flags & D_RED) return (d->flags & D_GONE);
	d->flags |= D_RED;

	if (d->ptag && marktags(s, sfind(s, d->ptag))) {
		d->flags |= D_SET|D_GONE;
		s->hasgone = 1;
	}
	if (d->mtag && marktags(s, sfind(s, d->mtag))) {
		d->flags |= D_SET|D_GONE;
		s->hasgone = 1;
	}
	return (d->flags & D_GONE);
}

/*
 * Find ourselves a new leaf.
 */
private void
newleaf(sccs *s)
{
	delta	*d;

	for (d = s->table; d; d = d->next) {
		if (d->symGraph && !(d->flags & D_GONE)) {
			d->symLeaf = 1;
			break;
		}
	}
}

int
stripdel_markSet(sccs *s, delta *d)
{
	delta	*e;

	for (e = s->table; e; e = e->next) {
		unless (e->type == 'D') continue;
		if ((e == d) || (e->flags & D_RED)) {
			if (e->parent) e->parent->flags |= D_RED;
			if (e->merge) sfind(s, e->merge)->flags |= D_RED;
			e->flags &= ~D_RED;
		} else {
			e->flags |= D_SET;
		}
	}
	return (0);
}

int
stripdel_setMeta(sccs *s, int stripBranches, int *count)
{
	int	i, n, left;
	int	redo_merge = 0;
	delta	*e, *leaf = 0;
	char	**tips = 0;

	/* Use D_RED to figure out tag graph tips.  Leaves D_RED cleared */
	for (n = left = 0, e = s->table; e; e = e->next) {
		if (e->symLeaf) leaf = e;
		if (e->symGraph) {
			/*
			 * build up a list of tips
			 * XXX: while multi tips is not desired,
			 * they do exist in the wild, so need to handle it
			 */
			unless (e->flags & D_RED) tips = addLine(tips, e);
			if (e->ptag) sfind(s, e->ptag)->flags |= D_RED;
			if (e->mtag) sfind(s, e->mtag)->flags |= D_RED;

			e->flags &= ~D_RED;	/* done with it, so reset */
		}

		if (stripBranches && e->r[2]) e->flags |= D_SET;

		/* Mark metas if their true parent is marked. */
		if (e->type != 'D') {
			if (noparent(e)) e->flags |= D_SET;
			unless (e->flags & D_SET) continue;
		}
		if (e->flags & D_SET) {
			n++;
			MK_GONE(s, e);
			if (e->merge) {
				sfind(s, e->merge)->flags &= ~D_MERGED;
				redo_merge = 1;
			}
			continue;
		}
		left++;
	}

	/* strip out tags whose ancestry tags a removed delta */
	EACH(tips) marktags(s, (delta *)tips[i]);
	if (leaf && (leaf->flags & D_GONE)) newleaf(s);

	/* Rebuild merge image:
	 *   The D_MERGE tag means a delta is the parent of a merge
	 *   relationship.  A delta can be a parent of more than one
	 *   merge relationship.  Clearing D_MERGED clears it for all
	 *   while all may not truly be cleared.  The following puts
	 *   back MERGE pointers that are still present.
	 */
	if (redo_merge) {
		for (e = s->table; e; e = e->next) {
			if ((e->flags & D_GONE) || !e->merge)  continue;
			sfind(s, e->merge)->flags |= D_MERGED;
		}
	}
	*count = n;
	if (tips) freeLines(tips, 0);
	return left;
}

private int
strip_list(s_opts opts)
{
	char	*rev, *name;
	char	*av[2] = {"-", 0};
	sccs	*s = 0;
	delta	*d;
	int 	rc = 1;
	int	iflags = opts.iflags|SILENT;

	for (name = sfileFirst("stripdel", av, 0);
	    name; name = sfileNext()) {
		if (!s || !streq(s->sfile, name)) {
			if (s && doit(s, opts)) goto fail;
			if (s) sccs_free(s);
			s = sccs_init(name, iflags);
			unless (s && HASGRAPH(s)) {
				fprintf(stderr,
					    "stripdel: can't init %s\n", name);
				goto fail;
			}
		}
		rev = sfileRev(); assert(rev);
		d = sccs_findrev(s, rev); 
		unless (d) {
			fprintf(stderr,
			    "stripdel: can't find rev %s in %s\n", rev, name);
			goto fail;
		}
		d->flags |= D_SET;
	}
	if (s && doit(s, opts)) goto fail;
	rc = 0;
fail:	if (s) sccs_free(s);
	sfileDone();
	return (rc);
}

private	delta	*
checkCset(sccs *s)
{
	delta	*d, *e;

	for (d = s->table; d; d = d->next) {
		unless (d->flags & D_SET) continue;
		for (e = d; e; e = e->kid) {
			if (e->flags & D_CSET) {
				return (e);
			}
		}
	}
	return (0);
}
