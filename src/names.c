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

private	 void	pass1(char *spath);
private	 void	pass2(u32 flags);
private	 int	try_rename(char *old, char *new, int dopass1, u32 flags);

private	int filenum;

int
names_main(int ac, char **av)
{
	sccs	*s;
	char	*n;
	int	todo = 0;
	int	error = 0;
	u32	flags = 0;

	/* this should be redundant, we should always be at the package root */
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "names: can not find package root.\n");
		return (1);
	}

	optind = 1;
	names_init();
	for (n = sfileFirst("names", &av[optind], 0); n; n = sfileNext()) {
		unless (s = sccs_init(n, 0, 0)) continue;
		unless (sccs_setpathname(s)) {
			fprintf(stderr,
			    "names: can't initialize pathname in %s\n",
			    	s->gfile);
			sccs_free(s);
			continue;
		}
		if (streq(s->spathname, s->sfile)) {
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
		todo += names_rename(s->sfile, s->spathname, flags);
		sccs_free(s);
	}
	sfileDone();
	names_cleanup(flags);
	purify_list();
	return (error);
}

void
names_init(void)
{
	/* this should be redundant, we should always be at the project root */

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "names: can not find project root.\n");
		exit(1);
	}
	filenum = 0;
}

int
names_rename(char *pathold, char *pathnew, u32 flags)
{
	return(try_rename(pathold, pathnew, 1, flags));
}

void
names_cleanup(u32 flags)
{
	if (filenum) pass2(flags);
}

private	void
pass1(char *spath)
{
	char	path[MAXPATH];

	unless (filenum) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	if (rename(spath, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", spath, path);
	}
}

private	void
pass2(u32 flags)
{
	char	path[MAXPATH];
	sccs	*s;
	int	worked = 0, failed = 0;
	int	i;
	
	unless (filenum) return;
	for (i = 1; i <= filenum; ++i) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", i);
		unless (s = sccs_init(path, 0, 0)) {
			fprintf(stderr, "Unable to init %s\n", path);
			failed++;
			continue;
		}
		unless (sccs_setpathname(s)) {
			fprintf(stderr,
			    "names: can't initialize pathname in %s\n",
			    	s->gfile);
			sccs_free(s);
			failed++;
			continue;
		}
		if (try_rename(path, s->spathname, 0, flags)) {
			fprintf(stderr, "Can't rename %s -> %s\n",
			    path, s->spathname);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		sccs_free(s);
		worked++;
	}
	unless (flags & SILENT) {
		fprintf(stderr,
		    "names: %d/%d worked, %d/%d failed\n",
		    worked, filenum, failed, filenum);
	}
}

/*
 * Just for fun, see if the place where this wants to go is taken.
 * If not, just move it there.  We should be clean so just do the s.file.
 */
private	int
try_rename(char *spathold, char *spathnew, int dopass1, u32 flags)
{
	assert(spathold);
	assert(spathnew);
	if (exists(spathnew)) {
		/* circular or deadlock */
		if (dopass1) pass1(spathold);
		return (1);
	}
	mkdirf(spathnew);
	if (rename(spathold, spathnew)) {
		if (dopass1) pass1(spathold);
		return (1);
	}
	unless (flags & SILENT) {
		fprintf(stderr, "names: %s -> %s\n", spathold, spathnew);
	}
	return (0);
}
