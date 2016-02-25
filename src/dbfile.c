/*
 * Copyright 2012,2014-2016 BitMover, Inc
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

#include "sccs.h"
#include "tomcrypt.h"

private int	dbexplode(char *file, char *dir, char *rev, u32 flags);
private int	dbimplode(char *file, char *dir, u32 flags);
private int	do_explode(char *file, char *dir, int mode, MDBM *db,
			   u32 flags);
private char	**getFields(char *dir);
private int	getFields_walkfn(char *path, char type, void *cookie);
private MDBM	*gfile2DB(char *gfile, int flags, MDBM *db);
private int	print_field(char *file, char *dir, mode_t mode, char *field,
			    u8 *data, int len, u32 flags);
private MDBM	*sfile2DB(char *root, char *sfile, char *rev, MDBM *DB);

private MDBM *
gfile2DB(char *gfile, int flags, MDBM *db)
{
	FILE	*f;

	unless (f = fopen(gfile, "r")) {
		perror(gfile);
		return (0);
	}
	unless (db) db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	assert(db);

	hash_fromStream(db->memdb, f);
	fclose(f);
	return (db);
}

private MDBM *
sfile2DB(char *root, char *sfile, char *rev, MDBM *db)
{
	sccs	*s = sccs_init(sfile, SILENT|INIT_NOSTAT|INIT_MUSTEXIST);

	unless (s) return (0);
	s->mdbm = db;
	db = 0;
	/* GET_HASHONLY|GET_NOREGET means to merge w/existing s->mdbm */
	if (sccs_get(s, rev, 0, 0, 0, SILENT|GET_HASHONLY|GET_NOREGET, 0, 0)) {
		sccs_free(s);
		return (0);
	}
	db = s->mdbm;
	s->mdbm = 0;
	sccs_free(s);
	return (db);
}

/*
 * If an sccs* is given and its db isn't loaded for the given rev,
 * load it and return it.  The caller should not free it unless
 * the caller also sets s->mdbm to NULL.
 *
 * Else, if file exists and rev==NULL, load the gfile's db.
 * Else load the db from file's sfile|rev.
 * For these last two cases, any passed-in db is merged with the
 * one that is loaded, and the caller must free the db.
 */
MDBM *
db_load(char *file, sccs *s, char *rev, MDBM *db)
{
	if (s) {
		if (s->mdbm) {
			mdbm_close(s->mdbm);
			s->mdbm = 0;
		}
		if (sccs_get(s, rev, 0, 0, 0, SILENT|GET_HASHONLY, 0, 0)) {
			s->mdbm = 0;
		}
		return (s->mdbm);
	}
	if (!rev && exists(file)) return (gfile2DB(file, DB_DB, db));
	file = name2sccs(file);
	db = sfile2DB(0, file, rev, db);
	free(file);
	return (db);
}

int
db_store(char *gfile, MDBM *db)
{
	int	ret = -1;
	FILE	*f;

	unless (f = fopen(gfile, "w")) {
		perror(gfile);
		return (-1);
	}
	ret = hash_toStream(db->memdb, f);
	if (fclose(f)) {
		perror(gfile);
		ret = -1;
	}
	return (ret);
}

private int
getFields_walkfn(char *path, char type, void *cookie)
{
	char	***lp = (char***)cookie;

	unless (type == 'd') *lp = addLine(*lp, strdup(path));
	return (0);
}

/*
 * Find all fields in the directory and subdirectories.
 */
private char **
getFields(char *dir)
{
	char	**lines = 0;

	walkdir(dir, (walkfns){ .file = getFields_walkfn }, &lines);
	uniqLines(lines, free);
	reverseLines(lines);
	return (lines);
}

private int
dbimplode(char *file, char *dir, u32 flags)
{
	char	*gfile, *fromdir;
	char	**files = 0;
	int	i, n, c;
	MDBM	*db = 0;
	FILE	*f;
	FILE	*save = fmem();
	datum	k, v;
	int	len, ret = 1;
	size_t	flen;
	char	buf[MAXLINE];

	gfile = (sccs_filetype(file) == 's') ?  sccs2name(file) : strdup(file);
	unless (fromdir = dir) fromdir = aprintf("%s_db.bk_skip", gfile);
	if (streq(gfile, fromdir)) {
		fprintf(stderr, "dbimplode: '%s' file and "
		    "directory must have different names.\n", gfile);
		goto out;
	}

	len = strlen(fromdir);
	unless (isdir(fromdir)) {
		fprintf(stderr, "dbimplode: %s not exploded\n", gfile);
		goto out;
	}

	/*
	 * Note: we're careful to insist that there are only files
	 * and directories in here and that we have no errors.
	 * Everything has to go right because we unlink the files
	 * and we'd lose data on any error.
	 */
	n = 0;
	unless (files = getFields(fromdir)) goto out;
	EACH(files) {
		unless (isreg(files[i])) {
			if (isdir(files[i])) continue;
			fprintf(stderr,
			    "implode: bad file type: %s\n", files[i]);
			goto out;
		}
		unless (db) db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
		unless (f = fopen(files[i], "r")) {
			perror(files[i]);
			goto out;
		}
		while ((c = fread(buf, 1, sizeof(buf), f)) > 0) {
			fwrite(buf, 1, c, save);
		}
		fclose(f);
		k.dptr = &files[i][len+1];
		k.dsize = strlen(k.dptr) + 1;
		v.dptr = fmem_peek(save, &flen);
		v.dsize = flen+1; /* always include trailing null */
		if (mdbm_store(db, k, v, MDBM_REPLACE)) {
			perror("implode store failed");
			goto out;
		}
		ftrunc(save, 0);
		++n;
	}
	unless (db) {
		perror(gfile);
		goto out;
	}

	unless (files) {
		fprintf(stderr, "implode: no fields found for \"%s\"\n", gfile);
		goto out;
	}

	if (db_store(gfile, db)) goto out;

	/* clean up the exploded data */
	rmtree(fromdir);	/* Careful: user's original data */

	ret = 0;

	T_DEBUG("%s: imploded %d fields.", gfile, n);

out:	free(gfile);
	fclose(save);
	if (fromdir != dir) free(fromdir);
	if (db) mdbm_close(db);
	freeLines(files, free);
	return (ret);
}

int
dbimplode_main(int ac, char **av)
{
	int	c, err = 0;
	u32	flags = 0;
	char	*dir = 0, *t;

	while ((c = getopt(ac, av, "d|q", 0)) != -1) {
		switch (c) {
		    case 'd': dir = optarg; break;
		    case 'q': flags = SILENT; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) usage();
	if (streq(av[optind], "-")) {
		if (dir) {
			fprintf(stderr,
			    "dbimplode: directory must be implied with -\n");
			usage();
		}
		while (t = fgetline(stdin)) {
			err |= dbimplode(t, 0, flags);
		}
	} else {
		if (dir && av[optind+1]) {
			fprintf(stderr,
			    "dbimplode: directory must be "
			    "implied with multiple files.\n");
			usage();
		}
		while (av[optind]) {
			err |= dbimplode(av[optind++], dir, flags);
		}
	}
	return (err);
}

private int
print_field(char *file, char *dir, mode_t mode, char *field, u8 *data,
	    int len, u32 flags)
{
	/*
	 * The data should always include at trailing null that we don't
	 * write to the disk.  We are careful here in case someone created
	 * the hash manually and forgot the trailing null.
	 */
	if (len && (data[len-1] == 0)) --len;

	if (streq(dir, "-")) {
		fwrite(data, len, 1, stdout);
	} else {
		int	fd;
		char	fname[MAXPATH];

		sprintf(fname, "%s/%s", dir, field);
		unlink(fname);
		mkdirf(fname);
		if ((fd = creat(fname, mode)) < 0) {
			perror(fname);
			return (-1);
		}
		write(fd, data, len);
		close(fd);
	}
	return (0);
}

private int
do_explode(char *file, char *dir, int mode, MDBM *db, u32 flags)
{
	int	n = 0;
	kvpair	kv;

	EACH_KV(db) {
		if (print_field(file, dir, mode,
			kv.key.dptr, kv.val.dptr, kv.val.dsize, flags)) {
			return (-1);
		}
		n++;
	}
	T_DEBUG("%s: exploded %u fields.", file, n);
	return (0);
}

private int
dbexplode(char *file, char *dir, char *rev, u32 flags)
{
	MDBM	*db;
	char	*gfile, *todir;
	int	ret = 1;
	struct  stat sbuf;
	mode_t	mode = 0444;

	gfile = (sccs_filetype(file) == 's') ? sccs2name(file) : file;
	unless (db = db_load(gfile, 0, rev, 0)) goto out;
	unless (todir = dir) todir = aprintf("%s_db.bk_skip", gfile);
	if (streq(todir, gfile)) {
		fprintf(stderr, "dbexplode: '%s' file and "
		    "directory must have different names\n", gfile);
		goto out;
	}
	if (stat(gfile, &sbuf) == 0) mode = sbuf.st_mode & 0777;
	ret = do_explode(gfile, todir, mode, db, flags);
	mdbm_close(db);
out:	if (gfile != file) free(gfile);
	return (ret);
}

int
dbexplode_main(int ac, char **av)
{
	int	c, err = 0;
	u32	flags = 0;
	char	*dir = 0, *rev = 0, buf[MAXPATH];

	while ((c = getopt(ac, av, "d|qr|", 0)) != -1) {
		switch (c) {
		    case 'd': dir = optarg; break;
		    case 'q': flags = SILENT; break;
		    case 'r': rev = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) usage();
	if (streq(av[optind], "-")) {
		if (dir) {
			fprintf(stderr,
			    "dbexplode: directory must be implied with -\n");
			usage();
		}
		while (fnext(buf, stdin)) {
			chomp(buf);
			err |= dbexplode(buf, 0, rev, flags);
		}
	} else {
		if (dir && av[optind+1]) {
			fprintf(stderr,
			    "dbexplode: directory must be "
			    "implied with multiple files.\n");
			usage();
		}
		while (av[optind]) {
			err |= dbexplode(av[optind++], dir, rev, flags);
		}
	}
	return (err);
}

int
db_sort(char *gfile_in, char *gfile_out)
{
	int	ret = -1;
	int	freeOut = 0;
	MDBM    *db;

	unless (db = db_load(gfile_in, 0, 0, 0)) return (-1);
	if (streq(gfile_in, gfile_out)) {
		char	*dir = dirname_alloc(gfile_out);

		gfile_out = aprintf("%s/.bk-%s", dir, basenm(gfile_out));
		freeOut = 1;
		free(dir);
	}
	unless (ret = db_store(gfile_out, db)) {
		ret = rename(gfile_out, gfile_in);
	}
	mdbm_close(db);
	if (freeOut) free(gfile_out);
	return (ret);
}
