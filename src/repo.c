/*
 * repo.c - stuff related to repository level utils / interfaces
 *
 * Copyright (c) 2004-2009 BitMover, Inc.
 */

#include "system.h"
#include "sccs.h"
#include "nested.h"

private	int	nfiles(void);

/*
 * bk nfiles - print the approximate number of files in the repo
 * (works in regular and nested collections).
 */
int
nfiles_main(int ac, char **av)
{
	int	c;
	int	recurse = 0;

	while ((c = getopt(ac, av, "r", 0)) != -1) {
		switch (c) {
		    case 'r':
			recurse = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();

	if (proj_cd2root()) {
		fprintf(stderr, "%s: not in a repo.\n", av[0]);
		return (1);
	}

	if (recurse) {
		proj_cd2product(); /* ignore error */
		printf("%u\n", nfiles());
	} else {
		printf("%u\n", repo_nfiles(0,0));
	}
	return (0);
}

/* key names for NFILES kv file */
#define TOTFILES	""
#define USRFILES	"usrfiles"

/*
 * Return the approximate number of files in a repository, both the
 * total number and the number not under BitKeeper/.
 * The counts are updated whenever 'bk -R sfiles' is run.
 */
int
repo_nfiles(project *p, filecnt *fc)
{
	hash	*h;
	FILE	*f;
	filecnt	junk;
	char	*cmd, *t;

	unless (fc) fc = &junk;
	fc->tot = -1;
	fc->usr = -1;
	if (h = hash_fromFile(0, proj_fullpath(p, "BitKeeper/log/NFILES"))) {
		unless ((fc->tot = hash_fetchStrNum(h, TOTFILES)) || h->kptr) {
			fc->tot = -1;
		}
		unless ((fc->usr = hash_fetchStrNum(h, USRFILES)) || h->kptr) {
			fc->usr = -1;
		}
		hash_free(h);
	}
	if ((fc->tot == -1) || (fc->usr == -1)) {
		/*
		 * The NFILES file is missing one or both fields.  We
		 * will run 'bk sfiles' to regenerate the file and
		 * count the data ourselves in case some permissions
		 * problem prevents sfiles from updating the cache.
		 */
		fc->tot = fc->usr = 0;
		cmd = aprintf("bk --cd='%s' sfiles 2> " DEVNULL_WR,
		    proj_root(p));
		f = popen(cmd, "r");
		free(cmd);
		while (t = fgetline(f)) {
			++fc->tot;
			unless (strneq(t, "BitKeeper/", 10)) ++fc->usr;
		}
		pclose(f);
	}
	return (fc->tot);
}

/*
 * Write a new NFILES file, if it changed.
 */
void
repo_nfilesUpdate(filecnt *nf)
{
	char	*n = proj_fullpath(0, "BitKeeper/log/NFILES");
	hash	*h;
	FILE	*f;
	char	ntmp[MAXPATH];

	h = hash_fromFile(hash_new(HASH_MEMHASH), n);
	if ((nf->tot != hash_fetchStrNum(h, TOTFILES)) ||
	    (nf->usr != hash_fetchStrNum(h, USRFILES))) {
		hash_storeStrNum(h, TOTFILES, nf->tot);
		hash_storeStrNum(h, USRFILES, nf->usr);
		sprintf(ntmp, "%s.new.%u", n, (u32)getpid());
		f = fopen(ntmp, "w");
		hash_toStream(h, f);
		fclose(f);
		if (rename(ntmp, n)) perror(n);
	}
	hash_free(h);
}

/*
 * Return the approximate number of files in a repo or nested collection.
 */
private	int
nfiles(void)
{
	comp	*c;
	nested	*n;
	int	i;
	u32	nfiles = 0;

	unless (proj_product(0)) return (repo_nfiles(0,0));
	if (proj_cd2product()) return (0);

	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	EACH_STRUCT(n->comps, c, i) {
		unless (c->present) continue;
		if (chdir(c->path)) assert(0);
		nfiles += repo_nfiles(0,0);
		proj_cd2product();
	}
	nested_free(n);
	return (nfiles);
}
