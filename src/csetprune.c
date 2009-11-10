#include "system.h"
#include "sccs.h"
#include "bam.h"

/*
 * csetprune - prune a list of files from a ChangeSet file
 *
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

private	int	csetprune(hash *prunekeys,
		    char *comppath, char **complist, char *ranbits);
private	int	filterWeave(sccs *cset, char **cweave, char ***delkeys,
		    hash *prunekeys, char *comppath, char **complist);
private	int	rmKeys(char **delkeys);
private	int	found(delta *start, delta *stop);
private	void	_pruneEmpty(delta *d);
private	void	pruneEmpty(sccs *s, sccs *sb);
private	hash	*getKeys(void);
private	char	**goneKeys(void);
private	int	keeper(char *rk);

private	int	do_file(sccs *s, char *path, char **deep);
private	char	**deepPrune(char **map, char *path);
private	char	*newname( char *delpath, char *comp, char *path, char **deep);
private	char	*s2delpath(sccs *s);
private	char	*rootkey2delpath(char *key);
private	char	*getPath(char *key, char **term);
private	int	do_files(char *comppath, char **deepnest);

private	int	flags;

/* restructure tag graph */
#define	PRUNE_GONE		0x10000000	/* use gone file as list */
#define	PRUNE_NEW_TAG_GRAPH	0x20000000	/* move tags to real deltas */
#define	PRUNE_NO_SCOMPRESS	0x40000000	/* leave serials alone */
#define	PRUNE_XCOMP		0x80000000	/* prune cross comp moves */
#define	PRUNE_LVEMPTY		0x01000000	/* leave empty nodes in graph */
#define	PRUNE_ALL		0x02000000	/* prune all user says to */

int
csetprune_main(int ac, char **av)
{
	char	*ranbits = 0;
	char	*comppath = 0;
	char	*compfile = 0;
	char	**complist = 0;
	hash	*prunekeys = 0;
	char	**gonekeys = 0;
	int	i, c, ret = 1;

	flags = PRUNE_NEW_TAG_GRAPH;
	while ((c = getopt(ac, av, "ac:C:Egk:NqsSX")) != -1) {
		switch (c) {
		    case 'a': flags |= PRUNE_ALL; break;
		    case 'c': comppath = optarg; break;
		    case 'C': compfile = optarg; break;
		    case 'E': flags |= PRUNE_LVEMPTY; break;
		    case 'g': flags |= PRUNE_GONE; break;
		    case 'k': ranbits = optarg; break;
		    case 'N': flags |= PRUNE_NO_SCOMPRESS; break;
		    case 'q': flags |= SILENT; break;
		    case 'S': flags &= ~PRUNE_NEW_TAG_GRAPH; break;
		    case 'X': flags |= PRUNE_XCOMP; break;
		    default:
usage:			system("bk help -s csetprune");
			return (1);
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
			if (!isxdigit(*p) || isupper(*p)) goto k_err;
		}
	}
	/*
	 * Backward compat -- fake '-' if no new stuff specified
	 */
	if ((!comppath && !compfile && !(flags & PRUNE_GONE)) ||
	    ((optind < ac) && streq(av[optind], "-"))) {
		unless (prunekeys = getKeys()) goto err;
	}
	if (compfile) {
		unless (complist = file2Lines(0, compfile)) {
			fprintf(stderr, "%s: missing complist file\n", prog);
			goto err;
		}
		sortLines(complist, string_sort);
	}
	if (proj_cd2root()) {
		fprintf(stderr, "%s: cannot find package root\n", prog);
		goto err;
	}
	if ((flags & PRUNE_GONE) && (gonekeys = goneKeys())) {
		EACH (gonekeys) {
			// Rick's removeLineN trick
			unless (gonekeys[i][0]) continue;
			unless (prunekeys) prunekeys = hash_new(HASH_MEMHASH);
			hash_insertStr(prunekeys, gonekeys[i], 0);
		}
		freeLines(gonekeys, free);
	}
	if (csetprune(prunekeys, comppath, complist, ranbits)) {
		fprintf(stderr, "%s: failed\n", prog);
		goto err;
	}
	ret = 0;

err:
	if (prunekeys) hash_free(prunekeys);
	if (complist) freeLines(complist, free);
	return (ret);
}

private	int
csetprune(hash *prunekeys, char *comppath, char **complist, char *ranbits)
{
	int	empty_nodes = 0, ret = 1;
	delta	*d;
	char	**delkeys = 0;
	sccs	*cset = 0, *csetb = 0;
	char	**cweave = 0;
	char	**deepnest = 0;
	char	buf[MAXPATH];

	unless (ranbits) {
		randomBits(buf);
		ranbits = buf;
	}

	unless (cset = sccs_csetInit(0)) {
		fprintf(stderr, "csetinit failed\n");
		goto err;
	}
	for (d = cset->table; d; d = d->next) d->flags |= D_SET;
	cset->state |= S_SET;
	if ((cweave = cset_mkList(cset)) == (char **)-1) {
		fprintf(stderr, "cset_mkList failed\n");
		goto err;
	}
	deepnest = deepPrune(complist, comppath);
	empty_nodes = filterWeave(
	    cset, cweave, &delkeys, prunekeys, comppath, deepnest);
	if (empty_nodes == -1) {
		fprintf(stderr, "filterWeave failed\n");
		goto err;
	}
	if (sccs_csetWrite(cset, cweave)) goto err;
	sccs_free(cset);
	cset = 0;
	freeLines(cweave, free);
	cweave = 0;

	rmKeys(delkeys);
	if (delkeys) freeLines(delkeys, free);
	delkeys = 0;
	if (empty_nodes == 0) goto finish;
	if (flags & PRUNE_LVEMPTY) goto finish;

	unless ((cset = sccs_csetInit(INIT_NOCKSUM)) && HASGRAPH(cset)) {
		fprintf(stderr, "%s: cannot init ChangeSet file\n", prog);
		goto err;
	}
	unless ((csetb = sccs_csetInit(INIT_NOCKSUM)) && HASGRAPH(csetb)) {
		fprintf(stderr,
		    "%s: cannot init ChangeSet backup file\n", prog);
		goto err;
	}
	verbose((stderr, "Pruning ChangeSet file...\n"));
	sccs_close(csetb); /* for win32 */
	pruneEmpty(cset, csetb);
	sccs_free(csetb);
	csetb = 0;
	unless ((cset = sccs_csetInit(INIT_WACKGRAPH|INIT_NOCKSUM)) &&
	    HASGRAPH(cset)) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		goto err;	 /* leave it locked! */
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(cset, SILENT, 0);
	sccs_newchksum(cset);
	sccs_free(cset);
	cset = 0;
	if (flags & PRUNE_NO_SCOMPRESS) goto finish;
	unless ((cset = sccs_csetInit(INIT_WACKGRAPH|INIT_NOCKSUM)) &&
	    HASGRAPH(cset)) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		goto err;	/* leave it locked! */
	}
	verbose((stderr, "Serial compressing ChangeSet file...\n"));
	sccs_scompress(cset, SILENT);
	sccs_free(cset);
	cset = 0;
finish:
	if (do_files(comppath, deepnest)) goto err;
	verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
	sys("bk", "checksum", "-f", "ChangeSet", SYS);
	verbose((stderr,
	    "Generating a new root key and updating files...\n"));
	sys("bk", "newroot", "-qycsetprune command", "-k", ranbits, SYS);
	verbose((stderr, "Running a check -ac...\n"));
	if (sys("bk", "-r", "check", "-ac", SYS)) goto err;
	verbose((stderr, "All operations completed.\n"));
	ret = 0;

err:	if (cset) sccs_free(cset);
	if (csetb) sccs_free(csetb);
	if (delkeys) freeLines(delkeys, free);
	/* ptrs into complist, don't free */
	if (deepnest) freeLines(deepnest, 0);
	return (ret);
}

/*
 * sort by whole rootkey, then serial
 * If that is not unique, then we have a rootkey in a serial twice(?)
 * Order from low serial to high serial
 * lines:
 *    <serial>\t<rootkey> <deltakey>
 */

private	int
sortByKeySer(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1 = strchr(s1, '\t');	/* path in rootkey */
	char	*p2 = strchr(s2, '\t');
	char	*d1 = separator(p1); /* start of delta key */
	char	*d2 = separator(p2);
	int	rc;

	*d1 = 0;
	*d2 = 0;
	unless (rc = strcmp(p1, p2)) rc = atoi(s1) - atoi(s2);
	assert(rc);
	*d1 = ' ';
	*d2 = ' ';
	return (rc);
}

/*
 * filterWeave - skip pruned, rename renames, add deleted
 */
private	int
filterWeave(sccs *cset, char **cweave, char ***delkeys,
    hash *prunekeys, char *comppath, char **deepnest)
{
	delta	*d;
	char	*rk, *dk;
	char	*delpath = 0, *path, *newpath, *p;
	int	ret = -1, skip, lasti;
	int	empty = 0, i, j, marked = 0;
	int	cnt, del_rk = 0, del_dk = 0, del = 0;
	ser_t	ser, oldser;
	char	last_rk[MAXKEY];
	char	new_rk[MAXKEY];
	char	new_dk[MAXKEY];

	assert(delkeys);

	verbose((stderr, "Processing ChangeSet file...\n"));

	sortLines(cweave, sortByKeySer);
	last_rk[0] = 0;
	cnt = 0;
	skip = 0;
	lasti = 0;
	d = sfind(cset, cset->table->serial);	/* preload whole table */
	EACH(cweave) {
		rk = strchr(cweave[i], '\t');
		rk++;
		dk = separator(cweave[i]);
		*dk++ = 0;

		unless (streq(rk, last_rk)) {
			if (cnt && del &&
			    last_rk[0] && (flags & PRUNE_XCOMP)) {
				/*
				 * Top delta is in another component
				 * so go back and delete file
				 */
				cnt = 0;
				for (j = lasti; j < i; j++) {
					cweave[j][0] = 0;
				}
			}
			/*
			 * If none were kept from last_rk, unlink sfile
			 */
			if (last_rk[0] && (cnt == 0)) {
				*delkeys = addLine(*delkeys, strdup(last_rk));
			}
			del = 0;
			cnt = 0;
			lasti = i;
			strcpy(last_rk, rk);
			skip = 0;
			if (prunekeys && ((flags & PRUNE_ALL) || !keeper(rk))
			    && hash_fetchStr(prunekeys, rk)) {
			    	skip = 1;
zero:				cweave[i][0] = 0;
				continue;
			}
			if (marked) {
				sccs_clearbits(cset, D_RED);
				marked = 0;
			}
			if (delpath) free(delpath);
			unless (delpath = rootkey2delpath(rk)) goto err;
			path = getPath(rk, &p);
			*p = 0;
			path[-1] = 0;
			newpath = newname(delpath, comppath, path, deepnest);
			del_rk = strneq(newpath, "BitKeeper/deleted/", 18);
			sprintf(new_rk, "%s|%s|%s", rk, newpath, &p[1]);
			*p = path[-1] = '|';
		}
		if (skip) goto zero;
		/*
		 * User may want to exclude specific deltas
		 */
		if (prunekeys && hash_fetchStr(prunekeys, dk)) goto zero;
		path = getPath(dk, &p);
		*p = 0;
		path[-1] = 0;
		newpath = newname(delpath, comppath, path, deepnest);
		del_dk = strneq(newpath, "BitKeeper/deleted/", 18);
		del = (newpath == delpath);	/* other component */
		sprintf(new_dk, "%s|%s|%s", dk, newpath, &p[1]);
		*p = path[-1] = '|';

		ser = atoi(cweave[i]);

		unless (del_rk) goto save;
		d = sfind(cset, ser);
		assert(d);
		if (d->flags & D_RED) goto save;

		if (del_dk) goto zero;
		/*
		 * Color from this node to tip along graph
		 */
		marked = 1;
		d->flags |= D_RED;
		for (j = d->serial + 1; j < cset->nextserial; j++) {
			unless (d = sfind(cset, j)) continue;
			if (d->flags & D_RED) continue;
			if ((d->parent->flags & D_RED) ||
			    (d->merge &&
			    (sfind(cset, d->merge)->flags & D_RED))) {
			    	d->flags |= D_RED;
			}
		}
save:
		cnt++;
		free(cweave[i]);
		cweave[i] = aprintf("%u\t%s %s", ser, new_rk, new_dk);
		assert(cweave[i]);
	}
	if (cnt && del && last_rk[0] && (flags & PRUNE_XCOMP)) {
		/*
		 * Top delta is in another component
		 * so go back and delete file
		 */
		cnt = 0;
		for (j = lasti; j < i; j++) {
			cweave[j][0] = 0;
		}
	}
	if (last_rk[0] && (cnt == 0)) {
		*delkeys = addLine(*delkeys, strdup(last_rk));
	}
	cnt = 0;

	if (marked) sccs_clearbits(cset, D_RED);
	marked = 0;

	sortLines(cweave, cset_byserials);

	d = cset->table;
	empty = 0;
	oldser = 0;
	EACH(cweave) {
		unless (cweave[i][0]) {
			/*
			 * all the deleted nodes are at the end: okay to free
			 */
			free(cweave[i]);
			cweave[i] = 0;
			continue;
		}
		ser = atoi(cweave[i]);
		if (ser == oldser) {
			cnt++;
			continue;
		}
		if (oldser) {
			d->added = cnt;
			d = d->next;
		}
		oldser = ser;
		cnt = 1;
		while (d->serial > ser) {
			if (d->added) empty++;
			d->added = 0;
			d = d->next;
		}
		assert(d->serial == ser);
	}
	assert(oldser);
	d->added = cnt;
	while (d = d->next) { 
		if (d->added) empty++;
		d->added = 0;
	}
	ret = empty;

err:
	debug((stderr, "%d empty deltas\n", empty));
	return (ret);
}

/*
 * rmKeys - remove the keys and build a new file.
 */
private int
rmKeys(char **delkeys)
{
	int	n = 0, i;
	MDBM	*dirs = mdbm_mem();
	MDBM	*idDB;
	kvpair	kv;

	/*
	 * Remove each file.
	 */
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		exit(1);
	}
	verbose((stderr, "Removing files...\n"));
	EACH(delkeys) {
		verbose((stderr, "%d removed\r", ++n));
		/* XXX: if have a verbose mode, pass a flags & SILENT */
		sccs_keyunlink(delkeys[i], idDB, dirs, SILENT);
	}
	mdbm_close(idDB);
	verbose((stderr, "\n"));

	verbose((stderr, "Removing directories...\n"));
	for (kv = mdbm_first(dirs); kv.key.dsize; kv = mdbm_next(dirs)) {
		if (isdir(kv.key.dptr)) sccs_rmEmptyDirs(kv.key.dptr);
	}
	mdbm_close(dirs);
	return (0);
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
	 * clean house
	 */
	for (d = s->table; d; d = d->next) {
		d->ptag = d->mtag = 0;
		d->symGraph = 0;
		d->symLeaf = 0;
		d->flags &= ~D_SYMBOLS;
	}

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
	delta	*d, *md, *p;
	symbol	*sym;

	/*
	 * Two phase fixing: do the sym table then the delta graph.
	 * The first fixes most of it, but misses the tag graph merge nodes.
	 *
	 * Each phase has 2 parts: see if the tagged node is gone,
	 * then see if the delta is gone.
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
				    "tag.\nPlease run 'bk support' "
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
			comments_free(md);
			assert(!md->include && !md->exclude && !md->merge);
		}
	}
	/*
	 * same two cases as above, but for the tag merge nodes which
	 * missed because there is no linkage in the symtable.
	 * using this list only won't work because the symtable would
	 * be out of date.  Since we need to do both, a single walk
	 * through them minimizes the work.  The symtable is done first
	 * so that it will not pass the tests here.  The only nodes
	 * done here are non symbol bearing entries in the tag graph.
	 *
	 * This looks similar to the above but it is not the same.
	 * the flow is the same, the data structure being tweaked is diff.
	 */
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'R') continue;
		if ((p = d->parent) && (p->flags & D_GONE)) {
			unless (p->parent) {
				/* No where to move it: root it */
				/* XXX: Can this ever happen?? */
				fprintf(stderr,
				    "csetprune: Tag node %s(%d) on pruned "
				    "revision "
				    "(%s) will be removed,\nbecause the "
				    "revision has no parent to receive the "
				    "tag.\nPlease run 'bk support' "
				    "describing what you did to get this "
				    "message.\nThis is a warning message, "
				    "not a failure.\n",
				    d->rev, d->serial, p->rev);
				d->parent = s->tree;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(p->parent->flags & D_GONE));
			p = p->parent;
			d->parent = p;
			d->pserial = p->serial;
		}
		/* If node is deleted node, make into a 'R' node */
		if (d->flags & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(d->type == 'D');
			d->type = 'R';
			d->flags &= ~(D_GONE|D_CKSUM|D_CSET);
			d->added = d->deleted = d->same = 0;
			comments_free(d);
			assert(!d->include && !d->exclude && !d->merge);
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
pruneEmpty(sccs *s, sccs *sb)
{
	int	i;
	delta	*n;

	sc = s;
	scb = sb;
	for (i = 1; i < sc->nextserial; i++) {
		unless ((n = sfind(sc, i)) && n->next && !TAG(n)) continue;
		_pruneEmpty(n);
	}

	verbose((stderr, "Rebuilding Tag Graph...\n"));
	(flags & PRUNE_NEW_TAG_GRAPH) ? rebuildTags(sc) : fixTags(sc);
	sccs_reDup(sc);
	sccs_newchksum(sc);
	sccs_free(sc);
}

/*
 * Keep if BitKeeper but not BitKeeper/deleted.
 */

private	int
keeper(char *rk)
{
	char	*path, *p;
	int	ret = 0;

	path = strchr(rk, '|');
	assert(path);
	path++;
	p = strchr(path, '|');
	assert(p);
	*p = 0;
	if (streq(path, GCHANGESET) ||
	    (strneq(path, "BitKeeper/", 10) &&
	    !strneq(path, "BitKeeper/deleted/", 18)))
	{
		ret = 1;
	}
	*p = '|';
	return (ret);
}

int
isRootKey(char *key)
{
	int	i = 0;

	while (key && *key) if (*key++ == '|') i++;
	assert((i == 3) || (i == 4));
	return (i == 4);
}

private hash	*
getKeys(void)
{
	char	*buf;
	hash	*prunekeys = hash_new(HASH_MEMHASH);

	verbose((stderr, "Reading keys...\n"));
	while (buf = fgetline(stdin)) {
		unless (sccs_iskeylong(buf)) {
			fprintf(stderr, "csetprune: bad key '%s'\n", buf);
			exit(1);
		}
		unless (hash_insertStr(prunekeys, buf, 0)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			hash_free(prunekeys);
			return (0);
		}
	}
	return (prunekeys);
}

private char**
goneKeys()
{
	char	**gonekeys;
	MDBM	*idDB;
	int	i;
	sccs	*s;

	system("bk get -qS BitKeeper/etc/gone");
	unless (exists("BitKeeper/etc/gone")) return (0);
	gonekeys = file2Lines(0, "BitKeeper/etc/gone");
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	EACH(gonekeys) {
		// Skip anything that doesn't look like a key.
		unless (isKey(gonekeys[i])) {
			gonekeys[i][0] = 0;
			continue;
		}

		// Skip delta keys
		// XXX - these will need to have their paths reworked.
		unless (isRootKey(gonekeys[i])) {
			gonekeys[i][0] = 0;
			continue;
		}
		s = sccs_keyinit(0, gonekeys[i], INIT_NOCKSUM|SILENT, idDB);
		if (s) {
			gonekeys[i][0] = 0;
			sccs_free(s);
			continue;
		}
		verbose((stderr, "Adding gonekey %s\n", gonekeys[i]));
	}
	mdbm_close(idDB);
	return (gonekeys);
}	

private	char	*
newname(char *delpath, char *comp, char *path, char **deep)
{
	char	*newpath, *p;
	int	len, i;

	len = comp ? strlen(comp) : 0;
	if (strneq(path, "BitKeeper/", 10)) {
		/* keep name as is */
		newpath = path;
	} else if (!len || (strneq(path, comp, len) && (path[len] == '/'))) {
		/* for component 'src', src/get.c => get.c */
		newpath = &path[len ? len+1 : 0];
		if (p = strchr(newpath, '/')) {
			/* if in deep nest -> deleted */
			EACH(deep) {
				p = deep[i];
				if (strneq(newpath, p, strlen(p)) &&
				    (newpath[strlen(p)] == '/')) {
					newpath = delpath;
					break;
				}
			}
		}
	} else {
		/* in other component, so store as deleted here */
		newpath = delpath;
	}
	return (newpath);
}


#define	DELPATH(name, rand) \
    aprintf("BitKeeper/deleted/.del-%s~%s", (name), (rand))

private	char	*
s2delpath(sccs *s)
{
	char	rand[MAXPATH];

	if (s->tree->random) {
		strcpy(rand, s->tree->random);
	} else {
		sprintf(rand, "%05u", s->tree->sum);
	}
	return (DELPATH(basenm(s->tree->pathname), rand));
}

private	char	*
rootkey2delpath(char *key)
{
	char	*p, *q, *path, *rand;

	path = strchr(key, '|');
	path++;
	p = strchr(path, '|');
	*p++ = 0;
	unless (q = strchr(p, '|')) {
		p[-1] = '|';
		fprintf(stderr, "Must use long keys: %s\n", key);
		return (0);
	}
	q++;
	if (rand = strchr(q, '|')) {
		rand++;
	} else {
		rand = q;
	}
	path = DELPATH(basenm(path), rand);
	p[-1] = '|';
	return (path);
}

/*
 * Assert in sorted order, so src is before src/libc
 * We want this list as small as possible because we cycle
 * through the list with every rootkey and deltakey
 */
private	char	**
deepPrune(char **map, char *path)
{
	int	oldlen = 0, len, i;
	char	*subpath, *oldsub = 0;
	char	**deep = 0;

	unless (map) return (0);
	len = path ? strlen(path) : 0;
	EACH(map) {
		unless (map[i][0]) continue;
		if (map[i][0] == '#') continue;
		unless (!len ||
		    (strneq(map[i], path, len) && (map[i][len] == '/'))) {
		    	continue;
		}
		subpath = &map[i][len ? len+1 : 0];
		if (oldsub &&
		    strneq(subpath, oldsub, oldlen) &&
		    subpath[oldlen] == '/') {
			continue;
		}
		deep = addLine(deep, subpath);
		oldsub = subpath;
		oldlen = strlen(oldsub);
	}
	return (deep);
}

private	char	*
getPath(char *key, char **term)
{
	char	*p, *q;

	p = strchr(key, '|');
	assert(p);
	p++;
	q = strchr(p, '|');
	assert(q);
	if (term) *term = q;
	return (p);
}

/*
 * This can use D_RED and leave it set
 */
private	int
do_file(sccs *s, char *comppath, char **deepnest)
{
	delta	*d;
	int	i, j, keep = 0;
	char	*newpath;
	char	*delpath;
	char	*bam_new;

	delpath = s2delpath(s);
	d = sfind(s, s->table->serial);	/* preload whole table */

	/*
	 * Save all the old bam dspecs before we start mucking with anything.
	 */
#define	bam_old	symlink
	for (i = 1; BAM(s) && (i < s->nextserial); i++) {
		unless (d = sfind(s, i)) continue;
		if (d->hash) {
			assert(!d->bam_old);
			d->bam_old = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
		}
	}

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		assert(!TAG(d));

		/*
		 * knowledge leak: duppath means parent
		 * inherit any name change that already went on
		 */
		if (d->flags & D_DUPPATH) {
			assert(d->parent);
			d->pathname = d->parent->pathname;
			goto cmark;
		}
		newpath = newname(delpath, comppath, d->pathname, deepnest);
		if (newpath == d->pathname) goto cmark;	/* no change */

		/* too noisy */
		//verbose((stderr, "Moving %s to %s\n", d->pathname, newpath));

		/*
		 * The order here is important: new can possibly
		 * point to inside pathname, so grab a copy of
		 * it before freeing
		 */
		newpath = strdup(newpath);
		free(d->pathname);
		d->pathname = newpath;

cmark:
		// BAM stuff
		if (d->hash) {
			bam_new = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
			unless (streq(d->bam_old, bam_new)) {
				MDBM	*db;
				char	*p;

				db = proj_BAMindex(s->proj, 1);
				assert(db);
				if (p = mdbm_fetch_str(db, d->bam_old)) {
					// mdbm doesn't like you to feed it
					// back data from a fetch from the
					// same db.
					p = strdup(p);
					mdbm_store_str(db,
					    bam_new, p, MDBM_REPLACE);
					bp_logUpdate(0, bam_new, p);
					free(p);
					mdbm_delete_str(db, d->bam_old);
					bp_logUpdate(0, d->bam_old, 0);
				}
			}
			free(bam_new);
		}
		if (keep || (d->flags & D_RED)) continue;
		if (strneq(d->pathname, "BitKeeper/deleted/", 18)) {
			d->flags &= ~D_CSET;
			continue;
		}
	    	if (d == s->tree) { /* keep all cmarks if root not deleted */
			keep = 1;
			continue;
		}
		d->flags |= D_RED;
		for (j = d->serial + 1; j < s->nextserial; j++) {
			unless (d = sfind(s, j)) continue;
			if (d->flags & D_RED) continue;
			if ((d->parent->flags & D_RED) ||
			    (d->merge &&
			    (sfind(s, d->merge)->flags & D_RED))) {
			    	d->flags |= D_RED;
			}
		}
	}
	for (i = 1; BAM(s) && (i < s->nextserial); i++) {
		unless (d = sfind(s, i)) continue;
		if (d->hash) {
			assert(d->bam_old);
			free(d->bam_old);
			d->bam_old = 0;
		}
	}
	free(delpath);
	sccs_newchksum(s);
	return (0);
}

private	int
do_files(char *comppath, char **deepnest)
{
	int	ret = 1;
	sccs	*s = 0;
	char	*sfile;
	FILE	*sfiles;

	unless (comppath || deepnest) return (0);

	unless (sfiles = popen("bk sfiles", "r")) {
		perror("sfiles");
		goto err;
	}
	verbose((stderr, "Fixing file internals .....\n"));
	while (sfile = fgetline(sfiles)) {
		if (streq(CHANGESET, sfile)) continue;
		unless (s = sccs_init(sfile, INIT_MUSTEXIST)) {
			fprintf(stderr, "%s: cannot init %s\n", prog, sfile);
			goto err;
		}
		assert(!CSET(s));
		if (do_file(s, comppath, deepnest)) goto err;
		sccs_free(s);
		s = 0;
	}
	/* Too noisy for (flags & SILENT) ? " -q" : "" */
	verbose((stderr, "Fixing names .....\n"));
	system("bk -r names -q");
	ret = 0;
err:
	if (s) sccs_free(s);
	if (sfiles) pclose(sfiles);
	return (ret);
}
