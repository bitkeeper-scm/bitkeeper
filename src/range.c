#include "system.h"
#include "sccs.h"
#include "range.h"

void
old2new(delta *d, delta *stop)
{
	unless (d) return;
	unless (d == stop) old2new(d->next, stop);
	fprintf(stderr, " %s", d->rev);
}

void
new2old(delta *d, delta *stop)
{
	unless (d) return;
	fprintf(stderr, " %s", d->rev);
	unless (d == stop) old2new(d->next, stop);
}

int
main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	char	*name;
	int	expand = 1;
	int	c;
	RANGE_DECL;

	while ((c = getopt(ac, av, "ec;r;")) != -1) {
		switch (c) {
		    case 'e': expand++; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr,
			    "usage: %s [-r<rev>] [-c<date>]\n", av[0]);
			exit(1);
		}
	}
	for (name = sfileFirst("range", &av[optind], 0);
	    name; name = sfileNext()) {
	    	unless (s = sccs_init(name, INIT_NOCKSUM, 0)) {
			continue;
		}
		if (!s->tree) goto next;
		RANGE("range", s, expand, 1);
		if (s->state & S_SET) {
			fprintf(stderr, "%s set:", s->gfile);
			for (e = s->table; e; e = e->next) {
				unless (e->type == 'D') continue;
				if (e->flags & D_SET) {
					fprintf(stderr, " %s", e->rev);
				}
			}
		} else {
			fprintf(stderr, "%s %s..%s:",
			    s->gfile, s->rstart->rev, s->rstop->rev);
			old2new(s->rstop, s->rstart);
			fprintf(stderr, "\n");
			fprintf(stderr, "%s %s..%s:",
			    s->gfile, s->rstop->rev, s->rstart->rev);
			for (e = s->rstop; e; e = e->next) {
				fprintf(stderr, " %s", e->rev);
				if (e == s->rstart) break;
			}
		}
		fprintf(stderr, "\n");
next:		sccs_free(s);
	}
	return (0);
}

