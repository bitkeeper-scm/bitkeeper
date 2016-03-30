/*
 * Copyright 2001-2005,2007-2016 BitMover, Inc
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

/*
 * ChangeSet weave - a one pass weave for the changeset file.
 *
 * The key observation is that there is no nesting in the ChangeSet file;
 * each delta either adds nothing (content free merge) or it adds a block
 * of contiguous lines.  The only requirement is that the file grows "up",
 * i.e., the most recent block is at the top of the weave body so that when
 * we go through the file looking for a version, we hit our stuff first.
 *
 * The fact that there is no real "weave", it's all additions, no subtractions,
 * and each delta is exactly one or zero additional blocks, makes putting the
 * file back together much easier.
 *
 * Our goal is to add the incoming data to the existing file in one pass,
 * but preserve the attributes that we have in the current explode/implode
 * approach, i.e., that the file converges to the same bits with the same
 * deltas.  Here's how we do that.
 *
 * We need a array, indexed by serial number, and containing a pointer and a
 * length.  When we map in the file, we point the pointer at the offset in the
 * file which contains our block of data (which could be handy in general).
 * We scan ahead until the end of the block and record the length.  The
 * pointer points at the first *data* byte, not the ^AI <ser>, and the length
 * is the length of the data only, not the ^AE <ser>.  We're going to be
 * rewriting the serial numbers anyway, they get wacked as a result of
 * weaving in the incoming deltas.
 *
 * In the patch, for the data, we do pretty much the same thing.  
 * For the deltas, we weave them into the delta table as we hit them, which
 * forces us to rewrite all the serial numbers which come after the one we
 * are inserting.  This is no big deal, you scan until you find the place
 * in the linear order, add the delta using the existing serial, then scan
 * again, incrementing every serial number you find (self, parent, in/ex
 * lists), up to but not including the delta you just added.  We also save
 * pointers to the data in the patch.  Note that these pointers have an
 * extra two bytes of "> " at the beginning of each line which must be
 * stripped out.
 *
 * At this point, we have a rebuilt delta table in memory, so we can write
 * that out.  Note that it has correct serial numbers.  Now we take another
 * pass through the inmemory delta table, and we write out the data.  To
 * do this, we just write out
 *	^AI <d->serial>
 *	write the data; in the patch case we need to strip off the leading
 *	"> "
 *	^AE <d->serial>
 */
#include "system.h"
#include "sccs.h"
#include "range.h"

private	int	fastCsetWeave(sccs *s, int fast);
private	void	fix(sccs *s);

/*
 * Initialize the structure used by cset_insert according to how
 * many csets are going to be added.
 */
int
cweave_init(sccs *s, int extras)
{
	assert(s);
	// assert(s->state & S_CSET);
	assert(!s->locs);
	s->iloc = 1;
	s->nloc = extras + 1;
	s->locs = calloc(s->nloc, sizeof(loc));
	return (0);
}

/*
 * Return true if 'a' is earlier than 'b'
 */
int
earlier(sccs *s, ser_t a, ser_t b)
{
        int ret;
	char	keya[MAXKEY], keyb[MAXKEY];

        if (DATE(s, a) < DATE(s, b)) return 1;
        if (DATE(s, a) > DATE(s, b)) return 0;

	sccs_sortkey(s, a, keya);
	sccs_sortkey(s, b, keyb);
	if (CSET(s)) {
		/*
		 * ChangeSet files can never depend on the pathname to
		 * assure ordering.  For components the pathname varies
		 * with repositories.
		 */
		ret = keycmp_nopath(keya, keyb);
	} else {
		ret = strcmp(keya, keyb);
	}
        if (ret < 0)   return 1;
        if (ret > 0)   return 0;
	// sortkeys compares sortSum. Look at current sum for hints.
	unless (SUM(s, a) == SUM(s, b)) {
		fprintf(stderr, "The source repository has had a different "
		    "transformation,\nlikely involving the presence or "
		    "absence of a file that\nhas been marked gone.\n"
		    "Please write support@bitkeeper.com for assistance.\n\n");
		fprintf(stderr, "File: %s\n", s->gfile);
		fprintf(stderr, "sortkey: %s\n", keya);
		fprintf(stderr, "sum: %u %u\n", SUM(s, a), SUM(s, b));
		exit (1);
	}
        assert("Can't figure out the order of deltas\n" == 0);
        return (-1); /* shut off VC++ comipler warning */
}

/*
 * Insert the delta into the table.
 * We do not bother with the parent/sibling pointers, just the table
 * pointer. If we need the others, we need to call dinsert.
 *
 * XXX Do we need any special processing for meta delta?
 */
ser_t
cset_insert(sccs *s, FILE *iF, FILE *dF, ser_t parent, int fast)
{
	int	i, error;
	ser_t	d, e, p;
	ser_t	serial = 0; /* serial number for 'd' */ 
	char	**syms = 0;
	char	key[MAXKEY];
	int	keep;
	symbol	*sym;

	/*
	 * Get the "new" delta from the mmaped patch file (iF)
	 */
	d = sccs_getInit(s, 0, iF, DELTA_PATCH, &error, 0, &syms);
	sccs_sdelta(s, d, key);
	if (e = sccs_findKey(s, key)) {
		/*
		 * d - remote patch delta
		 * e - local delta with matching key to d
		 */
		ser_t	rpar = parent, rmerge = MERGE(s, d);
		ser_t	lpar = PARENT(s, e), lmerge = MERGE(s, e);

		/* We already have this delta... lineage line up? */
		if ((lpar != rpar) || (lmerge != rmerge)) {
			fprintf(stderr, "%s: duplicate delta with "
			    "different parents\n", s->gfile);
			if (rpar) sccs_sdelta(s, rpar, key);
			fprintf(stderr,
			    "local parent: %s\n"
			    "remote parent: %s (%s)\n",
			    (lpar) ? REV(s, lpar) : "none",
			    rpar ? key : "none",
			    (rpar && (FLAGS(s, rpar) & D_REMOTE))
			    ? "remote"
			    : (rpar ? REV(s, rpar) : ""));
			if (rmerge) sccs_sdelta(s, rmerge, key);
			fprintf(stderr,
			    "local merge: %s\n"
			    "remote merge: %s (%s)\n",
			    lmerge ? REV(s, lmerge) : "none",
			    rmerge ? key : "none",
			    (rmerge && (FLAGS(s, rmerge) & D_REMOTE))
			    ? "remote"
			    : (rmerge ? REV(s, rmerge) : ""));
			fprintf(stderr, "Please send "
			    "the output to support@bitkeeper.com\n");
			return (D_INVALID);
		}
		keep = 0;
		if (DANGLING(s, e)) {
			FLAGS(s, e) &= ~D_DANGLING;
			keep = 1;
		}
		if (!(FLAGS(s, e) & D_CSET) &&
		   (FLAGS(s, d) & D_CSET)) {
		   	FLAGS(s, e) |= D_CSET;
			keep = 1;
		}
		sccs_freedelta(s, d);
		d = keep ? e : 0;
		serial = e;
		goto done;
	}
	FLAGS(s, d) |= D_REMOTE;

	T_DEBUG("%s/%d", delta_sdate(s, d), SUM(s, d));
	/*
	 * Insert new delta 'd' into TABLE(s) in time sorted order
	 */
	if (!TABLE(s)) {
		/*
	 	 * Easy case, this is a empty cset file
		 * We are propably getting the 1.0 delta
		 * We only get here if we do "takepatch -i"
		 */
		serial = 1;
	} else if (earlier(s, TABLE(s), d)) {
		/*
	 	 * Easy case, this is the newest delta
		 * Just insert it at the top
		 */
		serial = TABLE(s) + 1;
	} else {
		/*
		 * OK, We need to insert d somewhere in the middle of the list
		 * find the insertion point...
		 */
		p = TABLE(s);
		while (p) {
			e = sccs_prev(s, p);
			if (!e || earlier(s, e, d)) { /* got insertion point */
				serial = p;
				break;
			}
			p = e;
		}
	}
	d = sccs_insertdelta(s, d, serial);
	/* remap parent.  Needed by the "lineage line up?" section above */
	for (e = d + 1; e <= TABLE(s); e++) {
		EACH_PARENT(s, e, p, i) {
			if (p >= d) PARENTS_SET(s, e, i, p+1);
		}
	}
	if (d != TABLE(s)) {
		EACHP_REVERSE(s->symlist, sym) {
			if (sym->ser >= serial) sym->ser++;
			if (sym->meta_ser >= serial) sym->meta_ser++;
		}
	}

	T_DEBUG("serial=%d", serial);
	PARENT_SET(s, d, parent);

	sccs_inherit(s, d);

	/*
	 * Fix up tag/symbols
	 * We pass graph=0 which forces the tag to be added even if it
	 * is a duplicate.
	 */
	EACH(syms) addsym(s, d, 0, syms[i]);

done:
	/*
	 * Save dF info, used by cset_write() later
	 * Note: meta delta have NULL dF
	 */
	assert(s->iloc < s->nloc);
	s->locs[s->iloc].serial = serial;
	s->locs[s->iloc].dF = dF;
	s->iloc++;
	if (syms) freeLines(syms, free);
	return (d);
}

private	void
fix(sccs *s)
{
	ser_t	d, e;
	ser_t	base;
	ser_t	*remap = 0;
	char	*p;
	int	sign;
	int	i;
	FILE	*f = fmem();

#define	SERMAP(x) (remap[(x) - base])

	/* find first patch that was inserted */
	for (d = 0, i = 1; i < s->iloc; i++) {
		if ((d = s->locs[i].serial) && (FLAGS(s, d) & D_REMOTE)) break;
	}
	unless (d  && (FLAGS(s, d) & D_REMOTE)) return;
	/* 'base' okay to be 0; newest local delta that needs no change */
	base = d - 1;

	/*
	 * Update the internals of all local deltas.
	 */
	for (d++; d <= TABLE(s); d++) {
		if (FLAGS(s, d) & D_REMOTE) continue;
		addArray(&remap, &d);
		if (HAS_CLUDES(s, d)) {
			assert(INARRAY(s, d));
			p = CLUDES(s, d);
			while (e = sccs_eachNum(&p, &sign)) {
				if (e > base) e = SERMAP(e);
				sccs_saveNum(f, e, sign);
			}
			p = fmem_peek(f, 0);
			unless (streq(p, CLUDES(s, d))) CLUDES_SET(s, d, p);
			ftrunc(f, 0);
		}
		/* parents already remapped in cset_insert */
		EACH_PTAG(s, d, e, i) {
			if (e > base) PTAGS_SET(s, d, i, SERMAP(e));
		}
	}
	fclose(f);
	free(remap);
}

/*
 * Write out the new ChangeSet file.
 */
int
cset_write(sccs *s, int spinners, int fast)
{
	assert(s);
	assert(s->locs);

	/* finish up table remapping */
	fix(s);

	/*
	 * Call sccs_renumber() before writing out the new cset file.
	 * Since the cweave code doesn't build the ->kid pointers
	 *
	 * NOTE: kidlink uses samebranch, so needs renumber to be correct.
	 * Conceptually, that's backwards, as kidlink should be graph
	 * shape based and then renumber could go fast because it
	 * could trust kid pointers.
	 */
	if (CSET(s) && spinners) fprintf(stderr, "renumbering");
	sccs_renumber(s, SILENT);

	unless (sccs_startWrite(s)) goto err;
	s->state |= S_PFILE;
	if (CSET(s)) {
		if (fastCsetWeave(s, fast)) goto err;
	} else if (fast) {
		if (sccs_fastWeave(s)) goto err;
	} else {
		if (sccs_slowWeave(s)) goto err;
	}
	if (sccs_finishWrite(s)) goto err;
	return (0);

err:	sccs_abortWrite(s);
	return (-1);
}

/*
 * This is an interface glue piece to take the legacy iloc stuff
 * and connect it to the new interface way
 */
private	int
fastCsetWeave(sccs *s, int fast)
{
	int	i, cnt;
	ser_t	d = 0;
	loc	*lp;
	int	rc = 1;
	FILE	*f;
	char	*t;
	char	*rkey, *dkey;
	char	**keys = 0;
	hash	*h;
	u32	first, rkoff, dkoff;
	struct {
		char	**keys;
		ser_t	d;
	} *weave = 0, *w;

	assert(s);
	assert(CSET(s));
	assert(s->locs);
	lp = s->locs;
	i = s->iloc - 1; /* set index to final element in array */
	assert(i > 0); /* base 1 data structure */

	if (fast && lp[i].dF) {
		/* extract cset weave updates */
		while (t = fgetline(lp[i].dF)) {
			if (t[0] == 'I') {
				/*
				 * d = 0 - skipkeys
				 * ! d & D_REMOTE - delta already here
				 */
				d = lp[atoi(t+1)].serial;
				unless (d && (FLAGS(s, d) & D_REMOTE)) d = 0;
			} else if (t[0] == '>') {
				unless (d) continue;
				rkey = t+1;
				dkey = separator(rkey);
				*dkey++ = 0;
				keys = addLine(keys, strdup(rkey));
				keys = addLine(keys, strdup(dkey));
			} else if (t[0] == 'E') {
				unless (d) continue;
				w = growArray(&weave, 1);
				w->d = d;
				w->keys = keys;
				keys = 0;
			}
		}
	} else if (!fast) {
		/* old cset patch, one diff per delta */
		for (i = s->iloc - 1; i > 0; i--) {
			w = growArray(&weave, 1);
			w->d = lp[i].serial;
			unless (lp[i].dF) continue;
			while (t = fgetline(lp[i].dF)) {
				if (streq(t, "0a0")) continue;
				assert(strneq(t, "> ", 2));
				rkey = t+2;
				dkey = separator(t);
				*dkey++ = 0;
				w->keys = addLine(w->keys, strdup(rkey));
				w->keys = addLine(w->keys, strdup(dkey));
			}
		}
	}
	/*
	 * install cweave from oldest to newest so we can set
	 * the markers for new files correctly.
	 */
	cnt = 0;
	h = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
	EACHP_REVERSE(weave, w) {
		EACH(w->keys) {
			rkey = w->keys[i];
			dkey = w->keys[++i];
			if (rkoff = sccs_hasRootkey(s, rkey)) {
				/*
				 * remember first serial where we
				 * added this rootkey. It is possible
				 * this really is a new file and needs a
				 * marker, but we don't know. Later we
				 * will make sure deltas for this file
				 * newer than here don't have a
				 * marker.
				 */
				if (hash_insertU32U32(h, rkoff, w->d)) cnt++;
			} else {
				/* first time for this rootkey, new file */
				rkoff = sccs_addUniqRootkey(s, rkey);
				hash_insertU32U32(h, rkoff, 0);
				w->keys[i] = aprintf("|%s", dkey);
				free(dkey);
			}
		}
	}

	/*
	 * Now for the files a just added, walk the weave for newer csets
	 * add see if any of those csets have a weave end marker that needs
	 * to be cleared.
	 */
	if (cnt) {
		sccs_rdweaveInit(s);
		while (d = cset_rdweavePair(s, 0, &rkoff, &dkoff)) {
			unless (first = hash_fetchU32U32(h, rkoff)) continue;
			if (d > first) {
				if (dkoff) continue;
				weave_updateMarker(s, d, rkoff, 0);
			} else {
				*(u32 *)h->vptr = 0;	/* we are not oldest */
			}
			unless (--cnt) break;
		}
		sccs_rdweaveDone(s);
	}
	EACHP_REVERSE(weave, w) {
		EACH(w->keys) {
			rkey = w->keys[i];
			dkey = w->keys[++i];
			if ((rkoff = sccs_hasRootkey(s, rkey)) &&
			    hash_fetchU32U32(h, rkoff)) {
				w->keys[i] = aprintf("|%s", dkey);
				free(dkey);
				*(u32 *)h->vptr = 0;
			}
		}
		assert(!WEAVE_INDEX(s, w->d));
		weave_set(s, w->d, w->keys);
		freeLines(w->keys, free);
	}
	hash_free(h);

	if (delta_table(s, 0)) {
		perror("table");
		goto err;
	}
	unless (BWEAVE_OUT(s)) {
		sccs_rdweaveInit(s);
		f = sccs_wrweaveInit(s);
		while (t = sccs_nextdata(s)) {
			fputs(t, f);
			fputc('\n', f);
		}
		sccs_rdweaveDone(s);
		sccs_wrweaveDone(s);
	}
	rc = 0;
err:	return (rc);
}
