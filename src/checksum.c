#include "system.h"
#include "sccs.h"
#include "logging.h"

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
	int	doit = 0;
	char	*name;
	int	fix = 0, diags = 0, bad = 0, do_sccs = 0, ret = 0, spin = 0;
	int	c;
	char	*off = 0;
	char	*rev = 0;

	while ((c = getopt(ac, av, "cfr;s|v/")) != -1) {
		switch (c) {
		    case 'c': break;	/* obsolete */
		    case 'f': fix = 1; break;			/* doc 2.0 */
		    case 'r': rev = optarg; break;
		    case 's': do_sccs = 1; off = optarg; break;
		    case '/': spin = 1; break;
		    case 'v': diags++; break;			/* doc 2.0 */
		    default:  system("bk help -s checksum");
			      return (1);
		}
	}

	if (do_sccs) return (chksum_sccs(&av[optind], off));

	if (fix ? repository_wrlock() : repository_rdlock()) {
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
				for (d = s->table; d; d = d->next) {
					unless (d->type == 'D') continue;
					c = sccs_resum(s, d, diags, fix);
					if (c & 1) doit++;
					if (c & 2) bad++;
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
			if (sccs_newchksum(s)) {
			    	unless (ret) ret = 2;
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "admin -z of %s failed.\n",
					    s->sfile);
				}
			}
		}
		if (bad && !ret) ret = 1;
		sccs_free(s);
	}
	if (sfileDone() && !ret) ret = 1;
	fix ? repository_wrunlock(0) : repository_rdunlock(0);
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
	 * If there is no content change, then if no checksum, cons one up
	 * from the data in the delta table.
	 * NOTE: check using newly computed added and deleted (in *s)
	 */
	unless (s->added || s->deleted || d->include || d->exclude) {
		int	new = 0;

		if (d->flags & D_CKSUM) return (err);
		new = adler32(new, d->sdate, strlen(d->sdate));
		new = adler32(new, d->user, strlen(d->user));
		if (d->pathname) {
			new = adler32(new, d->pathname, strlen(d->pathname));
		}
		if (d->hostname) {
			new = adler32(new, d->hostname, strlen(d->hostname));
		}
		EACH_COMMENT(s, d) {
			new = adler32(new,
			    d->cmnts[i], strlen(d->cmnts[i]));
		}
		unless (fix) {
			fprintf(stderr, "%s:%s actual=<none> sum=%d\n",
			    s->gfile, d->rev, new);
			return (2);
		}
		if (diags > 1) {
			fprintf(stderr, "Derived %s:%s -> %d\n",
			    s->sfile, d->rev, (sum_t)new);
		}
		d->sum = (sum_t)new;
		d->flags |= D_CKSUM;
		return (1);
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
	int	num, size;
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

/* same semantics as sccs_resum() except one call for all deltas */
int
cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch)
{
	MDBM	*root2map = mdbm_mem();
	ser_t	ins_ser = 0;
	char	*p, *q;
	u8	*e;
	u16	sum;
	int	cnt, i, added, n = 0;
	serset	**map;
	ser_t	*slist;
	struct	_sse *sse;
	delta	*d;
	int	found = 0;
	char	*spin = "|/-\\";
	kvpair	kv;

	mdbm_set_alignment(root2map,
	    (sizeof(void *) == 8) ? _MDBM_ALGN64 : _MDBM_ALGN32);

	if (spinners) {
		if (takepatch) {
			fprintf(stderr, "checking checksums ");
		} else {
			fprintf(stderr, "%s", fix ? "Fixing" : "Checking");
			fprintf(stderr, " ChangeSet checksums ");
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
	/* the above is very fast, no need to optimize further */

	/* foreach delta */
	slist = 0;
	for (d = s->table; d; d->flags &= ~D_SET, d = d->next) {
		unless (d->type == 'D') continue;
		unless (d->added || d->include || d->exclude) continue;
		if (SET(s) && !(d->flags & D_SET)) continue;

		/* Is this serialmap a simple extension of the last one? */
		if (slist && (slist[0] == d->serial+1)) {
			slist[0] = d->serial;
			slist[d->serial+1] = 0;
		} else {
			if (slist) free(slist);
			slist = sccs_set(s, d, 0, 0); /* slow */
		}
		if (spinners && (((++n) & 0xf) == 0)) {
			fprintf(stderr, "%c\b", spin[(n>>4) & 0x3]);
		}
		sum = 0;
		added = 0;
		for (i = 0; i < cnt; i++) {
			ser_t	ser;

			sse = map[i]->data;
			ser = sse->ser;
			while (ser) {
				if ((ser <= d->serial) && slist[ser]) {
					sum += sse->sum;
					if (ser == d->serial) ++added;
					break;
				}
				++sse;
				ser = sse->ser;
			}
		}
		/* save serialmap if parent is easy to compute from it */
		if (d->merge || d->include || d->exclude ||
		    (d->pserial+1 != d->serial)) {
			free(slist);
			slist = 0;
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
				d->sum = sum;
				d->flags |= D_CKSUM;
			}
			++found;
		}
	}
	if (slist) free(slist);
	s->state &= ~S_SET;	/* if set, then done with it: clean up */
	for (i = 0; i < cnt; i++) free(map[i]);
	free(map);
	mdbm_close(root2map);
	return (found);
}
