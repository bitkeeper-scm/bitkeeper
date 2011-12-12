#include "system.h"
#include "sccs.h"
#include "range.h"

int
range_main(int ac, char **av)
{
	sccs	*s = 0;
	ser_t	e;
	char	*name;
	int	expand = 1;
	int	quiet = 0;
	int	all = 0;
	int	c;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "@|aec;qr;", 0)) != -1) {
		switch (c) {
		    case 'a': all++; break;
		    case 'e': expand++; break;
		    case 'q': quiet++; break;
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) usage();
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    case '@':
			if (range_urlArg(&rargs, optarg)) usage();
			break;
		    default: bk_badArg(c, av);
		}
	}
	for (name = sfileFirst("range", &av[optind], 0);
	    name; name = sfileNext()) {
		if (s && (streq(s->gfile, name) || streq(s->sfile, name))) {
			sccs_clearbits(s, D_SET|D_RED|D_BLUE);
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
		if (all) range_markMeta(s);
		if (s->state & S_SET) {
			printf("%s set:", s->gfile);
			for (e = TABLE(s); e >= TREE(s); e--) {
				if (FLAGS(s, e) & D_SET) {
					printf(" %s", REV(s, e));
					if (TAG(s, e)) printf("T");
				}
			}
		} else {
			printf("%s %s..%s:",
			    s->gfile, REV(s, s->rstop), REV(s, s->rstart));
			for (e = s->rstop; e >= TREE(s); e--) {
				printf(" %s", REV(s, e));
				if (TAG(s, e)) printf("T");
				if (e == s->rstart) break;
			}
		}
		printf("\n");
next:		;
	}
	if (s) sccs_free(s);
	return (0);
}

