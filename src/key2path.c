/*
 * Copyright 2002-2003,2006-2009,2012,2016 BitMover, Inc
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

MDBM	*md5key2key(void);

/*
 * LMXXX - needs an option (default?) to verify that the file is there and
 * is that key.  Perhaps that's the default and -F is fast and just prints
 * where it should be?
 */
int
key2path_main(int ac, char **av)
{
	char	*path;
	char	key[MAXKEY];
	MDBM	*idDB, *goneDB;
	MDBM	*m2k = 0;

	if (proj_cd2root()) {
		fprintf(stderr, "key2path: cannot find package root.\n");
		exit(1);
	}

	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		exit(1);
	}
	goneDB = loadDB(GONE, 0, DB_GONE);

	while (fnext(key, stdin)) {
		chomp(key);
		unless (path = key2path(key, idDB, goneDB, &m2k)) {
			fprintf(stderr, "Can't find path for key %s\n",key);
			mdbm_close(idDB);
			return (1);
		}
		printf("%s\n", path);
		free(path);
	}
	if (m2k) mdbm_close(m2k);
	mdbm_close(idDB);
	mdbm_close(goneDB);
	return (0);
}

/*
 * We want to make sure we don't get fooled by deleted and goned files.
 * So if it is not in the idcache, first make sure it is not in the
 * gone file.  If it is in the gone file, init it and make sure that's
 * the right file.
 *
 * If you are using this as a key parser (we really need a different
 * api for that) then pass 0, 0 for id, gone.
 */
char *
key2path(char *key, MDBM *idDB, MDBM *gone, MDBM **m2k)
{
	char	*path, *t;
	int	check = 0;
	sccs	*s = 0;
	char	rootkey[MAXKEY];

	unless (isKey(key)) return (0);
	unless (path = strchr(key, '|')) {
		if (m2k && !*m2k) *m2k = md5key2key();
		unless (m2k && *m2k) return (0);
		unless (key = mdbm_fetch_str(*m2k, key)) return (0);
	}
	if (path = mdbm_fetch_str(idDB, key)) return (strdup(path));
	if (t = mdbm_fetch_str(gone, key)) check = 1;
	path = strchr(key, '|');
	assert(path);
	path++;
	unless (t = strchr(path, '|')) return (0);
	*t = 0;
	path = strdup(path);
	*t = '|';
	if (check) {
		t = name2sccs(path);
		unless (s = sccs_init(t, INIT_MUSTEXIST|INIT_NOCKSUM)) {
			free(t);
			goto err;
		}
		free(t);
		sccs_sdelta(s, sccs_ino(s), rootkey);
		unless (streq(key, rootkey)) goto err;
		sccs_free(s);
	}
	return (path);
err:	free(path);
	sccs_free(s);
	return (0);
}

/*
 * Return a pointer to a read only MDBM that maps md5root -> longroot key.
 */
MDBM *
md5key2key(void)
{
	MDBM	*m2k, *cache;
	kvpair	kv;
	char	*rootkey, *mpath;
	char	buf[MD5LEN];

	rootkey =
	    backtick("bk -R prs -hr+ -nd:ROOTKEY: BitKeeper/etc/config", 0);
	(void)proj_cset2key(0, "+", rootkey);
	free(rootkey);
	mpath = aprintf("%s/BitKeeper/tmp/csetcache.%x",
	    proj_root(0), (u32)adler32(0, "+", 1));
	unless (cache = mdbm_open(mpath, O_RDONLY, 0600, 0)) {
		free(mpath);
		return (0);
	}
	free(mpath);
	m2k = mdbm_mem();
	for (kv = mdbm_first(cache); kv.key.dsize != 0; kv = mdbm_next(cache)) {
		unless (isKey(kv.key.dptr)) continue;
		sccs_key2md5(kv.key.dptr, buf);
		mdbm_store_str(m2k, buf, kv.key.dptr, MDBM_INSERT);
	}
	mdbm_close(cache);
	return (m2k);
}
