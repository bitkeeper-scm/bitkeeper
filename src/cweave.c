/*
 * ChangeSet weave - a one pass weave for the changeset file.
 * Copyright January 13th 2001, Larry McVoy, Andrew Chang
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

private	int	fastWeave(sccs *s);

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
		    "Please write support@bitmover.com for assistance.\n\n");
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
	int	i, error, sign, added = 0;
	ser_t	d, e, p;
	ser_t	serial = 0; /* serial number for 'd' */ 
	char	*t;
	char	**syms = 0;
	char	key[MAXKEY];
	int	keep;
	symbol	*sym;
	FILE	*f = 0;

	unless (iF) {	
		/* ignore in patch: like if in skipkeys */
		assert(fast);
		d = 0;
		serial = 0;
		goto done;
	}

	/*
	 * Get the "new" delta from the mmaped patch file (iF)
	 */
	d = sccs_getInit(s, 0, iF, DELTA_PATCH, &error, 0, &syms);
	if (fast) {
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
				    "the output to support@bitmover.com\n");
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
	}
	FLAGS(s, d) |= D_REMOTE;

	TRACE("%s/%d", delta_sdate(s, d), SUM(s, d));
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

	/*
	 * Update all reference to the moved serial numbers
	 */
	f = fmem();
	for (e = d + 1; e <= TABLE(s); e += 1) {

		if (HAS_CLUDES(s, e)) {
			assert(INARRAY(s, e));
			t = CLUDES(s, e);
			while (i = sccs_eachNum(&t, &sign)) {
				if (i >= serial) i++;
				sccs_saveNum(f, i, sign);
			}
			t = fmem_peek(f, 0);
			unless (streq(t, CLUDES(s, e))) {
				CLUDES_SET(s, e, t);
			}
			ftrunc(f, 0);
		}
		if (PARENT(s, e) >= serial) PARENT_SET(s, e, PARENT(s, e)+1);
		if (MERGE(s, e) >= serial) MERGE_SET(s, e, MERGE(s, e)+1);
		if (PTAG(s, e) >= serial) PTAG_SET(s, e, PTAG(s, e)+1);
		if (MTAG(s, e) >= serial) MTAG_SET(s, e, MTAG(s, e)+1);
	}
	fclose(f);
	if (d != TABLE(s)) {
		EACHP_REVERSE(s->symlist, sym) {
			if (sym->ser >= serial) sym->ser++;
			if (sym->meta_ser >= serial) sym->meta_ser++;
		}
	}

	TRACE("serial=%d", serial);
	PARENT_SET(s, d, parent);

	sccs_inherit(s, d);
	if (!fast && !TAG(s, d) && (TREE(s) != d)) SAME_SET(s, d, 1);

	/*
	 * Fix up tag/symbols
	 * We pass graph=0 which forces the tag to be added even if it
	 * is a duplicate.
	 */
	EACH(syms) addsym(s, d, 0, syms[i]);
	if (syms) freeLines(syms, free);

done:
	/*
	 * Save dF info, used by cset_write() later
	 * Note: meta delta have NULL dF
	 */
	assert(s->iloc < s->nloc);
	s->locs[s->iloc].serial = serial;
	if (s->locs[s->iloc].dF = dF) {
		unless (fast) {
			/*
			 * Fix up ADDED(s, d)
			 * If not fast, then it must be a cset, and
			 * it can't be empty (because dF is set)
			 * so it will have one insert block that is
			 * a command line and the rest of the lines
			 * are data lines.
			 */
			while ((i = getc(dF)) != EOF) if (i == '\n') added++;
			assert(added);
			ADDED_SET(s, d, added - 1);
			rewind(dF);
		}
	}
	s->iloc++;
	return (d);
}

/*
 * Write out the new ChangeSet file.
 */
int
cset_write(sccs *s, int spinners, int fast)
{
	assert(s);
	// assert(s->state & S_CSET);
	assert(s->locs);

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
	if (fast) {
		if (fastWeave(s)) goto err;
	} else {
		if (delta_table(s, 0)) {
			perror("table");
			goto err;
		}
		if (sccs_csetPatchWeave(s)) goto err;
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
fastWeave(sccs *s)
{
	u32	i;
	u32	base = 0, index = 0, offset = 0;
	ser_t	d, e;
	loc	*lp;
	ser_t	*weavemap = 0;
	ser_t	*patchmap = 0;
	int	rc = 1;

	assert(s);
	// assert(s->state & S_CSET);
	assert(s->locs);
	lp = s->locs;

	/*
	 * weavemap is used to renumber serials in the weave
	 * map(A) -> B, where map(A) is weavemap[A - map[0]],
	 * meaning weavemap[0] is A; ie, A maps to itself.
	 * And serials > A map to a big number to create the
	 * holes filled in by the new data coming in.
	 *
	 * patchmap is addLines mapping patch serial number (1..n) to
	 * serial to use in the weave.  It does this by being an addLines
	 * of pointers to d, then uses d->serial to write the weave.
	 * Note: patchmap could be empty if say, if we are updating
	 * a dangling delta pointer or cset mark.
	 */
	for (i = 1; i < s->iloc; i++) {
		unless (d = lp[i].serial) {
			d = D_INVALID;
			addArray(&patchmap, &d);
			continue;
		}
		addArray(&patchmap, &d);
		unless (FLAGS(s, d) & D_REMOTE) continue;
		unless (weavemap) {
			/*
			 * allocating more than needed.  We don't know until
			 * the end how many are wasted: it's in 'offset'.
			 */
			base = d - 1;
			weavemap = (ser_t *)calloc(
			    (TABLE(s) + 1 - base), sizeof(ser_t));
			assert(weavemap);
			index = d;
			weavemap[0] = base;
			offset = 0;
		}
		while (index + offset < d) {
			e = index + offset;
			weavemap[index - base] = e;
			index++;
		}
		offset++;
	}
	if (weavemap) {
		while (index + offset <= TABLE(s)) {
			e = index + offset;
			weavemap[index - base] = e;
			index++;
		}
	}
	if (delta_table(s, 0)) {
		perror("table");
		goto err;
	}

	i = s->iloc - 1; /* set index to final element in array */
	assert(i > 0); /* base 1 data structure */

	/* doit */
	rc = sccs_fastWeave(s, weavemap, patchmap, lp[i].dF);
err:	free(patchmap);
	if (weavemap) free(weavemap);
	return (rc);
}
