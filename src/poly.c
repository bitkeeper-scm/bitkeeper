#include "sccs.h"
#include "range.h"
#include "poly.h"

/*
 * XXX TODO list
 * - fix range_cset() in changes.c to use new API
 * - fix 'bk r2c' to return list
 * - collapse needs to drop diffs from poly files
 * - converge.c needs to automerge poly files
 */

/*
 *
 * files:
 *   PRODUCT/BitKeeper/etc/poly/md5_sortkey
 *
 * File format:
 *   @component_delta_key
 *   product_key endkey [merge key]
 *   product_key2 endkey [merge key]
 *   @component_delta_key2
 *   product_key endkey [merge key]
 *   product_key2 endkey [merge key]
 *
 */

private	void	polyLoad(sccs *cset);
private int	polyAdd(sccs *cset, ser_t d, char *pkey, int side);
private int	polyFlush(sccs *cset);
private	int	polyChk(sccs *cset, ser_t gca);
private	int	updatePolyDB(sccs *cset, ser_t *list, hash *cmarks);
private	void	addTips(sccs *cset, ser_t *gcalist, ser_t **list);
private	ser_t	*lowerBounds(sccs *s, ser_t d, u32 flags);
private ser_t	*findPoly(sccs *s, ser_t local, ser_t remote, ser_t fake);

static	hash	*cpoly_all;	/* ckey -> array of cmarks */
static	hash	*cpoly;		/* ckey -> array of cmarks */
static	project	*cpoly_proj;
static	int	cpoly_dirty;

/*
 * When given a delta in a component cset that is csetmarked it
 * returns information about the product cset(s) that include this delta.
 *
 * Returns a lines array of pointers to cmark structs, or 0 if
 * delta is not poly.
 */
cmark *
poly_check(sccs *cset, ser_t d)
{
	cmark	*ret;
	char	key[MAXKEY];

	assert(FLAGS(cset, d) & D_CSET);

	polyLoad(cset);

	sccs_sdelta(cset, d, key);
	ret = hash_fetchStrPtr(cpoly, key);
	return (ret);
}

private	int
inrange(sccs *s, ser_t d, void *token)
{
	return (d == p2uint(token));
}

/*
 * brain dead poly hunter
 * better if we color up D_SET from orig, and uncolor any non poly D_CSET
 * at some point, coloring will be starved and we'll just zip up the
 * list.  As it is, it has to be poly to consider.
 */
char **
poly_r2c(sccs *cset, ser_t orig)
{
	cmark	*list, *cm;
	ser_t	d, e, *lower = 0;
	char	**ret = 0;

	unless (d = sccs_csetBoundary(cset, orig, 0)) return (0);
	unless (list = poly_check(cset, d)) return (0);

	/* for the first one found, we are guarenteed to be in range */
	EACHP(list, cm) ret = addLine(ret, strdup(cm->pkey));
	free(list);

	for (d++; d <= TABLE(cset); d++) {
		unless (FLAGS(cset, d) & D_CSET) continue;
		unless (list = poly_check(cset, d)) continue;
		EACHP(list, cm) {
			// is it in the range?
			// a range can be lower than endpoints
			if (cm->ekey) {
				e = sccs_findKey(cset, cm->ekey);
				addArray(&lower, &e);
			}
			if (cm->emkey) {
				e = sccs_findKey(cset, cm->emkey);
				addArray(&lower, &e);
			}
			if (range_walkrevs(
			    cset, 0, lower, d, 0, inrange, uint2p(orig))) {
				ret = addLine(ret, strdup(cm->pkey));
			}
			FREE(lower);
		}
		free(list);
	}
	return (ret);
}

void
poly_range(sccs *s, ser_t d, char *pkey)
{
	cmark	*clist, *cm;
	ser_t	e;

	assert(d);
	if (CSET(s) &&
	    proj_isComponent(s->proj) && (clist = poly_check(s, d))) {
		EACHP(clist, cm) {
			if (!pkey || streq(cm->pkey, pkey)) break;
		}
		if (cm <= &clist[nLines(clist)]) {
			ser_t	*lower = 0;

			if (cm->ekey) {
				e = sccs_findKey(s, cm->ekey);
				addArray(&lower, &e);
			}
			if (cm->emkey) {
				e = sccs_findKey(s, cm->emkey);
				addArray(&lower, &e);
			}
			range_walkrevs(s, 0, lower, d, 0,
			    walkrevs_setFlags, uint2p(D_SET));
			s->state |= S_SET;
			FREE(lower);
		} else {
			// XXX Print or not? Currently prints.
			// if not print, then how to know that
			// so changes without -e can prune?
			// 
			// fprintf(stderr,
			//     "Poly tip %s %s\n", pkey, dkey);
			s->rstart = s->rstop = 0;
		}
	} else {
		range_cset(s, d);	/* not poly, do normal */
	}
}

/*
 * Save product info at nested_init time for use in poly_pull()
 * Called in product weave order (new to old)
 */
char **
poly_save(char **list, sccs *cset, ser_t d, char *ckey, int local)
{
	char	buf[MAXKEY];

	buf[0] = (local) ? 'L' : 'R';
	buf[1] = MERGE(cset, d) ? 'M' : 'S';
	sccs_sdelta(cset, d, buf + 2);
	strcat(buf, " ");
	strcat(buf, ckey);
	return (addLine(list, strdup(buf)));
}

/*
 * Pull Poly is both detection and fixups for handling poly in a pull.
 * If the local is ahead of the remote, but the remote adds cset marks,
 * then we need to fake a RESYNC and make it pending, so the tip can
 * be included in the product tip (enhancing the polyness some more).
 * If update only pull, then don't have to fake up the RESYNC, but do
 * have to do the pending work on the tip.   In there, also do error
 * detection and bail.
 */
int
poly_pull(int got_patch, char *mergefile)
{
	sccs	*cset = 0;
	ser_t	*polylist = 0, d, fake = 0, local = 0, remote = 0;
	char	*dfile, *resync;
	int	rc = 1, write = 0;
	int	i;
	char	*ckey;
	char	**list;
	hash	*cmarks = 0;

	resync = aprintf("%s/%s", ROOT2RESYNC, CHANGESET);

	/* cons up a RESYNC area in case "Nothing to pull" */
	unless (got_patch) {
		if (mkdir(ROOT2RESYNC, 0777)) {
			perror("make poly resync");
			goto err;
		}
		sccs_mkroot(ROOT2RESYNC);
		fileLink(CHANGESET, resync);
		touch(ROOT2RESYNC "/BitKeeper/tmp/patch", 0666);
	}

	unless (cset = sccs_init(resync, INIT_MUSTEXIST)) goto err;
	assert(cset);

	list = file2Lines(0, mergefile);
	cmarks = hash_new(HASH_MEMHASH);
	EACH(list) {
		ckey = separator(list[i]);
		assert(ckey);
		*ckey++ = 0;
		d = sccs_findKey(cset, ckey);
		assert(d);
		switch (*list[i]) {
		    case 'L':
			FLAGS(cset, d) |= D_LOCAL;
			unless (local) local = d;
			break;
		    case 'R':
			FLAGS(cset, d) |= D_REMOTE;
			unless (remote) remote = d;
			break;
		}

		/*
		 * Fix up cset marks - if poly, might be missing marks
		 * from remote.
		 */
		unless (FLAGS(cset, d) & D_CSET) {
			assert(FLAGS(cset, d) & D_REMOTE);
			FLAGS(cset, d) |= D_CSET;
			write = 1;
		}
		hash_insertStrPtr(cmarks, ckey, 0);
		/* Need to be able to match product key to range */
		*(char ***)cmarks->vptr =
		    addLine(*(char ***)cmarks->vptr, strdup(list[i]));
	}
	freeLines(list, free);
	list = 0;
	assert(local && remote);
	if (isReachable(cset, local, remote)) {
		fake = (local > remote) ? local : remote;
	}

	if ((polylist = findPoly(cset, local, remote, fake)) == INVALID) {
		polylist = 0;
		goto err;
	}
	if (polylist) {
		if (updatePolyDB(cset, polylist, cmarks)) goto err;
		free(polylist);
		polylist = 0;
	}

	/* if no merge file: fake pending to get included in product merge */
	if (fake) {
		assert(FLAGS(cset, fake) & D_CSET);
		FLAGS(cset, fake) &= ~D_CSET;
		write = 1;
		dfile = sccs_Xfile(cset, 'd');
		touch(dfile, 0666);
	}
	if (write && sccs_newchksum(cset)) goto err;
	if (polyFlush(cset)) goto err;
	rc = 0;
err:
	if (cmarks) hash_free(cmarks);
	FREE(polylist);
	sccs_free(cset);
	FREE(resync);
	return (rc);
}

private void
polyLoad(sccs *cset)
{
	FILE	*f;
	int	c;
	char	*line, *p;
	cmark	**data;
	char	*ckey;
	cmark	cm;
	char	buf[MAXPATH];

	if (cpoly) {
		/* assume we only work on one component at a time for now */
		if (cset->proj == cpoly_proj) return;
		if (cpoly_all &&
		    hash_fetch(cpoly_all, cset->proj, sizeof(project *))) {
			polyFlush(cset);
			cpoly_proj = cset->proj;
			cpoly = *(hash **)cpoly_all->vptr;
			cpoly_dirty = 0;
			return;
		}
	}

	concat_path(buf, proj_root(proj_product(cset->proj)),
		    ROOT2RESYNC "/BitKeeper/etc/poly/");
	sccs_md5delta(cset, sccs_ino(cset), buf + strlen(buf));

	unless (proj_isResync(cset->proj) &&
	    (exists(buf) || !get(buf, SILENT, "-"))) {
		/* not in RESYNC, try repo */
		str_subst(buf, ROOT2RESYNC "/", "", buf);
		unless (exists(buf)) get(buf, SILENT, "-");
	}

	cpoly = hash_new(HASH_MEMHASH);
	cpoly_proj = cset->proj;
	cpoly_dirty = 0;
	unless (cpoly_all) cpoly_all = hash_new(HASH_MEMHASH);
	hash_insert(cpoly_all,
	    cpoly_proj, sizeof(project *), &cpoly, sizeof(hash *));

	if (f = fopen(buf, "r")) {
		while (line = fgetline(f)) { /* foreach key */
			assert(line[0] == '@');
			webdecode(line+1, &ckey, 0);
			hash_insertStrPtr(cpoly, ckey, 0);
			data = (cmark **)cpoly->vptr;
			free(ckey);

			for (;;) {	/* foreach dataline */
				if ((c = fgetc(f)) == EOF) goto done;
				ungetc(c, f);
				if (c == '@') break;
				line = fgetline(f);
				unless (*line) continue;  /* blank lines ok */
				memset(&cm, 0, sizeof(cm));
				p = separator(line);
				assert(p); /* XXX goto err */
				*p++ = 0;
				cm.pkey = strdup(line);
				line = p;
				if (p = separator(line)) *p++ = 0;
				cm.ekey = strdup(line);
				if (p) cm.emkey = strdup(p);
				addArray(data, &cm);
			}
		}
done:		fclose(f);
	}
}

/*
 * Add an entry for a give component delta to the product poly files
 * for this component.
 * The changes are made in memory and we be written out when poly_commit()
 * is called.
 */
private int
polyAdd(sccs *cset, ser_t d, char *pkey, int side)
{
	int	cmp, i;
	cmark	**datap, *data;
	cmark	cm = {0};
	ser_t	*lower;
	char	key[MAXKEY];

	polyLoad(cset);
	sccs_sdelta(cset, d, key);
	hash_insertStrPtr(cpoly, key, 0);
	datap = (cmark **)cpoly->vptr;
	assert(datap);
	data = *datap;
	/* sort by pkey and strip dups */
	EACH(data) {
		unless (cmp = keycmp(pkey, data[i].pkey)) return (0);
		if (cmp < 0) break;
	}
	cm.pkey = strdup(pkey);
	if (lower = lowerBounds(cset, d, side)) {
		sccs_sdelta(cset, lower[1], key);
		cm.ekey = strdup(key);
		if (nLines(lower) > 1) {
			assert(nLines(lower) == 2);
			sccs_sdelta(cset, lower[2], key);
			cm.emkey = strdup(key);
		}
		free(lower);
	}
	insertArrayN(datap, i, &cm);
	cpoly_dirty = 1;
	return (0);
}

private int
polyFlush(sccs *cset)
{
	sccs	*s;
	FILE	*f;
	char	*p, *q;
	char	**keys = 0;
	int	i, rc;
	cmark	*data;
	cmark	*cm;
	char	*sfile;
	int	gflags = GET_EDIT|GET_SKIPGET|SILENT;
	int	dflags = DELTA_AUTO|DELTA_DONTASK|SILENT;
	char	prodpoly[MAXPATH];
	char	resyncpoly[MAXPATH];

	unless (cpoly_dirty) return (0);

	strcpy(prodpoly, proj_root(proj_product(cset->proj)));
	concat_path(resyncpoly, prodpoly, ROOT2RESYNC);

	p = prodpoly + strlen(prodpoly);
	concat_path(prodpoly, prodpoly, "BitKeeper/etc/poly/");
	q = p + strlen(p);
	sccs_md5delta(cset, sccs_ino(cset), q);

	concat_path(resyncpoly, resyncpoly, p);

	// Might be run in many times if pull_ensemble were to run threads
	mkdirf(resyncpoly);

	// write gfile
	f = fopen(resyncpoly, "w");
	assert(f);

	EACH_HASH(cpoly) keys = addLine(keys, cpoly->kptr);
	sortLines(keys, key_sort);

	EACH(keys) {
		fputc('@', f);
		webencode(f, keys[i], strlen(keys[i])+1);
		fputc('\n', f);
		data = (cmark *)hash_fetchStrPtr(cpoly, keys[i]);
		EACHP(data, cm) {
			fprintf(f, "%s %s", cm->pkey, cm->ekey);
			if (cm->emkey) fprintf(f, " %s", cm->emkey);
			fputc('\n', f);
		}
		fputc('\n', f);	/* blank line between keys */
	}
	freeLines(keys, 0);
	fclose(f);

	// edit sfile
	sfile = name2sccs(resyncpoly);
	if (!exists(sfile)) {
		p = name2sccs(prodpoly);
		if (exists(p)) fileLink(p, sfile);
		free(p);
	}
	s = sccs_init(sfile, SILENT);
	assert(s);
	free(sfile);
	if (HASGRAPH(s)) {
		/* edit the file */
		unless (HAS_PFILE(s)) {
			rc = sccs_get(s, 0, 0, 0, 0, gflags, "-");
			//assert(rc == 0);
		}
	} else {
		dflags |= NEWFILE;
	}
	rc = sccs_delta(s, dflags, 0, 0, 0, 0);
	if (rc == -2) {
		/* no delta if no diffs in file */
		unlink(s->pfile);
		unlink(s->gfile);
	}
	if (dflags & NEWFILE) {
		/*
		 * Needed to create a new file.  We are going to force
		 * the file to be created with a deterministic rootkey
		 */
		sccs_restart(s);
		USERHOST_SET(s, sccs_ino(s), USERHOST(cset, sccs_ino(cset)));
		DATE_SET(s, sccs_ino(s), DATE(cset, sccs_ino(cset)));
		ZONE_SET(s, sccs_ino(s), ZONE(cset, sccs_ino(cset)));
		SUM_SET(s, sccs_ino(s), SUM(cset, sccs_ino(cset)));
		RANDOM_SET(s, sccs_ino(s), RANDOM(cset, sccs_ino(cset)));
		sccs_newchksum(s);
	}
	sccs_free(s);
	return (rc);
}

/*
 * Gather up all nodes on or under GCA nodes that are marked
 * local or remote.
 * When we find D_CSET that aren't marked, that should mean GCA
 * from the point of view of the product.  Use those to terminate
 * walkrevs.
 */
private	int
polyMarked(sccs *s, ser_t d, void *token)
{
	ser_t	**list = (ser_t **)token;

	unless (FLAGS(s, d) & D_CSET) return (0);
	unless (FLAGS(s, d) & (D_LOCAL|D_REMOTE)) return (1);

	/* Already in list to be marked Poly; no need to add it again */
	if (FLAGS(s, d) & D_SET) return (0);

	FLAGS(s, d) |= D_SET;
	addArray(list, &d);
	return (0);
}

private ser_t *
findPoly(sccs *s, ser_t local, ser_t remote, ser_t fake)
{
	int	i, hasPoly = 0;
	ser_t	*gcalist, *list = 0;

	addArray(&list, &local);
	addArray(&list, &remote);
	gcalist = range_gcalist(s, list);
	free(list);
	list = 0;

	EACH(gcalist) {
		hasPoly |= polyChk(s, gcalist[i]);
	}
	unless (hasPoly) return (0);
	unless (proj_configbool(0, "poly")) return (INVALID);

	/*
	 * Take that gca list and get a list of poly D_CSET under the gca
	 * D_SET is used to keep from adding dups.
	 */
	range_walkrevs(s, 0, gcalist, 0, WR_STOP, polyMarked, &list);

	/*
	 * gca may be unmarked poly, so add in corresponding tips
	 * D_SET is used to keep from adding dups.
	 */
	addTips(s, gcalist, &list);

	/*
	 * If update only, the tip will be repeated in the merge
	 * so make sure it is tagged poly
	 */
	if (fake && !(FLAGS(s, fake) & D_SET)) {
		addArray(&list, &fake);
		FLAGS(s, fake) |= D_SET;
	}

	free(gcalist);
	EACH(list) FLAGS(s, list[i]) &= ~D_SET;

	return (list);
}

private	int
polyChk(sccs *cset, ser_t gca)
{
	char	*reason;
	u32	flags = (FLAGS(cset, gca) & (D_LOCAL|D_REMOTE));
	char	key[MAXKEY];

	if (flags) {
		assert(FLAGS(cset, gca) & D_CSET);
		unless (proj_configbool(0, "poly")) {
			switch (flags) {
			    case D_LOCAL: reason = "local"; break;
			    case D_REMOTE: reason = "remote"; break;
			    case D_LOCAL|D_REMOTE: reason = "both"; break;
			    default: assert("other reason" == 0); break;
			}
			sccs_sdelta(cset, gca, key);
			fprintf(stderr, "%s: poly on key %s marked in %s\n",
			    prog, key, reason);
		}
		return (1);
	}
	unless (FLAGS(cset, gca) & D_CSET) {
		unless (proj_configbool(0, "poly")) {
			sccs_sdelta(cset, gca, key);
			fprintf(stderr, "%s: poly on unmarked key %s\n",
			    prog, key);
		}
		return (1);
	}
	return (0);
}

/*
 * take gca items and for any non-marked nodes, find their csetmark
 * and add to the list if not already poly.
 * Any gca node that is marked will already be handed by the routine
 * marking children of gca, but all this should work fine if order
 * is switched.
 */
private	void
addTips(sccs *cset, ser_t *gcalist, ser_t **list)
{
	int	i, j;
	ser_t	d, gca;
	u32	flags;
	int	side[] = { D_LOCAL, D_REMOTE, 0};

	EACH(gcalist) {
		gca = gcalist[i];
		for (j = 0; side[j]; j++) {
			flags = side[j];
			d = sccs_csetBoundary(cset, gca, flags);
			unless (FLAGS(cset, d) & D_SET) {
				FLAGS(cset, d) |= D_SET;
				addArray(list, &d);
			}
		}
	}
}

private	int
updatePolyDB(sccs *cset, ser_t *list, hash *cmarks)
{
	int	i, j;
	ser_t	d;
	char	type, *rest;
	u32	side;
	char	**prodkeys = 0;
	char	key[MAXKEY];

	EACH(list) {
		d = list[i];
		sccs_sdelta(cset, d, key);
		prodkeys = *(char ***)hash_fetchStr(cmarks, key);
		assert(prodkeys);
		EACH_INDEX(prodkeys, j) {
			side = (prodkeys[j][0] == 'L') ? D_LOCAL : D_REMOTE;
			type = prodkeys[j][1];
			rest = &prodkeys[j][2];
			if (type == 'M') continue;	/* skip merge nodes */
			if (polyAdd(cset, d, rest, side)) return (1);
		}
	}
	return (0);
}

/*
 * lowerBound callback data structure
 */
typedef struct {
	ser_t	d, *list;
	u32	flags;
} cstop_t;

/*
 * pretend like D_CSET from other side aren't there.
 * That is, if doing D_LOCAL, ignore D_REMOTE
 */
private	int
csetStop(sccs *s, ser_t d, void *token)
{
	cstop_t	*p = token;

	if (!p->flags ||
	    ((FLAGS(s, d) & (D_LOCAL|D_REMOTE)) != p->flags)) {
		if ((p->d != d) && (FLAGS(s, d) & D_CSET)) {
			addArray(&p->list, &d);
			return (1);
		}
	}
	return (0);
}

/*
 * Get list of lower bounds (0 <= nLines(list) <= 2)
 * Caller knows that d is not in poly db.
 * Constrained version of range_cset.
 */
private	ser_t *
lowerBounds(sccs *s, ser_t d, u32 flags)
{
	cstop_t	cs;

	unless (d = sccs_csetBoundary(s, d, 0)) return (0); /* if pending */
	/* 'flags' has what to include; invert to what to ignore */
	if (flags) flags ^= (D_LOCAL|D_REMOTE);
	cs.d = d;
	cs.flags = flags;
	cs.list = 0;
	range_walkrevs(s, 0, 0, d, WR_STOP, csetStop, &cs);
	return (cs.list);
}

/*
 * poly test routine
 *
 * This is mostly a debugging function used to test the nested_init()
 * function in regressions.
 */
int
poly_main(int ac, char **av)
{
	int	c, i;
	int	rc = 1;
	ser_t	d;
	char	**keys = 0;
	sccs	*comp = 0, *prod = 0;
	cmark	*data, *cm;
	char	buf[MAXPATH];

	while ((c = getopt(ac, av, "a", 0)) != -1) {
		switch (c) {
		    case 'a':
		    	i = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		return (0);
	}
	unless (comp = sccs_csetInit(INIT_MUSTEXIST)) goto err;
	unless (proj_isComponent(comp->proj)) goto err;
	polyLoad(comp);

	concat_path(buf, proj_root(proj_product(comp->proj)),
	    proj_isResync(comp->proj) ? ROOT2RESYNC "/" CHANGESET : CHANGESET);
	unless (prod = sccs_init(buf, INIT_MUSTEXIST)) goto err;

	EACH_HASH(cpoly) keys = addLine(keys, cpoly->kptr);
	sortLines(keys, key_sort);

	EACH(keys) {
		unless (d = sccs_findKey(comp, keys[i])) goto err;
		printf("comp %u\n", d);
		data = (cmark *)hash_fetchStrPtr(cpoly, keys[i]);
		EACHP(data, cm) {
			unless (d = sccs_findKey(prod, cm->pkey)) goto err;
			printf("\tprod %u", d);
			unless (d = sccs_findKey(comp, cm->ekey)) goto err;
			printf(" end %u", d);
			if (cm->emkey) {
				unless (d = sccs_findKey(comp, cm->ekey)) {
					goto err;
				}
				printf(" %u", d);
			}
			putchar('\n');
		}
	}
	rc = 0;
err:
	freeLines(keys, 0);
	sccs_free(comp);
	sccs_free(prod);
	return (rc);
}
