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

private	int	fastWeave(sccs *s, FILE *out);
private	void	scompress(sccs *s, int serial);

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
earlier(sccs *s, delta *a, delta *b)
{
        int ret;
	char	keya[MAXKEY], keyb[MAXKEY];

        if (a->date < b->date) return 1;
        if (a->date > b->date) return 0;

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
delta *
cset_insert(sccs *s, MMAP *iF, MMAP *dF, delta *parent, int fast)
{
	int	i, error, added = 0;
	delta	*d, *e, *p;
	ser_t	serial = 0; /* serial number for 'd' */ 
	char	*t, *r;
	char	**syms = 0;
	char	key[MAXKEY];
	int	keep;

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
			/* We already have this delta... */
			keep = 0;
			if (e->dangling) {
				e->dangling = 0;
				keep = 1;
			}
			if (!(e->flags & D_CSET) &&
			   (d->flags & D_CSET)) {
			   	e->flags |= D_CSET;
				keep = 1;
			}
			sccs_freetree(d);
			d = keep ? e : 0;
			serial = e->serial;
			goto done;
		}
	}
	d->flags |= D_REMOTE;

	/*
	 * Insert new delta 'd' into s->table in time sorted order
	 */
	if (!s->table) {
		/*
	 	 * Easy case, this is a empty cset file
		 * We are propably getting the 1.0 delta
		 * We only get here if we do "takepatch -i"
		 */
		s->table = d;
		s->tree = d; /* sccs_findKey() wants this */
		d->next = NULL;
		serial = 1;
	} else if (earlier(s, s->table, d)) {
		/*
	 	 * Easy case, this is the newest delta
		 * Just insert it at the top
		 */
		serial = s->nextserial;
		d->next = s->table;
		s->table = d;
	} else {
		/*
		 * OK, We need to insert d somewhere in the middle of the list
		 * find the insertion point...
		 */
		p = s->table;
		while (p) {
			e = p->next;
			if (!e || earlier(s, e, d)) { /* got insertion point */
				serial = p->serial;
				d->next = e;
				p->next = d;
				break;
			}
			p = p->next;
		}
		/*
		 * Update all reference to the moved serial numbers 
		 */
		for (e = s->table; e; e = e->next) {
			int	i;

			if (e->serial < serial) break; /* optimization */

			if (e->serial >= serial) {
				e->serial++;
				sfind_update(s, e, e->serial - 1);
			}
			if (e->pserial >= serial) e->pserial++;
			if (e->merge >= serial) e->merge++;
			if (e->ptag >= serial) e->ptag++;
			if (e->mtag >= serial) e->mtag++;
			EACH(e->include) {
				if (e->include[i] >= serial) e->include[i]++;
			}
			EACH(e->exclude) {
				if (e->exclude[i] >= serial) e->exclude[i]++;
			}
		}
	}
	s->nextserial++;

	/*
	 * Fix up the parent pointer & serial.
	 * Note: this is just linked enough that we can finish takepatch,
	 * it is NOT a fully linked tree.
	 */
	if (parent) {
		d->pserial = parent->serial;
		d->parent = parent;
	}

	/*
	 * Fix up d->serial
	 */
	d->serial = serial;
	sfind_update(s, d, 0);
	assert((d->serial == 0) || (d->serial > d->pserial));

	sccs_inherit(s, d);
	if (!fast && (d->type == 'D') && (s->tree != d)) d->same = 1;

	/*
	 * Fix up tag/symbols
	 * We have not run "bk renumber" yet, remote delta 'd' may have
	 * the same rev as a local delta, i.e. We may have two rev 1.3.
	 * We pass in a NULL rev to addsym(), this force the symbol to be
	 * always added. Otherwise, addsym() will skip the symbol if it finds
	 * a local rev 1.3 tagged with the same symbol.
	 * Passing a NULL rev means s->symbols will be in a incomplete state. 
	 * This is ok, because delta_table() uses the s->symbols->metad pointer
	 * to dump the sysmbol information. It does not use rev in that process.
	 */
	EACH (syms) addsym(s, d, d, 0, NULL, syms[i]);
	if (syms) freeLines(syms, free);

	sccs_findKeyUpdate(s, d);
done:
	/*
	 * Save dF info, used by cset_write() later
	 * Note: meta delta have NULL dF
	 */
	assert(s->iloc < s->nloc);
	s->locs[s->iloc].p = NULL;
	s->locs[s->iloc].len = 0;
	s->locs[s->iloc].serial = serial;
	if (dF && dF->where) {
		s->locs[s->iloc].p = dF->where;
		s->locs[s->iloc].len = dF->size;

		unless (fast) {
			/*
			 * Fix up d->added
			 */
			t = dF->where;
			r = t + dF->size - 1;
			while ( t <= r) if (*t++ == '\n') added++;
			d->added = added - 1;
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
	FILE	*f = 0;
	int	i;
	delta	*d;

	assert(s);
	// assert(s->state & S_CSET);
	assert(s->locs);

	/*
	 * Call sccs_renumber() before writing out the new cset file.
	 * Since the cweave code doesn't build the ->kid pointers
	 * correctly we need to create them here. First we invalidate
	 * the sfind() cache and then call the code used by dinsert()
	 * to update d->kid and d->siblings.
	 *
	 * NOTE: kidlink uses samebranch, so needs renumber to be correct.
	 * Conceptually, that's backwards, as kidlink should be graph
	 * shape based and then renumber could go fast because it
	 * could trust kid pointers.
	 */
	s->ser2dsize = 0;
	if (CSET(s) && spinners) fprintf(stderr, "renumbering");
	sccs_renumber(s, SILENT);
	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		d->kid = d->siblings = 0;
		sccs_kidlink(s, d);
	}
	/*
	 * update the title section with the time sorted newest
	 * inner code lifted from sccs_delta in the PATCH part
	 * XXX can't we just look at s->tree and be done?
	 */
	if (fast) {
		for (d = s->table; d; d = d->next) {
			if (d->flags & D_TEXT) {
				unless (d->flags & D_REMOTE) break;
				if (s->text) {
					freeLines(s->text, free);
					s->text = 0;
				}
				EACH(d->text) {
					s->text = addLine(
					    s->text, strnonldup(d->text[i]));
				}
				break;
			}
		}
	}
	unless (f = sccs_startWrite(s)) goto err;
	s->state |= S_ZFILE|S_PFILE;
	if (fast) {
		if (fastWeave(s, f)) goto err;
	} else {
		if (delta_table(s, f, 0)) {
			perror("table");
			goto err;
		}
		if (sccs_csetPatchWeave(s, f)) goto err;
	}
	fseek(f, 0L, SEEK_SET);
	fprintf(f, "\001H%05u\n", s->cksum);
	if (sccs_finishWrite(s, &f)) return (-1);
	return (0);

err:	sccs_abortWrite(s, &f);
	return (-1);
}

/*
 * This is an interface glue piece to take the legacy iloc stuff
 * and connect it to the new interface way
 */
private	int
fastWeave(sccs *s, FILE *out)
{
	u32	i, serial;
	u32	base = 0, index = 0, offset = 0;
	delta	*d, *e;
	loc	*lp;
	ser_t	*weavemap = 0;
	char	**patchmap = 0;
	MMAP	*fastpatch = 0;
	int	fix = 0, rc = 1;

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
		unless (lp[i].serial) {
			patchmap = addLine(patchmap, INVALID);
			continue;
		}
		d = sfind(s, lp[i].serial);
		assert(d);
		patchmap = addLine(patchmap, d);
		unless (d->flags & D_REMOTE) continue;
		unless (weavemap) {
			/*
			 * allocating more than needed.  We don't know until
			 * the end how many are wasted: it's in 'offset'.
			 */
			base = d->serial - 1;
			weavemap = (ser_t *)calloc(
			    (s->nextserial - base), sizeof(ser_t));
			assert(weavemap);
			index = d->serial;
			weavemap[0] = base;
			offset = 0;
		}
		while (index + offset < d->serial) {
			if (e = sfind(s, index + offset)) {
				serial = e->next ? e->next->serial + 1 : 1;
				if (serial != e->serial) {
					e->serial = serial;
					fix = 1;
				}
				weavemap[index - base] = e->serial;
			}
			index++;
		}
		serial = d->next ? d->next->serial + 1 : 1;
		if (serial != d->serial) {
			d->serial = serial;
			fix = 1;
		}
		offset++;
	}
	if (weavemap) {
		while (index + offset < s->nextserial) {
			if (e = sfind(s, index + offset)) {
				serial = e->next ? e->next->serial + 1 : 1;
				if (serial != e->serial) {
					e->serial = serial;
					fix = 1;
				}
				weavemap[index - base] = e->serial;
			}
			index++;
		}
		if (fix) {
			scompress(s, weavemap[0]+1);
			s->ser2dsize = 0;
		}
	}
	if (delta_table(s, out, 0)) {
		perror("table");
		goto err;
	}

	/*
	 * XXX: mrange (in extractDelta) to p,len (above in cset_insert)
	 * and now back to mrange to give to the diff reader. Wacky.
	 * None of it is needed -- just weave in the stream and
	 * terminate if a blank line.
	 */
	i = s->iloc - 1; /* set index to final element in array */
	assert(i > 0); /* base 1 data structure */
	fastpatch = lp[i].len ? mrange(lp[i].p, lp[i].p + lp[i].len, "b") : 0;

	/* doit */
	rc = sccs_fastWeave(s, weavemap, patchmap, fastpatch, out);
err:	mclose(fastpatch);
	freeLines(patchmap, 0);
	if (weavemap) free(weavemap);
	return (rc);
}

/* XXX: this relies on sfind table not being updated */
private	void
scompress(sccs *s, int serial)
{
	delta	*d;
	int	i;

	while (serial < s->nextserial) {
		unless (d = sfind(s, serial++)) continue;

		if (d->pserial) d->pserial = d->parent->serial;
		if (d->ptag) d->ptag = sfind(s, d->ptag)->serial;
		if (d->mtag) d->mtag = sfind(s, d->mtag)->serial;
		if (d->merge) d->merge = sfind(s, d->merge)->serial;
		EACH(d->include) {
			d->include[i] = sfind(s, d->include[i])->serial;
		}
		EACH(d->exclude) {
			d->exclude[i] = sfind(s, d->exclude[i])->serial;
		}
	}
}
