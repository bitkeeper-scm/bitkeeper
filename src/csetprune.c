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
 * Walk table order and remove all tag information.
 * Walk "prune" with Rick's OK-to-be-gone alg, which fixes the pointers.
 * Walk sym and table order and rebuild tag information.
 * Write out the graph with the normal delta_table().
 * Write out the body, skipping all deltas marked as D_GONE.
 * Free prune, but not pristine.
 * Reinit the file into cset2, scompress it, write it, reinit again.
 * Walk the graph recursively and apply from root forward.
 * Create a new root key.
 * 
 * Copyright (c) 2001 Larry McVoy & Rick Smith
 */

int	csetprune_main(int ac, char **av);
private	int rmKeys(MDBM *s);
private	int found(delta *start, delta *stop);
private void _pruneEmpty(delta *d);
private void pruneEmpty(sccs *s, sccs *sb, MDBM *m);
private int getKeys(MDBM *m);
private int flags;

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
		sys("bk", "newroot", SYS);
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
	sccs_close(sb); /* for win32 */
	pruneEmpty(s, sb, m);
	mdbm_close(m);
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

private int
found(delta *start, delta *stop)
{
	delta	*d;

	assert(start && stop);
	if (start == stop) return (1);
	if (start->serial < stop->serial) {
		d = start;
		start = stop;
		stop = d;
	}

	for (d = start->next; d && d != stop; d = d->next) d->flags &= ~D_RED;
	start->flags |= D_RED;
	stop->flags &= ~D_RED;

	for (d = start; d && d != stop; d = d->next) {
		unless (d->flags & D_RED) continue;
		if (d->parent) d->parent->flags |= D_RED;
		if (d->merge) sfind(sc, d->merge)->flags |= D_RED;
	}
	return ((stop->flags & D_RED) != 0);
}

private void
rebuildTags(sccs *s)
{
	delta	*d, *md;
	delta	*last = 0;
	symbol	*sym;
	MDBM	*symdb = mdbm_mem();

	/*
	 * Only keep newest instance of each name
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		assert(sym->symname && sym->metad && sym->d);
		if (mdbm_store_str(symdb, sym->symname, "", MDBM_INSERT)) {
			/* no error, just ignoring duplicates */
			sym->metad = sym->d = 0;
			continue;
		}
		md = sym->metad;
		d = sym->d;
		assert(md->type != 'D' || md == d);
		/* If tag on a deleted node, (if parent) move tag to parent */
		if (d->flags & D_GONE) {
			unless (d->parent) {
				sym->metad = sym->d = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(d->parent->flags & D_GONE));
			d = sym->d = d->parent;
		}
		/* Move all tags directly onto delta, delete all "R" deltas */
		if (md != d) {
			md->flags |= D_GONE;
			md = sym->metad = d;
		}
		md->flags |= D_SYMBOLS;
	}
	/*
	 * symbols above could now be out of order.
	 * Use table order to build tag graph.
	 */
	for (d = s->table; d; d = d->next) {
		unless (d->flags & D_SYMBOLS) continue;
		assert(!(d->flags & D_GONE));
		if (last) {
			last->ptag = d->serial;
		} else {
			d->symLeaf = 1;
		}
		last = d;
		d->symGraph = 1;
	}
	mdbm_close(symdb);
}

/*
 * We maintain d->parent, d->merge, and d->pserial
 * We do not maintain d->kid, d->sibling, or d->flags & D_MERGED
 *
 * Which means, don't call much else after this, just get the file
 * written to disk!
 */
private void
_pruneEmpty(delta *d)
{
	delta	*m;

	/*
	 * Build reverse table order on stack,
	 * but skip over tag deltas
	 */
	for ( ; d ; d = d->next) {
		if (d->type == 'D') break;
	}
	unless (d && d->next) return;	/* don't prune root */
	_pruneEmpty(d->next);

	debug((stderr, "%s ", d->rev));
	if (d->merge) {
		debug((stderr, "\n"));
		/*
		 * cases which can happen:
		 * a) all nodes up both parents are D_GONE until C.A.
		 *    => merge and parent are same!  Remove merge marker
		 * b) all on the trunk are gone, but not the merge
		 *    => make merge the parent
		 * c) all on the merge are gone, but not the trunk
		 *    => remove merge marker
		 * d) non-gones in both, trunk and branch are backwards
		 *    => swap trunk and branch
		 * e) non-gones in both, trunk and branch are oriented okay
		 *    => do nothing
		 *
		 * First look to see if one is 'found' in ancestory of other.
		 * If so, the merge collapses (cases a, b, and c)
		 * Else, check for case d, else do nothing for case e.
		 *
		 * Then fix up those pesky include and exclude lists.
		 */
		m = sfind(sc, d->merge);
		if (found(d->parent, m)) {	/* merge collapses */
			if (d->merge > d->pserial) {
				d->parent = m;
				d->pserial = d->merge;
			}
			d->merge = 0;
		}
		/* else if merge .. (chk case d and e) */
		else if (sccs_needSwap(d->parent, m)) {
			d->parent = m;
			d->merge = d->pserial;
			d->pserial = d->parent->serial;
		}
		/* fix include and exclude lists */
		sccs_adjustSet(sc, scb, d);
		/* for now ... remove later if cset -i allowed in merge */
		assert((d->merge && d->include) || (!d->merge && !d->include));
		assert(!d->exclude);
	}
	/* Else this never was a merge node, so just adjust inc and exc */
	else if (d->include || d->exclude) {
		sccs_adjustSet(sc, scb, d);
	}
	/*
	 * See if node is a keeper ...
	 */
	if (d->added || d->merge || d->include || d->exclude) return;

	/* Not a keeper, so re-wire around it */
	debug((stderr, "RMDELTA(%s)\n", d->rev));
	d->flags |= D_GONE;
	assert(d->parent);	/* never get rid of root node */
	if (d->flags & D_MERGED) {
		for (m = sc->table; m && m->serial > d->serial; m = m->next) {
			unless (m->merge == d->serial) continue;
			debug((stderr,
			    "%s gets new merge parent %s (was %s)\n",
			    m->rev, d->parent->rev, d->rev));
			m->merge = d->pserial;
		}
	}
	for (m = d->kid; m; m = m->siblings) {
		unless (m->type == 'D') continue;
		debug((stderr, "%s gets new parent %s (was %s)\n",
			    m->rev, d->parent->rev, d->rev));
		m->parent = d->parent;
		m->pserial = d->pserial;
	}
	return;
}

private void
pruneEmpty(sccs *s, sccs *sb, MDBM *m)
{
	delta	*n;

	/*
	 * Mark the empty deltas, reset tag tree structure
	 * Delete 'R' tags with no symbols (tag graph merge nodes)
	 */
	for (n = s->table; n; n = n->next) {
		unless (n->type == 'R') {
			char	buf[100];

			sprintf(buf, "%u", n->serial);
			if (mdbm_fetch_str(m, buf)) {
				n->added = 0;
			}
		}
		else unless (n->flags & D_SYMBOLS) {
			n->flags &= D_GONE;
		}
		n->ptag = n->mtag = 0;
		n->symGraph = 0;
		n->symLeaf = 0;
		n->flags &= ~D_SYMBOLS;
	}
	sc = s;
	scb = sb;
	_pruneEmpty(sc->table);
	rebuildTags(sc);
	sccs_reDup(sc);
	sccs_newchksum(sc);
	sccs_free(sc);
}

private int
getKeys(MDBM *m)
{
	char	buf[MAXKEY];
	char	*gname, *gname_end;

	while (fnext(buf, stdin)) {
		unless (chop(buf) == '\n') {
			fprintf(stderr, "Bad input, no newline: %s\n", buf);
			return (1);
		}
		assert(sccs_iskeylong(buf));
		/* Filter out of list : important BK files */
		for (gname = buf; *gname; gname++) {
			if (*gname == '|') break;
		}
		assert(*gname == '|');
		gname++;
		for (gname_end = gname; *gname_end; gname_end++) {
			if (*gname_end == '|') break;
		}
		assert(*gname_end == '|');
		*gname_end = '\0';
		if (streq(gname, GCHANGESET) || strneq(gname, "BitKeeper/", 10))
		{
			verbose((stderr, "Keeping %s\n", gname));
			*gname_end = '|';
			continue;
		}
		*gname_end = '|';
		if (mdbm_store_str(m, buf, "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			return (1);
		}
	}
	return (0);
}
