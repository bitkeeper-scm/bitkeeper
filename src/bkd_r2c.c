/*
 * Copyright 1999-2000,2003-2006,2008,2010-2016 BitMover, Inc
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
#include "poly.h"

private	char	**r2c(char *file, RANGE *rarg);

int
r2c_main(int ac, char **av)
{
	int	c;
	char	*file = 0;
	char	*p;
	int	rc = 1;
	int	product = 1;
	MDBM	*idDB, *goneDB;
	MDBM	*md5DB = 0;
	RANGE	rargs = {0};
	char	*sfile;
	char	**revs = 0;
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* treat comps as standalone */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "PRr;S", lopts)) != -1) {
		switch (c) {
		    case 'P':	break;			// do not doc
		    case 'R':				// do not doc
		    case 'S': product = 0; break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    default:
			usage();
			break;
		}
	}
	unless ((file = av[optind]) && !av[optind+1]) usage();
	sfile = name2sccs(file);
	if (!isreg(sfile) && isKey(file)) {
		proj_cd2root();
		idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		goneDB = loadDB(GONE, 0, DB_GONE);

		file = key2path(file, idDB, goneDB, &md5DB);
		mdbm_close(idDB);
		mdbm_close(goneDB);
		mdbm_close(md5DB);
		unless (file) goto out;
	}
	unless (revs = r2c(file, &rargs)) goto out; /* will cd to file repo */
	if (product && proj_isComponent(0)) {
		p = proj_relpath(proj_product(0), proj_root(0));
		file = aprintf("%s/ChangeSet", p);
		free(p);
		proj_cd2product();
		bzero(&rargs, sizeof(rargs));
		p = joinLines(",", revs);
		freeLines(revs, free);
		range_addArg(&rargs, p, 0);
		unless (revs = r2c(file, &rargs)) {
			FREE(p);
			FREE(file);
			goto out;
		}
		FREE(p);
		FREE(file);
	}
	printf("%s\n", joinLines(",", revs));
	rc = 0;
out:	if (revs) freeLines(revs, free);
	free(sfile);
	return (rc);
}

/*
 * walkrevs callback.
 * return < 0 if this node is not in range, but is a range termination.
 *   This will keep on looking for other nodes if any more to search.
 *   This does not cause walkrevs to return non-zero.
 * return > 0 if this node is in range, and is in the D_SET set.
 *   This will terminate walkrevs immediately, even if more in range.
 *   This will cause walkrevs to return this value.
 * return 0 to keep on looking.
 */
private	int
inCset(sccs *s, ser_t d, void *token)
{
	/* see if range termination */
	if ((p2uint(token) != d) && (FLAGS(s, d) & D_CSET)) return (-1);

	/* fast exit in walkrevs if found a tagged node in the range */
	if (FLAGS(s, d) & D_SET) return (1);
	return (0);
}

/*
 * Similar to sccs_csetBoundary(), except that the input is a list
 * done by coloring D_SET.  The output is a list of the first nodes
 * found, which could include poly, but not all the poly (as some
 * of the poly may have a D_CSET marked node in their range).
 */
private	ser_t *
csetBoundarySet(sccs *s)
{
	int	i;
	ser_t	d, e;
	ser_t	*serlist = 0;

	for (d = s->rstart; d <= TABLE(s); ++d) {
		if (TAG(s, d)) continue;

		unless ((FLAGS(s, d) & D_SET) ||
		    ((e = PARENT(s, d)) && (FLAGS(s, e) & D_RED)) ||
		    ((e = MERGE(s, d)) && (FLAGS(s, e) & D_RED))) {
			continue;
		}
		unless (FLAGS(s, d) & D_CSET) {
			FLAGS(s, d) |= D_RED;
			continue;
		}
		addArray(&serlist, &d);
	}
	/* cleanup */
	for (d = s->rstart; d <= TABLE(s); ++d) {
		if (FLAGS(s, d) & D_RED) FLAGS(s, d) &= ~D_RED;
	}
	/*
	 * Note: walkrevs needs RED and BLUE, so don't merge below into above
	 *
	 * We can get false entries in serlist (see "r2c in non-poly"
	 * in regressions).  Expand each region and remove any that don't
	 * have any D_SET in there.
	 */
	EACH_REVERSE(serlist) {
		d = serlist[i];
		unless (range_walkrevs(
			    s, 0, L(d), 0, inCset, uint2p(d))) {
			removeArrayN(serlist, i);
		}
	}
	return (serlist);
}

/*
 * Run r2c on file@rev and return the appropriate
 * rev. If run on a component ChangeSet file, it
 * will return the rev of the product's ChangeSet
 * that corresponds to this component's ChangeSet
 * rev.
 *
 * Errors are printed on err (if not null).
 */
private char **
r2c(char *file, RANGE *rarg)
{
	char	*name;
	ser_t	d, e;
	sccs	*s = 0, *cset = 0;
	char	**ret = 0, **polykeys = 0;
	ser_t	*serlist = 0;
	hash	*keys = 0;
	int	i;
	int	redo = 0;
	time_t	oldest, candidate;
	u32	rkoff, dkoff;
	char	buf[MAXKEY*2];

	name = name2sccs(file);
	unless (s = sccs_init(name, INIT_NOCKSUM|INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: cannot init %s\n", prog, file);
		goto out;
	}
	if (chdir(proj_root(s->proj))) {
		fprintf(stderr, "%s: cannot find package root.\n", prog);
		goto out;
	}
	if (range_process("r2c", s, RANGE_SET, rarg)) {
		fprintf(stderr, "%s: cannot process range for %s\n",
		    prog, file);
		goto out;
	}
	/* Get list of closest D_CSET marked (missing some poly) */
	unless (serlist = csetBoundarySet(s)) {
		fprintf(stderr,
		    "%s: cannot find cset marker for the given revisions\n",
		    prog);
		goto out;
	}

	/* Prune out poly from list, save poly product keys */
	if (CSET(s) && proj_isComponent(s->proj)) {
		/* for the csetInit() below -- get the product */
		if (proj_cd2product()) goto out;
		EACH_REVERSE(serlist) {
			unless (poly_r2c(s, serlist[i], &polykeys)) {
				removeArrayN(serlist, i);
			}
		}
	}
	/* Look for non poly nodes in prod weave; need a key hash */
	keys = hash_new(HASH_MEMHASH);
	oldest = 0;
	EACH(serlist) {
		d = serlist[i];
		candidate = DATE(s, d) - DATE_FUDGE(s, d);
		if (!oldest || (oldest > candidate)) oldest = candidate;
		sccs_sdelta(s, d, buf);
		hash_insertStrSet(keys, buf);
	}
	FREE(serlist);

	strcpy(buf, CHANGESET);
	unless (cset = sccs_init(buf, INIT_NOCKSUM)) {
		fprintf(stderr, "%s: cannot init ChangeSet\n", prog);
		goto out;
	}

	assert(serlist == 0);
	/* Stick the poly prod csets in the answer */
	EACH(polykeys) {
		d = sccs_findKey(cset, polykeys[i]);
		assert(d);
		addArray(&serlist, &d);
	}
	unless (hash_count(keys)) goto done;	/* just polykeys or nothing */

	/* Stick the non-poly prod csets in the answer */
again:	sccs_rdweaveInit(cset);
	unless (redo) { /* Perf: limit cset content read */
		char	*skewstr = getenv("BK_R2C_CLOCKSKEW");
		int	skew = skewstr ? atoi(skewstr) : HOUR;

		assert(oldest);
		if (oldest > skew) oldest -= skew; // some time skew

		d = sccs_date2delta(cset, oldest);
		while (d && TAG(cset, d)) d--;
		assert(d);
		/* tune to be real time not fudged time */
		for (e = d; e <= TABLE(cset); e++) {
			if (TAG(cset, e)) continue;
			if (oldest <=
			    (DATE(cset, e) - DATE_FUDGE(cset, e))) {
				break;
			}
			d = e;
		}
		cset_firstPairReverse(cset, d); /* old to new */
	}
	while (d = cset_rdweavePair(cset, 0, &rkoff, &dkoff)) {
		unless (dkoff) continue; /* last key */
		unless (hash_deleteStr(keys, HEAP(cset, dkoff))) {
			addArray(&serlist, &d);
			unless (hash_count(keys)) break;
		}
	}
	sccs_rdweaveDone(cset);
	if (hash_count(keys) && !redo) {
		redo = 1;
		goto again;	/* full weave this time */
	}
done:
	/* List is sorted, with dups; so filter dups here */
	sortArray(serlist, serial_sortrev);
	d = 0;
	EACH(serlist) {
		if (d == serlist[i]) continue;
		d = serlist[i];
		ret = addLine(ret, strdup(REV(cset, d)));
	}
	unless (ret) {
		fprintf(stderr,
		    "%s: cannot find any of the specified revisions\n", prog);
	}
out:	FREE(name);
	FREE(serlist);
	if (keys) hash_free(keys);
	freeLines(polykeys, free);
	sccs_free(s);
	sccs_free(cset);
	return (ret);
}
