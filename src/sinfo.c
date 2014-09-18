/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"

/*
 * info - display information about edited files.
 */
int
sinfo_main(int ac, char **av)
{
	sccs	*s = 0;
	int	rc = 0;
	char	*name;
	int	c, fast = 0, sf_flags = SF_SILENT|SF_GFILE, flags = 0;

	while ((c = getopt(ac, av, "aft", 0)) != -1) {
		switch (c) {
		    /*report new & unedited files */
		    case 'a': sf_flags &= ~SF_GFILE; break;	/* undoc 2.0 */
		    case 'f': fast = 1; break;			/* undoc? 2.0 */
		    case 't': flags |= SINFO_TERSE; break;	/* undoc 2.0 */
		    default: bk_badArg(c, av);
		}
	}
	for (name = sfileFirst("info", &av[optind], sf_flags);
	    name; name = sfileNext()) {
		if (fast) {
			char	buf[100];
			char	*gfile = sccs2name(name);
			char	*s;

			sprintf(buf, "%s:", gfile);
			printf("%-23s ", gfile);
			if (s = xfile_fetch(gfile, 'p')) {
				chomp(s);
				fputs(s, stdout);
				free(s);
			}
			if (gfile) free(gfile);
			printf("\n");
			continue;
		}
		s = sccs_init(name, INIT_NOCKSUM);
		unless (s) continue;
		rc |= sccs_info(s, flags) ? 1 : 0;
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	return (rc);
}
