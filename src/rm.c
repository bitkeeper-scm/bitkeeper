/*
 * Copyright 1999-2006,2008-2010,2012-2016 BitMover, Inc
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

private	int	sccs_gone(int quiet, FILE *f);

/*
 * Emulate rm(1)
 *
 * usage: rm a b ....
 */
int
rm_main(int ac, char **av)
{
	char	*name;
	int	c, errors = 0;
	int	force = 0;
	MDBM	*idDB;
	project	*proj, *p;
	char	*dir;
	char	buf[MAXPATH];

        while ((c = getopt(ac, av, "f", 0)) != -1) {
                switch (c) {
		    case 'f': force = 1; break;
                    default: bk_badArg(c, av);
                }
        }
	unless (av[optind]) usage(); /* require one argument */

	proj = 0;
	idDB = 0;
	for (name = sfileFirst("sccsrm",&av[optind], 0);
	    name; name = sfileNext()) {
		strcpy(buf, name);
		dir = dirname(dirname(buf)); /* strip SCCS/s.file */
		p = proj_init(dir);
		if (p != proj) {
			if (idDB) {
				idcache_write(proj, idDB);
				mdbm_close(idDB);
				proj_free(proj);
			}
			proj = p;
			concat_path(buf, proj_root(proj), getIDCACHE(proj));
			idDB = loadDB(buf, 0, DB_IDCACHE);
			assert(idDB);
		} else if (p) {
			assert(idDB);
			proj_free(p);
		}
		errors |= sccs_rm(name, force, idDB);
	}
	if (idDB) {
		idcache_write(proj, idDB);
		mdbm_close(idDB);
		proj_free(proj);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}

/*
 * Returns the delete pathname for this file relative to the repository
 * root.
 *    ex:  BitKeeper/deleted/4c/basename~randbits
 */
char *
key2rmName(char *rootkey)
{
	int	lendiff;
	char	*path, *rand, *t, *bn;
	char	key[MAXKEY];
	char	buf[MAXPATH];

	strcpy(key, rootkey);
	path = strchr(key, '|') + 1;
	rand = strchr(path, '|');
	*rand++ = 0;			/* at utc */
	rand = strchr(rand, '|') + 1;	/* at sum (shortkeys stay here) */
	if (t = strchr(rand, '|')) {
		rand = t + 1;		/* at rand */
	}
	bn = basenm(path);
	if (strneq(path, "BitKeeper/deleted/", 18) ||
	    strneq(path, "BitKeeper/moved/", 16)) {
		/*
		 * handle files created in BitKeeper/deleted
		 * nested partition can do this
		 * b =~ s/^\.del-//
		 * b =~ s/~${rand}$//	(really, just count rand chars)
		 */
		if (strneq(bn, ".del-", 5)) bn += 5;
		lendiff = strlen(bn) - strlen(rand);
		if ((lendiff > 0) && (bn[lendiff-1] == '~')) bn[lendiff-1] = 0;
	}
	/* random =~ s/:/-/g;  fix win32 BAM keys with : in them */
	t = rand;
	while (t = strchr(t, ':')) *t++ = '-';

	sprintf(buf, "%s~%s", bn, rand);
	path = file_fanout(buf);
	concat_path(buf, "BitKeeper/deleted", path);
	free(path);
	return (strdup(buf));
}

/* returns malloced absolute pathname to deleted sfile */
char *
sccs_rmName(sccs *s)
{
	char	*rmname, *sfile;
	int	try;
	project	*parent;
	sccs	*s2;
	char	suffix[16];
	char	rootkey[MAXKEY], newkey[MAXKEY];
	char	path[MAXPATH];

	sccs_sdelta(s, sccs_ino(s), rootkey);
	rmname = key2rmName(rootkey);
	sfile = name2sccs(rmname);
	free(rmname);
	parent = proj_isResync(s->proj);
	for (try = 0; ; try++) {
		if (try) {
			sprintf(suffix, "~%d", try);
		} else {
			suffix[0] = 0;
		}
		if (parent) {
			sprintf(path, "%s/%s%s",
			    proj_root(parent), sfile, suffix);
			if (s2 = sccs_init(path, INIT_MUSTEXIST|INIT_NOCKSUM)) {
				sccs_sdelta(s2, sccs_ino(s2), newkey);
				sccs_free(s2);
				unless (streq(rootkey, newkey)) continue;
			}
		}
		sprintf(path, "%s/%s%s", proj_root(s->proj), sfile, suffix);
		if (s2 = sccs_init(path, INIT_MUSTEXIST|INIT_NOCKSUM)) {
			sccs_sdelta(s2, sccs_ino(s2), newkey);
			sccs_free(s2);
			unless (streq(rootkey, newkey)) continue;
		}

		/* found good name */
		break;
	}
	free(sfile);
	return (strdup(path));
}

int
sccs_rm(char *name, int force, MDBM *idDB)
{
	char	*rmName;
	char	*sfile, *t;
	int	error = 0;
	sccs	*s;

	T_SCCS("file=%s", name);
	sfile = name2sccs(name);
	t = fullname(sfile, 0);
	free(sfile);
	sfile = t;
	s = sccs_init(sfile, 0);
	unless (s && HASGRAPH(s) && BITKEEPER(s)) {
		fprintf(stderr,
		    "Warning: %s is not a BitKeeper file, ignored\n", name);
		sccs_free(s);
		return (1);
	}
	if (sccs_clean(s, SILENT|CLEAN_SHUTUP)) {
		unless (force) {
			fprintf(stderr,
			    "rm: %s is modified, delta|unedit first.\n",
			    s->gfile);
			sccs_free(s);
			return (1);
		}
		sccs_unedit(s, SILENT);
	}
	sccs_close(s);
	rmName = sccs_rmName(s);
	unless (samepath(rmName, sfile)) {
		error |= sccs_mv(sfile, rmName, 0, 1, 0, force, idDB);
	}
	sccs_free(s);
	free(rmName);
	free(sfile);
	return (error);
}

int
gone_main(int ac, char **av)
{
	int	c;
	int	quiet = 0;
	char	tmpfile[MAXPATH];
	FILE	*f;

	while ((c =  getopt(ac, av, "gqs", 0)) != -1) { 
		switch (c) {
		    case 'g': printf("%s\n", GONE); return (0);
		    case 'q': quiet++; break;	/* doc 2.0 */
		    case 's': printf("%s\n", SGONE); return (0);
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) {
		system("bk help -s gone");
		return (1);
	}

	if (streq("-", av[optind])) exit(sccs_gone(quiet, stdin));
	unless (bktmp(tmpfile)) exit(1);
	f = fopen(tmpfile, "w");
	while (av[optind]) fprintf(f, "%s\n", av[optind++]);
	fclose(f);
	f = fopen(tmpfile, "r");
	if (sccs_gone(quiet, f)) {
		fclose(f);
		unlink(tmpfile);
		exit(1);
	}
	fclose(f);
	unlink(tmpfile);
	exit(0);
}

private int
sccs_gone(int quiet, FILE *f)
{
	int	i;
	sccs	*s;
	FILE	*g;
	char	*root;
	kvpair	kv;
	MDBM	*db = mdbm_mem();
	char	**lines = 0;
	char	s_gone[MAXPATH], g_gone[MAXPATH], key[MAXKEY];


	T_SCCS(0);
	/* eat the keys first because check will complain if we edit the file */
	i = 0;
	while (fnext(key, f)) {
		unless (isKey(key)) {
			chomp(key);
			fprintf(stderr, "gone: '%s' is not a key.\n", key);
			return (1);
	    	}
		mdbm_store_str(db, key, "", MDBM_INSERT);
		i++;
	}
	unless (i) return (0);

	unless (root = proj_root(0)) {
		fprintf(stderr, "Can't find package root\n");
		exit(1);
	}
	sprintf(s_gone, "%s/%s", root, SGONE);
	sprintf(g_gone, "%s/%s", root, GONE);

	s = sccs_init(s_gone, SILENT);
	assert(s);
	if (exists(s_gone)) {
		unless (EDITED(s)) {
			sccs_get(s, 0, 0, 0, 0, SILENT|GET_EDIT, s->gfile, 0);
		}
		g = fopen(g_gone, "r");
		while (fnext(key, g)) mdbm_store_str(db, key, "", MDBM_INSERT);
		fclose(g);
		s = sccs_restart(s);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		lines = addLine(lines, kv.key.dptr);
	}
	/* do not close the mdbm yet, we are using that memory */
	sortLines(lines, 0);
	unless (g = fopen(g_gone, "w")) {
		perror(g_gone);
		exit(1);
	}
	EACH(lines) fputs(lines[i], g);
	fclose(g);
	mdbm_close(db);
	sccs_free(s);
	/* handle the collapse of dangling (see delta.c:strip_dangling() */
	if (sys("bk", "delta", "-aqyAdd items with bk gone", s_gone, SYS)) {
		return (1);
	}
	/* if MONOTONIC off, turn on; else nop */
	if (sys("bk", "admin", "-qfMONOTONIC", s_gone, SYS)) return (1);
	unless (quiet) {
		fprintf(stderr,
		    "Do not forget to commit the gone file "
		    "before attempting a pull.\n");
	}
	return (0);
}
