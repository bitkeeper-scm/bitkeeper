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

typedef struct {
	u32	flags;		/* sccs_init flags */
	int	filenum;	/* BitKeeper/RENAMES/<NUM> */
	hash	*empty;		/* dirs that may need to be deleted */
	char	**rename_stack;	/* sequence of renames */
	MDBM	*idDB;
} options;

private	int	pass1(options *opts, sccs *s);
private	int	pass2(options *opts);
private	int	try_rename(options *opts, sccs *s, ser_t d, int dopass1);
private	void	saveRenames(options *opts, char *src, char *dst);
private	int	names_undo(options *opts);

int
names_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	char	*p;
	int	nfiles = 0, n = 0;
	int	c, error = 0;
	options	*opts;
	ticker	*tick = 0;
	char	rkey[MAXKEY];

	opts = new(options);
	opts->flags = SILENT;
	while ((c = getopt(ac, av, "N;qv", 0)) != -1) {
		switch (c) {
		    case 'N': nfiles = atoi(optarg); break;
		    case 'q': break;	// default
		    case 'v': opts->flags = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	opts->flags |= INIT_NOCKSUM|INIT_MUSTEXIST|INIT_NOGCHK;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: cannot find project root.\n", prog);
		return (1);
	}
	if (nfiles) tick = progress_start(PROGRESS_BAR, nfiles);
	for (p = sfileFirst("names", &av[optind], 0); p; p = sfileNext()) {
		if (tick) progress(tick, ++n);
		if (streq(p, CHANGESET)) continue;
		unless (s = sccs_init(p, opts->flags)) {
			fprintf(stderr, "%s: init of %s failed\n", prog, p);
			error |= 1;
			break;
		}
		d = sccs_top(s);
		assert(d);
		if (sccs_patheq(PATHNAME(s, d), s->gfile)) {
			sccs_free(s);
			continue;
		}
		if (!isdir(s->gfile) && sccs_clean(s, SILENT|CLEAN_SKIPPATH)) {
			fprintf(stderr,
			    "%s: %s is edited and modified\n", prog, s->gfile);
			sccs_free(s);
			error |= 2;
			break;
		}
		sccs_close(s); /* for win32 */

		unless (opts->idDB) opts->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		sccs_sdelta(s, sccs_ino(s), rkey);
		idcache_item(opts->idDB, rkey, PATHNAME(s, d));
		if (try_rename(opts, s, d, 1)) error |= 2;
		sccs_free(s);
		if (error) break;
	}
	if (sfileDone()) error |= 4;
	if (opts->empty) {
		rmEmptyDirs(opts->empty);
		hash_free(opts->empty);
		opts->empty = 0;
	}
	if (!error && opts->filenum && pass2(opts)) error |= 8;
	if (error) {
		fprintf(stderr, "%s: failed to rename files, "
		    "restoring files to original locations\n", prog);
		names_undo(opts);
	} else {
		/* names passed */
		if (opts->idDB) idcache_write(0, opts->idDB);
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
pass1(options *opts, sccs *s)
{
	char	*curpath;
	char	path[MAXPATH];

	unless (opts->filenum) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	if (CSET(s) && proj_isComponent(s->proj)) {
		curpath = s->gfile;
		sprintf(path, "BitKeeper/RENAMES/repo.%d", ++opts->filenum);
	} else {
		curpath = s->sfile;
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++opts->filenum);
	}
	if (rename(curpath, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", curpath, path);
		return (1);
	}
	saveRenames(opts, curpath, path);
	return (0);
}

private	int
pass2(options *opts)
{
	char	path[MAXPATH];
	sccs	*s;
	ser_t	d;
	int	failed = 0;
	int	i;

	/* save marker for names_undo() */
	opts->rename_stack =  addLine(opts->rename_stack, strdup(""));

	for (i = 1; i <= opts->filenum; ++i) {
		/* file || component - more likely to be file */
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", i);
		unless (s = sccs_init(path, opts->flags)) {
			sprintf(path,
			    "BitKeeper/RENAMES/repo.%d/SCCS/s.ChangeSet", i);
			s = sccs_init(path, opts->flags);
		}
		unless (s) {
			fprintf(stderr, "Unable to init %s\n", path);
			failed++;
			break;
		}
		/* Note to Wayne: clean up since same assert in main loop */
		d = sccs_top(s);
		assert(d);

		sccs_close(s); /* for Win32 NTFS */
		if (try_rename(opts, s, d, 0)) {
			fprintf(stderr, "%s: can't rename %s -> %s\n",
			    prog, s->gfile, PATHNAME(s, d));
			sccs_free(s);
			failed++;
			break;
		}
		sccs_free(s);
	}
	if (!failed && !(opts->flags & SILENT)) {
		fprintf(stderr, "names: all rename problems corrected.\n");
	}
	return (failed);
}

/*
 * Just for fun, see if the place where this wants to go is taken.
 * If not, just move it there.  We should be clean so just do the s.file.
 */
private	int
try_rename(options *opts, sccs *s, ser_t d, int dopass1)
{
	int	ret;
	char	*sfile;

	/* Handle components */
	if (CSET(s) && proj_isComponent(s->proj)) {
		csetChomp(s->gfile);
		csetChomp(PATHNAME(s, d));
		s->state |= S_READ_ONLY;	// don't put those names back
		if (exists(PATHNAME(s, d))) {
			/* circular or deadlock */
			return (dopass1 ? pass1(opts, s) : 1);
		}
		mkdirf(PATHNAME(s, d));
		if (rename(s->gfile, PATHNAME(s, d))) {
			fprintf(stderr,
			    "%s->%s failed?\n", s->gfile, PATHNAME(s, d));
			// dunno, we'll try later
			return (dopass1 ? pass1(opts, s) : 1);
		}
		saveRenames(opts, s->gfile, PATHNAME(s, d));
		return (0);
	}

	if (exists(PATHNAME(s, d))) {
		/* gfile in way */
		return (dopass1 ? pass1(opts, s) : 1);
	}
	sfile = name2sccs(PATHNAME(s, d));
	assert(sfile);
	if (exists(sfile)) {
		/* circular or deadlock */
		free(sfile);
		return (dopass1 ? pass1(opts, s) : 1);
	}
	mkdirf(sfile);
	if (rename(s->sfile, sfile)) {
		free(sfile);
		return (dopass1 ? pass1(opts, s) : 1);
	}
	saveRenames(opts, s->sfile, sfile);

	s = sccs_init(sfile, opts->flags);
	free(sfile);
	unless (s) return (1);
	ret = 0;
	if (do_checkout(s, 0, 0)) ret = 1;
	sccs_free(s);
	return (ret);
}

private void
saveRenames(options *opts, char *src, char *dst)
{
	char *dir;

	unless (opts->flags & SILENT) {
		fprintf (stderr, "%s: %s -> %s\n", prog, src, dst);
	}

	opts->rename_stack =
	    addLine(opts->rename_stack, aprintf("%s|%s", src, dst));

	unless (opts->empty) opts->empty = hash_new(HASH_MEMHASH);
	/* src dir may be empty */
	dir = dirname_alloc(src);
	hash_insertStrSet(opts->empty, dir);
	free(dir);
	/* dest dir can't be empty */
	dir = dirname_alloc(dst);
	hash_deleteStr(opts->empty, dir);
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
	char    *p;
	char	**list = opts->rename_stack;

	opts->rename_stack = 0;

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
		mkdirf(list[i]);
		if (rename(p, list[i])) {
			fprintf(stderr,
			    "%s: mv %s->%s failed, unable to restore changes\n",
			    prog, p, list[i]);
			return(1);
		}
		saveRenames(opts, p, list[i]);
	}
	freeLines(list, free);
	return (0);
}
