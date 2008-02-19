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

private	 void	pass1(sccs *s);
private	 void	pass2(u32 flags);
private	 int	try_rename(sccs *s, delta *d, int dopass1, u32 flags);

int
names_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	char	*n;
	int	c, todo = 0, error = 0;
	u32	flags = 0;

	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q':	flags |= SILENT; break;		/* doc 2.0 */
		    default:	system("bk help -s names");
				return (1);
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "names: cannot find project root.\n");
		return (1);
	}
	for (n = sfileFirst("names", &av[optind], 0); n; n = sfileNext()) {
		if (streq(n, CHANGESET)) continue;
		unless (s = sccs_init(n, 0)) continue;
		unless (d = sccs_top(s)) {
			sccs_free(s);
			continue;
		}
		if (sccs_patheq(d->pathname, s->gfile)) {
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
		todo += try_rename(s, d, 1, flags);
		sccs_free(s);
	}
	if (sfileDone()) error |= 4;
	if (todo) pass2(flags);
	return (error);
}

private	int filenum = 0;

private	void
pass1(sccs *s)
{
	char	path[MAXPATH];

	unless (filenum) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	if (CSET(s) && proj_isComponent(s->proj)) {
		sprintf(path, "BitKeeper/RENAMES/repo.%d", ++filenum);
		if (rename(s->gfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", s->gfile, path);
			exit(1);
		}
		return;
	} 
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	if (rename(s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
		exit(1);
	}
}

private	void
pass2(u32 flags)
{
	char	path[MAXPATH];
	sccs	*s;
	delta	*d;
	int	worked = 0, failed = 0;
	int	i;
	
	unless (filenum) return;
	for (i = 1; i <= filenum; ++i) {
		sprintf(path, "BitKeeper/RENAMES/repo.%d/SCCS/s.ChangeSet", i);
		unless ((s = sccs_init(path, 0)) && HASGRAPH(s)) {
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
		if (try_rename(s, d, 0, flags)) {
			fprintf(stderr, "Can't rename %s -> %s\n",
			    s->gfile, d->pathname);
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
try_rename(sccs *s, delta *d, int dopass1, u32 flags)
{
	int	ret;
	char	*sfile;

	/* Handle components */
	if (CSET(s) && proj_isComponent(s->proj)) {
		*strrchr(s->gfile, '/') = 0;	// chomp /ChangeSet
		*strrchr(d->pathname, '/') = 0;	// chomp /ChangeSet
		s->state |= S_READ_ONLY;	// don't put those names back
		if (exists(d->pathname)) {
			/* circular or deadlock */
			if (dopass1) pass1(s);
			return (1);
		}
		mkdirf(d->pathname);
		if (rename(s->gfile, d->pathname)) {
			fprintf(stderr,
			    "%s->%s failed?\n", s->gfile, d->pathname);
			// dunno, we'll try later
			if (dopass1) pass1(s);
			return (1);
		}
		verbose((stderr, "names: %s -> %s\n", s->gfile, d->pathname));
		return (0);
	}

	sfile = name2sccs(d->pathname);
	assert(sfile);
	if (exists(sfile)) {
		/* circular or deadlock */
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	mkdirf(sfile);
	if (rename(s->sfile, sfile)) {
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	unless (flags & SILENT) {
		fprintf(stderr, "names: %s -> %s\n", s->sfile, sfile);
	}
	s = sccs_init(sfile, flags|INIT_NOCKSUM);
	free(sfile);
	unless (s) return (1);
	ret = 0;
	if (do_checkout(s)) ret = 1;
	sccs_free(s);
	return (ret);
}
