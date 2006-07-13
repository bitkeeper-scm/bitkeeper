#include "system.h"
#include "sccs.h"
#include "range.h"

int
range_main(int ac, char **av)
{
	sccs	*s = 0;
	delta	*e;
	char	*name;
	int	expand = 1;
	int	quiet = 0;
	int	c;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "ec;qr;")) != -1) {
		switch (c) {
		    case 'e': expand++; break;
		    case 'q': quiet++; break;
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) goto usage;
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) goto usage;
			break;
		    default:
usage:			fprintf(stderr,
			    "usage: %s [-q] [-r<rev>] [-c<date>]\n", av[0]);
			exit(1);
		}
	}
	for (name = sfileFirst("range", &av[optind], 0);
	    name; name = sfileNext()) {
		if (s && (streq(s->gfile, name) || streq(s->sfile, name))) {
			for (e = s->table; e; e = e->next) {
				e->flags &= ~(D_SET|D_RED|D_BLUE);
			}
		} else {
			if (s) sccs_free(s);
			unless (s = sccs_init(name, INIT_NOCKSUM)) {
				continue;
			}
		}
		unless (HASGRAPH(s)) {
			sccs_free(s);
			s = 0;
			continue;
		}
		if (range_process("range", s, RANGE_SET, &rargs)) goto next;
		if (s->state & S_SET) {
			printf("%s set:", s->gfile);
			for (e = s->table; e; e = e->next) {
				if (e->flags & D_SET) {
					printf(" %s", e->rev);
					if (e->type == 'R') printf("T");
				}
			}
		} else {
			printf("%s %s..%s:",
			    s->gfile, s->rstop->rev, s->rstart->rev);
			for (e = s->rstop; e; e = e->next) {
				printf(" %s", e->rev);
				if (e->type == 'R') printf("T");
				if (e == s->rstart) break;
			}
		}
		printf("\n");
next:		;
	}
	if (s) sccs_free(s);
	return (0);
}

