/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * Convert a BK file to SCCS format.
 */
int
unbk_main(int ac, char **av)
{
	sccs	*s;
	int	errors = 0;
	char	*name;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help unbk");
		return (1);
	}

	for (name = sfileFirst("unbk", &av[1], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		s->bitkeeper = 0;
		sccs_admin(s,
		    0, NEWCKSUM|ADMIN_RM1_0, 0, "none", 0, 0, 0, 0, 0, 0);
		sccs_free(s);
	}
	sfileDone();
	return (errors);
}
