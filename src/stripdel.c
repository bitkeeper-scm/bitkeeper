/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

typedef struct {
	u32	respectCset:1;
	u32	stripBranches:1;
	u32	checkOnly:1;
	u32	quiet:1;
} s_opts;

private	char	*stripdel_help = "\n\
usage: stripdel [-bcq] -r<rev> filename\n\n\
    -b		strip all branch deltas\n\
    -c		checks if the specified rev[s] can be stripped\n\
    -C		do not respect cset boundries\n\
    -q		run quietly\n\
    -r<rev>	set of revisions to be removed\n\n";

private	delta	*checkCset(sccs *s);
private int set_meta(sccs *s, int stripBranches, int *count);
private int doit(sccs *, s_opts);
private	int do_check(sccs *s, int flags);
private int strip_list(s_opts);

int
stripdel_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	c, rc;
	s_opts	opts = {1, 0, 0, 0};
	RANGE_DECL;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help stripdel");
		return (0);
	}
	while ((c = getopt(ac, av, "bcCqr;")) != -1) {
		switch (c) {
		    case 'b': opts.stripBranches = 1; break;	/* doc 2.0 */
		    case 'c': opts.checkOnly = 1; break;	/* doc 2.0 */
		    case 'C': opts.respectCset = 0; break;	/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;	/* doc 2.0 */
		    RANGE_OPTS('!', 'r');	/* doc 2.0 */
		    default:
usage:			system("bk help -s stripdel");
			return (1);
		}
	}
	if (av[optind] && streq(av[optind], "-")) return (strip_list(opts));

	unless (opts.stripBranches || (things && r[0])) {
		fprintf(stderr, "stripdel: must specify revisions.\n");
		return (1);
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

	unless ((s = sccs_init(name, 0, 0)) && s->tree) {
		fprintf(stderr, "stripdel: can't init %s\n", name);
		return (1);
	}

	unless (opts.stripBranches) RANGE("stripdel", s, 2, 1);
	rc = doit(s, opts);
	sccs_free(s);
	sfileDone();
	return (rc);
next:	return (1);
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
		fprintf(stderr, "stripdel: can't strip an edited file.\n");
		return (1);
	}

	if (opts.respectCset && (e = checkCset(s))) {
		fprintf(stderr,
    			"stripdel: can't remove committed delta %s@%s\n",
		    s->gfile, e->rev);
		return (1);
	}

	left = set_meta(s, opts.stripBranches, &n); 
	if (opts.checkOnly) return(do_check(s, flags));
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
	return 0;
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
	if (d->ptag && marktags(s, sfind(s, d->ptag))) d->flags |= D_SET|D_GONE;
	if (d->mtag && marktags(s, sfind(s, d->mtag))) d->flags |= D_SET|D_GONE;
	return (d->flags & D_GONE);
}

/*
 * Find ourselves a new leaf.
 */
private int
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
set_meta(sccs *s, int stripBranches, int *count)
{
	int	n, left;
	int	redo_merge = 0;
	delta	 *e, *leaf = 0;

	for (n = left = 0, e = s->table; e; e = e->next) {
		if (e->symLeaf) leaf = e;

		if (stripBranches && e->r[2]) e->flags |= D_SET;

		/* Mark metas if their true parent is marked. */
		if (e->type != 'D') {

			/* if either of these is marked, then this one is too */
			if (e->ptag && e->mtag &&
			    (noparent(sfind(s, e->ptag)) ||
			    noparent(sfind(s, e->mtag)))) {
			    	e->flags |= D_SET;
			}
			if (noparent(e)) e->flags |= D_SET;
			unless (e->flags & D_SET) continue;
		}
		if (e->flags & D_SET) {
			n++;
			e->flags |= D_GONE;
			if (e->merge) {
				sfind(s, e->merge)->flags &= ~D_MERGED;
				redo_merge = 1;
			}
			continue;
		}
		left++;
	}

	if (leaf) {
		marktags(s, leaf);
		if (leaf->flags & D_GONE) newleaf(s);
	}

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
	return left;
}

private int
strip_list(s_opts opts)
{
	char	*rev, *name;
	char	*av[2] = {"-", 0};
	sccs	*s = 0;
	delta	*d;
	project *proj = 0;
	int 	rc = 1;

	for (name = sfileFirst("stripdel", av, SF_HASREVS);
	    name; name = sfileNext()) {
		if (!s || !streq(s->sfile, name)) {
			if (s && doit(s, opts)) goto fail;
			if (s) sccs_free(s);
			s = sccs_init(name, SILENT|INIT_SAVEPROJ, proj);
			unless (s && s->tree) {
				fprintf(stderr,
					    "stripdel: can't init %s\n", name);
				goto fail;
			}
			unless (proj) proj = s->proj;
		}
		rev = sfileRev(); assert(rev);
		d = findrev(s, rev); 
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
	if (proj) proj_free(proj);
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
