#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * csetprune - prune a list of files from a ChangeSet file
 *
 * Save the ChangeSet file to SCCS/s..ChangeSet.
 * Read the list of keys from stdin and put them in an mdbm.
 * Walk the prune body and process each ^AI/^AE pair
 *	foreach line in block {
 *		if the rootkey is not in the mdbm, return 0
 *	}
 *	return 1
 * If that returned 1 then the whole delta is gone, mark the delta with D_GONE.
 * Walk "prune" with Rick's OK-to-be-gone alg, which fixes the pointers.
 * Walk "prune" and remove all tag information.
 * Write out the graph with the normal delta_table().
 * Write out the body, skipping all deltas marked as D_GONE.
 * Free prune, but not pristine.
 * Reinit the file into cset2, scompress it, write it, reinit again.
 * Walk pristine's tag graph and move the tags to the first real delta
 * found in the the shrunk file.
 * Walk the graph recursively and apply from root forward.
 * Create a new root key.
 * 
 * Copyright (c) 2001 Larry McVoy & Rick Smith
 */

int	csetsplit_main(int ac, char **av);
int	rmKeys(MDBM *m, MDBM *s);
private	int found(delta *d);
private void _pruneEmpty(delta *d);
private void pruneEmpty(sccs *s, sccs *sb, MDBM *m);
private int getKeys(MDBM *m);

/* XXX: get the slib.c prototype out of here! */
extern void sccs_adjustSet(sccs *sc, sccs *scb, delta *d);

int
csetsplit_main(int ac, char **av)
{
	sccs	*s, *sb;
	char	csetFile[] = "SCCS/s.ChangeSet";
	MDBM	*m1 = mdbm_mem();
	MDBM	*m2 = mdbm_mem();

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help -s csetsplit");
		return (0);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "%s: cannot find package root\n", av[0]);
		exit(1);
	}
	if (exists("SCCS/s..ChangeSet")) {
		fprintf(stderr, "Will not overwrite backup changeset file.\n");
		exit(1);
	}
	if (getKeys(m1)) exit(1);
	sys("bk", "admin", "-Znone", "SCCS/s.ChangeSet", SYS);
	rename("SCCS/s.ChangeSet", "SCCS/s..ChangeSet");
	unless (rmKeys(m1, m2)) {
		fprintf(stderr, "Wow!  No empty deltas, great!\n");
		sys("bk", "admin", "-z", "ChangeSet", SYS);
		sys("bk", "renumber", "ChangeSet", SYS);
		exit(sys("bk", "checksum", "-fvv", "ChangeSet", SYS));
	}
	m1 = 0;	/* it was closed */

	unless ((s = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet file\n", av[0]);
		exit(1);
	}

	unless ((sb = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet backup file\n",
			av[0]);
		exit(1);
	}
	pruneEmpty(s, sb, m2);
	sccs_free(sb);

	unless ((s = sccs_init(csetFile, ADMIN_SHUTUP|INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	sccs_renumber(s, 0, 0, 0, 0, 0);
	sccs_newchksum(s);
	sccs_free(s);
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	sccs_scompress(s, SILENT);
	sccs_free(s);
	sys("bk", "checksum", "-fv", "ChangeSet", SYS);
#if 0
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	//transferTags(pristine, m2);
	sccs_newchksum(s);
	sccs_free(s);
#endif
	rename("SCCS/s..ChangeSet", "SCCS/b.ChangeSet");
	exit(0);		/* bitchin' */
}

/*
 * rmKeys - remove the keys and build a new file.
 */
rmKeys(MDBM *m, MDBM *s)
{
	char	line[MAXKEY*2];
	FILE	*in = fopen("SCCS/s..ChangeSet", "r");
	FILE	*out = fopen("SCCS/s.ChangeSet", "w");
	ser_t	ser;
	int	first;
	char	*t;
	int	empty = 0;

	/*
	 * Do the top half
	 */
	while (fnext(line, in)) {
		fputs(line, out);
		if (streq("\001T\n", line)) break;
	}

	/*
	 * Do the weave.
	 */
	for (;;) {
		unless (fnext(line, in) && strneq(line, "\001I ", 3)) {
			fprintf(stderr, "Bad weave data: '%s'\n", line);
			exit(1);
		}
		ser = atoi(&line[3]);
		assert(ser > 0);
		first = 1;
		line[0] = 0;
		while (fnext(line, in)) {
			if (strneq(line, "\001E ", 3)) break;
			t = separator(line);
			assert(t);
			*t = 0;
			if (mdbm_fetch_str(m, line)) continue;
			if (first) {
				fprintf(out, "\001I %u\n", ser);
				first = 0;
			}
			*t = ' ';
			fputs(line, out);
			line[0] = 0;
		}
		unless (line[0]) {
done:			fclose(in);
			fclose(out);
			mdbm_close(m);
fprintf(stderr, "%d empty deltas\n", empty);
			return (empty);
		}
		if (ser == 1) {
			assert(first);
			assert(!fnext(line, in));
			fprintf(out, "\001I 1\n\001E 1\n");
			goto done;
		}
		if (first) {	/* we pushed out nothing */
			sprintf(line, "%u", ser);
			mdbm_store_str(s, line, "", MDBM_INSERT);
			empty++;
		} else {
			fprintf(out, "\001E %u\n", ser);
		}
	}
	/* NOTREACHED */
}

/*
 * Alg from Rick:
 * process in reverse table order (oldest..newest)
 * if (merge) {
 *	find GCA
 *	see if all ancestors are marked gone up either branch
 *	if not, return
 *	pruneMerge() {
 *		remove merge info
 *		merge parent may become parent pointer
 *	}
 * }
 * if (added) return;
 * if all deltas in include/exclude are marked gone then {
 *	delete() {
 *		mark this as gone
 *		fix up the parent pointer of all kids
 *	}
 * }
 *
 * also remove tags, write it out, and free the sccs*.
 */
private sccs *sc, *scb;
private delta *findme;

private int
found(delta *d)
{
	unless (d && !(d->flags & D_RED)) return (0);
	d->flags |= D_RED;
	if (d == findme) return (1);
	if ((d->serial > findme->serial) && found(d->parent)) return (1);
	if (d->merge >= findme->serial) return (found(sfind(sc, d->merge)));
	return (0);
}

private void
pruneList(ser_t *list)
{
	int	i;
	int	shift;
	
	unless (list) return;
	shift = 0;
	EACH(list) {
		unless (sfind(sc, list[i])->flags & D_GONE) {
			unless (shift) continue;
			list[i-shift] = list[i];
			list[i] = 0;
			continue;
		}
		list[i] = 0;
		shift++;
	}
}

private void
_pruneEmpty(delta *d)
{
	int	i;

	unless (d && d->parent && (d->type == 'D')) return;
	_pruneEmpty(d->next);
fprintf(stderr, "%s ", d->rev);
	pruneList(d->include);
	pruneList(d->exclude);
	if (d->merge) {
		delta	*e, *m;

fprintf(stderr, "\n");
		/*
		 * cases which can happen:
		 * a) all nodes up both parents are D_GONE until C.A.
		 * b) all on the trunk are gone, but not the merge
		 * c) all on the merge are gone, but not the trunk
		 * d) non-gones in both
		 *
		 * Given a parent and merge, the smaller serial is older.
		 * Set findme to the older node.
		 * Recurse up the other node and see if you find findme.
		   If (found) {
		  	if (older was merge) {
		  		remove merge and include list
		 		after asserting that all in include are D_GONE
		  	} else {
		 		the merge node becomes the parent node
				and the merge pointer and include list
				are removed
		  	}
		   }
		 */
		
		for (m = sc->table; m; m = m->next) m->flags &= ~D_RED;
		m = sfind(sc, d->merge);
		if (m->serial > d->parent->serial) {	/* parent is older */
			findme = d->parent;
//fprintf(stderr, "findme=%s d=%s m=%s\n", findme->rev, d->rev, m->rev);
			unless (found(m)) goto check;
			/* merge node becomes the parent */
fprintf(stderr, "%s gets new parent %s (M)\n", d->rev, m->rev);
			d->parent = m;
			d->pserial = m->serial;
		} else {				/* merge is older */
			findme = m;
			unless (found(d->parent)) goto check;
			EACH(d->include) {
			    assert("Collapsing merge with includes left" == 0);
			}
		}

		/*
		 * One might think that this is bogus since they could have
		 * done a cset -i.  It's not because we are in a merge delta
		 * and we create those in the RESYNC dir and there is no way
		 * to do a cset -i there.
		 */
		assert(!d->exclude);
		assert(d->include);
		free(d->include);
		d->include = 0;
		d->merge = 0;
	}
	/*
	 * May be a list with nothing in it, pruned above.
	 */
	EACH(d->include) goto check;
	EACH(d->exclude) goto check;
	if (d->added) return;		/* Note: do this after inc/exc check */
fprintf(stderr, "RMDELTA(%s)\n", d->rev);
	d->flags |= D_GONE;
	if (d->flags & D_MERGED) {
		delta	*m;
		int	i;

		assert(d->parent);
		for (m = sc->table; m; m = m->next) {
			unless (m->merge == d->serial) continue;
fprintf(stderr, "%s gets new merge parent %s (was %s)\n", m->rev, d->parent->rev, d->rev);
			m->merge = d->parent->serial;
			d->parent->flags |= D_MERGED;
		}
	}
	for (d = d->kid; d; d = d->siblings) {
		unless (d->type == 'D') continue;
		if (d->parent->parent) {
fprintf(stderr, "%s gets new parent %s\n", d->rev, d->parent->parent->rev);
			d->parent = d->parent->parent;
			d->pserial = d->parent->serial;
		} else {
			d->pserial = 0;
		}
	}
	return;

check:	/* for unpruned nodes that are merge or have includes or excludes */

	sccs_adjustSet(sc, scb, d);
}

private void
pruneEmpty(sccs *s, sccs *sb, MDBM *m)
{
	delta	*n, *p;

	/*
	 * Strip the tags.
	 * We don't have to worry about the pointers because we are just
	 * dumping all the removed deltas.
	 */
//fprintf(stderr, "Empty: ");
	for (p = 0, n = s->table; n; ) {
		if (n->type == 'R') {
			((p) ? p->next : s->table) = n->next;
			n->flags |= D_GONE;
		} else {
			char	buf[100];

			sprintf(buf, "%u", n->serial);
			if (mdbm_fetch_str(m, buf)) {
				n->added = 0;
//fprintf(stderr, "%s ", n->rev);
			}
			p = n;
		}
		n->ptag = n->mtag = 0;
		n->symGraph = 0;
		n->symLeaf = 0;
		n->flags &= ~D_SYMBOLS;
		n = n->next;
	}
//fprintf(stderr, "\n");
	sc = s;
	scb = sb;
	_pruneEmpty(sc->table);
	sccs_newchksum(sc);
	sccs_free(sc);
}

private int
getKeys(MDBM *m)
{
	char	buf[MAXKEY];

	while (fnext(buf, stdin)) {
		unless (chop(buf) == '\n') {
			fprintf(stderr, "Bad input, no newline: %s\n", buf);
			return (1);
		}
		if (mdbm_store_str(m, buf, "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			return (1);
		}
	}
	return (0);
}
