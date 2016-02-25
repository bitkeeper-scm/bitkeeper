/*
 * Copyright 1999-2003,2005-2016 BitMover, Inc
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
	u32	stripTags:1;
	u32	iflags;
	u32	nfiles;
	ticker	*tick;
} s_opts;

private	ser_t	checkCset(sccs *s);
private	int	doit(sccs *, s_opts);
private	int	do_check(sccs *s, int flags);
private	int	strip_list(s_opts);
private	void	checkStripTags(sccs *s, s_opts);

int
stripdel_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	c, rc;
	s_opts	opts = {1, 0, 0, 0, 0};
	RANGE	rargs = {0};
	char	nullrange[] = "+..+";
	longopt	lopts[] = {
		/* should only be used by csetprune or if you newroot */
		{ "strip-tags", 300 },		/* remove tag graph */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "bcCN;dqr;", lopts)) != -1) {
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
		    case 300: /* --strip-tags */
			if (opts.sawRev) usage();
			if (range_addArg(&rargs, nullrange, 0)) usage();
			opts.stripTags = 1;
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
	ser_t	e;
	int	left, n;
	int	flags = 0;
	int	cleanflags = SILENT;

	if (opts.quiet) flags |= SILENT;
	if (opts.checkOnly) flags |= CLEAN_CHECKONLY;

	if (sccs_clean(s, cleanflags)) {
		fprintf(stderr,
		    "stripdel: can't strip an edited file: %s\n", s->gfile);
		return (1);
	}

	if (MONOTONIC(s) && !opts.forward) {
		verbose((stderr, 
		    "Not stripping deltas from MONOTONIC file %s\n", s->gfile));
		for (e = TABLE(s); e >= TREE(s); e--) {
			if ((FLAGS(s, e) & D_SET) && !TAG(s, e)) {
				FLAGS(s, e) |= D_DANGLING;
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
		/* see ya! */
		verbose((stderr, "stripdel: remove file %s\n", s->sfile));
		sccs_close(s); /* for win32 */
		sfile_delete(s->proj, s->gfile);
		sccs_rmEmptyDirs(s->sfile);
		return (0);
	}

	checkStripTags(s, opts);

	/* work with bluearc Cunning Plan */
	FLAGS(s, TREE(s)) &= ~(D_SET|D_GONE);

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
	ser_t	d;
	int	leafset = 0;
	int	count = 0, left = 0, run_names = 0;

	for (d = TABLE(s); d >= TREE(s); d--) {
		if (FLAGS(s, d) & D_SET) {
			MK_GONE(s, d);
			FLAGS(s, d) &= ~D_SYMLEAF;
			if (!PARENT(s, d) ||
			    !streq(PATHNAME(s, PARENT(s, d)), PATHNAME(s, d))){
				run_names++;
			}
			count++;
			continue;
		}
		left++;
		if (!leafset && SYMGRAPH(s, d)) {
			leafset = 1;
			FLAGS(s, d) |= D_SYMLEAF;
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

	error = sccs_adminFlag(s, f);
	return(error ? 1 : 0);
}

private int
strip_list(s_opts opts)
{
	char	*rev, *name;
	char	*av[2] = {"-", 0};
	sccs	*s = 0;
	ser_t	d;
	int 	n = 0, rc = 1;
	int	iflags = opts.iflags|SILENT|INIT_MUSTEXIST;

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
			unless (s = sccs_init(name, iflags)) {
				fprintf(stderr,
				    "stripdel: can't init %s\n", name);
				goto fail;
			}
			s->rstart = s->rstop = 0;
		}
		rev = sfileRev(); assert(rev);
		d = sccs_findrev(s, rev); 
		unless (d) {
			fprintf(stderr,
			    "stripdel: can't find rev %s in %s\n", rev, name);
			goto fail;
		}
		FLAGS(s, d) |= D_SET;
		s->state |= S_SET;
		if (!s->rstop || (s->rstop < d)) {
			s->rstop = d;
		}
		if (!s->rstart || (s->rstart > d)) {
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

private	ser_t
checkCset(sccs *s)
{
	ser_t	d, e;
	int	saw_cset = 0;

	for (d = TABLE(s); d >= TREE(s); d--) {
		if (FLAGS(s, d) & D_CSET) saw_cset = 1;
		unless (FLAGS(s, d) & D_SET) continue;
		if (saw_cset && (e = sccs_csetBoundary(s, d, 0))) {
			return (e);
		}
	}
	return (0);
}

/*
 * Long ago, delta_table() used to, on the fly, strip out TAGS from
 * files, just leaving them as skipped serials.  Now those serials
 * need to get compressed.  This moves that logic out of old delta_table
 * and into here.  That means it also works on binary delta which may or
 * may not be a good thing.
 *
 * Strip tags out of files (and out of ChangeSet if requested) by marking
 * nodes D_GONE.  And then if things marked D_GONE, scompress them out.
 * This is graph only.  No data stripped from weave (unlike sccs_stripdel).
 */
private	void
checkStripTags(sccs *s, s_opts opts)
{
	ser_t	d;

	for (d = TREE(s); d <= TABLE(s); d++) {
		assert(FLAGS(s, d));
		if (TAG(s, d) && !(FLAGS(s, d) & D_GONE)) {
			/* always strip removed tags; sometimes all tags */
			if (opts.stripTags ||
			    !(SYMGRAPH(s, d) ||
			    (FLAGS(s, d) & D_SYMBOLS) ||
			    HAS_COMMENTS(s, d))) {
				MK_GONE(s, d);
			}
		}
		unless (opts.stripTags || !CSET(s)) continue;
		if (opts.stripTags) FLAGS(s, d) &= ~D_SYMBOLS;
		FLAGS(s, d) &= ~(D_SYMGRAPH | D_SYMLEAF);
		MTAG_SET(s, d, 0);
		PTAG_SET(s, d, 0);
	}
	if (opts.stripTags) {
		/* XXX: leaves items on the heap */
		FREE(s->symlist);
	}
	return;
}
