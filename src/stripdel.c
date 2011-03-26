/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "progress.h"

typedef struct {
	u32	respectCset:1;
	u32	checkOnly:1;
	u32	quiet:1;
	u32	forward:1;
	u32	sawRev:1;
	u32	iflags;
	u32	nfiles;
	ticker	*tick;
} s_opts;

private	delta	*checkCset(sccs *s);
private	int	doit(sccs *, s_opts);
private	int	do_check(sccs *s, int flags);
private	int	strip_list(s_opts);

int
stripdel_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	c, rc;
	s_opts	opts = {1, 0, 0, 0, 0};
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "bcCN;dqr;", 0)) != -1) {
		switch (c) {
		    case 'b':
			fprintf(stderr, "ERROR: stripdel -b obsolete\n");
			usage();
		    case 'c': opts.checkOnly = 1; break;	/* doc 2.0 */
		    case 'C': opts.respectCset = 0; break;	/* doc 2.0 */
		    case 'd':
			opts.forward = 1;
			opts.iflags = INIT_WACKGRAPH;
			break;
		    case 'N': opts.quiet = 1; opts.nfiles = atoi(optarg); break;
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r':
			opts.sawRev = 1;
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		if (opts.sawRev) usage();
		rc = strip_list(opts);
		goto done;
	}
	unless (rargs.rstart) {
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

	unless ((s = sccs_init(name, opts.iflags)) && HASGRAPH(s)) {
		fprintf(stderr, "stripdel: can't init %s\n", name);
		return (1);
	}

	if (range_process("stripdel", s, RANGE_SET, &rargs)) {
		sccs_free(s);
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
	char	*p;
	int	left, n;
	int	flags = 0;

	if (opts.quiet) flags |= SILENT;

	if (HAS_PFILE(s)) {
		if (!HAS_GFILE(s) || sccs_hasDiffs(s, SILENT, 1)) {
			fprintf(stderr, 
			    "stripdel: can't strip an edited file.\n");
			return (1);
		}
	}

	if (MONOTONIC(s) && !opts.forward) {
		verbose((stderr, 
		    "Not stripping deltas from MONOTONIC file %s\n", s->gfile));
		for (e = s->table; e; e = NEXT(e)) {
			if ((e->flags & D_SET) && (e->type == 'D')) {
				e->dangling = 1;
			}
		}
		sccs_newchksum(s);
		return (0);
	}

	if (opts.respectCset && (e = checkCset(s))) {
		/* NOTE: if you change msg, also change SDMSG in undo.c */
		fprintf(stderr,
    			"stripdel: can't remove committed delta %s@%s\n",
		    s->gfile, REV(s, e));
		return (1);
	}
	range_markMeta(s);
	left = stripdel_fixTable(s, &n);
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
		/* remove d.file - from [l]clone - so rmEmptyDir works*/
		p = strrchr(s->sfile, '/');
		p[1] = 'c';	/* unlink c.file */
		unlink(s->sfile);
		p[1] = 'd';	/* unlink d.file */
		unlink(s->sfile);
		p[1] = 's';
		sccs_rmEmptyDirs(s->sfile);
		return (0);
	}

	/* work with bluearc Cunning Plan */
	s->tree->flags &= ~(D_SET|D_GONE);

	if (sccs_stripdel(s, "stripdel")) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr,
			    "stripdel of %s failed.\n", s->sfile);
		}
		return (1); /* failed */
	}
	/* XXX unlink dfile if all pending deltas are gone */
	verbose((stderr, "stripdel: removed %d deltas from %s\n", n, s->gfile));

	return (0);
}

/*
 * glue various interfaces together at this point in the call:
 * marked as D_SET and D_GONE coming into this.
 * Count the unmarked and symLeaf becomes newest not goned
 */
int
stripdel_fixTable(sccs *s, int *pcnt)
{
	delta	*d;
	int	leafset = 0;
	int	count = 0, left = 0, run_names = 0;

	for (d = s->table; d; d = NEXT(d)) {
		if (d->flags & D_SET) {
			MK_GONE(s, d);
			d->symLeaf = 0;
			unless (d->flags & D_DUPPATH) run_names++;
			count++;
			continue;
		}
		left++;
		if (!leafset && d->symGraph) {
			leafset = 1;
			d->symLeaf = 1;
		}
	}
	if (pcnt) *pcnt = count;
	if (run_names && !exists("BitKeeper/tmp/run_names")) {
		close(creat("BitKeeper/tmp/run_names", 0666));
	}
	return (left);
}

int
do_check(sccs *s, int flags)
{
	int	f = ADMIN_BK|ADMIN_FORMAT|ADMIN_GONE|flags;
	int	error;

	error = sccs_admin(s, 0, f, 0, 0, 0, 0, 0, 0, 0);
	return(error ? 1 : 0);
}

private int
strip_list(s_opts opts)
{
	char	*rev, *name;
	char	*av[2] = {"-", 0};
	sccs	*s = 0;
	delta	*d;
	int 	n = 0, rc = 1;
	int	iflags = opts.iflags|SILENT;

	if (opts.nfiles) {
		progress_delayStderr();
		opts.tick = progress_start(PROGRESS_BAR, opts.nfiles);
	}
	for (name = sfileFirst("stripdel", av, 0);
	    name; name = sfileNext()) {
		if (opts.tick) progress(opts.tick, ++n);
		if (!s || !streq(s->sfile, name)) {
			if (s && doit(s, opts)) goto fail;
			if (s) sccs_free(s);
			s = sccs_init(name, iflags);
			s->rstart = s->rstop = 0;
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
		s->state |= S_SET;
		if (!s->rstop || (s->rstop->serial < d->serial)) {
			s->rstop = d;
		}
		if (!s->rstart || (s->rstart->serial > d->serial)) {
			s->rstart = d;
		}
	}
	if (s && doit(s, opts)) goto fail;
	rc = 0;
fail:	if (s) sccs_free(s);
	sfileDone();
	if (opts.tick) {
		progress_done(opts.tick, rc ? "FAILED" : "OK");
		progress_restoreStderr();
	}
	return (rc);
}

private	delta	*
checkCset(sccs *s)
{
	delta	*d, *e;

	for (d = s->table; d; d = NEXT(d)) {
		unless (d->flags & D_SET) continue;
		for (e = d; e; e = KID(e)) {
			if (e->flags & D_CSET) {
				return (e);
			}
		}
	}
	return (0);
}
