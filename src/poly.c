#include "sccs.h"
#include "range.h"
#include "poly.h"

/*
 * XXX TODO list
 * - "fix" tests so they fail again
 * - set feature bit if D_POLY is ever generated
 * - fix range_cset()
 * - fix sccs_csetBoundry()
 * - make everything else use those two
 * - D_POLY updates need to propagate on pull
 *   (pull may update poly file without component)
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
private ser_t	*findPoly(sccs *s, ser_t local, ser_t remote);

#define	LOCAL	0x01
#define	REMOTE	0x02
#define	BOTH	(LOCAL|REMOTE)

static	hash	*cpoly;		/* ckey -> array of cmarks */
static	project	*cpoly_proj;
static	int	cpoly_dirty;

/*
 * When given a delta in a component cset that is csetmarked it
 * returns information about the product cset(s) that include this delta.
 *
 *  Returns a lines array of pointers to cmark structs.
 */
cmark *
poly_check(sccs *cset, ser_t d)
{
	cmark	*ret;
	char	key[MAXKEY];

	assert(FLAGS(cset, d) & D_CSET);

	/* 
	 * We could actually return data for normal csetmarks, but for
	 * now we assume this function won't be called in that case.
	 */
	assert(FLAGS(cset, d) & D_POLY);

	polyLoad(cset);

	sccs_sdelta(cset, d, key);
	ret = hash_fetchStrPtr(cpoly, key);
	if (FLAGS(cset, d) & D_POLY) assert(ret);
	return (ret);
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
pullPoly(int got_patch, char *mergefile)
{
	sccs	*cset = 0;
	ser_t	*polylist = 0, d, local = 0, remote = 0;
	char	*prefix, *resync;
	int	rc = 1, write = 0;
	int	i;
	u8	side;
	char	*ckey;
	char	**list;
	hash	*cmarks = 0;

	resync = aprintf("%s/%s", ROOT2RESYNC, CHANGESET);

	/* cons up a RESYNC area in case "Nothing to pull" */
	unless (got_patch) {
		if (mkdir("RESYNC", 0777)) {
			perror("make poly resync");
			goto err;
		}
		sccs_mkroot("RESYNC");
		fileLink(CHANGESET, resync);
		touch("RESYNC/BitKeeper/tmp/patch", 0666);
	}

	unless (cset = sccs_init(resync, INIT_MUSTEXIST)) goto err;
	assert(cset);

	side = LOCAL;
	list = file2Lines(0, mergefile);
	cmarks = hash_new(HASH_MEMHASH);
	EACH(list) {
		unless (*list[i]) { // if blank line, which is separator
			side = REMOTE;
			continue;
		}
		ckey = separator(list[i]);
		assert(ckey);
		*ckey++ = 0;
		d = sccs_findKey(cset, ckey);
		assert(d);
		if (side & LOCAL) {
			FLAGS(cset, d) |= D_LOCAL;
			unless (local) local = d;
		}
		if (side & REMOTE) { 
			FLAGS(cset, d) |= D_REMOTE;
			unless (remote) remote = d;
		}

		/*
		 * Fix up cset marks - if poly, might be missing marks
		 * from remote.
		 */
		unless (FLAGS(cset, d) & D_CSET) {
			assert(side & REMOTE);
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

	if ((polylist = findPoly(cset, local, remote)) == INVALID) {
		polylist = 0;
		goto err;
	}
	if (polylist) {
		if (updatePolyDB(cset, polylist, cmarks)) goto err;
		free(polylist);
		polylist = 0;
	}

	/* if no merge file: fake pending to get included in product merge */
	prefix = resync + strlen(resync) - 11;
	assert(*prefix == 's');
	*prefix = 'r';
	if (!exists(resync)) {
		d = sccs_top(cset);
		assert(FLAGS(cset, d) & D_CSET);
		FLAGS(cset, d) &= ~D_CSET;
		write = 1;
		*prefix = 'd';
		touch(resync, 0666);
	}
	if (write && sccs_newchksum(cset)) goto err;
	rc = 0;
	polyFlush(cset);
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
		assert(cset->proj == cpoly_proj);
		return;
	}

	concat_path(buf, proj_root(proj_product(cset->proj)),
		    ROOT2RESYNC);
	concat_path(buf, buf, "BitKeeper/etc/poly/");
	sccs_md5delta(cset, sccs_ino(cset), buf + strlen(buf));
	
	unless (exists(buf) || !get(buf, SILENT, "-")) {
		/* not in RESYNC, try repo */
		str_subst(buf, ROOT2RESYNC "/", "", buf);
		unless (exists(buf)) get(buf, SILENT, "-");
	}

	cpoly = hash_new(HASH_MEMHASH);
	cpoly_proj = cset->proj;
	cpoly_dirty = 0;

	if (f = fopen(buf, "r")) {
		while (line = fgetline(f)) { /* foreach key */
			assert(line[0] == '@');
			webdecode(line+1, &ckey, 0);
			hash_insertStrPtr(cpoly, ckey, 0);
			data = (cmark **)cpoly->vptr;
			free(ckey);

			while ((c = fgetc(f)) != '@') { /* foreach dataline */
				ungetc(c, f);
				line = fgetline(f);
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
		fclose(f);
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
	cmark	**data;
	cmark	cm = {0};
	ser_t	*lower = lowerBounds(cset, d, side);
	char	key[MAXKEY];

#if 0
	fprintf(stderr, "comp %s, prod %s\n", REV(cset, d), pkey);
	EACH(lower) {
		fprintf(stderr, "\t%s\n", REV(cset, lower[i]));
	}
#endif

	polyLoad(cset);
	sccs_sdelta(cset, d, key);
	hash_insertStrPtr(cpoly, key, 0);
	data = (cmark **)cpoly->vptr;
	assert(data);
	cm.pkey = strdup(pkey);
	sccs_sdelta(cset, lower[1], key);
	cm.ekey = strdup(key);
	if (nLines(lower) > 1) {
		sccs_sdelta(cset, lower[2], key);
		cm.emkey = strdup(key);
	}
	free(lower);
	// sort by pkey and strip dups
	EACH(*data) {
		cmp = keycmp(cm.pkey, (*data)[i].pkey);
		if (cmp < 0) break;
		if (cmp == 0) {
			free(cm.pkey);
			free(cm.ekey);
			FREE(cm.emkey);
			return (0);
		}
	}
	insertArrayN(data, i, &cm);
	cpoly_dirty = 1;
	return (0);
}

private int
polyFlush(sccs *cset)
{
	sccs	*s;
	FILE	*f;
	char	**keys = 0;
	int	i, rc;
	cmark	*data;
	cmark	*cm;
	char	*sfile;
	int	gflags = GET_EDIT|GET_SKIPGET|SILENT;
	int	dflags = DELTA_AUTO|DELTA_DONTASK|SILENT;
	char	buf[MAXPATH];

	unless (cpoly_dirty) return (0);

	concat_path(buf, proj_root(proj_product(cset->proj)),
		    ROOT2RESYNC);
	concat_path(buf, buf, "BitKeeper/etc/poly/");
	sccs_md5delta(cset, sccs_ino(cset), buf + strlen(buf));

	mkdirf(buf);

	// write gfile
	f = fopen(buf, "w");
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
	}
	freeLines(keys, 0);		
	fclose(f);

	// edit sfile
	sfile = name2sccs(buf);
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

	/* Already marked Poly; no need to add it again */
	if (FLAGS(s, d) & (D_POLY|D_SET)) return (0);

	/*
	 * Don't color POLY yet, so we can see patch nodes
	 * that were already marked poly.  Wait until we
	 * write into poly db to mark them.
	 */
	FLAGS(s, d) |= D_SET;
	addArray(list, &d);
	return (0);
}

private ser_t *
findPoly(sccs *s, ser_t local, ser_t remote)
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
	 */
	range_walkrevs(s, 0, gcalist, 0, WR_STOP, polyMarked, &list);

	/*
	 * gca may be unmarked poly, so add in corresponding tips
	 */
	addTips(s, gcalist, &list);
	free(gcalist);

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
			unless (FLAGS(cset, d) & (D_POLY|D_SET)) {
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
	char	**prodkeys = 0;
	char	key[MAXKEY];

	EACH(list) {
		d = list[i];
		sccs_sdelta(cset, d, key);
		prodkeys = *(char ***)hash_fetchStr(cmarks, key);
		assert(prodkeys);
		j = 1;
		/* one or the other or if both, local first (see pullPoly) */
		if (FLAGS(cset, d) & D_LOCAL) {
			if (polyAdd(cset, d, prodkeys[j++], D_LOCAL)) {
				return (1);
			}
		}
		if (FLAGS(cset, d) & D_REMOTE) {
			if (polyAdd(cset, d, prodkeys[j++], D_REMOTE)) {
				return (1);
			}
		}
		FLAGS(cset, d) &= ~D_SET;
		FLAGS(cset, d) |= D_POLY;
	}
	return (0);
}

/*
 * Edits to Wayne's range_cset to operate under more constraints.
 * Gather up a list of lower bounds
 */
typedef struct {
	ser_t	d, *list;
	u32	flags;
} cstop_t;

/* pretend like D_CSET from other side aren't there.
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

private	ser_t *
lowerBounds(sccs *s, ser_t d, u32 flags)
{
	cstop_t	cs;

	unless (d = sccs_csetBoundary(s, d, 0)) return (0); /* if pending */
	/* This lower bound detector is only for non poly stuff */
	assert(!(FLAGS(s, d) & D_POLY));
	/* Pass in what to include; invert to what to ignore */
	if (flags) flags ^= (D_LOCAL|D_REMOTE);
	cs.d = d;
	cs.flags = flags;
	cs.list = 0;
	range_walkrevs(s, 0, 0, d, WR_STOP, csetStop, &cs);
	return (cs.list);
}
