/*
 * Copyright 2012-2016 BitMover, Inc
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

#include "sccs.h"
#include "range.h"
#include "poly.h"
#include "resolve.h"
#include "cfg.h"

/*
 *
 * files:
 *   PRODUCT/BitKeeper/etc/poly/md5rootkey
 *
 * File format:
 *   @component_delta_key
 *   product_key oldest_time [ endkey][ merge key]
 *   product_key2 oldest_time [ endkey[ merge key]
 *    <blank line>
 *   @component_delta_key2
 *   product_key oldest_time [ endkey][ merge key]
 *   product_key2 oldest_time [ endkey][ merge key]
 *    <blank line>
 */

private	hash	*polyLoad(sccs *cset, int side);
private	int	polyAdd(sccs *cset, ser_t d, char *ckey, char *pkey, int side);
private	int	polyFlush(void);
private	int	polyChk(sccs *cset, ser_t gca);
private	int	polyMerge(sccs *cset);
private	int	updatePolyDB(sccs *cset, ser_t *list, hash *cmarks);
private	void	addTips(sccs *cset, ser_t *gcalist, ser_t **list);
private	ser_t	*lowerBounds(sccs *s, ser_t d, u32 flags);
private	ser_t	*findPoly(sccs *s, ser_t local, ser_t remote, ser_t fake);

typedef struct {
	hash	*cpoly;		/* ckey -> list of pkeys (local db) */
	hash	*cpolyRemote;	/* ckey -> list of pkeys (remote db) */
	int	dirty;
	char	*rfile;		/* full pathname to file in RESYNC*/
	char	*file;		/* full pathname to file */
	u32	merge:1;	/* merge the polydb file when we write */
} polymap;

private	hash	*cpoly_all;	/* proj -> polymap */

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
	cmark	*ret = 0;
	hash	*cpoly;
	char	*t, *p, *next;
	int	len;
	cmark	cm;
	char	buf[(4 * MAXKEY) + 4];	/* prodkey oldtime ekey mkey\0 */

	cpoly = polyLoad(cset, 0);
	sccs_sdelta(cset, d, buf);
	unless (next = hash_fetchStr(cpoly, buf)) return (0);

	while (t = eachline(&next, &len)) {
		if (*t == '\n') continue; /* blank lines ok */
		memset(&cm, 0, sizeof(cm));
		strncpy(buf, t, len); /* separator() needs \0 */
		buf[len] = 0;
		if (p = separator(buf)) *p++ = 0;
		assert(p);
		cm.pkey = strdup(buf);
		cm.oldtime = strtoul(p, &t, 10);
		if (t && (*t == ' ')) {
			if (p = separator(++t)) *p++ = 0;
			cm.ekey = strdup(t);
			if (p) cm.emkey = strdup(p);
		}
		addArray(&ret, &cm);
	}
	return (ret);
}

/*
 * range_walkrevs() helper function used to see if ser_t token is in
 * the range being walked.  Returning >0 terminates walkrevs.
 */
private	int
inrange(sccs *s, ser_t d, void *token)
{
	return (d == p2uint(token));
}

/*
 * r2c for poly
 * Finds first cset mark and sees if that cset is poly.
 * If so, look to see if the orig cset is part of other poly csets.
 *
 * Returns 0 if poly, 1 if not.
 * Returns an appended list of product cset keys.
 */
int
poly_r2c(sccs *cset, ser_t orig, char ***pcsets)
{
	cmark	*list, *cm;
	ser_t	d, e, *lower = 0;

	unless (d = sccs_csetBoundary(cset, orig, 0)) return (1);
	unless (list = poly_check(cset, d)) return (1);

	/* for the first one found, we are guaranteed to be in range */
	EACHP(list, cm) *pcsets = addLine(*pcsets, strdup(cm->pkey));
	free(list);

	for (d++; d <= TABLE(cset); d++) {
		unless (FLAGS(cset, d) & D_CSET) continue;
		unless (list = poly_check(cset, d)) continue;
		EACHP(list, cm) {
			if (cm->oldtime > DATE(cset, orig)) {
				/* outside of range window */
				continue;
			}
			/*
			 * Walk the range to see if orig is in it
			 * Note: 'orig' can be in range if orig < endpoints
			 */
			if (cm->ekey) {
				e = sccs_findKey(cset, cm->ekey);
				addArray(&lower, &e);
			}
			if (cm->emkey) {
				e = sccs_findKey(cset, cm->emkey);
				addArray(&lower, &e);
			}
			if (range_walkrevs(
			    cset, lower, L(d), 0, inrange, uint2p(orig))) {
				*pcsets = addLine(*pcsets, strdup(cm->pkey));
			}
			FREE(lower);
		}
		free(list);
	}
	return (0);
}

/*
 * poly_range() is range_cset() with an additional product key context.
 * If not pkey, then arbitrarily picks the first one as a valid range.
 * If not poly, fall through to range_cset()
 *
 * Returns the range colored with D_SET, turns on s->state |= S_SET;
 * and marks the limits of the range with s->rstart and s->rstop
 * if the range is not empty.
 */
void
poly_range(sccs *s, ser_t d, char *pkey)
{
	cmark	*clist, *cm = 0;
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
			/* XXX: keep range_walkrevs; sets s->rstart/rstop */
			range_walkrevs(s, lower, L(d), 0,
			    walkrevs_setFlags, uint2p(D_SET));
			s->state |= S_SET;
			FREE(lower);
		} else {
			/*
			 * empty range: fake in a product merge
			 * see 'fake' in poly_pull()
			 */
			s->rstart = s->rstop = 0;
		}
	} else {
		range_cset(s, d);	/* not poly, do normal */
	}
}

/*
 * Save product info in nested_init() that is destined for poly_pull()
 * Called in product weave order (new to old)
 * Format of lines:
 * char0 char1 char2-n 
 * [L|R] prodkey space compkey
 *
 * char0: Local or Remote prod cset (ie, side)
 * Merge are used in processing will get filtered in updatePolyDB()
 *
 * Return a list with formatted line added.
 */
char **
poly_save(char **list, sccs *cset, ser_t d, char *ckey, int side)
{
	char	buf[(2 * MAXKEY) + 3]; /* [L|R]prodey compkey\0 */

	buf[0] = (!side) ? 'G' : ((side & D_LOCAL) ? 'L' : 'R');
	sccs_sdelta(cset, d, buf + 1);
	strcat(buf, " ");
	strcat(buf, ckey);
	return (addLine(list, strdup(buf)));
}

/*
 * Both detection and fixups for handling poly in a pull.
 * If new poly, update the comp's poly db in the product.
 *
 * If the local is ahead of the remote, but the remote adds cset marks,
 * then we need to fake a RESYNC and make it pending, so the tip can
 * be included in the product tip (enhancing the polyness some more).
 *
 * If update only pull, then don't have to fake up the RESYNC, but do
 * have to do the fake pending work on the tip.
 *
 * Returns 0 if succeeds.
 */
int
poly_pull(int got_patch, char *mergefile)
{
	sccs	*cset = 0;
	ser_t	*polylist = 0, d, fake = 0, local = 0, remote = 0;
	ser_t	*gca = 0;
	char	*resync;
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

	unless (cset = sccs_init(resync, INIT_MUSTEXIST)) {
		perror(resync);
		goto err;
	}

	/*
	 * Take local/remote info from product and color component graph
	 * format of line from poly_save():
	 * char0 char1 char2-n 
	 * [L|R|G] prodkey space compkey
	 */
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
		    case 'G':
		    	addArray(&gca, &d);
			continue;
		}

		/*
		 * Fix up cset marks - if poly, might be missing marks
		 * from remote if local already had it.
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
	/* if in GCA, then let D_CSET node be seen as termination */
	EACH(gca) FLAGS(cset, gca[i]) &= ~(D_LOCAL|D_REMOTE);
	FREE(gca);
	freeLines(list, free);
	list = 0;
	assert(local && remote);
	if (isReachable(cset, local, remote)) {
		/* Replicate tip in product merge */
		fake = (local > remote) ? local : remote;
	}

	/* Detect, and if poly found and not allowed, fail */
	if ((polylist = findPoly(cset, local, remote, fake)) == INVALID) {
		polylist = 0;
		goto err;
	}
	if (polylist) {
		if (updatePolyDB(cset, polylist, cmarks)) {
			goto err;
		}
		free(polylist);
		polylist = 0;
	}
	polyMerge(cset);

	/* if no comp merge: fake pending to get included in product merge */
	if (fake) {
		assert(FLAGS(cset, fake) & D_CSET);
		FLAGS(cset, fake) &= ~D_CSET;
		write = 1;
		xfile_store(cset->gfile, 'd', "");
	}
	if (write && sccs_newchksum(cset)) {
		perror(cset->sfile);
		goto err;
	}
	if (polyFlush()) goto err;
	rc = 0;
err:
	if (cmarks) hash_free(cmarks);
	FREE(polylist);
	sccs_free(cset);
	FREE(resync);
	return (rc);
}

private	int
polyRemote(polymap *pm, char *rfile)
{
	sccs	*s = 0;
	names	*revs = 0;
	char	*sfile = 0, *remote = 0;
	int	ret = 1;

	sfile = name2sccs(rfile);
	unless (s = sccs_init(sfile, SILENT|INIT_MUSTEXIST)) {
		ret = 0;
		goto err;
	}
	if (revs = res_getnames(s, 'r')) {
		remote = revs->remote;
		pm->merge = 1;
	}
	if (sccs_get(s, remote, 0, 0, 0, SILENT, s->gfile, 0)) {
		goto err;
	}
	ret = 0;

err:
	freenames(revs, 1);
	free(sfile);
	sccs_free(s);
	return (ret);
}

private hash *
polyLoad(sccs *cset, int side)
{
	char	*p;
	polymap	*pm;
	char	buf[MAXPATH];

	unless (cpoly_all) cpoly_all = hash_new(HASH_MEMHASH);
	hash_insert(cpoly_all,
	    &cset->proj, sizeof(project *),
	    0, sizeof(polymap));
	pm = (polymap *)cpoly_all->vptr;
	unless (pm->cpoly) {
		concat_path(buf, proj_root(proj_product(cset->proj)),
		    ROOT2RESYNC "/BitKeeper/etc/poly/");
		sccs_md5delta(cset, sccs_ino(cset), buf + strlen(buf));

		pm->rfile = strdup(buf);
		str_subst(buf, ROOT2RESYNC "/", "", buf);
		pm->file = strdup(buf);

		p = pm->file;
		unless (exists(p)) get(p, SILENT);
		pm->cpoly = hash_fromFile(hash_new(HASH_MEMHASH), p);

		if (proj_isResync(cset->proj)) {
			p = pm->rfile;
			(void)polyRemote(pm, p);	/* XXX ignore errors? */
			pm->cpolyRemote =
			    hash_fromFile(hash_new(HASH_MEMHASH), p);
		}
	}
	assert (!side || (side == D_LOCAL) || (side == D_REMOTE));
	assert ((side != D_REMOTE) || pm->cpolyRemote);

	return ((side == D_REMOTE) ? pm->cpolyRemote : pm->cpoly);
}

/*
 * Simple merge -- walk the remote db and add it to the local db
 * Note that this doesn't alter dirty, though it should be flushed.
 */
private int
polyMerge(sccs *cset)
{
	int	i, j, cmp;
	hash	*lpoly, *rpoly;
	char	*line, *lend, *rend;
	char	**local, **remote, **merge = 0;

	lpoly = polyLoad(cset, D_LOCAL);
	rpoly = polyLoad(cset, D_REMOTE);

	EACH_HASH(rpoly) {
		if (hash_insertStrStr(lpoly, rpoly->kptr, rpoly->vptr)) {
			continue;	/* was nothing, so just take it */
		}
		remote = splitLine(rpoly->vptr, "\n", 0);
		local = splitLine(lpoly->vptr, "\n", 0);
		j = 1;
		EACH(remote) {
			if (rend = separator(remote[i])) *rend = 0;
			EACH_START(j, local, j) {
				if (lend = separator(local[j])) *lend = 0;
				cmp = keycmp(remote[i], local[j]);
				if (lend) *lend = ' ';
				if (cmp <= 0) {
					unless (cmp) j++;
					break;
				}
				merge = addLine(merge, local[j]);
			}
			if (rend) *rend = ' ';
			merge = addLine(merge, remote[i]);
		}
		EACH_START(j, local, j) {
			assert (*local[j]);
			merge = addLine(merge, local[j]);
		}

		line = joinLines("\n", merge);
		/* XXX: (whitebox) there's room to put the last sep back */
		strcat(line, "\n");
		hash_storeStrStr(lpoly, rpoly->kptr, line);
		free(line);
		freeLines(local, free);
		freeLines(remote, free);
		truncArray(merge, 0);
	}
	freeLines(merge, 0);
	return (0);
}

/*
 * Add an entry for a given component delta to the product poly files
 * for this component.
 * The changes are made in memory and will be written out when poly_commit()
 * is called.
 *
 * Returns:
 *   1 if dup
 *   0 if new key added
 */
private int
polyAdd(sccs *cset, ser_t d, char *ckey, char *pkey, int side)
{
	int	i;
	ser_t	*lower;
	char	*t;
	polymap	*pm;
	hash	*cpoly;
	char	key[MAXKEY];
	char	new[(4 * MAXKEY) + 4];	/* prodkey oldtime ekey mkey\0 */

	cpoly = polyLoad(cset, side);

	if (hash_fetchStr(cpoly, ckey)) return (1);

	/* calculate the new line */
	t = new;
	t += sprintf(t, "%s", pkey);
	if (lower = lowerBounds(cset, d, side)) {
		assert(nLines(lower) <= 3);
		EACH(lower) {
			if (i == 1) {
				sprintf(key, "%lu", DATE(cset, lower[i]));
			} else {
				sccs_sdelta(cset, lower[i], key);
			}
			t += sprintf(t, " %s", key);
		}
		free(lower);
	}
	*t++ = '\n';
	*t = 0;

	hash_storeStrStr(cpoly, ckey, new);

	pm = (polymap *)hash_fetch(cpoly_all, &cset->proj, sizeof(project *));
	pm->dirty = 1;
	return (0);
}

/*
 * Free memory of all poly files and for the ones that changed, write
 * them out to disk and create a new delta.
 */
private int
polyFlush(void)
{
	sccs	*s, *cset;
	char	*p;
	int	rc;
	int	ret = 0;
	char	*sfile;
	polymap	*pm;
	project	*proj;
	int	gflags = GET_EDIT|GET_SKIPGET|SILENT;
	int	dflags = DELTA_AUTO|DELTA_DONTASK|SILENT;
	char	buf[MAXPATH];

	unless (cpoly_all) return (0);

	EACH_HASH(cpoly_all) {
		pm = (polymap *)cpoly_all->vptr;

		if (pm->merge || pm->dirty) {
			sfile = name2sccs(pm->rfile);
			mkdirf(sfile);
			unless (exists(sfile)) {
				p = name2sccs(pm->file);
				if (exists(p)) fileLink(p, sfile);
				free(p);
			}
			unlink(pm->rfile);
			hash_toFile(pm->cpoly, pm->rfile);
			// edit sfile
			s = sccs_init(sfile, SILENT);
			assert(s);
			free(sfile);
			if (pm->merge) {
				/* don't know trunk and branch, so fork */
				rc = sys("bk", "get", "-qgeM", s->gfile, SYS);
				unless (rc) {
					xfile_delete(s->gfile, 'r');
					s->state |= S_PFILE;
				}
				//assert(rc == 0);
			} else if (HASGRAPH(s)) {
				/* edit the file */
				unless (HAS_PFILE(s)) {
					rc = sccs_get(
					    s, 0, 0, 0, 0, gflags, 0, 0);
					//assert(rc == 0);
				} else {
					// XXX: Why would there be a pfile?
					assert(1);
				}
			} else {
				dflags |= DELTA_NEWFILE | DELTA_DB;
			}
			rc = sccs_delta(s, dflags, 0, 0, 0, 0);
			if (rc == -2) {
				/* no delta if no diffs in file */
				xfile_delete(s->gfile, 'p');
				unlink(s->gfile);
			}
			if (dflags & DELTA_NEWFILE) {
				/*
				 * Needed to create a new file.  We
				 * are going to force the file to be
				 * created with a deterministic
				 * rootkey
				 */
				proj = *(project **)cpoly_all->kptr;
				concat_path(buf, proj_root(proj), CHANGESET);
				cset = sccs_init(buf, SILENT|INIT_MUSTEXIST);
				assert(cset);
				sccs_restart(s);
				USERHOST_SET(s, sccs_ino(s),
				    USERHOST(cset, sccs_ino(cset)));
				DATE_SET(s, sccs_ino(s),
				    DATE(cset, sccs_ino(cset)));
				ZONE_SET(s, sccs_ino(s),
				    ZONE(cset, sccs_ino(cset)));
				SUM_SET(s, sccs_ino(s),
				    SUM(cset, sccs_ino(cset)));
				RANDOM_SET(s, sccs_ino(s),
				    RANDOM(cset, sccs_ino(cset)));
				sccs_newchksum(s);
				sccs_free(cset);
			}
			sccs_free(s);
		}
		free(pm->file);
		free(pm->rfile);
		hash_free(pm->cpoly);
		if (pm->cpolyRemote) hash_free(pm->cpolyRemote);
	}
	hash_free(cpoly_all);
	cpoly_all = 0;

	return (ret);
}

/*
 * Detect Poly by finding list of gca nodes for local and remote
 * then seeing if any of them are either marked as being new to
 * local and remote (think about it: how can a node common to local
 * and remote have originate as new in local or remote? poly!),
 * or no D_CSET mark (think about: how can a node that both local
 * and remote have built on not be marked? poly!)
 *
 * Compute the poly list by (time goes from left to right):
 * 1. walk left from the found gca to the "real" gca (uncolored D_CSET marked)
 * and save as poly any other D_CSET marked.
 * 2. take the found gca list and walk right to marked tip. This handles
 * any node in the gca that has no D_CSET mark.  See addTips()
 * 3. Add in the fake node as it will be replicated in the prod cset weave.
 *
 * Returns
 *   INVALID - if poly found and not allowed
 *   or a list serials for new poly csets.
 */
private ser_t *
findPoly(sccs *s, ser_t local, ser_t remote, ser_t fake)
{
	int	i;
	wrdata	wd;
	ser_t	*gcalist = 0, *list = 0, d;

	walkrevs_setup(&wd, s, L(local), L(remote), WR_GCA);
	while (d = walkrevs(&wd)) {
		if (polyChk(s, d)) addArray(&gcalist, &d);
	}
	walkrevs_done(&wd);

	if (nLines(gcalist)) {
		/* if new poly and not allowed - error */
		unless (cfg_bool(0, CFG_POLY)) {
			free(gcalist);
		        getMsg("pull_poly", 0, 0, stderr);
			return (INVALID);
		}

		/*
		 * Take that gca list and get a list of poly D_CSET
		 * on or under the gca.
		 * D_SET is used to keep from adding dups. 
		 * When we find D_CSET that aren't marked, that should mean GCA
		 * from the point of view of the product: prune walk.
		 */
		walkrevs_setup(&wd, s, 0, gcalist, 0);
		while (d = walkrevs(&wd)) {
			unless (FLAGS(s, d) & D_CSET) continue;
			unless (FLAGS(s, d) & (D_LOCAL|D_REMOTE)) {
				walkrevs_prune(&wd, d);
				continue;
			}
			unless (FLAGS(s, d) & D_SET) {
				FLAGS(s, d) |= D_SET;
				addArray(&list, &d);
			}
		}
		walkrevs_done(&wd);

		/*
		 * gca may be unmarked poly, so add in corresponding
		 * tips D_SET is used to keep from adding dups. 
		 */
		addTips(s, gcalist, &list);
	}
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

/*
 * Take a component gca cset and see if has poly signature:
 * not marked, or marked as being unique to local or remote.
 *
 * Returns 0 if not poly, 1 if poly
 */
private	int
polyChk(sccs *cset, ser_t gca)
{
	char	*reason;
	u32	flags = (FLAGS(cset, gca) & (D_LOCAL|D_REMOTE));
	char	key[MAXKEY];

	if (flags) {
		assert(FLAGS(cset, gca) & D_CSET);
		unless (cfg_bool(0, CFG_POLY)) {
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
		unless (cfg_bool(0, CFG_POLY)) {
			sccs_sdelta(cset, gca, key);
			fprintf(stderr, "%s: poly on unmarked key %s\n",
			    prog, key);
		}
		return (1);
	}
	return (0);
}

/*
 * Take gca items and for any non-marked items, find their csetmark
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

/*
 * hash value (key is component cset key)
 * char0 char1-n 
 * [L|R] prodkey
 * Local or Remote prod cset
 * Merge or Single parent prod cset
 *
 * Product merge nodes are not stored in poly db because they aren't part
 * of new poly -- they were created by previous poly, but in a catch-22,
 * the product merge cset wasn't yet created to know its prod key, and
 * now it is known, but not part of this transaction.
 *
 * Returns 0 if success
 */
private	int
updatePolyDB(sccs *cset, ser_t *list, hash *cmarks)
{
	int	i, j;
	ser_t	d;
	char	*rest, sidechar;
	u32	side;
	char	**prodkeys = 0;
	char	key[MAXKEY];

	EACH(list) {
		d = list[i];
		sccs_sdelta(cset, d, key);
		prodkeys = *(char ***)hash_fetchStr(cmarks, key);
		assert(prodkeys);
		EACH_INDEX(prodkeys, j) {
			sidechar = prodkeys[j][0];
			assert(sidechar != 'G');
			side = (sidechar == 'L') ? D_LOCAL : D_REMOTE;
			rest = &prodkeys[j][1];
			(void) polyAdd(cset, d, key, rest, side);
		}
	}
	return (0);
}

/*
 * lowerBound callback data structure
 */
typedef struct {
	ser_t	d, oldest, *list;
	u32	side;
} cstop_t;

/*
 * range_walkrevs() callback
 * Pretend like D_CSET from other side aren't there.
 * That is, if doing D_LOCAL, ignore D_REMOTE
 *
 * Returns 1 if end of range found; 0 to keep going
 */
private	int
csetStop(sccs *s, ser_t d, void *token)
{
	cstop_t	*p = token;
	u32	side;

	if ((d != p->d) && (FLAGS(s, d) & D_CSET)) {
		side = FLAGS(s, d) & (D_LOCAL|D_REMOTE);
		if (!side || (side & p->side)) {
			addArray(&p->list, &d);
			return (-1);
		}
	}
	p->oldest = d;
	return (0);
}

/*
 * Get list of lower bounds (0 <= nLines(list) <= 2)
 * Caller knows that d is not in poly db.
 * d is a tagged tip that is either D_LOCAL || D_REMOTE
 * Basically a version of range_cset that can ignore some D_CSET marks.
 *
 * Return a list of lower bounds
 */
private	ser_t *
lowerBounds(sccs *s, ser_t d, u32 side)
{
	cstop_t	cs;

	cs.d = d;
	cs.side = side;
	cs.list = 0;
	range_walkrevs(s, 0, L(d), 0, csetStop, &cs);
	insertArrayN(&cs.list, 1, &cs.oldest);
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
	hash	*cpoly;
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
	cpoly = polyLoad(comp, 0);

	concat_path(buf, proj_root(proj_product(comp->proj)),
	    proj_isResync(comp->proj) ? ROOT2RESYNC "/" CHANGESET : CHANGESET);
	unless (prod = sccs_init(buf, INIT_MUSTEXIST)) goto err;

	EACH_HASH(cpoly) keys = addLine(keys, cpoly->kptr);
	sortLines(keys, key_sort);

	EACH(keys) {
		unless (d = sccs_findKey(comp, keys[i])) goto err;
		printf("comp %u\n", d);
		data = poly_check(comp, d);
		EACHP(data, cm) {
			unless (d = sccs_findKey(prod, cm->pkey)) goto err;
			printf("\tprod %u", d);
			if (cm->ekey) {
				unless (d = sccs_findKey(comp, cm->ekey)) {
					goto err;
				}
				printf(" end %u", d);
			}
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
	if (rc) fprintf(stderr, "%s: parse error\n", prog);
	freeLines(keys, 0);
	sccs_free(comp);
	sccs_free(prod);
	return (rc);
}
