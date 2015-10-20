/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"

/*
 * Convert a BK file to SCCS format.
 */
int
unbk_main(int ac, char **av)
{
	sccs	*s;
	int	errors = 0;
	char	*name;

	unless(ac > 1 && streq("--I-know-this-destroys-my-bk-repo", av[1])) {
		fprintf(stderr, 
		    "usage: bk _unbk --I-know-this-destroys-my-bk-repo\n");
		return (1);
	}
	for (name = sfileFirst("_unbk", &av[2], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		s->bitkeeper = 0;
		s->encoding_out = sccs_encoding(s, 0, 0);
		s->encoding_out &= ~(E_BK|E_BWEAVE2|E_BWEAVE3|E_COMP);
		sccs_adminFlag(s, NEWCKSUM|ADMIN_RM1_0);
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}
