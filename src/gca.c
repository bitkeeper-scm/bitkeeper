/*
 * gca - list the closest GCA
 *
 * Copyright (c) 1999 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("%K%");

int
main(int ac, char **av)
{
	sccs	*s;
	char	*name, *r1 = 0, *r2 = 0;
	delta	*d1, *d2;
	int	c;

	while ((c = getopt(ac, av, "r|")) != -1) {
		switch (c) {
		    case 'r':
			unless (r1) {
				r1 = optarg;
			} else unless (r2) {
				r2 = optarg;
			} else {
				goto usage;
			}
			break;
		    default:
usage:			fprintf(stderr, "usage gca -rRev -rRev file\n");
			return (1);
		}
	}

	unless (r1 && r2) goto usage;
	unless (name = sfileFirst("gca", &av[optind], 0)) goto usage;
	if (sfileNext()) goto usage;
	unless (s = sccs_init(name, INIT_NOCKSUM, 0)) {
		perror(name);
		exit(1);
	}
	if (!s->tree) {
		perror(name);
		exit(1);
	}
	d1 = sccs_getrev(s, r1, 0, 0);
	d2 = sccs_getrev(s, r2, 0, 0);
	unless (d1 && d2) {
		sccs_free(s);
		fprintf(stderr, "gca: could not find '%s' or '%s'\n", d1, d2);
		return (1);
	}
	d1 = sccs_gca(s, d1, d2, 1);
	printf("%s\n", d1->rev);
	sfileDone();
	sccs_free(s);
	purify_list();
	return (0);
}
