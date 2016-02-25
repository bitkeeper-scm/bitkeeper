/*
 * Copyright 1999-2003,2005,2008-2011,2014-2016 BitMover, Inc
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

/*
 * names.c - make sure all files are where they are supposed to be.
 *
 * Alg: for each file, if it is in the right place, skip it, if not,
 * try to remove it to the right place and if that doesn't work move
 * it to a temp name in BitKeeper/RENAMES and leave it for pass 2.  In
 * pass 2, delete any directories that are now empty and then move
 * each file out of BitKeeper/RENAMES to where it wants to be,
 * erroring if there is some other file in that place.
 */
#include "system.h"
#include "sccs.h"
#include "nested.h"
#include "progress.h"

#define	TIPPATH(s, d) PATHNAME(s, d)

typedef struct {
	u32	flags;		/* sccs_init flags */
	hash	*empty;		/* dirs that may need to be deleted */
	char	**rename_stack;	/* sequence of renames */
	MDBM	*idDB;
	char	**renames;	/* remember destinations in BitKeeper/RENAMES */
	u32	list:1;		/* list new names on stdout */
} options;

private	int	pass1(options *opts, char *src, char *dest);
private	int	pass2(options *opts);
private	int	do_rename(options *opts, char *src, char *dest);
private	void	saveRenames(options *opts, char *src, char *dst);
private	int	names_undo(options *opts);

int
names_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	char	*p, *rk;
	int	nfiles = 0, n = 0;
	int	c, error = 0;
	options	*opts;
	ticker	*tick = 0;
	char	*src, *dest;
	char	rkey[MAXKEY];

	opts = new(options);
	opts->flags = SILENT;
	while ((c = getopt(ac, av, "lN;qv", 0)) != -1) {
		switch (c) {
		    case 'l': opts->list = 1; break;
		    case 'N': nfiles = atoi(optarg); break;
		    case 'q': break;	// default
		    case 'v': opts->flags = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	opts->flags |= INIT_NOCKSUM|INIT_MUSTEXIST|INIT_NOGCHK;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: cannot find project root\n", prog);
		return (1);
	}
	if (nfiles) tick = progress_start(PROGRESS_BAR, nfiles);
	for (p = sfileFirst("names", &av[optind], 0); p; p = sfileNext()) {
		if (tick) progress(tick, ++n);
		if (streq(p, CHANGESET)) continue;
		if (dest = sfileRev()) {
			/*
			 * alternative stdin src|dest|rkey
			 * Faster as it never needs to sccs_init() any files
			 */
			if (rk = strchr(dest, '|')) *rk++ = 0;
			assert(rk);
			src = sccs2name(p);
			dest = strdup(dest);

		} else {
			/* look in sfile to see where it goes */
			unless (s = sccs_init(p, opts->flags)) {
				fprintf(stderr,
				    "%s: init of %s failed\n", prog, p);
				error |= 1;
				break;
			}
			d = sccs_top(s);
			assert(d);
			src = strdup(s->gfile);
			dest = strdup(TIPPATH(s, d));
			sccs_sdelta(s, sccs_ino(s), rkey);
			rk = rkey;
			sccs_free(s);
		}
		if (streq(src, dest)) {
			if (opts->list) {
				printf("%s\n", src);
				fflush(stdout);
			}
		} else {
			unless (opts->idDB) {
				opts->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
			}
			idcache_item(opts->idDB, rk, dest);
			if (do_rename(opts, src, dest) &&
			    pass1(opts, src, dest)) {
				error |= 2;
			}
		}
		free(src);
		free(dest);
		if (error) break;
	}
	if (sfileDone()) error |= 4;
	if (!error && opts->renames && pass2(opts)) error |= 8;
	if (error) {
		fprintf(stderr, "%s: failed to rename files, "
		    "restoring files to original locations\n", prog);
		names_undo(opts);
	} else {
		/* names passed */
		if (opts->idDB) idcache_write(0, opts->idDB);
		unless (opts->flags & SILENT) {
			fprintf(stderr,
			    "names: all rename problems corrected\n");
		}
	}
	if (opts->empty) {
		rmEmptyDirs(opts->empty);
		hash_free(opts->empty);
		opts->empty = 0;
	}
	freeLines(opts->rename_stack, free);
	if (opts->idDB) mdbm_close(opts->idDB);
	if (tick) progress_done(tick, error ? "FAILED" : "OK");
	return (error);
}

private	int
pass1(options *opts, char *src, char *dest)
{
	char	path[MAXPATH];

	unless (opts->renames) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	opts->renames = addLine(opts->renames, strdup(dest));

	sprintf(path, "BitKeeper/RENAMES/%d", nLines(opts->renames));
	if (ends_with(src, "/ChangeSet")) concat_path(path, path, GCHANGESET);
	return (do_rename(opts, src, path));
}

private	int
pass2(options *opts)
{
	char	*dest;
	int	failed = 0;
	int	i;
	int	didrmdir = 0;
	char	path[MAXPATH];

	/* save marker for names_undo() */
	opts->rename_stack =  addLine(opts->rename_stack, strdup(""));

	EACH(opts->renames) {
		sprintf(path, "BitKeeper/RENAMES/%d", i);
		if (isdir(path)) concat_path(path, path, GCHANGESET);

		dest = opts->renames[i];

again:		if (do_rename(opts, path, dest)) {
			unless (didrmdir) {
				didrmdir = 1;
				rmEmptyDirs(opts->empty);
				hash_free(opts->empty);
				opts->empty = 0;
				goto again;
			}
			fprintf(stderr, "%s: can't rename %s -> %s\n",
			    prog, path, dest);
			failed++;
			break;
		}
	}
	freeLines(opts->renames, free);
	opts->renames = 0;
	return (failed);
}

/*
 * Just for fun, see if the place where this wants to go is taken.
 * If not, just move it there.  We should be clean so just do the s.file.
 */
private	int
do_rename(options *opts, char *src, char *dest)
{
	char	*comp1, *comp2;
	int	didgfile = 0;
	int	rc;
	struct	stat sb;

	/* Handle components */
	if (ends_with(src, "/ChangeSet")) {
		comp1 = strndup(src, strlen(src) - strlen("/ChangeSet"));
		comp2 = strndup(dest, strlen(dest) - strlen("/ChangeSet"));

		/* try to move the component in place */
		rc = exists(comp2) || mkdirf(comp2) || rename(comp1, comp2);
		free(comp1);
		free(comp2);
		if (rc) return (rc); /* failed, try later */
	} else {
		if (mkdirf(dest)) return (1);
		if (!lstat(src, &sb) && !S_ISDIR(sb.st_mode)) {
			/* we have a gfile, so need a move it too */
			if (exists(dest) || rename(src, dest)) return (1);
			didgfile = 1;
		} else if (!lstat(dest, &sb) && !S_ISDIR(sb.st_mode)) {
			/*
			 * no gfile, so a dir at dest is OK.  We assume it
			 * will be moved in the future.
			 */
			return (1);
		}
		if (sfile_exists(0, dest) || sfile_move(0, src, dest)) {
			if (didgfile) rename(dest, src);
			return (1);
		}
	}
	if (opts->list && !begins_with(dest, "BitKeeper/RENAMES/")) {
		char	*sfile = name2sccs(dest);

		printf("%s\n", sfile);
		free(sfile);
		fflush(stdout);
	}
	saveRenames(opts, src, dest);
	return (0);
}

private char *
sdir(char *dir)
{
	char	buf[MAXPATH];

	strcpy(buf, dir);
	if (streq(basenm(dir), GCHANGESET)) {
		return (strdup(dirname(buf)));
	} else {
		concat_path(buf, dirname(buf), "SCCS");
		return (strdup(buf));
	}
}


private void
saveRenames(options *opts, char *src, char *dst)
{
	char	*dir;

	unless (opts->flags & SILENT) {
		fprintf (stderr, "%s: %s -> %s\n", prog, src, dst);
	}

	opts->rename_stack =
	    addLine(opts->rename_stack, aprintf("%s|%s", src, dst));

	unless (opts->empty) opts->empty = hash_new(HASH_MEMHASH);
	/* src dir may be empty */
	hash_insertStrSet(opts->empty, (dir = sdir(src)));
	free(dir);
	/* dest dir can't be empty */
	hash_deleteStr(opts->empty, (dir = sdir(dst)));
	free(dir);
}

/*
 * Take the list of renames performed so far and perform them in reverse
 * so the repository is restored to its original condition.
 */
private int
names_undo(options *opts)
{
	int     i;
	char	*p;
	char    **list = opts->rename_stack;

	opts->rename_stack = 0;
	opts->list = 0;		/* so we don't print the cleanups */

	EACH_REVERSE(list) {
		unless (list[i][0]) {
			rmEmptyDirs(opts->empty);
			hash_free(opts->empty);
			opts->empty = 0;
			continue;
		}
		p = strchr(list[i], '|');
		assert(p);
		*p++ = 0;
		if (do_rename(opts, p, list[i])) {
			fprintf(stderr,
			    "%s: mv %s -> %s failed, "
			    "unable to restore changes\n",
			    prog, p, list[i]);
			return(1);
		}
	}
	freeLines(list, free);
	return (0);
}
