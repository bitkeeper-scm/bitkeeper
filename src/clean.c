/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private	int	hasGfile(char *name);

/*
 * This works even if there isn't a gfile.
 */
int
clean_main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT;
	int	sflags = 0;
	int	c;
	int	ret = 0;
	char	*name;
	
	while ((c = getopt(ac, av, "pqv", 0)) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'q': 					/* doc 2.0 */
			flags |= CLEAN_SHUTUP; sflags |= SF_SILENT; break;
		    case 'v': flags &= ~SILENT; break;		/* doc 2.0 */
			break;
		    default: bk_badArg(c, av);
		}
	}

	name = sfileFirst("clean", &av[optind], sflags);
	while (name) {
		unless (hasGfile(name)) goto next;
		s = sccs_init(name, SILENT|INIT_NOCKSUM);
		if (s) {
			if (sccs_clean(s, flags)) ret = 1;
			sccs_free(s);
		}
next:		name = sfileNext();
	}
	if (sfileDone()) ret = 1;
	return (ret);
}

/*
 * Delay sccs_initing; return 0 (no need to clean) if no g and no p file.
 */
private	int
hasGfile(char *sfile)
{
	char	*gfile = sccs2name(sfile);
	int	ret;

	assert(gfile);
	ret = exists(gfile);
	unless (ret) ret = xfile_exists(gfile, 'p');
	free(gfile);
	return (ret);
}

/* return true if file has pending deltas or local mods */
int
hasLocalWork(char *gfile)
{
	sccs	*s;
	char	*sfile = name2sccs(gfile);
	int	rc = 1;

	unless (s = sccs_init(sfile, SILENT)) return (-1);
	if (HASGRAPH(s)) {
		if ((FLAGS(s, sccs_top(s)) & D_CSET)
		    && !sccs_clean(s, CLEAN_CHECKONLY|SILENT)) {
			rc = 0;
		}
	} else {
		unless (exists(gfile)) rc = 0;
	}
	sccs_free(s);
	return (rc);
}
