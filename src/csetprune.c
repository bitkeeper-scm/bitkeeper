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

int	csetprune_main(int ac, char **av);
private	int rmKeys(MDBM *s);
private	int found(delta *d);
private void _pruneEmpty(delta *d);
private void pruneEmpty(sccs *s, sccs *sb, MDBM *m);
private int getKeys(MDBM *m);
private int flags;

/* XXX: get the slib.c prototype out of here! */
extern void sccs_adjustSet(sccs *sc, sccs *scb, delta *d);

int
csetprune_main(int ac, char **av)
{
	sccs	*s, *sb;
	char	csetFile[] = "SCCS/s.ChangeSet";
	MDBM	*m = mdbm_mem();

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help -s csetprune");
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
	unless (rmKeys(m)) {
		rename("SCCS/s..ChangeSet", "SCCS/b.ChangeSet");
		sys("bk", "admin", "-z", "ChangeSet", SYS);
		sys("bk", "renumber", "-q", "ChangeSet", SYS);
		sys("bk", "checksum", "-fv", "ChangeSet", SYS);
		sys("bk", "-r", "check", "-ac", SYS);
		exit(0);
	}
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet file\n", av[0]);
		exit(1);
	}
	unless ((sb = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet backup file\n",
			av[0]);
		exit(1);
	}
	verbose((stderr, "Pruning ChangeSet file...\n"));
	pruneEmpty(s, sb, m);
	sccs_free(sb);
	s = sccs_init(csetFile, ADMIN_SHUTUP|INIT_NOCKSUM, 0);
	unless (s && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(s, 0, 0, 0, 0, SILENT);
	sccs_newchksum(s);
	sccs_free(s);
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM, 0)) && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	verbose((stderr, "Serial compressing ChangeSet file...\n"));
	sccs_scompress(s, SILENT);
	sccs_free(s);
	verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
	sys("bk", "checksum", "-fv", "ChangeSet", SYS);
	rename("SCCS/s..ChangeSet", "SCCS/b.ChangeSet");
	verbose((stderr, "Generating a new root key and updating files...\n"));
	sys("bk", "newroot", SYS);
	verbose((stderr, "Running a check -ac...\n"));
	if (sys("bk", "-r", "check", "-ac", SYS)) exit(1);
	verbose((stderr, "All operations completed.\n"));
	exit(0);		/* bitchin' */
}

/*
 * rmKeys - remove the keys and build a new file.
 */
private int
rmKeys(MDBM *s)
{
	char	line[MAXKEY*2];
	FILE	*in, *out;
	ser_t	ser;
	int	first;
	char	*t;
	int	empty = 0;
	int	n = 0;
	MDBM	*m = mdbm_mem();
	MDBM	*dirs = mdbm_mem();
	MDBM	*idDB;
	kvpair	kv;
	project	*proj = proj_init(0);

	verbose((stderr, "Reading keys...\n"));
	if (getKeys(m)) exit(1);

	/*
	 * Remove each file.
	 */
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	verbose((stderr, "Removing files...\n"));
	for (kv = mdbm_first(m); kv.key.dsize; kv = mdbm_next(m)) {
		verbose((stderr, "%d removed\r", ++n));
		sccs_keyunlink(kv.key.dptr, proj, idDB, dirs);
	}
	mdbm_close(idDB);
	proj_free(proj);
	verbose((stderr, "\n"));

	verbose((stderr, "Removing directories...\n"));
	for (kv = mdbm_first(dirs); kv.key.dsize; kv = mdbm_next(dirs)) {
		if (isdir(kv.key.dptr)) sccs_rmEmptyDirs(kv.key.dptr);
	}
	mdbm_close(dirs);

	verbose((stderr, "Processing ChangeSet file...\n"));
	sys("bk", "admin", "-Znone", "SCCS/s.ChangeSet", SYS);
	rename("SCCS/s.ChangeSet", "SCCS/s..ChangeSet");
	in = fopen("SCCS/s..ChangeSet", "r");
	out = fopen("SCCS/s.ChangeSet", "w");

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
			assert(sccs_iskeylong(line));
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
			debug((stderr, "%d empty deltas\n", empty));
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
unDup(sccs *s)
{
	delta	*d;

#define	UNDUP(field, flag, str) \
	if (d->flags & flag) { \
		d->field = strdup(d->field); \
		d->flags &= ~flag; \
	}

	for (d = s->table; d; d = d->next) {
		UNDUP(pathname, D_DUPPATH, "path");
		UNDUP(hostname, D_DUPHOST, "host");
		UNDUP(zone, D_DUPZONE, "zone");
		UNDUP(csetFile, D_DUPCSETFILE, "csetFile");
		UNDUP(symlink, D_DUPLINK, "symlink");
	}
#undef	UNDUP
}

/*
 * Find all the tags associated with this delta and move them to the
 * parent, if there is a parent.  Otherwise delete them.
 */
private void
moveTags(delta *d)
{
	symbol	*sym;

	for (sym = sc->symbols; sym; sym = sym->next) {
		unless (sym->metad->type == 'R') continue;
		unless (sym->d == d) continue;
		unless (d->parent) {
			sym->d = 0;
			continue;
		}
		debug((stderr,
		    "TAG %s moved from %s to %s\n",
		    sym->symname, d->rev, d->parent->rev));
		sym->d = d->parent;
		d->parent->flags |= D_SYMBOLS;
		free(sym->metad->rev);
		sym->metad->rev = strdup(d->parent->rev);
		/* I'm not positive they are all bushy */
		assert(sym->metad->pserial == d->serial);
		sym->metad->pserial = d->parent->serial;
	}
}

private void
rebuildTags(sccs *s)
{
	delta	*d;
	delta	*last = 0;
	symbol	*sym, *sym2;

	/*
	 * Remove duplicate syms on the same real delta.
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		for (sym2 = s->symbols; sym2; sym2 = sym2->next) {
			if (sym == sym2) continue;
			unless ((sym->d == sym2->d) &&
			    streq(sym->symname, sym2->symname)) {
			    	continue;
			}
			sym2->d = sym2->metad = 0;
		}
	}
	
	/*
	 * Remove all the tag only deltas and rebuild the tag graph
	 */
	for (d = s->table; d; d = d->next) {
		for (sym = s->symbols; sym; sym = sym->next) {
			if (sym->metad == d) break;
		}
		if (sym) {
			if (last) {
				last->ptag = d->serial;
			} else {
				d->symLeaf = 1;
			}
			last = d;
			d->symGraph = 1;
		} else if (d->type == 'R') {
			d->flags |= D_GONE;
		}
	}
}

private void
_pruneEmpty(delta *d)
{
	int	i;

	unless (d && d->parent) return;
	_pruneEmpty(d->next);
	unless (d->type == 'D') return;
	debug((stderr, "%s ", d->rev));
	pruneList(d->include);
	pruneList(d->exclude);
	if (d->merge) {
		delta	*m;

		debug((stderr, "\n"));
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
			unless (found(m)) goto check;
			/* merge node becomes the parent */
			debug((stderr,
			    "%s gets new parent %s (M)\n", d->rev, m->rev));
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
	debug((stderr, "RMDELTA(%s)\n", d->rev));
	d->flags |= D_GONE;
	if (d->flags & D_SYMBOLS) moveTags(d);
	if (d->flags & D_MERGED) {
		delta	*m;

		assert(d->parent);
		for (m = sc->table; m; m = m->next) {
			unless (m->merge == d->serial) continue;
			debug((stderr,
			    "%s gets new merge parent %s (was %s)\n",
			    m->rev, d->parent->rev, d->rev));
			m->merge = d->parent->serial;
			d->parent->flags |= D_MERGED;
		}
	}
	for (d = d->kid; d; d = d->siblings) {
		unless (d->type == 'D') continue;
		if (d->parent->parent) {
			debug((stderr,
			    "%s gets new parent %s\n",
			    d->rev, d->parent->parent->rev));
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
	delta	*n;

	unDup(s);

	/*
	 * Mark the empty deltas.
	 */
	for (n = s->table; n; n = n->next) {
		unless (n->type == 'R') {
			char	buf[100];

			sprintf(buf, "%u", n->serial);
			if (mdbm_fetch_str(m, buf)) {
				n->added = 0;
			}
		}
		n->ptag = n->mtag = 0;
		n->symGraph = 0;
		n->symLeaf = 0;
	}
	sc = s;
	scb = sb;
	_pruneEmpty(sc->table);
	rebuildTags(sc);
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
		assert(sccs_iskeylong(buf));
		if (mdbm_store_str(m, buf, "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			return (1);
		}
	}
	return (0);
}
