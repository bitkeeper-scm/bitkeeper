/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"
WHATSTR("@(#)%K%");

private	project	*proj = 0;
private int sccs2bk(sccs *s, char *csetkey);
private void branchfudge(sccs *s);
private void regen(sccs *s, char *key);
private int verify = 1;
private	int mkinit(sccs *s, delta *d, char *file, char *key);

/*
 * Convert an SCCS (including Sun Teamware) file
 */
int
sccs2bk_main(int ac, char **av)
{
	sccs	*s;
	int	c, errors = 0;
	char	*csetkey = 0;
	char	*name;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		system("bk help sccs2bk");
		return (1);
	}

	while ((c = getopt(ac, av, "c|h")) != -1) {
		switch (c) {
		    case 'c': csetkey = optarg; break;
		    case 'h': verify = 0; break;
		    default: goto usage;
		}
	}

	unless (csetkey) goto usage;

	for (name = sfileFirst("sccs2bk", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_SAVEPROJ, proj)) continue;
		unless (proj) proj = s->proj;
		unless (HASGRAPH(s)) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		fprintf(stderr, "Converting %-65s\r", s->gfile);
		errors |= sccs2bk(s, csetkey);
		s = 0;	/* freed by sccs2bk */
	}
	sfileDone();
	if (proj) proj_free(proj);
	fprintf(stderr, "\n");
	return (errors);
}

/*
 * sccs2bk to BK.
 * 1. Reorder the date timestamps such that we will keep the same graph.
 * 2. Issue a series of shell command which rebuilds the file.
 */
private int
sccs2bk(sccs *s, char *csetkey)
{
	delta	*d;
	int	i;

	for (d = s->table; d; d = d->next) {
		unless (d->include) continue;
		assert(!d->merge);
		EACH(d->include);
		assert(!d->include[i]);
		assert(d->include[i-1]);
		d->merge = d->include[i-1];
	}

	/*
	 * Strip the dangling branches by coloring the graph and
	 * then losing anything which is not marked.
	 */
	sccs_color(s, sccs_top(s));
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_RED) continue;
		d->flags |= D_SET|D_GONE;
	}

	/*
	 * 3par had some BitKeeper files that Teamware had munged,
	 * strip the root if that's the case.
	 */
	if (streq(s->tree->rev, "1.0")) {
		d = s->tree;
		EACH(d->comments) {
			if (strneq("BitKeeper file", d->comments[i], 14)) {
				d->flags |= D_SET|D_GONE;
				break;
			}
		}
		if (d->flags & D_GONE) {
			fprintf(stderr,
			    "Stripping old BitKeeper data in %s\n", s->gfile);
		}
	}

	/*
	 * Go fudge the trunk timestamps so that they are earlier than
	 * the branches.
	 */
	branchfudge(s);

	/*
	 * Go spit out the commands to regen this
	 */
	regen(s, csetkey);

	return (0);
}

private int
old2new(const void *a, const void *b)
{
	return ((*(delta**)a)->date - (*(delta**)b)->date);
}

char	*
rev(MDBM *revs, char *r)
{
	char	*map;

	if (map = mdbm_fetch_str(revs, r)) return (map);
	return (r);
}

private void
regen(sccs *s, char *key)
{
	delta	*d;
	delta	**table = malloc(s->nextserial * sizeof(delta*));
	int	n = 0, pid = getpid();
	int	i;
	char	*sfile = s->sfile;
	char	*gfile = s->gfile;
	char	*tmp;
	char	*a1, *a2, *a3;
	MDBM	*revs = mdbm_mem();
	pfile	pf;

	for (d = s->table; d; d = d->next) {
		if (!(d->flags & D_GONE) && (d->type == 'D')) {
			table[n++] = d;
		}
	}
	qsort(table, n, sizeof(delta*), old2new);
	sccs_close(s);
	tmp = strdup(sfile);
	a1 = strrchr(tmp, '/');
	a1[3] = ',';
	a1[4] = 0;
	rename(sfile, tmp);
	close(creat(gfile, 0664));
	if (mkinit(s, s->tree, tmp, key)) {
		sys("bk", "delta",
		    "-q", "-Ebinary", "-RiISCCS/.init", gfile, SYS);
	} else {
		sys("bk", "delta", "-q", "-RiISCCS/.init", gfile, SYS);
	}
	for (i = 0; i < n; ++i) {
		d = table[i];
		mkinit(s, d, 0, 0);
		if (d->include) {
			delta	*inc = sfind(s, d->include[1]);

			assert(inc);
			assert(d->include[2] == 0);
			a1 = aprintf("-egr%s", rev(revs, d->parent->rev));
			a2 = aprintf("-M%s", rev(revs, inc->rev));
			sys("bk", "_get", "-q", a1, a2, gfile, SYS);
			free(a1);
			free(a2);
		} else {
			a1 = aprintf("-egr%s",
				i ? rev(revs, d->parent->rev) : "1.0");
			sys("bk", "_get", "-q", a1, gfile, SYS);
			free(a1);
		}
		if (sccs_read_pfile("sccs2bk", s, &pf)) exit(1);
		unless (streq(d->rev, pf.newrev)) {
			mdbm_store_str(revs, d->rev, pf.newrev, 0);
			unless (d->r[2]) {
				fprintf(stderr,
				    "MAP %s@%s -> %s\n",
				    gfile, d->rev, pf.newrev);
			}
		}
		free_pfile(&pf);
		a1 = aprintf("-kpr%s", d->rev);
		sysio(0, gfile, 0, "bk", "get", "-q", a1, tmp, SYS);
		free(a1);
		sys("bk", "delta", "-q", "-RISCCS/.init", gfile, SYS);
	}

	/*
	 * Now that we have the entries in the right order, go fix up the dates.
	 */
	sys("bk", "admin", "-u", gfile, SYS);

	unless (1 || verify) goto out;

	fprintf(stderr, "Verifying %-66s\r", gfile);
	for (i = 0; i < n; ++i) {
		d = table[i];
		a1 = aprintf("-kpr%s", rev(revs, d->rev));
		a2 = aprintf("/tmp/A%d", pid);
		sysio(0, a2, 0, "bk", "get", "-q", a1, gfile, SYS);

		free(a1);
		a1 = aprintf("-kpr%s", d->rev);
		a3 = aprintf("/tmp/B%d", pid);
		sysio(0, a3, 0, "bk", "get", "-q", a1, tmp, SYS);
		unless (sameFiles(a2, a3)) {
			fprintf(stderr, "%s@%s != orig@%s\n\n",
			    gfile, rev(revs, d->rev), d->rev);
			a1 = aprintf("bk diff /tmp/A%d /tmp/B%d", pid, pid);
			system(a1);
			//exit(1);
		}
		//fprintf(stderr, "%s@%s OK\n", gfile, rev(revs, d->rev));
		unlink(a2);
		unlink(a3);
		free(a1);
		free(a2);
		free(a3);
	}
out:	unlink(tmp);
	unlink("SCCS/.init");
	free(table);
	mdbm_close(revs);
	if (do_checkout(s)) exit(1);
	sccs_free(s);
}

/*
 * Teamware files do not converge based on dates.
 * When I wrote smoosh, the destination was the side that moved onto
 * a branch.
 * Since what they are importing is likely to be their "main" tree, they
 * will want to preserve their branch structure.
 *
 * I have to be later than my parent, my merge parent, and my earlier siblings.
 */
private void
fudge(sccs *s, delta *d)
{
	unless (d) return;
	/*
	 * We fudge GONE/removed deltas anyway because they are in the list
	 * and if we skip them it screws up other processing.
	 */
	if (d->parent) {
		time_t	latest = d->parent->date;

		if (d->merge) {
			delta	*mparent = sfind(s, d->merge);

			if (mparent->date >= latest) {
				latest = mparent->date;
			}
		}
		if (d->parent->kid != d) {
			delta	*e, *last = 0;

			for (e = d->parent->kid; e != d; e = e->siblings) {
				if ((e->type == 'D') && !(e->flags & D_GONE)) {
					last = e;
				}
			}
			assert(last);
			if (last->date >= latest) {
				latest = last->date;
			}
		}
		if (latest >= d->date) {
			int	f = (latest - d->date) + 1;

			d->dateFudge += f;
			d->date += f;
		}
	}
	fudge(s, d->siblings);
	fudge(s, d->kid);
}

private void
branchfudge(sccs *s)
{
	/* Teamware has time_t's of 0. */
	if (streq(s->tree->rev, "1.1")) {
		s->tree->dateFudge++;
		s->tree->date++;
	}
	fudge(s, s->tree);
}

/*
 * From Wayne.
 */
private int
mkinit(sccs *s, delta *d, char *file, char *key)
{
	char	randstr[17];
	int	chksum = 0;
	FILE	*fh;
	char	*p;
	int	size;
	char	buf[4096];
	u32	randbits = 0;
	int	i;
	int	binary = 0;

	if (file) {
		p = aprintf("bk get -qkpr1.1 %s", file);
		fh = popen(p, "r");
		while (size = fread(buf, 1, sizeof(buf), fh)) {
			randbits = adler32(randbits, buf, size);
			for (i = 0; i < size; i++) {
				unless (buf[i]) {
					binary = 1;
					break;
				}
			}
		}
		pclose(fh);
		sprintf(randstr, "%08x", randbits);
		chksum = randbits & 0xffff;
	}
	fh = fopen("SCCS/.init", "w");
	if (file) {
		struct	tm *tp = utc2tm(d->date - 1);

		assert(key);
		fprintf(fh, "D 1.0 %02d/%02d/%02d %02d:%02d:%02d %s@%s\n",
		    tp->tm_year % 100,
		    tp->tm_mon + 1,
		    tp->tm_mday,
		    tp->tm_hour,
		    tp->tm_min,
		    tp->tm_sec,
		    d->user,
		    d->hostname ? d->hostname : sccs_gethost());
		fprintf(fh,
			"B %s\n"
			"c sccs2bk\n"
			"K %05u\n"
			"O %o\n"
			"P %s\n"
			"R %.8s\n",
			key,
			chksum,
			0664,
			s->gfile,
			randstr);
	} else {
		fprintf(fh, "D %s %s %s@%s\n",
		    d->rev, d->sdate, d->user,
		    d->hostname ? d->hostname : sccs_gethost());
		EACH(d->comments) {
			fprintf(fh, "c %s\n", d->comments[i]);
		}
		if (d->dateFudge) fprintf(fh, "F %lu\n", d->dateFudge);
	}
	fprintf(fh, "------------------------------------------------\n");
	fclose(fh);
	return (binary);
}
