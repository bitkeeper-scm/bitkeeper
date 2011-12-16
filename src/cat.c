/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"

int
cat_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	skip_bin = 0, rc = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	c, gfile;

	while ((c = getopt(ac, av, "B", 0)) != -1) {
		switch (c) {
		    case 'B': skip_bin = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	for (name = sfileFirst("cat", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM)) {
			rc |= 1;
			continue;
		}
		gfile = (access(s->gfile, R_OK) == 0);
		unless (gfile || HASGRAPH(s)) {
			rc |= 1;
			sccs_free(s);
			continue;
		}
		if (skip_bin && (BINARY(s) ||
		    (gfile && !HASGRAPH(s) && !ascii(s->gfile)))) {
			sccs_free(s);
			continue;
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%lu\n", s->gfile,
			    adler32(0, s->gfile, strlen(s->gfile)));
			fflush(stdout);
		}
		if (gfile) {
			rc |= cat(s->gfile) ? 1 : 0;
			sccs_free(s);
			continue;
		}
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, "-")) rc |= 1;
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	return (rc);
}
