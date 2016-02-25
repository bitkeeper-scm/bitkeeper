/*
 * Copyright 1998-2005,2007,2009-2012,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"

private	int	dups;		/* duplicate key count */
private	void	rebuild(void);
private	int	caches(char *filename, char type, void *data);

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
idcache_update(char **files)
{
	MDBM	*idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	sccs	*s;
	int	i;
	int	changed = 0;
	char	key[MAXPATH];

	EACH(files) {
		if (streq(files[i], CHANGESET)) continue; /* cset can't move */
		unless (s = sccs_init(files[i], INIT_NOCKSUM)) continue;
		unless (HASGRAPH(s)) {
			fprintf(stderr, "No graph in %s?\n", files[i]);
			sccs_free(s);
			continue;
		}
		sccs_sdelta(s, sccs_ino(s), key);
		if (idcache_item(idDB, key, s->gfile)) changed = 1;
		sccs_free(s);
	}
	if (changed) idcache_write(0, idDB);
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
caches(char *file, char type, void *data)
{
	MDBM	*idDB = (MDBM *)data;
	sccs	*sc;
	ser_t	ino;
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
		assert(HAS_PATHNAME(sc, ino));
		sccs_sdelta(sc, ino, buf);
		save(sc, idDB, buf);
		unless (sc->grafted) break;
		while (ino = sccs_prev(sc, ino)) {
			if (HAS_RANDOM(sc, ino)) break;
		}
	} while (ino);
	sccs_free(sc);
	return (0);
}

/*
 * Update an item in the idcache.
 * Returns true if something changed.
 */
int
idcache_item(MDBM *idDB, char *rk, char *path)
{
	char	*rkpath = key2path(rk, 0, 0, 0);
	char	*npath;
	int	changed = 0;

	npath = mdbm_fetch_str(idDB, rk);
	if (streq(path, rkpath)) {
		if (npath) {
			changed = 1;
			mdbm_delete_str(idDB, rk);
		}
	} else if (!npath || !streq(path, npath)) {
		changed = 1;
		mdbm_store_str(idDB, rk, path, MDBM_REPLACE);
	}
	free(rkpath);
	return (changed);
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
	sprintf(id_tmp, "%s/%s.new.%u", root, getIDCACHE(p), getpid());
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
		chmod(buf, GROUP_MODE);
		if (sccs_unlockfile(id_lock)) perror(id_lock);
	}
	return (0);
}
