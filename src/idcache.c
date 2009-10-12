#include "system.h"
#include "sccs.h"

private	int	dups;		/* duplicate key count */
private	int	mixed;		/* running in mixed long/short mode */
private	void	rebuild(void);
private	int	caches(char *filename, struct stat *sb, void *data);

int
idcache_main(int ac, char **av)
{
	unless ((ac == 2) && streq(av[1], "-q")) {
		fprintf(stderr, "Rebuilding idcache\n");
	}

	if (proj_cd2root()) return (1);

	rebuild();
	return (dups ? 1 : 0);
}

/*
 * rebuild the caches.
 */
private	void
rebuild(void)
{
	sccs	*cset;
	MDBM	*idDB;

	/* only the bitmover tree might have short keys */
	if (streq(proj_rootkey(0),
		"lm@lm.bitmover.com|ChangeSet|19990319224848|02682")) {

		unless (cset = sccs_csetInit(0)) {
			perror("sfiles: can't init ChangeSet");
			exit(1);
		}
		unless (HAS_SFILE(cset) && cset->cksumok) {
			perror("sfiles: can't init ChangeSet");
			sccs_free(cset);
			exit(1);
		}
		mixed = !LONGKEY(cset);
		sccs_free(cset);
	}
	idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	assert(idDB);
	walksfiles(".", caches, idDB);
	if (dups) {
		fprintf(stderr, "Not updating idcache due to dups.\n");
	} else {
		idcache_write(0, idDB);
	}
	mdbm_close(idDB);
}

/*
 * I dislike this because it is yet another round of inits but I'm not
 * sure how to avoid it.
 */
void
idcache_update(char *filelist)
{
	MDBM	*idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	FILE	*in = fopen(filelist, "r");
	sccs	*s;
	u8	*u;
	int	n = 0;
	char	buf[MAXKEY + MAXPATH];
	char	key[MAXPATH];

	while (fnext(buf, in)) {
		chomp(buf);
		if (streq(buf, CHANGESET)) continue; /* cset can't move */
		unless (s = sccs_init(buf, INIT_NOCKSUM)) continue;
		unless (HASGRAPH(s)) {
			fprintf(stderr, "No graph in %s?\n", buf);
			sccs_free(s);
			continue;
		}
		sccs_sdelta(s, sccs_ino(s), key);
		u = mdbm_fetch_str(idDB, key);
		unless (u && streq(u, s->gfile)) {
			mdbm_store_str(idDB, key, s->gfile, MDBM_REPLACE);
			++n;
		}
		sccs_free(s);
	}
	fclose(in);
	unless (n) return;
	idcache_write(0, idDB);
	mdbm_close(idDB);
}

private	void
save(sccs *sc, MDBM *idDB, char *buf)
{
	if (mdbm_store_str(idDB, buf, sc->gfile, MDBM_INSERT)) {
		if (errno == EEXIST) {
			char	*sfile = name2sccs(mdbm_fetch_str(idDB, buf));

			fprintf(stderr,
			    "ROOTKEY %s\n\tused by %s\n\tand by  %s\n",
			    buf, sc->sfile, sfile);
			free(sfile);
			dups++;
		} else {
			perror("mdbm_store");
			exit(1);
		}
	}
}

private	int
caches(char *file, struct stat *sb, void *data)
{
	MDBM	*idDB = (MDBM *)data;
	sccs	*sc;
	delta	*ino;
	char	*t;
	char	buf[MAXPATH*2];

	file += 2;
	if (streq(file, CHANGESET)) return (0);
	unless (sc = sccs_init(file, INIT_NOCKSUM)) return (0);
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return (0);
	}

	ino = sccs_ino(sc);

	/*
	 * Update the id cache if root path != current path.
	 * Add entries for any grafted in roots as well.
	 */
	do {
		assert(ino->pathname);
		sccs_sdelta(sc, ino, buf);
		save(sc, idDB, buf);
		if (mixed && (t = sccs_iskeylong(buf))) {
			*t = 0;
			save(sc, idDB, buf);
			*t = '|';
		}
		unless (sc->grafted) break;
		while (ino = ino->next) {
			if (ino->random) break;
		}
	} while (ino);
	sccs_free(sc);
	return (0);
}

int
idcache_write(project *p, MDBM *idDB)
{
	FILE	*f;
	char	*t, *root;
	u32	id_sum;
	kvpair	kv;
	char	buf[MAXLINE];
	char	id_tmp[MAXPATH];
	char	id_lock[MAXPATH];

	root = proj_root(p);
	sprintf(id_tmp, "%s/%s.new", root, getIDCACHE(p));
	unless (f = fopen(id_tmp, "w")) {
		perror(id_tmp);
		return (1);
	}
	fprintf(f,
"# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, simply remove it and \n\
# and it will be rebuilt as needed.\n\
# The format of the file is <ID> <PATHNAME>\n\
# The file is used for performance during makepatch/takepatch commands.\n");
	id_sum = 0;
	EACH_KV(idDB) {
		sprintf(buf, "%s %s\n", kv.key.dptr, kv.val.dptr);
		for (t = buf; *t; ++t) id_sum += (u8)*t;
		fputs(buf, f);
	}
	fprintf(f, "#$sum$ %u\n", id_sum);
	fclose(f);

	if (proj_hasOldSCCS(p)) {
		sprintf(id_lock, "%s/%s",
		    root, "BitKeeper/etc/SCCS/z.id_cache");
	} else {
		sprintf(id_lock, "%s/%s",
		    root, "BitKeeper/log/z.id_cache");
	}
	sprintf(buf, "%s/%s", root, getIDCACHE(p));
	if (sccs_lockfile(id_lock, 16, 0)) {
		fprintf(stderr, "Not updating cache due to locking.\n");
		unlink(id_tmp);
		return (1);
	} else {
		if (rename(id_tmp, buf)) {
			perror("rename of idcache");
			unlink(buf);
		}
		if (sccs_unlockfile(id_lock)) perror(id_lock);
		chmod(buf, GROUP_MODE);
	}
	return (0);
}
