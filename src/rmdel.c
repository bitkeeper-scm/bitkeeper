/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");
char	*rmdel_help = "\n\
usage: rmdel [-qs] [-r<rev>] file\n\n\
    This command is not extended to minimize the removing\n\
    much information which can not be reclaimed.\n\n\
    -D		destroy all information newer than this revision\n\
    -q		run quietly\n\
    -r<r>	rmdel revision <r>\n\
    -s		run quietly\n\n";

/* XXX: this will appear someday in sccs.h */
extern sccs_rmdel(sccs *s, char *rev, int destroy, int flags);

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
rmdel_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	c, flags = 0, errors = 0;
	char	*name, *rev = 0;
	int	destroy = 0;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, rmdel_help);
		return (1);
	}
	while ((c = getopt(ac, av, "Dqr;s")) != -1) {
		switch (c) {
		    case 'D': destroy = 1; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 's': flags |= SILENT; break;

		    default:
			fprintf(stderr,
			    "rmdel: usage error, try rmdel --help\n");
			return (1);
		}
	}
	name = sfileFirst("rmdel", &av[optind], 0);
	do {
		unless (s = sccs_init(name, flags)) continue;
		if (!s->tree) {
			if (!(s->state & S_SFILE)) {
				fprintf(stderr, "rmdel: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			sccs_free(s);
			continue;
		}
		if (sccs_rmdel(s, rev, destroy, flags)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "rmdel of %s failed, skipping it.\n", name);
			}
			errors = 1;
		}
		sccs_free(s);
	} while (0);
	sfileDone();
#ifndef	NOPURIFY
	purify_list();
#endif
	return (errors);
}

int
main(int ac, char **av)
{
	return (rmdel_main(ac, av, "-"));
}
