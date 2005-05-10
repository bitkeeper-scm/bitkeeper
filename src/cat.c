/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"

int
cat_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	skip_bin = 0, errors = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	c, gfile;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
usage:		system("bk help -s cat");
		return (1);
	}
	while ((c = getopt(ac, av, "B")) != -1) {
		switch (c) {
		    case 'B': skip_bin = 1; break;
		    default: goto usage;
		}
	}
	for (name = sfileFirst("cat", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM)) continue;
		gfile = (access(s->gfile, R_OK) == 0);
		unless (gfile || HASGRAPH(s)) {
			sccs_free(s);
			continue;
		}
		if (skip_bin &&
		    ((s->encoding & E_BINARY) ||
		    (gfile && !HASGRAPH(s) && !ascii(s->gfile)))) {
			sccs_free(s);
			continue;
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%u\n", s->gfile, crc(s->gfile));
			fflush(stdout);
		}
		if (gfile) {
			errors |= cat(s->gfile);
			sccs_free(s);
			continue;
		}
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, "-")) errors |= 1;
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}
