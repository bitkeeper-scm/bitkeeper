/*
 * names.c - make sure all files are where they are supposed to be.
 *
 * Alg: for each file, if it is in the right place, skip it, if not,
 * move it to a temp name in BitKeeper/RENAMES and leave it for pass 2.
 * In pass 2, move each file out of BitKeeper/RENAMES to where it wants
 * to be, erroring if there is some other file in that place.
 */
#include "system.h"
#include "sccs.h"
#include "nested.h"
#include "progress.h"

typedef struct {
	u32	flags;
	int	filenum;	/* BitKeeper/RENAMES/<NUM> */
	hash	*empty;		/* dirs that may need to be deleted */
} options;

private	void	pass1(options *opts, sccs *s);
private	void	pass2(options *opts);
private	int	try_rename(options *opts, sccs *s, ser_t d, int dopass1);
private	void	saveRenames(options *opts, char *src, char *dst);

int
names_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	char	*p;
	int	nfiles = 0, n = 0;
	int	c, todo = 0, error = 0;
	options	*opts;
	ticker	*tick = 0;

	opts = new(options);
	opts->flags = SILENT;
	opts->empty = hash_new(HASH_MEMHASH);
	while ((c = getopt(ac, av, "N;qv", 0)) != -1) {
		switch (c) {
		    case 'N': nfiles = atoi(optarg); break;
		    case 'q': break;	// default
		    case 'v': opts->flags = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "names: cannot find project root.\n");
		return (1);
	}
	if (nfiles) tick = progress_start(PROGRESS_BAR, nfiles);
	for (p = sfileFirst("names", &av[optind], 0); p; p = sfileNext()) {
		if (tick) progress(tick, ++n);
		if (streq(p, CHANGESET)) continue;
		unless (s = sccs_init(p, INIT_MUSTEXIST)) continue;
		unless (d = sccs_top(s)) {
			sccs_free(s);
			continue;
		}
		if (sccs_patheq(PATHNAME(s, d), s->gfile)) {
			sccs_free(s);
			continue;
		}
		if (sccs_clean(s, SILENT|CLEAN_SKIPPATH)) {
			fprintf(stderr,
			    "names: %s is edited and modified\n", s->gfile);
			fprintf(stderr, "Wimping out on this rename\n");
			sccs_free(s);
			error |= 2;
			continue;
		}
		sccs_close(s); /* for win32 */
		todo += try_rename(opts, s, d, 1);
		sccs_free(s);
	}
	if (sfileDone()) error |= 4;
	if (todo) pass2(opts);
	rmEmptyDirs(opts->empty);
	hash_free(opts->empty);
	if (tick) progress_done(tick, error ? "FAILED" : "OK");
	return (error);
}

private	void
pass1(options *opts, sccs *s)
{
	char	path[MAXPATH];

	unless (opts->filenum) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	if (CSET(s) && proj_isComponent(s->proj)) {
		sprintf(path, "BitKeeper/RENAMES/repo.%d", ++opts->filenum);
		if (rename(s->gfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", s->gfile, path);
			exit(1);
		}
		return;
	}
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++opts->filenum);
	if (rename(s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
		exit(1);
	}
	saveRenames(opts, s->sfile, path);
}

private	void
pass2(options *opts)
{
	char	path[MAXPATH];
	sccs	*s;
	ser_t	d;
	int	worked = 0, failed = 0;
	int	i;
	u32	flags = opts->flags;

	unless (opts->filenum) return;
	for (i = 1; i <= opts->filenum; ++i) {
		sprintf(path, "BitKeeper/RENAMES/repo.%d/SCCS/s.ChangeSet", i);
		unless ((s = sccs_init(path, INIT_MUSTEXIST)) && HASGRAPH(s)) {
			sccs_free(s);
			sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", i);
			s = sccs_init(path, 0);
		}
		unless (s) {
			fprintf(stderr, "Unable to init %s\n", path);
			failed++;
			continue;
		}
		unless (d = sccs_top(s)) {
			/* should NEVER happen */
			fprintf(stderr, "Can't find TOT in %s\n", path);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		sccs_close(s); /* for Win32 NTFS */
		if (try_rename(opts, s, d, 0)) {
			fprintf(stderr, "Can't rename %s -> %s\n",
			    s->gfile, PATHNAME(s, d));
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		sccs_free(s);
		worked++;
	}
	if (failed) {
		fprintf(stderr, "Failed to fix %d renames\n", failed);
	} else {
		verbose((stderr, "names: all renames problems corrected.\n"));
	}
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
	u32	flags = opts->flags;

	/* Handle components */
	if (CSET(s) && proj_isComponent(s->proj)) {
		csetChomp(s->gfile);
		csetChomp(PATHNAME(s, d));
		s->state |= S_READ_ONLY;	// don't put those names back
		if (exists(PATHNAME(s, d))) {
			/* circular or deadlock */
			if (dopass1) pass1(opts, s);
			return (1);
		}
		mkdirf(PATHNAME(s, d));
		if (rename(s->gfile, PATHNAME(s, d))) {
			fprintf(stderr,
			    "%s->%s failed?\n", s->gfile, PATHNAME(s, d));
			// dunno, we'll try later
			if (dopass1) pass1(opts, s);
			return (1);
		}
		verbose((stderr, "names: %s -> %s\n",
			s->gfile, PATHNAME(s, d)));
		return (0);
	}

	sfile = name2sccs(PATHNAME(s, d));
	assert(sfile);
	if (exists(sfile)) {
		/* circular or deadlock */
		free(sfile);
		if (dopass1) pass1(opts, s);
		return (1);
	}
	mkdirf(sfile);
	if (rename(s->sfile, sfile)) {
		free(sfile);
		if (dopass1) pass1(opts, s);
		return (1);
	}
	unless (flags & SILENT) {
		fprintf(stderr, "names: %s -> %s\n", s->sfile, sfile);
	}
	saveRenames(opts, s->sfile, sfile);

	s = sccs_init(sfile, flags|INIT_NOCKSUM);
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

	/* src dir may be empty */
	dir = dirname_alloc(src);
	hash_insertStrSet(opts->empty, dir);
	free(dir);
	/* dest dir can't be empty */
	dir = dirname_alloc(dst);
	hash_deleteStr(opts->empty, dir);
	free(dir);
}
