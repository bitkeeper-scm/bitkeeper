#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "progress.h"
#include "graph.h"

private	int	do_chksum(int fd, int off, int *sump);
private	int	chksum_sccs(char **files, char *offset);
private	int	do_file(char *file, int off);

/*
 * checksum - check and/or regenerate the checksums associated with a file.
 *
 * Copyright (c) 1999-2001 L.W.McVoy
 */
int
checksum_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	int	doit = 0, bk4 = 0;
	char	*name;
	int	fix = 0, diags = 0, bad = 0, do_sccs = 0, ret = 0, spin = 0;
	int	c, i;
	char	*off = 0;
	char	*rev = 0;
	ticker	*tick = 0;

	while ((c = getopt(ac, av, "4cfpr;s|v", 0)) != -1) {
		switch (c) {
		    case '4': bk4 = 1; break;	/* obsolete */
		    case 'c': break;	/* obsolete */
		    case 'f': fix = 1; break;			/* doc 2.0 */
		    case 'r': rev = optarg; break;
		    case 's': do_sccs = 1; off = optarg; break;
		    case 'p': spin = 1; break;
		    case 'v': diags++; break;			/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	if (bk4 && diags) fprintf(stderr, "Running checksum in bk4\n");

	if (do_sccs) return (chksum_sccs(&av[optind], off));

	if (fix ? repository_wrlock(0) : repository_rdlock(0)) {
		repository_lockers(0);
		return (1);
	}
	for (name = sfileFirst("checksum", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, 0);
		unless (s) continue;
		unless (HASGRAPH(s)) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			continue;
		}
		unless (BITKEEPER(s)) {
			fprintf(stderr,
			    "%s: \"%s\" is not a BitKeeper file, ignored\n",
			    av[0], s->sfile);
			continue;
		}
		doit = bad = 0;
		/* should this be changed to use the range code? */
		if (rev) {
			unless (d = sccs_findrev(s, rev)) {
				fprintf(stderr,
				    "%s: unable to find rev %s in %s\n",
				    av[0], rev, s->gfile);
				continue;
			}
			c = sccs_resum(s, d, diags, fix);
			if (c & 1) doit++;
			if (c & 2) bad++;
		} else {
			if (CSET(s)) {
				c = cset_resum(s, diags, fix, spin, 0);
				if (fix) {
					doit = c;
				} else {
					bad = c;
				}
			} else {
				if (spin) {
					fputs(s->gfile, stderr);
					tick = progress_start(PROGRESS_MINI,
					    s->nextserial);
				}
				for (i = 0, d = s->table; d; d = NEXT(d)) {
					unless (d->type == 'D') continue;
					c = sccs_resum(s, d, diags, fix);
					if (tick) progress(tick, ++i);
					if (c & 1) doit++;
					if (c & 2) bad++;
				}
				if (tick) {
					progress_done(tick, 0);
					tick = 0;
					fputc('\n', stderr);
				}
			}
		}
		if (diags && fix) {
			fprintf(stderr,
			    "%s: fixed bad metadata in %d deltas\n",
			    s->gfile, doit);
		}
		if (diags && !fix) {
			fprintf(stderr,
			    "%s: bad metadata in %d deltas\n",
			    s->gfile, bad);
		}
		if ((doit || !s->cksumok) && fix) {
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (bk4) for (d = s->table; d; d = NEXT(d)) {
				if (d->flags & D_SORTSUM) {
					d->flags &= ~D_SORTSUM;
				}
			}
			if (sccs_newchksum(s)) {
			    	unless (ret) ret = 2;
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "admin -z of %s failed.\n",
					    s->sfile);
				}
			}
			unless (bk4) bk_featureSet(s->proj, FEAT_SORTKEY, 1);
		}
		if (bad && !ret) ret = 1;
		sccs_free(s);
	}
	if (sfileDone() && !ret) ret = 1;
	fix ? repository_wrunlock(0, 0) : repository_rdunlock(0, 0);
	return (ret);
}

int
sccs_resum(sccs *s, delta *d, int diags, int fix)
{
	int	i;
	int	err = 0;
	char	before[43];	/* 4000G/4000G/4000G will fit */
	char	after[43];

	unless (d) d = sccs_top(s);

	if (BAM(s) && !d->hash) return (0);

	if (S_ISLNK(d->mode)) {
		u8	*t;
		sum_t	sum = 0;
		delta	*e;

		/* don't complain about these, old BK binaries did this */
		e = getSymlnkCksumDelta(s, d);
		if (!fix && !e->sum) return (0);

		for (t = d->symlink; *t; sum += *t++);
		if ((e->flags & D_CKSUM) && (e->sum == sum)) return (0);
		unless (fix) {
			fprintf(stderr, "Bad symlink checksum %d:%d in %s|%s\n",
			    e->sum, sum, s->gfile, d->rev);
			return (2);
		} else {
			if (diags > 1) {
				fprintf(stderr, "Corrected %s:%s %d->%d\n",
				    s->sfile, d->rev, d->sum, sum);
			}
			unless (d->flags & D_SORTSUM) {
				d->sortSum = d->sum;
				d->flags |= D_SORTSUM;
			}
			d->sum = sum;
			d->flags |= D_CKSUM;
			return (1);
		}
	}

	/*
	 * This can rewrite added / deleted / same , so do it first
	 * then check to see if they are none zero, then check checksum
	 */

	if (sccs_get(s,
	    d->rev, 0, 0, 0, GET_SUM|GET_SHUTUP|SILENT|PRINT, "-")) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr,
			    "get of %s:%s failed, skipping it.\n",
			    s->gfile, d->rev);
		}
		return (4);
	}

	sprintf(before, "\001s %d/%d/%d\n", d->added, d->deleted, d->same);
	sprintf(after, "\001s %d/%d/%d\n", s->added, s->deleted, s->same);
	unless (streq(before, after)) {
		size_t	n;

		n = strlen(before);
		assert(n > 4 && before[n-1] == '\n');
		before[n-1] = 0;

		n = strlen(after);
		assert(n  > 4 && after[n-1] == '\n');
		after[n-1] = 0;

		unless (fix) {
#if	0
			/* XXX: t.long_line has one line counted as 4
			 * which means repositories out in the world which
			 * are working, would all of a sudden start gakking
			 */
			fprintf(stderr,
		    		"Bad a/d/s %s:%s in %s|%s\n",
		    		&before[3], &after[3], s->gfile, d->rev);
			err = 2;
#endif
		}
		else {
			if (diags > 1) {
				fprintf(stderr, "Corrected a/d/s "
					"%s:%s %s->%s\n",
		    			s->sfile, d->rev,
					&before[3], &after[3]);
			}
			d->added = s->added;
			d->deleted = s->deleted;
			d->same = s->same;
			err = 1;
		}
	}

	/*
	 * If there is no content change, then it should have a random
	 * checksum which is correct by default.
	 * NOTE: check using newly computed added and deleted (in *s)
	 */
	unless (s->added || s->deleted || d->include || d->exclude) {
		assert(d->flags & D_CKSUM);
		return (err);
	}

	if ((d->flags & D_CKSUM) && (d->sum == s->dsum)) return (err);
	unless (fix) {
		fprintf(stderr,
		    "Bad checksum %d:%d in %s|%s\n",
		    d->sum, s->dsum, s->gfile, d->rev);
		return (2);
	}
	if (diags > 1) {
		fprintf(stderr, "Corrected %s:%s %d->%d\n",
		    s->sfile, d->rev, d->sum, s->dsum);
	}
	unless (d->flags & D_SORTSUM) {
		d->sortSum = d->sum;
		d->flags |= D_SORTSUM;
	}
	d->sum = s->dsum;
	d->flags |= D_CKSUM;
	return (1);
	assert("Not reached" == 0);
}


/*
 * Calculate the same checksum as is used in BitKeeper.
 */
private int
chksum_sccs(char **files, char *offset)
{
	int	sum, i;
	int	off = 0;
	char	buf[MAXPATH];

	if (offset) off = atoi(offset);
	unless (files[0]) {
		if (do_chksum(0, off, &sum)) return (1);
		printf("%d\n", sum);
	} else if (streq("-", files[0]) && !files[1]) {
		while (fnext(buf, stdin)) {
			chop(buf);
			if (do_file(buf, off)) return (1);
		}
	} else for (i = 0; files[i]; ++i) {
		if (do_file(files[i], off)) return (1);
	}
	return (0);
}

private int
do_file(char *file, int off)
{
	int	sum, fd;

	fd = open(file, 0, 0);
	if (fd == -1) {
		perror(file);
		return (1);
	}
	if (do_chksum(fd, off, &sum)) {
		close(fd);
		return (1);
	}
	close(fd);
	printf("%-20s %d\n", file, sum);
	return (0);
}

private int
do_chksum(int fd, int off, int *sump)
{
	u8	 buf[16<<10];
	register unsigned char *p;
	register int i;
	u16	 sum = 0;

	while (off--) {
		if (read(fd, buf, 1) != 1) return (1);
	}
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf; i--; sum += *p++);
	}
	*sump = (int)sum;
	return (0);
}

typedef struct serset {
	ser_t	num, size, last;
	struct _sse {
		ser_t	ser;
		u16	sum;
	} data[0];
} serset;

#define	SS_SIZE  sizeof(serset)
#define	SSE_SIZE sizeof(struct _sse)

private void
add_ins(MDBM *h, char *root, int len, ser_t ser, u16 sum)
{
	serset	*ss, *tmp;
	datum	k, v;

	k.dptr = root;
	k.dsize = len;
	v = mdbm_fetch(h, k);
	ss = v.dptr ? *(serset **)v.dptr : 0;
	unless (ss) {
		/* new entry */
		ss = malloc(SS_SIZE + 4*SSE_SIZE);
		ss->num = 0;
		ss->size = 4;
		ss->last = 0;
		v.dptr = (void *)&ss;
		v.dsize = sizeof(serset *);
		mdbm_store(h, k, v, MDBM_INSERT);
	} else if (ss->num == (ss->size - 1)) {
		/* realloc 2X */
		tmp = malloc(SS_SIZE + (2 * ss->size)*SSE_SIZE);
		memcpy(tmp, ss, SS_SIZE + ss->size*SSE_SIZE);
		free(ss);
		ss = tmp;
		ss->size *= 2;
		v.dptr = (void *)&ss;
		v.dsize = sizeof(serset *);
		mdbm_store(h, k, v, MDBM_REPLACE);
	}
	ss->data[ss->num].ser = ser;
	ss->data[ss->num].sum = sum;
	++ss->num;
	ss->data[ss->num].ser = 0;
}

private	int
setOrder(sccs *s, delta *d, void *token)
{
	char	***order = (char ***)token;

	if (SET(s) && !(d->flags & D_SET)) return (0);
	d->flags &= ~D_SET;

	if (TAG(d)) return (0);
	unless (d->added || d->include || d->exclude) return (0);

	*order = addLine(*order, d);
	return (0);
}

private int
sortSets(const void *a, const void *b)
{
	serset *sa = *(serset **)a;
	serset *sb = *(serset **)b;
	int	na, nb;

	na = (sa->num > 0) ? sa->data[0].ser : 0;
	nb = (sb->num > 0) ? sb->data[0].ser : 0;
	return (nb - na);
}

/* same semantics as sccs_resum() except one call for all deltas */
int
cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch)
{
	MDBM	*root2map = mdbm_mem();
	ser_t	ins_ser = 0;
	char	*p, *q;
	u8	*e;
	u16	sum;
	int	cnt, i, added, orderIndex;
	serset	**map;
	u8	*slist = 0;
	char	**order = 0;
	struct	_sse *sse;
	delta	*d, *prev;
	int	found = 0;
	int	n = 0;
	kvpair	kv;
	ticker	*tick = 0;

	mdbm_set_alignment(root2map,
	    (sizeof(void *) == 8) ? _MDBM_ALGN64 : _MDBM_ALGN32);

	if (spinners) {
		if (takepatch) {
			fprintf(stderr, "checking checksums");
		} else {
			fprintf(stderr, "%s", fix ? "Fixing" : "Checking");
			fprintf(stderr, " ChangeSet checksums");
		}
	}

	/* build up weave data structure */
	sccs_rdweaveInit(s);
	while (p = sccs_nextdata(s)) {
		if (p[0] == '\001') {
			if (p[1] == 'I') ins_ser = atoi(p + 3);
		} else {
			q = separator(p);
			sum = 0;
			for (e = p; *e; e++) sum += *e;
			sum += '\n';
			add_ins(root2map, p, q-p, ins_ser, sum);
		}
	}
	if (sccs_rdweaveDone(s)) {
		fprintf(stderr, "checksum: failed to read cset weave\n");
		exit(1);
	}

	cnt = 0;
	EACH_KV(root2map) ++cnt;
	map = malloc(cnt * sizeof(serset *));
	cnt = 0;
	EACH_KV(root2map) {
		map[cnt] = *(serset **)kv.val.dptr;
		++cnt;
	}
	/* order array for better cache footprint */
	qsort(map, cnt, sizeof(serset *), sortSets);

	/* the above is very fast, no need to optimize further */

	/* foreach delta in an optimized order */
	graph_kidwalk(s, setOrder, 0, &order);

	if (spinners) {
		tick = progress_start(PROGRESS_MINI, nLines(order));
	}

	slist = (u8 *)calloc(s->nextserial, sizeof(u8));
	prev = 0;
	n = 0;
	EACH_INDEX(order, orderIndex) {
		d = (delta *)order[orderIndex];

		/* incremental serialmap */
		graph_symdiff(d, prev, slist, 0, -1, 0);
		prev = d;

		if (tick) progress(tick, ++n);
		sum = 0;
		added = 0;
		for (i = 0; i < cnt; i++) {
			ser_t	ser;
			ser_t	want = d->serial;

			sse = map[i]->data + map[i]->last;
			while ((sse > map[i]->data) && (sse[-1].ser <= want)) {
				--sse;
			}
			for (; (ser = sse->ser); ++sse) {
				if ((ser <= want) && slist[ser]) {
					sum += sse->sum;
					if (ser == want) ++added;
					break;
				}
			}
			map[i]->last = (sse - map[i]->data);
		}

		if ((d->added != added) || d->deleted || (d->same != 1)) {
			/*
			 * We dont report bad counts if we are not fixing.
			 * We have not been consistant about this in the past.
			 */
			if (diags > 1) {
				fprintf(stderr, "%s a/d/s "
					"%s:%s %d/%d/%d->%d/0/1\n",
				    (fix ? "Corrected" : "Bad"),
				    s->sfile, d->rev,
				    d->added, d->deleted, d->same, added);
			}
			if (fix) {
				d->added = added;
				d->deleted = 0;
				d->same = 1;
				++found;
			}
		}

		if (d->sum != sum) {
			if (!fix || (diags > 1)) {
				fprintf(stderr, "%s checksum %d:%d in %s|%s\n",
				    (fix ? "Corrected" : "Bad"),
				    d->sum, sum, s->gfile, d->rev);
			}
			if (fix) {
				unless (d->flags & D_SORTSUM) {
					d->sortSum = d->sum;
					d->flags |= D_SORTSUM;
				}
				d->sum = sum;
				d->flags |= D_CKSUM;
			}
			++found;
		}
	}
	if (slist) free(slist);
	if (order) free(order);
	if (tick) progress_done(tick, 0);
	if (spinners && !takepatch) fputc('\n', stderr);
	s->state &= ~S_SET;	/* if set, then done with it: clean up */
	for (i = 0; i < cnt; i++) free(map[i]);
	free(map);
	mdbm_close(root2map);
	return (found);
}
