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
 * 
 * That's it.  And we can make it be a standalone thing so we can test it
 * by handing it a patch with just the cset data in it.  The interface
 * should be a routine which gets a pointer to an mmapped file and returns
 * a length which it consumed; this is nice for takepatch.
 *
 * Implementation notes: I'd make a 
 *	typedef struct loc {
 *		char	*data;
 *		u32	len;
 *	} loc;
 *
 * and then allocate a chunk of these when reading in the s.ChangeSet file.
 * The hard part is that you don't know how many are needed from the patch,
 * it would be cool if takepatch counted these and passed in the count.  If
 * not, we need to pass over the patch file and count them.  The total count
 * is the count in the patch plus the d->serial of first delta seen in the
 * s.ChangeSet file.
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "zlib/zlib.h"


private int
atoi2(char **sp)
{
	register int val = 0;
	register char *s = *sp;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	*sp = s;
	return (val);
}                    

/*
 * Given an open sccs *, scan the data and map it.
 * Extras is the amount of extra space to allocate for the patch deltas.
 */
int
cset_map(sccs *s, int extras)
{
	char	*p;
	int	len;
	int	newline;
	ser_t	ser;

	assert(s);
	assert(s->state & S_CSET);
	assert(!s->locs);
	s->nloc = s->nextserial + extras;
	s->locs = calloc(s->nloc, sizeof(loc));
	assert(s->encoding == E_ASCII);
	p = s->mmap + s->data; /* set p to start of delta body */
	while (p < (s->mmap + s->size)) {
		assert(strneq(p, "\001I ", 3));
		p += 3;
		ser = atoi2(&p);
		assert(ser < s->nloc);
		assert(*p == '\n');
		p++;
		s->locs[ser].p = p;
		for (len = newline = 1; p < (s->mmap + s->size); p++, len++) {
			if (newline) {
				if (*p == '\001') break;
				newline = 0;
			}
			if (*p == '\n') newline = 1;
		}
		assert(strneq(p, "\001E ", 3));
		if (len) s->locs[ser].len = len - 1;
		while (*p++ != '\n');
	}
	return (0);
}

/*
 * Return true if 'a' is earlier than 'b'
 */
private int
earlier(sccs *s, delta *a, delta *b)
{
        int ret;
	char	keya[MAXKEY], keyb[MAXKEY];
 
        if (a->date < b->date) return 1;
        if (a->date > b->date) return 0;
	
	sccs_sdelta(s, a, keya);
	sccs_sdelta(s, b, keyb);
        ret = strcmp(keya, keyb);
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
cset_insert(sccs *s, MMAP *iF, MMAP *dF, char *parentKey)
{
	int	i, error, added = 0;
	delta	*d, *e, *p;
	ser_t	serial; /* serial number for 'd' */ 
	char	*t, *r;
	char	**syms = 0;

	assert(iF);

	/*
	 * Get the "new" delta from the mmaped patch file (iF)
	 */
	d = sccs_getInit(s, 0, iF, DELTA_PATCH, &error, 0, &syms);

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
		 * Move all serial numbers that is above
		 * the insertion point up one slot.
		 */
		memmove(&(s->locs[serial+1]), &(s->locs[serial]),
			(s->table->serial - serial + 1) * sizeof(loc));  

		/*
		 * Update all reference to the moved serial numbers 
		 */
		for (e = s->table; e; e = e->next) {
			int	i;

			if (e->serial >= serial) e->serial++;
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
			EACH(e->ignore) {
				if (e->ignore[i] >= serial) e->ignore[i]++;
			}
		}
	}
	s->nextserial++;
	assert(s->table->serial <= s->nloc);

	/*
	 *  Fix up d->pserial and d->same
	 */
	if (parentKey) {
		p = sccs_findKey(s, parentKey); /* find parent delta */
		assert(p);
		d->pserial = p->serial;
		d->parent = p;
		d->same = p->same + p->added; /* fix line count */
	}

	/*
	 * Fix up d->serial
	 */
	d->serial = serial;
	assert((d->serial == 0) || (d->serial > d->pserial));

	/*
	 * Save dF info, used by cset_write() later
	 * Note: meta delta have NULL dF
	 */
	if (dF) {
		s->locs[serial].p = dF->where;
		s->locs[serial].len = dF->size;
		s->locs[serial].isPatch = 1;

		/*
		 * Fix up d->added
		 */
		if ((t = dF->where) != 0)  {
			r = t + dF->size - 1;
			while ( t <= r) if (*t++ == '\n') added++;
			d->added = added - 1;
		}
	} else {
		s->locs[serial].p = NULL;
		s->locs[serial].len = 0;
		s->locs[serial].isPatch = 1;
	}

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
	if (syms) freeLines(syms);
}


/*
 * Write out the new ChangeSet file.
 */
int
cset_write(sccs *s)
{
	FILE	*f;
	delta	*d;
	size_t	n;
	u32	sum = 0;
	u8	*p, *e;
	char	buf[100];
	u32	chunk = 0;

	assert(s);
	assert(s->state & S_CSET);
	assert(s->locs);
	unless (f = fopen(sccs_Xfile(s, 'x'), "w")) {
		perror(sccs_Xfile(s, 'x'));
		return (-1);
	}
	s->state |= S_ZFILE|S_PFILE;
	if (delta_table(s, f, 0)) {
		perror("table");
		fclose(f);
		return (-1);
	}
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		unless (s->locs[d->serial].len) continue;

		/*
		 * We want the output to look like:
		 * ^AI serial
		 * key
		 * key
		 * ^AE
		 */
		sprintf(buf, "\001I %u\n", d->serial);
		fputs(buf, f);
		for (p = buf; *p; sum += *p++);

		/*
		 * Patch format is a little different, it looks like
		 * 0a1
		 * > key
		 * > key
		 * 
		 * We need to strip out all the non-key stuff: 
		 * a) "0a0"
		 * b) "> "
		 */
		if (s->locs[d->serial].isPatch) {
			p = s->locs[d->serial].p;
			assert(strneq(p, "0a0\n> ", 6));
			e = p + s->locs[d->serial].len - 1;
			p += 6; /* skip "0a0\n" */
			
			while (1) {
				fputc(*p, f);
				sum += *p;
				if (p == e) break;
				if (*p == '\n') {
					assert(strneq("\n> ", p, 3));
					p += 3; /* skip "> " */
				} else {
					p++;
				}
			}
			
		} else {
			n = fwrite(s->locs[d->serial].p,
						1, s->locs[d->serial].len, f);
			unless (n == s->locs[d->serial].len) {
				perror("fwrite");
				fclose(f);
				return (-1);
			}
			for (p = s->locs[d->serial].p; n--; sum += *p++);
		}
		sprintf(buf, "\001E %u\n", d->serial);
		fputs(buf, f);
		for (p = buf; *p; sum += *p++);
		++chunk;
	}
	strcpy(buf, "\001I 1\n\001E 1\n");
	fputs(buf, f);
	for (p = buf; *p; sum += *p++);
	s->cksum += sum;
	fseek(f, 0L, SEEK_SET);
	fprintf(f, "\001H%05u\n", s->cksum);
	if (fclose(f)) perror(sccs_Xfile(s, 'x'));

	sccs_close(s);
	unlink(s->sfile);
	if (rename(sccs_Xfile(s, 'x'), s->sfile)) {
		perror(s->sfile);
		return (-1);
	}
	return (0);
}
