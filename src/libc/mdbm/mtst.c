/*
 * Copyright 1999-2001,2004-2007 BitMover, Inc
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

int mdbm_test(void);


#define	MAXK   10000
#define	INC 1

private int
mdbm_test_1(char *dbname, int pre_split, int compress, int hash)
{
	MDBM	*tst_db;
	datum	k, v, l, r = {0, 0};
	int	key, i;
	char	kbuf[100], lbuf[100], vbuf[100], ref_buf[100];

	if (dbname) {
		tst_db = mdbm_open(dbname, O_RDWR|O_CREAT|O_TRUNC, 0644, 256);
	} else {
		tst_db = mdbm_open(NULL, 0, 0, 256);
	}

	if (tst_db == NULL) {
		    printf("can not open db\n");
		    return (-1);
	}

	if (pre_split  > 0) mdbm_pre_split(tst_db, pre_split);

	mdbm_sethash(tst_db, hash); /* select hash functiom */
	fprintf(stderr, "checking insert   ...........................................");
	for (key = 0; key < MAXK; key += INC) {
		k.dptr = (void*) &kbuf;
		sprintf(kbuf, "%d", key);
		k.dsize = strlen(kbuf);
		sprintf(vbuf, "<   %d    >", key);
		v.dptr = (void*)vbuf;
		v.dsize = strlen(vbuf) + 1;
		if (mdbm_store(tst_db, k, v, MDBM_INSERT)) {
			fprintf(stderr, "dbm_store failed\n");
			return (-1);
		}

	}
	fprintf(stderr, "OK\n");

	if (compress) mdbm_compress_tree(tst_db);
	fprintf(stderr, "checking fetch ..............................................");
	l.dptr = (void *) &lbuf;
	for (i = (key - INC); i >= 0; i -= INC) {
		sprintf(lbuf, "%d", i);
		l.dsize = strlen(lbuf);
		r = mdbm_fetch(tst_db, l);
		if (r.dptr == NULL) {
			fprintf(stderr, "###can not find data %s\n", l.dptr);
			return (-1);
		}
		sprintf(ref_buf, "<   %d    >", i);
		if (strcmp(ref_buf, r.dptr)) {
			fprintf(stderr,
			    "expected %s got %s\n", ref_buf, r.dptr);
			return (-1);
		}
	}
	fprintf(stderr, "OK\n");
	/* this should fail */
	fprintf(stderr, "checking insert of duplicate.................................");
	sprintf(lbuf, "%d", MAXK - 1);
	l.dptr = (void *) &lbuf;
	r.dptr = strcpy(vbuf, r.dptr);
	if (mdbm_store(tst_db, l, r, MDBM_INSERT) != 1) {
		fprintf(stderr, "Failed\n");
	} else {
		fprintf(stderr, "OK\n");
	}
	fprintf(stderr, "checking replace.............................................");
	sprintf(vbuf, "<###%d###>", key);
	if (mdbm_store(tst_db, l, v, MDBM_REPLACE)) {
		fprintf(stderr, "dbm_store failed\n");
		return (-1);
	}
	r = mdbm_fetch(tst_db, l);
	if (r.dptr == NULL) {
		fprintf(stderr, "###can not find data %s\n", l.dptr);
		return (-1);
	}
	if (strcmp(vbuf, r.dptr)) {
		fprintf(stderr, "expected %s got %s\n", vbuf, r.dptr);
		return (-1);
	} else {
		fprintf(stderr, "OK\n");
	}

	fprintf(stderr, "checking delete..............................................");
	sprintf(kbuf, "%d", MAXK-INC);
	if (mdbm_delete(tst_db, k)) {
		fprintf(stderr, "###cannot delete %s\n", k.dptr);
		return (-1);
	}
	/* look it up again, this should fail */
	if (mdbm_fetch(tst_db, k).dptr != NULL) {
		fprintf(stderr, "delete failed %s\n", k.dptr);
		return (-1);
	} else {
		fprintf(stderr, "OK\n");
	}


	mdbm_chk_all_page(tst_db);
	//mdbm_stat_all_page(tst_db);
	mdbm_close(tst_db);
	return 0;
}

#define	COMPRESS_TREE 1
private int
basic_test(int hash)
{
	char	dbpath[500];
	char	*tmp = getenv("BK_TMP") ? getenv("BK_TMP") : "/tmp";
	int	rc = 1;

	sprintf(dbpath, "%s/mdbm_test%u", tmp, getpid());
	unlink(dbpath);

	fprintf(stderr, "---checking mmap/file based mdbm\n");

	/* test regular mdbm */
	if (mdbm_test_1(dbpath, 0, 0, hash)) goto out;
	fprintf(stderr, "---checking mmap/file based mdbm with pre-split\n");

	/* pre_split to 32 pages */
	if (mdbm_test_1(dbpath, 32, 0, hash)) goto out;
	fprintf(stderr,
	    "---checking mmap/file based mdbm with pre-split & compress\n");
	if (mdbm_test_1(dbpath, 32, COMPRESS_TREE, hash)) goto out;
	fprintf(stderr, "---checking memory based mdbm\n");

	/* test memory only mdbm */
	if (mdbm_test_1(NULL, 0, 0, hash)) goto out;
	fprintf(stderr, "---checking memory based mdbm with pre-split\n");
	/* pre_split to 32 pages */
	if (mdbm_test_1(NULL, 32, 0, hash)) goto out;
	fprintf(stderr,
	    "---checking memory based mdbm with pre-split & compress\n");
	if (mdbm_test_1(NULL, 32, COMPRESS_TREE, hash)) goto out;
	rc = 0;
out:
	unlink(dbpath);
	return (rc);
}

int
main(int argc, char* argv[])
{
	basic_test(0);
	basic_test(1);
	basic_test(2);
	return (0);
}
