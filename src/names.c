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
private	 void	pass2(void);
private	 int	try_rename(sccs *s, delta *d, int dopass1);

int
names_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	char	*n;
	int	todo = 0;
	int	error = 0;

	/* this should be redundant, we should always be at the package root */
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "names: can not find package root.\n");
		return (1);
	}

	optind = 1;
	for (n = sfileFirst("names", &av[optind], 0); n; n = sfileNext()) {
		unless (s = sccs_init(n, 0, 0)) continue;
		unless (d = sccs_getrev(s, "+", 0, 0)) {
			sccs_free(s);
			continue;
		}
		if (streq(d->pathname, s->gfile)) {
			sccs_free(s);
			continue;
		}
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "names: %s is edited and modified\n", s->gfile);
			fprintf(stderr, "Wimping out on this rename\n");
			sccs_free(s);
			error |= 2;
			continue;
		}
		todo += try_rename(s, d, 1);
		sccs_free(s);
	}
	sfileDone();
	purify_list();
	if (todo) pass2();
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
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	if (rename(s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
	}
}

private	void
pass2()
{
	char	path[MAXPATH];
	sccs	*s;
	delta	*d;
	int	worked = 0, failed = 0;
	int	i;
	
	unless (filenum) return;
	for (i = 1; i <= filenum; ++i) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", i);
		unless (s = sccs_init(path, 0, 0)) {
			fprintf(stderr, "Unable to init %s\n", path);
			continue;
		}
		unless (d = sccs_getrev(s, "+", 0, 0)) {
			/* should NEVER happen */
			fprintf(stderr, "Can't find TOT in %s\n", path);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		if (try_rename(s, d, 0)) {
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
	fprintf(stderr,
	    "names: %d/%d worked, %d/%d failed\n",
	    worked, filenum, failed, filenum);
}

/*
 * Just for fun, see if the place where this wants to go is taken.
 * If not, just move it there.  We should be clean so just do the s.file.
 */
private	int
try_rename(sccs *s, delta *d, int dopass1)
{
	char	*sfile = name2sccs(d->pathname);

	assert(sfile);
	if (exists(sfile)) {
		/* circular or deadlock */
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	if (rename(s->sfile, sfile)) {
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	fprintf(stderr, "rename: %s -> %s\n", s->sfile, sfile);
	free(sfile);
	return (0);
}
