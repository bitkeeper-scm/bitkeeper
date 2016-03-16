/*
 * Copyright 1999-2016 BitMover, Inc
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
#include "progress.h"
#include "graph.h"
#include "range.h"
#include "cfg.h"

private	int	do_chksum(int fd, int off, int *sump);
private	int	chksum_sccs(char **files, char *offset);
private	int	do_file(char *file, int off);
private	u32	badCutoff(sccs *s);
private	void	dumpMaxfail(sccs *s, ser_t old, ser_t new, int diffcount);

/*
 * checksum - check and/or regenerate the checksums associated with a file.
 *
 * Copyright (c) 1999-2001 L.W.McVoy
 */
int
checksum_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
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

	cmdlog_lock(fix ? CMD_WRLOCK : CMD_RDLOCK);
	for (name = sfileFirst("checksum", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, 0);
		unless (s) continue;
		unless (HASGRAPH(s)) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			goto next;
		}
		unless (BITKEEPER(s)) {
			fprintf(stderr,
			    "%s: \"%s\" is not a BitKeeper file, ignored\n",
			    av[0], s->sfile);
			goto next;
		}
		doit = bad = 0;
		/* should this be changed to use the range code? */
		if (rev) {
			unless (d = sccs_findrev(s, rev)) {
				fprintf(stderr,
				    "%s: unable to find rev %s in %s\n",
				    av[0], rev, s->gfile);
				goto next;
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
					    TABLE(s) + 1);
				}
				for (i = 0, d = TABLE(s); d >= TREE(s); d--) {
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
			if (bk4) for (d = TABLE(s); d >= TREE(s); d--) {
				SORTSUM_SET(s, d, SUM(s, d));
			}
			if (sccs_newchksum(s)) {
			    	unless (ret) ret = 2;
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "admin -z of %s failed.\n",
					    s->sfile);
				}
			}
			unless (bk4) features_set(s->proj, FEAT_SORTKEY, 1);
		}
		if (bad && !ret) ret = 1;
next:		sccs_free(s);
	}
	if (sfileDone() && !ret) ret = 1;
	return (ret);
}

int
sccs_resum(sccs *s, ser_t d, int diags, int fix)
{
	int	err = 0;
	char	before[43];	/* 4000G/4000G/4000G will fit */
	char	after[43];
	ser_t	e;
	sum_t	want;

	T_SCCS("file=%s", s->gfile);
	unless (d) d = sccs_top(s);

	if (TAG(s, d)) {
		unless (SUM(s, d)) return (0);
		unless (fix) {
			fprintf(stderr,
			    "Bad tag checksum %d:0 in %s|=%u\n",
			    SUM(s, d), s->gfile, d);
			return (2);
		}
		if (diags > 1) {
			fprintf(stderr, "Corrected %s:=%u %u->0\n",
			    s->sfile, d, SUM(s, d));
		}
		SUM_SET(s, d, 0);
		return (0);
	}
	if (CSET(s) && (diags <= 1) && !fix) {
		u32	sum;

		e = sccs_getCksumDelta(s, d);
		want = e ? SUM(s, e) : 0;
		sum = rset_checksum(s, d, 0);
		if (sum != want) {
			fprintf(stderr,
			    "Bad checksum %05u:%05u in %s|%s\n",
			    want, sum, s->gfile, REV(s, d));
			return (2);
		}
		return (0);
	}
	if (BAM(s) && !HAS_BAMHASH(s, d)) return (0);

	if (S_ISLNK(MODE(s, d))) {
		u8	*t;
		sum_t	sum = 0;

		/*
		 * don't complain about these, old BK binaries did
		 * this see get_link()
		 */
		e = getSymlnkCksumDelta(s, d);
		if (!fix && !SUM(s, e)) return (0);

		for (t = SYMLINK(s, d); *t; sum += *t++);
		if (SUM(s, e) == sum) return (0);
		unless (fix) {
			fprintf(stderr, "Bad symlink checksum %05u:%05u "
			    "in %s|%s\n",
			    SUM(s, e), sum, s->gfile, REV(s, d));
			return (2);
		} else {
			if (diags > 1) {
				fprintf(stderr, "Corrected %s:%s %d->%d\n",
				    s->sfile, REV(s, d), SUM(s, d), sum);
			}
			SUM_SET(s, d, sum);
			return (1);
		}
	}

	/*
	 * This can rewrite added / deleted / same , so do it first
	 * then check to see if they are none zero, then check checksum
	 */

	if (sccs_get(s,
	    REV(s, d), 0, 0, 0, GET_SUM|GET_SHUTUP|SILENT, 0, 0)) {
		fprintf(stderr, "get of %s:%s failed, skipping it.\n",
		    s->gfile, REV(s, d));
		return (4);
	}

	sprintf(before, "\001s %d/%d/%d\n", ADDED(s, d), DELETED(s, d), SAME(s, d));
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
			    &before[3], &after[3], s->gfile, REV(s, d));
			err = 2;
#endif
		}
		else {
			if (diags > 1) {
				fprintf(stderr, "Corrected a/d/s "
					"%s:%s %s->%s\n",
					s->sfile, REV(s, d),
					&before[3], &after[3]);
			}
			ADDED_SET(s, d, s->added);
			DELETED_SET(s, d, s->deleted);
			SAME_SET(s, d, s->same);
			err = 1;
		}
	}

	/*
	 * If there is no content change, then it should have a random
	 * checksum which is correct by default.
	 * NOTE: check using newly computed added and deleted (in *s)
	 */
	e = sccs_getCksumDelta(s, d);
	want = e ? SUM(s, e) : 0;
	if (want == s->dsum) return (err);
	unless (fix) {
		if (d != e) {
			fprintf(stderr,
			    "Bad checksum %05u:%05u in baseline %s|%s\n",
			    SUM(s, d), s->dsum, s->gfile,
			    e ? REV(s, e) : "0");
		} else {
			fprintf(stderr,
			    "Bad checksum %05u:%05u in %s|%s\n",
			    SUM(s, d), s->dsum, s->gfile, REV(s, d));
		}
		return (2);
	}
	if (d != e) {
		fprintf(stderr,
		    "Not fixing bad checksum in baseline %s|%s\n",
		    s->gfile, e ? REV(s, e) : "0");
		return (2);
	}
	if (diags > 1) {
		fprintf(stderr, "Corrected %s:%s %d->%d\n",
		    s->sfile, REV(s, d), SUM(s, d), s->dsum);
	}
	SUM_SET(s, d, s->dsum);
	return (1);
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

private	int
setOrder(sccs *s, ser_t d, void *token)
{
	ser_t	**order = (ser_t **)token;

	if (SET(s) && !(FLAGS(s, d) & D_SET)) return (0);
	FLAGS(s, d) &= ~D_SET;

	if (TAG(s, d)) return (0);
	unless (ADDED(s, d) || HAS_CLUDES(s, d) || MERGE(s, d)) return (0);

	addArray(order, &d);
	return (0);
}

typedef	struct {
	u32	index;		/* index into rkarray */
	u16	sum;		/* sum of rootkey + " \n" */
} rkinfo;

typedef	struct {
	ser_t	ser;		/* which cset serial */
	u16	sum;		/* sum of that cset line */
} Sse;

typedef	struct {		/* data remembered per-rootkey */
	Sse	*sse;		/* list of csets which changed this file */
	ser_t	last;		/* the last index in sse match for this rk */
	ser_t	lastseen;	/* the last cset serial matched for this rk */
} rkdata;

/* same semantics as sccs_resum() except one call for all deltas */
int
cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch)
{
	hash	*root2id = hash_new(HASH_U32HASH, sizeof(u32), sizeof(rkinfo));
	rkinfo	*rkid;		/* rkid->index = root2id{rkey} */
	rkdata	*rkarray = 0;	/* rkarray[rkid->index] = per-rk-stuff */
	Sse	snew;
	u32	**csetlist = 0;	/* csetlist[d] = list{ rkid's touched by d } */
	ser_t	start;
	u32	rkoff, dkoff;
	u8	*e;
	u16	sum;
	int	bits, cnt, i, added, orderIndex;
	rkdata	*item;
	u8	*slist = 0;
	u8	*symdiff = 0;
	ser_t	*order = 0;
	ser_t	d, prev;
	int	found = 0;
	int	n = 0;
	int	verify;
	u32	cutoff;
	hash	*differs = 0;
	int	diffcount = 0;
	ser_t	dualerr = 0;
	ser_t	maxfail = 0;
	u32	index;
	ticker	*tick = 0;

	if (cutoff = cfg_int(s->proj, CFG_NOGRAPHVERIFY)) {
		verify = 1;
	} else if (verify = !cfg_bool(s->proj, CFG_NOGRAPHVERIFY)) {
		cutoff = ~0;	/* set up lazy check for hardcoded cutoff */
	}

	T_PERF("file=%s", s->gfile);

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
	cnt = 1;
	growArray(&csetlist, TABLE(s));
	while (d = cset_rdweavePair(s, 0, &rkoff, &dkoff)) {
		unless (dkoff) continue; /* last key */
		if (rkid = hash_insert(root2id,
		    &rkoff, sizeof(rkoff), 0, sizeof(*rkid))) {
			addArray(&rkarray, 0);
			rkid->index = cnt++;
			sum = 0;
			for (e = HEAP(s, rkoff); *e; e++) sum += *e;
			sum += ' ' + '\n';
			rkid->sum = sum;
		} else {
			rkid = (rkinfo *)root2id->vptr;
			sum = rkid->sum;
		}
		for (e = HEAP(s, dkoff); *e; e++) sum += *e;
		snew.ser = d;
		snew.sum = sum;
		addArray(&rkarray[rkid->index].sse, &snew);
		addArray(&csetlist[d], &rkid->index);
	}
	hash_free(root2id);
	if (sccs_rdweaveDone(s)) {
		fprintf(stderr, "checksum: failed to read cset weave\n");
		exit(1);
	}
	EACH(rkarray) {
		/* initially no files active */
		rkarray[i].last = nLines(rkarray[i].sse) + 1;
	}

	/* foreach delta in an optimized order */
	graph_kidwalk(s, setOrder, 0, &order);

	if (spinners) {
		tick = progress_start(PROGRESS_MINI, nLines(order));
	}

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	symdiff = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	prev = 0;
	n = 0;
	sum = 0;
	EACH_INDEX(order, orderIndex) {
		d = order[orderIndex];

		/* serialmap[i] = (slist[i] ^ symdiff[i]) & 1 */
		bits = graph_symdiff(s, L(d), prev, 0, symdiff, 0, -1);
		start = (d > prev) ? d : prev;
		/* closure[i] = (slist[i] ^ symdiff[i]) & 2 */
		if (verify) {
			wrdata	wr;
			ser_t	tmpd;

			walkrevs_setup(&wr, s, L(prev), L(d), WR_EITHER);
			while (tmpd = walkrevs(&wr)) {
				unless (symdiff[tmpd]) bits++;
				symdiff[tmpd] |= 2;
			}
			walkrevs_done(&wr);
		}

		if (tick) progress(tick, ++n);
		added = 0;
		for (i = start; bits; i--) {
			unless (symdiff[i]) continue;
			/* transition slist from prev to d */
			slist[i] ^= symdiff[i];
			symdiff[i] = 0;
			bits--;

			/* get list of files changed by serial 'i' */
			EACH_INDEX(csetlist[i], index) {
				ser_t	ser = 0;
				int	j;
				int	num;
				int	fid;

				fid = csetlist[i][index];
				item = &rkarray[fid];
				if (item->lastseen == d) continue;
				item->lastseen = d;

				num = nLines(item->sse);
				j = item->last;
				if (j <= num) {
					/* remove this file in 'prev' */
					sum -= item->sse[j].sum;
				}
				while ((j > 1) && (item->sse[j-1].ser <= d)) {
					--j;
				}
				for (; j <= num; ++j) {
					ser = item->sse[j].ser;
					if ((ser <= d) &&
					    (slist[ser] ^ symdiff[ser])) {
						sum += item->sse[j].sum;
						if (ser == d) ++added;
						break;
					}
				}
				item->last = j;
				unless (verify) continue;
				if (diffcount) {
					if (hash_fetchU32U32(differs, fid)) {
						hash_storeU32U32(
						    differs, fid, 0);
						--diffcount;
					}
				}
				unless ((j <= num) && 
				    ((slist[ser] ^ symdiff[ser]) != 3)) {
					continue;
				}
				/* mismatch between graph and serialmap */
				unless (differs) {
					differs = hash_new(HASH_U32HASH,
					    sizeof(u32), sizeof(u32));
				}
				/* If it was in hash, it was removed above */
				hash_storeU32U32(differs, fid, 1);
				++diffcount;
				/* if serialmap version, we're done */
				if ((slist[ser] ^ symdiff[ser]) & 1) continue;
				/* else, update serialmap version */
				sum -= item->sse[j].sum;
				if (ser == d) --added;
				for (; j <= num; ++j) {
					ser = item->sse[j].ser;
					if ((slist[ser] ^ symdiff[ser]) & 1) {
						sum += item->sse[j].sum;
						if (ser == d) ++added;
						break;
					}
				}
				item->last = j;
			}
		}
		prev = d;

		if ((ADDED(s, d) != added) || DELETED(s, d) || (SAME(s, d) != 1)) {
			/*
			 * We dont report bad counts if we are not fixing.
			 * We have not been consistant about this in the past.
			 */
			if (diags > 1) {
				fprintf(stderr, "%s a/d/s "
					"%s:%s %d/%d/%d->%d/0/1\n",
				    (fix ? "Corrected" : "Bad"),
				    s->sfile, REV(s, d),
				    ADDED(s, d), DELETED(s, d), SAME(s, d), added);
			}
			if (fix) {
				ADDED_SET(s, d, added);
				DELETED_SET(s, d, 0);
				SAME_SET(s, d, 1);
				++found;
			}
		}
		if (diffcount) {
			if (maxfail < d) maxfail = d;
			if (cutoff == ~0) cutoff = badCutoff(s);
			if (!cutoff || (DATE(s, d) > cutoff)) {
				unless (dualerr || (dualerr > d)) dualerr = d;
				unless (fix) found++;
			}
		}
		if (SUM(s, d) != sum) {
			if (!fix || (diags > 1)) {
				fprintf(stderr,
				    "%s checksum %05u:%05u in %s|%s\n",
				    (fix ? "Corrected" : "Bad"),
				    SUM(s, d), sum, s->gfile, REV(s, d));
			}
			if (fix) SUM_SET(s, d, sum);
			++found;
		}
	}
	if (differs) hash_free(differs);
	if (dualerr && !fix) dumpMaxfail(s, dualerr, maxfail, diffcount);
	if (slist) free(slist);
	if (symdiff) free(symdiff);
	if (order) free(order);
	if (tick) progress_done(tick, 0);
	if (spinners && !takepatch) fputc('\n', stderr);
	s->state &= ~S_SET;	/* if set, then done with it: clean up */
	EACH(rkarray) free(rkarray[i].sse);
	free(rkarray);
	EACH(csetlist) free(csetlist[i]);
	free(csetlist);
	T_PERF("done");
	return (found);
}

private	u32
badCutoff(sccs *s)
{
	int	i;
	u32	cutoff = 0;
	char    utcTime[32];	/* mostly 32: bk grep -i utctime'\[' */
	const struct {
		char	*utctime;
		u32	cutoff;
	} badrepo[] = {
		{"20020322184504", 1055522031}, /* customer */
		{"20040627230739", 1443746616},	/* regressions */
		{0, 0}
	};

	if (getenv("_BK_DEVELOPER")) return (0);
	sccs_utctime(s, sccs_ino(s), utcTime);
	for (i = 0; badrepo[i].utctime; i++) {
		if (streq(badrepo[i].utctime, utcTime)) {
			cutoff = badrepo[i].cutoff;
			break;
		}
	}
	return (cutoff);
}

private	void
dumpMaxfail(sccs *s, ser_t old, ser_t new, int diffcount)
{
	char    utcTime[32];

	sccs_utctime(s, new, utcTime);
	fprintf(stderr,
	    "Dual sum failure at %s in %s\n"
	    "Last rev to fail %s(%lu %s)\n",
 	    REV(s, old), s->gfile,
	    REV(s, new), DATE(s, new), utcTime);

	sccs_utctime(s, sccs_ino(s), utcTime);
	fprintf(stderr, "root time: %s\n", utcTime);
	if (diffcount) {
		fprintf(stderr, "%d files still differ\n", diffcount);
	}
}
