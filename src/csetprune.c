#include "system.h"
#include "sccs.h"
#include "bam.h"
#include "nested.h"
#include "range.h"

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

/*
 * replace delta keys for the gone file in the changeset file because
 * the keys will have new checksums.
 */
typedef struct {
	hash	*db;	// old delta key -> new delta key
	char	*rk;	// rootkey for the gone file
} goner;

private	int	csetprune(hash *prunekeys, char *comppath, char **complist,
		    char ***addweave, char *ranbits, char *rev);
private	int	filterWeave(sccs *cset, char ***cweave, char ***addweave,
		    char ***delkeys, hash *prunekeys, char *comppath,
		    char **complist, goner *gonemap);
private	int	newBKfiles(sccs *cset, hash *prunekeys, char ***cweavep);
private	int	printXcomp(char **cweave, char **complist, hash *prunekeys);
private	goner	*filterGone( char **cweave, char *comppath, char **deepnest);
private	int	rmKeys(char **delkeys);
private	char	*mkRandom(char *input);
private	int	found(delta *start, delta *stop);
private	void	_pruneEmpty(sccs *s, delta *d, u8 *slist);
private	void	pruneEmpty(sccs *s, sccs *sb);
private	hash	*getKeys(char *comppath);
private	int	keeper(char *rk);
private	void	freeGone(goner *gonemap);

private	int	do_file(sccs *s, char *path, char **deep);
private	char	**deepPrune(char **map, char *path);
private	char	*newname( char *delpath, char *comp, char *path, char **deep);
private	char	*getPath(char *key, char **term);
private	int	do_files(char *comppath, char **deepnest);
private	hash	*mklookup(char **cweave);

private	int	flags;

/* restructure tag graph */
#define	PRUNE_NEW_TAG_GRAPH	0x20000000	/* move tags to real deltas */
#define	PRUNE_NO_SCOMPRESS	0x40000000	/* leave serials alone */
#define	PRUNE_XCOMP		0x80000000	/* prune cross comp moves */
#define	PRUNE_LVEMPTY		0x01000000	/* leave empty nodes in graph */
#define	PRUNE_ALL		0x02000000	/* prune all user says to */
#define	PRUNE_FIXGONE		0x04000000	/* Redo the keys in the gone */
#define	PRUNE_NO_NEWROOT	0x08000000	/* leave backpointers alone */

int
csetprune_main(int ac, char **av)
{
	char	*ranbits = 0;
	char	*comppath = 0;
	char	*compfile = 0;
	char	**complist = 0;
	char	*weavefile = 0;
	hash	*prunekeys = 0;
	char	**addweave = 0;
	char	*rev = 0;
	char	*p;
	int	c, ret = 1;

	flags = PRUNE_NEW_TAG_GRAPH;
	while ((c = getopt(ac, av, "ac:C:EGk:KNqr:sSXW:", 0)) != -1) {
		switch (c) {
		    case 'a': flags |= PRUNE_ALL; break;
		    case 'c': comppath = optarg; break;
		    case 'C': compfile = optarg; break;
		    case 'E': flags |= PRUNE_LVEMPTY; break;
		    case 'G': flags |= PRUNE_FIXGONE; break;
		    case 'k': ranbits = optarg; break;
		    case 'K': flags |= PRUNE_NO_NEWROOT; break;
		    case 'N': flags |= PRUNE_NO_SCOMPRESS; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 'S': flags &= ~PRUNE_NEW_TAG_GRAPH; break;
		    case 'X': flags |= PRUNE_XCOMP; break;
		    case 'W': weavefile = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	if (rev && !(flags & PRUNE_XCOMP)) {
		fprintf(stderr, "%s: -r only with -X\n", prog);
		goto err;
	}
	if (ranbits) {
		u8	*p;
		if (strlen(ranbits) != 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option '%s' must have 16 lower case "
			    "hex digits\n", ranbits);
			usage();
		}
		for (p = ranbits; *p; p++) {
			if (!isxdigit(*p) || isupper(*p)) goto k_err;
		}
	}
	/*
	 * Backward compat -- fake '-' if no new stuff specified
	 */
	if ((!comppath && !compfile) ||
	    ((optind < ac) && streq(av[optind], "-"))) {
		p = comppath ? comppath : (compfile ? "." : 0);
		if (flags & PRUNE_XCOMP) p = 0; /* global prune */
		unless (prunekeys = getKeys(p)) goto err;
	}
	if (compfile) {
		unless (complist = file2Lines(0, compfile)) {
			fprintf(stderr, "%s: missing complist file\n", prog);
			goto err;
		}
		sortLines(complist, string_sort);
	}
	if (weavefile) {
		unless (addweave = file2Lines(0, weavefile)) {
			fprintf(stderr, "%s: missing weave file\n", prog);
			goto err;
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "%s: cannot find package root\n", prog);
		goto err;
	}
	if (csetprune(prunekeys, comppath, complist, &addweave, ranbits, rev)) {
		fprintf(stderr, "%s: failed\n", prog);
		goto err;
	}
	ret = 0;

err:
	if (prunekeys) hash_free(prunekeys);
	if (addweave) freeLines(addweave, free);
	if (complist) freeLines(complist, free);
	return (ret);
}

private	int
csetprune(hash *prunekeys, char *comppath, char **complist, char ***addweave,
    char *ranbits, char *rev)
{
	int	empty_nodes = 0, ret = 1;
	int	status;
	delta	*d = 0;
	sccs	*cset = 0, *csetb = 0;
	char	*p, *p1;
	char	**delkeys = 0;
	char	**cweave = 0;
	char	**deepnest = 0;
	goner	*gonemap = 0;
	char	buf[MAXPATH];

	unless (cset = sccs_csetInit(0)) {
		fprintf(stderr, "csetinit failed\n");
		goto err;
	}
	unless (!rev || (d = sccs_findrev(cset, rev))) {
		fprintf(stderr, "%s: not a valid revision: %s\n", prog, rev);
		goto err;
	}
	range_walkrevs(cset, 0, 0, d, 0, walkrevs_setFlags, int2p(D_SET));
	cset->state |= S_SET;
	if ((cweave = cset_mkList(cset)) == (char **)-1) {
		fprintf(stderr, "cset_mkList failed\n");
		goto err;
	}
	if (flags & PRUNE_XCOMP) {
		ret = printXcomp(cweave, complist, prunekeys);
		if (cweave) freeLines(cweave, free);
		sccs_free(cset);
		return (ret);
	}

	unless (ranbits) {
		randomBits(buf);
		ranbits = buf;
	}
	deepnest = deepPrune(complist, comppath);

	if (flags & PRUNE_FIXGONE) {
		gonemap = filterGone(cweave, comppath, deepnest);
		if (gonemap == INVALID) {
			fprintf(stderr, "filterGone failed\n");
			goto err;
		}
	}

	empty_nodes = filterWeave(cset, &cweave, addweave,
	    &delkeys, prunekeys, comppath, deepnest, gonemap);
	if (empty_nodes == -1) {
		fprintf(stderr, "filterWeave failed\n");
		goto err;
	}
	if (comppath) {
		p = aprintf("%s %s\n", comppath, ranbits);
		p1 = mkRandom(p);
		free(p);
		strcpy(buf, p1);
		free(p1);
		ranbits = buf;
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
	pruneEmpty(cset, csetb);	/* does a sccs_free(cset) */
	sccs_free(csetb);
	csetb = 0;
	unless ((cset = sccs_csetInit(INIT_WACKGRAPH|INIT_NOCKSUM)) &&
	    HASGRAPH(cset)) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		goto err;	 /* leave it locked! */
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(cset, SILENT);
	if (flags & PRUNE_NO_SCOMPRESS) {
		sccs_newchksum(cset);
	} else {
		verbose((stderr, "Serial compressing ChangeSet file...\n"));
		sccs_scompress(cset, SILENT);
	}
	sccs_free(cset);
	cset = 0;
finish:
	if (do_files(comppath, deepnest)) goto err;
	proj_reset(0);	/* let go of BAM index */
	verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
	sys("bk", "checksum", "-f", "ChangeSet", SYS);
	// sometimes want to skip this to save another sfile write/walk.
	unless (flags & PRUNE_NO_NEWROOT) {
		verbose((stderr,
		    "Generating a new root key and updating files...\n"));
		if (sys("bk", "newroot",
		    "-qycsetprune command", "-k", ranbits, SYS)) {
			goto err;
		}
	}
	/* Find any missing keys and make a delta about them. */
	if (complist) {
		verbose((stderr, "Running a check...\n"));
		status = system("bk -r check -aggg | bk gone -q -");
		unless (WIFEXITED(status)) goto err;
		unless ((WEXITSTATUS(status) == 0) ||
		    (WEXITSTATUS(status) == 0x40)) {
		    	goto err;
		}
		if (WEXITSTATUS(status) == 0x40) {
			verbose((stderr, "Updating gone...\n"));
		}
	} else if (flags & PRUNE_NO_NEWROOT) {
		/* Hack - 4 g's is same as ignore gone */
		verbose((stderr, "Running a check...\n"));
		status = system("bk -r check -agggg");
		unless (WIFEXITED(status)) goto err;
		unless ((WEXITSTATUS(status) == 0) ||
		    (WEXITSTATUS(status) == 0x40)) {
		    	goto err;
		}
		if (WEXITSTATUS(status) == 0x40) {
			verbose((stderr, "Ignoring gone...\n"));
		}
	} else {
		verbose((stderr, "Running a check -ac...\n"));
		if (system("bk -r check -ac")) goto err;
	}
	verbose((stderr, "All operations completed.\n"));
	ret = 0;

err:	if (cset) sccs_free(cset);
	freeGone(gonemap);
	if (csetb) sccs_free(csetb);
	if (delkeys) freeLines(delkeys, free);
	/* ptrs into complist, don't free */
	if (deepnest) freeLines(deepnest, 0);
	return (ret);
}

/*
 * sort by whole rootkey, then serial - big serial first.
 * lines:
 *    <serial>\t<rootkey> <deltakey>
 */

private	int
sortByKeyBigSer(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1 = strchr(s1, '\t');	/* rootkey */
	char	*p2 = strchr(s2, '\t');
	char	*d1 = separator(p1);	/* deltakey */
	char	*d2 = separator(p2);
	int	rc;

	*d1 = 0;
	*d2 = 0;
	unless (rc = strcmp(p1, p2)) rc = atoi(s2) - atoi(s1);
	assert(rc);
	*d1 = ' ';
	*d2 = ' ';
	return (rc);
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

private	void
freeGone(goner *gonemap)
{
	unless (gonemap && (gonemap != INVALID)) return;
	if (gonemap->db) hash_free(gonemap->db);
	if (gonemap->rk) free(gonemap->rk);
	free(gonemap);
}
/*
 * This is a hard problem.  There could be many gone files.  If we
 * want a clone -r to work, then all gone files would need to be
 * fixed.  Let's at least flush out the mechanics by working on the
 * current gone file...
 */
private	goner	*
filterGone(char **cweave, char *comppath, char **deepnest)
{
	int	iflags = INIT_MUSTEXIST|INIT_NOCKSUM|SILENT;
	int	i;
	goner	*gonemap = 0;
	FILE	*out = 0;
	sccs	*s = 0;
	delta	*d;
	char	*buf, *p, *e, *newpath, *delpath, *rootkey = 0;
	char	*freeme = 0;
	hash	*lookup = 0;
	char	oldkey[MAXKEY], newkey[MAXKEY];

	/*
	 * Name the repo's gone instead of SGONE which could be an env var
	 */
	unless (s = sccs_init("BitKeeper/etc/SCCS/s.gone", iflags)) goto err;
	if ((d = sccs_top(s)) && d->dangling) {
		fprintf(stderr,
		    "%s: dangling deltas in gone need to be removed\n", prog);
		sccs_free(s);
		return (INVALID);
	}
	unless (sccs_lock(s, 'z')) {
		fprintf(stderr, "can't zlock %s\n", s->gfile);
		goto err;
	}
	unless (out = sccs_startWrite(s)) goto err;
	if (delta_table(s, out, 0)) goto err;
	if (s->encoding & E_GZIP) sccs_zputs_init(s, out);
	sccs_rdweaveInit(s);
	while (buf = sccs_nextdata(s)) {
		/* do a remap of path only in keys with path in them */
		if ((i = strcnt(buf, '|')) == 4) {	/* new style rootkey */
			rootkey = buf;
		} else if (i == 3) {	/* deltakey or short rootkey */
			unless (lookup) lookup = mklookup(cweave);
			unless (rootkey = hash_fetchStr(lookup, buf)) {
				/*
				 * Gone file could be MONOTONIC and this 'buf'
				 * be a valid key in another repo.
				 * So ignore it.
				 */
				i = 0;
			}
		} else {
			i = 0;
			/* Comment out non keys */
			unless (strchr("\001#\n", *buf)) {
				fputdata(s, "# csetpruned: ", out);
			}
		}
		if (i) {
			unless (delpath = key2rmName(rootkey)) goto err;
			p = strchr(buf, '|');
			assert(p);
			e = strchr(p+1, '|');
			assert(e);
			*p = *e = 0;
			newpath = newname(delpath, comppath, p+1, deepnest);
			freeme = aprintf("%s|%s|%s", buf, newpath, e+1);
			free(delpath);
			*p = *e = '|';
			buf = freeme;
		}

		fputdata(s, buf, out);
		fputdata(s, "\n", out);
		if (freeme) {
			free(freeme);
			freeme = 0;
		}
	}
	sccs_rdweaveDone(s);
	if (lookup) {
		hash_free(lookup);
		lookup = 0;
	}
	if (fflushdata(s, out)) goto err;
	fseek(out, 0L, SEEK_SET);
	fprintf(out, "\001%c%05u\n", BITKEEPER(s) ? 'H' : 'h', s->cksum);
	if (sccs_finishWrite(s, &out)) goto err;
	sccs_restart(s);	/* delta checksums are wrong */
	for (d = s->table; d; d = d->next) {
		if (TAG(d)) continue;	// assert?
		unless (d->flags & D_CSET) {
			sccs_resum(s, d, 0, 1);
			continue;
		}

		sccs_sdelta(s, d, oldkey);
		sccs_resum(s, d, 0, 1);
		sccs_sdelta(s, d, newkey);
		if (streq(oldkey, newkey)) continue;

		unless (gonemap) {
			gonemap = new(goner);
			gonemap->db = hash_new(HASH_MEMHASH);
		}
		unless (hash_insertStr(gonemap->db, oldkey, newkey)) {
			fprintf(stderr,
			    "Duplicate gone delta?\nKEY: %s\n", oldkey);
			goto err;
		}
	}
	if (gonemap && !gonemap->rk) {
		sccs_sdelta(s, sccs_ino(s), newkey);
		gonemap->rk = strdup(newkey);
	}
	sccs_unlock(s, 'z');
	sccs_newchksum(s);	/* new checksums (XXX: could track if new) */
	unlink("BitKeeper/etc/gone");
	return (gonemap);
err:
	if (gonemap) freeGone(gonemap);
	if (s) {
		sccs_abortWrite(s, &out);
		sccs_unlock(s, 'z');
		sccs_free(s);
	}
	return (INVALID);
}

private	hash	*
mklookup(char **cweave)
{
	int	i;
	char	*rk, *dk, *p;
	hash	*lookup = hash_new(HASH_MEMHASH);

	EACH(cweave) {
		rk = strchr(cweave[i], '\t');
		assert(rk);
		rk++;
		dk = separator(rk);
		assert(dk);
		*dk++ = 0;
		unless (hash_insertStr(lookup, dk, rk)) {
			p = hash_fetchStr(lookup, dk);
			assert(p && streq(p, rk));
		}
		dk[-1] = ' ';
	}
	return (lookup);
}

private	char	*
whichComp(char *path, char **reverseComps)
{
	int	i, len;
	char	*p, *ret = 0;

	unless (p = strrchr(path, '/')) return (0);	/* file in product */
	*p = 0;	/* just look at the dir */
	len = strlen(path);
	EACH(reverseComps) {
		if ((len >= strlen(reverseComps[i])) &&
		    paths_overlap(path, reverseComps[i])) {
			ret = reverseComps[i];
			break;
		}
	}
	*p = '/';
	return (ret);
}

/*
 * Print out things to prune relating to cross component moves:
 * Files that are currently deleted: rootkey
 * Files that moved from A to B: comp||rootkey
 * where comp = '.' if product.
 *
 * NOTE: this writes in and does not restore the cweave list.
 */
private	int
printXcomp(char **cweave, char **complist, hash *prunekeys)
{
	char	*rk, *dk, *path, *p, *comppath = 0, *delcomp;
	char	**reverseComps = 0;
	char	**deepnest = 0;
	hash	*delcomps = hash_new(HASH_MEMHASH);
	hash	*seen = hash_new(HASH_MEMHASH);
	int	i, skip, ret = 1;
	char	last_rk[MAXKEY];

	assert(delcomps && seen);
	verbose((stderr, "Extracting cross component prune list..\n"));

	EACH(complist) reverseComps = addLine(reverseComps, complist[i]);
	reverseLines(reverseComps);

	sortLines(cweave, sortByKeyBigSer);
	last_rk[0] = 0;
	skip = 0;
	EACH(cweave) {
		rk = strchr(cweave[i], '\t');
		rk++;
		dk = separator(cweave[i]);
		*dk++ = 0;
		if (prunekeys && hash_fetchStr(prunekeys, dk)) continue;
		path = getPath(dk, &p);
		*p = 0;

		unless (streq(rk, last_rk)) {
			unless (sccs_iskeylong(rk)) {
				fprintf(stderr,
				    "ChangeSet file has short rootkeys\n");
				goto err;
			}
			strcpy(last_rk, rk);
			skip = 0;
			if (deepnest) {
				freeLines(deepnest, 0);
				deepnest = 0;
			}
			if (strneq(path, "BitKeeper/deleted/", 18)) {
				/* delete this rk in all */
				puts(rk);
				skip = 1;
				continue;
			}
			if (strneq(path, "BitKeeper/", 10)) {
				/* keep - it is in all components */
				skip = 1;
				continue;
			}
			if (prunekeys && hash_fetchStr(prunekeys, rk)) {
			    	skip = 1;
				continue;
			}
			comppath = whichComp(path, reverseComps);
			deepnest = deepPrune(complist, comppath);
			/* filter rootkey path too */
			path = getPath(rk, &p);
			*p = 0;
		}
		if (skip) continue;
		delcomp = newname(0, comppath, path, deepnest);
		if (delcomp) continue;
		unless (hash_insertStr(seen, path, 0)) continue;
		unless (delcomp = whichComp(path, reverseComps)) {
			delcomp = ".";
		}
		*p = '|';	/* restore key (matters if rootkey) */
		delcomp = aprintf("%s||%s", delcomp, rk);
		if (hash_insertStr(delcomps, delcomp, 0)) {
			puts(delcomp);
		}
		free(delcomp);
	}
	ret = 0;
err:
	if (deepnest) freeLines(deepnest, 0);
	if (reverseComps) freeLines(reverseComps, 0);
	if (delcomps) hash_free(delcomps);
	if (seen) hash_free(seen);
	return (ret);
}

private	int
keyExists(char *rk, char *path)
{
	sccs	*s;
	MDBM	*idDB;

	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		return (1);
	}
	s = sccs_keyinit(0, rk, INIT_NOCKSUM|INIT_MUSTEXIST, idDB);
	mdbm_close(idDB);
	if (s) {
		/* delta key for something being replaced */
		fprintf(stderr,
		    "%s: delta found for path '%s' which has "
		    "rootkey\n\t%s\nfrom a different path\n"
		    "To fix, remove sfile %s and mark it gone "
		    "and rerun, or send mail to support@bitmover.com\n",
		    prog, path, rk, s->sfile);
		sccs_free(s);
		return (1);
	}
	return (0);
}

private	int
newFileEnv(sccs *cset, char **user, char **host)
{
	char	c = 0, *p, *dtz;
	delta	*d;

	if (p = getenv("BK_USER")) p = strdup(p);
	*user = p;
	if (p = getenv("BK_HOST")) p = strdup(p);
	*host = p;
	d = sccs_findrev(cset, "1.1");
	assert(d);
	dtz = sccs_prsbuf(cset, d, PRS_FORCE, ":D: :T::TZ:");
	safe_putenv("BK_DATE_TIME_ZONE=%s", dtz+2);
	free(dtz);

	if (p = strchr(d->user, '/')) *p = 0;
	safe_putenv("BK_USER=%s", d->user);
	if (p) *p = '/';
	if ((p = strchr(d->hostname, '/')) || (p = strchr(d->hostname, '['))) {
		c = *p;
		*p = 0;
	}
	safe_putenv("BK_HOST=%s", d->hostname);
	if (p) *p = c;
	putenv("_BK_NO_UNIQ=1");
	putenv("BK_IMPORT=");
	return (d->serial);
}

private	void
clrFileEnv(char *user, char *host)
{
	// XXX save and restore USER and HOST? Since we can delta a gone file
	putenv("BK_DATE_TIME_ZONE=");
	if (user) {
		safe_putenv("BK_USER=%s", user);
	} else {
		putenv("BK_USER=");
	}
	if (host) {
		safe_putenv("BK_HOST=%s", host);
	} else {
		putenv("BK_HOST=");
	}
	putenv("_BK_NO_UNIQ=");
}

private	char	*
newFile(char *path, int ser)
{
	FILE	*f;
	char	*ret = 0, *randin = 0, *line = 0, *cmt = 0;
	char	*spath;
	sccs	*s;
	delta	*d;
	char	rk[MAXKEY];
	char	dk[MAXKEY];

	spath = name2sccs(path);
	unlink(path);
	unlink(spath);
	line = strrchr(spath, '/');
	assert(line);
	line++;
	*line = 'p';
	unlink(spath);
	*line = 'd';
	unlink(spath);
	*line = 'c';
	unlink(spath);
	*line = 's';

	unless (f = fopen(path, "w")) {
		perror(path);
		goto err;
	}
	if (streq(path, ALIASES)) {
		fprintf(f, "@default\nall\n");	/* from setup.c */
	}
	if (fclose(f)) {
		perror(prog);
		goto err;
	}

	randin = aprintf("%s %s %s %s\n",
	    path, getenv("BK_DATE_TIME_ZONE"),
	    getenv("BK_USER"), getenv("BK_HOST"));
	line = mkRandom(randin);
	safe_putenv("BK_RANDOM=%s", line);
	free(line);

	cmt = aprintf("-yNew %s", path);
	assert(cmt);
	if (sys("bk", "new", cmt, "-qp", path, SYS)) {
		perror("new");
		goto err;
	}

	unless (s = sccs_init(spath, INIT_MUSTEXIST)) {
		perror("spath");
		goto err;
	}
	d = sccs_top(s);
	d->flags |= D_CSET;
	sccs_sdelta(s, d, dk);
	sccs_sdelta(s, sccs_ino(s), rk);
	sccs_newchksum(s);
	unlink(sccs_Xfile(s, 'd'));
	sccs_free(s);
	ret = aprintf("%u\t%s %s", ser, rk, dk);

err:
	if (spath) free(spath);
	if (cmt) free(cmt);
	if (randin) free(randin);
	return (ret);
}

#define	TRIGGERS	"BitKeeper/triggers/"
#define	TRIGLEN		(sizeof(TRIGGERS) - 1)

/*
 * filterWeave - skip pruned, rename renames, add deleted
 */
private	int
filterWeave(sccs *cset, char ***cweavep, char ***addweave, char ***delkeys,
    hash *prunekeys, char *comppath, char **deepnest, goner *gonemap)
{
	delta	*d;
	char	*rk, *dk;
	char	*delpath = 0, *path, *newpath, *p, **list;
	char	**cweave = *cweavep;
	int	ret = -1, striptrig = 0, skip, lasti;
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
	if (prunekeys && hash_fetchStr(prunekeys, TRIGGERS)) {
		striptrig = 1;
	}
	EACH(cweave) {
		rk = strchr(cweave[i], '\t');
		rk++;
		dk = separator(cweave[i]);
		*dk++ = 0;

		unless (streq(rk, last_rk)) {
			unless (sccs_iskeylong(rk)) {
				fprintf(stderr,
				    "ChangeSet file has short rootkeys\n");
				goto err;
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
			unless (delpath = key2rmName(rk)) goto err;
			path = getPath(rk, &p);
			*p = 0;
			path[-1] = 0;
			newpath = newname(delpath, comppath, path, deepnest);
			del_rk = strneq(newpath, "BitKeeper/deleted/", 18);
			sprintf(new_rk, "%s|%s|%s", rk, newpath, &p[1]);
			if ((prunekeys && hash_fetchStr(prunekeys, path)) ||
			    (striptrig && strneq(path, TRIGGERS, TRIGLEN))) {
				skip = 1;
			}
			*p = path[-1] = '|';
		}
		if (skip) goto zero;
		/*
		 * User may want to exclude specific deltas
		 */
		if (prunekeys && hash_fetchStr(prunekeys, dk)) goto zero;
		if (gonemap && streq(rk, gonemap->rk)) {
			if (hash_fetchStr(gonemap->db, dk)) {
				dk = gonemap->db->vptr;
			}
		}
		path = getPath(dk, &p);
		*p = 0;
		path[-1] = 0;
		newpath = newname(delpath, comppath, path, deepnest);
		del_dk = strneq(newpath, "BitKeeper/deleted/", 18);
		del = (newpath == delpath);	/* other component */
		sprintf(new_dk, "%s|%s|%s", dk, newpath, &p[1]);
		if ((prunekeys && hash_fetchStr(prunekeys, path)) ||
		    (striptrig && strneq(path, TRIGGERS, TRIGLEN))) {
			if (keyExists(rk, path)) {
				*p = path[-1] = '|';
				goto err;
			}
		}
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
			if ((PARENT(cset, d)->flags & D_RED) ||
			    (d->merge &&
			    (MERGE(cset, d)->flags & D_RED))) {
			    	d->flags |= D_RED;
			}
		}
save:
		cnt++;
		free(cweave[i]);
		cweave[i] = aprintf("%u\t%s %s", ser, new_rk, new_dk);
		assert(cweave[i]);
	}
	if (delpath) {
		free(delpath);
		delpath = 0;
	}
	if (last_rk[0] && (cnt == 0)) {
		*delkeys = addLine(*delkeys, strdup(last_rk));
	}
	cnt = 0;

	if (marked) sccs_clearbits(cset, D_RED);
	marked = 0;

	list = *addweave;
	EACH(list) cweave = addLine(cweave, list[i]);
	freeLines(list, 0);
	*addweave = list = 0;
	*cweavep = cweave;
	if (prunekeys) {
		if (newBKfiles(cset, prunekeys, cweavep)) goto err;
		cweave = *cweavep;
	}

	sortLines(cweave, cset_byserials);

	d = cset->table;
	empty = 0;
	oldser = 0;
	lasti = 0;
	EACH(cweave) {
		unless (cweave[i][0]) {
			/*
			 * all the deleted nodes are at the end: okay to free
			 */
			free(cweave[i]);
			unless (lasti) lasti = i;
			continue;
		}
		ser = atoi(cweave[i]);
		if (ser == oldser) {
			cnt++;
			continue;
		}
		if (oldser) {
			d->added = cnt;
			d = NEXT(d);
		}
		oldser = ser;
		cnt = 1;
		while (d->serial > ser) {
			if (d->added) empty++;
			d->added = 0;
			d = NEXT(d);
		}
		assert(d->serial == ser);
	}
	if (lasti) truncLines(cweave, lasti-1);
	assert(oldser);
	d->added = cnt;
	while (d = NEXT(d)) { 
		if (d->added) empty++;
		d->added = 0;
	}
	ret = empty;

err:
	if (delpath) {
		free(delpath);
		delpath = 0;
	}
	debug((stderr, "%d empty deltas\n", empty));
	return (ret);
}

/*
 * cull pruned files from the list that are in BitKeeper/etc
 * and make new ones, shoving their keys into the weave.
 */
private	int
newBKfiles(sccs *cset, hash *prunekeys, char ***cweavep)
{
	int	i, ret = 1;
	ser_t	ser = 0;
	char	*rkdk, **list = 0, *user = 0, *host = 0;

	unless (prunekeys) return (0);

	EACH_HASH(prunekeys) {
		unless (strchr(prunekeys->kptr, '|')) {
			if (strneq(prunekeys->kptr, "BitKeeper/etc/", 14)) {
				list = addLine(list, prunekeys->kptr);
			}
		}
	}
	unless (list) return (0);

	unless (ser = newFileEnv(cset, &user, &host)) goto err;
	EACH(list) {
		unless (rkdk = newFile(list[i], ser)) goto err;
		*cweavep = addLine(*cweavep, rkdk);
	}
	ret = 0;
 err:
	clrFileEnv(user, host);
	if (user) free(user);
	if (host) free(host);
	freeLines(list, 0);
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
		++n;
		/* XXX: if have a verbose mode, pass a flags & SILENT */
		sccs_keyunlink(delkeys[i], idDB, dirs, SILENT);
	}
	mdbm_close(idDB);
	verbose((stderr, "%d removed\n", n));

	verbose((stderr, "Removing directories...\n"));
	for (kv = mdbm_first(dirs); kv.key.dsize; kv = mdbm_next(dirs)) {
		if (isdir(kv.key.dptr)) sccs_rmEmptyDirs(kv.key.dptr);
	}
	mdbm_close(dirs);
	return (0);
}

private	char	*
mkRandom(char *input)
{
	FILE	*f;
	char	*tmpf = 0, *cmd = 0, *ret = 0;

	tmpf = bktmp(0, "bkrandom");
	cmd = aprintf("bk undos -n | bk crypto -hX - > '%s'", tmpf);
	unless (f = popen(cmd, "w")) {
		perror(cmd);
		goto err;
	}
	fputs(input, f);
	if (pclose(f)) {
		perror(cmd);
		goto err;
	}
	ret = loadfile(tmpf, 0);
	assert(strlen(ret) > 16);
	ret[16] = 0;
err:
	if (cmd) free(cmd);
	if (tmpf) {
		unlink(tmpf);
		free(tmpf);
	}
	return(ret);
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

	for (d = NEXT(start); d && d != stop; d = NEXT(d)) d->flags &= ~D_RED;
	start->flags |= D_RED;
	stop->flags &= ~D_RED;

	for (d = start; d && d != stop; d = NEXT(d)) {
		unless (d->flags & D_RED) continue;
		if (d->pserial) PARENT(sc, d)->flags |= D_RED;
		if (d->merge) MERGE(sc, d)->flags |= D_RED;
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
		if (p = PARENT(s, d)) {
			unless (p->symGraph) {
				p = (p->ptag) ? sfind(s, p->ptag) : 0;
			}
		}
		m = 0;
		if (m = MERGE(s, d)) {
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
	for (d = s->table; d; d = NEXT(d)) {
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
			unless (d->pserial) {
				/* No where to move it: drop tag */
				sym->metad = sym->d = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(PARENT(s, d)->flags & D_GONE));
			d = sym->d = PARENT(s, d);
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
	for (d = s->table; d; d = NEXT(d)) {
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
			unless (d->pserial) {
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
			assert(!(PARENT(s, d)->flags & D_GONE));
			d = PARENT(s, d);
			sym->d = d;
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
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->type == 'R') continue;
		if ((p = PARENT(s, d)) && (p->flags & D_GONE)) {
			unless (p->pserial) {
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
			assert(!(PARENT(s, p)->flags & D_GONE));
			p = PARENT(s, p);
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
_pruneEmpty(sccs *s, delta *d, u8 *slist)
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
		m = MERGE(s, d);
		if (found(PARENT(s, d), m)) {	/* merge collapses */
			if (d->merge > d->pserial) {
				d->parent = m;
				d->pserial = d->merge;
			}
			d->merge = 0;
		}
		/* else if merge .. (chk case d and e) */
		else if (sccs_needSwap(s, PARENT(s, d), m)) {
			d->merge = d->pserial;
			d->parent = m;
			d->pserial = m->serial;
		}
		/* fix include and exclude lists */
		sccs_adjustSet(sc, scb, d, slist);
		// XXX: come up with a test case for 1st one (xaraya core)
		// assert((d->merge && d->include)||(!d->merge && !d->include));
		unless ((d->merge && d->include)||(!d->merge && !d->include)) {
			int	i;
			delta	*w;

			fprintf(stderr, "# Ignoring old assert\n");
			fprintf(stderr, "new delta %s(%u) ", d->rev, d->serial);
			fprintf(stderr, "parent %s(%u) ",
			    d->pserial ? sfind(s, d->pserial)->rev : "none",
			    d->pserial ? d->pserial : 0);
			fprintf(stderr, "merge %s(%u) include",
			    d->merge ? sfind(s, d->merge)->rev : "none",
			    d->merge ? d->merge : 0);
			
			EACH(d->include) {
				fprintf(stderr, " %s(%u)",
				    sfind(s, d->include[i])->rev,
				    d->include[i]);
			}
			fprintf(stderr, "\n");

			w = d;
			d = sfind(scb, d->serial);
			s = scb;

			fprintf(stderr, "old delta %s(%u) ", d->rev, d->serial);
			fprintf(stderr, "parent %s(%u) ",
			    d->pserial ? sfind(s, d->pserial)->rev : "none",
			    d->pserial ? d->pserial : 0);
			fprintf(stderr, "merge %s(%u) include",
			    d->merge ? sfind(s, d->merge)->rev : "none",
			    d->merge ? d->merge : 0);
			
			EACH(d->include) {
				fprintf(stderr, " %s(%u)",
				    sfind(s, d->include[i])->rev,
				    d->include[i]);
			}
			fprintf(stderr, "\n");
			d = w;
			s = sc;
		}
		// assert(!d->exclude); -- we can have excludes (see test)
	}
	/* Else this never was a merge node, so just adjust inc and exc */
	else if (d->include || d->exclude) {
		sccs_adjustSet(sc, scb, d, slist);
	}
	/*
	 * See if node is a keeper ...
	 */
	if (d->added || d->merge || d->include || d->exclude) return;

	/* Not a keeper, so re-wire around it */
	debug((stderr, "RMDELTA(%s)\n", d->rev));
	MK_GONE(sc, d);
	assert(d->pserial);	/* never get rid of root node */
	if (d->flags & D_MERGED) {
		for (m = sc->table; m && m->serial > d->serial; m = NEXT(m)) {
			unless (m->merge == d->serial) continue;
			debug((stderr,
			    "%s gets new merge parent %s (was %s)\n",
			    m->rev, d->parent->rev, d->rev));
			m->merge = d->pserial;
		}
	}
	for (m = KID(d); m; m = SIBLINGS(m)) {
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
	u8	*slist;

	sc = s;
	scb = sb;
	slist = (u8 *)calloc(sc->nextserial, sizeof(u8));
	assert(slist);
	for (i = 1; i < sc->nextserial; i++) {
		unless ((n = sfind(sc, i)) && NEXT(n) && !TAG(n)) continue;
		_pruneEmpty(sc, n, slist);
	}
	free(slist);

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

private hash	*
getKeys(char *comppath)
{
	char	*buf, *p;
	hash	*prunekeys = hash_new(HASH_MEMHASH);

	verbose((stderr, "Reading keys...\n"));
	while (buf = fgetline(stdin)) {
		if ((p = strchr(buf, '|')) && (p[1] == '|')) {
			unless (comppath) continue;
			if (p == buf) {
				/* all components; ignore for product */
				if (streq(comppath, ".")) continue;
			} else {
				/* ignore unless this component or product */
				*p = 0;
				unless (streq(buf, comppath)) continue;
			}
			buf = p + 2; /* rest is key or path */
		} else if (comppath) {
			continue;	/* not for a specific component */
		}
		unless (hash_insertStr(prunekeys, buf, 0)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			hash_free(prunekeys);
			return (0);
		}
	}
	/* force a new aliases file in the product */
	if (comppath && streq(comppath, ".")) {
		unless (hash_storeStr(prunekeys, ALIASES, 0)) {
			fprintf(stderr, "Hash store aliases failed\n");
			hash_free(prunekeys);
			return (0);
		}
	}
	return (prunekeys);
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
	char	*sortpath;
	char	rk[MAXKEY];

	sccs_sdelta(s, sccs_ino(s), rk);
	delpath = key2rmName(rk);
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
			assert(d->pserial);
			d->pathname = PARENT(s, d)->pathname;
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
		sortpath = PATH_SORTPATH(d->pathname);
		if (*sortpath) {
			newpath = PATH_BUILD(newpath, sortpath);
		} else {
			newpath = PATH_BUILD(newpath, d->pathname);
		}
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
			if ((PARENT(s, d)->flags & D_RED) ||
			    (d->merge &&
			    (MERGE(s, d)->flags & D_RED))) {
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
	pclose(sfiles);
	sfiles = 0;
	/* Too noisy for (flags & SILENT) ? " -q" : "" */
	verbose((stderr, "Fixing names .....\n"));
	system("bk -r names -q");
	ret = 0;
err:
	if (s) sccs_free(s);
	if (sfiles) pclose(sfiles);
	return (ret);
}
