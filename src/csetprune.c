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

/* set to 1 to not unlink all the files.  Good for debugging */
#define	PRUNE_JUST_CHANGESET	0x10000000
/* restructure tag graph */
#define	PRUNE_NEW_TAG_GRAPH	0x20000000

int
csetprune_main(int ac, char **av)
{
	sccs	*s, *sb;
	char	csetFile[] = "SCCS/s.ChangeSet";
	MDBM	*m = mdbm_mem();
	char	buf[MAXPATH];
	char	*ranbits = 0;
	int	c;

	flags = PRUNE_NEW_TAG_GRAPH; /* |PRUNE_JUST_CHANGESET; */
	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		system("bk help -s csetprune");
		return (1);
	}
	while ((c = getopt(ac, av, "k:qS")) != -1) {
		switch (c) {
		    case 'k': ranbits = optarg; break;
		    case 'q': flags |= SILENT; break;
		    case 'S': flags &= ~PRUNE_NEW_TAG_GRAPH; break;
		    default: goto usage;
		}
	}
	if (ranbits) {
		u8	*p;
		if (strlen(ranbits) != 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option '%s' must have 16 lower case "
			    "hex digits\n", ranbits);
			goto usage;
		}
		for (p = ranbits; *p; p++) {
			unless (isxdigit(*p)) break;
			if (isupper(*p)) break;
		}
		if (*p) goto k_err;
	} else {
		randomBits(buf);
		ranbits = buf;
	}
	if (proj_cd2root()) {
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
		sys("bk", "newroot", "-k", ranbits, SYS);
		sys("bk", "-r", "check", "-ac", SYS);
		exit(0);
	}
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet file\n", av[0]);
		exit(1);
	}
	unless ((sb = sccs_init(csetFile, INIT_NOCKSUM)) && s->tree) {
		fprintf(stderr, "%s: cannot init ChangeSet backup file\n",
			av[0]);
		exit(1);
	}
	verbose((stderr, "Pruning ChangeSet file...\n"));
	sccs_close(sb); /* for win32 */
	pruneEmpty(s, sb, m);
	mdbm_close(m);
	sccs_free(sb);
	s = sccs_init(csetFile, ADMIN_SHUTUP|INIT_NOCKSUM);
	unless (s && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(s, SILENT);
	sccs_newchksum(s);
	sccs_free(s);
	unless ((s = sccs_init(csetFile, INIT_NOCKSUM)) && s->tree) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		exit(1);	/* leave it locked! */
	}
	verbose((stderr, "Serial compressing ChangeSet file...\n"));
	sccs_scompress(s, SILENT);
	sccs_free(s);
	if (flags & PRUNE_JUST_CHANGESET) exit(0);
	verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
	sys("bk", "checksum", "-fv", "ChangeSet", SYS);
	rename("SCCS/s..ChangeSet", "SCCS/b.ChangeSet");
	verbose((stderr, "Generating a new root key and updating files...\n"));
	sys("bk", "newroot", "-k", ranbits, SYS);
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
		if (flags & PRUNE_JUST_CHANGESET) continue;
		verbose((stderr, "%d removed\r", ++n));
		sccs_keyunlink(kv.key.dptr, idDB, dirs);
	}
	mdbm_close(idDB);
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

/*
 * Make the Tag graph mimic the real graph.
 * All symbols are on 'D' deltas, so wire them together
 * based on that graph.  This means making some of the merge
 * deltas into merge deltas on the tag graph.
 * Algorithm uses d->ptag on deltas not in the tag graph
 * to cache graph information.
 * These settings are ignored elsewhere unless d->symGraph is set.
 */

private int
mkTagGraph(sccs *s)
{
	delta	*d, *p, *m;
	int	i;
	int	tips = 0;

	/* in reverse table order */
	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		if (d->flags & D_GONE) continue;

		/* initialize that which might be inherited later */
		d->mtag = d->ptag = 0;

		/* go from real parent to tag parent (and also for merge) */
		if (p = d->parent) {
			unless (p->symGraph) {
				p = (p->ptag) ? sfind(s, p->ptag) : 0;
			}
		}
		m = 0;
		if (d->merge) {
			m = sfind(s, d->merge);
			assert(m);
			unless (m->symGraph) {
				m = (m->ptag) ? sfind(s, m->ptag) : 0;
			}
		}

		/*
		 * p and m are parent and merge in tag graph
		 * this section only deals with adjustments if they
		 * are not okay.
		 */
		/* if only one, have it be 'p' */
		if (!p && m) {
			p = m;
			m = 0;
		}
		/* if both, but one is contained in other: use newer as p */
		if (p && m && found(p, m)) {
			if (m->serial > p->serial) p = m;
			m = 0;
		}
		/* p and m are now as we would like them.  assert if not */
		assert(p || !m);

		/* If this has a symbol, it is in tag graph */
		if (d->flags & D_SYMBOLS) {
			unless (d->symLeaf) tips++;
			d->symGraph = 1;
			d->symLeaf = 1;
		}
		/*
		 * if this has both, then make it part of the tag graph
		 * unless it is already part of the tag graph.
		 * The cover just the 'm' side.  p will be next.
		 */
		if (m) {
			assert(p);
			unless (d->symLeaf) tips++;
			d->symGraph = 1;
			d->symLeaf = 1;
			d->mtag = m->serial;
			if (m->symLeaf) tips--;
			m->symLeaf = 0;
		}
		if (p) {
			d->ptag = p->serial;
			if (d->symGraph) {
				if (p->symLeaf) tips--;
				p->symLeaf = 0;
			}
		}
	}
	return (tips);
}

private void
rebuildTags(sccs *s)
{
	delta	*d, *md;
	symbol	*sym;
	MDBM	*symdb = mdbm_mem();
	int	tips;

	/*
	 * Only keep newest instance of each name
	 * Move all symbols onto real deltas that are not D_GONE
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		md = sym->metad;
		d = sym->d;
		assert(sym->symname && md && d);
		if (mdbm_store_str(symdb, sym->symname, "", MDBM_INSERT)) {
			/* no error, just ignoring duplicates */
			sym->metad = sym->d = 0;
			continue;
		}
		assert(md->type != 'D' || md == d);
		/* If tag on a deleted node, (if parent) move tag to parent */
		if (d->flags & D_GONE) {
			unless (d->parent) {
				/* No where to move it: drop tag */
				sym->metad = sym->d = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(d->parent->flags & D_GONE));
			d = sym->d = d->parent;
		}
		/* Move all tags directly onto real delta */
		if (md != d) {
			md = sym->metad = d;
		}
		assert(md == d && d->type == 'D');
		d->flags |= D_SYMBOLS;
	}
	/*
	 * Symbols are now marked, but not connected.
	 * Prepare structure for building symbol graph.
	 * and D_GONE all 'R' nodes in graph.
	 */
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'R') continue;
		assert(!(d->flags & D_SYMBOLS));
		MK_GONE(s, d);
	}
	tips = mkTagGraph(s);
	verbose((stderr, "Tag graph rebuilt with %d tip%s\n",
		tips, (tips != 1) ? "s" : ""));
	mdbm_close(symdb);
}

/*
 * This is used when we want to keep the tag graph, just make sure
 * it is a valid one.  Need to change 'D' to 'R' for D_GONE'd deltas
 * and make sure real deltas are tagged.
 */
private void
fixTags(sccs *s)
{
	delta	*d, *md;
	symbol	*sym;

	/*
	 * Only keep newest instance of each name
	 * Move all symbols onto real deltas that are not D_GONE
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		md = sym->metad;
		d = sym->d;
		assert(sym->symname && md && d);
		assert(md->type != 'D' || md == d);
		/*
		 * If tags a deleted node, (if parent) move tag to parent
		 * XXX: do this first, as md check can clear D_GONE flag.
		 */
		if (d->flags & D_GONE) {
			unless (d->parent) {
				/* No where to move it: drop tag */
				/* XXX: Can this ever happen?? */
				fprintf(stderr,
				    "csetprune: Tag (%s) on pruned revision "
				    "(%s) will be removed,\nbecause the "
				    "revision has no parent to receive the "
				    "tag.\nPlease write support@bitmover.com "
				    "describing what you did to get this "
				    "message.\nThis is a warning message, "
				    "not a failure.\n", sym->symname, d->rev);
				sym->metad = sym->d = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(d->parent->flags & D_GONE));
			d = sym->d = d->parent;
			d->flags |= D_SYMBOLS;
			md->parent = d;
			md->pserial = d->serial;
		}
		/* If tag is deleted node, make into a 'R' node */
		if (md->flags & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(md->type == 'D');
			md->type = 'R';
			md->flags &= ~(D_GONE|D_CKSUM|D_CSET);
			md->added = md->deleted = md->same = 0;
			if (md->comments) {
				freeLines(md->comments, free);
				md->comments = 0;
			}
			assert(!md->include && !md->exclude && !md->merge);
		}
	}
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
	MK_GONE(sc, d);
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
	 * Mark the empty deltas, possibly reset tag tree structure
	 */
	for (n = s->table; n; n = n->next) {
		unless (n->type == 'R') {
			char	buf[100];

			sprintf(buf, "%u", n->serial);
			if (mdbm_fetch_str(m, buf)) {
				n->added = 0;
			}
		}
		if (flags & PRUNE_NEW_TAG_GRAPH) {
			n->ptag = n->mtag = 0;
			n->symGraph = 0;
			n->symLeaf = 0;
			n->flags &= ~D_SYMBOLS;
		}
	}
	sc = s;
	scb = sb;
	_pruneEmpty(sc->table);
	verbose((stderr, "Rebuilding Tag Graph...\n"));
	(flags & PRUNE_NEW_TAG_GRAPH) ? rebuildTags(sc) : fixTags(sc);
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
