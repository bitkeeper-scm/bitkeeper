/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * info - display information about edited files.
 */
int
main(int ac, char **av)
{
	sccs	*s = 0;
	int	e = 0;
	char	*name;
	int	c, fast = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "usage: %s [-f] [files...]\n", av[0]);
		return (1);
	}
	while ((c = getopt(ac, av, "f")) != -1) {
		switch (c) {
		    case 'f': fast = 1; break;
		    default: goto usage;
		}
	}
	for (name = sfileFirst("info", &av[optind], SF_SILENT|SF_GFILE);
	    name; name = sfileNext()) {
		if (fast) {
			FILE	*f;
			char	buf[100];
			char	*gfile = sccs2name(name);
			char	*s;

			sprintf(buf, "%s:", gfile);
			printf("%-16s", gfile);
			if ((s = rindex(name, '/'))) {
				s++;
			} else {
				s = name;
			}
			*s = 'p';
			f = fopen(name, "r");
			if (fgets(buf, sizeof(buf), f)) {
				char	*s;
				for (s = buf; *s && *s != '\n'; ++s)
					;
				*s = 0;
				printf(buf);
			}
			fclose(f);
			if (gfile) free(gfile);
			printf("\n");
			continue;
		}
		s = sccs_init(name, 0);
		if (!s) continue;
		e |= sccs_info(s, 0);
		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (e);
}
